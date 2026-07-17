/**
 * @file graph_scan.h
 * @brief 图扫描算子接口
 *
 * 图扫描算子直接调用 graph_engine，实现将在 Phase 7 完成。
 */
#ifndef DB_EXECUTOR_GRAPH_SCAN_H
#define DB_EXECUTOR_GRAPH_SCAN_H

#include "db/sql/sql_executor.h"  /* PlanState, TupleTableSlot */

#ifdef __cplusplus
extern "C" {
#endif

/** 图扫描状态 */
typedef struct GraphScanState_s {
    PlanState ps;             /**< 基类 PlanState */
    void     *start_vertex;   /**< 起始顶点 */
    int       max_depth;      /**< 最大遍历深度 */
    void     *traversal_pattern; /**< 遍历模式 */
} GraphScanState;

/**
 * @brief 初始化图扫描
 */
GraphScanState *exec_graph_scan_init(PlanState *parent,
    void *start_vertex, int max_depth, void *traversal_pattern);

/**
 * @brief 执行图扫描，返回下一行
 */
TupleTableSlot *exec_graph_scan_next(GraphScanState *state);

/**
 * @brief 关闭图扫描
 */
void exec_graph_scan_close(GraphScanState *state);

#ifdef __cplusplus
}
#endif

#endif /* DB_EXECUTOR_GRAPH_SCAN_H */