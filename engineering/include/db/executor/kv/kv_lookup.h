/**
 * @file kv_lookup.h
 * @brief KV 点查算子接口
 *
 * 实现 Volcano 迭代器协议的 KV 点查算子。
 * 直接调用 kv_engine_get() 进行单个键的查找。
 */
#ifndef DB_EXECUTOR_KV_LOOKUP_H
#define DB_EXECUTOR_KV_LOOKUP_H

#include "db/sql/sql_executor.h"  /* PlanState, TupleTableSlot */

#ifdef __cplusplus
extern "C" {
#endif

/** KV 点查状态 */
typedef struct KvPointLookupState_s {
    PlanState ps;         /**< 基类 PlanState */
    void     *key;        /**< 查询键 */
    size_t    key_len;    /**< 键长度 */
    bool      done;       /**< 是否已查询 */
} KvPointLookupState;

/**
 * @brief 初始化 KV 点查
 */
KvPointLookupState *exec_kv_lookup_init(PlanState *parent, void *key, size_t key_len);

/**
 * @brief 执行 KV 点查，返回结果行
 */
TupleTableSlot *exec_kv_lookup_next(KvPointLookupState *state);

/**
 * @brief 关闭 KV 点查
 */
void exec_kv_lookup_close(KvPointLookupState *state);

#ifdef __cplusplus
}
#endif

#endif /* DB_EXECUTOR_KV_LOOKUP_H */