/**
 * @file test_executor.cpp
 * @brief SQL 执行器 Volcano 框架单元测试（Task 1.4）
 *
 * 测试范围：
 *   - EState 创建/销毁
 *   - ExprContext 创建/销毁
 *   - TupleTableSlot 操作
 *   - QueryDesc 创建/释放
 *   - DestReceiver 创建/销毁
 *   - 节点注册表
 *   - Volcano 迭代器接口
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "db/sql/executor.h"
}

namespace {

/**
 * @brief 测试 EState 创建和销毁
 */
TEST(ExecutorTest, CreateAndDestroy) {
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);
    EXPECT_EQ(estate->es_processed, 0);
    EXPECT_EQ(estate->es_direction, ForwardScanDirection);
    EXPECT_EQ(estate->type, T_EState);

    FreeEState(estate);
}

/**
 * @brief 测试 EState 与 MemoryContext 集成
 */
TEST(ExecutorTest, EStateWithMemoryContext) {
    EState *estate = CreateEState();
    ASSERT_NE(estate->es_query_cxt, nullptr);

    /* 可以在 es_query_cxt 中分配内存 */
    void *p = palloc(estate->es_query_cxt, 100);
    ASSERT_NE(p, nullptr);

    FreeEState(estate);
}

/**
 * @brief 测试 ExprContext 创建
 */
TEST(ExecutorTest, ExprContextCreate) {
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    ExprContext *ectxt = CreateExprContext(estate);
    ASSERT_NE(ectxt, nullptr);
    EXPECT_NE(ectxt->ecxt_per_query_memory, nullptr);
    EXPECT_NE(ectxt->ecxt_per_tuple_memory, nullptr);
    EXPECT_EQ(ectxt->type, T_ExprContext);

    FreeExprContext(ectxt, true);
    FreeEState(estate);
}

/**
 * @brief 测试 ExprContext 多个实例
 */
TEST(ExecutorTest, MultipleExprContexts) {
    EState *estate = CreateEState();
    ExprContext *ctx1 = CreateExprContext(estate);
    ExprContext *ctx2 = CreateExprContext(estate);

    ASSERT_NE(ctx1, nullptr);
    ASSERT_NE(ctx2, nullptr);
    EXPECT_NE(ctx1, ctx2);

    FreeExprContext(ctx1, true);
    FreeExprContext(ctx2, true);
    FreeEState(estate);
}

/**
 * @brief 测试 PlanState 虚函数字段
 */
TEST(PlanStateTest, VirtualFunction) {
    PlanState ps;
    memset(&ps, 0, sizeof(ps));
    ps.type = T_PlanState;
    ps.ExecProcNode = NULL;

    EXPECT_EQ(ps.type, T_PlanState);
    /* ExecProcNode 为 NULL 时不应被调用 */
    EXPECT_EQ(ps.ExecProcNode, nullptr);
}

/**
 * @brief 测试 PlanState 左/右子树字段
 */
TEST(PlanStateTest, Subtrees) {
    PlanState parent;
    PlanState left;
    PlanState right;
    memset(&parent, 0, sizeof(parent));
    memset(&left, 0, sizeof(left));
    memset(&right, 0, sizeof(right));

    parent.lefttree = &left;
    parent.righttree = &right;

    EXPECT_EQ(parent.lefttree, &left);
    EXPECT_EQ(parent.righttree, &right);
}

/**
 * @brief 测试 TupleTableSlot 创建和释放
 */
TEST(TupleTableSlotTest, MakeAndFree) {
    TupleTableSlot *slot = MakeTupleTableSlot();
    ASSERT_NE(slot, nullptr);
    EXPECT_EQ(slot->type, T_TupleTableSlot);
    EXPECT_EQ(slot->tts_nvalid, 0);

    FreeTupleTableSlot(slot);
}

/**
 * @brief 测试 TupleTableSlot 清空
 */
TEST(TupleTableSlotTest, ClearTuple) {
    TupleTableSlot *slot = MakeTupleTableSlot();
    ASSERT_NE(slot, nullptr);

    /* 模拟设置 nvalid */
    slot->tts_nvalid = 5;
    EXPECT_EQ(slot->tts_nvalid, 5);

    ExecClearTuple(slot);
    EXPECT_EQ(slot->tts_nvalid, 0);

    FreeTupleTableSlot(slot);
}

/**
 * @brief 测试 TupIsNull 宏
 */
TEST(TupleTableSlotTest, TupIsNull) {
    /* NULL 槽 */
    EXPECT_TRUE(TupIsNull(NULL));

    /* 空槽 */
    TupleTableSlot *slot = MakeTupleTableSlot();
    ASSERT_NE(slot, nullptr);
    EXPECT_TRUE(TupIsNull(slot));

    /* 模拟填充元组 */
    slot->tts_nvalid = 3;
    EXPECT_FALSE(TupIsNull(slot));

    FreeTupleTableSlot(slot);
}

/**
 * @brief 测试 QueryDesc 创建和释放
 */
TEST(QueryDescTest, CreateAndFree) {
    QueryDesc *qdesc = CreateQueryDesc(NULL, NULL);
    ASSERT_NE(qdesc, nullptr);
    EXPECT_EQ(qdesc->type, T_QueryDesc);
    EXPECT_EQ(qdesc->direction, ForwardScanDirection);
    EXPECT_EQ(qdesc->count, 0u);
    EXPECT_EQ(qdesc->doInstrument, false);

    FreeQueryDesc(qdesc);
}

/**
 * @brief 测试 QueryDesc 与 DestReceiver 集成
 */
TEST(QueryDescTest, WithDestReceiver) {
    QueryDesc *qdesc = CreateQueryDesc(NULL, NULL);
    ASSERT_NE(qdesc, nullptr);

    qdesc->dest = CreateDestReceiverObj();
    ASSERT_NE(qdesc->dest, nullptr);
    EXPECT_EQ(qdesc->dest->type, T_DestReceiver);

    FreeQueryDesc(qdesc);
}

/**
 * @brief 测试 DestReceiver 创建和销毁
 */
TEST(DestReceiverTest, CreateAndDestroy) {
    DestReceiver *self = CreateDestReceiverObj();
    ASSERT_NE(self, nullptr);
    EXPECT_EQ(self->type, T_DestReceiver);
    EXPECT_NE(self->rStartup, nullptr);
    EXPECT_NE(self->rShutdown, nullptr);
    EXPECT_NE(self->rDestroy, nullptr);

    DestReceiverDestroy(self);
}

/**
 * @brief 测试 DestReceiver 销毁 NULL
 */
TEST(DestReceiverTest, DestroyNull) {
    /* 不应崩溃 */
    DestReceiverDestroy(NULL);
}

/**
 * @brief 测试节点注册表初始化
 */
TEST(NodeRegistryTest, Init) {
    /* 调用应幂等 */
    executor_register_nodes();
    executor_register_nodes();
    /* 不应崩溃 */
}

/**
 * @brief 测试节点注册函数
 */
TEST(NodeRegistryTest, Register) {
    executor_register_nodes();

    /* 注册一个测试函数 */
    auto test_init = [](Plan *plan, EState *estate, int eflags) -> PlanState * {
        (void)plan;
        (void)estate;
        (void)eflags;
        return nullptr;
    };

    /* 注册到 T_Result */
    executor_register_node(T_Result, test_init);

    /* 验证可通过 ExecInitNode 查找（间接测试） */
    /* 由于 ExecInitNode 需要 Plan 节点，这里仅验证注册成功 */

    /* 测试 NULL 函数指针被忽略 */
    executor_register_node(T_SeqScan, nullptr);
}

/**
 * @brief 测试 ExecutorStart 处理 NULL
 */
TEST(ExecutorLifecycleTest, StartWithNull) {
    /* 不应崩溃 */
    ExecutorStart(NULL, 0);
}

/**
 * @brief 测试 ExecutorRun 处理 NULL
 */
TEST(ExecutorLifecycleTest, RunWithNull) {
    /* 不应崩溃 */
    ExecutorRun(NULL, ForwardScanDirection, 0);
}

/**
 * @brief 测试 ExecutorFinish 处理 NULL
 */
TEST(ExecutorLifecycleTest, FinishWithNull) {
    /* 不应崩溃 */
    ExecutorFinish(NULL);
}

/**
 * @brief 测试 ExecutorEnd 处理 NULL
 */
TEST(ExecutorLifecycleTest, EndWithNull) {
    /* 不应崩溃 */
    ExecutorEnd(NULL);
}

/**
 * @brief 测试完整生命周期
 */
TEST(ExecutorLifecycleTest, FullCycle) {
    QueryDesc *qdesc = CreateQueryDesc(NULL, NULL);
    ASSERT_NE(qdesc, nullptr);

    /* ExecutorStart：创建 EState */
    ExecutorStart(qdesc, EXEC_FLAG_NONE);
    EXPECT_NE(qdesc->estate, nullptr);

    /* ExecutorRun：应能安全处理 NULL planstate */
    ExecutorRun(qdesc, ForwardScanDirection, 0);

    /* ExecutorFinish：清理 */
    ExecutorFinish(qdesc);

    /* ExecutorEnd：释放 EState */
    ExecutorEnd(qdesc);
    EXPECT_EQ(qdesc->estate, nullptr);

    FreeQueryDesc(qdesc);
}

/**
 * @brief 测试 ExecInitNode 处理 NULL
 */
TEST(PlanStateTreeTest, InitNodeNull) {
    EXPECT_EQ(ExecInitNode(NULL, NULL, 0), nullptr);
}

/**
 * @brief 测试 ExecEndNode 处理 NULL
 */
TEST(PlanStateTreeTest, EndNodeNull) {
    /* 不应崩溃 */
    ExecEndNode(NULL);
}

/**
 * @brief 测试 ExecReScan 处理 NULL
 */
TEST(PlanStateTreeTest, ReScanNull) {
    /* 不应崩溃 */
    ExecReScan(NULL);
}

/**
 * @brief 测试 ExecProcNode inline 函数
 */
TEST(VolcanoTest, ExecProcNodeNull) {
    /* ExecProcNode(NULL) 应返回 NULL */
    EXPECT_EQ(ExecProcNode(NULL), nullptr);
}

/**
 * @brief 测试 ExecProcNode 在 ExecProcNode=NULL 时
 */
TEST(VolcanoTest, ExecProcNodeWithoutFunction) {
    PlanState ps;
    memset(&ps, 0, sizeof(ps));
    ps.type = T_PlanState;
    ps.ExecProcNode = NULL;

    /* ExecProcNode 应处理 NULL 函数指针 */
    EXPECT_EQ(ExecProcNode(&ps), nullptr);
}

/**
 * @brief 测试 EState 字段默认值
 */
TEST(EStateTest, DefaultValues) {
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    /* 验证关键字段 */
    EXPECT_EQ(estate->es_processed, 0u);
    EXPECT_EQ(estate->es_output_cid, 0u);
    EXPECT_EQ(estate->es_direction, ForwardScanDirection);
    EXPECT_FALSE(estate->es_use_parallel_mode);
    EXPECT_EQ(estate->es_parallel_workers_needed, 0);
    EXPECT_EQ(estate->es_tupleTable, nullptr);
    EXPECT_EQ(estate->es_snapshot.type, T_Invalid);  /* Snapshot 已清零 */

    FreeEState(estate);
}

/**
 * @brief 测试 FreeEState(NULL)
 */
TEST(EStateTest, FreeNull) {
    /* 不应崩溃 */
    FreeEState(nullptr);
}

/**
 * @brief 测试 FreeExprContext(NULL)
 */
TEST(ExprContextTest, FreeNull) {
    /* 不应崩溃 */
    FreeExprContext(nullptr, true);
}

/**
 * @brief 测试 CreateExprContext(NULL estate)
 */
TEST(ExprContextTest, CreateWithNullEstate) {
    EXPECT_EQ(CreateExprContext(nullptr), nullptr);
}

/**
 * @brief 测试 Volcano 桥接函数
 */
TEST(VolcanoBridgeTest, CreateByPhysType) {
    /* PHYS_RESULT 在 PhysicalOpType 枚举中值为 45 */
    PlanState *state = executor_create_plan_state_by_phys_type(45);
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->type, T_Result);
    /* Result 节点应挂载占位执行函数 */
    EXPECT_NE(state->ExecProcNode, nullptr);

    free(state);

    /* 其他节点类型 */
    state = executor_create_plan_state_by_phys_type(0 /* PHYS_SEQ_SCAN */);
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->ExecProcNode, nullptr);

    free(state);
}

}  /* namespace */