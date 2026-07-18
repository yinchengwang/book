/**
 * @file stream_scan.c
 * @brief 流扫描算子实现
 *
 * 实现 Volcano 迭代器协议的流扫描算子。
 * 首次 next 调用时打开引擎，执行 stream_engine_query()，逐行返回。
 * 返回 3 列：timestamp (int64), value (double), tag (const char*)
 */
#include "db/executor/stream/stream_scan.h"
#include "db/stream_engine.h"
#include <stdlib.h>
#include <string.h>

/* 流扫描内部状态 */
typedef struct stream_scan_internal_s {
    stream_engine_db_t *engine;      /**< 流引擎句柄 */
    stream_record_t *buffer;         /**< 查询结果缓冲区 */
    uint32_t buffer_count;           /**< 缓冲区中记录数 */
    uint32_t num_results;            /**< 总结果数 */
    uint32_t current_index;          /**< 当前结果索引 */
    bool done;                       /**< 是否已返回所有结果 */
    bool queried;                    /**< 是否已执行查询 */
    char *collection_name;           /**< 流名称 */
} stream_scan_internal_t;

StreamScanState *exec_stream_scan_init(PlanState *parent,
    void *stream_oid, int64_t watermark, int batch_size)
{
    StreamScanState *state = (StreamScanState *)calloc(1, sizeof(StreamScanState));
    if (state == NULL) return NULL;

    state->ps.type = EXEC_STREAM_SCAN;
    state->ps.left = NULL;
    state->ps.right = NULL;
    if (parent) {
        state->ps.expr_context = parent->expr_context;
    }
    state->stream_oid = stream_oid;
    state->watermark = watermark;
    state->batch_size = batch_size > 0 ? batch_size : 100;

    /* 分配内部状态 */
    stream_scan_internal_t *internal = (stream_scan_internal_t *)calloc(1, sizeof(stream_scan_internal_t));
    if (internal == NULL) {
        free(state);
        return NULL;
    }

    internal->done = false;
    internal->queried = false;
    internal->current_index = 0;
    internal->num_results = 0;
    internal->buffer_count = 0;
    internal->buffer = NULL;
    internal->collection_name = NULL;

    /* 从 stream_oid 解析流名称 */
    if (stream_oid != NULL) {
        internal->collection_name = strdup((const char *)stream_oid);
    } else {
        internal->collection_name = strdup("test_stream");
    }

    /* 分配结果缓冲区 */
    internal->buffer = (stream_record_t *)calloc(state->batch_size, sizeof(stream_record_t));
    if (internal->buffer == NULL) {
        free(internal->collection_name);
        free(internal);
        free(state);
        return NULL;
    }

    state->ps.state = internal;

    /* 创建 3 列描述符：timestamp (int64), value (double), tag (const char*) */
    ExecTupleDesc *desc = exec_make_tuple_desc(3);
    if (desc == NULL) {
        exec_stream_scan_close(state);
        return NULL;
    }
    desc->attrs[0].name = "timestamp";
    desc->attrs[0].attnum = 1;
    desc->attrs[0].type = 0; /* int64 */
    desc->attrs[0].len = sizeof(int64_t);
    desc->attrs[0].isnull = false;

    desc->attrs[1].name = "value";
    desc->attrs[1].attnum = 2;
    desc->attrs[1].type = 1; /* double */
    desc->attrs[1].len = sizeof(double);
    desc->attrs[1].isnull = false;

    desc->attrs[2].name = "tag";
    desc->attrs[2].attnum = 3;
    desc->attrs[2].type = 2; /* string */
    desc->attrs[2].len = 64;
    desc->attrs[2].isnull = false;

    state->ps.ps_TupDesc = desc;

    return state;
}

TupleTableSlot *exec_stream_scan_next(StreamScanState *state)
{
    if (state == NULL) return NULL;

    stream_scan_internal_t *internal = (stream_scan_internal_t *)state->ps.state;
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
            internal->engine, state->watermark,
            internal->buffer, &out_count, (uint32_t)state->batch_size);

        if (rc != 0) {
            internal->done = true;
            return NULL;
        }

        internal->num_results = out_count;
        internal->current_index = 0;
        internal->queried = true;
    }

    /* 遍历结果 */
    if (internal->current_index >= internal->num_results) {
        internal->done = true;
        return NULL;
    }

    stream_record_t *record = &internal->buffer[internal->current_index];
    internal->current_index++;

    /* 创建并填充元组槽 */
    ExecTupleDesc *desc = exec_make_tuple_desc(3);
    if (desc == NULL) return NULL;

    TupleTableSlot *slot = exec_make_tuple_slot(desc);
    if (slot == NULL) {
        exec_drop_tuple_desc(desc);
        return NULL;
    }

    /* 填充值 */
    /* 列 1: timestamp (int64) */
    slot->tts_values[0] = (void *)(uintptr_t)record->timestamp;
    slot->tts_isnull[0] = false;

    /* 列 2: value (double) — 通过位模式转换存储 */
    uint64_t val_bits;
    memcpy(&val_bits, &record->value, sizeof(double));
    slot->tts_values[1] = (void *)(uintptr_t)val_bits;
    slot->tts_isnull[1] = false;

    /* 列 3: tag (const char*) — 直接使用缓冲区中的字符串 */
    slot->tts_values[2] = record->tag;
    slot->tts_isnull[2] = false;

    slot->tts_nvalid = 3;

    return slot;
}

void exec_stream_scan_close(StreamScanState *state)
{
    if (state == NULL) return;

    stream_scan_internal_t *internal = (stream_scan_internal_t *)state->ps.state;
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