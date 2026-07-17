/**
 * @file test_seqscan.cpp
 * @brief SeqScan 执行器节点单元测试
 */

#include <gtest/gtest.h>
extern "C" {
#include "db/sql/nodes/nodeSeqscan.h"
#include "db/sql/sql_executor.h"
}

namespace {

/* 从 catalog.h 复制的常量，避免引入冲突的头文件 */
#define TEST_INVALID_OID ((uint32_t)0)

/**
 * @brief 测试 SeqScanState 创建和销毁
 */
TEST(SeqScanTest, CreateAndDestroy) {
    /* 创建计划节点 */
    SeqScanPlan plan;
    memset(&plan, 0, sizeof(plan));
    plan.type = EXEC_SEQ_SCAN;
    plan.scanrelid = TEST_INVALID_OID;  /* 暂不关联真实表 */
    plan.qual = NULL;
    plan.targetlist = NULL;

    /* 初始化执行状态 */
    SeqScanState *state = ExecInitSeqScan(&plan, NULL, 0);
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->ss.ps.type, EXEC_SEQ_SCAN);

    SeqScanExtState *ext_state = ExecGetSeqScanExtState(state);
    ASSERT_NE(ext_state, nullptr);
    EXPECT_FALSE(ext_state->ss_scan_started);
    EXPECT_FALSE(ext_state->ss_scan_ended);

    /* 结束执行 */
    ExecEndSeqScan(state);
}

/**
 * @brief 测试 SeqScan 执行（无 Relation）
 */
TEST(SeqScanTest, ExecuteWithoutRelation) {
    /* 创建计划节点 */
    SeqScanPlan plan;
    memset(&plan, 0, sizeof(plan));
    plan.type = EXEC_SEQ_SCAN;
    plan.scanrelid = TEST_INVALID_OID;

    /* 初始化执行状态 */
    SeqScanState *state = ExecInitSeqScan(&plan, NULL, 0);
    ASSERT_NE(state, nullptr);

    SeqScanExtState *ext_state = ExecGetSeqScanExtState(state);
    ASSERT_NE(ext_state, nullptr);

    /* 执行扫描：没有 Relation，应立即返回 NULL */
    TupleTableSlot *slot = ExecSeqScan((PlanState *)state);
    EXPECT_EQ(slot, nullptr);
    EXPECT_TRUE(ext_state->ss_scan_ended);

    /* 清理 */
    ExecEndSeqScan(state);
}

/**
 * @brief 测试 SeqScan 多次迭代
 */
TEST(SeqScanTest, MultipleIterations) {
    /* 创建计划节点 */
    SeqScanPlan plan;
    memset(&plan, 0, sizeof(plan));
    plan.type = EXEC_SEQ_SCAN;
    plan.scanrelid = TEST_INVALID_OID;

    /* 初始化执行状态 */
    SeqScanState *state = ExecInitSeqScan(&plan, NULL, 0);
    ASSERT_NE(state, nullptr);

    /* 多次执行，都应返回 NULL（没有 Relation） */
    for (int i = 0; i < 3; i++) {
        TupleTableSlot *slot = ExecSeqScan((PlanState *)state);
        EXPECT_EQ(slot, nullptr);
    }

    /* 清理 */
    ExecEndSeqScan(state);
}

/**
 * @brief 测试 SeqScan 重置
 */
TEST(SeqScanTest, Reset) {
    /* 创建计划节点 */
    SeqScanPlan plan;
    memset(&plan, 0, sizeof(plan));
    plan.type = EXEC_SEQ_SCAN;
    plan.scanrelid = TEST_INVALID_OID;

    /* 初始化执行状态 */
    SeqScanState *state = ExecInitSeqScan(&plan, NULL, 0);
    ASSERT_NE(state, nullptr);

    SeqScanExtState *ext_state = ExecGetSeqScanExtState(state);
    ASSERT_NE(ext_state, nullptr);

    /* 执行扫描 */
    TupleTableSlot *slot = ExecSeqScan((PlanState *)state);
    EXPECT_EQ(slot, nullptr);
    EXPECT_TRUE(ext_state->ss_scan_ended);

    /* 重置 */
    ExecReScanSeqScan(state);
    EXPECT_FALSE(ext_state->ss_scan_started);
    EXPECT_FALSE(ext_state->ss_scan_ended);
    EXPECT_EQ(ext_state->ss_tuples_scanned, 0);

    /* 清理 */
    ExecEndSeqScan(state);
}

/**
 * @brief 测试 SeqScan 统计信息
 */
TEST(SeqScanTest, Statistics) {
    /* 创建计划节点 */
    SeqScanPlan plan;
    memset(&plan, 0, sizeof(plan));
    plan.type = EXEC_SEQ_SCAN;
    plan.scanrelid = TEST_INVALID_OID;

    /* 初始化执行状态 */
    SeqScanState *state = ExecInitSeqScan(&plan, NULL, 0);
    ASSERT_NE(state, nullptr);

    SeqScanExtState *ext_state = ExecGetSeqScanExtState(state);
    ASSERT_NE(ext_state, nullptr);

    /* 初始统计信息应为 0 */
    EXPECT_EQ(ext_state->ss_tuples_scanned, 0);
    EXPECT_EQ(ext_state->ss_tuples_returned, 0);

    /* 执行扫描 */
    ExecSeqScan((PlanState *)state);

    /* 清理 */
    ExecEndSeqScan(state);
}

/**
 * @brief 测试 SeqScan 元组描述符
 */
TEST(SeqScanTest, TupleDescriptor) {
    /* 创建计划节点 */
    SeqScanPlan plan;
    memset(&plan, 0, sizeof(plan));
    plan.type = EXEC_SEQ_SCAN;
    plan.scanrelid = TEST_INVALID_OID;

    /* 初始化执行状态 */
    SeqScanState *state = ExecInitSeqScan(&plan, NULL, 0);
    ASSERT_NE(state, nullptr);

    /* 获取元组描述符 */
    ExecTupleDesc *desc = ExecGetSeqScanTupleDesc(state);
    ASSERT_NE(desc, nullptr);
    EXPECT_EQ(desc->natts, 2);  /* 简化实现使用 2 列 */

    /* 清理 */
    ExecEndSeqScan(state);
}

/**
 * @brief 测试 SeqScan 表达式上下文
 */
TEST(SeqScanTest, ExprContext) {
    /* 创建计划节点 */
    SeqScanPlan plan;
    memset(&plan, 0, sizeof(plan));
    plan.type = EXEC_SEQ_SCAN;
    plan.scanrelid = TEST_INVALID_OID;

    /* 初始化执行状态 */
    SeqScanState *state = ExecInitSeqScan(&plan, NULL, 0);
    ASSERT_NE(state, nullptr);

    /* 检查表达式上下文 */
    ASSERT_NE(state->ss.ps.expr_context, nullptr);
    ASSERT_NE(state->ss.ps.expr_context->slot, nullptr);

    /* 清理 */
    ExecEndSeqScan(state);
}

/**
 * @brief 测试 NULL 参数处理
 */
TEST(SeqScanTest, NullParameters) {
    /* ExecEndSeqScan 应能处理 NULL */
    EXPECT_NO_FATAL_FAILURE(ExecEndSeqScan(NULL));

    /* ExecReScanSeqScan 应能处理 NULL */
    EXPECT_NO_FATAL_FAILURE(ExecReScanSeqScan(NULL));

    /* ExecSeqScan 应能处理 NULL */
    EXPECT_EQ(ExecSeqScan(NULL), nullptr);

    /* ExecGetSeqScanExtState 应能处理 NULL */
    EXPECT_EQ(ExecGetSeqScanExtState(NULL), nullptr);

    /* ExecGetSeqScanTupleDesc 应能处理 NULL */
    EXPECT_EQ(ExecGetSeqScanTupleDesc(NULL), nullptr);
}

/**
 * @brief 测试无效 OID 处理
 */
TEST(SeqScanTest, InvalidOidTest) {
    SeqScanPlan plan;
    memset(&plan, 0, sizeof(plan));
    plan.type = EXEC_SEQ_SCAN;
    plan.scanrelid = TEST_INVALID_OID;  /* 无效 OID */

    /* 初始化应成功（不打开 Relation） */
    SeqScanState *state = ExecInitSeqScan(&plan, NULL, 0);
    ASSERT_NE(state, nullptr);

    SeqScanExtState *ext_state = ExecGetSeqScanExtState(state);
    ASSERT_NE(ext_state, nullptr);
    EXPECT_EQ(ext_state->ss_currentRelation, nullptr);

    /* 清理 */
    ExecEndSeqScan(state);
}

/**
 * @brief 测试执行函数指针
 */
TEST(SeqScanTest, ExecProcPointer) {
    SeqScanPlan plan;
    memset(&plan, 0, sizeof(plan));
    plan.type = EXEC_SEQ_SCAN;
    plan.scanrelid = TEST_INVALID_OID;

    SeqScanState *state = ExecInitSeqScan(&plan, NULL, 0);
    ASSERT_NE(state, nullptr);

    /* 检查执行函数指针 */
    EXPECT_EQ(state->ss.ps.exec_proc, ExecSeqScan);

    /* 通过函数指针调用 */
    TupleTableSlot *slot = state->ss.ps.exec_proc((PlanState *)state);
    EXPECT_EQ(slot, nullptr);

    /* 清理 */
    ExecEndSeqScan(state);
}

/**
 * @brief 测试计划节点字段
 */
TEST(SeqScanTest, PlanNodeFields) {
    SeqScanPlan plan;
    memset(&plan, 0, sizeof(plan));
    plan.type = EXEC_SEQ_SCAN;
    plan.scanrelid = 0;  /* 无效 OID，不关联真实表 */
    plan.qual = NULL;
    plan.targetlist = NULL;

    SeqScanState *state = ExecInitSeqScan(&plan, NULL, 0);
    ASSERT_NE(state, nullptr);

    SeqScanExtState *ext_state = ExecGetSeqScanExtState(state);
    ASSERT_NE(ext_state, nullptr);

    /* 检查计划信息被正确复制 */
    EXPECT_EQ(ext_state->ss_qual, nullptr);
    EXPECT_EQ(ext_state->ss_targetlist, nullptr);

    /* 清理 */
    ExecEndSeqScan(state);
}

/**
 * @brief 测试扫描状态转换
 */
TEST(SeqScanTest, ScanStateTransition) {
    SeqScanPlan plan;
    memset(&plan, 0, sizeof(plan));
    plan.type = EXEC_SEQ_SCAN;
    plan.scanrelid = TEST_INVALID_OID;

    SeqScanState *state = ExecInitSeqScan(&plan, NULL, 0);
    ASSERT_NE(state, nullptr);

    SeqScanExtState *ext_state = ExecGetSeqScanExtState(state);
    ASSERT_NE(ext_state, nullptr);

    /* 初始状态：未开始 */
    EXPECT_FALSE(ext_state->ss_scan_started);
    EXPECT_FALSE(ext_state->ss_scan_ended);

    /* 第一次执行：开始扫描 */
    ExecSeqScan((PlanState *)state);
    EXPECT_TRUE(ext_state->ss_scan_started);
    EXPECT_TRUE(ext_state->ss_scan_ended);

    /* 再次执行：仍然结束 */
    ExecSeqScan((PlanState *)state);
    EXPECT_TRUE(ext_state->ss_scan_started);
    EXPECT_TRUE(ext_state->ss_scan_ended);

    /* 清理 */
    ExecEndSeqScan(state);
}

}  /* namespace */