/**
 * @file hnsw_scan.h
 * @brief HNSW 索引扫描算子接口
 *
 * HNSW 扫描算子直接调用 vector_engine 的 HNSW 搜索接口。
 * 注意：HnswScanState 已在 db/sql/sql_executor.h 中定义，
 * 此处仅提供算子生命周期函数声明。
 */
#ifndef DB_EXECUTOR_HNSW_SCAN_H
#define DB_EXECUTOR_HNSW_SCAN_H

#include "db/sql/sql_executor.h"  /* HnswScanState, TupleTableSlot */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 HNSW 扫描
 */
HnswScanState *exec_hnsw_scan_init(PlanState *parent,
    float *query_vector, int top_k, int distance_metric,
    int ef, int M);

/**
 * @brief 执行 HNSW 扫描，返回下一行
 */
TupleTableSlot *exec_hnsw_scan_next(HnswScanState *state);

/**
 * @brief 关闭 HNSW 扫描
 */
void exec_hnsw_scan_close(HnswScanState *state);

#ifdef __cplusplus
}
#endif

#endif /* DB_EXECUTOR_HNSW_SCAN_H */