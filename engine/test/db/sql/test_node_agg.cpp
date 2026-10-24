/**
 * @file test_node_agg.cpp
 * @brief Agg 聚合执行器节点单元测试（Task 2.4）
 *
 * 测试范围：
 *   - AggState 初始化与释放
 *   - ExecProcNode 返回 NULL（框架实现）
 *   - 子节点树构建
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "db/sql/executor.h"
#include "db/sql/nodeAgg.h"
#include "db/sql/nodeSeqscan.h"
#include "db/sql/nodes/nodetags.h"
#include "db/sql/nodes/execnodes.h"
#include "db/sql/memctx.h"
}

namespace {

/**
 * @brief 测试 AggState 初始化和释放
 */
TEST(AggNodeTest, InitAndEnd) {
    // 创建 EState
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    // 在查询上下文中分配 Agg 计划节点
    Agg *agg_plan = (Agg *)palloc0(estate->es_query_cxt, sizeof(Agg));
    agg_plan->plan.type = T_Agg;
    agg_plan->aggstrategy = AGG_PLAIN;
    agg_plan->numCols = 0;
    agg_plan->grpColIdx = nullptr;

    // 初始化 AggState
    AggState *state = (AggState *)ExecInitAgg((Plan *)agg_plan, estate, 0);
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->ps.type, T_AggState);
    EXPECT_NE(state->ps.ExecProcNode, nullptr);
    EXPECT_EQ(state->aggstrategy, AGG_PLAIN);

    // 清理
    ExecEndAgg(state);
    FreeEState(estate);
}

/**
 * @brief 测试 Agg 节点的基本初始化
 */
TEST(AggNodeTest, BasicInit) {
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    // 创建 Agg 计划节点（无子节点）
    Agg *agg_plan = (Agg *)palloc0(estate->es_query_cxt, sizeof(Agg));
    ASSERT_NE(agg_plan, nullptr);
    agg_plan->plan.type = T_Agg;
    agg_plan->plan.lefttree = nullptr;
    agg_plan->plan.righttree = nullptr;
    agg_plan->aggstrategy = AGG_PLAIN;
    agg_plan->numCols = 0;
    agg_plan->grpColIdx = nullptr;

    // 初始化 AggState
    AggState *state = (AggState *)ExecInitAgg((Plan *)agg_plan, estate, 0);
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->ps.type, T_AggState);
    EXPECT_EQ(state->aggstrategy, AGG_PLAIN);

    // 清理
    ExecEndAgg(state);
    FreeEState(estate);
}

/**
 * @brief 测试 ExecProcNode 返回 NULL（框架实现）
 */
TEST(AggNodeTest, ExecProcNodeReturnsNull) {
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    Agg *agg_plan = (Agg *)palloc0(estate->es_query_cxt, sizeof(Agg));
    agg_plan->plan.type = T_Agg;
    agg_plan->aggstrategy = AGG_PLAIN;

    AggState *state = (AggState *)ExecInitAgg((Plan *)agg_plan, estate, 0);
    ASSERT_NE(state, nullptr);

    // 框架实现应返回 NULL
    TupleTableSlot *slot = ExecProcNode(&state->ps);
    EXPECT_EQ(slot, nullptr);

    // 清理
    ExecEndAgg(state);
    FreeEState(estate);
}

/**
 * @brief 测试 ExecEndAgg 处理 NULL
 */
TEST(AggNodeTest, EndNull) {
    // 不应崩溃
    ExecEndAgg(nullptr);
}

/**
 * @brief 测试 ExecReScanAgg 重置节点
 */
TEST(AggNodeTest, ReScan) {
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    Agg *agg_plan = (Agg *)palloc0(estate->es_query_cxt, sizeof(Agg));
    agg_plan->plan.type = T_Agg;
    agg_plan->aggstrategy = AGG_PLAIN;

    AggState *state = (AggState *)ExecInitAgg((Plan *)agg_plan, estate, 0);
    ASSERT_NE(state, nullptr);

    // 重置节点（不应崩溃）
    ExecReScanAgg(state);

    // 清理
    ExecEndAgg(state);
    FreeEState(estate);
}

/**
 * @brief 测试 Agg 节点注册到执行器
 */
TEST(AggNodeTest, NodeRegistration) {
    // 注册节点
    executor_register_nodes();

    // 验证 T_Agg 已注册
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    Agg *agg_plan = (Agg *)palloc0(estate->es_query_cxt, sizeof(Agg));
    agg_plan->plan.type = T_Agg;
    agg_plan->aggstrategy = AGG_PLAIN;

    // 使用 ExecInitNode 应能找到注册的初始化函数
    PlanState *ps = ExecInitNode((Plan *)agg_plan, estate, 0);
    ASSERT_NE(ps, nullptr);
    EXPECT_EQ(ps->type, T_AggState);
    EXPECT_NE(ps->ExecProcNode, nullptr);

    ExecEndAgg((AggState *)ps);
    FreeEState(estate);
}

/**
 * @brief 测试 AggStrategy 枚举值
 */
TEST(AggNodeTest, AggStrategyEnum) {
    EXPECT_EQ(AGG_PLAIN, 0);
    EXPECT_EQ(AGG_SORTED, 1);
    EXPECT_EQ(AGG_HASHED, 2);
}

}  /* namespace */