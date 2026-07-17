/**
 * @file nodeNestloop.h
 * @brief NestLoop 执行器节点头文件
 *
 * 实现嵌套循环连接执行器节点，基于 Volcano 迭代器模型。
 */
#ifndef DB_SQL_NODE_NESTLOOP_H
#define DB_SQL_NODE_NESTLOOP_H

#include "db/sql/sql_executor.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * NestLoop 计划节点
 * ============================================================ */

/**
 * @brief NestLoop 计划节点
 *
 * 由计划器生成，包含嵌套循环连接所需的静态信息
 */
typedef struct NestLoopPlan {
    ExecutorType type;            /**< 节点类型（EXEC_NESTLOOP） */
    List *joinqual;               /**< 连接条件列表 */
    List *nlc_CurrentSearch;      /**< 搜索条件 */
    List *targetlist;             /**< 目标列列表 */
} NestLoopPlan;

/* ============================================================
 * NestLoop 扩展状态
 * ============================================================ */

/**
 * @brief NestLoop 扩展执行状态
 *
 * 存储嵌套循环连接的运行时状态
 */
typedef struct NestLoopExtState {
    /* 子节点 */
    PlanState *nl_OuterTask;      /**< 外表任务 */
    PlanState *nl_InnerTask;      /**< 内表任务 */

    /* 当前元组 */
    TupleTableSlot *nl_curouter;  /**< 当前外表元组 */

    /* 目标列表 */
    List *nl_JoinQual;            /**< 连接条件 */

    /* 统计信息 */
    uint64_t nl_outerTuples;      /**< 外表元组数 */
    uint64_t nl_innerTuples;      /**< 内表扫描次数 */
    uint64_t nl_hits;             /**< 匹配命中数 */

    /* 状态标志 */
    bool nl_returned_outer;       /**< 是否已返回外表元组 */
    bool nl_exhausted;            /**< 是否已耗尽 */
} NestLoopExtState;

/* ============================================================
 * 函数声明
 * ============================================================ */

/**
 * @brief 初始化 NestLoop 执行状态
 *
 * @param node NestLoop 计划节点
 * @param estate 执行器状态（EState）
 * @param eflags 执行标志
 * @return NestLoopState 指针，失败返回 NULL
 */
NestLoopState *ExecInitNestLoop(NestLoopPlan *node, void *estate, int eflags);

/**
 * @brief 执行 NestLoop 迭代
 *
 * Volcano 模型：每次调用返回一个匹配的元组
 *
 * @param pstate PlanState 指针（实际上是 NestLoopState）
 * @return TupleTableSlot 指针，没有更多匹配元组时返回 NULL
 */
TupleTableSlot *ExecNestLoop(PlanState *pstate);

/**
 * @brief 结束 NestLoop 执行
 *
 * 释放子节点和资源
 *
 * @param node NestLoopState 指针
 */
void ExecEndNestLoop(NestLoopState *node);

/**
 * @brief 重置 NestLoop 执行状态
 *
 * 用于重新执行连接
 *
 * @param node NestLoopState 指针
 */
void ExecReScanNestLoop(NestLoopState *node);

/**
 * @brief 获取 NestLoop 的扩展状态
 *
 * @param node NestLoopState 指针
 * @return NestLoopExtState 指针
 */
NestLoopExtState *ExecGetNestLoopExtState(NestLoopState *node);

/**
 * @brief 创建 NestLoop 计划节点
 *
 * @param joinqual 连接条件列表
 * @return NestLoopPlan 指针
 */
NestLoopPlan *make_nestloop_plan(List *joinqual);

#ifdef __cplusplus
}
#endif

#endif /* DB_SQL_NODE_NESTLOOP_H */
