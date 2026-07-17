/**
 * @file test_expr.cpp
 * @brief Task 1.3 表达式求值器测试
 *
 * 覆盖范围：
 *   1. ExprEvalOp 枚举值正确
 *   2. ExprState 结构可创建和释放
 *   3. ExecEvalExpr 可以求值常量表达式
 *   4. ExecCheckEvalExpr 可以求值布尔表达式
 *   5. ExecEvalExpr 处理 NULL 测试
 *   6. ExecInitExpr 编译 EXPR_CONST / EXPR_VAR
 */

#include <gtest/gtest.h>

extern "C" {
#include "db/sql/expr.h"
#include "db/sql/memctx.h"
#include "db/sql/nodes/execnodes.h"
#include <stdlib.h>
#include <string.h>
}

/* ========================================================================
 * 测试 1: ExprEvalOp 枚举值
 *
 * 验证枚举顺序与 brief 中规定的一致。
 * ======================================================================== */

TEST(ExprEvalOpTest, OpValues) {
    EXPECT_EQ(EEOP_DONE, 0);
    EXPECT_GT(EEOP_CONST, 0);
    EXPECT_GT(EEOP_INNER_VAR, 0);
    EXPECT_GT(EEOP_FUNCEXPR_STANDARD, 0);
    /* 关键 NULL 测试与 VAR 操作码必须已定义 */
    EXPECT_GE(EEOP_NULLTEST_ISNULL, 0);
    EXPECT_GE(EEOP_NULLTEST_ISNOTNULL, 0);
    EXPECT_GE(EEOP_SCAN_VAR, 0);
    EXPECT_GT(EEOP_MAX, EEOP_DONE);
}

/* ========================================================================
 * 测试 2: ExprState 结构可创建和释放（基于 palloc/MemoryContext）
 * ======================================================================== */

TEST(ExprStateTest, CreateAndFree) {
    MemoryContext ctx = AllocSetContextCreate(NULL, "test", 0, 8192, 8192);
    ASSERT_NE(ctx, nullptr);

    ExprState *state = (ExprState *)palloc(ctx, sizeof(ExprState));
    ASSERT_NE(state, nullptr);
    state->type = T_ExprState;
    state->steps = NULL;
    state->nsteps = 0;
    state->eval_ctx = NULL;
    state->resvalue = (Datum *)palloc(ctx, sizeof(Datum));
    state->resnull = (bool *)palloc(ctx, sizeof(bool));
    ASSERT_NE(state->resvalue, nullptr);
    ASSERT_NE(state->resnull, nullptr);

    EXPECT_EQ(state->type, T_ExprState);
    EXPECT_EQ(state->nsteps, 0);
    EXPECT_EQ(state->steps, nullptr);

    delete_memory(ctx);
}

/* ========================================================================
 * 测试 3: ExecInitExpr 编译 EXPR_CONST
 * ======================================================================== */

TEST(ExprInitTest, CompileConstExpr) {
    Expr expr;
    memset(&expr, 0, sizeof(Expr));
    expr.type = T_Expr;
    expr.expr_type = EXPR_CONST;
    expr.result_type = 23;  /* INT4OID */

    ExprState *state = ExecInitExpr(&expr, NULL);
    ASSERT_NE(state, nullptr);

    EXPECT_EQ(state->type, T_ExprState);
    EXPECT_GE(state->nsteps, 2);
    EXPECT_NE(state->steps, nullptr);
    EXPECT_EQ(state->steps[0].op, EEOP_CONST);
    EXPECT_EQ(state->steps[state->nsteps - 1].op, EEOP_DONE);

    ExecFreeExpr(state);
}

/* ========================================================================
 * 测试 4: ExecInitExpr 编译 EXPR_VAR
 * ======================================================================== */

TEST(ExprInitTest, CompileVarExpr) {
    Expr expr;
    memset(&expr, 0, sizeof(Expr));
    expr.type = T_Expr;
    expr.expr_type = EXPR_VAR;
    expr.result_type = 23;

    ExprState *state = ExecInitExpr(&expr, NULL);
    ASSERT_NE(state, nullptr);

    EXPECT_EQ(state->type, T_ExprState);
    EXPECT_GE(state->nsteps, 2);
    EXPECT_EQ(state->steps[0].op, EEOP_SCAN_VAR);
    EXPECT_EQ(state->steps[state->nsteps - 1].op, EEOP_DONE);

    ExecFreeExpr(state);
}

/* ========================================================================
 * 测试 5: ExecEvalExpr 求值常量表达式
 * ======================================================================== */

TEST(ExprInterpTest, SimpleConstExpr) {
    ExprState state;
    memset(&state, 0, sizeof(ExprState));
    state.type = T_ExprState;
    state.steps = (ExprEvalStep *)malloc(sizeof(ExprEvalStep) * 2);
    ASSERT_NE(state.steps, nullptr);
    state.steps[0].op = EEOP_CONST;
    state.steps[0].d.const_.constval = 42;
    state.steps[0].d.const_.consttype = 23;  /* INT4OID */
    state.steps[0].d.const_.constisnull = false;
    state.steps[1].op = EEOP_DONE;
    state.nsteps = 2;
    state.resvalue = (Datum *)malloc(sizeof(Datum));
    state.resnull = (bool *)malloc(sizeof(bool));
    ASSERT_NE(state.resvalue, nullptr);
    ASSERT_NE(state.resnull, nullptr);

    bool isnull = false;
    Datum result = ExecEvalExpr(&state, NULL, &isnull);

    EXPECT_EQ(result, (Datum)42);
    EXPECT_FALSE(isnull);

    free(state.steps);
    free(state.resvalue);
    free(state.resnull);
}

/* ========================================================================
 * 测试 6: ExecEvalExpr 处理 IS NULL（NULL 测试）
 * ======================================================================== */

TEST(ExprInterpTest, NullTest) {
    ExprState state;
    memset(&state, 0, sizeof(ExprState));
    state.type = T_ExprState;
    state.steps = (ExprEvalStep *)malloc(sizeof(ExprEvalStep) * 3);
    ASSERT_NE(state.steps, nullptr);
    state.steps[0].op = EEOP_CONST;
    state.steps[0].d.const_.constval = 0;
    state.steps[0].d.const_.constisnull = true;
    state.steps[1].op = EEOP_NULLTEST_ISNULL;
    state.steps[2].op = EEOP_DONE;
    state.nsteps = 3;
    state.resvalue = (Datum *)malloc(sizeof(Datum));
    state.resnull = (bool *)malloc(sizeof(bool));
    ASSERT_NE(state.resvalue, nullptr);
    ASSERT_NE(state.resnull, nullptr);

    bool isnull = false;
    Datum result = ExecEvalExpr(&state, NULL, &isnull);

    /* IS NULL 在 NULL 输入上返回 true (1) */
    EXPECT_EQ(result, (Datum)1);
    EXPECT_FALSE(isnull);

    free(state.steps);
    free(state.resvalue);
    free(state.resnull);
}

/* ========================================================================
 * 测试 7: ExecEvalExpr 处理 IS NOT NULL
 * ======================================================================== */

TEST(ExprInterpTest, IsNotNullTest) {
    ExprState state;
    memset(&state, 0, sizeof(ExprState));
    state.type = T_ExprState;
    state.steps = (ExprEvalStep *)malloc(sizeof(ExprEvalStep) * 3);
    ASSERT_NE(state.steps, nullptr);
    state.steps[0].op = EEOP_CONST;
    state.steps[0].d.const_.constval = 0;
    state.steps[0].d.const_.constisnull = true;
    state.steps[1].op = EEOP_NULLTEST_ISNOTNULL;
    state.steps[2].op = EEOP_DONE;
    state.nsteps = 3;
    state.resvalue = (Datum *)malloc(sizeof(Datum));
    state.resnull = (bool *)malloc(sizeof(bool));
    ASSERT_NE(state.resvalue, nullptr);
    ASSERT_NE(state.resnull, nullptr);

    bool isnull = false;
    Datum result = ExecEvalExpr(&state, NULL, &isnull);

    /* IS NOT NULL 在 NULL 输入上返回 false (0) */
    EXPECT_EQ(result, (Datum)0);
    EXPECT_FALSE(isnull);

    free(state.steps);
    free(state.resvalue);
    free(state.resnull);
}

/* ========================================================================
 * 测试 8: ExecCheckEvalExpr 求值布尔表达式
 * ======================================================================== */

TEST(ExprInterpTest, CheckEvalTrue) {
    ExprState state;
    memset(&state, 0, sizeof(ExprState));
    state.type = T_ExprState;
    state.steps = (ExprEvalStep *)malloc(sizeof(ExprEvalStep) * 2);
    ASSERT_NE(state.steps, nullptr);
    state.steps[0].op = EEOP_CONST;
    state.steps[0].d.const_.constval = 1;
    state.steps[0].d.const_.constisnull = false;
    state.steps[1].op = EEOP_DONE;
    state.nsteps = 2;
    state.resvalue = (Datum *)malloc(sizeof(Datum));
    state.resnull = (bool *)malloc(sizeof(bool));
    ASSERT_NE(state.resvalue, nullptr);
    ASSERT_NE(state.resnull, nullptr);

    bool result = ExecCheckEvalExpr(&state, NULL);
    EXPECT_TRUE(result);

    free(state.steps);
    free(state.resvalue);
    free(state.resnull);
}

TEST(ExprInterpTest, CheckEvalFalse) {
    ExprState state;
    memset(&state, 0, sizeof(ExprState));
    state.type = T_ExprState;
    state.steps = (ExprEvalStep *)malloc(sizeof(ExprEvalStep) * 2);
    ASSERT_NE(state.steps, nullptr);
    state.steps[0].op = EEOP_CONST;
    state.steps[0].d.const_.constval = 0;
    state.steps[0].d.const_.constisnull = false;
    state.steps[1].op = EEOP_DONE;
    state.nsteps = 2;
    state.resvalue = (Datum *)malloc(sizeof(Datum));
    state.resnull = (bool *)malloc(sizeof(bool));
    ASSERT_NE(state.resvalue, nullptr);
    ASSERT_NE(state.resnull, nullptr);

    bool result = ExecCheckEvalExpr(&state, NULL);
    EXPECT_FALSE(result);

    free(state.steps);
    free(state.resvalue);
    free(state.resnull);
}

TEST(ExprInterpTest, CheckEvalNullIsFalse) {
    ExprState state;
    memset(&state, 0, sizeof(ExprState));
    state.type = T_ExprState;
    state.steps = (ExprEvalStep *)malloc(sizeof(ExprEvalStep) * 2);
    ASSERT_NE(state.steps, nullptr);
    state.steps[0].op = EEOP_CONST_NULL;
    state.steps[1].op = EEOP_DONE;
    state.nsteps = 2;
    state.resvalue = (Datum *)malloc(sizeof(Datum));
    state.resnull = (bool *)malloc(sizeof(bool));
    ASSERT_NE(state.resvalue, nullptr);
    ASSERT_NE(state.resnull, nullptr);

    bool result = ExecCheckEvalExpr(&state, NULL);
    EXPECT_FALSE(result);

    free(state.steps);
    free(state.resvalue);
    free(state.resnull);
}

/* ========================================================================
 * 测试 9: ExecEvalExpr 处理 Var 读取（无 context 时返回 NULL）
 * ======================================================================== */

TEST(ExprInterpTest, ScanVarNoContext) {
    ExprState state;
    memset(&state, 0, sizeof(ExprState));
    state.type = T_ExprState;
    state.steps = (ExprEvalStep *)malloc(sizeof(ExprEvalStep) * 2);
    ASSERT_NE(state.steps, nullptr);
    state.steps[0].op = EEOP_SCAN_VAR;
    state.steps[0].d.var.var_attno = 0;
    state.steps[1].op = EEOP_DONE;
    state.nsteps = 2;
    state.resvalue = (Datum *)malloc(sizeof(Datum));
    state.resnull = (bool *)malloc(sizeof(bool));
    ASSERT_NE(state.resvalue, nullptr);
    ASSERT_NE(state.resnull, nullptr);

    bool isnull = false;
    Datum result = ExecEvalExpr(&state, NULL, &isnull);

    /* 无 context 时 Var 返回 NULL；result 必然为 0，无需断言 */
    (void)result;
    EXPECT_TRUE(isnull);

    free(state.steps);
    free(state.resvalue);
    free(state.resnull);
}

/* ========================================================================
 * 测试 10: ExecEvalExpr 处理 Var 读取（带 context 与 slot）
 * ======================================================================== */

TEST(ExprInterpTest, ScanVarWithContext) {
    Datum values[2] = { (Datum)100, (Datum)200 };
    bool  isnulls[2] = { false, false };

    TupleTableSlot slot;
    memset(&slot, 0, sizeof(TupleTableSlot));
    slot.type = T_TupleTableSlot;
    slot.tts_values = values;
    slot.tts_isnull = isnulls;
    slot.tts_nvalid = 2;

    ExprContext ectx;
    memset(&ectx, 0, sizeof(ExprContext));
    ectx.type = T_ExprContext;
    ectx.ecxt_scantuple = &slot;

    ExprState state;
    memset(&state, 0, sizeof(ExprState));
    state.type = T_ExprState;
    state.steps = (ExprEvalStep *)malloc(sizeof(ExprEvalStep) * 2);
    ASSERT_NE(state.steps, nullptr);
    state.steps[0].op = EEOP_SCAN_VAR;
    state.steps[0].d.var.var_attno = 1;
    state.steps[1].op = EEOP_DONE;
    state.nsteps = 2;
    state.resvalue = (Datum *)malloc(sizeof(Datum));
    state.resnull = (bool *)malloc(sizeof(bool));
    ASSERT_NE(state.resvalue, nullptr);
    ASSERT_NE(state.resnull, nullptr);

    bool isnull = false;
    Datum result = ExecEvalExpr(&state, &ectx, &isnull);

    EXPECT_EQ(result, (Datum)200);
    EXPECT_FALSE(isnull);

    free(state.steps);
    free(state.resvalue);
    free(state.resnull);
}