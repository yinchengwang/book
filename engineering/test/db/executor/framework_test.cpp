/**
 * @file framework_test.cpp
 * @brief Executor framework 单元测试
 */

#include <gtest/gtest.h>

extern "C" {
#include "db/executor/executor_framework.h"
#include "db/core/columnar_store.h"
#include "db/vectorized/vectorized.h"
#include "db/executor/exec_operators.h"
}

#include <cstring>
#include <cstdlib>

class ExecutorFrameworkTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ExecutorFrameworkTest, NullPlan) {
    ExecNode *root = exec_create(NULL);
    EXPECT_EQ(root, nullptr);
}

TEST_F(ExecutorFrameworkTest, SeqScanBasic) {
    int32_t col0[] = {1, 2, 3, 4, 5};
    int col_types[] = {COLUMN_INT32};
    void *col_data[] = {col0};
    int col_elem_size[] = {sizeof(int32_t)};

    ExecNode *root = exec_create_seqscan(0, 1, col_types, col_data, col_elem_size, 5, 3);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(exec_open(root), 0);

    int count = 0;
    VectorBlock *block;
    while ((block = exec_next(root)) != NULL) {
        count++;
        vector_block_destroy(block);
    }
    EXPECT_EQ(count, 2);
    exec_close(root);
    exec_destroy(root);
}

TEST_F(ExecutorFrameworkTest, FilterBasic) {
    int32_t col0[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    int col_types[] = {COLUMN_INT32};
    void *col_data[] = {col0};
    int col_elem_size[] = {sizeof(int32_t)};

    ExecNode *scan = exec_create_seqscan(0, 1, col_types, col_data, col_elem_size, 10, 4);
    ASSERT_NE(scan, nullptr);

    vecx_pred_t pred;
    memset(&pred, 0, sizeof(pred));
    pred.col = 0;
    pred.op = CMP_GT;
    pred.i64 = 5;

    ExecNode *filter = exec_create_filter(&pred);
    ASSERT_NE(filter, nullptr);
    filter->left = scan;

    EXPECT_EQ(exec_open(filter), 0);
    exec_close(filter);
    exec_destroy(filter);
}

TEST_F(ExecutorFrameworkTest, ExecExecBasic) {
    int32_t col0[] = {1, 2, 3};
    int col_types[] = {COLUMN_INT32};
    void *col_data[] = {col0};
    int col_elem_size[] = {sizeof(int32_t)};

    ExecNode *root = exec_create_seqscan(0, 1, col_types, col_data, col_elem_size, 3, 10);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(exec_open(root), 0);
    VectorBlock *block = exec_next(root);
    EXPECT_NE(block, nullptr);
    EXPECT_EQ(block->num_rows, 3);
    vector_block_destroy(block);
    exec_close(root);
    exec_destroy(root);
}

TEST_F(ExecutorFrameworkTest, OpenFailure) {
    int32_t col0[] = {1, 2, 3};
    int col_types[] = {COLUMN_INT32};
    void *col_data[] = {col0};
    int col_elem_size[] = {sizeof(int32_t)};

    ExecNode *root = exec_create_seqscan(0, 1, col_types, col_data, col_elem_size, 3, 0);
    if (root) {
        EXPECT_NE(exec_open(root), 0);
        exec_destroy(root);
    }
}

TEST_F(ExecutorFrameworkTest, ProjectBasic) {
    int32_t col0[] = {1, 2, 3, 4, 5};
    int col_types[] = {COLUMN_INT32};
    void *col_data[] = {col0};
    int col_elem_size[] = {sizeof(int32_t)};

    ExecNode *scan = exec_create_seqscan(0, 1, col_types, col_data, col_elem_size, 5, 10);
    ASSERT_NE(scan, nullptr);

    vecx_expr_t *expr = vecx_expr_bin(VEXPR_MUL, vecx_expr_col(0), vecx_expr_const(2.0));
    ASSERT_NE(expr, nullptr);

    ExecNode *project = exec_create_project(expr);
    ASSERT_NE(project, nullptr);
    project->left = scan;

    EXPECT_EQ(exec_open(project), 0);
    exec_close(project);
    exec_destroy(project);
}

TEST_F(ExecutorFrameworkTest, HashJoinBasic) {
    ExecNode *hashjoin = exec_create_hashjoin(0, 0);
    ASSERT_NE(hashjoin, nullptr);
    exec_destroy(hashjoin);
}

TEST_F(ExecutorFrameworkTest, HashAggBasic) {
    ExecNode *hashagg = exec_create_hashagg(0, 1);
    ASSERT_NE(hashagg, nullptr);
    exec_destroy(hashagg);
}
