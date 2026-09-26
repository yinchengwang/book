/**
 * @file nodeHash.c
 * @brief Hash 辅助执行器节点实现（辅助节点 — 主路径不使用）
 *
 * 实现 Hash 辅助节点：
 *   - ExecInitHash: 初始化 HashState
 *   - ExecEndHash: 释放资源
 *
 * Hash 节点仅为 HashJoin 的辅助占位；HashJoin 走独立哈希表路径
 * （nodeHashjoin.c 的 hashtable 字段），主路径不调用本节点的
 * ExecProcNode。规范 §5.3 要求禁静默桩，因此 exec_hash_impl
 * 改为显式硬报错（stderr + abort）。
 */

#include "db/sql/nodeHash.h"
#include "db/sql/executor.h"
#include "db/sql/memctx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ========================================================================
 * Hash 节点执行函数
 * ======================================================================== */

/**
 * @brief Hash 节点执行函数（辅助占位，主路径不调用）
 *
 * 显式硬报错：spec §5.3 禁止静默桩；任何走到本节点 ExecProcNode 的调用
 * 都属主路径未涵盖，进程级失败优于返回错误结果集。
 *
 * @param pstate PlanState（实际类型为 HashState）
 */
static TupleTableSlot *exec_hash_impl(PlanState *pstate) {
    (void)pstate;
    fprintf(stderr,
        "FATAL: Hash 节点 ExecProcNode 被调用 — 主路径不使用此节点，"
        "HashJoin 走独立 hashtable 路径（nodeHashjoin.c）。\n");
    abort();
    /* unreachable */
    return NULL;
}

/* ========================================================================
 * 公共 API
 * ======================================================================== */

/**
 * @brief 初始化 Hash 节点
 *
 * 分配 HashState 并初始化字段。
 *
 * @param plan   计划节点（实际类型为 Hash*）
 * @param estate 执行器状态
 * @param eflags 执行器标志
 *
 * @return 初始化后的 HashState（作为 PlanState*）；失败返回 NULL
 */
PlanState *ExecInitHash(Plan *plan, EState *estate, int eflags) {
    Hash *node;
    HashState *state;

    /* 参数检查 */
    if (plan == NULL || estate == NULL) {
        return NULL;
    }

    node = (Hash *)plan;

    /* 在查询上下文中分配 HashState */
    state = (HashState *)palloc0(estate->es_query_cxt, sizeof(HashState));
    if (state == NULL) {
        return NULL;
    }

    /* 初始化基类 */
    state->ps.type = T_HashState;
    state->ps.plan = plan;
    state->ps.state = estate;
    state->ps.ExecProcNode = exec_hash_impl;
    state->ps.ExecProcNodeReal = exec_hash_impl;

    /* 初始化子节点 */
    if (node->plan.lefttree != NULL) {
        state->ps.lefttree = ExecInitNode(node->plan.lefttree, estate, eflags);
    } else {
        state->ps.lefttree = NULL;
    }

    state->ps.righttree = NULL;

    /* 初始化表达式上下文 */
    state->ps.ps_ExprContext = CreateExprContext(estate);
    if (state->ps.ps_ExprContext == NULL) {
        /* 回退：释放已分配资源 */
        return NULL;
    }

    /* 创建结果槽 */
    state->ps.ps_ResultTupleSlot = MakeTupleTableSlotWithMCxt(estate->es_query_cxt);
    if (state->ps.ps_ResultTupleSlot == NULL) {
        return NULL;
    }

    /* 初始化 Hash 特定字段 */
    state->hashtable = NULL;  /* 框架版本：哈希表为 NULL */
    state->hashsize = 1024;    /* 默认大小 */

    return (PlanState *)state;
}

/**
 * @brief 结束 Hash 节点
 *
 * 释放 HashState 关联的资源。
 *
 * @param node HashState（可为 NULL）
 */
void ExecEndHash(HashState *node) {
    if (node == NULL) {
        return;
    }

    /* 释放子节点 */
    if (node->ps.lefttree != NULL) {
        ExecEndNode(node->ps.lefttree);
        node->ps.lefttree = NULL;
    }

    if (node->ps.righttree != NULL) {
        ExecEndNode(node->ps.righttree);
        node->ps.righttree = NULL;
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

    /* 释放哈希表 */
    if (node->hashtable != NULL) {
        /* 主路径无 hashtable；防御性兜底 */
        node->hashtable = NULL;
    }

    /* 注意：HashState 本身由 EState 的查询上下文管理，不单独释放 */
}