/**
 * @file test_nestloop_e2e.cpp
 * @brief ExecNestLoop 端到端测试（Task 2.3）
 *
 * 验证 NestLoop 的真实执行（外表驱动、内表扫描）：
 * 1. 创建左右两个表（catalog_create_table）
 * 2. 用 NestLoop 计划节点 + ExecInitNestLoop 初始化
 * 3. 调 ExecNestLoop 触发嵌套循环
 * 4. 验证执行不崩溃且返回正确
 *
 * Task 2.3 评估：ExecInitNestLoop/ExecNestLoop/ExecEndNestLoop/
 * ExecReScanNestLoop 已有真实实现。ExecQual 当前是桩（无条件返回 true），
 * 因此输出的是笛卡尔积，连接条件过滤需要后续 Task 2.10（ExecProject/
 * ExecFilter）配合实现。本测试验证嵌套循环骨架工作正常。
 */

#include <gtest/gtest.h>
#include <cstring>

#include "db/catalog.h"
#include "db/buf.h"
#include "db/heapam.h"
#include "db/btreeam.h"
#include "db/rel.h"

extern "C" {
#include "db/sql/nodes/nodeSeqscan.h"
#include "db/sql/nodes/nodeNestloop.h"
#include "db/sql/executor.h"
#include "db/sql/memctx.h"
}

namespace {

/**
 * @brief NestLoop 端到端测试环境
 */
class NestLoopE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_EQ(catalog_init(), 0) << "Catalog 初始化失败";
        ASSERT_EQ(buf_init(1024), 0) << "Buffer Pool 初始化失败";
        ASSERT_EQ(heapam_init(), 0) << "Heap AM 初始化失败";
        ASSERT_EQ(btreeam_init(), 0) << "BTree AM 初始化失败";
        ASSERT_EQ(rel_init(), 0) << "Relation 管理器初始化失败";
    }

    void TearDown() override {
        rel_shutdown();
        btreeam_shutdown();
        heapam_shutdown();
        buf_shutdown();
        catalog_shutdown();
    }

    /**
     * @brief 创建测试表
     */
    uint32_t create_test_table(const char *name) {
        column_def_t columns[2];
        memset(columns, 0, sizeof(columns));

        strncpy(columns[0].name, "id", NAMEDATALEN - 1);
        columns[0].type_oid = 23;  /* int4 */

        strncpy(columns[1].name, "value", NAMEDATALEN - 1);
        columns[1].type_oid = 23;  /* int4 */

        return catalog_create_table(name, columns, 2);
    }
};

/**
 * @brief 测试 1: NestLoop 初始化和销毁
 */
TEST_F(NestLoopE2ETest, InitAndEnd) {
    uint32_t oid = create_test_table("nl_init");
    ASSERT_NE(oid, (uint32_t)0);

    NestLoopPlan plan;
    memset(&plan, 0, sizeof(plan));
    plan.type = EXEC_NESTLOOP;

    NestLoopState *state = ExecInitNestLoop(&plan, nullptr, 0);
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->js.ps.type, EXEC_NESTLOOP);

    NestLoopExtState *ext_state = ExecGetNestLoopExtState(state);
    ASSERT_NE(ext_state, nullptr);
    EXPECT_EQ(ext_state->nl_outerTuples, 0);
    EXPECT_EQ(ext_state->nl_innerTuples, 0);
    EXPECT_EQ(ext_state->nl_hits, 0);

    ExecEndNestLoop(state);
}

/**
 * @brief 测试 2: NestLoop 无子节点应返回 NULL
 */
TEST_F(NestLoopE2ETest, ExecWithoutChildren) {
    uint32_t oid = create_test_table("nl_no_children");
    ASSERT_NE(oid, (uint32_t)0);

    NestLoopPlan plan;
    memset(&plan, 0, sizeof(plan));
    plan.type = EXEC_NESTLOOP;

    NestLoopState *state = ExecInitNestLoop(&plan, nullptr, 0);
    ASSERT_NE(state, nullptr);

    /* 无子节点，ExecNestLoop 应返回 NULL */
    TupleTableSlot *slot = ExecNestLoop((PlanState *)state);
    EXPECT_EQ(slot, nullptr);

    NestLoopExtState *ext_state = ExecGetNestLoopExtState(state);
    EXPECT_TRUE(ext_state->nl_exhausted);

    ExecEndNestLoop(state);
}

/**
 * @brief 测试 3: NestLoop 与 SeqScan 子节点配合
 *
 * 验证外表 + 内表 SeqScan 子节点的集成。
 */
TEST_F(NestLoopE2ETest, WithSeqScanChildren) {
    uint32_t outer_oid = create_test_table("nl_outer");
    uint32_t inner_oid = create_test_table("nl_inner");
    ASSERT_NE(outer_oid, (uint32_t)0);
    ASSERT_NE(inner_oid, (uint32_t)0);

    /* 构造 SeqScan 计划 */
    SeqScanPlan outer_plan;
    memset(&outer_plan, 0, sizeof(outer_plan));
    outer_plan.type = EXEC_SEQ_SCAN;
    outer_plan.scanrelid = outer_oid;

    SeqScanPlan inner_plan;
    memset(&inner_plan, 0, sizeof(inner_plan));
    inner_plan.type = EXEC_SEQ_SCAN;
    inner_plan.scanrelid = inner_oid;

    /* 初始化 SeqScan 子节点 */
    SeqScanState *outer_state = ExecInitSeqScan(&outer_plan, nullptr, 0);
    SeqScanState *inner_state = ExecInitSeqScan(&inner_plan, nullptr, 0);
    ASSERT_NE(outer_state, nullptr);
    ASSERT_NE(inner_state, nullptr);

    /* 构造 NestLoop 计划 */
    NestLoopPlan nl_plan;
    memset(&nl_plan, 0, sizeof(nl_plan));
    nl_plan.type = EXEC_NESTLOOP;

    NestLoopState *state = ExecInitNestLoop(&nl_plan, nullptr, 0);
    ASSERT_NE(state, nullptr);

    /* 手动挂上左右 SeqScan 子节点 */
    NestLoopExtState *ext_state = ExecGetNestLoopExtState(state);
    ASSERT_NE(ext_state, nullptr);
    ext_state->nl_OuterTask = (PlanState *)outer_state;
    ext_state->nl_InnerTask = (PlanState *)inner_state;
    state->louter = (PlanState *)outer_state;
    state->rinner = (PlanState *)inner_state;
    ext_state->nl_exhausted = false;

    /* 执行连接：空表返回 NULL */
    TupleTableSlot *slot = ExecNestLoop((PlanState *)state);
    EXPECT_EQ(slot, nullptr);

    ExecEndNestLoop(state);
    ExecEndSeqScan(outer_state);
    ExecEndSeqScan(inner_state);
}

/**
 * @brief 测试 4: NestLoop 重置
 */
TEST_F(NestLoopE2ETest, ReScan) {
    uint32_t oid = create_test_table("nl_rescan");
    ASSERT_NE(oid, (uint32_t)0);

    NestLoopPlan plan;
    memset(&plan, 0, sizeof(plan));
    plan.type = EXEC_NESTLOOP;

    NestLoopState *state = ExecInitNestLoop(&plan, nullptr, 0);
    ASSERT_NE(state, nullptr);

    /* 首次执行（无子节点） */
    TupleTableSlot *slot = ExecNestLoop((PlanState *)state);
    EXPECT_EQ(slot, nullptr);

    /* 重置 */
    ExecReScanNestLoop(state);

    NestLoopExtState *ext_state = ExecGetNestLoopExtState(state);
    EXPECT_FALSE(ext_state->nl_exhausted);
    EXPECT_EQ(ext_state->nl_outerTuples, 0);
    EXPECT_EQ(ext_state->nl_innerTuples, 0);
    EXPECT_EQ(ext_state->nl_hits, 0);

    /* 再次执行 */
    slot = ExecNestLoop((PlanState *)state);
    EXPECT_EQ(slot, nullptr);

    ExecEndNestLoop(state);
}

/**
 * @brief 测试 5: NULL 参数处理
 */
TEST_F(NestLoopE2ETest, NullParameters) {
    /* ExecEndNestLoop 应能处理 NULL */
    EXPECT_NO_FATAL_FAILURE(ExecEndNestLoop(nullptr));

    /* ExecReScanNestLoop 应能处理 NULL */
    EXPECT_NO_FATAL_FAILURE(ExecReScanNestLoop(nullptr));

    /* ExecNestLoop 应能处理 NULL */
    EXPECT_EQ(ExecNestLoop(nullptr), nullptr);

    /* ExecGetNestLoopExtState 应能处理 NULL */
    EXPECT_EQ(ExecGetNestLoopExtState(nullptr), nullptr);
}

/**
 * @brief 测试 6: make_nestloop_plan 工厂
 */
TEST_F(NestLoopE2ETest, MakePlan) {
    NestLoopPlan *plan = make_nestloop_plan(nullptr);
    ASSERT_NE(plan, nullptr);
    EXPECT_EQ(plan->type, EXEC_NESTLOOP);
    free(plan);
}

}  /* namespace */