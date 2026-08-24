/**
 * @file expr.c
 * @brief 表达式编译入口（ExecInitExpr / ExecFreeExpr）
 *
 * 本文件实现表达式从 Expr 树到 ExprState（ExprEvalStep 字节码序列）的编译。
 *
 * 当前版本支持的编译子集：
 *   - EXPR_CONST：单步 EEOP_CONST + EEOP_DONE
 *   - EXPR_VAR  ：单步 EEOP_SCAN_VAR + EEOP_DONE
 *   - 其他类型：默认生成 EEOP_DONE 序列（返回 NULL），避免编译失败阻塞上层
 *
 * 解释器主体（字节码执行）位于 expr_interp.c。
 *
 * 内存管理（Task 10）：
 *   - 已切换到基于 MemoryContext 的 palloc 分配，使用 CurrentMemoryContext
 *   - 生命周期由所属 MemoryContext 统一管理，ExecFreeExpr 变为 no-op
 *   - 调用方负责在调用 ExecInitExpr 前使用 MemoryContextSwitchTo 切换到合适的上下文
 *     （通常是 EState 的 es_query_cxt，或调用方提供的 AllocSet 派生上下文）
 */

#include "db/sql/expr.h"
#include "db/sql/memctx.h"
/* 引入 Expr 完整定义 */
#include "db/sql/sql_planner.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * ExecInitExpr - 编译入口
 *
 * 简化策略：根据 expr->expr_type 选择编译模式。
 * 对于规划期尚未实现完整表达式的情形，本任务保证能产生合法字节码序列。
 *
 * Task 10：返回值通过 CurrentMemoryContext 的 palloc0 分配。
 * 调用方应保证 CurrentMemoryContext 已指向合适的查询级/函数级上下文。
 * ======================================================================== */

ExprState *
ExecInitExpr(Expr *expr, PlanState *parent)
{
    /* parent 当前未使用，预留以便后续任务在初始化子 ExprState 时传递上下文 */
    (void)parent;

    if (expr == NULL) {
        return NULL;
    }

    /* Task 10：使用 palloc0 在 CurrentMemoryContext 中分配 */
    MemoryContext cxt = MemoryContextCurrent();
    ExprState *state = (ExprState *)palloc0(cxt, sizeof(ExprState));
    if (state == NULL) {
        return NULL;
    }
    state->type = T_ExprState;
    state->expr = expr;

    /* 分配两步序列：操作码 + EEOP_DONE */
    state->steps = (ExprEvalStep *)palloc(cxt, sizeof(ExprEvalStep) * 2);
    if (state->steps == NULL) {
        /* palloc 失败通常意味着 OOM，由调用方上下文决定是否回退 */
        return NULL;
    }
    memset(state->steps, 0, sizeof(ExprEvalStep) * 2);

    /* 写入结果缓冲 */
    state->resvalue = (Datum *)palloc(cxt, sizeof(Datum));
    state->resnull  = (bool *)palloc(cxt, sizeof(bool));
    if (state->resvalue == NULL || state->resnull == NULL) {
        /* palloc 失败时返回 NULL，由调用方释放所属上下文 */
        return NULL;
    }
    *(state->resvalue) = 0;
    *(state->resnull)  = true;

    switch (expr->expr_type) {
    case EXPR_CONST:
        state->steps[0].op = EEOP_CONST;
        state->steps[0].d.const_.constval    = 0;
        state->steps[0].d.const_.consttype   = expr->result_type;
        state->steps[0].d.const_.constisnull = true;
        break;
    case EXPR_VAR:
        state->steps[0].op = EEOP_SCAN_VAR;
        state->steps[0].d.var.var_attno = 0;
        state->steps[0].d.var.result    = state->resvalue;
        state->steps[0].d.var.isnull    = state->resnull;
        break;
    default:
        /* 未知类型：留 EEOP_DONE，返回 NULL 结果 */
        break;
    }

    state->steps[1].op = EEOP_DONE;
    state->nsteps      = 2;
    state->eval_ctx    = NULL;

    return state;
}

/* ========================================================================
 * ExecFreeExpr - 释放表达式状态（Task 10: 改为 no-op）
 *
 * Task 10：ExprState 及其步骤缓冲由所属 MemoryContext 统一管理。
 * 本函数保留为空操作以保证调用方兼容（旧代码无需修改），
 * 实际内存释放由 MemoryContext 的 reset/delete 完成。
 * ======================================================================== */

void
ExecFreeExpr(ExprState *state)
{
    /* Task 10: ExprState 由 MemoryContext 统一管理，无需手动释放 */
    (void)state;
}