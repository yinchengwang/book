/**
 * @file yang_scan.h
 * @brief Yang 路径扫描算子接口
 *
 * Yang 路径扫描算子直接调用 yang_engine，实现将在 Phase 9 完成。
 */
#ifndef DB_EXECUTOR_YANG_SCAN_H
#define DB_EXECUTOR_YANG_SCAN_H

#include "db/sql/sql_executor.h"  /* PlanState, TupleTableSlot */

#ifdef __cplusplus
extern "C" {
#endif

/** Yang 扫描状态 */
typedef struct YangScanState_s {
    PlanState ps;           /**< 基类 PlanState */
    void     *yang_path;    /**< Yang Path 表达式 */
    void     *xpath_filter; /**< XPath 过滤条件 */
} YangScanState;

/**
 * @brief 初始化 Yang 扫描
 */
YangScanState *exec_yang_scan_init(PlanState *parent,
    void *yang_path, void *xpath_filter);

/**
 * @brief 执行 Yang 扫描，返回下一行
 */
TupleTableSlot *exec_yang_scan_next(YangScanState *state);

/**
 * @brief 关闭 Yang 扫描
 */
void exec_yang_scan_close(YangScanState *state);

#ifdef __cplusplus
}
#endif

#endif /* DB_EXECUTOR_YANG_SCAN_H */