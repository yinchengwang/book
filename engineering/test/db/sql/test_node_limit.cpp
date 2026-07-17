/**
 * @file test_node_limit.cpp
 * @brief Limit 节点单元测试
 *
 * 测试 Limit 执行器节点（Task 2.6）。
 */

#include <gtest/gtest.h>

extern "C" {
#include "db/sql/nodeLimit.h"
#include "db/sql/executor.h"
#include "db/sql/memctx.h"
}

/* ========================================================================
 * 测试 fixture
 * ======================================================================== */

class LimitNodeTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* 创建内存上下文 */
        context = AllocSetContextCreate(NULL, "test_limit", 8192, 8192, 8192);
        ASSERT_NE(context, nullptr);

        /* 创建 EState */
        estate = (EState *)palloc0(context, sizeof(EState));
        ASSERT_NE(estate, nullptr);
        estate->es_query_cxt = context;
        estate->es_processed = 0;
    }

    void TearDown() override {
        /* 清理：estate 和 context 都在内存上下文中分配，释放根上下文即可 */
        if (context != nullptr) {
            delete_memory(context);
        }
    }

    MemoryContext context = nullptr;
    EState *estate = nullptr;
};

/* ========================================================================
 * 初始化测试
 * ======================================================================== */

TEST_F(LimitNodeTest, InitAndEnd) {
    /* 创建 Limit 计划节点 */
    Limit plan = {};
    plan.plan.type = T_Limit;
    plan.limitOffset = 0;
    plan.limitCount = 10;

    /* 初始化节点 */
    PlanState *ps = ExecInitLimit(&plan.plan, estate, 0);
    ASSERT_NE(ps, nullptr);
    EXPECT_EQ(ps->type, T_LimitState);
    EXPECT_NE(ps->ExecProcNode, nullptr);

    /* 清理 */
    ExecEndLimit((LimitState *)ps);
}

TEST_F(LimitNodeTest, BasicInit) {
    /* 创建 Limit 计划节点（无子节点） */
    Limit plan = {};
    plan.plan.type = T_Limit;
    plan.limitOffset = 5;
    plan.limitCount = 10;

    /* 初始化节点 */
    PlanState *ps = ExecInitLimit(&plan.plan, estate, 0);
    ASSERT_NE(ps, nullptr);
    EXPECT_EQ(ps->type, T_LimitState);
    EXPECT_EQ(ps->plan, &plan.plan);

    LimitState *ls = (LimitState *)ps;
    EXPECT_EQ(ls->limitOffset, 5);
    EXPECT_EQ(ls->limitCount, 10);
    EXPECT_EQ(ls->position, 0);
    EXPECT_FALSE(ls->noCount);

    /* 清理 */
    ExecEndLimit(ls);
}

TEST_F(LimitNodeTest, ExecProcNodeReturnsNull) {
    /* 框架版本：ExecProcNode 应返回 NULL */
    Limit plan = {};
    plan.plan.type = T_Limit;
    plan.limitOffset = 0;
    plan.limitCount = 10;

    PlanState *ps = ExecInitLimit(&plan.plan, estate, 0);
    ASSERT_NE(ps, nullptr);

    TupleTableSlot *slot = ps->ExecProcNode(ps);
    EXPECT_EQ(slot, nullptr);

    ExecEndLimit((LimitState *)ps);
}

TEST_F(LimitNodeTest, EndNull) {
    /* ExecEndLimit 应安全处理 NULL */
    ExecEndLimit(nullptr);
}

TEST_F(LimitNodeTest, ReScan) {
    /* 创建并初始化节点 */
    Limit plan = {};
    plan.plan.type = T_Limit;
    plan.limitOffset = 0;
    plan.limitCount = 10;

    PlanState *ps = ExecInitLimit(&plan.plan, estate, 0);
    ASSERT_NE(ps, nullptr);

    LimitState *ls = (LimitState *)ps;
    ls->position = 5;

    /* 重置 */
    ExecReScanLimit(ls);
    EXPECT_EQ(ls->position, 0);

    /* 清理 */
    ExecEndLimit(ls);
}

TEST_F(LimitNodeTest, NodeRegistration) {
    /* 确保节点已注册 */
    Limit plan = {};
    plan.plan.type = T_Limit;
    plan.limitOffset = 0;
    plan.limitCount = 10;

    PlanState *ps = ExecInitLimit(&plan.plan, estate, 0);
    ASSERT_NE(ps, nullptr);

    /* 节点应已通过 executor_register_node 注册 */
    EXPECT_NE(ps->ExecProcNode, nullptr);

    ExecEndLimit((LimitState *)ps);
}

TEST_F(LimitNodeTest, NoCountFlag) {
    /* 测试 noCount 标志 */
    Limit plan = {};
    plan.plan.type = T_Limit;
    plan.limitOffset = 5;
    plan.limitCount = -1;  /* 无 LIMIT */

    PlanState *ps = ExecInitLimit(&plan.plan, estate, 0);
    ASSERT_NE(ps, nullptr);

    LimitState *ls = (LimitState *)ps;
    EXPECT_TRUE(ls->noCount);

    ExecEndLimit(ls);
}