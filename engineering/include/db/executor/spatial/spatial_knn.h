/**
 * @file spatial_knn.h
 * @brief 空间 KNN 搜索算子接口
 *
 * 实现空间 KNN 搜索，实现将在 Phase 6 完成。
 */
#ifndef DB_EXECUTOR_SPATIAL_KNN_H
#define DB_EXECUTOR_SPATIAL_KNN_H

#include "db/sql/sql_executor.h"  /* PlanState, TupleTableSlot */

#ifdef __cplusplus
extern "C" {
#endif

/** 空间 KNN 搜索状态 */
typedef struct SpatialKnnState_s {
    PlanState ps;           /**< 基类 PlanState */
    void     *query_point;  /**< 查询点 */
    int       top_k;        /**< Top-K 参数 */
    void     *index;        /**< 空间索引句柄 */
} SpatialKnnState;

/**
 * @brief 初始化空间 KNN 搜索
 */
SpatialKnnState *exec_spatial_knn_init(PlanState *parent,
    void *query_point, int top_k);

/**
 * @brief 执行空间 KNN 搜索，返回下一行
 */
TupleTableSlot *exec_spatial_knn_next(SpatialKnnState *state);

/**
 * @brief 关闭空间 KNN 搜索
 */
void exec_spatial_knn_close(SpatialKnnState *state);

#ifdef __cplusplus
}
#endif

#endif /* DB_EXECUTOR_SPATIAL_KNN_H */