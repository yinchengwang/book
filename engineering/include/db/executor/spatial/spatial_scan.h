/**
 * @file spatial_scan.h
 * @brief 空间扫描算子接口
 *
 * 空间扫描算子直接调用 spatial_engine，实现将在 Phase 6 完成。
 */
#ifndef DB_EXECUTOR_SPATIAL_SCAN_H
#define DB_EXECUTOR_SPATIAL_SCAN_H

#include "db/sql/sql_executor.h"  /* PlanState, TupleTableSlot */

#ifdef __cplusplus
extern "C" {
#endif

/** 空间扫描状态 */
typedef struct SpatialScanState_s {
    PlanState ps;           /**< 基类 PlanState */
    void     *geometry;     /**< 查询几何体 */
    int       spatial_op;   /**< 空间操作：0=contain, 1=within, 2=intersect */
    void     *bbox;         /**< 包围盒过滤 */
} SpatialScanState;

/**
 * @brief 初始化空间扫描
 */
SpatialScanState *exec_spatial_scan_init(PlanState *parent,
    void *geometry, int spatial_op, void *bbox);

/**
 * @brief 执行空间扫描，返回下一行
 */
TupleTableSlot *exec_spatial_scan_next(SpatialScanState *state);

/**
 * @brief 关闭空间扫描
 */
void exec_spatial_scan_close(SpatialScanState *state);

#ifdef __cplusplus
}
#endif

#endif /* DB_EXECUTOR_SPATIAL_SCAN_H */