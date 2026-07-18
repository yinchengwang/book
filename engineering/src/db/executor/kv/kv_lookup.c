/**
 * @file kv_lookup.c
 * @brief KV 点查算子实现
 *
 * 实现 Volcano 迭代器协议的 KV 点查算子。
 * 直接调用 kv_engine_get() 进行单个键查找，返回单行结果。
 */
#include "db/executor/kv/kv_lookup.h"
#include "db/storage/kv/kv_engine.h"
#include <stdlib.h>
#include <string.h>

KvPointLookupState *exec_kv_lookup_init(PlanState *parent, void *key, size_t key_len)
{
    KvPointLookupState *state = (KvPointLookupState *)calloc(1, sizeof(KvPointLookupState));
    if (state == NULL) return NULL;

    state->ps.type = EXEC_VALUES_SCAN;
    state->ps.left = NULL;
    state->ps.right = NULL;
    if (parent) {
        state->ps.expr_context = parent->expr_context;
    }
    state->key = key;
    state->key_len = key_len;
    state->done = false;

    return state;
}

TupleTableSlot *exec_kv_lookup_next(KvPointLookupState *state)
{
    if (state == NULL || state->done) return NULL;
    state->done = true;

    /* 打开 KV 引擎并执行点查 */
    kv_engine_db_t *engine = (kv_engine_db_t *)kv_engine_open("default", ACCESS_MODE_READ);
    if (engine == NULL) return NULL;

    void *value = NULL;
    size_t value_len = 0;

    int rc = kv_engine_get(engine, state->key, state->key_len, &value, &value_len);
    if (rc != 0 || value == NULL) {
        kv_engine_close(engine);
        return NULL;
    }

    /* 创建元组槽 */
    ExecTupleDesc *desc = exec_make_tuple_desc(3);
    if (desc == NULL) {
        kv_engine_close(engine);
        return NULL;
    }

    TupleTableSlot *slot = exec_make_tuple_slot(desc);
    if (slot == NULL) {
        exec_drop_tuple_desc(desc);
        kv_engine_close(engine);
        return NULL;
    }

    slot->tts_values[0] = state->key;
    slot->tts_values[1] = value;
    slot->tts_values[2] = (void *)(uintptr_t)value_len;
    slot->tts_nvalid = 3;

    kv_engine_close(engine);
    return slot;
}

void exec_kv_lookup_close(KvPointLookupState *state)
{
    if (state) free(state);
}