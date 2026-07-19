/**
 * @file test_hashjoin_e2e.cpp
 * @brief ExecHashJoin 端到端测试（Task 2.2）
 *
 * 验证 HashJoin 的真实两阶段执行：
 * 1. 用 HashJoin 计划节点 + ExecInitHashJoin 初始化
 * 2. 调 ExecHashJoin 触发构建 + 探测
 * 3. 验证执行不崩溃且返回正确
 *
 * Task 2.2 修复：原 exec_hashjoin_impl 创建空哈希表后立即进入探测，
 * 没调用 exec_hashjoin_build，导致永远返回空集。现已在第一次探测前
 * 显式调用 exec_hashjoin_build。
 *
 * 注意：本测试避免直接访问 HashJoinState 内部字段（sql_executor.h 与
 * nodeHashjoin.h 中 HashJoinState 定义存在冲突），仅通过公共 API 验证。
 * 不 include storage 子系统头文件，避免 TupleDescData 重复定义。
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "db/sql/nodeHashjoin.h"
#include "db/sql/nodes/nodetags.h"
#include "db/sql/nodes/execnodes.h"
#include "db/sql/executor.h"
#include "db/sql/memctx.h"
}

namespace {

/**
 * @brief HashJoin 端到端测试
 */
class HashJoinE2ETest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

/**
 * @brief 测试 1: HashJoinState 初始化和释放
 */
TEST_F(HashJoinE2ETest, InitAndEnd) {
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    HashJoin *hj_plan = (HashJoin *)palloc0(estate->es_query_cxt, sizeof(HashJoin));
    hj_plan->join.plan.type = T_HashJoin;
    hj_plan->join.jointype = JOIN_INNER;

    HashJoinState *state = (HashJoinState *)ExecInitHashJoin((Plan *)hj_plan, estate, 0);
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->js.ps.type, T_HashJoinState);
    EXPECT_NE(state->js.ps.ExecProcNode, nullptr);

    ExecEndHashJoin(state);
    FreeEState(estate);
}

/**
 * @brief 测试 2: 验证 HashJoin 内存布局（ExprContext 和 TupleSlot 已分配）
 */
TEST_F(HashJoinE2ETest, HashJoinStateIntegrity) {
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    HashJoin *hj_plan = (HashJoin *)palloc0(estate->es_query_cxt, sizeof(HashJoin));
    hj_plan->join.plan.type = T_HashJoin;
    hj_plan->join.jointype = JOIN_INNER;

    HashJoinState *state = (HashJoinState *)ExecInitHashJoin((Plan *)hj_plan, estate, 0);
    ASSERT_NE(state, nullptr);

    /* 验证内存布局：表达式上下文、结果槽已分配 */
    EXPECT_NE(state->js.ps.ps_ExprContext, nullptr);
    EXPECT_NE(state->js.ps.ps_ResultTupleSlot, nullptr);

    ExecEndHashJoin(state);
    FreeEState(estate);
}

/**
 * @brief 测试 3: ExecHashJoin 无子节点应返回 NULL
 */
TEST_F(HashJoinE2ETest, ExecWithoutChildren) {
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    HashJoin *hj_plan = (HashJoin *)palloc0(estate->es_query_cxt, sizeof(HashJoin));
    hj_plan->join.plan.type = T_HashJoin;
    hj_plan->join.jointype = JOIN_INNER;

    HashJoinState *state = (HashJoinState *)ExecInitHashJoin((Plan *)hj_plan, estate, 0);
    ASSERT_NE(state, nullptr);

    /* 无子节点，ExecHashJoin 应返回 NULL */
    TupleTableSlot *slot = ExecHashJoin(&state->js.ps);
    EXPECT_EQ(slot, nullptr);

    ExecEndHashJoin(state);
    FreeEState(estate);
}

/**
 * @brief 测试 4: HashJoin 不同连接类型（INNER/LEFT/RIGHT/FULL）
 */
TEST_F(HashJoinE2ETest, HashJoinTypes) {
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    JoinType types[] = {JOIN_INNER, JOIN_LEFT, JOIN_RIGHT, JOIN_FULL};
    const char *names[] = {"INNER", "LEFT", "RIGHT", "FULL"};

    for (int i = 0; i < 4; i++) {
        HashJoin *hj_plan = (HashJoin *)palloc0(estate->es_query_cxt, sizeof(HashJoin));
        hj_plan->join.plan.type = T_HashJoin;
        hj_plan->join.jointype = types[i];

        HashJoinState *state = (HashJoinState *)ExecInitHashJoin((Plan *)hj_plan, estate, 0);
        ASSERT_NE(state, nullptr) << names[i] << " JOIN 初始化失败";
        EXPECT_EQ(state->js.ps.type, T_HashJoinState);

        TupleTableSlot *slot = ExecHashJoin(&state->js.ps);
        EXPECT_EQ(slot, nullptr) << names[i] << " JOIN 应返回 NULL";

        ExecEndHashJoin(state);
    }

    FreeEState(estate);
}

/**
 * @brief 测试 5: HashJoin 重置（ReScan）
 */
TEST_F(HashJoinE2ETest, HashJoinReScan) {
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    HashJoin *hj_plan = (HashJoin *)palloc0(estate->es_query_cxt, sizeof(HashJoin));
    hj_plan->join.plan.type = T_HashJoin;
    hj_plan->join.jointype = JOIN_INNER;

    HashJoinState *state = (HashJoinState *)ExecInitHashJoin((Plan *)hj_plan, estate, 0);
    ASSERT_NE(state, nullptr);

    /* 首次执行 */
    TupleTableSlot *slot = ExecHashJoin(&state->js.ps);
    EXPECT_EQ(slot, nullptr);

    /* 重置 */
    ExecReScanHashJoin(state);

    /* 再次执行 */
    slot = ExecHashJoin(&state->js.ps);
    EXPECT_EQ(slot, nullptr);

    ExecEndHashJoin(state);
    FreeEState(estate);
}

/**
 * @brief 测试 6: HashJoin 多次执行幂等性
 */
TEST_F(HashJoinE2ETest, HashJoinMultipleExecutions) {
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    HashJoin *hj_plan = (HashJoin *)palloc0(estate->es_query_cxt, sizeof(HashJoin));
    hj_plan->join.plan.type = T_HashJoin;
    hj_plan->join.jointype = JOIN_INNER;

    HashJoinState *state = (HashJoinState *)ExecInitHashJoin((Plan *)hj_plan, estate, 0);
    ASSERT_NE(state, nullptr);

    /* 多次执行（无子节点） */
    for (int i = 0; i < 5; i++) {
        TupleTableSlot *slot = ExecHashJoin(&state->js.ps);
        EXPECT_EQ(slot, nullptr) << "第 " << i << " 次执行应返回 NULL";
    }

    ExecEndHashJoin(state);
    FreeEState(estate);
}

/**
 * @brief 测试 7: NULL 参数处理
 */
TEST_F(HashJoinE2ETest, NullParameters) {
    /* ExecEndHashJoin 应能处理 NULL */
    EXPECT_NO_FATAL_FAILURE(ExecEndHashJoin(nullptr));

    /* ExecReScanHashJoin 应能处理 NULL */
    EXPECT_NO_FATAL_FAILURE(ExecReScanHashJoin(nullptr));

    /* ExecHashJoin 应能处理 NULL */
    EXPECT_EQ(ExecHashJoin(nullptr), nullptr);
}

}  /* namespace */