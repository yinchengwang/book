/**
 * @file stream_join.c
 * @brief 流连接算子实现
 *
 * 实现 Volcano 迭代器协议的流连接算子。
 * 支持流-流连接（stream-stream）和流-表连接（stream-table）。
 * 对于左流中的每条记录，在右流中查找时间戳在 [ts - interval, ts + interval] 范围内的记录。
 * 返回 4 列：left_timestamp (int64), left_value (double), right_timestamp (int64), right_value (double)
 *
 * 优化：右流数据按时间戳排序后使用二分查找定位时间窗口起始位置，
 * 将原始嵌套循环 O(n*m) 优化为 O(n*log m + k)，其中 k 为匹配记录数。
 */
#include "db/executor/stream/stream_join.h"
#include "db/stream_engine.h"
#include <stdlib.h>
#include <string.h>

/* 流连接内部状态 */
typedef struct stream_join_internal_s {
    stream_engine_db_t *left_engine;     /**< 左流引擎 */
    stream_engine_db_t *right_engine;    /**< 右流引擎 */
    stream_record_t *left_buffer;        /**< 左流缓冲区 */
    stream_record_t *right_buffer;       /**< 右流缓冲区 */
    uint32_t left_count;                 /**< 左流记录数 */
    uint32_t right_count;                /**< 右流记录数 */
    uint32_t left_index;                 /**< 左流当前索引 */
    uint32_t right_index;                /**< 右流当前索引 */
    int64_t interval;                    /**< 连接时间窗口（毫秒） */
    int join_type;                       /**< 连接类型：0=stream-stream, 1=stream-table */
    bool done;                           /**< 是否已完成 */
    bool queried;                        /**< 是否已执行查询 */
    bool left_changed;                   /**< 左记录是否已变化（触发二分查找重新定位） */
    char *left_name;                     /**< 左流名称 */
    char *right_name;                    /**< 右流名称 */
} stream_join_internal_t;

StreamJoinState *exec_stream_join_init(PlanState *parent,
    int64_t interval, int join_type)
{
    StreamJoinState *state = (StreamJoinState *)calloc(1, sizeof(StreamJoinState));
    if (state == NULL) return NULL;

    state->ps.type = EXEC_STREAM_JOIN;
    state->ps.left = NULL;
    state->ps.right = NULL;
    if (parent) {
        state->ps.expr_context = parent->expr_context;
    }
    state->interval = interval > 0 ? interval : 1000; /* 默认 1 秒 */
    state->join_type = join_type;

    /* 分配内部状态 */
    stream_join_internal_t *internal = (stream_join_internal_t *)calloc(1, sizeof(stream_join_internal_t));
    if (internal == NULL) {
        free(state);
        return NULL;
    }

    internal->done = false;
    internal->queried = false;
    internal->left_index = 0;
    internal->right_index = 0;
    internal->left_count = 0;
    internal->right_count = 0;
    internal->interval = state->interval;
    internal->join_type = state->join_type;
    internal->left_changed = false;
    internal->left_name = strdup("stream_left");
    internal->right_name = strdup("stream_right");

    /* 分配缓冲区 */
    internal->left_buffer = (stream_record_t *)calloc(1024, sizeof(stream_record_t));
    internal->right_buffer = (stream_record_t *)calloc(1024, sizeof(stream_record_t));
    if (internal->left_buffer == NULL || internal->right_buffer == NULL) {
        free(internal->left_buffer);
        free(internal->right_buffer);
        free(internal->left_name);
        free(internal->right_name);
        free(internal);
        free(state);
        return NULL;
    }

    state->ps.state = internal;

    /* 创建 4 列描述符 */
    ExecTupleDesc *desc = exec_make_tuple_desc(4);
    if (desc == NULL) {
        free(internal->left_buffer);
        free(internal->right_buffer);
        free(internal->left_name);
        free(internal->right_name);
        free(internal);
        free(state);
        return NULL;
    }
    desc->attrs[0].name = "left_timestamp";
    desc->attrs[0].attnum = 1;
    desc->attrs[0].type = 0;
    desc->attrs[0].len = sizeof(int64_t);
    desc->attrs[0].isnull = false;

    desc->attrs[1].name = "left_value";
    desc->attrs[1].attnum = 2;
    desc->attrs[1].type = 1;
    desc->attrs[1].len = sizeof(double);
    desc->attrs[1].isnull = false;

    desc->attrs[2].name = "right_timestamp";
    desc->attrs[2].attnum = 3;
    desc->attrs[2].type = 0;
    desc->attrs[2].len = sizeof(int64_t);
    desc->attrs[2].isnull = false;

    desc->attrs[3].name = "right_value";
    desc->attrs[3].attnum = 4;
    desc->attrs[3].type = 1;
    desc->attrs[3].len = sizeof(double);
    desc->attrs[3].isnull = false;

    state->ps.ps_TupDesc = desc;

    return state;
}

/**
 * @brief 设置连接的两侧流名称
 */
void stream_join_set_streams(StreamJoinState *state, const char *left_name, const char *right_name)
{
    if (state == NULL) return;
    stream_join_internal_t *internal = (stream_join_internal_t *)state->ps.state;
    if (internal == NULL) return;

    if (left_name) {
        free(internal->left_name);
        internal->left_name = strdup(left_name);
    }
    if (right_name) {
        free(internal->right_name);
        internal->right_name = strdup(right_name);
    }
}

TupleTableSlot *exec_stream_join_next(StreamJoinState *state)
{
    if (state == NULL) return NULL;

    stream_join_internal_t *internal = (stream_join_internal_t *)state->ps.state;
    if (internal == NULL || internal->done) return NULL;

    /* 首次调用时执行查询，加载两侧数据 */
    if (!internal->queried) {
        internal->left_engine = (stream_engine_db_t *)stream_engine_open(
            internal->left_name, ACCESS_MODE_READ);
        internal->right_engine = (stream_engine_db_t *)stream_engine_open(
            internal->right_name, ACCESS_MODE_READ);

        if (internal->left_engine == NULL || internal->right_engine == NULL) {
            internal->done = true;
            return NULL;
        }

        /* 查询左流 */
        uint32_t left_out = 0;
        int rc = stream_engine_query_records(
            internal->left_engine, 0, internal->left_buffer, &left_out, 1024);
        if (rc != 0) {
            internal->done = true;
            return NULL;
        }
        internal->left_count = left_out;

        /* 查询右流 */
        uint32_t right_out = 0;
        rc = stream_engine_query_records(
            internal->right_engine, 0, internal->right_buffer, &right_out, 1024);
        if (rc != 0) {
            internal->done = true;
            return NULL;
        }
        internal->right_count = right_out;

        /* 按时间戳排序右流，启用二分查找 */
        if (internal->right_count > 1) {
            /* 简单插入排序（数据量不大时足够） */
            for (uint32_t i = 1; i < internal->right_count; i++) {
                stream_record_t key = internal->right_buffer[i];
                int64_t j = (int64_t)i - 1;
                while (j >= 0 && internal->right_buffer[j].timestamp > key.timestamp) {
                    internal->right_buffer[j + 1] = internal->right_buffer[j];
                    j--;
                }
                internal->right_buffer[j + 1] = key;
            }
        }

        internal->queried = true;
        internal->left_index = 0;
        internal->right_index = 0;
    }

    /* 对左流每条记录，在右流中用二分查找定位时间窗口起始位置 */
    while (internal->left_index < internal->left_count) {
        stream_record_t *left_rec = &internal->left_buffer[internal->left_index];
        int64_t left_ts = left_rec->timestamp;
        int64_t window_start = left_ts - internal->interval;
        int64_t window_end = left_ts + internal->interval;

        /* 首次处理此左记录时，二分查找定位起始位置 */
        if (internal->right_index == 0 || internal->left_changed) {
            /* 二分查找第一个 >= window_start 的位置 */
            int lo = 0, hi = (int)internal->right_count;
            while (lo < hi) {
                int mid = lo + (hi - lo) / 2;
                if (internal->right_buffer[mid].timestamp < window_start) {
                    lo = mid + 1;
                } else {
                    hi = mid;
                }
            }
            internal->right_index = (uint32_t)lo;
            internal->left_changed = false;
        }

        /* 从起始位置顺序扫描匹配（已时间序，超出 window_end 即终止） */
        while (internal->right_index < internal->right_count) {
            stream_record_t *right_rec = &internal->right_buffer[internal->right_index];
            int64_t right_ts = right_rec->timestamp;

            /* 超出时间窗口上限 */
            if (right_ts > window_end) break;

            internal->right_index++;

            /* 创建并填充元组槽 */
            ExecTupleDesc *desc = exec_make_tuple_desc(4);
            if (desc == NULL) return NULL;

            TupleTableSlot *slot = exec_make_tuple_slot(desc);
            if (slot == NULL) {
                exec_drop_tuple_desc(desc);
                return NULL;
            }

            /* 填充左流值 */
            slot->tts_values[0] = (void *)(uintptr_t)left_rec->timestamp;
            slot->tts_isnull[0] = false;

            uint64_t left_val_bits;
            memcpy(&left_val_bits, &left_rec->value, sizeof(double));
            slot->tts_values[1] = (void *)(uintptr_t)left_val_bits;
            slot->tts_isnull[1] = false;

            /* 填充右流值 */
            slot->tts_values[2] = (void *)(uintptr_t)right_rec->timestamp;
            slot->tts_isnull[2] = false;

            uint64_t right_val_bits;
            memcpy(&right_val_bits, &right_rec->value, sizeof(double));
            slot->tts_values[3] = (void *)(uintptr_t)right_val_bits;
            slot->tts_isnull[3] = false;

            slot->tts_nvalid = 4;

            return slot;
        }

        /* 移动到左流下一条记录 */
        internal->left_index++;
        internal->left_changed = true;
    }

    internal->done = true;
    return NULL;
}

void exec_stream_join_close(StreamJoinState *state)
{
    if (state == NULL) return;

    stream_join_internal_t *internal = (stream_join_internal_t *)state->ps.state;
    if (internal) {
        if (internal->left_engine) {
            stream_engine_close(internal->left_engine);
        }
        if (internal->right_engine) {
            stream_engine_close(internal->right_engine);
        }
        if (internal->left_buffer) {
            free(internal->left_buffer);
        }
        if (internal->right_buffer) {
            free(internal->right_buffer);
        }
        if (internal->left_name) {
            free(internal->left_name);
        }
        if (internal->right_name) {
            free(internal->right_name);
        }
        free(internal);
    }

    if (state->ps.ps_TupDesc) {
        exec_drop_tuple_desc(state->ps.ps_TupDesc);
    }

    free(state);
}
