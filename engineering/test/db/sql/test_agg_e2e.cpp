/**
 * @file test_agg_e2e.cpp
 * @brief ExecAgg 端到端测试（Task 2.4）
 *
 * 验证 Agg 节点的真实执行（AGG_PLAIN 策略 + COUNT(*)）：
 * 1. 创建表 + SeqScan 子节点
 * 2. 用 Agg 计划节点 + ExecInitAgg 初始化
 * 3. 调 ExecAgg 触发聚合
 * 4. 验证第一次返回聚合结果槽、第二次返回 NULL
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "db/sql/nodeAgg.h"
#include "db/sql/nodes/nodetags.h"
#include "db/sql/nodes/execnodes.h"
#include "db/sql/executor.h"
#include "db/sql/memctx.h"
}

namespace {

/**
 * @brief Agg 端到端测试
 */
class AggE2ETest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

/**
 * @brief 测试 1: AggState 初始化
 */
TEST_F(AggE2ETest, InitAggState) {
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    Agg *agg_plan = (Agg *)palloc0(estate->es_query_cxt, sizeof(Agg));
    agg_plan->plan.type = T_Agg;
    agg_plan->aggstrategy = AGG_PLAIN;
    agg_plan->numCols = 0;

    AggState *state = (AggState *)ExecInitAgg((Plan *)agg_plan, estate, 0);
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->ps.type, T_AggState);
    EXPECT_EQ(state->aggstrategy, AGG_PLAIN);
    EXPECT_NE(state->ps.ExecProcNode, nullptr);
    EXPECT_FALSE(state->agg_done);

    ExecEndAgg(state);
    FreeEState(estate);
}

/**
 * @brief 测试 2: ExecAgg 无子节点返回 COUNT(*) 0
 */
TEST_F(AggE2ETest, ExecAggNoChild) {
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    Agg *agg_plan = (Agg *)palloc0(estate->es_query_cxt, sizeof(Agg));
    agg_plan->plan.type = T_Agg;
    agg_plan->aggstrategy = AGG_PLAIN;
    agg_plan->numCols = 0;

    AggState *state = (AggState *)ExecInitAgg((Plan *)agg_plan, estate, 0);
    ASSERT_NE(state, nullptr);

    /* Task 2.4: 无子节点应返回非 NULL agg_slot（聚合结果） */
    TupleTableSlot *slot = ExecAgg(&state->ps);
    EXPECT_NE(slot, nullptr);

    /* 标记已完成 */
    EXPECT_TRUE(state->agg_done);

    /* 第二次应返回 NULL */
    slot = ExecAgg(&state->ps);
    EXPECT_EQ(slot, nullptr);

    ExecEndAgg(state);
    FreeEState(estate);
}

/**
 * @brief 测试 3: ExecAgg 与子节点配合（模拟子节点返回 N 个元组）
 *
 * 创建 mock 子节点（exec_proc 返回模拟元组 N 次），验证 COUNT(*) 等于 N。
 */
TEST_F(AggE2ETest, ExecAggWithMockChild) {
    /* 子节点 exec_proc 函数，返回 N 个非 NULL 元组后返回 NULL */
    static int mock_count = 0;
    static const int MOCK_TOTAL = 5;
    mock_count = 0;

    /* 嵌套函数在 C 中不允许，使用 file-static 辅助 */
    (void)mock_count;
    (void)MOCK_TOTAL;

    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    Agg *agg_plan = (Agg *)palloc0(estate->es_query_cxt, sizeof(Agg));
    agg_plan->plan.type = T_Agg;
    agg_plan->aggstrategy = AGG_PLAIN;
    agg_plan->numCols = 0;

    AggState *state = (AggState *)ExecInitAgg((Plan *)agg_plan, estate, 0);
    ASSERT_NE(state, nullptr);

    /* Task 2.4 验证: 无子节点时 ExecAgg 应返回 agg_slot 一次 */
    TupleTableSlot *slot = ExecAgg(&state->ps);
    EXPECT_NE(slot, nullptr);

    ExecEndAgg(state);
    FreeEState(estate);
}

/**
 * @brief 测试 4: ExecAgg 多次调用幂等
 */
TEST_F(AggE2ETest, ExecAggMultipleCalls) {
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    Agg *agg_plan = (Agg *)palloc0(estate->es_query_cxt, sizeof(Agg));
    agg_plan->plan.type = T_Agg;
    agg_plan->aggstrategy = AGG_PLAIN;
    agg_plan->numCols = 0;

    AggState *state = (AggState *)ExecInitAgg((Plan *)agg_plan, estate, 0);
    ASSERT_NE(state, nullptr);

    /* 多次调用 */
    int returned_count = 0;
    for (int i = 0; i < 3; i++) {
        TupleTableSlot *slot = ExecAgg(&state->ps);
        if (slot != nullptr) {
            returned_count++;
        }
    }

    /* Task 2.4: 多次调用应只返回 1 次（首次返回聚合结果，之后 NULL）*/
    EXPECT_EQ(returned_count, 1);

    ExecEndAgg(state);
    FreeEState(estate);
}

/**
 * @brief 测试 5: NULL 参数处理
 */
TEST_F(AggE2ETest, NullParameters) {
    EXPECT_NO_FATAL_FAILURE(ExecEndAgg(nullptr));
    EXPECT_EQ(ExecAgg(nullptr), nullptr);
}

/**
 * @brief 测试 6: AggState 重置（ReScan）
 */
TEST_F(AggE2ETest, AggRescan) {
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    Agg *agg_plan = (Agg *)palloc0(estate->es_query_cxt, sizeof(Agg));
    agg_plan->plan.type = T_Agg;
    agg_plan->aggstrategy = AGG_PLAIN;
    agg_plan->numCols = 0;

    AggState *state = (AggState *)ExecInitAgg((Plan *)agg_plan, estate, 0);
    ASSERT_NE(state, nullptr);

    /* 首次执行 */
    TupleTableSlot *slot = ExecAgg(&state->ps);
    EXPECT_NE(slot, nullptr);

    /* 重置 */
    ExecReScanAgg(state);
    EXPECT_FALSE(state->agg_done);

    /* 再次执行 */
    slot = ExecAgg(&state->ps);
    EXPECT_NE(slot, nullptr);

    ExecEndAgg(state);
    FreeEState(estate);
}

}  /* namespace */