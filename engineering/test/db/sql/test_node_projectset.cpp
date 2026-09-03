/**
 * @file test_node_projectset.cpp
 * @brief ProjectSet 节点单元测试
 *
 * 测试 ProjectSet 执行器节点（Task 2.8）。
 */

#include <gtest/gtest.h>

extern "C" {
#include "db/sql/nodeProjectSet.h"
#include "db/sql/executor.h"
#include "db/sql/memctx.h"
}

/* ========================================================================
 * 测试 fixture
 * ======================================================================== */

class ProjectSetTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* 创建内存上下文 */
        context = AllocSetContextCreate(NULL, "test_projectset", 8192, 8192, 8192);
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

TEST_F(ProjectSetTest, InitAndEnd) {
    /* 创建 ProjectSet 计划节点 */
    ProjectSet plan = {};
    plan.plan.type = T_ProjectSet;

    /* 初始化节点 */
    PlanState *ps = ExecInitProjectSet(&plan.plan, estate, 0);
    ASSERT_NE(ps, nullptr);
    EXPECT_EQ(ps->type, T_ProjectSetState);
    EXPECT_NE(ps->ExecProcNode, nullptr);

    /* 清理 */
    ExecEndProjectSet((ProjectSetState *)ps);
}

TEST_F(ProjectSetTest, BasicInit) {
    /* 创建 ProjectSet 计划节点（无子节点） */
    ProjectSet plan = {};
    plan.plan.type = T_ProjectSet;

    /* 初始化节点 */
    PlanState *ps = ExecInitProjectSet(&plan.plan, estate, 0);
    ASSERT_NE(ps, nullptr);
    EXPECT_EQ(ps->type, T_ProjectSetState);
    EXPECT_EQ(ps->plan, &plan.plan);

    ProjectSetState *pss = (ProjectSetState *)ps;
    EXPECT_EQ(pss->numexprs, 0);
    EXPECT_FALSE(pss->done);

    /* 清理 */
    ExecEndProjectSet(pss);
}

TEST_F(ProjectSetTest, ExecProcNodeReturnsNull) {
    /* 框架版本：ExecProcNode 应返回 NULL */
    ProjectSet plan = {};
    plan.plan.type = T_ProjectSet;

    PlanState *ps = ExecInitProjectSet(&plan.plan, estate, 0);
    ASSERT_NE(ps, nullptr);

    TupleTableSlot *slot = ps->ExecProcNode(ps);
    EXPECT_EQ(slot, nullptr);

    ExecEndProjectSet((ProjectSetState *)ps);
}

TEST_F(ProjectSetTest, EndNull) {
    /* ExecEndProjectSet 应安全处理 NULL */
    ExecEndProjectSet(nullptr);
}

TEST_F(ProjectSetTest, ReScan) {
    /* 创建并初始化节点 */
    ProjectSet plan = {};
    plan.plan.type = T_ProjectSet;

    PlanState *ps = ExecInitProjectSet(&plan.plan, estate, 0);
    ASSERT_NE(ps, nullptr);

    ProjectSetState *pss = (ProjectSetState *)ps;
    pss->done = true;

    /* 重置 */
    ExecReScanProjectSet(pss);
    EXPECT_FALSE(pss->done);

    /* 清理 */
    ExecEndProjectSet(pss);
}

TEST_F(ProjectSetTest, NodeRegistration) {
    /* 确保节点已注册 */
    ProjectSet plan = {};
    plan.plan.type = T_ProjectSet;

    PlanState *ps = ExecInitProjectSet(&plan.plan, estate, 0);
    ASSERT_NE(ps, nullptr);

    /* 节点应已通过 executor_register_node 注册 */
    EXPECT_NE(ps->ExecProcNode, nullptr);

    ExecEndProjectSet((ProjectSetState *)ps);
}
