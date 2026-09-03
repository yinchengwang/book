/**
 * @file nodeProjectSet.h
 * @brief ProjectSet SRF 展开执行器节点
 *
 * ProjectSet 节点用于展开集合返回函数（SRF）：
 *   - SELECT generate_series(1, 5) → 返回 5 行
 *   - SELECT unnest(array[1,2,3]) → 返回 3 行
 *
 * 执行行为：
 *   - 从子节点拉取元组
 *   - 对每个元组展开 SRF 函数
 *   - 生成多行输出
 *
 * 本文件是 SQL 执行引擎 Phase 2 核心算子层的 Task 2.8。
 */

#ifndef DB_SQL_NODE_PROJECTSET_H
#define DB_SQL_NODE_PROJECTSET_H

#include "db/sql/nodes/nodetags.h"
#include "db/sql/nodes/execnodes.h"
#include "db/sql/expr.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * ProjectSet 计划节点
 * ======================================================================== */

/**
 * @brief ProjectSet 计划节点
 *
 * 用于展开集合返回函数（SRF）。
 *
 * 设计说明：ProjectSet 结构体嵌入 Plan 作为第一个字段，
 * 使得 ProjectSet* 可以安全转换为 Plan*。
 */
typedef struct ProjectSet {
    Plan         plan;               /**< 基类：计划节点（必须作为第一个字段） */
    List        *setexprs;           /**< SRF 表达式列表 */
} ProjectSet;

/* ========================================================================
 * ProjectSetState - ProjectSet 执行状态
 * ======================================================================== */

/**
 * @brief ProjectSet 执行状态
 *
 * 维护 ProjectSet 节点的运行时状态。
 */
typedef struct ProjectSetState {
    PlanState    ps;                 /**< 基类：计划状态 */
    ExprState  **setexprs;           /**< 编译后的 SRF 数组 */
    int          numexprs;           /**< SRF 数量 */
    TupleTableSlot *result_slot;     /**< 结果槽 */
    bool         done;               /**< 是否已完成 */
} ProjectSetState;

/* ========================================================================
 * 公共 API
 * ======================================================================== */

/**
 * @brief 初始化 ProjectSet 节点
 *
 * 分配 ProjectSetState 并设置 ExecProcNode 函数指针。
 * 同时初始化子节点和 SRF 表达式。
 *
 * @param plan   计划节点（实际类型为 ProjectSet*）
 * @param estate 执行器状态
 * @param eflags 执行器标志
 *
 * @return 初始化后的 ProjectSetState（作为 PlanState*）；失败返回 NULL
 */
PlanState *ExecInitProjectSet(Plan *plan, EState *estate, int eflags);

/**
 * @brief ProjectSet 节点执行函数
 *
 * 执行 SRF 展开。
 * 当前为框架实现，返回 NULL。
 *
 * @param pstate PlanState（实际类型为 ProjectSetState）
 *
 * @return 结果元组槽；无更多元组时返回 NULL
 */
TupleTableSlot *ExecProjectSet(PlanState *pstate);

/**
 * @brief 结束 ProjectSet 节点
 *
 * 释放 ProjectSetState 关联的资源。
 *
 * @param node ProjectSetState（可为 NULL）
 */
void ExecEndProjectSet(ProjectSetState *node);

/**
 * @brief 重置 ProjectSet 节点（用于重新扫描）
 *
 * 重置完成标志。
 *
 * @param node ProjectSetState
 */
void ExecReScanProjectSet(ProjectSetState *node);

#ifdef __cplusplus
}
#endif

#endif /* DB_SQL_NODE_PROJECTSET_H */