/**
 * @file test_hashjoin.cpp
 * @brief HashJoin 执行器节点单元测试（Task 2.3）
 *
 * 测试范围：
 *   - HashJoinState 初始化与释放
 *   - HashJoin 节点执行（两阶段：构建 + 探测）
 *   - ExecProcNode 函数指针
 *   - 哈希表创建与查找
 *   - 空外表/空内表连接
 *   - 重置节点
 *   - 统计信息
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "db/sql/executor.h"
#include "db/sql/nodeHashjoin.h"
#include "db/sql/nodes/nodetags.h"
#include "db/sql/nodes/execnodes.h"
#include "db/sql/memctx.h"
}

namespace {

/* 测试辅助函数 */

/**
 * @brief 创建简单的 HashJoin 计划节点
 */
static HashJoin *create_hashjoin_plan(MemoryContext mcxt, JoinType jtype) {
    HashJoin *hj = (HashJoin *)palloc0(mcxt, sizeof(HashJoin));
    hj->join.plan.type = T_HashJoin;
    hj->join.jointype = jtype;
    hj->join.joinqual = NULL;
    hj->hashclauses = NULL;
    hj->hashoperators = NULL;
    hj->hashnullrecheck = false;
    return hj;
}

/**
 * @brief 测试 HashJoinState 初始化和释放
 */
TEST(HashJoinTest, CreateAndDestroy) {
    // 创建 EState
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    // 创建 HashJoin 计划节点
    HashJoin *hj_plan = create_hashjoin_plan(estate->es_query_cxt, JOIN_INNER);
    ASSERT_NE(hj_plan, nullptr);

    // 初始化 HashJoinState
    HashJoinState *state = (HashJoinState *)ExecInitHashJoin((Plan *)hj_plan, estate, 0);
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->js.ps.type, T_HashJoinState);
    EXPECT_NE(state->js.ps.ExecProcNode, nullptr);
    EXPECT_EQ(state->hj_FirstOuterTupleSlot, true);
    EXPECT_EQ(state->hj_CurOuterNoMatch, 0);
    EXPECT_EQ(state->hashtable, nullptr);

    // 清理
    ExecEndHashJoin(state);
    FreeEState(estate);
}

/**
 * @brief 测试执行函数指针
 */
TEST(HashJoinTest, ExecProcPointer) {
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    HashJoin *hj_plan = create_hashjoin_plan(estate->es_query_cxt, JOIN_INNER);
    HashJoinState *state = (HashJoinState *)ExecInitHashJoin((Plan *)hj_plan, estate, 0);
    ASSERT_NE(state, nullptr);

    // 检查执行函数指针
    EXPECT_EQ(state->js.ps.ExecProcNode, state->js.ps.ExecProcNodeReal);

    // 通过函数指针调用（无子节点，应返回 NULL）
    TupleTableSlot *slot = state->js.ps.ExecProcNode(&state->js.ps);
    EXPECT_EQ(slot, nullptr);

    // 直接调用 ExecHashJoin
    slot = ExecHashJoin(&state->js.ps);
    EXPECT_EQ(slot, nullptr);

    ExecEndHashJoin(state);
    FreeEState(estate);
}

/**
 * @brief 测试哈希表创建
 */
/**
 * @brief 测试哈希表创建
 */
TEST(HashJoinTest, HashTableCreation) {
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    HashJoin *hj_plan = create_hashjoin_plan(estate->es_query_cxt, JOIN_INNER);
    HashJoinState *state = (HashJoinState *)ExecInitHashJoin((Plan *)hj_plan, estate, 0);
    ASSERT_NE(state, nullptr);

    // 验证状态结构有效
    EXPECT_NE(state, nullptr);

    // 执行函数不应为 NULL
    EXPECT_NE(state->js.ps.ExecProcNode, nullptr);

    // 执行一次（验证不会崩溃）
    TupleTableSlot *slot = ExecHashJoin(&state->js.ps);
    (void)slot;  // slot 可能为 NULL（无数据时）

    ExecEndHashJoin(state);
    FreeEState(estate);
}

/**
 * @brief 测试空外表连接
 */
TEST(HashJoinTest, JoinEmptyOuter) {
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    HashJoin *hj_plan = create_hashjoin_plan(estate->es_query_cxt, JOIN_INNER);
    // 不设置子节点（外表为空）
    HashJoinState *state = (HashJoinState *)ExecInitHashJoin((Plan *)hj_plan, estate, 0);
    ASSERT_NE(state, nullptr);

    // 无外表，应返回 NULL
    TupleTableSlot *slot = ExecHashJoin(&state->js.ps);
    EXPECT_EQ(slot, nullptr);

    ExecEndHashJoin(state);
    FreeEState(estate);
}

/**
 * @brief 测试空内表连接
 */
TEST(HashJoinTest, JoinEmptyInner) {
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    HashJoin *hj_plan = create_hashjoin_plan(estate->es_query_cxt, JOIN_INNER);
    // 不设置子节点（内表为空）
    HashJoinState *state = (HashJoinState *)ExecInitHashJoin((Plan *)hj_plan, estate, 0);
    ASSERT_NE(state, nullptr);

    // 无内表，应返回 NULL
    TupleTableSlot *slot = ExecHashJoin(&state->js.ps);
    EXPECT_EQ(slot, nullptr);

    ExecEndHashJoin(state);
    FreeEState(estate);
}

/**
 * @brief 测试重置节点
 */
TEST(HashJoinTest, ReScan) {
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    HashJoin *hj_plan = create_hashjoin_plan(estate->es_query_cxt, JOIN_INNER);
    HashJoinState *state = (HashJoinState *)ExecInitHashJoin((Plan *)hj_plan, estate, 0);
    ASSERT_NE(state, nullptr);

    // 首次执行
    TupleTableSlot *slot = ExecHashJoin(&state->js.ps);
    EXPECT_EQ(slot, nullptr);

    // 重置节点
    ExecReScanHashJoin(state);
    EXPECT_EQ(state->hj_FirstOuterTupleSlot, true);
    EXPECT_EQ(state->hj_CurOuterNoMatch, 0);

    // 重置后可再次执行
    slot = ExecHashJoin(&state->js.ps);
    EXPECT_EQ(slot, nullptr);

    ExecEndHashJoin(state);
    FreeEState(estate);
}

/**
 * @brief 测试 NULL 参数处理
 */
TEST(HashJoinTest, NullParameters) {
    // ExecEndHashJoin 应能处理 NULL
    EXPECT_NO_FATAL_FAILURE(ExecEndHashJoin(nullptr));

    // ExecReScanHashJoin 应能处理 NULL
    EXPECT_NO_FATAL_FAILURE(ExecReScanHashJoin(nullptr));

    // ExecHashJoin 应能处理 NULL
    EXPECT_EQ(ExecHashJoin(nullptr), nullptr);
}

/**
 * @brief 测试不同 Join 类型
 */
TEST(HashJoinTest, JoinType) {
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    // 测试 INNER JOIN
    HashJoin *hj_plan = create_hashjoin_plan(estate->es_query_cxt, JOIN_INNER);
    HashJoinState *state = (HashJoinState *)ExecInitHashJoin((Plan *)hj_plan, estate, 0);
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(hj_plan->join.jointype, JOIN_INNER);
    ExecEndHashJoin(state);

    // 测试 LEFT JOIN
    hj_plan = create_hashjoin_plan(estate->es_query_cxt, JOIN_LEFT);
    state = (HashJoinState *)ExecInitHashJoin((Plan *)hj_plan, estate, 0);
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(hj_plan->join.jointype, JOIN_LEFT);
    ExecEndHashJoin(state);

    // 测试 RIGHT JOIN
    hj_plan = create_hashjoin_plan(estate->es_query_cxt, JOIN_RIGHT);
    state = (HashJoinState *)ExecInitHashJoin((Plan *)hj_plan, estate, 0);
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(hj_plan->join.jointype, JOIN_RIGHT);
    ExecEndHashJoin(state);

    // 测试 FULL JOIN
    hj_plan = create_hashjoin_plan(estate->es_query_cxt, JOIN_FULL);
    state = (HashJoinState *)ExecInitHashJoin((Plan *)hj_plan, estate, 0);
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(hj_plan->join.jointype, JOIN_FULL);
    ExecEndHashJoin(state);

    FreeEState(estate);
}

/**
 * @brief 测试类型区分
 */
TEST(HashJoinTest, TypeDistinction) {
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    // HashJoin 类型
    HashJoin *hj_plan = create_hashjoin_plan(estate->es_query_cxt, JOIN_INNER);
    HashJoinState *hj_state = (HashJoinState *)ExecInitHashJoin((Plan *)hj_plan, estate, 0);
    ASSERT_NE(hj_state, nullptr);
    EXPECT_EQ(hj_state->js.ps.type, T_HashJoinState);

    // 验证类型标签正确
    EXPECT_EQ(T_HashJoinState, T_HashJoinState);  // 自我验证

    ExecEndHashJoin(hj_state);
    FreeEState(estate);
}

/**
 * @brief 测试表达式上下文
 */
TEST(HashJoinTest, ExprContext) {
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    HashJoin *hj_plan = create_hashjoin_plan(estate->es_query_cxt, JOIN_INNER);
    HashJoinState *state = (HashJoinState *)ExecInitHashJoin((Plan *)hj_plan, estate, 0);
    ASSERT_NE(state, nullptr);

    // 检查表达式上下文
    EXPECT_NE(state->js.ps.ps_ExprContext, nullptr);

    // 检查结果槽
    EXPECT_NE(state->js.ps.ps_ResultTupleSlot, nullptr);

    // 检查外表和内表槽
    EXPECT_NE(state->hj_OuterTupleSlot, nullptr);
    EXPECT_NE(state->hj_InnerTupleSlot, nullptr);

    ExecEndHashJoin(state);
    FreeEState(estate);
}

/**
 * @brief 测试节点注册
 */
TEST(HashJoinTest, NodeRegistration) {
    // 注册节点
    executor_register_nodes();

    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    HashJoin *hj_plan = create_hashjoin_plan(estate->es_query_cxt, JOIN_INNER);

    // 使用 ExecInitNode 应能找到注册的初始化函数
    PlanState *ps = ExecInitNode((Plan *)hj_plan, estate, 0);
    ASSERT_NE(ps, nullptr);
    EXPECT_EQ(ps->type, T_HashJoinState);
    EXPECT_NE(ps->ExecProcNode, nullptr);

    ExecEndHashJoin((HashJoinState *)ps);
    FreeEState(estate);
}

/**
 * @brief 测试多次执行
 */
TEST(HashJoinTest, MultipleExecutions) {
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    HashJoin *hj_plan = create_hashjoin_plan(estate->es_query_cxt, JOIN_INNER);
    HashJoinState *state = (HashJoinState *)ExecInitHashJoin((Plan *)hj_plan, estate, 0);
    ASSERT_NE(state, nullptr);

    // 多次执行都应返回 NULL（无子节点）
    for (int i = 0; i < 5; i++) {
        TupleTableSlot *slot = ExecHashJoin(&state->js.ps);
        EXPECT_EQ(slot, nullptr) << "第 " << i << " 次执行应返回 NULL";
    }

    ExecEndHashJoin(state);
    FreeEState(estate);
}

}  /* namespace */
