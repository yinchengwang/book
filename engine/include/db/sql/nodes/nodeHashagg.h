/**
 * @file nodeHashagg.h
 * @brief HashAgg 执行器节点头文件
 *
 * 实现哈希聚合执行器节点，基于 Volcano 迭代器模型。
 */
#ifndef DB_SQL_NODE_HASHAGG_H
#define DB_SQL_NODE_HASHAGG_H

#include "db/sql/sql_executor.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 前向声明
 * ============================================================ */

struct PlanState_s;
typedef struct PlanState_s PlanState;

/* ============================================================
 * HashAgg 计划节点
 * ============================================================ */

/**
 * @brief HashAgg 计划节点
 *
 * 由计划器生成，包含哈希聚合所需的静态信息
 */
typedef struct HashAggPlan {
    ExecutorType type;            /**< 节点类型 */
    int numGroupCols;            /**< 分组列数 */
    int *grpColIdx;              /**< 分组列索引 */
    List *aggfnames;            /**< 聚合函数名 */
    List *targetlist;            /**< 目标列列表 */
} HashAggPlan;

/* ============================================================
 * HashAggState - 哈希聚合执行状态
 * ============================================================ */

/**
 * @brief HashAggState - 哈希聚合执行状态
 *
 * 使用 Hash 表进行聚合的运行时状态。
 * 基于 Volcano 迭代器模型。
 */
typedef struct HashAggState_s {
    PlanState               *ps;                       /**< 基类 PlanState */
    void                   *agg_hashTable;             /**< 聚合哈希表 */
    void                   *agg_currentGroup;          /**< 当前分组 */
    TupleTableSlot         *result_slot;              /**< 结果槽 */
    int                     num_group_cols;           /**< 分组列数 */
    int                    *grp_col_idx;              /**< 分组列索引 */
    uint64_t                agg_groups;               /**< 分组数 */
    uint64_t                agg_tuples;               /**< 输入元组数 */
    uint64_t                agg_output;               /**< 输出元组数 */
    bool                    agg_finished;             /**< 是否已完成 */
    bool                    agg_first_pass;           /**< 是否首次扫描 */
    bool                    agg_build_phase;           /**< 是否为构建阶段 */
} HashAggState;

/* ============================================================
 * 函数声明
 * ============================================================ */

/**
 * @brief 初始化 HashAgg 执行状态
 *
 * @param node HashAgg 计划节点
 * @param estate 执行器状态（EState）
 * @param eflags 执行标志
 * @return HashAggState 指针，失败返回 NULL
 */
HashAggState *ExecInitHashAgg(HashAggPlan *node, void *estate, int eflags);

/**
 * @brief 执行 HashAgg 迭代
 *
 * Volcano 模型：每次调用返回一个分组结果
 *
 * 两阶段执行：
 * 1. 构建阶段：扫描子节点，构建分组哈希表
 * 2. 输出阶段：遍历哈希表，输出分组结果
 *
 * @param pstate PlanState 指针（实际上是 HashAggState）
 * @return TupleTableSlot 指针，没有更多分组时返回 NULL
 */
TupleTableSlot *ExecHashAgg(PlanState *pstate);

/**
 * @brief 结束 HashAgg 执行
 *
 * 释放哈希表和子节点资源
 *
 * @param node HashAggState 指针
 */
void ExecEndHashAgg(HashAggState *node);

/**
 * @brief 重置 HashAgg 执行状态
 *
 * 用于重新执行聚合
 *
 * @param node HashAggState 指针
 */
void ExecReScanHashAgg(HashAggState *node);

/**
 * @brief 获取 HashAgg 的子节点
 *
 * @param node HashAggState 指针
 * @return PlanState 子节点指针
 */
PlanState *ExecGetHashAggChild(HashAggState *node);

/**
 * @brief 设置 HashAgg 的子节点
 *
 * @param node HashAggState 指针
 * @param child 子节点 PlanState
 */
void ExecSetHashAggChild(HashAggState *node, PlanState *child);

/**
 * @brief 创建 HashAgg 计划节点
 *
 * @param numGroupCols 分组列数
 * @param grpColIdx 分组列索引数组
 * @return HashAggPlan 指针
 */
HashAggPlan *make_hashagg_plan(int numGroupCols, int *grpColIdx);

#ifdef __cplusplus
}
#endif

#endif /* DB_SQL_NODE_HASHAGG_H */
