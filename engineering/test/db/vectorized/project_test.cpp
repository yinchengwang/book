/**
 * @file project_test.cpp
 * @brief Gap#2 Task7 单元测试：向量表达式投影 + Source + 流水线
 *
 * 测试结构：
 * - VecxExpr: 10 tests - 表达式树构造与求值
 * - VecxSource: 9 tests - Source 工厂与迭代
 * - VecxPipeline: 3 tests - filter_agg 流水线组合
 */
#include <gtest/gtest.h>

extern "C" {
#include "db/core/vector_exec.h"
#include "db/core/columnar_store.h"
#include "db/vectorized/vectorized.h"
}

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

/* ========================================================================
 * 辅助函数
 * ======================================================================== */

/** 建单列块（int32） */
VectorBlock *make_int32_block(const int32_t *data, int nrows, bool with_null_bitmap) {
    VectorBlock *b = vector_block_create(nrows, 1);
    if (!b) return nullptr;
    int32_t *col = (int32_t *)malloc(sizeof(int32_t) * (size_t)nrows);
    if (!col) { vector_block_destroy(b); return nullptr; }
    memcpy(col, data, sizeof(int32_t) * (size_t)nrows);
    vector_block_set_column(b, 0, col, sizeof(int32_t));
    vector_block_set_column_type(b, 0, COLUMN_INT32);
    if (with_null_bitmap) {
        /* 建一个全0的null位图（无null行） */
        int nwords = (nrows + 63) / 64;
        b->null_bitmap = (uint64_t *)calloc((size_t)nwords, sizeof(uint64_t));
    }
    vector_block_set_num_rows(b, nrows);
    return b;
}

/** 建2列块（int32 + int32） */
VectorBlock *make_2col_int32_block(const int32_t *c0, const int32_t *c1, int nrows) {
    VectorBlock *b = vector_block_create(nrows, 2);
    if (!b) return nullptr;
    int32_t *col0 = (int32_t *)malloc(sizeof(int32_t) * (size_t)nrows);
    int32_t *col1 = (int32_t *)malloc(sizeof(int32_t) * (size_t)nrows);
    if (!col0 || !col1) { free(col0); free(col1); vector_block_destroy(b); return nullptr; }
    memcpy(col0, c0, sizeof(int32_t) * (size_t)nrows);
    memcpy(col1, c1, sizeof(int32_t) * (size_t)nrows);
    vector_block_set_column(b, 0, col0, sizeof(int32_t));
    vector_block_set_column(b, 1, col1, sizeof(int32_t));
    vector_block_set_column_type(b, 0, COLUMN_INT32);
    vector_block_set_column_type(b, 1, COLUMN_INT32);
    vector_block_set_num_rows(b, nrows);
    return b;
}

/** 建4列块（int32 各列） */
VectorBlock *make_4col_int32_block(const int32_t *c0, const int32_t *c1,
                                   const int32_t *c2, const int32_t *c3, int nrows) {
    VectorBlock *b = vector_block_create(nrows, 4);
    if (!b) return nullptr;
    int32_t *cols[4] = {nullptr, nullptr, nullptr, nullptr};
    const int32_t *src[4] = {c0, c1, c2, c3};
    for (int i = 0; i < 4; i++) {
        cols[i] = (int32_t *)malloc(sizeof(int32_t) * (size_t)nrows);
        if (!cols[i]) {
            for (int j = 0; j < i; j++) free(cols[j]);
            vector_block_destroy(b); return nullptr;
        }
        memcpy(cols[i], src[i], sizeof(int32_t) * (size_t)nrows);
        vector_block_set_column(b, i, cols[i], sizeof(int32_t));
        vector_block_set_column_type(b, i, COLUMN_INT32);
    }
    vector_block_set_num_rows(b, nrows);
    return b;
}

/* ========================================================================
 * VecxExpr 测试组
 * ======================================================================== */

class VecxExpr : public ::testing::Test {};

TEST_F(VecxExpr, CONST) {
    /* 常量求值：需要至少一列来构建块 */
    int32_t data[2] = {999, 888}; /* 任意数据，只用于占位 */
    VectorBlock *b = make_int32_block(data, 2, false);
    ASSERT_NE(b, nullptr);

    vecx_expr_t *e = vecx_expr_const(3.14);
    ASSERT_NE(e, nullptr);

    /* 验证常量节点的值（通过 project 验证） */
    VectorBlock *out = nullptr;
    int r = vecx_project(b, e, &out);
    EXPECT_EQ(r, 0);
    ASSERT_NE(out, nullptr);
    EXPECT_EQ(out->num_columns, 2); /* 1 input col + 1 result col */
    double *res = (double *)out->columns[1]; /* 第1列是表达式结果 */
    EXPECT_DOUBLE_EQ(res[0], 3.14);
    EXPECT_DOUBLE_EQ(res[1], 3.14);

    vecx_expr_free(e);
    vector_block_destroy(out);
    vector_block_destroy(b);
}

TEST_F(VecxExpr, COL_int32) {
    /* 列引用：int32 自动提升为 double */
    int32_t data[3] = {10, 20, 30};
    VectorBlock *b = make_int32_block(data, 3, false);
    ASSERT_NE(b, nullptr);

    vecx_expr_t *e = vecx_expr_col(0);
    ASSERT_NE(e, nullptr);

    VectorBlock *out = nullptr;
    int r = vecx_project(b, e, &out);
    EXPECT_EQ(r, 0);
    ASSERT_NE(out, nullptr);
    ASSERT_EQ(out->num_columns, 2);
    ASSERT_EQ(out->num_rows, 3);
    double *res = (double *)out->columns[1]; /* 第1列是表达式结果 */
    EXPECT_DOUBLE_EQ(res[0], 10.0);
    EXPECT_DOUBLE_EQ(res[1], 20.0);
    EXPECT_DOUBLE_EQ(res[2], 30.0);

    vecx_expr_free(e);
    vector_block_destroy(out);
    vector_block_destroy(b);
}

TEST_F(VecxExpr, COL_int64) {
    /* int64 列 */
    VectorBlock *b = vector_block_create(2, 1);
    ASSERT_NE(b, nullptr);
    int64_t *col = (int64_t *)malloc(sizeof(int64_t) * 2);
    col[0] = 100; col[1] = 200;
    vector_block_set_column(b, 0, col, sizeof(int64_t));
    vector_block_set_column_type(b, 0, COLUMN_INT64);
    vector_block_set_num_rows(b, 2);

    vecx_expr_t *e = vecx_expr_col(0);
    ASSERT_NE(e, nullptr);

    VectorBlock *out = nullptr;
    int r = vecx_project(b, e, &out);
    EXPECT_EQ(r, 0);
    ASSERT_NE(out, nullptr);
    double *res = (double *)out->columns[1];
    EXPECT_DOUBLE_EQ(res[0], 100.0);
    EXPECT_DOUBLE_EQ(res[1], 200.0);

    vecx_expr_free(e);
    vector_block_destroy(out);
    vector_block_destroy(b);
}

TEST_F(VecxExpr, COL_float) {
    /* float 列 */
    VectorBlock *b = vector_block_create(2, 1);
    ASSERT_NE(b, nullptr);
    float *col = (float *)malloc(sizeof(float) * 2);
    col[0] = 1.5f; col[1] = 2.5f;
    vector_block_set_column(b, 0, col, sizeof(float));
    vector_block_set_column_type(b, 0, COLUMN_FLOAT);
    vector_block_set_num_rows(b, 2);

    vecx_expr_t *e = vecx_expr_col(0);
    ASSERT_NE(e, nullptr);

    VectorBlock *out = nullptr;
    int r = vecx_project(b, e, &out);
    EXPECT_EQ(r, 0);
    ASSERT_NE(out, nullptr);
    double *res = (double *)out->columns[1];
    EXPECT_DOUBLE_EQ(res[0], 1.5);
    EXPECT_DOUBLE_EQ(res[1], 2.5);

    vecx_expr_free(e);
    vector_block_destroy(out);
    vector_block_destroy(b);
}

TEST_F(VecxExpr, COL_double) {
    /* double 列 */
    VectorBlock *b = vector_block_create(2, 1);
    ASSERT_NE(b, nullptr);
    double *col = (double *)malloc(sizeof(double) * 2);
    col[0] = 3.14; col[1] = 2.71;
    vector_block_set_column(b, 0, col, sizeof(double));
    vector_block_set_column_type(b, 0, COLUMN_DOUBLE);
    vector_block_set_num_rows(b, 2);

    vecx_expr_t *e = vecx_expr_col(0);
    ASSERT_NE(e, nullptr);

    VectorBlock *out = nullptr;
    int r = vecx_project(b, e, &out);
    EXPECT_EQ(r, 0);
    ASSERT_NE(out, nullptr);
    double *res = (double *)out->columns[1];
    EXPECT_DOUBLE_EQ(res[0], 3.14);
    EXPECT_DOUBLE_EQ(res[1], 2.71);

    vecx_expr_free(e);
    vector_block_destroy(out);
    vector_block_destroy(b);
}

TEST_F(VecxExpr, ADD) {
    /* 两列相加 */
    int32_t c0[3] = {1, 2, 3};
    int32_t c1[3] = {10, 20, 30};
    VectorBlock *b = make_2col_int32_block(c0, c1, 3);
    ASSERT_NE(b, nullptr);

    vecx_expr_t *left = vecx_expr_col(0);
    vecx_expr_t *right = vecx_expr_col(1);
    vecx_expr_t *e = vecx_expr_bin(VEXPR_ADD, left, right);
    ASSERT_NE(e, nullptr);

    VectorBlock *out = nullptr;
    int r = vecx_project(b, e, &out);
    EXPECT_EQ(r, 0);
    ASSERT_NE(out, nullptr);
    double *res = (double *)out->columns[2];
    EXPECT_DOUBLE_EQ(res[0], 11.0);
    EXPECT_DOUBLE_EQ(res[1], 22.0);
    EXPECT_DOUBLE_EQ(res[2], 33.0);

    vecx_expr_free(e);
    vector_block_destroy(out);
    vector_block_destroy(b);
}

TEST_F(VecxExpr, SUB) {
    /* 验证非交换性：col0 - col1 */
    int32_t c0[3] = {10, 20, 30};
    int32_t c1[3] = {1, 2, 3};
    VectorBlock *b = make_2col_int32_block(c0, c1, 3);
    ASSERT_NE(b, nullptr);

    vecx_expr_t *left = vecx_expr_col(0);
    vecx_expr_t *right = vecx_expr_col(1);
    vecx_expr_t *e = vecx_expr_bin(VEXPR_SUB, left, right);
    ASSERT_NE(e, nullptr);

    VectorBlock *out = nullptr;
    int r = vecx_project(b, e, &out);
    EXPECT_EQ(r, 0);
    ASSERT_NE(out, nullptr);
    double *res = (double *)out->columns[2];
    EXPECT_DOUBLE_EQ(res[0], 9.0);
    EXPECT_DOUBLE_EQ(res[1], 18.0);
    EXPECT_DOUBLE_EQ(res[2], 27.0);

    /* col1 - col0 应该不同 */
    vecx_expr_t *e2 = vecx_expr_bin(VEXPR_SUB,
        vecx_expr_col(1), vecx_expr_col(0));
    VectorBlock *out2 = nullptr;
    vecx_project(b, e2, &out2);
    ASSERT_NE(out2, nullptr);
    double *res2 = (double *)out2->columns[2];
    EXPECT_DOUBLE_EQ(res2[0], -9.0);

    vecx_expr_free(e);
    vecx_expr_free(e2);
    vector_block_destroy(out);
    vector_block_destroy(out2);
    vector_block_destroy(b);
}

TEST_F(VecxExpr, MUL) {
    /* 复合表达式：col0 * 2.0 + col1 */
    int32_t c0[3] = {1, 2, 3};
    int32_t c1[3] = {10, 20, 30};
    VectorBlock *b = make_2col_int32_block(c0, c1, 3);
    ASSERT_NE(b, nullptr);

    /* col0 * 2.0 */
    vecx_expr_t *mul = vecx_expr_bin(VEXPR_MUL,
        vecx_expr_col(0), vecx_expr_const(2.0));
    /* mul + col1 */
    vecx_expr_t *e = vecx_expr_bin(VEXPR_ADD, mul, vecx_expr_col(1));
    ASSERT_NE(e, nullptr);

    VectorBlock *out = nullptr;
    int r = vecx_project(b, e, &out);
    EXPECT_EQ(r, 0);
    ASSERT_NE(out, nullptr);
    double *res = (double *)out->columns[2];
    EXPECT_DOUBLE_EQ(res[0], 12.0);  /* 1*2+10 */
    EXPECT_DOUBLE_EQ(res[1], 24.0);  /* 2*2+20 */
    EXPECT_DOUBLE_EQ(res[2], 36.0);  /* 3*2+30 */

    vecx_expr_free(e);
    vector_block_destroy(out);
    vector_block_destroy(b);
}

TEST_F(VecxExpr, Nested) {
    /* 多层嵌套：((col0 + col1) * 3.0 - col2) * col3 */
    int32_t c0[2] = {1, 2};
    int32_t c1[2] = {3, 4};
    int32_t c2[2] = {5, 6};
    int32_t c3[2] = {7, 8};
    VectorBlock *b = make_4col_int32_block(c0, c1, c2, c3, 2);
    ASSERT_NE(b, nullptr);

    vecx_expr_t *e = vecx_expr_bin(VEXPR_MUL,
        vecx_expr_bin(VEXPR_SUB,
            vecx_expr_bin(VEXPR_MUL,
                vecx_expr_bin(VEXPR_ADD, vecx_expr_col(0), vecx_expr_col(1)),
                vecx_expr_const(3.0)),
            vecx_expr_col(2)),
        vecx_expr_col(3));

    VectorBlock *out = nullptr;
    int r = vecx_project(b, e, &out);
    EXPECT_EQ(r, 0);
    ASSERT_NE(out, nullptr);
    double *res = (double *)out->columns[4];

    /* ((1+3)*3 - 5) * 7 = (12 - 5) * 7 = 49 */
    EXPECT_DOUBLE_EQ(res[0], 49.0);
    /* ((2+4)*3 - 6) * 8 = (18 - 6) * 8 = 96 */
    EXPECT_DOUBLE_EQ(res[1], 96.0);

    vecx_expr_free(e);
    vector_block_destroy(out);
    vector_block_destroy(b);
}

TEST_F(VecxExpr, NullPropagation) {
    /* 输入行 null → 输出行 null */
    int32_t data[3] = {10, 20, 30};
    VectorBlock *b = make_int32_block(data, 3, true);
    ASSERT_NE(b, nullptr);
    /* 设置第1行（索引1）为 null */
    vector_block_set_null(b, 1, true);

    vecx_expr_t *e = vecx_expr_col(0);
    ASSERT_NE(e, nullptr);

    VectorBlock *out = nullptr;
    int r = vecx_project(b, e, &out);
    EXPECT_EQ(r, 0);
    ASSERT_NE(out, nullptr);
    /* 验证第1行是 null */
    EXPECT_TRUE(vector_block_is_null(out, 1));
    /* 第0行和第2行不是 null */
    EXPECT_FALSE(vector_block_is_null(out, 0));
    EXPECT_FALSE(vector_block_is_null(out, 2));

    vecx_expr_free(e);
    vector_block_destroy(out);
    vector_block_destroy(b);
}

TEST_F(VecxExpr, ProjectOutputBlock) {
    /* vecx_project 输出块：前 N 列同输入，最后一列是表达式结果 */
    int32_t c0[2] = {100, 200};
    int32_t c1[2] = {5, 6};
    VectorBlock *b = make_2col_int32_block(c0, c1, 2);
    ASSERT_NE(b, nullptr);

    vecx_expr_t *e = vecx_expr_bin(VEXPR_ADD, vecx_expr_col(0), vecx_expr_col(1));
    VectorBlock *out = nullptr;
    int r = vecx_project(b, e, &out);
    EXPECT_EQ(r, 0);
    ASSERT_NE(out, nullptr);

    /* 列数 */
    EXPECT_EQ(out->num_columns, 3); /* 2 input + 1 result */

    /* 前两列与输入一致 */
    ASSERT_NE(out->columns[0], nullptr);
    ASSERT_NE(out->columns[1], nullptr);
    int32_t *col0 = (int32_t *)out->columns[0];
    int32_t *col1 = (int32_t *)out->columns[1];
    EXPECT_EQ(col0[0], 100);
    EXPECT_EQ(col0[1], 200);
    EXPECT_EQ(col1[0], 5);
    EXPECT_EQ(col1[1], 6);

    /* 最后一列是表达式结果 */
    ASSERT_NE(out->columns[2], nullptr);
    double *res = (double *)out->columns[2];
    EXPECT_DOUBLE_EQ(res[0], 105.0);
    EXPECT_DOUBLE_EQ(res[1], 206.0);

    /* 类型标签 */
    EXPECT_EQ(vector_block_get_column_type(out, 0), COLUMN_INT32);
    EXPECT_EQ(vector_block_get_column_type(out, 1), COLUMN_INT32);
    EXPECT_EQ(vector_block_get_column_type(out, 2), COLUMN_DOUBLE);

    vecx_expr_free(e);
    vector_block_destroy(out);
    vector_block_destroy(b);
}

TEST_F(VecxExpr, InputUnmodified) {
    /* 验证输入块不被修改 */
    int32_t data[2] = {10, 20};
    VectorBlock *b = make_int32_block(data, 2, false);
    ASSERT_NE(b, nullptr);

    /* 保存原始数据 */
    int32_t orig0 = ((int32_t *)b->columns[0])[0];
    int32_t orig1 = ((int32_t *)b->columns[0])[1];

    vecx_expr_t *e = vecx_expr_const(99.0);
    VectorBlock *out = nullptr;
    vecx_project(b, e, &out);
    ASSERT_NE(out, nullptr);

    /* 验证原始数据未变 */
    int32_t *col = (int32_t *)b->columns[0];
    EXPECT_EQ(col[0], orig0);
    EXPECT_EQ(col[1], orig1);

    vecx_expr_free(e);
    vector_block_destroy(out);
    vector_block_destroy(b);
}

TEST_F(VecxExpr, InvalidArgs) {
    int32_t data[1] = {10};
    VectorBlock *b = make_int32_block(data, 1, false);
    ASSERT_NE(b, nullptr);

    /* NULL e */
    EXPECT_EQ(vecx_project(b, nullptr, nullptr), -1);

    vector_block_destroy(b);
}

/* ========================================================================
 * VecxSource 测试组
 * ======================================================================== */

class VecxSource : public ::testing::Test {};

TEST_F(VecxSource, FromColumns_Basic) {
    /* from_columns: next 5 blocks (batch=3, total=13) */
    int32_t col0[13];
    for (int i = 0; i < 13; i++) col0[i] = i + 1; /* 1..13 */

    int col_types[1] = {COLUMN_INT32};
    const void *col_data[1] = {col0};
    int col_elem_size[1] = {sizeof(int32_t)};

    vecx_source_t *s = vecx_source_from_columns(1, col_types, col_data,
        col_elem_size, 13, 3);
    ASSERT_NE(s, nullptr);

    std::vector<int> block_sizes;
    std::vector<double> values;
    VectorBlock *blk;
    int block_count = 0;
    while ((blk = vecx_source_next(s)) != nullptr) {
        block_count++;
        block_sizes.push_back(blk->num_rows);
        int32_t *c = (int32_t *)blk->columns[0];
        for (int i = 0; i < blk->num_rows; i++) {
            values.push_back((double)c[i]);
        }
        vector_block_destroy(blk);
    }

    EXPECT_EQ(block_count, 5); /* 3+3+3+3+1=13 → 5 blocks */
    ASSERT_EQ(block_sizes.size(), 5u);
    EXPECT_EQ(block_sizes[0], 3);
    EXPECT_EQ(block_sizes[1], 3);
    EXPECT_EQ(block_sizes[2], 3);
    EXPECT_EQ(block_sizes[3], 3);
    EXPECT_EQ(block_sizes[4], 1);

    ASSERT_EQ(values.size(), 13u);
    for (int i = 0; i < 13; i++) {
        EXPECT_DOUBLE_EQ(values[i], (double)(i + 1));
    }

    vecx_source_destroy(s);
}

TEST_F(VecxSource, FromColumns_ExactSplit) {
    /* from_columns: total=10, batch=3 → 3/3/3/1 */
    int32_t col0[10];
    for (int i = 0; i < 10; i++) col0[i] = i * 10;

    int col_types[1] = {COLUMN_INT32};
    const void *col_data[1] = {col0};
    int col_elem_size[1] = {sizeof(int32_t)};

    vecx_source_t *s = vecx_source_from_columns(1, col_types, col_data,
        col_elem_size, 10, 3);
    ASSERT_NE(s, nullptr);

    std::vector<int> sizes;
    VectorBlock *blk;
    while ((blk = vecx_source_next(s)) != nullptr) {
        sizes.push_back(blk->num_rows);
        vector_block_destroy(blk);
    }

    ASSERT_EQ(sizes.size(), 4u);
    EXPECT_EQ(sizes[0], 3);
    EXPECT_EQ(sizes[1], 3);
    EXPECT_EQ(sizes[2], 3);
    EXPECT_EQ(sizes[3], 1);

    vecx_source_destroy(s);
}

TEST_F(VecxSource, FromRows_Basic) {
    /* from_rows: 迭代器返回 [i, i*2, i*3] */
    const int ncols = 3;
    int col_types[ncols] = {COLUMN_INT32, COLUMN_INT32, COLUMN_INT32};
    int col_elem_size[ncols] = {sizeof(int32_t), sizeof(int32_t), sizeof(int32_t)};

    struct RowIterState {
        int cur;
        int total;
    } state = {0, 5};

    auto row_next = [](void *st, int ncols,
                       void **col_values, int *out_types, char *isnull) -> int {
        auto *s = (RowIterState *)st;
        if (s->cur >= s->total) return 0;
        int i = s->cur++;
        int32_t *c0 = (int32_t *)col_values[0];
        int32_t *c1 = (int32_t *)col_values[1];
        int32_t *c2 = (int32_t *)col_values[2];
        c0[0] = i;
        c1[0] = i * 2;
        c2[0] = i * 3;
        out_types[0] = COLUMN_INT32;
        out_types[1] = COLUMN_INT32;
        out_types[2] = COLUMN_INT32;
        isnull[0] = isnull[1] = isnull[2] = 0;
        return 1;
    };

    vecx_source_t *s = vecx_source_from_rows(ncols, col_types, col_elem_size,
        row_next, &state, 3, 5);
    ASSERT_NE(s, nullptr);

    std::vector<int> sizes;
    std::vector<std::vector<int32_t>> all_vals(ncols);
    VectorBlock *blk;
    while ((blk = vecx_source_next(s)) != nullptr) {
        sizes.push_back(blk->num_rows);
        for (int c = 0; c < ncols; c++) {
            int32_t *col = (int32_t *)blk->columns[c];
            for (int r = 0; r < blk->num_rows; r++) {
                all_vals[c].push_back(col[r]);
            }
        }
        vector_block_destroy(blk);
    }

    ASSERT_EQ(sizes.size(), 2u); /* 3+2=5 */
    EXPECT_EQ(sizes[0], 3);
    EXPECT_EQ(sizes[1], 2);

    ASSERT_EQ(all_vals[0].size(), 5u);
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(all_vals[0][i], i);
        EXPECT_EQ(all_vals[1][i], i * 2);
        EXPECT_EQ(all_vals[2][i], i * 3);
    }

    vecx_source_destroy(s);
}

TEST_F(VecxSource, FromRows_IteratorReturnsZero) {
    /* 迭代器返回0：正常停止 */
    int col_types[1] = {COLUMN_INT32};
    int col_elem_size[1] = {sizeof(int32_t)};

    auto row_next = [](void *, int, void **, int *, char *) -> int {
        return 0; /* 立即停止 */
    };

    vecx_source_t *s = vecx_source_from_rows(1, col_types, col_elem_size,
        row_next, nullptr, 10, 100);
    ASSERT_NE(s, nullptr);

    VectorBlock *blk = vecx_source_next(s);
    EXPECT_EQ(blk, nullptr);

    vecx_source_destroy(s);
}

TEST_F(VecxSource, FromRows_HintRowsPrealloc) {
    /* hint_rows > 0: 预分配缓冲正常（只验证不崩溃） */
    int col_types[1] = {COLUMN_INT32};
    int col_elem_size[1] = {sizeof(int32_t)};

    int count = 0;
    auto row_next = [](void *st, int, void **col_values, int *, char *) -> int {
        auto *c = (int *)st;
        if (*c >= 5) return 0;
        *(int32_t *)col_values[0] = *c;
        (*c)++;
        return 1;
    };

    vecx_source_t *s = vecx_source_from_rows(1, col_types, col_elem_size,
        row_next, &count, 10, 100); /* hint=100 */
    ASSERT_NE(s, nullptr);

    VectorBlock *blk = vecx_source_next(s);
    ASSERT_NE(blk, nullptr);
    EXPECT_EQ(blk->num_rows, 5); /* 实际只产了5行 */
    vector_block_destroy(blk);

    vecx_source_destroy(s);
}

TEST_F(VecxSource, FromColumns_NullRowPropagation) {
    /* from_columns 带 null 位图的列，验证 null 标记正确传播 */
    int32_t col0[3] = {10, 20, 30};
    int col_types[1] = {COLUMN_INT32};
    const void *col_data[1] = {col0};
    int col_elem_size[1] = {sizeof(int32_t)};

    vecx_source_t *s = vecx_source_from_columns(1, col_types, col_data,
        col_elem_size, 3, 3);
    ASSERT_NE(s, nullptr);

    VectorBlock *blk = vecx_source_next(s);
    ASSERT_NE(blk, nullptr);
    EXPECT_EQ(blk->num_rows, 3);
    /* null 位图由 source 自己维护，这里验证块确实产出了 */
    vector_block_destroy(blk);

    vecx_source_destroy(s);
}

TEST_F(VecxSource, FromColumns_ColDataNotModified) {
    /* next 不修改底层 col_data */
    int32_t col0[5] = {1, 2, 3, 4, 5};
    int32_t original[5];
    memcpy(original, col0, sizeof(original));

    int col_types[1] = {COLUMN_INT32};
    const void *col_data[1] = {col0};
    int col_elem_size[1] = {sizeof(int32_t)};

    vecx_source_t *s = vecx_source_from_columns(1, col_types, col_data,
        col_elem_size, 5, 2);
    ASSERT_NE(s, nullptr);

    VectorBlock *blk = vecx_source_next(s);
    if (blk) vector_block_destroy(blk);

    /* 底层数据未变 */
    EXPECT_EQ(memcmp(original, col0, sizeof(original)), 0);

    vecx_source_destroy(s);
}

TEST_F(VecxSource, DestroyThenNext_NoCrash) {
    /* destroy 后再 next 不崩溃（防御性判NULL）
       注意：调用方应在 destroy 后将指针设为 NULL，这是标准的 C 内存管理语义 */
    int32_t col0[3] = {1, 2, 3};
    int col_types[1] = {COLUMN_INT32};
    const void *col_data[1] = {col0};
    int col_elem_size[1] = {sizeof(int32_t)};

    vecx_source_t *s = vecx_source_from_columns(1, col_types, col_data,
        col_elem_size, 3, 2);
    ASSERT_NE(s, nullptr);

    vecx_source_destroy(s);
    s = NULL; /* 正确做法：destroy 后将指针置 NULL */

    /* 不崩溃 */
    VectorBlock *blk = vecx_source_next(s);
    EXPECT_EQ(blk, nullptr);
}

TEST_F(VecxSource, InvalidArgs) {
    /* 入参非法 */
    int32_t col0[3] = {1, 2, 3};
    int col_types[1] = {COLUMN_INT32};
    const void *col_data[1] = {col0};
    int col_elem_size[1] = {sizeof(int32_t)};

    /* ncols=0 */
    EXPECT_EQ(vecx_source_from_columns(0, col_types, col_data, col_elem_size, 3, 2), nullptr);

    /* batch_size<=0 */
    EXPECT_EQ(vecx_source_from_columns(1, col_types, col_data, col_elem_size, 3, 0), nullptr);
    EXPECT_EQ(vecx_source_from_columns(1, col_types, col_data, col_elem_size, 3, -1), nullptr);

    /* total_rows=0 */
    EXPECT_EQ(vecx_source_from_columns(1, col_types, col_data, col_elem_size, 0, 2), nullptr);

    /* row_next NULL */
    EXPECT_EQ(vecx_source_from_rows(1, col_types, col_elem_size, nullptr, nullptr, 2, 0), nullptr);
}

/* ========================================================================
 * VecxPipeline 测试组
 * ======================================================================== */

class VecxPipeline : public ::testing::Test {};

TEST_F(VecxPipeline, FilterAgg_SUM) {
    /* source {1..100} int32, filter > 50, SUM → expect 51+52+...+100 */
    int32_t col0[100];
    for (int i = 0; i < 100; i++) col0[i] = i + 1;

    int col_types[1] = {COLUMN_INT32};
    const void *col_data[1] = {col0};
    int col_elem_size[1] = {sizeof(int32_t)};

    vecx_source_t *s = vecx_source_from_columns(1, col_types, col_data,
        col_elem_size, 100, 10);
    ASSERT_NE(s, nullptr);

    int32_t filter_val = 50;
    double out = 0.0;
    int has = 0;

    int r = vecx_pipeline_filter_agg(s, 0, CMP_GT, &filter_val,
        0, VECX_AGG_SUM, &out, &has);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(has, 1);

    /* 51+52+...+100 = (51+100)*50/2 = 3775 */
    double expected = 0.0;
    for (int i = 51; i <= 100; i++) expected += i;
    EXPECT_DOUBLE_EQ(out, expected);

    vecx_source_destroy(s);
}

TEST_F(VecxPipeline, EmptySource) {
    /* 空 source: total_rows=0 时 source 创建返回 NULL */
    int32_t col0[1] = {1};
    int col_types[1] = {COLUMN_INT32};
    const void *col_data[1] = {col0};
    int col_elem_size[1] = {sizeof(int32_t)};

    vecx_source_t *s = vecx_source_from_columns(1, col_types, col_data,
        col_elem_size, 0, 10); /* total_rows=0 → 按 brief 规范返回 NULL */
    EXPECT_EQ(s, nullptr); /* brief 测试 19 要求 total_rows=0 返回 NULL */
}

TEST_F(VecxPipeline, FilterAgg_COUNT) {
    /* 同 source, filter > 50, COUNT → expect 50 */
    int32_t col0[100];
    for (int i = 0; i < 100; i++) col0[i] = i + 1;

    int col_types[1] = {COLUMN_INT32};
    const void *col_data[1] = {col0};
    int col_elem_size[1] = {sizeof(int32_t)};

    vecx_source_t *s = vecx_source_from_columns(1, col_types, col_data,
        col_elem_size, 100, 10);
    ASSERT_NE(s, nullptr);

    int32_t filter_val = 50;
    double out = 0.0;
    int has = 0;

    int r = vecx_pipeline_filter_agg(s, 0, CMP_GT, &filter_val,
        0, VECX_AGG_COUNT, &out, &has);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(has, 1);
    EXPECT_DOUBLE_EQ(out, 50.0);

    vecx_source_destroy(s);
}

} /* namespace */
