/**
 * @file test_node_modifytable.cpp
 * @brief ModifyTable 节点单元测试
 *
 * 测试 ModifyTable 执行器节点（Task 2.7）。
 */

#include <gtest/gtest.h>

extern "C" {
#include "db/sql/nodeModifyTable.h"
#include "db/sql/executor.h"
#include "db/sql/memctx.h"
}

/* ========================================================================
 * 测试 fixture
 * ======================================================================== */

class ModifyTableTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* 创建内存上下文 */
        context = AllocSetContextCreate(NULL, "test_modifytable", 8192, 8192, 8192);
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

TEST_F(ModifyTableTest, InitAndEnd) {
    /* 创建 ModifyTable 计划节点 */
    ModifyTable plan = {};
    plan.plan.type = T_ModifyTable;
    plan.operation = CMD_INSERT;

    /* 初始化节点 */
    PlanState *ps = ExecInitModifyTable(&plan.plan, estate, 0);
    ASSERT_NE(ps, nullptr);
    EXPECT_EQ(ps->type, T_ModifyTableState);
    EXPECT_NE(ps->ExecProcNode, nullptr);

    /* 清理 */
    ExecEndModifyTable((ModifyTableState *)ps);
}

TEST_F(ModifyTableTest, BasicInit) {
    /* 创建 ModifyTable 计划节点（无子节点） */
    ModifyTable plan = {};
    plan.plan.type = T_ModifyTable;
    plan.operation = CMD_UPDATE;

    /* 初始化节点 */
    PlanState *ps = ExecInitModifyTable(&plan.plan, estate, 0);
    ASSERT_NE(ps, nullptr);
    EXPECT_EQ(ps->type, T_ModifyTableState);
    EXPECT_EQ(ps->plan, &plan.plan);

    ModifyTableState *mts = (ModifyTableState *)ps;
    EXPECT_EQ(mts->mt_operation, CMD_UPDATE);
    EXPECT_FALSE(mts->mt_done);

    /* 清理 */
    ExecEndModifyTable(mts);
}

TEST_F(ModifyTableTest, ExecProcNodeReturnsNull) {
    /* 框架版本：ExecProcNode 应返回 NULL */
    ModifyTable plan = {};
    plan.plan.type = T_ModifyTable;
    plan.operation = CMD_DELETE;

    PlanState *ps = ExecInitModifyTable(&plan.plan, estate, 0);
    ASSERT_NE(ps, nullptr);

    TupleTableSlot *slot = ps->ExecProcNode(ps);
    EXPECT_EQ(slot, nullptr);

    ExecEndModifyTable((ModifyTableState *)ps);
}

TEST_F(ModifyTableTest, EndNull) {
    /* ExecEndModifyTable 应安全处理 NULL */
    ExecEndModifyTable(nullptr);
}

TEST_F(ModifyTableTest, ReScan) {
    /* 创建并初始化节点 */
    ModifyTable plan = {};
    plan.plan.type = T_ModifyTable;
    plan.operation = CMD_INSERT;

    PlanState *ps = ExecInitModifyTable(&plan.plan, estate, 0);
    ASSERT_NE(ps, nullptr);

    ModifyTableState *mts = (ModifyTableState *)ps;
    mts->mt_done = true;

    /* 重置 */
    ExecReScanModifyTable(mts);
    EXPECT_FALSE(mts->mt_done);

    /* 清理 */
    ExecEndModifyTable(mts);
}

TEST_F(ModifyTableTest, NodeRegistration) {
    /* 确保节点已注册 */
    ModifyTable plan = {};
    plan.plan.type = T_ModifyTable;
    plan.operation = CMD_INSERT;

    PlanState *ps = ExecInitModifyTable(&plan.plan, estate, 0);
    ASSERT_NE(ps, nullptr);

    /* 节点应已通过 executor_register_node 注册 */
    EXPECT_NE(ps->ExecProcNode, nullptr);

    ExecEndModifyTable((ModifyTableState *)ps);
}

TEST_F(ModifyTableTest, AllOperationTypes) {
    /* 测试所有操作类型 */
    CmdType ops[] = {CMD_INSERT, CMD_UPDATE, CMD_DELETE};

    for (int i = 0; i < 3; i++) {
        ModifyTable plan = {};
        plan.plan.type = T_ModifyTable;
        plan.operation = ops[i];

        PlanState *ps = ExecInitModifyTable(&plan.plan, estate, 0);
        ASSERT_NE(ps, nullptr);

        ModifyTableState *mts = (ModifyTableState *)ps;
        EXPECT_EQ(mts->mt_operation, ops[i]);

        ExecEndModifyTable(mts);
    }
}
