/**
 * @file vector_scan.h
 * @brief 向量扫描算子接口
 *
 * 向量扫描算子直接调用 vector_engine，不走 Access Method 层。
 * 注意：VectorScanState 已在 db/sql/sql_executor.h 中定义，
 * 此处仅提供算子生命周期函数声明。
 */
#ifndef DB_EXECUTOR_VECTOR_SCAN_H
#define DB_EXECUTOR_VECTOR_SCAN_H

#include "db/sql/sql_executor.h"  /* VectorScanState, TupleTableSlot */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化向量扫描
 */
VectorScanState *exec_vector_scan_init(PlanState *parent,
    float *query_vector, int top_k, int distance_metric);

/**
 * @brief 执行向量扫描，返回下一行
 */
TupleTableSlot *exec_vector_scan_next(VectorScanState *state);

/**
 * @brief 关闭向量扫描
 */
void exec_vector_scan_close(VectorScanState *state);

#ifdef __cplusplus
}
#endif

#endif /* DB_EXECUTOR_VECTOR_SCAN_H */