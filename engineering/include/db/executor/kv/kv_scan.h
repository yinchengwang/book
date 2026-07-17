/**
 * @file kv_scan.h
 * @brief KV 扫描算子接口
 *
 * KV 扫描算子直接调用 kv_engine，实现将在 Phase 3 完成。
 */
#ifndef DB_EXECUTOR_KV_SCAN_H
#define DB_EXECUTOR_KV_SCAN_H

#include "db/sql/sql_executor.h"  /* PlanState, TupleTableSlot */

#ifdef __cplusplus
extern "C" {
#endif

/** KV 扫描状态 */
typedef struct KvScanState_s {
    PlanState ps;         /**< 基类 PlanState */
    void     *key;        /**< 点查键 */
    void     *key_range_start; /**< 范围查询起始键 */
    void     *key_range_end;   /**< 范围查询结束键 */
    int       is_range;   /**< 是否为范围查询 */
} KvScanState;

/**
 * @brief 初始化 KV 扫描
 */
KvScanState *exec_kv_scan_init(PlanState *parent, void *key);

/**
 * @brief 初始化 KV 范围扫描
 */
KvScanState *exec_kv_range_scan_init(PlanState *parent,
    void *key_start, void *key_end);

/**
 * @brief 执行 KV 扫描，返回下一行
 */
TupleTableSlot *exec_kv_scan_next(KvScanState *state);

/**
 * @brief 关闭 KV 扫描
 */
void exec_kv_scan_close(KvScanState *state);

#ifdef __cplusplus
}
#endif

#endif /* DB_EXECUTOR_KV_SCAN_H */