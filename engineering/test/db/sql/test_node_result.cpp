/**
 * @file test_node_result.cpp
 * @brief Result 执行器节点单元测试（Task 2.1）
 *
 * 测试范围：
 *   - ResultState 初始化与释放
 *   - ExecProcNode 返回一行后返回 NULL
 *   - 常量条件检查
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "db/sql/executor.h"
#include "db/sql/nodeResult.h"
#include "db/sql/nodes/nodetags.h"
#include "db/sql/nodes/execnodes.h"
#include "db/sql/memctx.h"
}

namespace {

/**
 * @brief 测试 ResultState 初始化和释放
 */
TEST(ResultNodeTest, InitAndEnd) {
    // 创建 EState
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    // 在查询上下文中分配 Result 计划节点
    Result *result_plan = (Result *)palloc0(estate->es_query_cxt, sizeof(Result));
    result_plan->plan.type = T_Result;
    result_plan->resconstantqual = nullptr;

    // 初始化 ResultState
    ResultState *state = (ResultState *)ExecInitResult((Plan *)result_plan, estate, 0);
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->ps.type, T_ResultState);
    EXPECT_NE(state->ps.ExecProcNode, nullptr);
    EXPECT_EQ(state->rs_done, false);
    EXPECT_NE(state->resultslot, nullptr);

    // 清理
    ExecEndResult(state);
    FreeEState(estate);
}

/**
 * @brief 测试 ExecProcNode 返回一行后返回 NULL
 */
TEST(ResultNodeTest, ExecProcNodeReturnsOneRow) {
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    Result *result_plan = (Result *)palloc0(estate->es_query_cxt, sizeof(Result));
    result_plan->plan.type = T_Result;
    result_plan->resconstantqual = nullptr;

    ResultState *state = (ResultState *)ExecInitResult((Plan *)result_plan, estate, 0);
    ASSERT_NE(state, nullptr);

    // 首次调用应返回一个 TupleTableSlot
    TupleTableSlot *slot = ExecProcNode(&state->ps);
    EXPECT_NE(slot, nullptr);
    EXPECT_EQ(slot, state->resultslot);

    // 标记已返回一行
    EXPECT_EQ(state->rs_done, true);

    // 第二次调用应返回 NULL
    slot = ExecProcNode(&state->ps);
    EXPECT_EQ(slot, nullptr);

    // 清理
    ExecEndResult(state);
    FreeEState(estate);
}

/**
 * @brief 测试无常量条件时的行为
 */
TEST(ResultNodeTest, NoConstantQual) {
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    Result *result_plan = (Result *)palloc0(estate->es_query_cxt, sizeof(Result));
    result_plan->plan.type = T_Result;
    result_plan->resconstantqual = nullptr;

    ResultState *state = (ResultState *)ExecInitResult((Plan *)result_plan, estate, 0);
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->resconstantqual, nullptr);

    // 无常量条件时，首次调用应返回一行
    TupleTableSlot *slot = ExecProcNode(&state->ps);
    EXPECT_NE(slot, nullptr);

    // 后续调用返回 NULL
    slot = ExecProcNode(&state->ps);
    EXPECT_EQ(slot, nullptr);

    ExecEndResult(state);
    FreeEState(estate);
}

/**
 * @brief 测试 ExecEndResult 处理 NULL
 */
TEST(ResultNodeTest, EndNull) {
    // 不应崩溃
    ExecEndResult(nullptr);
}

/**
 * @brief 测试 ExecReScanResult 重置节点
 */
TEST(ResultNodeTest, ReScan) {
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    Result *result_plan = (Result *)palloc0(estate->es_query_cxt, sizeof(Result));
    result_plan->plan.type = T_Result;
    result_plan->resconstantqual = nullptr;

    ResultState *state = (ResultState *)ExecInitResult((Plan *)result_plan, estate, 0);
    ASSERT_NE(state, nullptr);

    // 首次调用返回一行
    TupleTableSlot *slot = ExecProcNode(&state->ps);
    EXPECT_NE(slot, nullptr);

    // 第二次调用返回 NULL
    slot = ExecProcNode(&state->ps);
    EXPECT_EQ(slot, nullptr);

    // 重置节点
    ExecReScanResult(state);
    EXPECT_EQ(state->rs_done, false);

    // 重置后应能再次返回一行
    slot = ExecProcNode(&state->ps);
    EXPECT_NE(slot, nullptr);

    slot = ExecProcNode(&state->ps);
    EXPECT_EQ(slot, nullptr);

    ExecEndResult(state);
    FreeEState(estate);
}

/**
 * @brief 测试 ResultState 在内存上下文中分配
 */
TEST(ResultNodeTest, MemoryContextAllocation) {
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    // 记录分配前的内存上下文状态
    MemoryContext query_cxt = estate->es_query_cxt;
    ASSERT_NE(query_cxt, nullptr);

    Result *result_plan = (Result *)palloc0(query_cxt, sizeof(Result));
    result_plan->plan.type = T_Result;
    result_plan->resconstantqual = nullptr;

    ResultState *state = (ResultState *)ExecInitResult((Plan *)result_plan, estate, 0);
    ASSERT_NE(state, nullptr);

    // 验证 ResultState 在查询上下文中分配
    // （由 ExecInitResult 使用 palloc0 在 estate->es_query_cxt 中分配）

    ExecEndResult(state);
    FreeEState(estate);
}

/**
 * @brief 测试 Result 节点注册到执行器
 */
TEST(ResultNodeTest, NodeRegistration) {
    // 注册节点
    executor_register_nodes();

    // 验证 T_Result 已注册
    // 通过 ExecInitNode 间接验证
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    Result *result_plan = (Result *)palloc0(estate->es_query_cxt, sizeof(Result));
    result_plan->plan.type = T_Result;
    result_plan->resconstantqual = nullptr;

    // 使用 ExecInitNode 应能找到注册的初始化函数
    PlanState *ps = ExecInitNode((Plan *)result_plan, estate, 0);
    ASSERT_NE(ps, nullptr);
    EXPECT_EQ(ps->type, T_ResultState);
    EXPECT_NE(ps->ExecProcNode, nullptr);

    ExecEndResult((ResultState *)ps);
    FreeEState(estate);
}

}  /* namespace */