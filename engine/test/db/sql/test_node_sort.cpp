/**
 * @file test_node_sort.cpp
 * @brief Sort 排序执行器节点单元测试（Task 2.5）
 *
 * 测试范围：
 *   - SortState 初始化与释放
 *   - ExecProcNode 返回 NULL（框架实现）
 *   - 子节点树构建
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "db/sql/executor.h"
#include "db/sql/nodeSort.h"
#include "db/sql/nodeSeqscan.h"
#include "db/sql/nodes/nodetags.h"
#include "db/sql/nodes/execnodes.h"
#include "db/sql/memctx.h"
}

namespace {

/**
 * @brief 测试 SortState 初始化和释放
 */
TEST(SortNodeTest, InitAndEnd) {
    // 创建 EState
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    // 在查询上下文中分配 Sort 计划节点
    Sort *sort_plan = (Sort *)palloc0(estate->es_query_cxt, sizeof(Sort));
    sort_plan->plan.type = T_Sort;
    sort_plan->numCols = 1;
    sort_plan->sortColIdx = nullptr;

    // 初始化 SortState
    SortState *state = (SortState *)ExecInitSort((Plan *)sort_plan, estate, 0);
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->ps.type, T_SortState);
    EXPECT_NE(state->ps.ExecProcNode, nullptr);
    EXPECT_EQ(state->sort_Done, false);

    // 清理
    ExecEndSort(state);
    FreeEState(estate);
}

/**
 * @brief 测试 Sort 节点的基本初始化
 */
TEST(SortNodeTest, BasicInit) {
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    // 创建 Sort 计划节点（无子节点）
    Sort *sort_plan = (Sort *)palloc0(estate->es_query_cxt, sizeof(Sort));
    ASSERT_NE(sort_plan, nullptr);
    sort_plan->plan.type = T_Sort;
    sort_plan->plan.lefttree = nullptr;
    sort_plan->plan.righttree = nullptr;
    sort_plan->numCols = 1;

    // 初始化 SortState
    SortState *state = (SortState *)ExecInitSort((Plan *)sort_plan, estate, 0);
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->ps.type, T_SortState);
    EXPECT_EQ(state->sort_Done, false);

    // 清理
    ExecEndSort(state);
    FreeEState(estate);
}

/**
 * @brief 测试 ExecProcNode 返回 NULL（框架实现）
 */
TEST(SortNodeTest, ExecProcNodeReturnsNull) {
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    Sort *sort_plan = (Sort *)palloc0(estate->es_query_cxt, sizeof(Sort));
    sort_plan->plan.type = T_Sort;
    sort_plan->numCols = 1;

    SortState *state = (SortState *)ExecInitSort((Plan *)sort_plan, estate, 0);
    ASSERT_NE(state, nullptr);

    // 框架实现应返回 NULL
    TupleTableSlot *slot = ExecProcNode(&state->ps);
    EXPECT_EQ(slot, nullptr);

    // 清理
    ExecEndSort(state);
    FreeEState(estate);
}

/**
 * @brief 测试 ExecEndSort 处理 NULL
 */
TEST(SortNodeTest, EndNull) {
    // 不应崩溃
    ExecEndSort(nullptr);
}

/**
 * @brief 测试 ExecReScanSort 重置节点
 */
TEST(SortNodeTest, ReScan) {
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    Sort *sort_plan = (Sort *)palloc0(estate->es_query_cxt, sizeof(Sort));
    sort_plan->plan.type = T_Sort;
    sort_plan->numCols = 1;

    SortState *state = (SortState *)ExecInitSort((Plan *)sort_plan, estate, 0);
    ASSERT_NE(state, nullptr);

    // 重置节点（不应崩溃）
    ExecReScanSort(state);

    // 清理
    ExecEndSort(state);
    FreeEState(estate);
}

/**
 * @brief 测试 Sort 节点注册到执行器
 */
TEST(SortNodeTest, NodeRegistration) {
    // 注册节点
    executor_register_nodes();

    // 验证 T_Sort 已注册
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    Sort *sort_plan = (Sort *)palloc0(estate->es_query_cxt, sizeof(Sort));
    sort_plan->plan.type = T_Sort;
    sort_plan->numCols = 1;

    // 使用 ExecInitNode 应能找到注册的初始化函数
    PlanState *ps = ExecInitNode((Plan *)sort_plan, estate, 0);
    ASSERT_NE(ps, nullptr);
    EXPECT_EQ(ps->type, T_SortState);
    EXPECT_NE(ps->ExecProcNode, nullptr);

    ExecEndSort((SortState *)ps);
    FreeEState(estate);
}

}  /* namespace */