/**
 * @file datalog_scan.h
 * @brief Datalog 扫描算子接口
 *
 * Datalog 扫描算子实现半朴素求值，实现将在 Phase 8 完成。
 */
#ifndef DB_EXECUTOR_DATALOG_SCAN_H
#define DB_EXECUTOR_DATALOG_SCAN_H

#include "db/sql/sql_executor.h"  /* PlanState, TupleTableSlot */

#ifdef __cplusplus
extern "C" {
#endif

/** Datalog 扫描状态 */
typedef struct DatalogScanState_s {
    PlanState ps;           /**< 基类 PlanState */
    void     *rule_set;     /**< 规则集合 */
    void     *edb;          /**< 外延数据库（事实） */
    void     *idb;          /**< 内涵数据库（推导结果） */
} DatalogScanState;

/**
 * @brief 初始化 Datalog 扫描
 */
DatalogScanState *exec_datalog_scan_init(PlanState *parent,
    void *rule_set, void *edb, void *idb);

/**
 * @brief 执行 Datalog 扫描，返回下一行
 */
TupleTableSlot *exec_datalog_scan_next(DatalogScanState *state);

/**
 * @brief 关闭 Datalog 扫描
 */
void exec_datalog_scan_close(DatalogScanState *state);

#ifdef __cplusplus
}
#endif

#endif /* DB_EXECUTOR_DATALOG_SCAN_H */