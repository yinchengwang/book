/**
 * @file test_limit.cpp
 * @brief Limit 执行器节点单元测试
 *
 * 测试 Limit 节点的创建、销毁、执行和重置功能。
 * 验证 OFFSET 和 LIMIT 逻辑的正确性。
 */

#include <gtest/gtest.h>

extern "C" {
#include "db/sql/nodeLimit.h"
#include "db/sql/nodes/nodetags.h"
#include "db/sql/nodes/execnodes.h"
#include "db/sql/memctx.h"
}

#include <stdlib.h>
#include <string.h>

namespace {

/* ========================================================================
 * 测试夹具类
 * ======================================================================== */

/**
 * @brief Limit 节点测试夹具
 */
class LimitTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* 创建内存上下文 */
        ctx_ = AllocSetContextCreate(
            NULL,
            "LimitTestContext",
            0,
            ALLOCSET_DEFAULT_BLOCK_SIZE,
            ALLOCSET_DEFAULT_BLOCK_SIZE
        );
        ASSERT_NE(ctx_, nullptr);

        /* 创建 EState */
        estate_ = (EState *)palloc0(ctx_, sizeof(EState));
        ASSERT_NE(estate_, nullptr);
        estate_->es_query_cxt = ctx_;
        estate_->type = T_EState;
    }

    void TearDown() override {
        if (ctx_ != NULL) {
            delete_memory(ctx_);
            ctx_ = NULL;
        }
        estate_ = NULL;
    }

    /**
     * @brief 创建简单的 Limit 计划节点
     */
    Limit *CreateLimitPlan(int offset, int count) {
        Limit *plan = (Limit *)palloc0(ctx_, sizeof(Limit));
        plan->plan.type = T_Limit;
        plan->plan.lefttree = NULL;
        plan->plan.righttree = NULL;
        plan->limitOffset = offset;
        plan->limitCount = count;
        return plan;
    }

    MemoryContext ctx_;
    EState *estate_;
};

/* ========================================================================
 * 测试用例
 * ======================================================================== */

/**
 * @brief 测试 LimitState 创建和销毁
 */
TEST_F(LimitTest, CreateAndDestroy) {
    Limit *plan = CreateLimitPlan(0, 10);

    /* 初始化 LimitState */
    PlanState *state = ExecInitLimit((Plan *)plan, estate_, 0);
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->type, T_LimitState);

    /* 检查 LimitState 特定字段 */
    LimitState *limit_state = (LimitState *)state;
    EXPECT_EQ(limit_state->limitOffset, 0);
    EXPECT_EQ(limit_state->limitCount, 10);
    EXPECT_FALSE(limit_state->noCount);

    /* 结束执行 */
    ExecEndLimit(limit_state);
}

/**
 * @brief 测试 Limit 执行函数指针
 */
TEST_F(LimitTest, ExecProcPointer) {
    Limit *plan = CreateLimitPlan(0, 10);

    /* 初始化执行状态 */
    PlanState *state = ExecInitLimit((Plan *)plan, estate_, 0);
    ASSERT_NE(state, nullptr);

    /* 检查执行函数指针 */
    EXPECT_NE(state->ExecProcNode, nullptr);

    /* 通过函数指针调用（无子节点，应返回 NULL） */
    TupleTableSlot *slot = state->ExecProcNode(state);
    EXPECT_EQ(slot, nullptr);

    /* 结束执行 */
    ExecEndLimit((LimitState *)state);
}

/**
 * @brief 测试 LIMIT 0（限制 0 个）
 */
TEST_F(LimitTest, LimitZero) {
    Limit *plan = CreateLimitPlan(0, 0);

    /* 初始化执行状态 */
    PlanState *state = ExecInitLimit((Plan *)plan, estate_, 0);
    ASSERT_NE(state, nullptr);

    /* 检查状态 */
    LimitState *limit_state = (LimitState *)state;
    EXPECT_EQ(limit_state->limitCount, 0);
    EXPECT_FALSE(limit_state->noCount);  /* limitCount = 0 不是 noCount */

    /* 执行：应返回 NULL（框架版本总是返回 NULL） */
    TupleTableSlot *slot = ExecLimit(state);
    EXPECT_EQ(slot, nullptr);

    /* 结束执行 */
    ExecEndLimit(limit_state);
}

/**
 * @brief 测试带 OFFSET 的限制
 */
TEST_F(LimitTest, LimitWithOffset) {
    Limit *plan = CreateLimitPlan(5, 10);

    /* 初始化执行状态 */
    PlanState *state = ExecInitLimit((Plan *)plan, estate_, 0);
    ASSERT_NE(state, nullptr);

    /* 检查状态 */
    LimitState *limit_state = (LimitState *)state;
    EXPECT_EQ(limit_state->limitOffset, 5);
    EXPECT_EQ(limit_state->limitCount, 10);

    /* 结束执行 */
    ExecEndLimit(limit_state);
}

/**
 * @brief 测试无 LIMIT（limitCount = -1）
 */
TEST_F(LimitTest, NoLimit) {
    Limit *plan = CreateLimitPlan(0, -1);

    /* 初始化执行状态 */
    PlanState *state = ExecInitLimit((Plan *)plan, estate_, 0);
    ASSERT_NE(state, nullptr);

    /* 检查状态 */
    LimitState *limit_state = (LimitState *)state;
    EXPECT_EQ(limit_state->limitCount, -1);
    EXPECT_TRUE(limit_state->noCount);  /* 无限制模式 */

    /* 执行：无子节点，返回 NULL */
    TupleTableSlot *slot = ExecLimit(state);
    EXPECT_EQ(slot, nullptr);

    /* 结束执行 */
    ExecEndLimit(limit_state);
}

/**
 * @brief 测试重置
 */
TEST_F(LimitTest, ReScan) {
    Limit *plan = CreateLimitPlan(0, 10);

    /* 初始化执行状态 */
    PlanState *state = ExecInitLimit((Plan *)plan, estate_, 0);
    ASSERT_NE(state, nullptr);

    LimitState *limit_state = (LimitState *)state;

    /* 第一次执行 */
    TupleTableSlot *slot = ExecLimit(state);
    EXPECT_EQ(slot, nullptr);  /* 框架版本返回 NULL */

    /* 重置 */
    ExecReScanLimit(limit_state);
    EXPECT_EQ(limit_state->position, 0);

    /* 再次执行 */
    slot = ExecLimit(state);
    EXPECT_EQ(slot, nullptr);

    /* 结束执行 */
    ExecEndLimit(limit_state);
}

/**
 * @brief 测试计划节点构造
 */
TEST_F(LimitTest, MakePlan) {
    Limit *plan;

    /* 测试正常构造 */
    plan = CreateLimitPlan(5, 10);
    ASSERT_NE(plan, nullptr);
    EXPECT_EQ(plan->plan.type, T_Limit);
    EXPECT_EQ(plan->limitOffset, 5);
    EXPECT_EQ(plan->limitCount, 10);

    /* 测试 OFFSET 0 */
    plan = CreateLimitPlan(0, 5);
    ASSERT_NE(plan, nullptr);
    EXPECT_EQ(plan->limitOffset, 0);
    EXPECT_EQ(plan->limitCount, 5);

    /* 测试无限制 */
    plan = CreateLimitPlan(0, -1);
    ASSERT_NE(plan, nullptr);
    EXPECT_EQ(plan->limitCount, -1);

    /* 测试仅 OFFSET */
    plan = CreateLimitPlan(10, -1);
    ASSERT_NE(plan, nullptr);
    EXPECT_EQ(plan->limitOffset, 10);
    EXPECT_EQ(plan->limitCount, -1);
}

/**
 * @brief 测试类型区分
 */
TEST_F(LimitTest, TypeDistinction) {
    Limit *plan = CreateLimitPlan(0, 10);

    /* 初始化执行状态 */
    PlanState *state = ExecInitLimit((Plan *)plan, estate_, 0);
    ASSERT_NE(state, nullptr);

    /* 检查类型 */
    EXPECT_EQ(state->type, T_LimitState);

    /* 结束执行 */
    ExecEndLimit((LimitState *)state);
}

/**
 * @brief 测试 NULL 参数处理
 */
TEST_F(LimitTest, NullParameters) {
    /* ExecEndLimit 应能处理 NULL */
    EXPECT_NO_FATAL_FAILURE(ExecEndLimit(NULL));

    /* ExecReScanLimit 应能处理 NULL */
    EXPECT_NO_FATAL_FAILURE(ExecReScanLimit(NULL));

    /* ExecLimit 应能处理 NULL */
    EXPECT_EQ(ExecLimit(NULL), nullptr);

    /* ExecInitLimit 应能处理 NULL plan */
    EXPECT_EQ(ExecInitLimit(NULL, estate_, 0), nullptr);

    /* ExecInitLimit 应能处理 NULL estate */
    Limit *plan = CreateLimitPlan(0, 10);
    ASSERT_NE(plan, nullptr);
    EXPECT_EQ(ExecInitLimit((Plan *)plan, NULL, 0), nullptr);
}

/**
 * @brief 测试大数值限制
 */
TEST_F(LimitTest, LargeLimit) {
    Limit *plan = CreateLimitPlan(0, 1000000);

    /* 初始化执行状态 */
    PlanState *state = ExecInitLimit((Plan *)plan, estate_, 0);
    ASSERT_NE(state, nullptr);

    LimitState *limit_state = (LimitState *)state;
    EXPECT_EQ(limit_state->limitCount, 1000000);
    EXPECT_FALSE(limit_state->noCount);

    /* 结束执行 */
    ExecEndLimit(limit_state);
}

/**
 * @brief 测试大数值偏移
 */
TEST_F(LimitTest, LargeOffset) {
    Limit *plan = CreateLimitPlan(1000000, 10);

    /* 初始化执行状态 */
    PlanState *state = ExecInitLimit((Plan *)plan, estate_, 0);
    ASSERT_NE(state, nullptr);

    LimitState *limit_state = (LimitState *)state;
    EXPECT_EQ(limit_state->limitOffset, 1000000);

    /* 结束执行 */
    ExecEndLimit(limit_state);
}

}  /* namespace */
