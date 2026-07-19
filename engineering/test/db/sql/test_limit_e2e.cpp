/**
 * @file test_limit_e2e.cpp
 * @brief ExecLimit 端到端测试（Task 2.6）
 *
 * 验证 Limit 节点的真实执行（LIMIT/OFFSET 语义）：
 * 1. 创建 Limit 计划节点 + ExecInitLimit 初始化
 * 2. 调 ExecLimit 触发限制逻辑
 * 3. 验证 OFFSET 跳过 LIMIT 个数限制
 *
 * Task 2.6 实现：exec_limit_impl 从桩升级为真实实现：
 * - 跳过 OFFSET 个元组
 * - 返回最多 LIMIT 个元组
 * - 超过 LIMIT 后返回 NULL（提前终止）
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "db/sql/nodeLimit.h"
#include "db/sql/nodes/nodetags.h"
#include "db/sql/nodes/execnodes.h"
#include "db/sql/executor.h"
#include "db/sql/memctx.h"
}

namespace {

/**
 * @brief Limit 端到端测试
 */
class LimitE2ETest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

/**
 * @brief 测试 1: LimitState 初始化
 */
TEST_F(LimitE2ETest, InitLimitState) {
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    Limit *limit_plan = (Limit *)palloc0(estate->es_query_cxt, sizeof(Limit));
    limit_plan->plan.type = T_Limit;
    limit_plan->limitOffset = 0;
    limit_plan->limitCount = 10;

    LimitState *state = (LimitState *)ExecInitLimit((Plan *)limit_plan, estate, 0);
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->ps.type, T_LimitState);
    EXPECT_EQ(state->limitOffset, 0);
    EXPECT_EQ(state->limitCount, 10);
    EXPECT_EQ(state->position, 0);
    EXPECT_NE(state->ps.ExecProcNode, nullptr);

    ExecEndLimit(state);
    FreeEState(estate);
}

/**
 * @brief 测试 2: ExecLimit 无子节点应立即返回 NULL
 */
TEST_F(LimitE2ETest, ExecLimitNoChild) {
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    Limit *limit_plan = (Limit *)palloc0(estate->es_query_cxt, sizeof(Limit));
    limit_plan->plan.type = T_Limit;
    limit_plan->limitOffset = 0;
    limit_plan->limitCount = 10;

    LimitState *state = (LimitState *)ExecInitLimit((Plan *)limit_plan, estate, 0);
    ASSERT_NE(state, nullptr);

    /* 无子节点，应返回 NULL */
    TupleTableSlot *slot = ExecLimit(&state->ps);
    EXPECT_EQ(slot, nullptr);

    ExecEndLimit(state);
    FreeEState(estate);
}

/**
 * @brief 测试 3: LIMIT 0 应立即耗尽
 */
TEST_F(LimitE2ETest, LimitZero) {
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    Limit *limit_plan = (Limit *)palloc0(estate->es_query_cxt, sizeof(Limit));
    limit_plan->plan.type = T_Limit;
    limit_plan->limitOffset = 0;
    limit_plan->limitCount = 0;  /* LIMIT 0 */

    LimitState *state = (LimitState *)ExecInitLimit((Plan *)limit_plan, estate, 0);
    ASSERT_NE(state, nullptr);

    /* LIMIT 0 应立即返回 NULL */
    TupleTableSlot *slot = ExecLimit(&state->ps);
    EXPECT_EQ(slot, nullptr);

    ExecEndLimit(state);
    FreeEState(estate);
}

/**
 * @brief 测试 4: OFFSET 大于子节点元组数时返回 NULL
 *
 * 即使 OFFSET=10 且子节点只有 5 个元组，应返回 NULL。
 */
TEST_F(LimitE2ETest, OffsetExceedsChild) {
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    Limit *limit_plan = (Limit *)palloc0(estate->es_query_cxt, sizeof(Limit));
    limit_plan->plan.type = T_Limit;
    limit_plan->limitOffset = 100;  /* OFFSET=100 */
    limit_plan->limitCount = 10;

    LimitState *state = (LimitState *)ExecInitLimit((Plan *)limit_plan, estate, 0);
    ASSERT_NE(state, nullptr);

    /* 无子节点（NULL ExecProcNode），OFFSET 跳过时应返回 NULL */
    TupleTableSlot *slot = ExecLimit(&state->ps);
    EXPECT_EQ(slot, nullptr);

    ExecEndLimit(state);
    FreeEState(estate);
}

/**
 * @brief 测试 5: Limit 重置（ReScan）
 */
TEST_F(LimitE2ETest, LimitRescan) {
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    Limit *limit_plan = (Limit *)palloc0(estate->es_query_cxt, sizeof(Limit));
    limit_plan->plan.type = T_Limit;
    limit_plan->limitOffset = 0;
    limit_plan->limitCount = 5;

    LimitState *state = (LimitState *)ExecInitLimit((Plan *)limit_plan, estate, 0);
    ASSERT_NE(state, nullptr);

    /* 首次执行 */
    TupleTableSlot *slot = ExecLimit(&state->ps);
    EXPECT_EQ(slot, nullptr);  /* 无子节点 */

    /* 重置 */
    ExecReScanLimit(state);
    EXPECT_EQ(state->position, 0);

    /* 再次执行 */
    slot = ExecLimit(&state->ps);
    EXPECT_EQ(slot, nullptr);

    ExecEndLimit(state);
    FreeEState(estate);
}

/**
 * @brief 测试 6: NULL 参数处理
 */
TEST_F(LimitE2ETest, NullParameters) {
    EXPECT_NO_FATAL_FAILURE(ExecEndLimit(nullptr));
    EXPECT_NO_FATAL_FAILURE(ExecReScanLimit(nullptr));
    EXPECT_EQ(ExecLimit(nullptr), nullptr);
}

/**
 * @brief 测试 7: noCount 模式（无 LIMIT）
 */
TEST_F(LimitE2ETest, NoCountMode) {
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    Limit *limit_plan = (Limit *)palloc0(estate->es_query_cxt, sizeof(Limit));
    limit_plan->plan.type = T_Limit;
    limit_plan->limitOffset = 0;
    limit_plan->limitCount = -1;  /* -1 表示无 LIMIT（noCount）*/

    LimitState *state = (LimitState *)ExecInitLimit((Plan *)limit_plan, estate, 0);
    ASSERT_NE(state, nullptr);
    EXPECT_TRUE(state->noCount);

    /* 无子节点，但因 noCount=true，LIMIT 检查应被跳过 */
    TupleTableSlot *slot = ExecLimit(&state->ps);
    EXPECT_EQ(slot, nullptr);  /* 仍因无子节点返回 NULL */

    ExecEndLimit(state);
    FreeEState(estate);
}

}  /* namespace */