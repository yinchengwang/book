/**
 * @file test_executor.cpp
 * @brief SQL 执行器单元测试
 */

#include <gtest/gtest.h>
extern "C" {
#include "db/sql/sql_executor.h"
#include "db/sql/sql_planner.h"
}

namespace {

/**
 * @brief 测试元组槽创建和销毁
 */
TEST(TupleSlotTest, CreateAndDestroy) {
    /* 创建元组描述符 */
    ExecTupleDesc *desc = exec_make_tuple_desc(3);
    ASSERT_NE(desc, nullptr);
    EXPECT_EQ(desc->natts, 3);

    /* 创建元组槽 */
    TupleTableSlot *slot = exec_make_tuple_slot(desc);
    ASSERT_NE(slot, nullptr);
    EXPECT_EQ(slot->tts_nvalid, 0);

    /* 清理 */
    exec_drop_tuple_slot(slot);
    exec_drop_tuple_desc(desc);
}

/**
 * @brief 测试元组槽值操作
 */
TEST(TupleSlotTest, ValueOperations) {
    ExecTupleDesc *desc = exec_make_tuple_desc(2);
    ASSERT_NE(desc, nullptr);

    TupleTableSlot *slot = exec_make_tuple_slot(desc);
    ASSERT_NE(slot, nullptr);

    /* 设置值 */
    int val1 = 42;
    double val2 = 3.14;

    slot_set_value(slot, 1, &val1, sizeof(int));
    slot_set_value(slot, 2, &val2, sizeof(double));

    EXPECT_EQ(slot->tts_nvalid, 2);
    EXPECT_FALSE(slot_attisnull(slot, 1));
    EXPECT_FALSE(slot_attisnull(slot, 2));

    /* 获取值 */
    int *read_val1 = (int *)slot_get_value(slot, 1, NULL);
    EXPECT_EQ(*read_val1, 42);

    /* 设置 NULL */
    slot_set_null(slot, 1);
    EXPECT_TRUE(slot_attisnull(slot, 1));

    /* 清理 */
    exec_drop_tuple_slot(slot);
    exec_drop_tuple_desc(desc);
}

/**
 * @brief 测试元组槽复制
 */
TEST(TupleSlotTest, Copy) {
    ExecTupleDesc *desc = exec_make_tuple_desc(2);
    ASSERT_NE(desc, nullptr);

    TupleTableSlot *src = exec_make_tuple_slot(desc);
    ASSERT_NE(src, nullptr);

    int val = 100;
    slot_set_value(src, 1, &val, sizeof(int));

    TupleTableSlot *dst = exec_copy_tuple_slot(src);
    ASSERT_NE(dst, nullptr);
    EXPECT_EQ(dst->tts_nvalid, src->tts_nvalid);

    /* 清理 */
    exec_drop_tuple_slot(src);
    exec_drop_tuple_slot(dst);
    exec_drop_tuple_desc(desc);
}

/**
 * @brief 测试元组槽清空
 */
TEST(TupleSlotTest, Clear) {
    ExecTupleDesc *desc = exec_make_tuple_desc(2);
    ASSERT_NE(desc, nullptr);

    TupleTableSlot *slot = exec_make_tuple_slot(desc);
    ASSERT_NE(slot, nullptr);

    int val = 50;
    slot_set_value(slot, 1, &val, sizeof(int));
    EXPECT_EQ(slot->tts_nvalid, 1);

    exec_clear_tuple_slot(slot);
    EXPECT_EQ(slot->tts_nvalid, 0);

    /* 清理 */
    exec_drop_tuple_slot(slot);
    exec_drop_tuple_desc(desc);
}

/**
 * @brief 测试表达式上下文
 */
TEST(ExprContextTest, CreateAndDestroy) {
    ExprContext *ctx = exec_create_expr_context();
    ASSERT_NE(ctx, nullptr);
    EXPECT_EQ(ctx->numscans, 0);

    exec_destroy_expr_context(ctx);
}

/**
 * @brief 测试执行器生命周期
 */
TEST(ExecutorTest, Lifecycle) {
    /* 创建执行器 */
    void *exec = executor_create();
    ASSERT_NE(exec, nullptr);

    /* 初始化 */
    executor_init(exec, NULL, NULL);

    /* 运行（空计划） */
    long count = executor_run(exec, NULL, FORWARD, 10, NULL);
    EXPECT_EQ(count, 0);

    /* 结束 */
    executor_finish(exec);

    /* 销毁 */
    executor_destroy(exec);
}

/**
 * @brief 测试执行器创建和销毁
 */
TEST(ExecutorTest, CreateAndDestroy) {
    void *exec = executor_create();
    ASSERT_NE(exec, nullptr);

    executor_destroy(exec);
}

/**
 * @brief 测试执行器开始和继续
 */
TEST(ExecutorTest, StartAndContinue) {
    void *exec = executor_create();
    ASSERT_NE(exec, nullptr);

    executor_start(exec, NULL, NULL);

    bool has_more = executor_continue(exec);
    EXPECT_FALSE(has_more);

    executor_destroy(exec);
}

/**
 * @brief 测试执行器取消
 */
TEST(ExecutorTest, Cancel) {
    void *exec = executor_create();
    ASSERT_NE(exec, nullptr);

    executor_cancel(exec);
    executor_destroy(exec);
}

/**
 * @brief 测试结果接收器
 */
TEST(DestReceiverTest, CreateAndDestroy) {
    DestReceiver *recv = CreateDestReceiver(DEST_TUPLES);
    ASSERT_NE(recv, nullptr);
    EXPECT_EQ(recv->dest, DEST_TUPLES);

    DestroyDestReceiver(recv);
}

/**
 * @brief 测试结果接收器接收元组
 */
TEST(DestReceiverTest, ReceiveTuple) {
    DestReceiver *recv = CreateDestReceiver(DEST_TUPLES);
    ASSERT_NE(recv, nullptr);

    ExecTupleDesc *desc = exec_make_tuple_desc(2);
    TupleTableSlot *slot = exec_make_tuple_slot(desc);

    /* 接收元组（不会崩溃） */
    ReceiveTuple(recv, slot);

    /* 清理 */
    exec_drop_tuple_slot(slot);
    exec_drop_tuple_desc(desc);
    DestroyDestReceiver(recv);
}

/**
 * @brief 测试执行器状态打印
 */
TEST(DebugTest, DumpState) {
    PlanState state;
    memset(&state, 0, sizeof(state));
    state.type = EXEC_SEQ_SCAN;

    /* 不崩溃 */
    EXPECT_NO_FATAL_FAILURE(executor_dump_state(&state));
}

/**
 * @brief 测试打印元组
 */
TEST(DebugTest, PrintTuple) {
    ExecTupleDesc *desc = exec_make_tuple_desc(2);
    TupleTableSlot *slot = exec_make_tuple_slot(desc);

    int val = 123;
    slot_set_value(slot, 1, &val, sizeof(int));

    /* 不崩溃 */
    EXPECT_NO_FATAL_FAILURE(print_tuple(slot));

    /* 打印 NULL 元组 */
    EXPECT_NO_FATAL_FAILURE(print_tuple(NULL));

    /* 清理 */
    exec_drop_tuple_slot(slot);
    exec_drop_tuple_desc(desc);
}

/**
 * @brief 测试元组比较
 */
TEST(UtilTest, TupleCompare) {
    int result = tuple_compare(NULL, NULL, NULL);
    EXPECT_EQ(result, 0);
}

/**
 * @brief 测试元组内存操作
 */
TEST(UtilTest, TupleMemory) {
    int src = 42;

    void *copy = tuple_copy(&src, sizeof(int));
    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(*(int *)copy, 42);
    free(copy);

    void *alloc = tuple_alloc(100);
    ASSERT_NE(alloc, nullptr);
    tuple_free(alloc);
}

/**
 * @brief 测试元组描述符缓存
 */
TEST(UtilTest, CachedRowtype) {
    ExecTupleDesc *desc = get_cached_rowtype(0, 0);
    EXPECT_EQ(desc, nullptr);  /* 简化实现返回 NULL */

    /* 释放 NULL 不应崩溃 */
    ReleaseCachedRowtype(NULL);
}

/**
 * @brief 测试元组存储
 */
TEST(TupleStoreTest, Basic) {
    void *store = CreateTupleStore();
    EXPECT_EQ(store, nullptr);  /* 简化实现返回 NULL */

    TupleTableSlot *slot = tuple_store_get(store, true);
    EXPECT_EQ(slot, nullptr);
}

/* Task 1.1 重构说明：
 * 原测试 EvalTest.Basic/ConstExpr/Func/Agg/WindowFunc
 * 仅验证 eval_expr/eval_const_expr/eval_func/eval_agg/eval_window_func
 * 桩函数返回 NULL。这些桩函数已删除（实际求值由 executor 框架
 * 通过 PlanState->ExecProcNode 调度），相应测试已删除。
 * 保留 eval_case/eval_in/eval_nulltest/eval_bool_expr 的桩测试。 */

/**
 * @brief 测试 CASE 表达式求值
 */
TEST(EvalTest, Case) {
    ExprContext *ctx = exec_create_expr_context();
    ASSERT_NE(ctx, nullptr);

    void *result = eval_case(NULL, ctx);
    EXPECT_EQ(result, nullptr);

    exec_destroy_expr_context(ctx);
}

/**
 * @brief 测试 IN 表达式求值
 */
TEST(EvalTest, In) {
    bool result = eval_in(NULL, NULL, NULL);
    EXPECT_FALSE(result);
}

/**
 * @brief 测试 NULL 测试求值
 */
TEST(EvalTest, NullTest) {
    bool result = eval_nulltest(NULL, false, NULL);
    EXPECT_FALSE(result);
}

/**
 * @brief 测试布尔表达式求值
 */
TEST(EvalTest, BoolExpr) {
    bool result = eval_bool_expr(NULL, NULL);
    EXPECT_TRUE(result);  /* 简化实现返回 true */
}

/* Task 1.1 重构说明：
 * 原测试 ScanOps/JoinOps/AggOps/ModifyOps/ChangeOps/ResultOps
 * 仅验证桩函数返回 NULL，桩函数已删除，相应测试已迁移到
 * nodeXxx.c 的真实实现测试中。此处仅保留 index_scan 桩测试。 */

/**
 * @brief 测试索引扫描执行函数（真实实现位于 nodeIndexscan.c）
 */
TEST(ExecOpsTest, IndexScanOps) {
    IndexScanState idx_node;
    memset(&idx_node, 0, sizeof(idx_node));

    TupleTableSlot *slot = ExecIndexScan(&idx_node);
    EXPECT_EQ(slot, nullptr);
}

}  /* namespace */
