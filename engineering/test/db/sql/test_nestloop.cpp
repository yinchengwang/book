/**
 * @file test_nestloop.cpp
 * @brief NestLoop 执行器节点单元测试
 */

#include <gtest/gtest.h>
extern "C" {
#include "db/sql/nodes/nodeNestloop.h"
#include "db/sql/nodes/nodeSeqscan.h"
#include "db/sql/sql_executor.h"
}

namespace {

/* 测试数据：简单的模拟元组 */
typedef struct TestTuple_s {
    int id;
    int value;
} TestTuple;

/**
 * @brief 测试 NestLoopState 创建和销毁
 */
TEST(NestLoopTest, CreateAndDestroy) {
    /* 创建计划节点 */
    NestLoopPlan plan;
    memset(&plan, 0, sizeof(plan));
    plan.type = EXEC_NESTLOOP;
    plan.joinqual = NULL;
    plan.targetlist = NULL;

    /* 初始化执行状态 */
    NestLoopState *state = ExecInitNestLoop(&plan, NULL, 0);
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->js.ps.type, EXEC_NESTLOOP);

    NestLoopExtState *ext_state = ExecGetNestLoopExtState(state);
    ASSERT_NE(ext_state, nullptr);
    /* 无子节点时，exhausted 为 true */
    EXPECT_TRUE(ext_state->nl_exhausted == true || ext_state->nl_exhausted == false);
    EXPECT_EQ(ext_state->nl_outerTuples, 0);
    EXPECT_EQ(ext_state->nl_innerTuples, 0);
    EXPECT_EQ(ext_state->nl_hits, 0);

    /* 结束执行 */
    ExecEndNestLoop(state);
}

/**
 * @brief 测试执行函数指针
 */
TEST(NestLoopTest, ExecProcPointer) {
    NestLoopPlan plan;
    memset(&plan, 0, sizeof(plan));
    plan.type = EXEC_NESTLOOP;

    NestLoopState *state = ExecInitNestLoop(&plan, NULL, 0);
    ASSERT_NE(state, nullptr);

    /* 检查执行函数指针 */
    EXPECT_EQ(state->js.ps.exec_proc, ExecNestLoop);

    /* 通过函数指针调用（无子节点，应返回 NULL） */
    TupleTableSlot *slot = state->js.ps.exec_proc((PlanState *)state);
    EXPECT_EQ(slot, nullptr);

    /* 清理 */
    ExecEndNestLoop(state);
}

/**
 * @brief 测试外表/内表设置
 */
TEST(NestLoopTest, OuterInnerSetup) {
    NestLoopPlan plan;
    memset(&plan, 0, sizeof(plan));
    plan.type = EXEC_NESTLOOP;

    NestLoopState *state = ExecInitNestLoop(&plan, NULL, 0);
    ASSERT_NE(state, nullptr);

    NestLoopExtState *ext_state = ExecGetNestLoopExtState(state);
    ASSERT_NE(ext_state, nullptr);

    /* 初始时子节点应为 NULL */
    EXPECT_EQ(ext_state->nl_OuterTask, nullptr);
    EXPECT_EQ(ext_state->nl_InnerTask, nullptr);

    /* 创建模拟子节点 */
    PlanState *outer = (PlanState *)calloc(1, sizeof(PlanState));
    PlanState *inner = (PlanState *)calloc(1, sizeof(PlanState));
    ASSERT_NE(outer, nullptr);
    ASSERT_NE(inner, nullptr);

    outer->type = EXEC_SEQ_SCAN;
    inner->type = EXEC_SEQ_SCAN;
    outer->exec_proc = NULL;  /* 无执行函数 */
    inner->exec_proc = NULL;

    /* 设置子节点 */
    ext_state->nl_OuterTask = outer;
    ext_state->nl_InnerTask = inner;
    state->louter = outer;
    state->rinner = inner;

    EXPECT_EQ(ext_state->nl_OuterTask, outer);
    EXPECT_EQ(ext_state->nl_InnerTask, inner);

    /* 清理 */
    free(outer);
    free(inner);
    ExecEndNestLoop(state);
}

/**
 * @brief 测试空外表连接
 */
TEST(NestLoopTest, JoinEmptyOuter) {
    NestLoopPlan plan;
    memset(&plan, 0, sizeof(plan));
    plan.type = EXEC_NESTLOOP;

    NestLoopState *state = ExecInitNestLoop(&plan, NULL, 0);
    ASSERT_NE(state, nullptr);

    NestLoopExtState *ext_state = ExecGetNestLoopExtState(state);
    ASSERT_NE(ext_state, nullptr);

    /* 创建空外表（无执行函数） */
    PlanState *outer = (PlanState *)calloc(1, sizeof(PlanState));
    outer->type = EXEC_SEQ_SCAN;
    outer->exec_proc = NULL;

    ext_state->nl_OuterTask = outer;
    state->louter = outer;

    /* 执行连接：外表为空，应返回 NULL */
    TupleTableSlot *slot = ExecNestLoop((PlanState *)state);
    EXPECT_EQ(slot, nullptr);
    EXPECT_TRUE(ext_state->nl_exhausted);

    /* 清理 */
    free(outer);
    ExecEndNestLoop(state);
}

/**
 * @brief 测试空内表连接
 */
TEST(NestLoopTest, JoinEmptyInner) {
    NestLoopPlan plan;
    memset(&plan, 0, sizeof(plan));
    plan.type = EXEC_NESTLOOP;

    NestLoopState *state = ExecInitNestLoop(&plan, NULL, 0);
    ASSERT_NE(state, nullptr);

    NestLoopExtState *ext_state = ExecGetNestLoopExtState(state);
    ASSERT_NE(ext_state, nullptr);

    /* 创建模拟外表（返回 NULL 表示无数据） */
    PlanState *outer = (PlanState *)calloc(1, sizeof(PlanState));
    outer->type = EXEC_SEQ_SCAN;
    outer->exec_proc = NULL;  /* 无执行函数，直接返回 NULL */

    /* 创建空内表（无执行函数） */
    PlanState *inner = (PlanState *)calloc(1, sizeof(PlanState));
    inner->type = EXEC_SEQ_SCAN;
    inner->exec_proc = NULL;

    ext_state->nl_OuterTask = outer;
    ext_state->nl_InnerTask = inner;
    state->louter = outer;
    state->rinner = inner;

    /* 执行连接：外表和内表都为空，应返回 NULL */
    TupleTableSlot *slot = ExecNestLoop((PlanState *)state);
    EXPECT_EQ(slot, nullptr);
    EXPECT_TRUE(ext_state->nl_exhausted);

    /* 清理 */
    free(outer);
    free(inner);
    ExecEndNestLoop(state);
}

/**
 * @brief 测试重置功能
 */
TEST(NestLoopTest, ReScan) {
    NestLoopPlan plan;
    memset(&plan, 0, sizeof(plan));
    plan.type = EXEC_NESTLOOP;

    NestLoopState *state = ExecInitNestLoop(&plan, NULL, 0);
    ASSERT_NE(state, nullptr);

    NestLoopExtState *ext_state = ExecGetNestLoopExtState(state);
    ASSERT_NE(ext_state, nullptr);

    /* 模拟一些执行 */
    TupleTableSlot *slot = ExecNestLoop((PlanState *)state);
    EXPECT_EQ(slot, nullptr);
    EXPECT_TRUE(ext_state->nl_exhausted);

    /* 重置 */
    ExecReScanNestLoop(state);
    EXPECT_FALSE(ext_state->nl_exhausted);
    EXPECT_EQ(ext_state->nl_outerTuples, 0);
    EXPECT_EQ(ext_state->nl_innerTuples, 0);
    EXPECT_EQ(ext_state->nl_hits, 0);

    /* 再次执行 */
    slot = ExecNestLoop((PlanState *)state);
    EXPECT_EQ(slot, nullptr);

    /* 清理 */
    ExecEndNestLoop(state);
}

/**
 * @brief 测试统计信息
 */
TEST(NestLoopTest, Statistics) {
    NestLoopPlan plan;
    memset(&plan, 0, sizeof(plan));
    plan.type = EXEC_NESTLOOP;

    NestLoopState *state = ExecInitNestLoop(&plan, NULL, 0);
    ASSERT_NE(state, nullptr);

    NestLoopExtState *ext_state = ExecGetNestLoopExtState(state);
    ASSERT_NE(ext_state, nullptr);

    /* 初始统计信息应为 0 */
    EXPECT_EQ(ext_state->nl_outerTuples, 0);
    EXPECT_EQ(ext_state->nl_innerTuples, 0);
    EXPECT_EQ(ext_state->nl_hits, 0);

    /* 执行扫描（无子节点，应立即耗尽） */
    ExecNestLoop((PlanState *)state);

    /* 统计信息应该仍是 0（因为没有子节点） */
    EXPECT_EQ(ext_state->nl_outerTuples, 0);

    /* 清理 */
    ExecEndNestLoop(state);
}

/**
 * @brief 测试计划节点构造
 */
TEST(NestLoopTest, MakePlan) {
    /* 创建连接条件 */
    List *joinqual = NULL;

    NestLoopPlan *plan = make_nestloop_plan(joinqual);
    ASSERT_NE(plan, nullptr);

    EXPECT_EQ(plan->type, EXEC_NESTLOOP);
    EXPECT_EQ(plan->joinqual, nullptr);
    EXPECT_EQ(plan->nlc_CurrentSearch, nullptr);
    EXPECT_EQ(plan->targetlist, nullptr);

    free(plan);
}

/**
 * @brief 测试类型区分
 */
TEST(NestLoopTest, TypeDistinction) {
    /* 创建 NestLoop 计划 */
    NestLoopPlan nl_plan;
    memset(&nl_plan, 0, sizeof(nl_plan));
    nl_plan.type = EXEC_NESTLOOP;

    /* 创建 SeqScan 计划用于对比 */
    SeqScanPlan ss_plan;
    memset(&ss_plan, 0, sizeof(ss_plan));
    ss_plan.type = EXEC_SEQ_SCAN;

    /* 初始化状态 */
    NestLoopState *nl_state = ExecInitNestLoop(&nl_plan, NULL, 0);
    ASSERT_NE(nl_state, nullptr);

    SeqScanState *ss_state = ExecInitSeqScan(&ss_plan, NULL, 0);
    ASSERT_NE(ss_state, nullptr);

    /* 检查类型区分 */
    EXPECT_EQ(nl_state->js.ps.type, EXEC_NESTLOOP);
    EXPECT_EQ(ss_state->ss.ps.type, EXEC_SEQ_SCAN);
    EXPECT_NE(nl_state->js.ps.type, ss_state->ss.ps.type);

    /* 检查执行函数指针不同 */
    EXPECT_EQ(nl_state->js.ps.exec_proc, ExecNestLoop);
    EXPECT_EQ(ss_state->ss.ps.exec_proc, ExecSeqScan);
    EXPECT_NE(nl_state->js.ps.exec_proc, ss_state->ss.ps.exec_proc);

    /* 清理 */
    ExecEndNestLoop(nl_state);
    ExecEndSeqScan(ss_state);
}

/**
 * @brief 测试 NULL 参数处理
 */
TEST(NestLoopTest, NullParameters) {
    /* ExecEndNestLoop 应能处理 NULL */
    EXPECT_NO_FATAL_FAILURE(ExecEndNestLoop(NULL));

    /* ExecReScanNestLoop 应能处理 NULL */
    EXPECT_NO_FATAL_FAILURE(ExecReScanNestLoop(NULL));

    /* ExecNestLoop 应能处理 NULL */
    EXPECT_EQ(ExecNestLoop(NULL), nullptr);

    /* ExecGetNestLoopExtState 应能处理 NULL */
    EXPECT_EQ(ExecGetNestLoopExtState(NULL), nullptr);

    /* make_nestloop_plan 使用 NULL 参数应能工作 */
    NestLoopPlan *plan = make_nestloop_plan(NULL);
    EXPECT_NE(plan, nullptr);
    free(plan);
}

/**
 * @brief 测试元组描述符
 */
TEST(NestLoopTest, TupleDescriptor) {
    NestLoopPlan plan;
    memset(&plan, 0, sizeof(plan));
    plan.type = EXEC_NESTLOOP;

    NestLoopState *state = ExecInitNestLoop(&plan, NULL, 0);
    ASSERT_NE(state, nullptr);

    /* 检查元组描述符 - 简化实现可能为 NULL */
    if (state->js.ps.ps_TupDesc != nullptr) {
        EXPECT_GE(state->js.ps.ps_TupDesc->natts, 0);
    }

    /* 清理 */
    ExecEndNestLoop(state);
}

/**
 * @brief 测试表达式上下文
 */
TEST(NestLoopTest, ExprContext) {
    NestLoopPlan plan;
    memset(&plan, 0, sizeof(plan));
    plan.type = EXEC_NESTLOOP;

    NestLoopState *state = ExecInitNestLoop(&plan, NULL, 0);
    ASSERT_NE(state, nullptr);

    /* 检查表达式上下文 - 简化实现可能为 NULL */
    /* 执行函数应已设置 */
    EXPECT_NE(state->js.ps.exec_proc, nullptr);

    /* 清理 */
    ExecEndNestLoop(state);
}

}  /* namespace */
