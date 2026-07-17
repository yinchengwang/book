/**
 * @file test_hashagg.cpp
 * @brief HashAgg 执行器节点单元测试
 */

#include <gtest/gtest.h>
extern "C" {
#include "db/sql/nodes/nodeHashagg.h"
#include "db/sql/sql_executor.h"
}

namespace {

/* ============================================================
 * 测试辅助函数
 * ============================================================ */

/**
 * @brief 创建测试用的 HashAggState
 */
static HashAggState *create_test_hashagg_state(int num_group_cols, int *grp_col_idx) {
    HashAggPlan plan;
    memset(&plan, 0, sizeof(plan));
    plan.type = EXEC_HASHAGG;
    plan.numGroupCols = num_group_cols;
    plan.grpColIdx = grp_col_idx;
    plan.aggfnames = NULL;
    plan.targetlist = NULL;

    return ExecInitHashAgg(&plan, NULL, 0);
}

/* ============================================================
 * 测试用例
 * ============================================================ */

/**
 * @brief 测试 1: CreateAndDestroy - 创建和销毁
 */
TEST(HashAggTest, CreateAndDestroy) {
    int grp_col_idx[1] = {1};

    /* 创建 HashAggState */
    HashAggState *state = create_test_hashagg_state(1, grp_col_idx);
    ASSERT_NE(state, nullptr);
    ASSERT_NE(state->ps, nullptr);

    /* 检查基本字段 */
    EXPECT_EQ(state->ps->type, EXEC_HASHAGG);
    EXPECT_EQ(state->ps->exec_proc, ExecHashAgg);

    /* 检查扩展状态 */
    EXPECT_EQ(state->num_group_cols, 1);
    EXPECT_EQ(state->agg_groups, 0);
    EXPECT_EQ(state->agg_tuples, 0);
    EXPECT_EQ(state->agg_output, 0);
    EXPECT_FALSE(state->agg_finished);
    EXPECT_TRUE(state->agg_build_phase);

    /* 销毁 */
    ExecEndHashAgg(state);
}

/**
 * @brief 测试 2: ExecProcPointer - 执行函数指针
 */
TEST(HashAggTest, ExecProcPointer) {
    int grp_col_idx[1] = {1};

    HashAggState *state = create_test_hashagg_state(1, grp_col_idx);
    ASSERT_NE(state, nullptr);
    ASSERT_NE(state->ps, nullptr);

    /* 检查执行函数指针 */
    EXPECT_EQ(state->ps->exec_proc, ExecHashAgg);

    /* 通过函数指针调用 */
    TupleTableSlot *slot = state->ps->exec_proc((PlanState *)state);
    EXPECT_EQ(slot, nullptr);  /* 没有子节点，返回 NULL */

    /* 销毁 */
    ExecEndHashAgg(state);
}

/**
 * @brief 测试 3: HashTableCreation - 哈希表创建
 */
TEST(HashAggTest, HashTableCreation) {
    int grp_col_idx[2] = {1, 2};

    HashAggState *state = create_test_hashagg_state(2, grp_col_idx);
    ASSERT_NE(state, nullptr);

    /* 验证哈希表已创建 */
    EXPECT_NE(state->agg_hashTable, nullptr);

    /* 验证分组信息 */
    EXPECT_EQ(state->num_group_cols, 2);
    EXPECT_EQ(state->grp_col_idx[0], 1);
    EXPECT_EQ(state->grp_col_idx[1], 2);

    /* 销毁 */
    ExecEndHashAgg(state);
}

/**
 * @brief 测试 4: GroupByEmpty - 空输入分组
 */
TEST(HashAggTest, GroupByEmpty) {
    int grp_col_idx[1] = {1};

    HashAggState *state = create_test_hashagg_state(1, grp_col_idx);
    ASSERT_NE(state, nullptr);

    /* 没有子节点，应该立即完成 */
    TupleTableSlot *slot = ExecHashAgg((PlanState *)state);
    EXPECT_EQ(slot, nullptr);

    /* 验证状态 */
    EXPECT_TRUE(state->agg_finished);
    EXPECT_EQ(state->agg_groups, 0);
    EXPECT_EQ(state->agg_tuples, 0);

    /* 销毁 */
    ExecEndHashAgg(state);
}

/**
 * @brief 测试 5: SingleGroup - 单分组
 */
TEST(HashAggTest, SingleGroup) {
    int grp_col_idx[1] = {1};

    HashAggState *state = create_test_hashagg_state(1, grp_col_idx);
    ASSERT_NE(state, nullptr);

    /* 验证初始状态 */
    EXPECT_TRUE(state->agg_build_phase);
    EXPECT_EQ(state->agg_groups, 0);

    /* 执行（没有子节点） */
    TupleTableSlot *slot = ExecHashAgg((PlanState *)state);
    EXPECT_EQ(slot, nullptr);

    /* 验证完成 */
    EXPECT_TRUE(state->agg_finished);

    /* 销毁 */
    ExecEndHashAgg(state);
}

/**
 * @brief 测试 6: ReScan - 重置
 */
TEST(HashAggTest, ReScan) {
    int grp_col_idx[1] = {1};

    HashAggState *state = create_test_hashagg_state(1, grp_col_idx);
    ASSERT_NE(state, nullptr);

    /* 执行一次 */
    ExecHashAgg((PlanState *)state);
    EXPECT_TRUE(state->agg_finished);

    /* 重置 */
    ExecReScanHashAgg(state);

    /* 验证状态已重置 */
    EXPECT_FALSE(state->agg_finished);
    EXPECT_TRUE(state->agg_build_phase);
    EXPECT_EQ(state->agg_groups, 0);
    EXPECT_EQ(state->agg_tuples, 0);
    EXPECT_EQ(state->agg_output, 0);

    /* 再次执行 */
    TupleTableSlot *slot = ExecHashAgg((PlanState *)state);
    EXPECT_EQ(slot, nullptr);

    /* 销毁 */
    ExecEndHashAgg(state);
}

/**
 * @brief 测试 7: Statistics - 统计信息
 */
TEST(HashAggTest, Statistics) {
    int grp_col_idx[1] = {1};

    HashAggState *state = create_test_hashagg_state(1, grp_col_idx);
    ASSERT_NE(state, nullptr);

    /* 初始统计信息应为 0 */
    EXPECT_EQ(state->agg_groups, 0);
    EXPECT_EQ(state->agg_tuples, 0);
    EXPECT_EQ(state->agg_output, 0);

    /* 执行 */
    ExecHashAgg((PlanState *)state);

    /* 验证统计信息 */
    EXPECT_TRUE(state->agg_finished);
    EXPECT_EQ(state->agg_groups, 0);  /* 没有输入 */
    EXPECT_EQ(state->agg_tuples, 0);

    /* 销毁 */
    ExecEndHashAgg(state);
}

/**
 * @brief 测试 8: MakePlan - 计划节点构造
 */
TEST(HashAggTest, MakePlan) {
    int grp_col_idx[2] = {1, 2};

    /* 创建计划节点 */
    HashAggPlan *plan = make_hashagg_plan(2, grp_col_idx);
    ASSERT_NE(plan, nullptr);

    /* 验证计划节点字段 */
    EXPECT_EQ(plan->type, EXEC_HASHAGG);
    EXPECT_EQ(plan->numGroupCols, 2);
    EXPECT_EQ(plan->grpColIdx[0], 1);
    EXPECT_EQ(plan->grpColIdx[1], 2);

    /* 使用计划节点创建执行状态 */
    HashAggState *state = ExecInitHashAgg(plan, NULL, 0);
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->num_group_cols, 2);

    /* 销毁 */
    ExecEndHashAgg(state);
    free(plan);
}

/**
 * @brief 测试 9: TypeDistinction - 类型区分
 */
TEST(HashAggTest, TypeDistinction) {
    int grp_col_idx[1] = {1};

    /* HashAgg 与其他执行器类型不同 */
    HashAggState *state = create_test_hashagg_state(1, grp_col_idx);
    ASSERT_NE(state, nullptr);
    ASSERT_NE(state->ps, nullptr);

    /* 验证类型 */
    EXPECT_EQ(state->ps->type, EXEC_HASHAGG);
    EXPECT_NE(state->ps->type, EXEC_SEQ_SCAN);
    EXPECT_NE(state->ps->type, EXEC_NESTLOOP);
    EXPECT_NE(state->ps->type, EXEC_HASHJOIN);

    /* 验证执行函数不同 */
    EXPECT_EQ(state->ps->exec_proc, ExecHashAgg);

    /* 销毁 */
    ExecEndHashAgg(state);
}

/* ============================================================
 * 边界测试
 * ============================================================ */

/**
 * @brief 测试 NULL 参数处理
 */
TEST(HashAggTest, NullParameters) {
    /* ExecEndHashAgg 应能处理 NULL */
    EXPECT_NO_FATAL_FAILURE(ExecEndHashAgg(NULL));

    /* ExecReScanHashAgg 应能处理 NULL */
    EXPECT_NO_FATAL_FAILURE(ExecReScanHashAgg(NULL));

    /* ExecHashAgg 应能处理 NULL */
    EXPECT_EQ(ExecHashAgg(NULL), nullptr);

    /* ExecGetHashAggChild 应能处理 NULL */
    EXPECT_EQ(ExecGetHashAggChild(NULL), nullptr);

    /* ExecInitHashAgg 应能处理 NULL 计划节点 */
    EXPECT_EQ(ExecInitHashAgg(NULL, NULL, 0), nullptr);
}

/**
 * @brief 测试多列分组
 */
TEST(HashAggTest, MultiColumnGroup) {
    int grp_col_idx[3] = {1, 2, 3};

    HashAggState *state = create_test_hashagg_state(3, grp_col_idx);
    ASSERT_NE(state, nullptr);

    /* 验证分组信息 */
    EXPECT_EQ(state->num_group_cols, 3);

    /* 执行 */
    TupleTableSlot *slot = ExecHashAgg((PlanState *)state);
    EXPECT_EQ(slot, nullptr);

    /* 验证完成 */
    EXPECT_TRUE(state->agg_finished);

    /* 销毁 */
    ExecEndHashAgg(state);
}

/**
 * @brief 测试多次重置
 */
TEST(HashAggTest, MultipleRescan) {
    int grp_col_idx[1] = {1};

    HashAggState *state = create_test_hashagg_state(1, grp_col_idx);
    ASSERT_NE(state, nullptr);

    /* 多次重置和执行 */
    for (int i = 0; i < 3; i++) {
        ExecReScanHashAgg(state);
        EXPECT_FALSE(state->agg_finished);

        TupleTableSlot *slot = ExecHashAgg((PlanState *)state);
        EXPECT_EQ(slot, nullptr);
        EXPECT_TRUE(state->agg_finished);
    }

    /* 销毁 */
    ExecEndHashAgg(state);
}

/**
 * @brief 测试子节点设置
 */
TEST(HashAggTest, ChildManipulation) {
    int grp_col_idx[1] = {1};

    HashAggState *state = create_test_hashagg_state(1, grp_col_idx);
    ASSERT_NE(state, nullptr);

    /* 初始子节点为 NULL */
    EXPECT_EQ(ExecGetHashAggChild(state), nullptr);

    /* 设置子节点 */
    PlanState *fake_child = (PlanState *)malloc(sizeof(PlanState));
    ASSERT_NE(fake_child, nullptr);
    memset(fake_child, 0, sizeof(PlanState));
    ExecSetHashAggChild(state, fake_child);

    /* 验证子节点已设置 */
    EXPECT_EQ(ExecGetHashAggChild(state), fake_child);

    /* 清理子节点 */
    free(fake_child);

    /* 销毁 */
    ExecEndHashAgg(state);
}

}  /* namespace */
