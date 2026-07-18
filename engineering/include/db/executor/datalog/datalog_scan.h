/**
 * @file datalog_scan.h
 * @brief Datalog 扫描算子接口
 *
 * Datalog 扫描算子实现半朴素求值，实现将在 Phase 8 完成。
 *
 * 注意：DatalogScanState 类型已在 sql_executor.h 中定义，
 * 本文件仅提供算子函数声明，避免类型重定义。
 */
#ifndef DB_EXECUTOR_DATALOG_SCAN_H
#define DB_EXECUTOR_DATALOG_SCAN_H

#include "db/sql/sql_executor.h"  /* PlanState, TupleTableSlot, DatalogScanState */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 Datalog 扫描
 * @param parent 父 PlanState
 * @param rule_set 规则集合
 * @param edb 外延数据库（已知事实）
 * @param idb 内涵数据库（推导结果，可复用的缓冲）
 * @param max_iter 最大迭代次数（0 表示使用默认值 1000）
 */
DatalogScanState *exec_datalog_scan_init(PlanState *parent,
    void *rule_set, void *edb, void *idb, int max_iter);

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