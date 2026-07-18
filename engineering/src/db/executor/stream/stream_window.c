/**
 * @file stream_window.c
 * @brief 流窗口算子实现
 *
 * 实现 Volcano 迭代器协议的流窗口计算算子。
 * 支持 Tumbling（滚动）、Sliding（滑动）、Session（会话）三种窗口类型。
 * 返回 3 列：window_start (int64), window_end (int64), value_count (int32)
 */
#include "db/executor/stream/stream_window.h"
#include "db/stream_engine.h"
#include <stdlib.h>
#include <string.h>

/* 流窗口内部状态 */
typedef struct stream_window_internal_s {
    stream_engine_db_t *engine;      /**< 流引擎句柄 */
    stream_record_t *buffer;         /**< 查询结果缓冲区 */
    uint32_t buffer_count;           /**< 缓冲区中记录数 */
    uint32_t current_index;          /**< 当前记录索引 */
    int64_t window_size;             /**< 窗口大小（毫秒） */
    int64_t slide;                   /**< 滑动步长（毫秒） */
    int window_type;                 /**< 窗口类型：0=tumbling, 1=sliding, 2=session */
    int64_t session_gap;             /**< 会话间隙（毫秒） */
    bool done;                       /**< 是否已完成 */
    bool queried;                    /**< 是否已执行查询 */
    char *collection_name;           /**< 流名称 */

    /* 窗口计算状态 */
    int64_t current_window_start;    /**< 当前窗口起始时间 */
    int64_t current_window_end;      /**< 当前窗口结束时间 */
    int32_t current_window_count;    /**< 当前窗口内记录数 */
} stream_window_internal_t;

StreamWindowState *exec_stream_window_init(PlanState *parent,
    int64_t window_size, int64_t slide, int window_type)
{
    StreamWindowState *state = (StreamWindowState *)calloc(1, sizeof(StreamWindowState));
    if (state == NULL) return NULL;

    state->ps.type = EXEC_STREAM_WINDOW;
    state->ps.left = NULL;
    state->ps.right = NULL;
    if (parent) {
        state->ps.expr_context = parent->expr_context;
    }
    state->window_size = window_size > 0 ? window_size : 1000; /* 默认 1 秒 */
    state->slide = slide > 0 ? slide : window_size; /* 默认等于窗口大小（滚动窗口） */
    state->window_type = window_type;

    /* 分配内部状态 */
    stream_window_internal_t *internal = (stream_window_internal_t *)calloc(1, sizeof(stream_window_internal_t));
    if (internal == NULL) {
        free(state);
        return NULL;
    }

    internal->done = false;
    internal->queried = false;
    internal->current_index = 0;
    internal->window_size = state->window_size;
    internal->slide = state->slide;
    internal->window_type = state->window_type;
    internal->session_gap = window_size; /* 会话间隙默认等于窗口大小 */
    internal->collection_name = strdup("test_stream");
    internal->buffer = (stream_record_t *)calloc(1024, sizeof(stream_record_t));
    if (internal->buffer == NULL) {
        free(internal->collection_name);
        free(internal);
        free(state);
        return NULL;
    }

    internal->current_window_start = 0;
    internal->current_window_end = 0;
    internal->current_window_count = 0;

    state->ps.state = internal;

    /* 创建 3 列描述符 */
    ExecTupleDesc *desc = exec_make_tuple_desc(3);
    if (desc == NULL) {
        free(internal->buffer);
        free(internal->collection_name);
        free(internal);
        free(state);
        return NULL;
    }
    desc->attrs[0].name = "window_start";
    desc->attrs[0].attnum = 1;
    desc->attrs[0].type = 0; /* int64 */
    desc->attrs[0].len = sizeof(int64_t);
    desc->attrs[0].isnull = false;

    desc->attrs[1].name = "window_end";
    desc->attrs[1].attnum = 2;
    desc->attrs[1].type = 0; /* int64 */
    desc->attrs[1].len = sizeof(int64_t);
    desc->attrs[1].isnull = false;

    desc->attrs[2].name = "value_count";
    desc->attrs[2].attnum = 3;
    desc->attrs[2].type = 1; /* int32 */
    desc->attrs[2].len = sizeof(int32_t);
    desc->attrs[2].isnull = false;

    state->ps.ps_TupDesc = desc;

    return state;
}

/**
 * @brief 对记录按时间戳排序的比较函数
 */
static int compare_timestamp(const void *a, const void *b)
{
    const stream_record_t *ra = (const stream_record_t *)a;
    const stream_record_t *rb = (const stream_record_t *)b;
    if (ra->timestamp < rb->timestamp) return -1;
    if (ra->timestamp > rb->timestamp) return 1;
    return 0;
}

/**
 * @brief 计算 Tumbling 窗口
 */
static TupleTableSlot *compute_tumbling_window(StreamWindowState *state, stream_window_internal_t *internal)
{
    if (internal->current_index >= internal->buffer_count) {
        internal->done = true;
        return NULL;
    }

    /* 初始化窗口 */
    if (internal->current_window_end == 0) {
        stream_record_t *first = &internal->buffer[0];
        internal->current_window_start = (first->timestamp / internal->window_size) * internal->window_size;
        internal->current_window_end = internal->current_window_start + internal->window_size;
        internal->current_window_count = 0;
    }

    /* 检查是否需要前进到下一个窗口 */
    while (internal->current_index < internal->buffer_count) {
        stream_record_t *record = &internal->buffer[internal->current_index];

        if (record->timestamp < internal->current_window_end) {
            /* 记录属于当前窗口 */
            internal->current_window_count++;
            internal->current_index++;
        } else {
            /* 输出当前窗口并移动到下一个窗口 */
            ExecTupleDesc *desc = exec_make_tuple_desc(3);
            if (desc == NULL) return NULL;

            TupleTableSlot *slot = exec_make_tuple_slot(desc);
            if (slot == NULL) {
                exec_drop_tuple_desc(desc);
                return NULL;
            }

            slot->tts_values[0] = (void *)(uintptr_t)internal->current_window_start;
            slot->tts_values[1] = (void *)(uintptr_t)internal->current_window_end;
            slot->tts_values[2] = (void *)(uintptr_t)(int64_t)internal->current_window_count;
            slot->tts_nvalid = 3;

            /* 移动到下一个窗口 */
            internal->current_window_start = internal->current_window_end;
            internal->current_window_end += internal->window_size;
            internal->current_window_count = 0;

            return slot;
        }
    }

    /* 输出最后一个窗口 */
    if (internal->current_window_count > 0) {
        ExecTupleDesc *desc = exec_make_tuple_desc(3);
        if (desc == NULL) return NULL;

        TupleTableSlot *slot = exec_make_tuple_slot(desc);
        if (slot == NULL) {
            exec_drop_tuple_desc(desc);
            return NULL;
        }

        slot->tts_values[0] = (void *)(uintptr_t)internal->current_window_start;
        slot->tts_values[1] = (void *)(uintptr_t)internal->current_window_end;
        slot->tts_values[2] = (void *)(uintptr_t)(int64_t)internal->current_window_count;
        slot->tts_nvalid = 3;

        internal->done = true;
        return slot;
    }

    internal->done = true;
    return NULL;
}

/**
 * @brief 计算 Sliding 窗口
 */
static TupleTableSlot *compute_sliding_window(StreamWindowState *state, stream_window_internal_t *internal)
{
    if (internal->current_index >= internal->buffer_count) {
        internal->done = true;
        return NULL;
    }

    /* 初始化窗口 */
    if (internal->current_window_end == 0) {
        stream_record_t *first = &internal->buffer[0];
        internal->current_window_start = first->timestamp;
        internal->current_window_end = internal->current_window_start + internal->window_size;
        internal->current_window_count = 0;
    }

    /* 计算当前窗口内的记录数 */
    internal->current_window_count = 0;
    for (uint32_t i = 0; i < internal->buffer_count; i++) {
        stream_record_t *record = &internal->buffer[i];
        if (record->timestamp >= internal->current_window_start &&
            record->timestamp < internal->current_window_end) {
            internal->current_window_count++;
        }
    }

    /* 输出当前窗口 */
    ExecTupleDesc *desc = exec_make_tuple_desc(3);
    if (desc == NULL) return NULL;

    TupleTableSlot *slot = exec_make_tuple_slot(desc);
    if (slot == NULL) {
        exec_drop_tuple_desc(desc);
        return NULL;
    }

    slot->tts_values[0] = (void *)(uintptr_t)internal->current_window_start;
    slot->tts_values[1] = (void *)(uintptr_t)internal->current_window_end;
    slot->tts_values[2] = (void *)(uintptr_t)(int64_t)internal->current_window_count;
    slot->tts_nvalid = 3;

    /* 移动到下一个窗口 */
    internal->current_window_start += internal->slide;
    internal->current_window_end += internal->slide;
    internal->current_window_count = 0;

    /* 检查是否已完成 */
    if (internal->current_window_start >= internal->buffer[internal->buffer_count - 1].timestamp) {
        internal->done = true;
    }

    return slot;
}

/**
 * @brief 计算 Session 窗口
 */
static TupleTableSlot *compute_session_window(StreamWindowState *state, stream_window_internal_t *internal)
{
    if (internal->current_index >= internal->buffer_count) {
        internal->done = true;
        return NULL;
    }

    /* 开始新会话 */
    stream_record_t *first = &internal->buffer[internal->current_index];
    internal->current_window_start = first->timestamp;
    internal->current_window_end = first->timestamp;
    internal->current_window_count = 1;
    internal->current_index++;

    /* 扩展会话直到超过间隙 */
    while (internal->current_index < internal->buffer_count) {
        stream_record_t *record = &internal->buffer[internal->current_index];

        if (record->timestamp - internal->current_window_end <= internal->session_gap) {
            /* 属于同一会话 */
            internal->current_window_end = record->timestamp;
            internal->current_window_count++;
            internal->current_index++;
        } else {
            /* 会话结束 */
            break;
        }
    }

    /* 输出会话窗口 */
    ExecTupleDesc *desc = exec_make_tuple_desc(3);
    if (desc == NULL) return NULL;

    TupleTableSlot *slot = exec_make_tuple_slot(desc);
    if (slot == NULL) {
        exec_drop_tuple_desc(desc);
        return NULL;
    }

    slot->tts_values[0] = (void *)(uintptr_t)internal->current_window_start;
    slot->tts_values[1] = (void *)(uintptr_t)internal->current_window_end;
    slot->tts_values[2] = (void *)(uintptr_t)(int64_t)internal->current_window_count;
    slot->tts_nvalid = 3;

    return slot;
}

TupleTableSlot *exec_stream_window_next(StreamWindowState *state)
{
    if (state == NULL) return NULL;

    stream_window_internal_t *internal = (stream_window_internal_t *)state->ps.state;
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
                  sizeof(stream_record_t), compare_timestamp);
        }
    }

    /* 根据窗口类型计算窗口 */
    switch (internal->window_type) {
        case 0: /* Tumbling */
            return compute_tumbling_window(state, internal);
        case 1: /* Sliding */
            return compute_sliding_window(state, internal);
        case 2: /* Session */
            return compute_session_window(state, internal);
        default:
            internal->done = true;
            return NULL;
    }
}

void exec_stream_window_close(StreamWindowState *state)
{
    if (state == NULL) return;

    stream_window_internal_t *internal = (stream_window_internal_t *)state->ps.state;
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