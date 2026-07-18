/**
 * @file ts_scan.c
 * @brief 时序扫描算子实现
 *
 * 实现 Volcano 迭代器协议的时序扫描算子。
 * 直接调用 ts_engine_query() 进行聚合查询，
 * 将结果逐行返回给上层算子。
 */
#include "db/executor/ts/ts_scan.h"
#include "db/ts_engine.h"
#include <stdlib.h>
#include <string.h>

/* 时序扫描内部状态 */
typedef struct ts_scan_internal_s {
    ts_engine_db_t      *engine;        /**< 时序引擎句柄 */
    ts_query_results_t   results;       /**< 聚合查询结果 */
    int                  current_index; /**< 当前结果索引 */
    bool                 done;          /**< 是否已返回所有结果 */
    bool                 engine_opened; /**< 引擎是否已打开 */
} ts_scan_internal_t;

/**
 * @brief 将降采样窗口（毫秒）映射为时间粒度
 *
 * 根据 down_sample_window 选择合适的 ts_granularity_t：
 * - 0：逐点返回（使用 SECONDS 粒度，实际由引擎决定）
 * - 1~1000ms：SECOND
 * - 1001~60000ms：MINUTE
 * - 60001~3600000ms：HOUR
 * - >3600000ms：DAY
 */
static ts_granularity_t window_to_granularity(int64_t down_sample_window)
{
    if (down_sample_window <= 0) {
        return TS_GRANULARITY_SECOND;
    } else if (down_sample_window <= 1000) {
        return TS_GRANULARITY_SECOND;
    } else if (down_sample_window <= 60000) {
        return TS_GRANULARITY_MINUTE;
    } else if (down_sample_window <= 3600000) {
        return TS_GRANULARITY_HOUR;
    } else {
        return TS_GRANULARITY_DAY;
    }
}

/**
 * @brief 根据 time_start/time_end 选择默认聚合函数
 *
 * 如果时间范围较大，使用 AVG 降采样；否则使用 SUM。
 */
static ts_aggregate_func_t pick_default_agg(int64_t time_start, int64_t time_end)
{
    if (time_start == 0 && time_end == 0) {
        return AGG_SUM;
    }
    int64_t range = time_end - time_start;
    if (range > 3600000) { /* >1 小时 */
        return AGG_AVG;
    }
    return AGG_SUM;
}

TsScanState *exec_ts_scan_init(PlanState *parent,
    int64_t time_start, int64_t time_end, void *tag_filters)
{
    TsScanState *state = (TsScanState *)calloc(1, sizeof(TsScanState));
    if (state == NULL) return NULL;

    state->ps.type = EXEC_TIMESERIES_SCAN;
    state->ps.left = NULL;
    state->ps.right = NULL;
    if (parent) {
        state->ps.expr_context = parent->expr_context;
    }
    state->time_start = time_start;
    state->time_end = time_end;
    state->tag_filters = tag_filters;
    state->down_sample_window = 0;

    /* 分配内部状态 */
    ts_scan_internal_t *internal = (ts_scan_internal_t *)calloc(1, sizeof(ts_scan_internal_t));
    if (internal == NULL) {
        free(state);
        return NULL;
    }
    internal->done = false;
    internal->current_index = 0;
    internal->engine_opened = false;
    memset(&internal->results, 0, sizeof(ts_query_results_t));
    state->ps.state = internal;

    return state;
}

TupleTableSlot *exec_ts_scan_next(TsScanState *state)
{
    if (state == NULL) return NULL;

    ts_scan_internal_t *internal = (ts_scan_internal_t *)state->ps.state;
    if (internal == NULL || internal->done) return NULL;

    /* 首次调用时打开引擎并执行查询 */
    if (!internal->engine_opened) {
        /* 打开时序引擎 */
        internal->engine = (ts_engine_db_t *)ts_engine_open("default", ACCESS_MODE_READ);
        if (internal->engine == NULL) {
            internal->done = true;
            return NULL;
        }
        internal->engine_opened = true;

        /* 确定查询参数 */
        int64_t start = state->time_start;
        int64_t end = state->time_end;
        if (start == 0 && end == 0) {
            /* 未指定时间范围，使用引擎的默认范围 */
            start = internal->engine->start_time;
            end = internal->engine->end_time;
            if (start == 0 && end == 0) {
                /* 引擎也无数据，直接返回空 */
                internal->done = true;
                return NULL;
            }
        }

        ts_granularity_t granularity = window_to_granularity(state->down_sample_window);
        ts_aggregate_func_t agg_func = pick_default_agg(start, end);

        /* 执行聚合查询 */
        int rc = ts_engine_query(internal->engine,
                                 start, end,
                                 granularity,
                                 agg_func,
                                 &internal->results);
        if (rc != 0 || internal->results.count <= 0) {
            internal->done = true;
            return NULL;
        }
    }

    /* 遍历结果 */
    if (internal->current_index >= internal->results.count) {
        internal->done = true;
        return NULL;
    }

    ts_aggregate_result_t *result = &internal->results.results[internal->current_index];
    internal->current_index++;

    /* 创建元组描述符：3 列 — timestamp(int64), value(double), tag(占位) */
    ExecTupleDesc *desc = exec_make_tuple_desc(3);
    if (desc == NULL) return NULL;

    TupleTableSlot *slot = exec_make_tuple_slot(desc);
    if (slot == NULL) {
        exec_drop_tuple_desc(desc);
        return NULL;
    }

    /* 填充列值 */
    slot->tts_values[0] = (void *)(uintptr_t)result->timestamp;   /* 时间戳 */
    {
        /* double 值需要通过位模式存储以避免指针截断 */
        double val = result->value;
        uint64_t bits;
        memcpy(&bits, &val, sizeof(bits));
        slot->tts_values[1] = (void *)(uintptr_t)bits;
    }
    slot->tts_values[2] = NULL;  /* tag 占位 */
    slot->tts_nvalid = 3;

    return slot;
}

void exec_ts_scan_close(TsScanState *state)
{
    if (state == NULL) return;

    ts_scan_internal_t *internal = (ts_scan_internal_t *)state->ps.state;
    if (internal) {
        if (internal->engine_opened && internal->engine) {
            ts_engine_close(internal->engine);
        }
        ts_engine_free_results(&internal->results);
        free(internal);
    }
    free(state);
}