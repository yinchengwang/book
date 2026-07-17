/**
 * @file nodeHash.c
 * @brief Hash 辅助执行器节点实现
 *
 * 实现 Task 2.3 的 Hash 辅助节点：
 *   - ExecInitHash: 初始化 HashState
 *   - ExecEndHash: 释放资源
 *
 * 当前为框架版本：
 *   - 哈希表使用简化实现（void* hashtable = NULL）
 *   - 重点验证节点初始化和生命周期管理
 *   - 实际哈希表构建逻辑后续完善
 */

#include "db/sql/nodeHash.h"
#include "db/sql/executor.h"
#include "db/sql/memctx.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ========================================================================
 * Hash 节点执行函数
 * ======================================================================== */

/**
 * @brief Hash 节点执行函数（占位）
 *
 * Hash 节点不产生输出元组，仅构建哈希表。
 * 框架版本返回 NULL。
 *
 * @param pstate PlanState（实际类型为 HashState）
 *
 * @return NULL（Hash 节点不产生输出）
 */
static TupleTableSlot *exec_hash_impl(PlanState *pstate) {
    (void)pstate;
    /* 框架版本：Hash 节点不产生输出 */
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

    /* 释放哈希表（框架版本：hashtable 为 NULL，无需释放） */
    if (node->hashtable != NULL) {
        /* TODO: 后续实现哈希表释放 */
        node->hashtable = NULL;
    }

    /* 注意：HashState 本身由 EState 的查询上下文管理，不单独释放 */
}