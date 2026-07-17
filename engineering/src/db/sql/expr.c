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
 * 内存管理：
 *   - 当前实现使用 malloc/free，与 Task 1.3 的测试约定保持一致
 *   - 后续任务将切换到基于 MemoryContext 的 palloc / reset
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
 * 返回的 ExprState 使用 malloc 分配，必须配合 ExecFreeExpr 释放。
 * ======================================================================== */

ExprState *
ExecInitExpr(Expr *expr, PlanState *parent)
{
    /* parent 当前未使用，预留以便后续任务在初始化子 ExprState 时传递上下文 */
    (void)parent;

    if (expr == NULL) {
        return NULL;
    }

    ExprState *state = (ExprState *)malloc(sizeof(ExprState));
    if (state == NULL) {
        return NULL;
    }
    memset(state, 0, sizeof(ExprState));
    state->type = T_ExprState;
    state->expr = expr;

    /* 分配两步序列：操作码 + EEOP_DONE */
    state->steps = (ExprEvalStep *)malloc(sizeof(ExprEvalStep) * 2);
    if (state->steps == NULL) {
        free(state);
        return NULL;
    }
    memset(state->steps, 0, sizeof(ExprEvalStep) * 2);

    /* 写入结果缓冲 */
    state->resvalue = (Datum *)malloc(sizeof(Datum));
    state->resnull  = (bool *)malloc(sizeof(bool));
    if (state->resvalue == NULL || state->resnull == NULL) {
        free(state->steps);
        free(state);
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
 * ExecFreeExpr - 释放表达式状态
 *
 * 释放由 ExecInitExpr 分配的 ExprState 及其内部缓冲。
 * 调用 ExecFreeExpr 后指针不再有效。
 * ======================================================================== */

void
ExecFreeExpr(ExprState *state)
{
    if (state == NULL) {
        return;
    }

    if (state->steps != NULL) {
        free(state->steps);
        state->steps = NULL;
    }
    if (state->resvalue != NULL) {
        free(state->resvalue);
        state->resvalue = NULL;
    }
    if (state->resnull != NULL) {
        free(state->resnull);
        state->resnull = NULL;
    }

    state->nsteps   = 0;
    state->expr     = NULL;
    state->eval_ctx = NULL;
    free(state);
}