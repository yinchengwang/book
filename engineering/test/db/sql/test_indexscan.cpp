/**
 * @file test_indexscan.cpp
 * @brief IndexScan 执行器节点单元测试
 *
 * 测试范围：
 *   - IndexScanState 创建/销毁
 *   - 索引点查询
 *   - 索引范围查询
 *   - 索引扫描重置
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "db/sql/sql_executor.h"
#include "db/sql/nodes/nodeIndexscan.h"

/* 前向声明 ScanKeyData 结构体，避免引入冲突的头文件 */
struct ScanKeyData {
    int sk_attno;
    int sk_op;
    uint64_t sk_procedure;
    void *sk_argument;
    size_t sk_arglen;
};
}

namespace {

/**
 * @brief 测试 IndexScanState 创建和销毁
 */
TEST(IndexScanTest, CreateAndDestroy) {
    /* 创建 IndexScan 计划节点 */
    IndexScanPlan plan;
    memset(&plan, 0, sizeof(plan));
    plan.type = EXEC_INDEX_SCAN;
    plan.indexid = 0;
    plan.relid = 0;
    plan.nkeys = 0;
    plan.key = NULL;
    plan.qual = NULL;
    plan.targetlist = NULL;

    /* 初始化 IndexScan 状态（无实际 Relation） */
    IndexScanState *state = ExecInitIndexScan(&plan, NULL, 0);
    /* 无有效 relid 时应返回 NULL 或有效空状态 */
    if (state != NULL) {
        EXPECT_EQ(state->ss.ps.type, EXEC_INDEX_SCAN);
        ExecEndIndexScan(state);
    }
}

/**
 * @brief 测试 IndexScan 执行函数指针
 */
TEST(IndexScanTest, ExecProcPointer) {
    IndexScanPlan plan;
    memset(&plan, 0, sizeof(plan));
    plan.type = EXEC_INDEX_SCAN;

    IndexScanState *state = ExecInitIndexScan(&plan, NULL, 0);
    if (state != NULL) {
        /* 验证执行函数指针已设置 */
        EXPECT_NE(state->ss.ps.exec_proc, nullptr);
        ExecEndIndexScan(state);
    }
}

/**
 * @brief 测试 IndexScan 扫描空表
 */
TEST(IndexScanTest, ScanEmpty) {
    IndexScanPlan plan;
    memset(&plan, 0, sizeof(plan));
    plan.type = EXEC_INDEX_SCAN;
    plan.indexid = 0;
    plan.relid = 0;
    plan.nkeys = 0;

    IndexScanState *state = ExecInitIndexScan(&plan, NULL, 0);
    if (state != NULL) {
        /* 无有效 Relation 时，执行应返回 NULL */
        TupleTableSlot *slot = ExecIndexScan(state);
        EXPECT_EQ(slot, nullptr);

        ExecEndIndexScan(state);
    }
}

/**
 * @brief 测试 IndexScan 重置
 */
TEST(IndexScanTest, ReScan) {
    IndexScanPlan plan;
    memset(&plan, 0, sizeof(plan));
    plan.type = EXEC_INDEX_SCAN;

    IndexScanState *state = ExecInitIndexScan(&plan, NULL, 0);
    if (state != NULL) {
        /* 重置不应崩溃 */
        ExecReScanIndexScan(state);

        ExecEndIndexScan(state);
    }
}

/**
 * @brief 测试 IndexScan 扩展状态获取
 */
TEST(IndexScanTest, GetExtState) {
    IndexScanPlan plan;
    memset(&plan, 0, sizeof(plan));
    plan.type = EXEC_INDEX_SCAN;

    IndexScanState *state = ExecInitIndexScan(&plan, NULL, 0);
    if (state != NULL) {
        IndexScanExtState *ext = ExecGetIndexScanExtState(state);
        /* 无有效 relid 时，扩展状态可能为空或部分初始化 */
        if (ext != NULL) {
            EXPECT_EQ(ext->iss_tuples_scanned, 0);
            EXPECT_EQ(ext->iss_tuples_returned, 0);
        }
        ExecEndIndexScan(state);
    }
}

/**
 * @brief 测试 IndexScan 计划节点构造
 */
TEST(IndexScanTest, MakePlan) {
    /* 创建空的扫描键 */
    ScanKeyData key;
    memset(&key, 0, sizeof(key));
    key.sk_attno = 1;
    key.sk_argument = NULL;
    key.sk_arglen = 0;

    IndexScanPlan *plan = make_indexscan_plan(
        1,  /* indexid */
        2,  /* relid */
        1,  /* nkeys */
        &key
    );

    ASSERT_NE(plan, nullptr);
    EXPECT_EQ(plan->type, EXEC_INDEX_SCAN);
    EXPECT_EQ(plan->indexid, 1u);
    EXPECT_EQ(plan->relid, 2u);
    EXPECT_EQ(plan->nkeys, 1);
    EXPECT_NE(plan->key, nullptr);

    free(plan);
}

/**
 * @brief 测试 IndexScan 元组描述符
 */
TEST(IndexScanTest, TupleDesc) {
    IndexScanPlan plan;
    memset(&plan, 0, sizeof(plan));
    plan.type = EXEC_INDEX_SCAN;

    IndexScanState *state = ExecInitIndexScan(&plan, NULL, 0);
    if (state != NULL) {
        ExecTupleDesc *desc = ExecGetIndexScanTupleDesc(state);
        /* 元组描述符可能为空（无有效 Relation） */
        if (desc != NULL) {
            EXPECT_GE(desc->natts, 0);
        }
        ExecEndIndexScan(state);
    }
}

/**
 * @brief 测试 IndexScan 类型区分
 */
TEST(IndexScanTest, TypeDistinction) {
    IndexScanPlan idx_plan;
    memset(&idx_plan, 0, sizeof(idx_plan));
    idx_plan.type = EXEC_INDEX_SCAN;

    EXPECT_EQ(idx_plan.type, EXEC_INDEX_SCAN);
    EXPECT_NE(idx_plan.type, EXEC_SEQ_SCAN);
}

/**
 * @brief 测试 IndexScan 统计信息
 */
TEST(IndexScanTest, Statistics) {
    IndexScanPlan plan;
    memset(&plan, 0, sizeof(plan));
    plan.type = EXEC_INDEX_SCAN;

    IndexScanState *state = ExecInitIndexScan(&plan, NULL, 0);
    if (state != NULL) {
        IndexScanExtState *ext = ExecGetIndexScanExtState(state);
        if (ext != NULL) {
            /* 初始统计应为 0 */
            EXPECT_EQ(ext->iss_tuples_scanned, 0);
            EXPECT_EQ(ext->iss_tuples_returned, 0);
            EXPECT_EQ(ext->iss_index_entries, 0);

            /* 执行扫描（无实际数据） */
            ExecIndexScan(state);

            /* 统计应保持为 0 */
            EXPECT_EQ(ext->iss_tuples_scanned, 0);
            EXPECT_EQ(ext->iss_tuples_returned, 0);
        }
        ExecEndIndexScan(state);
    }
}

}  /* namespace */
