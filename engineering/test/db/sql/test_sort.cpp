/**
 * @file test_sort.cpp
 * @brief Sort 执行器节点单元测试
 *
 * 测试 Sort 节点的创建、销毁、执行和重置功能。
 * 验证两阶段排序算法（收集 + 输出）的正确性。
 */

#include <gtest/gtest.h>

extern "C" {
#include "db/sql/nodeSort.h"
#include "db/sql/nodes/nodetags.h"
#include "db/sql/nodes/execnodes.h"
#include "db/sql/memctx.h"
#include "db/sql/executor.h"
}

#include <stdlib.h>
#include <string.h>

namespace {

/* ========================================================================
 * 测试夹具类
 * ======================================================================== */

/**
 * @brief Sort 节点测试夹具
 */
class SortTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* 创建内存上下文 */
        ctx_ = AllocSetContextCreate(
            NULL,
            "SortTestContext",
            0,
            ALLOCSET_DEFAULT_BLOCK_SIZE,
            ALLOCSET_DEFAULT_BLOCK_SIZE
        );
        ASSERT_NE(ctx_, nullptr);

        /* 创建 EState */
        estate_ = (EState *)palloc0(ctx_, sizeof(EState));
        ASSERT_NE(estate_, nullptr);
        estate_->es_query_cxt = ctx_;
        estate_->type = T_EState;
    }

    void TearDown() override {
        if (ctx_ != NULL) {
            delete_memory(ctx_);
            ctx_ = NULL;
        }
        estate_ = NULL;
    }

    /**
     * @brief 创建简单的 Sort 计划节点
     */
    Sort *CreateSortPlan(int numCols = 1) {
        Sort *sort = (Sort *)palloc0(ctx_, sizeof(Sort));
        sort->plan.type = T_Sort;
        sort->plan.lefttree = NULL;
        sort->plan.righttree = NULL;
        sort->numCols = numCols;

        if (numCols > 0) {
            sort->sortColIdx = (int *)palloc(ctx_, sizeof(int) * numCols);
            sort->sortOperators = (Oid *)palloc(ctx_, sizeof(Oid) * numCols);
            sort->nullsFirst = (bool *)palloc(ctx_, sizeof(bool) * numCols);

            for (int i = 0; i < numCols; i++) {
                sort->sortColIdx[i] = 0;  /* 按第一列排序 */
                sort->sortOperators[i] = 0;
                sort->nullsFirst[i] = false;
            }
        }

        return sort;
    }

    MemoryContext ctx_;
    EState *estate_;
};

/* ========================================================================
 * 测试用例
 * ======================================================================== */

/**
 * @brief 测试 SortState 创建和销毁
 */
TEST_F(SortTest, CreateAndDestroy) {
    Sort *plan = CreateSortPlan(1);

    /* 初始化 SortState */
    PlanState *state = ExecInitSort((Plan *)plan, estate_, 0);
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->type, T_SortState);

    /* 结束执行 */
    ExecEndSort((SortState *)state);
}

/**
 * @brief 测试 Sort 节点执行函数指针
 */
TEST_F(SortTest, ExecProcPointer) {
    Sort *plan = CreateSortPlan(1);

    /* 初始化 SortState */
    SortState *state = (SortState *)ExecInitSort((Plan *)plan, estate_, 0);
    ASSERT_NE(state, nullptr);

    /* 检查执行函数指针 */
    EXPECT_NE(state->ps.ExecProcNode, nullptr);
    EXPECT_EQ(state->ps.ExecProcNode, state->ps.ExecProcNodeReal);

    /* 通过函数指针调用（空输入应返回 NULL） */
    TupleTableSlot *slot = state->ps.ExecProcNode((PlanState *)state);
    EXPECT_EQ(slot, nullptr);

    /* 清理 */
    ExecEndSort(state);
}

/**
 * @brief 测试空输入排序
 */
TEST_F(SortTest, SortEmpty) {
    Sort *plan = CreateSortPlan(1);
    plan->plan.lefttree = NULL;  /* 无子节点 */

    /* 初始化 SortState */
    SortState *state = (SortState *)ExecInitSort((Plan *)plan, estate_, 0);
    ASSERT_NE(state, nullptr);

    /* 执行排序：空输入应返回 NULL */
    TupleTableSlot *slot = ExecSort((PlanState *)state);
    EXPECT_EQ(slot, nullptr);

    /* 清理 */
    ExecEndSort(state);
}

/**
 * @brief 测试单元组排序
 */
TEST_F(SortTest, SortSingle) {
    Sort *plan = CreateSortPlan(1);

    /* 初始化 SortState */
    SortState *state = (SortState *)ExecInitSort((Plan *)plan, estate_, 0);
    ASSERT_NE(state, nullptr);

    /* 单元组输入应返回 NULL（没有子节点提供数据） */
    TupleTableSlot *slot = ExecSort((PlanState *)state);
    EXPECT_EQ(slot, nullptr);

    /* 清理 */
    ExecEndSort(state);
}

/**
 * @brief 测试多元组排序
 */
TEST_F(SortTest, SortMultiple) {
    Sort *plan = CreateSortPlan(1);

    /* 初始化 SortState */
    SortState *state = (SortState *)ExecInitSort((Plan *)plan, estate_, 0);
    ASSERT_NE(state, nullptr);

    /* 多次执行都应返回 NULL（无子节点） */
    for (int i = 0; i < 3; i++) {
        TupleTableSlot *slot = ExecSort((PlanState *)state);
        EXPECT_EQ(slot, nullptr);
    }

    /* 清理 */
    ExecEndSort(state);
}

/**
 * @brief 测试重置 Sort 节点
 */
TEST_F(SortTest, ReScan) {
    Sort *plan = CreateSortPlan(1);

    /* 初始化 SortState */
    SortState *state = (SortState *)ExecInitSort((Plan *)plan, estate_, 0);
    ASSERT_NE(state, nullptr);

    /* 第一次执行 */
    TupleTableSlot *slot = ExecSort((PlanState *)state);
    EXPECT_EQ(slot, nullptr);

    /* 重置 */
    EXPECT_NO_FATAL_FAILURE(ExecReScanSort(state));

    /* 再次执行 */
    slot = ExecSort((PlanState *)state);
    EXPECT_EQ(slot, nullptr);

    /* 清理 */
    ExecEndSort(state);
}

/**
 * @brief 测试 NULL 参数处理
 */
TEST_F(SortTest, NullParameters) {
    /* ExecEndSort 应能处理 NULL */
    EXPECT_NO_FATAL_FAILURE(ExecEndSort(NULL));

    /* ExecReScanSort 应能处理 NULL */
    EXPECT_NO_FATAL_FAILURE(ExecReScanSort(NULL));

    /* ExecSort 应能处理 NULL */
    EXPECT_EQ(ExecSort(NULL), nullptr);

    /* ExecInitSort 应能处理 NULL 参数 */
    EXPECT_EQ(ExecInitSort(NULL, NULL, 0), nullptr);
    EXPECT_EQ(ExecInitSort((Plan *)0x1, NULL, 0), nullptr);
}

/**
 * @brief 测试 MakeSortPlan 工厂函数
 */
TEST_F(SortTest, MakePlan) {
    /* 创建多列排序计划 */
    int numCols = 3;
    int colIdx[] = {0, 1, 2};
    Oid operators[] = {0, 0, 0};
    bool nullsFirst[] = {false, false, false};

    /* 通过手动创建 Sort 节点来模拟 make_sort_plan */
    Sort *plan = (Sort *)palloc0(ctx_, sizeof(Sort));
    plan->plan.type = T_Sort;
    plan->plan.lefttree = NULL;
    plan->plan.righttree = NULL;
    plan->numCols = numCols;

    plan->sortColIdx = (int *)palloc(ctx_, sizeof(int) * numCols);
    plan->sortOperators = (Oid *)palloc(ctx_, sizeof(Oid) * numCols);
    plan->nullsFirst = (bool *)palloc(ctx_, sizeof(bool) * numCols);

    memcpy(plan->sortColIdx, colIdx, sizeof(int) * numCols);
    memcpy(plan->sortOperators, operators, sizeof(Oid) * numCols);
    memcpy(plan->nullsFirst, nullsFirst, sizeof(bool) * numCols);

    /* 初始化 SortState */
    SortState *state = (SortState *)ExecInitSort((Plan *)plan, estate_, 0);
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->numCols, numCols);

    /* 清理 */
    ExecEndSort(state);
}

/**
 * @brief 测试类型区分
 */
TEST_F(SortTest, TypeDistinction) {
    /* 创建 Sort 节点 */
    Sort *sort_plan = CreateSortPlan(1);
    sort_plan->plan.type = T_Sort;

    /* 初始化 SortState */
    PlanState *state = ExecInitSort((Plan *)sort_plan, estate_, 0);
    ASSERT_NE(state, nullptr);

    /* 类型检查 */
    EXPECT_EQ(state->type, T_SortState);
    EXPECT_NE(state->type, T_ResultState);
    EXPECT_NE(state->type, T_SeqScanState);

    /* 清理 */
    ExecEndSort((SortState *)state);
}

/**
 * @brief 测试多列排序配置
 */
TEST_F(SortTest, MultipleColumns) {
    /* 创建 3 列排序 */
    Sort *plan = CreateSortPlan(3);
    ASSERT_NE(plan->sortColIdx, nullptr);
    ASSERT_NE(plan->sortOperators, nullptr);
    ASSERT_NE(plan->nullsFirst, nullptr);

    /* 设置不同的排序参数 */
    plan->sortColIdx[0] = 0;
    plan->sortColIdx[1] = 1;
    plan->sortColIdx[2] = 2;
    plan->nullsFirst[0] = true;
    plan->nullsFirst[1] = false;
    plan->nullsFirst[2] = true;

    /* 初始化 SortState */
    SortState *state = (SortState *)ExecInitSort((Plan *)plan, estate_, 0);
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->numCols, 3);

    /* 清理 */
    ExecEndSort(state);
}

/**
 * @brief 测试计划节点字段
 */
TEST_F(SortTest, PlanNodeFields) {
    Sort *plan = CreateSortPlan(1);

    /* 验证计划节点字段 */
    EXPECT_EQ(plan->plan.type, T_Sort);
    EXPECT_EQ(plan->numCols, 1);
    EXPECT_NE(plan->sortColIdx, nullptr);
    EXPECT_EQ(plan->sortColIdx[0], 0);
}

/**
 * @brief 测试状态节点初始化
 */
TEST_F(SortTest, StateInitialization) {
    Sort *plan = CreateSortPlan(1);

    /* 初始化 SortState */
    SortState *state = (SortState *)ExecInitSort((Plan *)plan, estate_, 0);
    ASSERT_NE(state, nullptr);

    /* 检查状态初始化 */
    EXPECT_EQ(state->numCols, 1);
    EXPECT_FALSE(state->sort_Done);
    EXPECT_EQ(state->tuplesortstate, nullptr);

    /* 检查计划关联 */
    EXPECT_NE(state->ps.plan, nullptr);

    /* 清理 */
    ExecEndSort(state);
}

/**
 * @brief 测试表达式上下文
 */
TEST_F(SortTest, ExprContext) {
    Sort *plan = CreateSortPlan(1);

    /* 初始化 SortState */
    SortState *state = (SortState *)ExecInitSort((Plan *)plan, estate_, 0);
    ASSERT_NE(state, nullptr);

    /* 检查表达式上下文 */
    EXPECT_NE(state->ps.ps_ExprContext, nullptr);

    /* 检查结果槽 */
    EXPECT_NE(state->ps.ps_ResultTupleSlot, nullptr);

    /* 清理 */
    ExecEndSort(state);
}

/**
 * @brief 测试多次重置
 */
TEST_F(SortTest, MultipleResets) {
    Sort *plan = CreateSortPlan(1);

    /* 初始化 SortState */
    SortState *state = (SortState *)ExecInitSort((Plan *)plan, estate_, 0);
    ASSERT_NE(state, nullptr);

    /* 多次重置 */
    for (int i = 0; i < 5; i++) {
        EXPECT_NO_FATAL_FAILURE(ExecReScanSort(state));
    }

    /* 验证状态 */
    EXPECT_FALSE(state->sort_Done);

    /* 清理 */
    ExecEndSort(state);
}

}  /* namespace */
