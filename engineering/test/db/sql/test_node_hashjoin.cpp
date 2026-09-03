/**
 * @file test_node_hashjoin.cpp
 * @brief HashJoin 执行器节点单元测试（Task 2.3）
 *
 * 测试范围：
 *   - HashJoinState 初始化与释放
 *   - HashState 辅助节点初始化与释放
 *   - 子节点树构建（lefttree/righttree）
 *   - ExecProcNode 返回 NULL（框架实现）
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "db/sql/executor.h"
#include "db/sql/nodeHashjoin.h"
#include "db/sql/nodeHash.h"
#include "db/sql/nodeSeqscan.h"
#include "db/sql/nodes/nodetags.h"
#include "db/sql/nodes/execnodes.h"
#include "db/sql/memctx.h"
}

namespace {

/**
 * @brief 测试 HashState 初始化和释放
 */
TEST(HashNodeTest, InitAndEnd) {
    // 创建 EState
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    // 在查询上下文中分配 Hash 计划节点
    Hash *hash_plan = (Hash *)palloc0(estate->es_query_cxt, sizeof(Hash));
    hash_plan->plan.type = T_Hash;
    hash_plan->hashoperator = 0;
    hash_plan->hashkeys = nullptr;

    // 初始化 HashState
    HashState *state = (HashState *)ExecInitHash((Plan *)hash_plan, estate, 0);
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->ps.type, T_HashState);
    EXPECT_EQ(state->hashtable, nullptr);  // 框架版本：哈希表为 NULL
    EXPECT_EQ(state->hashsize, 1024);       // 默认大小

    // 清理
    ExecEndHash(state);
    FreeEState(estate);
}

/**
 * @brief 测试 ExecEndHash 处理 NULL
 */
TEST(HashNodeTest, EndNull) {
    // 不应崩溃
    ExecEndHash(nullptr);
}

/**
 * @brief 测试 HashJoinState 初始化和释放
 */
TEST(HashJoinNodeTest, InitAndEnd) {
    // 创建 EState
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    // 在查询上下文中分配 HashJoin 计划节点
    HashJoin *hj_plan = (HashJoin *)palloc0(estate->es_query_cxt, sizeof(HashJoin));
    hj_plan->join.plan.type = T_HashJoin;
    hj_plan->join.jointype = JOIN_INNER;
    hj_plan->join.joinqual = nullptr;
    hj_plan->hashclauses = nullptr;

    // 初始化 HashJoinState
    HashJoinState *state = (HashJoinState *)ExecInitHashJoin((Plan *)hj_plan, estate, 0);
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->js.ps.type, T_HashJoinState);
    EXPECT_NE(state->js.ps.ExecProcNode, nullptr);
    EXPECT_NE(state->hj_OuterTupleSlot, nullptr);
    EXPECT_NE(state->hj_InnerTupleSlot, nullptr);

    // 清理
    ExecEndHashJoin(state);
    FreeEState(estate);
}

/**
 * @brief 测试 HashJoin 带子节点的初始化（简化版）
 */
TEST(HashJoinNodeTest, InitWithChildNodes) {
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    // 创建 HashJoin 计划节点（不设置子节点）
    HashJoin *hj_plan = (HashJoin *)palloc0(estate->es_query_cxt, sizeof(HashJoin));
    hj_plan->join.plan.type = T_HashJoin;
    hj_plan->join.jointype = JOIN_INNER;
    hj_plan->hashclauses = nullptr;

    // 左子树和右子树保持为 NULL（框架版本允许）
    hj_plan->join.plan.lefttree = nullptr;
    hj_plan->join.plan.righttree = nullptr;

    // 初始化 HashJoinState
    HashJoinState *state = (HashJoinState *)ExecInitHashJoin((Plan *)hj_plan, estate, 0);
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->js.ps.type, T_HashJoinState);

    // 验证子节点指针为 NULL
    EXPECT_EQ(state->js.ps.lefttree, nullptr);
    EXPECT_EQ(state->js.ps.righttree, nullptr);
    EXPECT_EQ(state->hashtable, nullptr);

    // 清理
    ExecEndHashJoin(state);
    FreeEState(estate);
}

/**
 * @brief 测试 ExecProcNode 返回 NULL（框架实现）
 */
TEST(HashJoinNodeTest, ExecProcNodeReturnsNull) {
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    HashJoin *hj_plan = (HashJoin *)palloc0(estate->es_query_cxt, sizeof(HashJoin));
    hj_plan->join.plan.type = T_HashJoin;
    hj_plan->join.jointype = JOIN_INNER;
    hj_plan->hashclauses = nullptr;

    HashJoinState *state = (HashJoinState *)ExecInitHashJoin((Plan *)hj_plan, estate, 0);
    ASSERT_NE(state, nullptr);

    // 框架实现应返回 NULL（表示需要后续完善）
    TupleTableSlot *slot = ExecProcNode(&state->js.ps);
    EXPECT_EQ(slot, nullptr);

    // 清理
    ExecEndHashJoin(state);
    FreeEState(estate);
}

/**
 * @brief 测试 ExecEndHashJoin 处理 NULL
 */
TEST(HashJoinNodeTest, EndNull) {
    // 不应崩溃
    ExecEndHashJoin(nullptr);
}

/**
 * @brief 测试 ExecReScanHashJoin 重置节点
 */
TEST(HashJoinNodeTest, ReScan) {
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    HashJoin *hj_plan = (HashJoin *)palloc0(estate->es_query_cxt, sizeof(HashJoin));
    hj_plan->join.plan.type = T_HashJoin;
    hj_plan->join.jointype = JOIN_INNER;
    hj_plan->hashclauses = nullptr;

    HashJoinState *state = (HashJoinState *)ExecInitHashJoin((Plan *)hj_plan, estate, 0);
    ASSERT_NE(state, nullptr);

    // 重置节点（不应崩溃）
    ExecReScanHashJoin(state);

    // 清理
    ExecEndHashJoin(state);
    FreeEState(estate);
}

/**
 * @brief 测试 HashJoin 节点注册到执行器
 */
TEST(HashJoinNodeTest, NodeRegistration) {
    // 注册节点
    executor_register_nodes();

    // 验证 T_HashJoin 已注册
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    HashJoin *hj_plan = (HashJoin *)palloc0(estate->es_query_cxt, sizeof(HashJoin));
    hj_plan->join.plan.type = T_HashJoin;
    hj_plan->join.jointype = JOIN_INNER;
    hj_plan->hashclauses = nullptr;

    // 使用 ExecInitNode 应能找到注册的初始化函数
    PlanState *ps = ExecInitNode((Plan *)hj_plan, estate, 0);
    ASSERT_NE(ps, nullptr);
    EXPECT_EQ(ps->type, T_HashJoinState);
    EXPECT_NE(ps->ExecProcNode, nullptr);

    ExecEndHashJoin((HashJoinState *)ps);
    FreeEState(estate);
}

/**
 * @brief 测试 Hash 节点注册到执行器
 */
TEST(HashNodeTest, NodeRegistration) {
    // 注册节点
    executor_register_nodes();

    // 验证 T_Hash 已注册
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    Hash *hash_plan = (Hash *)palloc0(estate->es_query_cxt, sizeof(Hash));
    hash_plan->plan.type = T_Hash;

    // 使用 ExecInitNode 应能找到注册的初始化函数
    PlanState *ps = ExecInitNode((Plan *)hash_plan, estate, 0);
    ASSERT_NE(ps, nullptr);
    EXPECT_EQ(ps->type, T_HashState);

    ExecEndHash((HashState *)ps);
    FreeEState(estate);
}

/**
 * @brief 测试 JoinType 枚举值
 */
TEST(HashJoinNodeTest, JoinTypeEnum) {
    EXPECT_EQ(JOIN_INNER, 0);
    EXPECT_EQ(JOIN_LEFT, 1);
    EXPECT_EQ(JOIN_RIGHT, 2);
    EXPECT_EQ(JOIN_FULL, 3);
    EXPECT_EQ(JOIN_SEMI, 4);
    EXPECT_EQ(JOIN_ANTI, 5);
}

}  /* namespace */
