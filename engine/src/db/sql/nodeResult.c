/**
 * @file nodeResult.c
 * @brief Result 执行器节点实现
 *
 * 实现 Result 节点的核心接口：
 *   - ExecInitResult：初始化 ResultState
 *   - ExecResult：Volcano 迭代执行函数
 *   - ExecEndResult：释放资源
 *   - ExecReScanResult：重置节点状态
 *
 * 本文件是 SQL 执行引擎 Phase 2 核心算子层的第一个任务（Task 2.1）。
 */

#include "db/sql/nodeResult.h"
#include "db/sql/executor.h"
#include "db/sql/memctx.h"

#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * 前向声明：Result 节点执行函数
 * ======================================================================== */

static TupleTableSlot *exec_result_impl(PlanState *pstate);

/* ========================================================================
 * ExecInitResult - 初始化 Result 节点
 * ======================================================================== */

/**
 * @brief 初始化 Result 节点
 *
 * 主要工作：
 *   1. 在查询上下文中分配 ResultState
 *   2. 设置 ExecProcNode 函数指针
 *   3. 创建结果元组槽
 *   4. 编译常量条件表达式
 *   5. 关联表达式上下文
 *
 * @param plan   计划节点（实际类型为 Result*）
 * @param estate 执行器状态
 * @param eflags 执行器标志
 */
PlanState *
ExecInitResult(Plan *plan, EState *estate, int eflags)
{
    ResultState *state;
    Result *node;

    /* 参数检查 */
    if (plan == NULL || estate == NULL) {
        return NULL;
    }

    /* 类型转换：Plan* -> Result*（安全转换，因为 Result 将 Plan 作为第一个字段） */
    node = (Result *)plan;

    /* 在查询上下文中分配 ResultState */
    state = (ResultState *)palloc0(estate->es_query_cxt, sizeof(ResultState));
    if (state == NULL) {
        return NULL;
    }

    /* 初始化基类 PlanState */
    state->ps.type = T_ResultState;
    state->ps.plan = plan;  /* 保存原始 plan 指针 */
    state->ps.state = estate;

    /* 设置执行函数指针 */
    PlanStateSetExecProc(&state->ps, exec_result_impl);

    /* 初始化子树（Result 节点可能有子计划，如 FROM 子查询） */
    state->ps.lefttree = NULL;
    state->ps.righttree = NULL;

    /* 创建结果元组槽 */
    state->resultslot = MakeTupleTableSlotWithMCxt(estate->es_query_cxt);
    state->ps.ps_ResultTupleSlot = state->resultslot;

    /* 编译常量条件表达式 */
    if (node->resconstantqual != NULL) {
        state->resconstantqual = ExecInitExpr(node->resconstantqual, &state->ps);
    } else {
        state->resconstantqual = NULL;
    }

    /* 关联表达式上下文（使用 EState 的 per-tuple 上下文） */
    if (estate->es_per_tuple_exprctx == NULL) {
        estate->es_per_tuple_exprctx = CreateExprContext(estate);
    }
    state->ps.ps_ExprContext = estate->es_per_tuple_exprctx;

    /* 初始化状态标志 */
    state->rs_done = false;

    /* 初始化其他字段 */
    state->ps.qual = NULL;
    state->ps.recheck = NULL;
    state->ps.ps_ProjInfo = NULL;
    state->ps.ps_ResultTupleDesc = NULL;
    state->ps.instrument = NULL;
    state->ps.needs_to_scan_queue = false;
    state->ps.chgParam = NULL;

    (void)eflags;  /* 当前任务未使用 eflags */

    return (PlanState *)state;
}

/* ========================================================================
 * exec_result_impl - Result 节点执行函数
 * ======================================================================== */

/**
 * @brief Result 节点执行函数
 *
 * 执行逻辑：
 *   1. 如果已返回一行（rs_done = true），直接返回 NULL
 *   2. 如果有常量条件，检查条件是否为真
 *   3. 标记已返回一行（rs_done = true）
 *   4. 返回结果元组槽
 *
 * 注意：
 *   - 当前版本不处理子计划（lefttree），后续任务会扩展
 *   - 结果槽内容在投影阶段填充，此处只返回槽本身
 */
static TupleTableSlot *
exec_result_impl(PlanState *pstate)
{
    ResultState *node = (ResultState *)pstate;

    /* 已返回过一行，后续返回 NULL */
    if (node->rs_done) {
        return NULL;
    }

    /* 检查常量条件 */
    if (node->resconstantqual != NULL) {
        ExprContext *ectxt = node->ps.ps_ExprContext;
        bool qual_result = ExecCheckEvalExpr(node->resconstantqual, ectxt);

        if (!qual_result) {
            /* 条件为假，不返回任何行 */
            node->rs_done = true;
            return NULL;
        }
    }

    /* 标记已返回一行 */
    node->rs_done = true;

    /* 返回结果槽 */
    /* 对于常量查询，结果槽内容在投影阶段填充 */
    return node->resultslot;
}

/* ========================================================================
 * ExecEndResult - 结束 Result 节点
 * ======================================================================== */

/**
 * @brief 结束 Result 节点
 *
 * 主要工作：
 *   1. 释放常量条件表达式状态
 *   2. 释放结果元组槽
 *   3. ResultState 本身由 MemoryContext 管理，无需手动释放
 */
void
ExecEndResult(ResultState *node)
{
    if (node == NULL) {
        return;
    }

    /* 释放常量条件表达式 */
    if (node->resconstantqual != NULL) {
        ExecFreeExpr(node->resconstantqual);
        node->resconstantqual = NULL;
    }

    /* 释放结果槽 */
    if (node->resultslot != NULL) {
        FreeTupleTableSlot(node->resultslot);
        node->resultslot = NULL;
    }

    /* 释放表达式上下文（如果由本节点创建） */
    if (node->ps.ps_ExprContext != NULL) {
        FreeExprContext(node->ps.ps_ExprContext, true);
        node->ps.ps_ExprContext = NULL;
    }

    /* ResultState 本身由 MemoryContext 管理，无需手动释放 */
}

/* ========================================================================
 * ExecReScanResult - 重置 Result 节点
 * ======================================================================== */

/**
 * @brief 重置 Result 节点（用于重新扫描）
 *
 * 主要工作：
 *   1. 重置 rs_done 标志
 *   2. 清空结果槽
 */
void
ExecReScanResult(ResultState *node)
{
    if (node == NULL) {
        return;
    }

    /* 重置状态 */
    node->rs_done = false;

    /* 清空结果槽 */
    if (node->resultslot != NULL) {
        ExecClearTuple(node->resultslot);
    }
}