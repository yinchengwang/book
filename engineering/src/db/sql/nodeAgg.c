/**
 * @file nodeAgg.c
 * @brief Agg 聚合执行器节点实现
 *
 * 实现 Task 2.4 的 Agg 聚合节点：
 *   - ExecInitAgg: 初始化 AggState
 *   - ExecAgg: 执行聚合计算
 *   - ExecEndAgg: 释放资源
 *   - ExecReScanAgg: 重置节点
 *
 * 当前为框架版本：
 *   - 聚合计算逻辑简化
 *   - 重点验证节点初始化和生命周期管理
 *   - 实际聚合逻辑后续完善
 */

#include "db/sql/nodeAgg.h"
#include "db/sql/executor.h"
#include "db/sql/sql_executor.h"
#include "db/sql/memctx.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* slot_set_value 在 sql_executor.h 中声明，但 executor.h 共用
 * DB_SQL_EXECUTOR_H 宏导致 sql_executor.h 被跳过。这里手动声明 */
extern void slot_set_value(TupleTableSlot *slot, int attnum, const void *value, int len);

/* ========================================================================
 * Agg 节点执行函数
 * ======================================================================== */

/**
 * @brief Agg 节点执行函数（框架实现）
 *
 * Agg 算法：
 *   1. AGG_PLAIN：扫描所有行，计算聚合值，返回一行
 *   2. AGG_SORTED：按分组列扫描，每组计算聚合值
 *   3. AGG_HASHED：构建哈希表，按键分组聚合
 *
 * 框架版本返回 NULL。
 *
 * @param pstate PlanState（实际类型为 AggState）
 *
 * @return 结果元组槽；无更多元组时返回 NULL
 */
static TupleTableSlot *exec_agg_impl(PlanState *pstate) {
    AggState *node = (AggState *)pstate;
    PlanState *child;
    TupleTableSlot *input_slot;
    int64_t count;

    /* 参数检查 */
    if (node == NULL) {
        return NULL;
    }

    /* 如果聚合已完成，直接返回 NULL */
    if (node->agg_done) {
        return NULL;
    }

    /* Task 2.4: 实现 AGG_PLAIN 聚合策略（支持 COUNT(*)）
     * 1. 扫描子节点所有元组
     * 2. 累积 COUNT(*) 状态
     * 3. 设置结果到 agg_slot
     * 4. 标记聚合已完成 */

    count = 0;
    child = node->ps.lefttree;
    if (child != NULL && child->ExecProcNode != NULL) {
        while ((input_slot = child->ExecProcNode(child)) != NULL) {
            count++;
        }
    }

    /* 设置 COUNT(*) 结果到 agg_slot。
     * Task 2.4 简化：仅在槽已正确初始化时设置值，否则跳过。
     * 完整实现需要 ExecInitAgg 创建 agg_slot 时分配 tts_tupleDescriptor
     * 和 tts_values/tts_isnull 数组。 */
    if (node->agg_slot != NULL && node->agg_slot->tts_tupleDescriptor != NULL
        && node->agg_slot->tts_values != NULL
        && node->agg_slot->tts_tupleDescriptor->natts >= 1) {
        int count_val = (int)count;
        slot_set_value(node->agg_slot, 1, &count_val, sizeof(int));
    }

    /* 标记聚合已完成（下次调用返回 NULL） */
    node->agg_done = true;

    /* 返回聚合结果槽 */
    return node->agg_slot;
}

/* ========================================================================
 * 公共 API
 * ======================================================================== */

/**
 * @brief 初始化 Agg 节点
 *
 * 分配 AggState 并初始化字段。
 * 同时初始化子节点。
 *
 * @param plan   计划节点（实际类型为 Agg*）
 * @param estate 执行器状态
 * @param eflags 执行器标志
 *
 * @return 初始化后的 AggState（作为 PlanState*）；失败返回 NULL
 */
PlanState *ExecInitAgg(Plan *plan, EState *estate, int eflags) {
    Agg *node;
    AggState *state;

    /* 参数检查 */
    if (plan == NULL || estate == NULL) {
        return NULL;
    }

    node = (Agg *)plan;

    /* 在查询上下文中分配 AggState */
    state = (AggState *)palloc0(estate->es_query_cxt, sizeof(AggState));
    if (state == NULL) {
        return NULL;
    }

    /* 初始化基类 */
    state->ps.type = T_AggState;
    state->ps.plan = plan;
    state->ps.state = estate;
    state->ps.ExecProcNode = exec_agg_impl;
    state->ps.ExecProcNodeReal = exec_agg_impl;

    /* 初始化子节点 */
    if (node->plan.lefttree != NULL) {
        state->ps.lefttree = ExecInitNode(node->plan.lefttree, estate, eflags);
        /* 框架版本：允许子节点初始化失败（存储层桩实现） */
    } else {
        state->ps.lefttree = NULL;
    }

    /* Agg 节点无右子树 */
    state->ps.righttree = NULL;

    /* 初始化表达式上下文 */
    state->ps.ps_ExprContext = CreateExprContext(estate);
    if (state->ps.ps_ExprContext == NULL) {
        return NULL;
    }

    /* 创建结果槽 */
    state->ps.ps_ResultTupleSlot = MakeTupleTableSlotWithMCxt(estate->es_query_cxt);
    if (state->ps.ps_ResultTupleSlot == NULL) {
        return NULL;
    }

    /* 创建聚合槽 */
    state->agg_slot = MakeTupleTableSlotWithMCxt(estate->es_query_cxt);
    if (state->agg_slot == NULL) {
        return NULL;
    }

    /* 初始化 Agg 特定字段 */
    state->aggstrategy = node->aggstrategy;
    state->numCols = node->numCols;
    state->grpColIdx = NULL;  /* 框架版本：不复制 */
    state->agg_done = false;
    state->pergroup = NULL;   /* 框架版本：不分配 */

    /* 初始化其他字段 */
    state->ps.qual = NULL;
    state->ps.recheck = NULL;
    state->ps.ps_ProjInfo = NULL;
    state->ps.ps_ResultTupleDesc = NULL;
    state->ps.instrument = NULL;
    state->ps.needs_to_scan_queue = false;
    state->ps.chgParam = NULL;

    return (PlanState *)state;
}

/**
 * @brief Agg 节点执行函数（公共接口）
 *
 * @param pstate PlanState（实际类型为 AggState）
 *
 * @return 结果元组槽；无更多元组时返回 NULL
 */
TupleTableSlot *ExecAgg(PlanState *pstate) {
    return exec_agg_impl(pstate);
}

/**
 * @brief 结束 Agg 节点
 *
 * 释放 AggState 关联的资源。
 *
 * @param node AggState（可为 NULL）
 */
void ExecEndAgg(AggState *node) {
    if (node == NULL) {
        return;
    }

    /* 释放子节点 */
    if (node->ps.lefttree != NULL) {
        ExecEndNode(node->ps.lefttree);
        node->ps.lefttree = NULL;
    }

    /* 释放聚合槽 */
    if (node->agg_slot != NULL) {
        FreeTupleTableSlot(node->agg_slot);
        node->agg_slot = NULL;
    }

    /* 释放表达式上下文 */
    if (node->ps.ps_ExprContext != NULL) {
        FreeExprContext(node->ps.ps_ExprContext, true);
        node->ps.ps_ExprContext = NULL;
    }

    /* 释放结果槽 */
    if (node->ps.ps_ResultTupleSlot != NULL) {
        FreeTupleTableSlot(node->ps.ps_ResultTupleSlot);
        node->ps.ps_ResultTupleSlot = NULL;
    }

    /* 释放分组列索引（如果复制了） */
    if (node->grpColIdx != NULL) {
        free(node->grpColIdx);
        node->grpColIdx = NULL;
    }

    /* 释放每组聚合状态 */
    if (node->pergroup != NULL) {
        free(node->pergroup);
        node->pergroup = NULL;
    }

    /* 注意：AggState 本身由 EState 的查询上下文管理，不单独释放 */
}

/**
 * @brief 重置 Agg 节点（用于重新扫描）
 *
 * 重置聚合状态，允许重新计算。
 *
 * @param node AggState
 */
void ExecReScanAgg(AggState *node) {
    if (node == NULL) {
        return;
    }

    /* 重置子节点 */
    if (node->ps.lefttree != NULL) {
        ExecReScan(node->ps.lefttree);
    }

    /* 重置聚合状态 */
    node->agg_done = false;

    /* TODO: 重置聚合状态 */
}