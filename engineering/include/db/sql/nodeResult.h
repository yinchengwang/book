/**
 * @file nodeResult.h
 * @brief Result 执行器节点
 *
 * Result 节点用于执行常量查询，如：
 *   - SELECT 1
 *   - SELECT 'hello'
 *   - SELECT 1+2
 *   - SELECT NOW()
 *   - SELECT 1 WHERE true
 *
 * 执行行为：
 *   - 首次调用返回一行（或零行，如果 resconstantqual 为假）
 *   - 后续调用返回 NULL（只返回一行）
 *   - 如果有子计划（lefttree），则返回子计划的输出
 *
 * 本文件是 SQL 执行引擎 Phase 2 核心算子层的第一个任务（Task 2.1）。
 */

#ifndef DB_SQL_NODE_RESULT_H
#define DB_SQL_NODE_RESULT_H

#include "db/sql/nodes/nodetags.h"
#include "db/sql/nodes/execnodes.h"
#include "db/sql/expr.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Result 计划节点
 *
 * 简化版定义，完整版在 planner 任务中补充。
 * ======================================================================== */

/**
 * @brief Result 计划节点
 *
 * 用于常量查询或带常量条件的查询。
 *
 * 设计说明：Result 结构体嵌入 Plan 作为第一个字段，使得 Result* 可以安全转换为 Plan*。
 */
typedef struct Result {
    Plan         plan;               /**< 基类：计划节点（必须作为第一个字段） */
    Expr        *resconstantqual;    /**< 常量条件（如 WHERE true/false） */
} Result;

/* ========================================================================
 * ResultState - Result 执行状态
 * ======================================================================== */

/**
 * @brief Result 执行状态
 *
 * 维护 Result 节点的运行时状态。
 */
typedef struct ResultState {
    PlanState    ps;                 /**< 基类：计划状态 */
    ExprState   *resconstantqual;    /**< 编译后的常量条件表达式 */
    TupleTableSlot *resultslot;      /**< 结果元组槽 */
    bool         rs_done;            /**< 是否已返回一行 */
} ResultState;

/* ========================================================================
 * 公共 API
 * ======================================================================== */

/**
 * @brief 初始化 Result 节点
 *
 * 分配 ResultState 并设置 ExecProcNode 函数指针。
 *
 * @param plan   计划节点（实际类型为 Result*）
 * @param estate 执行器状态
 * @param eflags 执行器标志
 *
 * @return 初始化后的 ResultState（作为 PlanState*）；失败返回 NULL
 */
PlanState *ExecInitResult(Plan *plan, EState *estate, int eflags);

/**
 * @brief Result 节点执行函数
 *
 * 首次调用返回一行，后续调用返回 NULL。
 *
 * @param pstate PlanState（实际类型为 ResultState）
 *
 * @return 结果元组槽；无更多元组时返回 NULL
 */
TupleTableSlot *ExecResult(PlanState *pstate);

/**
 * @brief 结束 Result 节点
 *
 * 释放 ResultState 关联的资源。
 *
 * @param node ResultState（可为 NULL）
 */
void ExecEndResult(ResultState *node);

/**
 * @brief 重置 Result 节点（用于重新扫描）
 *
 * 重置 rs_done 标志，允许再次返回一行。
 *
 * @param node ResultState
 */
void ExecReScanResult(ResultState *node);

#ifdef __cplusplus
}
#endif

#endif /* DB_SQL_NODE_RESULT_H */