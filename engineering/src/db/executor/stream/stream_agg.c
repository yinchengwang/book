/**
 * @file stream_agg.c
 * @brief 流聚合算子实现
 *
 * 实现 Volcano 迭代器协议的流聚合算子。
 * 按 window_size 分批聚合，每批内计算指定聚合函数。
 * 支持 AVG、MAX、MIN、SUM、COUNT 五种聚合类型。
 * 返回 2 列：window_end (int64), agg_result (double)
 */
#include "db/executor/stream/stream_agg.h"
#include "db/stream_engine.h"
#include <stdlib.h>
#include <string.h>

/* 流聚合内部状态 */
typedef struct stream_agg_internal_s {
    stream_engine_db_t *engine;      /**< 流引擎句柄 */
    stream_record_t *buffer;         /**< 查询结果缓冲区 */
    uint32_t buffer_count;           /**< 缓冲区中记录数 */
    uint32_t current_index;          /**< 当前记录索引 */
    int agg_type;                    /**< 聚合类型：0=AVG, 1=MAX, 2=MIN, 3=SUM, 4=COUNT */
    int64_t window_size;             /**< 窗口大小（毫秒） */
    double acc_value;                /**< 累积值 */
    int64_t acc_count;               /**< 累积计数 */
    bool done;                       /**< 是否已完成 */
    bool queried;                    /**< 是否已执行查询 */
    char *collection_name;           /**< 流名称 */

    /* 窗口计算状态 */
    int64_t current_window_end;      /**< 当前窗口结束时间 */
    double window_agg_result;        /**< 当前窗口聚合结果 */
} stream_agg_internal_t;

/**
 * @brief 按时间戳排序的比较函数
 */
static int compare_timestamp_agg(const void *a, const void *b)
{
    const stream_record_t *ra = (const stream_record_t *)a;
    const stream_record_t *rb = (const stream_record_t *)b;
    if (ra->timestamp < rb->timestamp) return -1;
    if (ra->timestamp > rb->timestamp) return 1;
    return 0;
}

StreamAggState *exec_stream_agg_init(PlanState *parent,
    int agg_type, int64_t window_size)
{
    StreamAggState *state = (StreamAggState *)calloc(1, sizeof(StreamAggState));
    if (state == NULL) return NULL;

    state->ps.type = EXEC_STREAM_AGG;
    state->ps.left = NULL;
    state->ps.right = NULL;
    if (parent) {
        state->ps.expr_context = parent->expr_context;
    }
    state->agg_type = agg_type;
    state->window_size = window_size > 0 ? window_size : 1000; /* 默认 1 秒 */
    state->acc_state = NULL;

    /* 分配内部状态 */
    stream_agg_internal_t *internal = (stream_agg_internal_t *)calloc(1, sizeof(stream_agg_internal_t));
    if (internal == NULL) {
        free(state);
        return NULL;
    }

    internal->done = false;
    internal->queried = false;
    internal->current_index = 0;
    internal->buffer_count = 0;
    internal->agg_type = state->agg_type;
    internal->window_size = state->window_size;
    internal->acc_value = 0.0;
    internal->acc_count = 0;
    internal->current_window_end = 0;
    internal->window_agg_result = 0.0;
    internal->collection_name = strdup("test_stream");

    internal->buffer = (stream_record_t *)calloc(1024, sizeof(stream_record_t));
    if (internal->buffer == NULL) {
        free(internal->collection_name);
        free(internal);
        free(state);
        return NULL;
    }

    state->ps.state = internal;

    /* 创建 2 列描述符：window_end (int64), agg_result (double) */
    ExecTupleDesc *desc = exec_make_tuple_desc(2);
    if (desc == NULL) {
        free(internal->buffer);
        free(internal->collection_name);
        free(internal);
        free(state);
        return NULL;
    }
    desc->attrs[0].name = "window_end";
    desc->attrs[0].attnum = 1;
    desc->attrs[0].type = 0; /* int64 */
    desc->attrs[0].len = sizeof(int64_t);
    desc->attrs[0].isnull = false;

    desc->attrs[1].name = "agg_result";
    desc->attrs[1].attnum = 2;
    desc->attrs[1].type = 1; /* double */
    desc->attrs[1].len = sizeof(double);
    desc->attrs[1].isnull = false;

    state->ps.ps_TupDesc = desc;

    return state;
}

/**
 * @brief 计算窗口内的聚合值
 */
static double compute_aggregate(stream_record_t *records, uint32_t start_idx,
                                uint32_t end_idx, int agg_type)
{
    if (start_idx >= end_idx) {
        return 0.0;
    }

    double result = 0.0;
    int64_t count = 0;
    double sum = 0.0;
    double max_val = records[start_idx].value;
    double min_val = records[start_idx].value;

    for (uint32_t i = start_idx; i < end_idx; i++) {
        double val = records[i].value;
        sum += val;
        count++;

        if (val > max_val) max_val = val;
        if (val < min_val) min_val = val;
    }

    switch (agg_type) {
        case 0: /* AVG */
            result = (count > 0) ? (sum / count) : 0.0;
            break;
        case 1: /* MAX */
            result = (count > 0) ? max_val : 0.0;
            break;
        case 2: /* MIN */
            result = (count > 0) ? min_val : 0.0;
            break;
        case 3: /* SUM */
            result = sum;
            break;
        case 4: /* COUNT */
            result = (double)count;
            break;
        default:
            result = 0.0;
            break;
    }

    return result;
}

TupleTableSlot *exec_stream_agg_next(StreamAggState *state)
{
    if (state == NULL) return NULL;

    stream_agg_internal_t *internal = (stream_agg_internal_t *)state->ps.state;
    if (internal == NULL || internal->done) return NULL;

    /* 首次调用时执行查询 */
    if (!internal->queried) {
        internal->engine = (stream_engine_db_t *)stream_engine_open(
            internal->collection_name, ACCESS_MODE_READ);
        if (internal->engine == NULL) {
            internal->done = true;
            return NULL;
        }

        /* 执行查询 */
        uint32_t out_count = 0;
        int rc = stream_engine_query_records(
            internal->engine, 0, internal->buffer, &out_count, 1024);

        if (rc != 0) {
            internal->done = true;
            return NULL;
        }

        internal->buffer_count = out_count;
        internal->queried = true;

        /* 按时间戳排序 */
        if (internal->buffer_count > 0) {
            qsort(internal->buffer, internal->buffer_count,
                  sizeof(stream_record_t), compare_timestamp_agg);
        }
    }

    if (internal->current_index >= internal->buffer_count) {
        internal->done = true;
        return NULL;
    }

    /* 计算当前窗口的边界 */
    stream_record_t *first_in_window = &internal->buffer[internal->current_index];
    int64_t window_start = (first_in_window->timestamp / internal->window_size) * internal->window_size;
    int64_t window_end = window_start + internal->window_size;

    /* 收集当前窗口内的所有记录 */
    uint32_t window_start_idx = internal->current_index;
    while (internal->current_index < internal->buffer_count) {
        stream_record_t *rec = &internal->buffer[internal->current_index];
        if (rec->timestamp >= window_end) {
            break;
        }
        internal->current_index++;
    }

    /* 计算聚合值 */
    double agg_result = compute_aggregate(internal->buffer, window_start_idx,
                                          internal->current_index, internal->agg_type);

    /* 创建并填充元组槽 */
    ExecTupleDesc *desc = exec_make_tuple_desc(2);
    if (desc == NULL) return NULL;

    TupleTableSlot *slot = exec_make_tuple_slot(desc);
    if (slot == NULL) {
        exec_drop_tuple_desc(desc);
        return NULL;
    }

    slot->tts_values[0] = (void *)(uintptr_t)window_end;
    slot->tts_isnull[0] = false;

    uint64_t agg_bits;
    memcpy(&agg_bits, &agg_result, sizeof(double));
    slot->tts_values[1] = (void *)(uintptr_t)agg_bits;
    slot->tts_isnull[1] = false;

    slot->tts_nvalid = 2;

    return slot;
}

void exec_stream_agg_close(StreamAggState *state)
{
    if (state == NULL) return;

    stream_agg_internal_t *internal = (stream_agg_internal_t *)state->ps.state;
    if (internal) {
        if (internal->engine) {
            stream_engine_close(internal->engine);
        }
        if (internal->buffer) {
            free(internal->buffer);
        }
        if (internal->collection_name) {
            free(internal->collection_name);
        }
        free(internal);
    }

    if (state->ps.ps_TupDesc) {
        exec_drop_tuple_desc(state->ps.ps_TupDesc);
    }

    free(state);
}