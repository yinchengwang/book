/**
 * @file nodeHashjoin.h
 * @brief HashJoin 执行器节点
 *
 * HashJoin 节点用于等值连接查询：
 *   - SELECT * FROM t1 JOIN t2 ON t1.id = t2.id
 *   - 时间复杂度：O(n+m)，最高效的连接算法之一
 *
 * 执行行为：
 *   - 构建阶段：扫描内表，构建哈希表
 *   - 探测阶段：扫描外表，查找哈希表，输出匹配行
 *
 * 本文件是 SQL 执行引擎 Phase 2 核心算子层的第三个任务（Task 2.3）。
 */

#ifndef DB_SQL_NODE_HASHJOIN_H
#define DB_SQL_NODE_HASHJOIN_H

#include "db/sql/nodes/nodetags.h"
#include "db/sql/nodes/execnodes.h"
#include "db/sql/expr.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Join 类型
 * ======================================================================== */

/**
 * @brief Join 类型枚举
 *
 * 注意：parsenodes.h 中已定义 JOIN_INNER, JOIN_LEFT, JOIN_RIGHT, JOIN_FULL, JOIN_CROSS
 * 这里补充 JOIN_SEMI 和 JOIN_ANTI（执行器专用）
 */
#ifndef JOIN_SEMI
#define JOIN_SEMI   4
#endif

#ifndef JOIN_ANTI
#define JOIN_ANTI   5
#endif

/* ========================================================================
 * Join 计划基类
 * ======================================================================== */

/**
 * @brief Join 计划基类
 *
 * 所有连接节点的基类（HashJoin、MergeJoin、NestLoop 等）。
 */
typedef struct Join {
    Plan         plan;               /**< 基类：计划节点 */
    JoinType     jointype;           /**< 连接类型 */
    List        *joinqual;           /**< 连接条件（非哈希条件） */
} Join;

/* ========================================================================
 * HashJoin 计划节点
 * ======================================================================== */

/**
 * @brief HashJoin 计划节点
 *
 * 用于等值连接查询。
 *
 * 设计说明：HashJoin 结构体嵌入 Join 作为第一个字段，使得 HashJoin* 可以安全转换为 Join* 或 Plan*。
 */
typedef struct HashJoin {
    Join         join;               /**< 基类：连接节点（必须作为第一个字段） */
    List        *hashclauses;        /**< 哈希连接条件（等值条件） */
    List        *hashoperators;      /**< 哈希操作符列表 */
    bool         hashnullrecheck;    /**< NULL 重检查（用于 nullable 连接键） */
} HashJoin;

/* ========================================================================
 * JoinState 基类
 * ======================================================================== */

/**
 * @brief JoinState 执行状态基类
 *
 * 所有连接节点的运行时状态基类。
 */
typedef struct JoinState {
    PlanState    ps;                 /**< 基类：计划状态 */
    ExprState   *joinqual;           /**< 编译后的连接条件 */
} JoinState;

/* ========================================================================
 * HashJoinState - HashJoin 执行状态
 * ======================================================================== */

/**
 * @brief HashJoin 执行状态
 *
 * 维护 HashJoin 节点的运行时状态。
 */
typedef struct HashJoinState {
    JoinState    js;                 /**< 基类：连接状态 */
    ExprState   *hashclauses;        /**< 编译后的哈希连接条件 */
    void        *hashtable;          /**< 哈希表（内部使用 HashJoinHashTable） */
    TupleTableSlot *hj_OuterTupleSlot;  /**< 外表元组槽 */
    TupleTableSlot *hj_InnerTupleSlot;  /**< 内表元组槽 */
    TupleTableSlot *hj_NullInnerTupleSlot; /**< NULL 内表槽（LEFT JOIN 用） */
    bool         hj_FirstOuterTupleSlot;  /**< 首次探测标志 */
    int          hj_CurOuterNoMatch;  /**< 当前未匹配行计数 */
} HashJoinState;

/* ========================================================================
 * 公共 API
 * ======================================================================== */

/**
 * @brief 初始化 HashJoin 节点
 *
 * 分配 HashJoinState 并设置 ExecProcNode 函数指针。
 * 同时初始化子节点（lefttree/righttree）和 Hash 辅助节点。
 *
 * @param plan   计划节点（实际类型为 HashJoin*）
 * @param estate 执行器状态
 * @param eflags 执行器标志
 *
 * @return 初始化后的 HashJoinState（作为 PlanState*）；失败返回 NULL
 */
PlanState *ExecInitHashJoin(Plan *plan, EState *estate, int eflags);

/**
 * @brief HashJoin 节点执行函数
 *
 * 构建哈希表并探测匹配行。
 * 当前为框架实现，返回 NULL。
 *
 * @param pstate PlanState（实际类型为 HashJoinState）
 *
 * @return 结果元组槽；无更多元组时返回 NULL
 */
TupleTableSlot *ExecHashJoin(PlanState *pstate);

/**
 * @brief 结束 HashJoin 节点
 *
 * 释放 HashJoinState 关联的资源，包括子节点和 Hash 表。
 *
 * @param node HashJoinState（可为 NULL）
 */
void ExecEndHashJoin(HashJoinState *node);

/**
 * @brief 重置 HashJoin 节点（用于重新扫描）
 *
 * 重置哈希表状态，允许重新构建和探测。
 *
 * @param node HashJoinState
 */
void ExecReScanHashJoin(HashJoinState *node);

#ifdef __cplusplus
}
#endif

#endif /* DB_SQL_NODE_HASHJOIN_H */