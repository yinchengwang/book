/**
 * @file test_nodes.cpp
 * @brief 节点类型枚举与基础结构单元测试
 *
 * 覆盖 Task 1.2 验收标准：
 *   - NodeTag 枚举值完整、唯一
 *   - PlanState/EState/ExprContext/TupleTableSlot 结构定义正确
 *   - 与 Task 1.1 MemoryContext 无冲突
 */

#include <gtest/gtest.h>
#include <cstdlib>
#include <cstring>

extern "C" {
#include "db/sql/nodes/nodetags.h"
#include "db/sql/nodes/execnodes.h"
#include "db/sql/memctx.h"
}

/* ========================================================================
 * NodeTag 枚举值测试
 * ======================================================================== */

TEST(NodeTagTest, TagValues) {
    /* 基础哨兵值 */
    EXPECT_EQ(T_Invalid, 0);

    /* T_ExecutorBase 必须从 100 起，与解析期标签保留间距 */
    EXPECT_EQ(T_ExecutorBase, 100);

    /* 执行器层标签全部 > 解析期标签上限 */
    EXPECT_GT(T_EState, T_AllocSetContext);
    EXPECT_GT(T_PlanState, T_Invalid);
    EXPECT_GT(T_ExprState, T_Invalid);
    EXPECT_GT(T_EState, T_Invalid);
    EXPECT_GT(T_TupleTableSlot, T_Invalid);
    EXPECT_GT(T_Expr, T_Invalid);
    EXPECT_GT(T_Var, T_Invalid);
    EXPECT_GT(T_Const, T_Invalid);
}

TEST(NodeTagTest, PlanTags) {
    /* 计划节点标签存在且单调递增 */
    EXPECT_GT(T_Plan, T_ExecutorBase);
    EXPECT_GT(T_Result, T_Plan);
    EXPECT_GT(T_Append, T_Result);
    EXPECT_GT(T_SeqScan, T_Append);
    EXPECT_GT(T_IndexScan, T_SeqScan);
    EXPECT_GT(T_NestLoop, T_IndexScan);
    EXPECT_GT(T_HashJoin, T_NestLoop);
    EXPECT_GT(T_Hash, T_HashJoin);
    EXPECT_GT(T_Sort, T_Hash);
    EXPECT_GT(T_Agg, T_Sort);
    EXPECT_GT(T_WindowAgg, T_Agg);
    EXPECT_GT(T_Limit, T_WindowAgg);
    EXPECT_GT(T_ModifyTable, T_Limit);
    EXPECT_GT(T_Gather, T_ModifyTable);
}

TEST(NodeTagTest, PlanStateTags) {
    /* 计划状态节点标签 */
    EXPECT_GT(T_PlanState, T_Gather);
    EXPECT_GT(T_ResultState, T_PlanState);
    EXPECT_GT(T_SeqScanState, T_ResultState);
    EXPECT_GT(T_HashJoinState, T_SeqScanState);
    EXPECT_GT(T_HashState, T_HashJoinState);
    EXPECT_GT(T_AggState, T_HashState);
}

/* ========================================================================
 * PlanState 结构测试
 * ======================================================================== */

TEST(PlanStateTest, BasicStructure) {
    PlanState ps;
    /* 字段必须可访问，并可独立赋值 */
    std::memset(&ps, 0, sizeof(ps));
    ps.type = T_PlanState;
    ps.state = NULL;
    ps.ExecProcNode = NULL;
    ps.ExecProcNodeReal = NULL;
    ps.lefttree = NULL;
    ps.righttree = NULL;
    ps.qual = NULL;
    ps.recheck = NULL;
    ps.ps_ProjInfo = NULL;
    ps.ps_ResultTupleSlot = NULL;
    ps.ps_ResultTupleDesc = NULL;
    ps.ps_ExprContext = NULL;
    ps.instrument = NULL;
    ps.needs_to_scan_queue = false;
    ps.chgParam = NULL;

    EXPECT_EQ(ps.type, T_PlanState);
    EXPECT_EQ(ps.state, nullptr);
    EXPECT_EQ(ps.lefttree, nullptr);
    EXPECT_EQ(ps.righttree, nullptr);
    EXPECT_EQ(ps.ExecProcNode, nullptr);
    EXPECT_FALSE(ps.needs_to_scan_queue);
}

TEST(PlanStateTest, SizeSanity) {
    /* PlanState 是基类，应该有非零大小 */
    EXPECT_GT(sizeof(PlanState), 0u);
}

/* ========================================================================
 * EState 结构测试
 * ======================================================================== */

TEST(EStateTest, Create) {
    EState *estate = static_cast<EState *>(std::malloc(sizeof(EState)));
    ASSERT_NE(estate, nullptr);
    std::memset(estate, 0, sizeof(*estate));

    estate->type = T_EState;
    estate->es_query_cxt = NULL;
    estate->es_trig_tuple_slot = NULL;
    estate->es_processed = 0;
    estate->es_direction = ForwardScanDirection;
    estate->es_use_parallel_mode = false;
    estate->es_parallel_workers_needed = 0;

    EXPECT_EQ(estate->type, T_EState);
    EXPECT_EQ(estate->es_processed, 0u);
    EXPECT_EQ(estate->es_direction, ForwardScanDirection);
    EXPECT_FALSE(estate->es_use_parallel_mode);

    std::free(estate);
}

TEST(EStateTest, MemoryContextIntegration) {
    /* 与 Task 1.1 的 MemoryContext 集成 */
    MemoryContext mcxt = AllocSetContextCreate(NULL, "estate_test", 0, 8192, 8192);
    ASSERT_NE(mcxt, nullptr);

    EState *estate = static_cast<EState *>(palloc(mcxt, sizeof(EState)));
    ASSERT_NE(estate, nullptr);

    estate->type = T_EState;
    estate->es_query_cxt = mcxt;
    estate->es_processed = 12345;

    EXPECT_EQ(estate->es_query_cxt, mcxt);
    EXPECT_EQ(estate->es_processed, 12345u);

    /* 子上下文可用于表达式分配 */
    MemoryContext ecxt = AllocSetContextCreate(mcxt, "expr_cxt", 0, 4096, 4096);
    ASSERT_NE(ecxt, nullptr);
    void *p = palloc(ecxt, 64);
    EXPECT_NE(p, nullptr);

    delete_memory(estate->es_query_cxt);
}

/* ========================================================================
 * ExprContext 结构测试
 * ======================================================================== */

TEST(ExprContextTest, BasicStructure) {
    ExprContext ctx;
    std::memset(&ctx, 0, sizeof(ctx));

    ctx.type = T_ExprContext;
    ctx.ecxt_scantuple = NULL;
    ctx.ecxt_innertuple = NULL;
    ctx.ecxt_outertuple = NULL;
    ctx.ecxt_param_values = NULL;
    ctx.ecxt_param_nulls = NULL;
    ctx.ecxt_param_num = 0;

    EXPECT_EQ(ctx.type, T_ExprContext);
    EXPECT_EQ(ctx.ecxt_scantuple, nullptr);
    EXPECT_EQ(ctx.ecxt_innertuple, nullptr);
    EXPECT_EQ(ctx.ecxt_outertuple, nullptr);
    EXPECT_EQ(ctx.ecxt_param_num, 0);
}

TEST(ExprContextTest, PointerAssignment) {
    /* 验证三个槽字段都可独立赋值并取出 */
    int marker1 = 0, marker2 = 0, marker3 = 0;

    ExprContext ctx;
    std::memset(&ctx, 0, sizeof(ctx));
    ctx.type = T_ExprContext;
    ctx.ecxt_scantuple = reinterpret_cast<TupleTableSlot *>(&marker1);
    ctx.ecxt_innertuple = reinterpret_cast<TupleTableSlot *>(&marker2);
    ctx.ecxt_outertuple = reinterpret_cast<TupleTableSlot *>(&marker3);

    EXPECT_EQ(ctx.ecxt_scantuple, reinterpret_cast<TupleTableSlot *>(&marker1));
    EXPECT_EQ(ctx.ecxt_innertuple, reinterpret_cast<TupleTableSlot *>(&marker2));
    EXPECT_EQ(ctx.ecxt_outertuple, reinterpret_cast<TupleTableSlot *>(&marker3));
}

/* ========================================================================
 * TupleTableSlot 结构测试
 * ======================================================================== */

TEST(TupleTableSlotTest, BasicStructure) {
    TupleTableSlot slot;
    std::memset(&slot, 0, sizeof(slot));

    slot.type = T_TupleTableSlot;
    slot.tts_tupleDescriptor = NULL;
    slot.tts_values = NULL;
    slot.tts_isnull = NULL;
    slot.tts_ops = NULL;
    slot.tts_nvalid = 0;
    slot.tts_shouldFree = false;
    slot.tts_shouldFreeMin = false;
    slot.tts_tuple = NULL;
    slot.tts_tupleLen = 0;
    slot.tts_minTupleLen = 0;

    EXPECT_EQ(slot.type, T_TupleTableSlot);
    EXPECT_EQ(slot.tts_nvalid, 0);
    EXPECT_EQ(slot.tts_values, nullptr);
    EXPECT_EQ(slot.tts_isnull, nullptr);
    EXPECT_FALSE(slot.tts_shouldFree);
}

/* ========================================================================
 * 辅助结构体测试
 * ======================================================================== */

TEST(BitmapsetTest, StructPresence) {
    /* Bitmapset 是柔性数组 header，验证基础字段存在 */
    EXPECT_GE(sizeof(Bitmapset), sizeof(int) + sizeof(unsigned long));
}

TEST(InstrumentationTest, BasicStructure) {
    Instrumentation ins;
    std::memset(&ins, 0, sizeof(ins));
    ins.startup_time = 0.001;
    ins.total_time = 0.5;
    ins.tuples_out = 100;
    ins.buf_hit = 50;
    ins.buf_read = 10;

    EXPECT_EQ(ins.startup_time, 0.001);
    EXPECT_EQ(ins.total_time, 0.5);
    EXPECT_EQ(ins.tuples_out, 100u);
    EXPECT_EQ(ins.buf_hit, 50u);
    EXPECT_EQ(ins.buf_read, 10u);
}

TEST(ForwardScanDirectionTest, Constants) {
    EXPECT_EQ(ForwardScanDirection, 0);
    EXPECT_EQ(BackwardScanDirection, 1);
}
