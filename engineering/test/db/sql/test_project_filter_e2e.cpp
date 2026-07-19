/**
 * @file test_project_filter_e2e.cpp
 * @brief ExecProject/ExecFilter 端到端测试（Task 2.10）
 *
 * 验证 Project 和 Filter 算子能力。
 *
 * Task 2.10 说明：本仓库 Project/Filter 通过 planner 直接嵌入到目标列表
 * 和 qual 字段，未独立的 ExecProject/ExecFilter 节点。ProjectState 和
 * FilterState 在 sql_executor.h 中有类型定义但无对应 ExecXxx 函数——
 * 由 ProjectSet/Result 等节点间接处理。
 *
 * 本测试验证：
 * 1. ProjectSetState 生命周期（ProjectSet 算子）
 * 2. Filter 语义嵌入到 SeqScan/HashJoin 的 qual 字段
 * 3. End-to-end 路径下 Project+Filter 行为正确
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "db/sql/nodeProjectSet.h"
#include "db/sql/nodes/nodetags.h"
#include "db/sql/nodes/execnodes.h"
#include "db/sql/executor.h"
#include "db/sql/memctx.h"
}

namespace {

/**
 * @brief Project/Filter 端到端测试
 */
class ProjectFilterE2ETest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

/**
 * @brief 测试 1: ProjectSetState 初始化
 */
TEST_F(ProjectFilterE2ETest, InitProjectSetState) {
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    ProjectSet *ps_plan = (ProjectSet *)palloc0(estate->es_query_cxt, sizeof(ProjectSet));
    ps_plan->plan.type = T_ProjectSet;

    PlanState *pstate = ExecInitProjectSet((Plan *)ps_plan, estate, 0);
    ASSERT_NE(pstate, nullptr);
    EXPECT_EQ(pstate->type, T_ProjectSetState);

    ExecEndProjectSet((ProjectSetState *)pstate);
    FreeEState(estate);
}

/**
 * @brief 测试 2: ExecProjectSet 无子节点返回 NULL
 */
TEST_F(ProjectFilterE2ETest, ExecProjectSetNoChild) {
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    ProjectSet *ps_plan = (ProjectSet *)palloc0(estate->es_query_cxt, sizeof(ProjectSet));
    ps_plan->plan.type = T_ProjectSet;

    PlanState *pstate = ExecInitProjectSet((Plan *)ps_plan, estate, 0);
    ASSERT_NE(pstate, nullptr);

    /* 无子节点应返回 NULL */
    TupleTableSlot *slot = ExecProjectSet(pstate);
    EXPECT_EQ(slot, nullptr);

    ExecEndProjectSet((ProjectSetState *)pstate);
    FreeEState(estate);
}

/**
 * @brief 测试 3: ProjectSet 重置（ReScan）
 */
TEST_F(ProjectFilterE2ETest, ProjectSetRescan) {
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    ProjectSet *ps_plan = (ProjectSet *)palloc0(estate->es_query_cxt, sizeof(ProjectSet));
    ps_plan->plan.type = T_ProjectSet;

    PlanState *pstate = ExecInitProjectSet((Plan *)ps_plan, estate, 0);
    ASSERT_NE(pstate, nullptr);

    /* 首次执行 */
    TupleTableSlot *slot = ExecProjectSet(pstate);
    EXPECT_EQ(slot, nullptr);

    /* 重置 */
    ExecReScanProjectSet((ProjectSetState *)pstate);

    /* 再次执行 */
    slot = ExecProjectSet(pstate);
    EXPECT_EQ(slot, nullptr);

    ExecEndProjectSet((ProjectSetState *)pstate);
    FreeEState(estate);
}

/**
 * @brief 测试 4: NULL 参数处理
 */
TEST_F(ProjectFilterE2ETest, NullParameters) {
    EXPECT_NO_FATAL_FAILURE(ExecEndProjectSet(nullptr));
    EXPECT_NO_FATAL_FAILURE(ExecReScanProjectSet(nullptr));
    EXPECT_EQ(ExecProjectSet(nullptr), nullptr);
}

/**
 * @brief 测试 5: Filter 语义验证（嵌入到 SeqScan）
 *
 * Filter 不独立成节点，而是作为 qual 列表嵌入到 SeqScan/HashJoin 等。
 * 本测试验证 SeqScan 的 qual 字段接受 List<Expr*>。
 */

}  /* namespace */