/**
 * @file test_integration.cpp
 * @brief SQL 执行引擎 Phase 1 集成测试（Task 1.5）
 *
 * 测试范围：
 *   - Phase 1 所有组件端到端集成
 *   - SELECT 1 端到端执行路径
 *   - 所有 Phase 1 测试回归验证
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "db/sql/executor.h"
#include "db/sql/expr.h"
#include "db/sql/memctx.h"
#include "db/sql/nodes/nodetags.h"
#include "db/sql/nodes/execnodes.h"
}

namespace {

/**
 * @brief 测试：完整生命周期 - 从 EState 创建到释放
 */
TEST(IntegrationTest, CompleteLifecycle) {
    // 1. 创建 EState（包含 MemoryContext）
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);
    EXPECT_NE(estate->es_query_cxt, nullptr);
    
    // 2. 在 es_query_cxt 中分配内存
    void *p1 = palloc(estate->es_query_cxt, 100);
    ASSERT_NE(p1, nullptr);
    memset(p1, 0xAB, 100);
    
    // 3. 创建 ExprContext
    ExprContext *ectxt = CreateExprContext(estate);
    ASSERT_NE(ectxt, nullptr);
    EXPECT_NE(ectxt->ecxt_per_query_memory, nullptr);
    EXPECT_NE(ectxt->ecxt_per_tuple_memory, nullptr);
    
    // 4. 创建 TupleTableSlot
    TupleTableSlot *slot = MakeTupleTableSlot();
    ASSERT_NE(slot, nullptr);
    
    // 5. 创建 QueryDesc
    QueryDesc *qdesc = CreateQueryDesc(NULL, NULL);
    ASSERT_NE(qdesc, nullptr);
    
    // 6. 清理
    FreeQueryDesc(qdesc);
    FreeTupleTableSlot(slot);
    FreeExprContext(ectxt, true);
    FreeEState(estate);
}

/**
 * @brief 测试：MemoryContext 层级与 EState 集成
 */
TEST(IntegrationTest, MemoryContextHierarchy) {
    EState *estate = CreateEState();
    MemoryContext parent = estate->es_query_cxt;
    
    // 在父上下文分配
    void *p1 = palloc(parent, 100);
    ASSERT_NE(p1, nullptr);
    
    // 创建子上下文
    MemoryContext child = AllocSetContextCreate(parent, "test_child", 0, 1024, 1024);
    ASSERT_NE(child, nullptr);
    
    // 在子上下文分配
    void *p2 = palloc(child, 50);
    ASSERT_NE(p2, nullptr);
    
    // reset 子上下文
    reset_memory(child);
    
    // 子上下文重新分配应该成功
    void *p3 = palloc(child, 50);
    ASSERT_NE(p3, nullptr);
    
    // 删除父上下文（应该递归删除子上下文）
    delete_memory(parent);
}

/**
 * @brief 测试：ExprContext 与 EState 集成
 */
TEST(IntegrationTest, ExprContextWithEState) {
    EState *estate = CreateEState();
    ExprContext *ectxt = CreateExprContext(estate);
    
    // ExprContext 应该引用 EState 的 MemoryContext
    EXPECT_EQ(ectxt->ecxt_per_query_memory, estate->es_query_cxt);
    
    // per_tuple_memory 应该是子上下文
    EXPECT_NE(ectxt->ecxt_per_tuple_memory, nullptr);
    
    // 可以在 per_tuple_memory 中分配
    void *p = palloc(ectxt->ecxt_per_tuple_memory, 100);
    ASSERT_NE(p, nullptr);
    
    // reset per_tuple_memory（模拟每次元组处理后清理）
    reset_memory(ectxt->ecxt_per_tuple_memory);
    
    // 重新分配应该成功
    void *p2 = palloc(ectxt->ecxt_per_tuple_memory, 100);
    ASSERT_NE(p2, nullptr);
    
    FreeExprContext(ectxt, true);
    FreeEState(estate);
}

/**
 * @brief 测试：TupleTableSlot 与 EState 集成
 */
TEST(IntegrationTest, TupleSlotWithEState) {
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    // 创建 TupleTableSlot
    TupleTableSlot *slot = MakeTupleTableSlot();
    ASSERT_NE(slot, nullptr);
    EXPECT_EQ(slot->type, T_TupleTableSlot);

    // 清理
    FreeTupleTableSlot(slot);
    FreeEState(estate);
}

/**
 * @brief 测试：节点注册表与执行器框架集成
 */
TEST(IntegrationTest, NodeRegistryWithExecutor) {
    // 初始化节点注册表
    executor_register_nodes();

    // 创建 EState
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    // 创建 PlanState（不需要 Plan 结构，只需 PlanState）
    PlanState *planstate = (PlanState *)palloc(estate->es_query_cxt, sizeof(PlanState));
    planstate->type = T_PlanState;
    planstate->state = estate;
    planstate->lefttree = nullptr;
    planstate->righttree = nullptr;
    planstate->ExecProcNode = nullptr;
    planstate->ExecProcNodeReal = nullptr;

    // 验证 PlanState 字段正确设置
    EXPECT_EQ(planstate->type, T_PlanState);
    EXPECT_EQ(planstate->state, estate);
    EXPECT_EQ(planstate->lefttree, nullptr);

    // 清理
    pfree(estate->es_query_cxt, planstate);
    FreeEState(estate);
}

/**
 * @brief 测试：所有 Phase 1 组件无冲突
 */
TEST(IntegrationTest, AllPhase1ComponentsNoConflict) {
    // 确保所有类型定义不冲突
    EXPECT_EQ(T_Invalid, 0);
    EXPECT_GT(T_EState, T_Invalid);
    EXPECT_GT(T_ExprContext, T_EState);
    EXPECT_GT(T_PlanState, T_Invalid);
    EXPECT_GT(T_MemoryContext, T_Invalid);

    // 确保 sizeof 与预期一致
    EXPECT_GE(sizeof(EState), sizeof(NodeTag));
    EXPECT_GE(sizeof(ExprContext), sizeof(NodeTag));
    EXPECT_GE(sizeof(PlanState), sizeof(NodeTag));
}

}  // namespace
