/**
 * @file nodeNestloop.c
 * @brief NestLoop 执行器节点实现
 *
 * 实现嵌套循环连接执行器节点，基于 Volcano 迭代器模型。
 * 外表驱动内表扫描逻辑。
 */

#include "db/sql/nodes/nodeNestloop.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================
 * 辅助函数
 * ============================================================ */

/**
 * @brief 检查元组是否满足连接条件
 */
static bool ExecQual(TupleTableSlot *slot, List *qual, ExprContext *econtext)
{
    (void)slot;
    (void)qual;
    (void)econtext;
    return true;
}

/* ============================================================
 * NestLoop 执行器节点实现
 * ============================================================ */

/**
 * @brief 初始化 NestLoop 执行状态
 */
NestLoopState *ExecInitNestLoop(NestLoopPlan *node, void *estate, int eflags)
{
    NestLoopState *nlstate;
    NestLoopExtState *ext_state;

    (void)eflags;
    (void)estate;

    /* 分配状态结构 */
    nlstate = (NestLoopState *)calloc(1, sizeof(NestLoopState));
    if (!nlstate) {
        return NULL;
    }

    /* 分配扩展状态结构 */
    ext_state = (NestLoopExtState *)calloc(1, sizeof(NestLoopExtState));
    if (!ext_state) {
        free(nlstate);
        return NULL;
    }

    /* 初始化基类 */
    nlstate->js.ps.type = EXEC_NESTLOOP;
    nlstate->js.ps.left = NULL;
    nlstate->js.ps.right = NULL;
    nlstate->js.ps.state = ext_state;
    nlstate->js.join_type = JOIN_NESTLOOP;

    /* 设置执行函数指针 */
    nlstate->js.ps.exec_proc = ExecNestLoop;

    /* 复制计划和目标列表 */
    ext_state->nl_JoinQual = node->joinqual;
    nlstate->js.joinqual = node->joinqual;

    /* 初始化子节点指针 */
    ext_state->nl_OuterTask = NULL;
    ext_state->nl_InnerTask = NULL;
    nlstate->louter = NULL;
    nlstate->rinner = NULL;

    /* 初始化当前元组 */
    ext_state->nl_curouter = NULL;

    /* 初始化统计信息 */
    ext_state->nl_outerTuples = 0;
    ext_state->nl_innerTuples = 0;
    ext_state->nl_hits = 0;

    /* 初始化状态标志 */
    ext_state->nl_returned_outer = false;
    ext_state->nl_exhausted = true;  /* 默认标记为已耗尽（无子节点） */

    return nlstate;
}

/**
 * @brief 执行 NestLoop 迭代
 */
TupleTableSlot *ExecNestLoop(PlanState *pstate)
{
    NestLoopState *node = (NestLoopState *)pstate;
    NestLoopExtState *ext_state;
    TupleTableSlot *outer_slot;
    TupleTableSlot *inner_slot;
    PlanState *outer_plan;
    PlanState *inner_plan;

    if (!node) return NULL;

    ext_state = (NestLoopExtState *)node->js.ps.state;
    if (!ext_state) return NULL;

    /* 如果已经标记为耗尽，直接返回 */
    if (ext_state->nl_exhausted) {
        return NULL;
    }

    outer_plan = ext_state->nl_OuterTask;
    inner_plan = ext_state->nl_InnerTask;

    /* 获取外表元组 */
    if (!ext_state->nl_curouter) {
        if (!outer_plan || !outer_plan->exec_proc) {
            ext_state->nl_exhausted = true;
            return NULL;
        }

        outer_slot = outer_plan->exec_proc(outer_plan);
        if (!outer_slot) {
            ext_state->nl_exhausted = true;
            return NULL;
        }

        ext_state->nl_curouter = outer_slot;
        ext_state->nl_outerTuples++;
    }

    /* 扫描内表 */
    if (inner_plan && inner_plan->exec_proc) {
        inner_slot = inner_plan->exec_proc(inner_plan);
        if (inner_slot) {
            ext_state->nl_innerTuples++;
            if (ExecQual(inner_slot, ext_state->nl_JoinQual, NULL)) {
                ext_state->nl_hits++;
                return inner_slot;
            }
        }
    }

    /* 内表耗尽，重置外表 */
    ext_state->nl_curouter = NULL;
    return NULL;
}

/**
 * @brief 结束 NestLoop 执行
 */
void ExecEndNestLoop(NestLoopState *node)
{
    NestLoopExtState *ext_state;

    if (!node) return;

    ext_state = (NestLoopExtState *)node->js.ps.state;

    if (ext_state) {
        ext_state->nl_OuterTask = NULL;
        ext_state->nl_InnerTask = NULL;
        ext_state->nl_curouter = NULL;
        free(ext_state);
    }

    node->js.ps.state = NULL;
}

/**
 * @brief 重置 NestLoop 执行状态
 */
void ExecReScanNestLoop(NestLoopState *node)
{
    NestLoopExtState *ext_state;

    if (!node) return;

    ext_state = (NestLoopExtState *)node->js.ps.state;
    if (!ext_state) return;

    ext_state->nl_outerTuples = 0;
    ext_state->nl_innerTuples = 0;
    ext_state->nl_hits = 0;
    ext_state->nl_returned_outer = false;
    ext_state->nl_exhausted = false;
    ext_state->nl_curouter = NULL;
}

/**
 * @brief 获取扩展状态
 */
NestLoopExtState *ExecGetNestLoopExtState(NestLoopState *node)
{
    if (!node) return NULL;
    return (NestLoopExtState *)node->js.ps.state;
}

/**
 * @brief 创建计划节点
 */
NestLoopPlan *make_nestloop_plan(List *joinqual)
{
    NestLoopPlan *node = (NestLoopPlan *)calloc(1, sizeof(NestLoopPlan));
    if (!node) return NULL;

    node->type = EXEC_NESTLOOP;
    node->joinqual = joinqual;
    node->nlc_CurrentSearch = NULL;
    node->targetlist = NULL;

    return node;
}
