/**
 * @file nodeLimit.h
 * @brief Limit 限制执行器节点
 *
 * Limit 节点用于限制查询结果，如：
 *   - SELECT * FROM table LIMIT 10
 *   - SELECT * FROM table LIMIT 10 OFFSET 5
 *   - SELECT * FROM table OFFSET 5
 *
 * 执行行为：
 *   - 跳过 OFFSET 个元组
 *   - 返回 LIMIT 个元组
 *   - 提前终止子节点扫描
 *
 * 本文件是 SQL 执行引擎 Phase 2 核心算子层的 Task 2.6。
 */

#ifndef DB_SQL_NODE_LIMIT_H
#define DB_SQL_NODE_LIMIT_H

#include "db/sql/nodes/nodetags.h"
#include "db/sql/nodes/execnodes.h"
#include "db/sql/expr.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Limit 计划节点
 * ======================================================================== */

/**
 * @brief Limit 计划节点
 *
 * 用于限制查询结果。
 *
 * 设计说明：Limit 结构体嵌入 Plan 作为第一个字段，使得 Limit* 可以安全转换为 Plan*。
 */
typedef struct Limit {
    Plan         plan;               /**< 基类：计划节点（必须作为第一个字段） */
    int          limitOffset;        /**< OFFSET 值（-1 表示未指定） */
    int          limitCount;         /**< LIMIT 值（-1 表示未指定） */
} Limit;

/* ========================================================================
 * LimitState - Limit 执行状态
 * ======================================================================== */

/**
 * @brief Limit 执行状态
 *
 * 维护 Limit 节点的运行时状态。
 */
typedef struct LimitState {
    PlanState    ps;                 /**< 基类：计划状态 */
    int          limitOffset;        /**< OFFSET 值 */
    int          limitCount;         /**< LIMIT 值 */
    int          position;           /**< 当前位置计数器 */
    bool         noCount;            /**< 是否无 LIMIT（limitCount = -1） */
} LimitState;

/* ========================================================================
 * 公共 API
 * ======================================================================== */

/**
 * @brief 初始化 Limit 节点
 *
 * 分配 LimitState 并设置 ExecProcNode 函数指针。
 * 同时初始化子节点。
 *
 * @param plan   计划节点（实际类型为 Limit*）
 * @param estate 执行器状态
 * @param eflags 执行器标志
 *
 * @return 初始化后的 LimitState（作为 PlanState*）；失败返回 NULL
 */
PlanState *ExecInitLimit(Plan *plan, EState *estate, int eflags);

/**
 * @brief Limit 节点执行函数
 *
 * 执行限制计算。
 * 当前为框架实现，返回 NULL。
 *
 * @param pstate PlanState（实际类型为 LimitState）
 *
 * @return 结果元组槽；无更多元组时返回 NULL
 */
TupleTableSlot *ExecLimit(PlanState *pstate);

/**
 * @brief 结束 Limit 节点
 *
 * 释放 LimitState 关联的资源。
 *
 * @param node LimitState（可为 NULL）
 */
void ExecEndLimit(LimitState *node);

/**
 * @brief 重置 Limit 节点（用于重新扫描）
 *
 * 重置位置计数器。
 *
 * @param node LimitState
 */
void ExecReScanLimit(LimitState *node);

#ifdef __cplusplus
}
#endif

#endif /* DB_SQL_NODE_LIMIT_H */