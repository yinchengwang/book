/**
 * @file expr_interp.c
 * @brief 表达式字节码解释器（ExecEvalExpr / ExecCheckEvalExpr）
 *
 * 解释器核心：从 steps[0] 开始顺序遍历，根据 op 字段分派执行，
 * 遇 EEOP_DONE 退出，最终结果写入 state->resvalue/state->resnull。
 *
 * 第一版本支持的子集（与任务规范一致）：
 *   - EEOP_CONST / EEOP_CONST_NULL
 *   - EEOP_NULLTEST_ISNULL / EEOP_NULLTEST_ISNOTNULL
 *   - EEOP_SCAN_VAR / EEOP_INNER_VAR / EEOP_OUTER_VAR
 *   - EEOP_FUNCEXPR_STANDARD / EEOP_FUNCEXPR_STRICT（占位：默认返回 0/NULL）
 *   - EEOP_DONE
 *
 * 跳转操作码（EEOP_JUMP*）作为占位：第一版不做真跳转，跳转目标继续顺序执行。
 *
 * EEOP_SCAN_VAR 读取语义：
 *   - 若 context == NULL 或 context->ecxt_scantuple == NULL，结果为 NULL
 *   - 否则按 attno 索引 tts_values/tts_isnull；越界视为 NULL
 */

#include "db/sql/expr.h"
#include "db/sql/nodes/execnodes.h"
#include "db/sql/memctx.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * 内部辅助：读取 slot 字段值
 * ======================================================================== */

static void
read_slot_field(TupleTableSlot *slot, int attno,
                Datum *out_value, bool *out_isnull)
{
    if (slot == NULL || slot->tts_values == NULL || slot->tts_isnull == NULL) {
        *out_value = 0;
        *out_isnull = true;
        return;
    }
    if (attno < 0 || attno >= slot->tts_nvalid) {
        *out_value = 0;
        *out_isnull = true;
        return;
    }
    *out_value = slot->tts_values[attno];
    *out_isnull = slot->tts_isnull[attno];
}

/* ========================================================================
 * ExecEvalExpr - 主解释循环
 * ======================================================================== */

Datum
ExecEvalExpr(ExprState *state, ExprContext *context, bool *isnull)
{
    if (state == NULL || state->steps == NULL || state->nsteps <= 0) {
        if (isnull != NULL) {
            *isnull = true;
        }
        return (Datum)0;
    }

    /* 上下文参数：当前未深度使用，保留以扩展 */
    (void)context;

    /* 缓存的结果指针（Var 操作需要写入 state->resvalue/resnull） */
    Datum *resvalue = state->resvalue;
    bool  *resnull  = state->resnull;

    /*
     * 字节码寄存器约定：
     *   - 每个 EEOP_CONST 步骤都直接把常量写入 resvalue/resnull
     *   - EEOP_VAR 读取 slot 值并写入 resvalue/resnull
     *   - EEOP_NULLTEST_* 读取当前 isnull，写回 resvalue/resnull
     *   - EEOP_DONE 直接返回当前结果
     */
    int pc = 0;
    while (pc < state->nsteps) {
        const ExprEvalStep *step = &state->steps[pc];

        switch (step->op) {
        case EEOP_CONST:
            if (resvalue != NULL) {
                *resvalue = step->d.const_.constval;
            }
            if (resnull != NULL) {
                *resnull = step->d.const_.constisnull;
            }
            pc++;
            break;

        case EEOP_CONST_NULL:
            if (resvalue != NULL) {
                *resvalue = (Datum)0;
            }
            if (resnull != NULL) {
                *resnull = true;
            }
            pc++;
            break;

        case EEOP_SCAN_VAR:
            if (context == NULL) {
                if (resvalue != NULL) {
                    *resvalue = (Datum)0;
                }
                if (resnull != NULL) {
                    *resnull = true;
                }
            } else {
                read_slot_field(context->ecxt_scantuple,
                                step->d.var.var_attno,
                                resvalue, resnull);
            }
            pc++;
            break;

        case EEOP_INNER_VAR:
            if (context == NULL) {
                if (resvalue != NULL) {
                    *resvalue = (Datum)0;
                }
                if (resnull != NULL) {
                    *resnull = true;
                }
            } else {
                read_slot_field(context->ecxt_innertuple,
                                step->d.var.var_attno,
                                resvalue, resnull);
            }
            pc++;
            break;

        case EEOP_OUTER_VAR:
            if (context == NULL) {
                if (resvalue != NULL) {
                    *resvalue = (Datum)0;
                }
                if (resnull != NULL) {
                    *resnull = true;
                }
            } else {
                read_slot_field(context->ecxt_outertuple,
                                step->d.var.var_attno,
                                resvalue, resnull);
            }
            pc++;
            break;

        case EEOP_ASSIGN_INNER_VAR:
        case EEOP_ASSIGN_OUTER_VAR:
        case EEOP_ASSIGN_SCAN_VAR:
            /* 赋值操作在第一版本未启用，作为占位 */
            pc++;
            break;

        case EEOP_PARAM_EXEC:
        case EEOP_PARAM_EXTERN: {
            /* 参数未启用，返回 NULL */
            if (resvalue != NULL) {
                *resvalue = (Datum)0;
            }
            if (resnull != NULL) {
                *resnull = true;
            }
            pc++;
            break;
        }

        case EEOP_NULLTEST_ISNULL: {
            /* 当前 isnull 即测试结果 */
            bool cur_null = (resnull != NULL) ? *resnull : true;
            if (resvalue != NULL) {
                *resvalue = (Datum)(cur_null ? 1 : 0);
            }
            if (resnull != NULL) {
                *resnull = false;
            }
            pc++;
            break;
        }

        case EEOP_NULLTEST_ISNOTNULL: {
            bool cur_null = (resnull != NULL) ? *resnull : true;
            if (resvalue != NULL) {
                *resvalue = (Datum)(cur_null ? 0 : 1);
            }
            if (resnull != NULL) {
                *resnull = false;
            }
            pc++;
            break;
        }

        case EEOP_BOOLTEST_IS_TRUE: {
            bool cur_null = (resnull != NULL) ? *resnull : true;
            if (cur_null) {
                if (resvalue != NULL) {
                    *resvalue = (Datum)0;
                }
                if (resnull != NULL) {
                    *resnull = true;
                }
            } else {
                Datum v = (resvalue != NULL) ? *resvalue : 0;
                if (resvalue != NULL) {
                    *resvalue = (Datum)(v != 0 ? 1 : 0);
                }
                if (resnull != NULL) {
                    *resnull = false;
                }
            }
            pc++;
            break;
        }

        case EEOP_BOOLTEST_IS_FALSE: {
            bool cur_null = (resnull != NULL) ? *resnull : true;
            if (cur_null) {
                if (resvalue != NULL) {
                    *resvalue = (Datum)0;
                }
                if (resnull != NULL) {
                    *resnull = true;
                }
            } else {
                Datum v = (resvalue != NULL) ? *resvalue : 0;
                if (resvalue != NULL) {
                    *resvalue = (Datum)(v == 0 ? 1 : 0);
                }
                if (resnull != NULL) {
                    *resnull = false;
                }
            }
            pc++;
            break;
        }

        case EEOP_FUNCEXPR_STANDARD:
        case EEOP_FUNCEXPR_STRICT:
            /*
             * 占位实现：函数调用尚未支持。
             * 返回 NULL，由后续任务补充。
             */
            if (resvalue != NULL) {
                *resvalue = (Datum)0;
            }
            if (resnull != NULL) {
                *resnull = true;
            }
            pc++;
            break;

        case EEOP_JUMP:
            /* 第一版本：不做真跳转，顺序前进 */
            pc++;
            break;
        case EEOP_JUMP_IF_NULL: {
            bool cur_null = (resnull != NULL) ? *resnull : true;
            if (cur_null) {
                pc = step->d.jump.jumpdest;
            } else {
                pc++;
            }
            break;
        }
        case EEOP_JUMP_IF_NOT_NULL: {
            bool cur_null = (resnull != NULL) ? *resnull : true;
            if (!cur_null) {
                pc = step->d.jump.jumpdest;
            } else {
                pc++;
            }
            break;
        }
        case EEOP_JUMP_IF_NOT_TRUE: {
            bool cur_null = (resnull != NULL) ? *resnull : true;
            Datum v = (resvalue != NULL) ? *resvalue : 0;
            if (cur_null || v == 0) {
                pc = step->d.jump.jumpdest;
            } else {
                pc++;
            }
            break;
        }
        case EEOP_BOOL_AND_STEP_FIRST:
        case EEOP_BOOL_AND_STEP:
        case EEOP_BOOL_OR_STEP_FIRST:
        case EEOP_OR_STEP:
        case EEOP_BOOL_NOT_STEP:
            /* 短路求值未启用：占位顺序执行 */
            pc++;
            break;

        case EEOP_AGGREF:
        case EEOP_WINDOW_FUNC:
        case EEOP_CASE_TESTVAL:
        case EEOP_ARRAYEXPR:
        case EEOP_ROW:
        case EEOP_NULLIF:
        case EEOP_COALESCE:
            /* 复杂表达式占位：返回 NULL */
            if (resvalue != NULL) {
                *resvalue = (Datum)0;
            }
            if (resnull != NULL) {
                *resnull = true;
            }
            pc++;
            break;

        case EEOP_DONE:
            if (isnull != NULL) {
                *isnull = (resnull != NULL) ? *resnull : true;
            }
            return (resvalue != NULL) ? *resvalue : (Datum)0;

        default:
            /* 未知操作码：安全退出 */
            if (isnull != NULL) {
                *isnull = true;
            }
            return (Datum)0;
        }
    }

    /* 到达此处：缺少 EEOP_DONE，返回当前结果 */
    if (isnull != NULL) {
        *isnull = (resnull != NULL) ? *resnull : true;
    }
    return (resvalue != NULL) ? *resvalue : (Datum)0;
}

/* ========================================================================
 * ExecCheckEvalExpr - 布尔求值
 * ======================================================================== */

bool
ExecCheckEvalExpr(ExprState *state, ExprContext *context)
{
    bool isnull = false;
    Datum v = ExecEvalExpr(state, context, &isnull);
    if (isnull) {
        return false;
    }
    return (v != (Datum)0);
}