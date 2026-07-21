/**
 * @file nodeMergejoin.h
 * @brief W4.5: MergeJoin 执行器节点
 *
 * MergeJoin 节点用于有序归并连接：
 *   - SELECT * FROM t1 JOIN t2 ON t1.id = t2.id
 *   - 要求内外表已按连接键排序
 *   - 时间复杂度：O(n+m)，适合大数据量排序后的连接
 *
 * 执行行为：
 *   1. 同时扫描内外表（已排序）
 *   2. 比较当前元组的连接键
 *   3. 相等则输出，否则推进较小的那一边
 */

#ifndef DB_SQL_NODE_MERGEJOIN_H
#define DB_SQL_NODE_MERGEJOIN_H

#include "db/sql/nodes/nodetags.h"
#include "db/sql/nodes/execnodes.h"
#include "db/sql/expr.h"
#include "db/sql/nodeHashjoin.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * MergeJoin 计划节点
 * ======================================================================== */

/**
 * @brief MergeJoin 计划节点
 *
 * 用于排序归并连接。
 */
typedef struct MergeJoin {
    Join         join;               /**< 基类：连接节点 */
    List        *mergeclauses;       /**< 归并连接条件 */
    List        *mergesortop;        /**< 排序操作符 */
    List        *mergenullsfirst;    /**< NULL 排序策略 */
    List        *mergecollations;    /**< 排序规则 */
} MergeJoin;

/* MergeJoin 的 Plan 类型标记 */
#define T_MergeJoin T_MergeJoin

/* ========================================================================
 * MergeJoinState 执行状态
 * ======================================================================== */

/**
 * @brief MergeJoin 执行状态
 */
typedef struct MergeJoinState {
    JoinState    js;                 /**< 基类：连接状态 */
    ExprState   *mergeclauses;       /**< 编译后的归并条件 */
    int          merge_initialized;  /**< 是否已初始化扫描 */
    TupleTableSlot *mj_OuterTupleSlot; /**< 外表当前元组 */
    TupleTableSlot *mj_InnerTupleSlot; /**< 内表当前元组 */
    bool         mj_OuterDone;       /**< 外表是否已耗尽 */
    bool         mj_InnerDone;       /**< 内表是否已耗尽 */
    int          mj_JoinState;       /**< 连接状态：0=初始, 1=匹配中, 2=完成 */
} MergeJoinState;

/* ========================================================================
 * 公共 API
 * ======================================================================== */

/**
 * @brief 初始化 MergeJoin 节点
 *
 * @param plan   计划节点（实际类型为 MergeJoin*）
 * @param estate 执行器状态
 * @param eflags 执行器标志
 * @return MergeJoinState（作为 PlanState*）；失败返回 NULL
 */
PlanState *ExecInitMergeJoin(Plan *plan, EState *estate, int eflags);

/**
 * @brief MergeJoin 节点执行函数
 *
 * 同时扫描排序后的内外表，归并输出匹配行。
 *
 * @param pstate PlanState（实际类型为 MergeJoinState）
 * @return 结果元组槽；无更多元组时返回 NULL
 */
TupleTableSlot *ExecMergeJoin(PlanState *pstate);

/**
 * @brief 结束 MergeJoin 节点
 *
 * @param node MergeJoinState（可为 NULL）
 */
void ExecEndMergeJoin(MergeJoinState *node);

/**
 * @brief 重置 MergeJoin 节点（用于重新扫描）
 *
 * @param node MergeJoinState
 */
void ExecReScanMergeJoin(MergeJoinState *node);

#ifdef __cplusplus
}
#endif

#endif /* DB_SQL_NODE_MERGEJOIN_H */