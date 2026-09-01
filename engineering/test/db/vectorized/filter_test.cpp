/**
 * @file filter_test.cpp
 * @brief Gap#2 向量化执行引擎 Task3 单元测试：向量化过滤算子
 *
 * 覆盖：
 * - vecx_filter_block：int32 / int64 / float / double / string 五种列类型分派
 * - null 行排除（值满足条件但行为 null 时不匹配）
 * - 全匹配 / 无匹配 / 空块 / 单行块 / 满行块等边界
 * - vecx_filter_multi：多条件 AND / OR 合并，跨列、跨类型
 * - vector_filter_execute：类型化路径返回**原始行号**；无类型（-1）路径保持旧字符串行为
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

/** 建一个单 int32 列的块，值为 vals，列类型标记为 COLUMN_INT32 */
VectorBlock *make_int32_block(const std::vector<int32_t> &vals) {
    int n = (int)vals.size();
    int cap = n > 0 ? n : 1;
    VectorBlock *b = vector_block_create(cap, 1);
    if (!b) return nullptr;
    int32_t *col = (int32_t *)malloc(sizeof(int32_t) * (size_t)cap);
    for (int i = 0; i < n; i++) col[i] = vals[(size_t)i];
    vector_block_set_column(b, 0, col, (int)sizeof(int32_t));
    vector_block_set_column_type(b, 0, COLUMN_INT32);
    vector_block_set_num_rows(b, n);
    return b;
}

/** 建一个单 int64 列的块 */
VectorBlock *make_int64_block(const std::vector<int64_t> &vals) {
    int n = (int)vals.size();
    int cap = n > 0 ? n : 1;
    VectorBlock *b = vector_block_create(cap, 1);
    if (!b) return nullptr;
    int64_t *col = (int64_t *)malloc(sizeof(int64_t) * (size_t)cap);
    for (int i = 0; i < n; i++) col[i] = vals[(size_t)i];
    vector_block_set_column(b, 0, col, (int)sizeof(int64_t));
    vector_block_set_column_type(b, 0, COLUMN_INT64);
    vector_block_set_num_rows(b, n);
    return b;
}

/** 建一个单 double 列的块 */
VectorBlock *make_double_block(const std::vector<double> &vals) {
    int n = (int)vals.size();
    int cap = n > 0 ? n : 1;
    VectorBlock *b = vector_block_create(cap, 1);
    if (!b) return nullptr;
    double *col = (double *)malloc(sizeof(double) * (size_t)cap);
    for (int i = 0; i < n; i++) col[i] = vals[(size_t)i];
    vector_block_set_column(b, 0, col, (int)sizeof(double));
    vector_block_set_column_type(b, 0, COLUMN_DOUBLE);
    vector_block_set_num_rows(b, n);
    return b;
}

/** 建一个单 float 列的块 */
VectorBlock *make_float_block(const std::vector<float> &vals) {
    int n = (int)vals.size();
    int cap = n > 0 ? n : 1;
    VectorBlock *b = vector_block_create(cap, 1);
    if (!b) return nullptr;
    float *col = (float *)malloc(sizeof(float) * (size_t)cap);
    for (int i = 0; i < n; i++) col[i] = vals[(size_t)i];
    vector_block_set_column(b, 0, col, (int)sizeof(float));
    vector_block_set_column_type(b, 0, COLUMN_FLOAT);
    vector_block_set_num_rows(b, n);
    return b;
}

/**
 * 建一个单字符串列的块：列缓冲是 const char* 指针数组（元素大小 sizeof(char*)），
 * 字符串本身由调用方持有（测试里用静态字面量），块只做浅拷贝指针。
 */
VectorBlock *make_string_block(const std::vector<const char *> &vals) {
    int n = (int)vals.size();
    int cap = n > 0 ? n : 1;
    VectorBlock *b = vector_block_create(cap, 1);
    if (!b) return nullptr;
    const char **col = (const char **)malloc(sizeof(const char *) * (size_t)cap);
    for (int i = 0; i < n; i++) col[i] = vals[(size_t)i];
    vector_block_set_column(b, 0, (void *)col, (int)sizeof(const char *));
    vector_block_set_column_type(b, 0, COLUMN_STRING);
    vector_block_set_num_rows(b, n);
    return b;
}

}  // namespace

/* ========================================================================
 * 1. int32 GT
 * ======================================================================== */
TEST(VecxFilterBlock, Int32GreaterThan) {
    const std::vector<int32_t> vals = {5, 100, -3, 42, 7, 99, 0, 43};
    VectorBlock *in = make_int32_block(vals);
    ASSERT_NE(in, nullptr);

    int32_t threshold = 42;
    VectorBlock *out = nullptr;
    int n = vecx_filter_block(in, 0, CMP_GT, &threshold, &out);

    /* 手算：> 42 的是 100(idx1)、99(idx5)、43(idx7) */
    ASSERT_EQ(n, 3);
    ASSERT_NE(out, nullptr);
    EXPECT_EQ(out->num_rows, 3);
    EXPECT_EQ(vector_block_get_column_type(out, 0), COLUMN_INT32);

    const int32_t *d = (const int32_t *)vector_block_get_column(out, 0);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d[0], 100);
    EXPECT_EQ(d[1], 99);
    EXPECT_EQ(d[2], 43);

    vector_block_destroy(out);
    vector_block_destroy(in);
}

/* ========================================================================
 * 2. int64 EQ
 * ======================================================================== */
TEST(VecxFilterBlock, Int64Equal) {
    const std::vector<int64_t> vals = {7, 1234567890123LL, 7, -7, 7, 0};
    VectorBlock *in = make_int64_block(vals);
    ASSERT_NE(in, nullptr);

    int64_t needle = 7;
    VectorBlock *out = nullptr;
    int n = vecx_filter_block(in, 0, CMP_EQ, &needle, &out);

    ASSERT_EQ(n, 3);  /* idx 0、2、4 */
    ASSERT_NE(out, nullptr);
    const int64_t *d = (const int64_t *)vector_block_get_column(out, 0);
    ASSERT_NE(d, nullptr);
    for (int i = 0; i < n; i++) EXPECT_EQ(d[i], 7);
    EXPECT_EQ(vector_block_get_column_type(out, 0), COLUMN_INT64);

    vector_block_destroy(out);
    vector_block_destroy(in);
}

/* ========================================================================
 * 3. double LT
 * ======================================================================== */
TEST(VecxFilterBlock, DoubleLessThan) {
    const std::vector<double> vals = {1.5, -2.25, 3.75, 0.0, 10.125, -100.5};
    VectorBlock *in = make_double_block(vals);
    ASSERT_NE(in, nullptr);

    double threshold = 1.0;
    VectorBlock *out = nullptr;
    int n = vecx_filter_block(in, 0, CMP_LT, &threshold, &out);

    /* < 1.0 的是 -2.25(idx1)、0.0(idx3)、-100.5(idx5) */
    ASSERT_EQ(n, 3);
    ASSERT_NE(out, nullptr);
    const double *d = (const double *)vector_block_get_column(out, 0);
    ASSERT_NE(d, nullptr);
    EXPECT_DOUBLE_EQ(d[0], -2.25);
    EXPECT_DOUBLE_EQ(d[1], 0.0);
    EXPECT_DOUBLE_EQ(d[2], -100.5);

    vector_block_destroy(out);
    vector_block_destroy(in);
}

/* ========================================================================
 * 4. float GE
 * ======================================================================== */
TEST(VecxFilterBlock, FloatGreaterEqual) {
    const std::vector<float> vals = {1.0f, 2.5f, 2.5f, -1.0f, 9.0f};
    VectorBlock *in = make_float_block(vals);
    ASSERT_NE(in, nullptr);

    float threshold = 2.5f;
    VectorBlock *out = nullptr;
    int n = vecx_filter_block(in, 0, CMP_GE, &threshold, &out);

    ASSERT_EQ(n, 3);  /* idx 1、2、4 */
    ASSERT_NE(out, nullptr);
    const float *d = (const float *)vector_block_get_column(out, 0);
    ASSERT_NE(d, nullptr);
    EXPECT_FLOAT_EQ(d[0], 2.5f);
    EXPECT_FLOAT_EQ(d[1], 2.5f);
    EXPECT_FLOAT_EQ(d[2], 9.0f);

    vector_block_destroy(out);
    vector_block_destroy(in);
}

/* ========================================================================
 * 5. string EQ（标量 strcmp）
 * ======================================================================== */
TEST(VecxFilterBlock, StringEqual) {
    const std::vector<const char *> vals = {"apple", "banana", "apple", "cherry"};
    VectorBlock *in = make_string_block(vals);
    ASSERT_NE(in, nullptr);

    const char *needle = "apple";
    VectorBlock *out = nullptr;
    /* 字符串列的 value 语义是 char**：指向 C 串指针的指针 */
    int n = vecx_filter_block(in, 0, CMP_EQ, &needle, &out);

    ASSERT_EQ(n, 2);  /* idx 0、2 */
    ASSERT_NE(out, nullptr);
    const char **d = (const char **)vector_block_get_column(out, 0);
    ASSERT_NE(d, nullptr);
    EXPECT_STREQ(d[0], "apple");
    EXPECT_STREQ(d[1], "apple");
    EXPECT_EQ(vector_block_get_column_type(out, 0), COLUMN_STRING);

    vector_block_destroy(out);
    vector_block_destroy(in);
}

/* ========================================================================
 * 6. null 行排除：值满足条件但行为 null 时不匹配
 * ======================================================================== */
TEST(VecxFilterBlock, NullRowsExcluded) {
    const std::vector<int32_t> vals = {10, 20, 30, 40, 50};
    VectorBlock *in = make_int32_block(vals);
    ASSERT_NE(in, nullptr);
    /* idx 1、3 标记为 null（值本来都满足 > 5） */
    vector_block_set_null(in, 1, true);
    vector_block_set_null(in, 3, true);

    int32_t threshold = 5;
    VectorBlock *out = nullptr;
    int n = vecx_filter_block(in, 0, CMP_GT, &threshold, &out);

    ASSERT_EQ(n, 3);  /* 只剩 idx 0、2、4 */
    ASSERT_NE(out, nullptr);
    const int32_t *d = (const int32_t *)vector_block_get_column(out, 0);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d[0], 10);
    EXPECT_EQ(d[1], 30);
    EXPECT_EQ(d[2], 50);
    /* 输出块中不应残留 null 标记 */
    for (int i = 0; i < n; i++) EXPECT_FALSE(vector_block_is_null(out, i));

    vector_block_destroy(out);
    vector_block_destroy(in);
}

/* ========================================================================
 * 7. 全匹配：输出块内容与输入一致
 * ======================================================================== */
TEST(VecxFilterBlock, AllMatch) {
    const std::vector<int32_t> vals = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    VectorBlock *in = make_int32_block(vals);
    ASSERT_NE(in, nullptr);

    int32_t threshold = 0;
    VectorBlock *out = nullptr;
    int n = vecx_filter_block(in, 0, CMP_GT, &threshold, &out);

    ASSERT_EQ(n, in->num_rows);
    ASSERT_NE(out, nullptr);
    EXPECT_EQ(out->num_rows, in->num_rows);
    EXPECT_EQ(vector_block_get_column_type(out, 0), COLUMN_INT32);
    const int32_t *d = (const int32_t *)vector_block_get_column(out, 0);
    ASSERT_NE(d, nullptr);
    for (int i = 0; i < n; i++) EXPECT_EQ(d[i], vals[(size_t)i]);

    vector_block_destroy(out);
    vector_block_destroy(in);
}

/* ========================================================================
 * 8. 无匹配：返回 0 且 *out 为 NULL
 * ======================================================================== */
TEST(VecxFilterBlock, NoMatch) {
    const std::vector<int32_t> vals = {1, 2, 3, 4, 5};
    VectorBlock *in = make_int32_block(vals);
    ASSERT_NE(in, nullptr);

    int32_t threshold = 1000;
    VectorBlock *out = (VectorBlock *)(void *)&threshold;  /* 故意填脏值，验证被置 NULL */
    int n = vecx_filter_block(in, 0, CMP_GT, &threshold, &out);

    EXPECT_EQ(n, 0);
    EXPECT_EQ(out, nullptr);

    vector_block_destroy(in);
}

/* ========================================================================
 * 9. 边界：空块 / 单行块 / 跨 64 行的满块 / 非法入参
 * ======================================================================== */
TEST(VecxFilterBlock, EmptyBlock) {
    VectorBlock *in = make_int32_block({});
    ASSERT_NE(in, nullptr);
    EXPECT_EQ(in->num_rows, 0);

    int32_t threshold = 0;
    VectorBlock *out = nullptr;
    EXPECT_EQ(vecx_filter_block(in, 0, CMP_GT, &threshold, &out), 0);
    EXPECT_EQ(out, nullptr);

    vector_block_destroy(in);
}

TEST(VecxFilterBlock, SingleRowBlock) {
    VectorBlock *in = make_int32_block({42});
    ASSERT_NE(in, nullptr);

    int32_t v = 42;
    VectorBlock *out = nullptr;
    EXPECT_EQ(vecx_filter_block(in, 0, CMP_EQ, &v, &out), 1);
    ASSERT_NE(out, nullptr);
    EXPECT_EQ(((const int32_t *)vector_block_get_column(out, 0))[0], 42);
    vector_block_destroy(out);

    out = nullptr;
    EXPECT_EQ(vecx_filter_block(in, 0, CMP_NE, &v, &out), 0);
    EXPECT_EQ(out, nullptr);

    vector_block_destroy(in);
}

TEST(VecxFilterBlock, MultiWordBlock) {
    /* 200 行跨 4 个位图字，验证尾字与跨字都正确 */
    std::vector<int32_t> vals;
    for (int i = 0; i < 200; i++) vals.push_back(i);
    VectorBlock *in = make_int32_block(vals);
    ASSERT_NE(in, nullptr);

    int32_t threshold = 100;
    VectorBlock *out = nullptr;
    int n = vecx_filter_block(in, 0, CMP_GE, &threshold, &out);

    ASSERT_EQ(n, 100);  /* 100..199 */
    ASSERT_NE(out, nullptr);
    const int32_t *d = (const int32_t *)vector_block_get_column(out, 0);
    ASSERT_NE(d, nullptr);
    for (int i = 0; i < n; i++) EXPECT_EQ(d[i], 100 + i);

    vector_block_destroy(out);
    vector_block_destroy(in);
}

TEST(VecxFilterBlock, InvalidArgs) {
    VectorBlock *in = make_int32_block({1, 2, 3});
    ASSERT_NE(in, nullptr);
    int32_t v = 1;
    VectorBlock *out = nullptr;

    EXPECT_EQ(vecx_filter_block(nullptr, 0, CMP_EQ, &v, &out), -1);
    EXPECT_EQ(vecx_filter_block(in, -1, CMP_EQ, &v, &out), -1);
    EXPECT_EQ(vecx_filter_block(in, 5, CMP_EQ, &v, &out), -1);
    EXPECT_EQ(vecx_filter_block(in, 0, CMP_EQ, nullptr, &out), -1);
    EXPECT_EQ(vecx_filter_block(in, 0, CMP_EQ, &v, nullptr), -1);

    /* 未设类型的列（-1）不支持类型分派 */
    VectorBlock *untyped = vector_block_create(4, 1);
    ASSERT_NE(untyped, nullptr);
    int32_t *col = (int32_t *)calloc(4, sizeof(int32_t));
    vector_block_set_column(untyped, 0, col, (int)sizeof(int32_t));
    vector_block_set_num_rows(untyped, 4);
    EXPECT_EQ(vecx_filter_block(untyped, 0, CMP_EQ, &v, &out), -1);
    vector_block_destroy(untyped);

    vector_block_destroy(in);
}

/* ========================================================================
 * 10. 多条件 AND
 * ======================================================================== */
TEST(VecxFilterMulti, AndTwoColumns) {
    /* 两列：col0 = int32 年龄，col1 = double 分数 */
    const int N = 8;
    VectorBlock *in = vector_block_create(N, 2);
    ASSERT_NE(in, nullptr);
    int32_t *ages = (int32_t *)malloc(sizeof(int32_t) * N);
    double *scores = (double *)malloc(sizeof(double) * N);
    const int32_t age_v[N]   = {10, 20, 30, 40, 50, 60, 70, 80};
    const double  score_v[N] = {1.0, 9.0, 2.0, 8.0, 3.0, 7.0, 4.0, 6.0};
    for (int i = 0; i < N; i++) { ages[i] = age_v[i]; scores[i] = score_v[i]; }
    vector_block_set_column(in, 0, ages, (int)sizeof(int32_t));
    vector_block_set_column(in, 1, scores, (int)sizeof(double));
    vector_block_set_column_type(in, 0, COLUMN_INT32);
    vector_block_set_column_type(in, 1, COLUMN_DOUBLE);
    vector_block_set_num_rows(in, N);

    /* 条件：age > 25 AND score > 5.0 → idx3(40,8.0)、idx5(60,7.0)、idx7(80,6.0) */
    vecx_pred_t conds[2];
    memset(conds, 0, sizeof(conds));
    conds[0].col = 0; conds[0].op = CMP_GT; conds[0].i64 = 25;
    conds[1].col = 1; conds[1].op = CMP_GT; conds[1].f64 = 5.0;

    VectorBlock *out = nullptr;
    int n = vecx_filter_multi(in, conds, 2, 1, &out);

    ASSERT_EQ(n, 3);
    ASSERT_NE(out, nullptr);
    const int32_t *a = (const int32_t *)vector_block_get_column(out, 0);
    const double  *s = (const double  *)vector_block_get_column(out, 1);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(a[0], 40); EXPECT_DOUBLE_EQ(s[0], 8.0);
    EXPECT_EQ(a[1], 60); EXPECT_DOUBLE_EQ(s[1], 7.0);
    EXPECT_EQ(a[2], 80); EXPECT_DOUBLE_EQ(s[2], 6.0);

    vector_block_destroy(out);
    vector_block_destroy(in);
}

/* ========================================================================
 * 11. 多条件 OR
 * ======================================================================== */
TEST(VecxFilterMulti, OrTwoColumns) {
    const int N = 8;
    VectorBlock *in = vector_block_create(N, 2);
    ASSERT_NE(in, nullptr);
    int32_t *ages = (int32_t *)malloc(sizeof(int32_t) * N);
    double *scores = (double *)malloc(sizeof(double) * N);
    const int32_t age_v[N]   = {10, 20, 30, 40, 50, 60, 70, 80};
    const double  score_v[N] = {1.0, 9.0, 2.0, 8.0, 3.0, 7.0, 4.0, 6.0};
    for (int i = 0; i < N; i++) { ages[i] = age_v[i]; scores[i] = score_v[i]; }
    vector_block_set_column(in, 0, ages, (int)sizeof(int32_t));
    vector_block_set_column(in, 1, scores, (int)sizeof(double));
    vector_block_set_column_type(in, 0, COLUMN_INT32);
    vector_block_set_column_type(in, 1, COLUMN_DOUBLE);
    vector_block_set_num_rows(in, N);

    /* age < 25 OR score > 7.5 → idx0(10)、idx1(20,9.0)、idx3(40,8.0) */
    vecx_pred_t conds[2];
    memset(conds, 0, sizeof(conds));
    conds[0].col = 0; conds[0].op = CMP_LT; conds[0].i64 = 25;
    conds[1].col = 1; conds[1].op = CMP_GT; conds[1].f64 = 7.5;

    VectorBlock *out = nullptr;
    int n = vecx_filter_multi(in, conds, 2, 0, &out);

    ASSERT_EQ(n, 3);
    ASSERT_NE(out, nullptr);
    const int32_t *a = (const int32_t *)vector_block_get_column(out, 0);
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a[0], 10);
    EXPECT_EQ(a[1], 20);
    EXPECT_EQ(a[2], 40);

    vector_block_destroy(out);
    vector_block_destroy(in);
}

/* ========================================================================
 * 12. 多条件：单条件退化 / null 排除 / 无匹配 / 非法入参 / 字符串条件
 * ======================================================================== */
TEST(VecxFilterMulti, SingleCondEqualsFilterBlock) {
    const std::vector<int32_t> vals = {1, 5, 9, 13, 17};
    VectorBlock *in = make_int32_block(vals);
    ASSERT_NE(in, nullptr);

    vecx_pred_t cond;
    memset(&cond, 0, sizeof(cond));
    cond.col = 0; cond.op = CMP_GE; cond.i64 = 9;

    VectorBlock *out_and = nullptr;
    VectorBlock *out_or = nullptr;
    int n_and = vecx_filter_multi(in, &cond, 1, 1, &out_and);
    int n_or  = vecx_filter_multi(in, &cond, 1, 0, &out_or);
    /* 单条件下 AND / OR 结果必须一致 */
    EXPECT_EQ(n_and, 3);
    EXPECT_EQ(n_or, 3);

    /* 与 vecx_filter_block 逐值一致 */
    int32_t v = 9;
    VectorBlock *out_single = nullptr;
    int n_single = vecx_filter_block(in, 0, CMP_GE, &v, &out_single);
    ASSERT_EQ(n_single, 3);
    ASSERT_NE(out_single, nullptr);
    ASSERT_NE(out_and, nullptr);
    const int32_t *da = (const int32_t *)vector_block_get_column(out_and, 0);
    const int32_t *ds = (const int32_t *)vector_block_get_column(out_single, 0);
    for (int i = 0; i < 3; i++) EXPECT_EQ(da[i], ds[i]);

    vector_block_destroy(out_and);
    vector_block_destroy(out_or);
    vector_block_destroy(out_single);
    vector_block_destroy(in);
}

TEST(VecxFilterMulti, NullExcludedAndNoMatch) {
    const std::vector<int64_t> vals = {1, 2, 3, 4, 5, 6};
    VectorBlock *in = make_int64_block(vals);
    ASSERT_NE(in, nullptr);
    vector_block_set_null(in, 2, true);  /* 值 3 是 null */

    vecx_pred_t conds[2];
    memset(conds, 0, sizeof(conds));
    conds[0].col = 0; conds[0].op = CMP_GE; conds[0].i64 = 3;
    conds[1].col = 0; conds[1].op = CMP_LE; conds[1].i64 = 3;

    /* AND：只有值 3 满足，但它是 null → 0 匹配 */
    VectorBlock *out = nullptr;
    EXPECT_EQ(vecx_filter_multi(in, conds, 2, 1, &out), 0);
    EXPECT_EQ(out, nullptr);

    /* OR：全部行满足（>=3 或 <=3），扣掉 null 行 → 5 */
    out = nullptr;
    EXPECT_EQ(vecx_filter_multi(in, conds, 2, 0, &out), 5);
    ASSERT_NE(out, nullptr);
    vector_block_destroy(out);

    /* 非法入参 */
    out = nullptr;
    EXPECT_EQ(vecx_filter_multi(nullptr, conds, 2, 1, &out), -1);
    EXPECT_EQ(vecx_filter_multi(in, nullptr, 2, 1, &out), -1);
    EXPECT_EQ(vecx_filter_multi(in, conds, 0, 1, &out), -1);
    EXPECT_EQ(vecx_filter_multi(in, conds, 2, 1, nullptr), -1);

    /* 条件里列索引越界 → -1 */
    vecx_pred_t bad;
    memset(&bad, 0, sizeof(bad));
    bad.col = 7; bad.op = CMP_EQ; bad.i64 = 1;
    EXPECT_EQ(vecx_filter_multi(in, &bad, 1, 1, &out), -1);

    vector_block_destroy(in);
}

TEST(VecxFilterMulti, StringCondition) {
    const std::vector<const char *> vals = {"red", "green", "blue", "red"};
    VectorBlock *in = make_string_block(vals);
    ASSERT_NE(in, nullptr);

    vecx_pred_t cond;
    memset(&cond, 0, sizeof(cond));
    cond.col = 0; cond.op = CMP_EQ; cond.str = "red";

    VectorBlock *out = nullptr;
    int n = vecx_filter_multi(in, &cond, 1, 1, &out);
    ASSERT_EQ(n, 2);
    ASSERT_NE(out, nullptr);
    const char **d = (const char **)vector_block_get_column(out, 0);
    EXPECT_STREQ(d[0], "red");
    EXPECT_STREQ(d[1], "red");

    vector_block_destroy(out);
    vector_block_destroy(in);
}

/* ========================================================================
 * 13. vector_filter_execute：类型化路径返回原始行号
 * ======================================================================== */
TEST(VectorFilterExecuteTyped, Int32ReturnsOriginalRowIds) {
    const std::vector<int32_t> vals = {5, 100, -3, 42, 7, 99, 0, 43};
    VectorBlock *in = make_int32_block(vals);
    ASSERT_NE(in, nullptr);

    int32_t threshold = 42;
    VectorFilterResult *r = vector_filter_execute(in, 0, &threshold, CMP_GT);
    ASSERT_NE(r, nullptr);
    /* 手算原始行号：1、5、7 */
    ASSERT_EQ(r->num_matches, 3);
    ASSERT_NE(r->matches, nullptr);
    EXPECT_EQ(r->matches[0], 1);
    EXPECT_EQ(r->matches[1], 5);
    EXPECT_EQ(r->matches[2], 7);

    vector_filter_result_free(r);
    vector_block_destroy(in);
}

TEST(VectorFilterExecuteTyped, DoubleAndNullExcluded) {
    const std::vector<double> vals = {1.0, 2.0, 3.0, 4.0, 5.0};
    VectorBlock *in = make_double_block(vals);
    ASSERT_NE(in, nullptr);
    vector_block_set_null(in, 3, true);  /* 值 4.0 是 null */

    double threshold = 2.5;
    VectorFilterResult *r = vector_filter_execute(in, 0, &threshold, CMP_GT);
    ASSERT_NE(r, nullptr);
    /* > 2.5 的原始行号 2、3、4，扣掉 null 行 3 → 2、4 */
    ASSERT_EQ(r->num_matches, 2);
    EXPECT_EQ(r->matches[0], 2);
    EXPECT_EQ(r->matches[1], 4);

    vector_filter_result_free(r);
    vector_block_destroy(in);
}

TEST(VectorFilterExecuteTyped, Int64NoMatchReturnsEmptyResult) {
    const std::vector<int64_t> vals = {1, 2, 3};
    VectorBlock *in = make_int64_block(vals);
    ASSERT_NE(in, nullptr);

    int64_t needle = 999;
    VectorFilterResult *r = vector_filter_execute(in, 0, &needle, CMP_EQ);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->num_matches, 0);

    vector_filter_result_free(r);
    vector_block_destroy(in);
}

/* ========================================================================
 * 14. vector_filter_execute：无类型（-1）路径保持旧的 64 字节 strcmp 行为
 * ======================================================================== */
TEST(VectorFilterExecuteLegacy, UntypedKeepsFixedWidthStrcmp) {
    const int N = 4;
    VectorBlock *in = vector_block_create(N, 1);
    ASSERT_NE(in, nullptr);
    /* 旧路径约定：列缓冲是定长 64 字节的字符串数组 */
    char *col = (char *)calloc((size_t)N * 64, 1);
    strcpy(col + 0 * 64, "alpha");
    strcpy(col + 1 * 64, "beta");
    strcpy(col + 2 * 64, "alpha");
    strcpy(col + 3 * 64, "gamma");
    vector_block_set_column(in, 0, col, 64);
    /* 故意不设置列类型 → 保持 -1（未知） */
    ASSERT_EQ(vector_block_get_column_type(in, 0), -1);
    vector_block_set_num_rows(in, N);

    VectorFilterResult *r = vector_filter_execute(in, 0, (void *)"alpha", CMP_EQ);
    ASSERT_NE(r, nullptr);
    ASSERT_EQ(r->num_matches, 2);
    EXPECT_EQ(r->matches[0], 0);
    EXPECT_EQ(r->matches[1], 2);
    vector_filter_result_free(r);

    /* 旧路径支持字典序比较（数值 SIMD 路径不涉及） */
    r = vector_filter_execute(in, 0, (void *)"beta", CMP_LT);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->num_matches, 2);  /* alpha、alpha */
    vector_filter_result_free(r);

    vector_block_destroy(in);
}

TEST(VectorFilterExecuteLegacy, NullArgsStillReturnNull) {
    int32_t v = 1;
    EXPECT_EQ(vector_filter_execute(nullptr, 0, &v, CMP_EQ), nullptr);
    VectorBlock *in = make_int32_block({1, 2, 3});
    ASSERT_NE(in, nullptr);
    EXPECT_EQ(vector_filter_execute(in, 0, nullptr, CMP_EQ), nullptr);
    vector_block_destroy(in);
}
