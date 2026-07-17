/**
 * @file vector_scan.h
 * @brief 向量扫描算子接口
 *
 * 向量扫描算子直接调用 vector_engine，不走 Access Method 层。
 * 实现将在 Phase 2 完成。
 */
#ifndef DB_EXECUTOR_VECTOR_SCAN_H
#define DB_EXECUTOR_VECTOR_SCAN_H

#include "db/sql/sql_executor.h"  /* PlanState, TupleTableSlot */

#ifdef __cplusplus
extern "C" {
#endif

/** 向量扫描状态 */
typedef struct VectorScanState_s {
    PlanState ps;            /**< 基类 PlanState */
    float    *query_vector;  /**< 查询向量 */
    int       top_k;         /**< Top-K 参数 */
    int       distance_metric; /**< 距离度量：0=L2, 1=IP, 2=COSINE */
    void     *index;         /**< 向量索引句柄 */
    int       ef;            /**< HNSW 搜索参数 */
    int       nprobe;        /**< IVF 搜索参数 */
} VectorScanState;

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