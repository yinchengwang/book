/**
 * @file yang_scan.h
 * @brief Yang 路径扫描算子接口
 *
 * Yang 路径扫描算子直接调用 yang_engine。
 * 注意：YangScanState 类型已在 sql_executor.h 中定义，
 * 本文件仅提供算子函数声明，避免类型重定义。
 */
#ifndef DB_EXECUTOR_YANG_SCAN_H
#define DB_EXECUTOR_YANG_SCAN_H

#include "db/sql/sql_executor.h"  /* PlanState, TupleTableSlot, YangScanState */

#ifdef __cplusplus
extern "C" {
#endif

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