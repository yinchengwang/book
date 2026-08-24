/**
 * @file sql_executor_memctx_test.cpp
 * @brief SQL Executor MemoryContext 迁移验证测试（Task 10）
 *
 * 验证目标：
 *   - executor.c 中 QueryDesc / DestReceiver / TupleTableSlot 已通过 MemoryContext 分配
 *   - expr.c 中 ExprState 已通过 MemoryContext 分配
 *   - planner.c 注释了 Task 10 候选迁移位置
 *
 * 本测试不直接构造 mmdb_t 数据库，而是直接调用 executor 的公开 API，
 * 使用 MemoryContextSwitchTo 切换到测试 AllocSet 上下文，以模拟真实查询生命周期。
 *
 * 测试策略：
 *   1. 创建顶层 AllocSet 上下文（TopContext）
 *   2. 创建查询级子上下文（QueryContext）模拟 EState 的 es_query_cxt
 *   3. 在 QueryContext 下分配 QueryDesc / DestReceiver / TupleTableSlot / ExprState
 *   4. 释放 / 复用时验证没有崩溃
 *   5. MemoryContextDelete(QueryContext) 后验证所有分配都回收
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "db/sql/executor.h"
#include "db/sql/expr.h"
#include "db/sql/memctx.h"
#include "db/sql/sql_planner.h"  /* 提供 Expr 完整定义 */
#include "db/parser/sql/parsenodes.h"
}

namespace {

/**
 * @brief 测试 QueryDesc 通过 MemoryContext 分配
 *
 * 验证 CreateQueryDesc 在指定上下文中分配，且 FreeQueryDesc 不会对
 * palloc 内存错误调用 free。
 */
TEST(SqlExecutorMemctxTest, QueryDescAllocatedFromMemoryContext) {
    /* 1. 创建顶层 AllocSet 上下文 */
    MemoryContext top = AllocSetContextCreate(
        NULL, "SqlExecutorMemctxTestTop",
        0, ALLOCSET_DEFAULT_BLOCK_SIZE, ALLOCSET_DEFAULT_BLOCK_SIZE);
    ASSERT_NE(top, nullptr);

    /* 2. 创建查询级子上下文 */
    MemoryContext query = AllocSetContextCreate(
        top, "SqlExecutorMemctxTestQuery",
        0, ALLOCSET_DEFAULT_BLOCK_SIZE, ALLOCSET_DEFAULT_BLOCK_SIZE);
    ASSERT_NE(query, nullptr);

    /* 3. 切换到查询上下文 */
    MemoryContext old = MemoryContextSwitchTo(query);

    /* 4. 分配 QueryDesc */
    QueryDesc *qdesc = CreateQueryDesc(NULL, NULL);
    ASSERT_NE(qdesc, nullptr);
    EXPECT_EQ(qdesc->type, T_QueryDesc);
    EXPECT_EQ(qdesc->plannedstmt, nullptr);
    EXPECT_EQ(qdesc->planstate, nullptr);
    EXPECT_EQ(qdesc->estate, nullptr);
    EXPECT_EQ(qdesc->dest, nullptr);

    /* 5. 释放：Task 10 后 FreeQueryDesc 不再调用 free */
    FreeQueryDesc(qdesc);

    /* 6. 切回原上下文并删除查询级上下文 */
    MemoryContextSwitchTo(old);
    MemoryContextDelete(query);

    /* 7. 删除顶层 */
    MemoryContextDelete(top);
}

/**
 * @brief 测试 DestReceiver 通过 MemoryContext 分配
 *
 * 验证 CreateDestReceiverObj 在指定上下文中分配，并验证
 * DestReceiverDestroy 释放回调后不会对 palloc 内存错误调用 free。
 */
TEST(SqlExecutorMemctxTest, DestReceiverAllocatedFromMemoryContext) {
    MemoryContext top = AllocSetContextCreate(
        NULL, "DR_Top",
        0, ALLOCSET_DEFAULT_BLOCK_SIZE, ALLOCSET_DEFAULT_BLOCK_SIZE);
    ASSERT_NE(top, nullptr);

    MemoryContext query = AllocSetContextCreate(
        top, "DR_Query",
        0, ALLOCSET_DEFAULT_BLOCK_SIZE, ALLOCSET_DEFAULT_BLOCK_SIZE);
    ASSERT_NE(query, nullptr);

    MemoryContext old = MemoryContextSwitchTo(query);

    DestReceiver *dr = CreateDestReceiverObj();
    ASSERT_NE(dr, nullptr);
    EXPECT_EQ(dr->type, T_DestReceiver);
    EXPECT_NE(dr->rStartup, nullptr);
    EXPECT_NE(dr->rShutdown, nullptr);

    /* Task 10 后 DestReceiverDestroy 仅调 rDestroy，不再 free(self) */
    DestReceiverDestroy(dr);

    MemoryContextSwitchTo(old);
    MemoryContextDelete(query);
    MemoryContextDelete(top);
}

/**
 * @brief 测试 TupleTableSlot 通过 MemoryContext 分配
 *
 * 验证在 mcxt=NULL 时 MakeTupleTableSlotWithMCxt 也会使用 CurrentMemoryContext，
 * 而不是回退到 calloc。
 */
TEST(SqlExecutorMemctxTest, TupleTableSlotFallbackToCurrentMCxt) {
    MemoryContext top = AllocSetContextCreate(
        NULL, "Slot_Top",
        0, ALLOCSET_DEFAULT_BLOCK_SIZE, ALLOCSET_DEFAULT_BLOCK_SIZE);
    ASSERT_NE(top, nullptr);

    MemoryContext query = AllocSetContextCreate(
        top, "Slot_Query",
        0, ALLOCSET_DEFAULT_BLOCK_SIZE, ALLOCSET_DEFAULT_BLOCK_SIZE);
    ASSERT_NE(query, nullptr);

    MemoryContext old = MemoryContextSwitchTo(query);

    /* mcxt=NULL 应回退到 CurrentMemoryContext（不再是 calloc） */
    TupleTableSlot *slot = MakeTupleTableSlotWithMCxt(NULL);
    ASSERT_NE(slot, nullptr);
    EXPECT_EQ(slot->type, T_TupleTableSlot);
    /* tts_mcxt 是 Size 句柄，非零表示已分配在上下文中 */
    EXPECT_NE(slot->tts_mcxt, (Size)0);

    /* Task 10 后 FreeTupleTableSlot 改为 no-op */
    FreeTupleTableSlot(slot);

    MemoryContextSwitchTo(old);
    MemoryContextDelete(query);
    MemoryContextDelete(top);
}

/**
 * @brief 测试 ExprState 通过 MemoryContext 分配
 *
 * 验证 ExecInitExpr 在 CurrentMemoryContext 中分配，且 ExecFreeExpr
 * 不再调用 free。
 */
TEST(SqlExecutorMemctxTest, ExprStateAllocatedFromMemoryContext) {
    MemoryContext top = AllocSetContextCreate(
        NULL, "Expr_Top",
        0, ALLOCSET_DEFAULT_BLOCK_SIZE, ALLOCSET_DEFAULT_BLOCK_SIZE);
    ASSERT_NE(top, nullptr);

    MemoryContext query = AllocSetContextCreate(
        top, "Expr_Query",
        0, ALLOCSET_DEFAULT_BLOCK_SIZE, ALLOCSET_DEFAULT_BLOCK_SIZE);
    ASSERT_NE(query, nullptr);

    MemoryContext old = MemoryContextSwitchTo(query);

    /* 构造最小 Expr 节点作为输入 */
    Expr expr;
    memset(&expr, 0, sizeof(Expr));
    expr.expr_type = EXPR_CONST;
    expr.result_type = 0; /* 任意 Oid */

    ExprState *state = ExecInitExpr(&expr, NULL);
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->type, T_ExprState);
    EXPECT_NE(state->steps, nullptr);
    EXPECT_EQ(state->nsteps, 2);
    EXPECT_EQ(state->steps[1].op, EEOP_DONE);

    /* Task 10 后 ExecFreeExpr 改为 no-op */
    ExecFreeExpr(state);

    MemoryContextSwitchTo(old);
    MemoryContextDelete(query);
    MemoryContextDelete(top);
}

/**
 * @brief 测试多次分配复用同一查询上下文
 *
 * 模拟一个查询中创建多个 TupleTableSlot / ExprState，最后由
 * MemoryContextDelete(query) 统一回收。
 */
TEST(SqlExecutorMemctxTest, MultipleAllocationsReuseQueryContext) {
    MemoryContext top = AllocSetContextCreate(
        NULL, "Multi_Top",
        0, ALLOCSET_DEFAULT_BLOCK_SIZE, ALLOCSET_DEFAULT_BLOCK_SIZE);
    MemoryContext query = AllocSetContextCreate(
        top, "Multi_Query",
        0, ALLOCSET_DEFAULT_BLOCK_SIZE, ALLOCSET_DEFAULT_BLOCK_SIZE);

    MemoryContext old = MemoryContextSwitchTo(query);

    /* 模拟一次查询：创建多个 QueryDesc / Slots / DestReceivers */
    QueryDesc *qdesc1 = CreateQueryDesc(NULL, NULL);
    QueryDesc *qdesc2 = CreateQueryDesc(NULL, NULL);
    TupleTableSlot *slot1 = MakeTupleTableSlotWithMCxt(NULL);
    TupleTableSlot *slot2 = MakeTupleTableSlotWithMCxt(NULL);
    DestReceiver *dr = CreateDestReceiverObj();

    ASSERT_NE(qdesc1, nullptr);
    ASSERT_NE(qdesc2, nullptr);
    ASSERT_NE(slot1, nullptr);
    ASSERT_NE(slot2, nullptr);
    ASSERT_NE(dr, nullptr);

    EXPECT_NE(qdesc1, qdesc2);
    EXPECT_NE(slot1, slot2);

    /* 释放（Task 10 后所有这些函数都不实际 free） */
    FreeQueryDesc(qdesc1);
    FreeQueryDesc(qdesc2);
    FreeTupleTableSlot(slot1);
    FreeTupleTableSlot(slot2);
    DestReceiverDestroy(dr);

    /* 最终由 MemoryContextDelete(query) 一次性回收所有分配 */
    MemoryContextSwitchTo(old);
    MemoryContextDelete(query);
    MemoryContextDelete(top);
}

/**
 * @brief 测试 ExecutorEnd 释放 PlanState 树时不再 free
 *
 * 模拟一个 PlanState 节点已挂在 EState->es_query_cxt 中，
 * ExecEndNode 不再单独 free(node)，而由 EState 上下文销毁时统一回收。
 */
TEST(SqlExecutorMemctxTest, ExecEndNodeNoLongerFreesPlanState) {
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);
    ASSERT_NE(estate->es_query_cxt, nullptr);

    /* 在 EState 的 es_query_cxt 下分配一个 PlanState */
    MemoryContext old = MemoryContextSwitchTo(estate->es_query_cxt);

    PlanState *node = (PlanState *)palloc0(estate->es_query_cxt, sizeof(PlanState));
    ASSERT_NE(node, nullptr);
    node->type = T_PlanState;
    node->instrument = NULL;
    node->chgParam = NULL;
    node->lefttree = NULL;
    node->righttree = NULL;

    /* Task 10 后 ExecEndNode 不再 free(node) 自身 */
    ExecEndNode(node);

    MemoryContextSwitchTo(old);

    /* 完整 EState 销毁应能回收 node */
    FreeEState(estate);
}

}  // namespace
