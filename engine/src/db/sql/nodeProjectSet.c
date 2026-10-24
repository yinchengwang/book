/**
 * @file nodeProjectSet.c
 * @brief ProjectSet SRF 展开执行器节点实现（主路径不使用）
 *
 * 实现 ProjectSet SRF 展开节点：
 *   - ExecInitProjectSet: 初始化 ProjectSetState
 *   - ExecProjectSet: 执行 SRF 展开
 *   - ExecEndProjectSet: 释放资源
 *   - ExecReScanProjectSet: 重置节点
 *
 * SRF 展开属后续子项目（S1）范畴，主路径不调用本节点。
 * 规范 §5.3 要求禁静默桩，exec_projectset_impl 改为显式硬报错
 * （stderr + abort）。
 */

#include "db/sql/nodeProjectSet.h"
#include "db/sql/executor.h"
#include "db/sql/memctx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ========================================================================
 * ProjectSet 节点执行函数
 * ======================================================================== */

/**
 * @brief ProjectSet 节点执行函数（主路径不使用）
 *
 * 显式硬报错：spec §5.3 禁止静默桩；任何走到本节点 ExecProcNode 的调用
 * 都属主路径未涵盖，进程级失败优于返回错误结果集。
 *
 * @param pstate PlanState（实际类型为 ProjectSetState）
 */
static TupleTableSlot *exec_projectset_impl(PlanState *pstate) {
    ProjectSetState *node = (ProjectSetState *)pstate;
    (void)node;
    fprintf(stderr,
        "FATAL: ProjectSet 节点 ExecProcNode 被调用 — SRF 展开未实现，"
        "主路径不调用此节点，留待 S1 实装。\n");
    abort();
    /* unreachable */
    return NULL;
}

/* ========================================================================
 * 公共 API
 * ======================================================================== */

/**
 * @brief 初始化 ProjectSet 节点
 *
 * 分配 ProjectSetState 并初始化字段。
 * 同时初始化子节点和 SRF 表达式。
 *
 * @param plan   计划节点（实际类型为 ProjectSet*）
 * @param estate 执行器状态
 * @param eflags 执行器标志
 *
 * @return 初始化后的 ProjectSetState（作为 PlanState*）；失败返回 NULL
 */
PlanState *ExecInitProjectSet(Plan *plan, EState *estate, int eflags) {
    ProjectSet *node;
    ProjectSetState *state;

    /* 参数检查 */
    if (plan == NULL || estate == NULL) {
        return NULL;
    }

    node = (ProjectSet *)plan;

    /* 在查询上下文中分配 ProjectSetState */
    state = (ProjectSetState *)palloc0(estate->es_query_cxt, sizeof(ProjectSetState));
    if (state == NULL) {
        return NULL;
    }

    /* 初始化基类 */
    state->ps.type = T_ProjectSetState;
    state->ps.plan = plan;
    state->ps.state = estate;
    state->ps.ExecProcNode = exec_projectset_impl;
    state->ps.ExecProcNodeReal = exec_projectset_impl;

    /* 初始化子节点 */
    if (node->plan.lefttree != NULL) {
        state->ps.lefttree = ExecInitNode(node->plan.lefttree, estate, eflags);
        /* 框架版本：允许子节点初始化失败（存储层桩实现） */
    } else {
        state->ps.lefttree = NULL;
    }

    /* ProjectSet 节点无右子树 */
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

    /* 初始化 ProjectSet 特定字段 */
    state->setexprs = NULL;  /* 框架版本：暂不初始化 */
    state->numexprs = 0;
    state->result_slot = state->ps.ps_ResultTupleSlot;
    state->done = false;

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
 * @brief ProjectSet 节点执行函数（公共接口）
 *
 * @param pstate PlanState（实际类型为 ProjectSetState）
 *
 * @return 结果元组槽；无更多元组时返回 NULL
 */
TupleTableSlot *ExecProjectSet(PlanState *pstate) {
    return exec_projectset_impl(pstate);
}

/**
 * @brief 结束 ProjectSet 节点
 *
 * 释放 ProjectSetState 关联的资源。
 *
 * @param node ProjectSetState（可为 NULL）
 */
void ExecEndProjectSet(ProjectSetState *node) {
    if (node == NULL) {
        return;
    }

    /* 释放子节点 */
    if (node->ps.lefttree != NULL) {
        ExecEndNode(node->ps.lefttree);
        node->ps.lefttree = NULL;
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

    /* 注意：ProjectSetState 本身由 EState 的查询上下文管理，不单独释放 */
}

/**
 * @brief 重置 ProjectSet 节点（用于重新扫描）
 *
 * 重置完成标志。
 *
 * @param node ProjectSetState
 */
void ExecReScanProjectSet(ProjectSetState *node) {
    if (node == NULL) {
        return;
    }

    /* 重置子节点 */
    if (node->ps.lefttree != NULL) {
        ExecReScan(node->ps.lefttree);
    }

    /* 重置完成标志 */
    node->done = false;
}
