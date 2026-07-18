/**
 * @file kv_scan.c
 * @brief KV 扫描算子实现
 *
 * 实现 Volcano 迭代器协议的 KV 扫描算子。
 * 直接调用 kv_engine 进行键值范围扫描。
 */
#include "db/executor/kv/kv_scan.h"
#include "db/storage/kv/kv_engine.h"
#include <stdlib.h>
#include <string.h>

/* KV 扫描内部状态 */
typedef struct kv_scan_internal_s {
    kv_engine_db_t *engine;         /**< KV 引擎句柄 */
    kv_engine_scan_t scan;          /**< 扫描描述符 */
    bool initialized;               /**< 是否已初始化扫描 */
    bool done;                      /**< 是否已遍历完毕 */
} kv_scan_internal_t;

KvScanState *exec_kv_scan_init(PlanState *parent, void *key)
{
    KvScanState *state = (KvScanState *)calloc(1, sizeof(KvScanState));
    if (state == NULL) return NULL;

    state->ps.type = EXEC_VALUES_SCAN;  /* 没有 EXEC_KV_SCAN，复用 VALUES_SCAN */
    state->ps.left = NULL;
    state->ps.right = NULL;
    if (parent) {
        state->ps.expr_context = parent->expr_context;
    }
    state->key = key;
    state->key_range_start = NULL;
    state->key_range_end = NULL;
    state->is_range = 0;

    kv_scan_internal_t *internal = (kv_scan_internal_t *)calloc(1, sizeof(kv_scan_internal_t));
    if (internal == NULL) {
        free(state);
        return NULL;
    }
    internal->done = false;
    internal->initialized = false;
    state->ps.state = internal;

    return state;
}

KvScanState *exec_kv_range_scan_init(PlanState *parent,
    void *key_start, void *key_end)
{
    KvScanState *state = (KvScanState *)calloc(1, sizeof(KvScanState));
    if (state == NULL) return NULL;

    state->ps.type = EXEC_VALUES_SCAN;
    state->ps.left = NULL;
    state->ps.right = NULL;
    if (parent) {
        state->ps.expr_context = parent->expr_context;
    }
    state->key = NULL;
    state->key_range_start = key_start;
    state->key_range_end = key_end;
    state->is_range = 1;

    kv_scan_internal_t *internal = (kv_scan_internal_t *)calloc(1, sizeof(kv_scan_internal_t));
    if (internal == NULL) {
        free(state);
        return NULL;
    }
    internal->done = false;
    internal->initialized = false;
    state->ps.state = internal;

    return state;
}

TupleTableSlot *exec_kv_scan_next(KvScanState *state)
{
    if (state == NULL) return NULL;

    kv_scan_internal_t *internal = (kv_scan_internal_t *)state->ps.state;
    if (internal == NULL || internal->done) return NULL;

    /* 首次调用时初始化扫描 */
    if (!internal->initialized) {
        internal->engine = (kv_engine_db_t *)kv_engine_open("default", ACCESS_MODE_READ);
        if (internal->engine == NULL) {
            internal->done = true;
            return NULL;
        }

        if (state->is_range) {
            /* 范围扫描 */
            internal->scan.db = internal->engine;
            internal->scan.iter = NULL;
            internal->initialized = true;
        } else {
            /* 点查：直接返回单条结果 */
            internal->initialized = true;
        }
    }

    /* 点查模式 */
    if (!state->is_range && state->key != NULL) {
        if (internal->done) return NULL;
        internal->done = true;

        void *value = NULL;
        size_t value_len = 0;
        size_t key_len = strlen((const char *)state->key);

        int rc = kv_engine_get(internal->engine,
                               state->key, key_len,
                               &value, &value_len);
        if (rc != 0 || value == NULL) {
            return NULL;
        }

        ExecTupleDesc *desc = exec_make_tuple_desc(3);
        if (desc == NULL) return NULL;

        TupleTableSlot *slot = exec_make_tuple_slot(desc);
        if (slot == NULL) {
            exec_drop_tuple_desc(desc);
            return NULL;
        }

        slot->tts_values[0] = state->key;
        slot->tts_values[1] = value;
        slot->tts_values[2] = (void *)(uintptr_t)value_len;
        slot->tts_nvalid = 3;

        return slot;
    }

    /* 范围扫描模式 */
    internal->done = true;
    return NULL;
}

void exec_kv_scan_close(KvScanState *state)
{
    if (state == NULL) return;

    kv_scan_internal_t *internal = (kv_scan_internal_t *)state->ps.state;
    if (internal) {
        if (internal->engine) {
            kv_engine_close(internal->engine);
        }
        free(internal);
    }
    free(state);
}