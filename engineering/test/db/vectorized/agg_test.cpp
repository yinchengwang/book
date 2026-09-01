/**
 * @file agg_test.cpp
 * @brief Gap#2 向量化执行引擎 Task4 单元测试：向量化标量聚合算子
 *
 * 覆盖：
 * - vecx_agg_scalar 在 int32 / int64 / float / double 四种数值列上的
 *   COUNT / SUM / MIN / MAX / AVG
 * - 选择向量子集、乱序 + 重复的选择向量（聚合不去重）
 * - null 行跳过、全 null、空选择、空块（SQL 语义：COUNT=0 有结果，其余无结果）
 * - 大整数精度（钉死 int64 累加器）、负数/混合符号、±inf 极值
 * - 入参错误与"聚合是只读算子"（不修改输入块）
 */
#include <gtest/gtest.h>

extern "C" {
#include "db/core/vector_exec.h"
#include "db/core/columnar_store.h"
#include "db/vectorized/vectorized.h"
}

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

/*
 * 建块 helper 与 filter_test.cpp 保持同一套写法：
 * 列缓冲用 malloc 分配后交给 vector_block_set_column，所有权随之转移给块，
 * 由 vector_block_destroy 统一 free——调用方不得再自行释放（否则 double-free）。
 */

/** 建一个单 int32 列的块，列类型标记为 COLUMN_INT32 */
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

/** 跑一次聚合并断言"有结果"，返回结果值（失败时用 ADD_FAILURE 记录并返回 NaN） */
double agg_ok(const VectorBlock *b, int col, vecx_agg_kind_t kind,
              const int *sel, int nsel) {
    double out = -12345.0;   /* 故意填脏值，验证被算子覆盖 */
    int has = -1;
    int rc = vecx_agg_scalar(b, col, kind, sel, nsel, &out, &has);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(has, 1);
    if (rc != 0 || has != 1) return NAN;
    return out;
}

/** 断言某种聚合是"空集"：返回 0、has_result=0、out=0.0 */
void expect_agg_empty(const VectorBlock *b, int col, vecx_agg_kind_t kind,
                      const int *sel, int nsel) {
    double out = -12345.0;
    int has = -1;
    EXPECT_EQ(vecx_agg_scalar(b, col, kind, sel, nsel, &out, &has), 0);
    EXPECT_EQ(has, 0);
    EXPECT_DOUBLE_EQ(out, 0.0);
}

/**
 * 空集的完整期望（brief 第 7/8/9 条共用）：
 * COUNT 有结果且为 0（SQL 的 COUNT 对空集返回 0 而非 NULL），其余四种均无结果。
 */
void expect_empty_set_semantics(const VectorBlock *b, int col,
                                const int *sel, int nsel) {
    double out = -12345.0;
    int has = -1;
    EXPECT_EQ(vecx_agg_scalar(b, col, VECX_AGG_COUNT, sel, nsel, &out, &has), 0);
    EXPECT_EQ(has, 1);
    EXPECT_DOUBLE_EQ(out, 0.0);

    expect_agg_empty(b, col, VECX_AGG_SUM, sel, nsel);
    expect_agg_empty(b, col, VECX_AGG_MIN, sel, nsel);
    expect_agg_empty(b, col, VECX_AGG_MAX, sel, nsel);
    expect_agg_empty(b, col, VECX_AGG_AVG, sel, nsel);
}

}  // namespace

/* ========================================================================
 * 1. int64 列基础：五种聚合与手算结果逐一相等
 * ======================================================================== */
TEST(VecxAggScalar, Int64AllKinds) {
    const std::vector<int64_t> vals = {10, -4, 7, 100, 3};
    VectorBlock *b = make_int64_block(vals);
    ASSERT_NE(b, nullptr);

    /* 手算：count=5，sum=116，min=-4，max=100，avg=116/5 */
    EXPECT_DOUBLE_EQ(agg_ok(b, 0, VECX_AGG_COUNT, nullptr, 0), 5.0);
    EXPECT_DOUBLE_EQ(agg_ok(b, 0, VECX_AGG_SUM, nullptr, 0), 116.0);
    EXPECT_DOUBLE_EQ(agg_ok(b, 0, VECX_AGG_MIN, nullptr, 0), -4.0);
    EXPECT_DOUBLE_EQ(agg_ok(b, 0, VECX_AGG_MAX, nullptr, 0), 100.0);
    EXPECT_DOUBLE_EQ(agg_ok(b, 0, VECX_AGG_AVG, nullptr, 0), 116.0 / 5.0);

    vector_block_destroy(b);
}

/* ========================================================================
 * 2. int32 列 / double 列各跑一遍全部五种聚合
 * ======================================================================== */
TEST(VecxAggScalar, Int32AllKinds) {
    const std::vector<int32_t> vals = {5, -2, 8, 1};
    VectorBlock *b = make_int32_block(vals);
    ASSERT_NE(b, nullptr);

    /* 手算：count=4，sum=12，min=-2，max=8，avg=3 */
    EXPECT_DOUBLE_EQ(agg_ok(b, 0, VECX_AGG_COUNT, nullptr, 0), 4.0);
    EXPECT_DOUBLE_EQ(agg_ok(b, 0, VECX_AGG_SUM, nullptr, 0), 12.0);
    EXPECT_DOUBLE_EQ(agg_ok(b, 0, VECX_AGG_MIN, nullptr, 0), -2.0);
    EXPECT_DOUBLE_EQ(agg_ok(b, 0, VECX_AGG_MAX, nullptr, 0), 8.0);
    EXPECT_DOUBLE_EQ(agg_ok(b, 0, VECX_AGG_AVG, nullptr, 0), 3.0);

    vector_block_destroy(b);
}

TEST(VecxAggScalar, DoubleAllKinds) {
    const std::vector<double> vals = {1.5, -2.5, 4.0, 7.0};
    VectorBlock *b = make_double_block(vals);
    ASSERT_NE(b, nullptr);

    /* 手算：count=4，sum=10.0，min=-2.5，max=7.0，avg=2.5 */
    EXPECT_DOUBLE_EQ(agg_ok(b, 0, VECX_AGG_COUNT, nullptr, 0), 4.0);
    EXPECT_DOUBLE_EQ(agg_ok(b, 0, VECX_AGG_SUM, nullptr, 0), 10.0);
    EXPECT_DOUBLE_EQ(agg_ok(b, 0, VECX_AGG_MIN, nullptr, 0), -2.5);
    EXPECT_DOUBLE_EQ(agg_ok(b, 0, VECX_AGG_MAX, nullptr, 0), 7.0);
    EXPECT_DOUBLE_EQ(agg_ok(b, 0, VECX_AGG_AVG, nullptr, 0), 2.5);

    vector_block_destroy(b);
}

/* ========================================================================
 * 3. float 列：取能被 float 精确表示的值，验证提升到 double 后结果准确
 * ======================================================================== */
TEST(VecxAggScalar, FloatPromotedToDouble) {
    /* 1.5 / 2.25 / -0.75 / 4.0 都是 2 的负幂组合，float 与 double 下均无舍入 */
    const std::vector<float> vals = {1.5f, 2.25f, -0.75f, 4.0f};
    VectorBlock *b = make_float_block(vals);
    ASSERT_NE(b, nullptr);

    /* 手算：count=4，sum=7.0，min=-0.75，max=4.0，avg=1.75 */
    EXPECT_DOUBLE_EQ(agg_ok(b, 0, VECX_AGG_COUNT, nullptr, 0), 4.0);
    EXPECT_DOUBLE_EQ(agg_ok(b, 0, VECX_AGG_SUM, nullptr, 0), 7.0);
    EXPECT_DOUBLE_EQ(agg_ok(b, 0, VECX_AGG_MIN, nullptr, 0), -0.75);
    EXPECT_DOUBLE_EQ(agg_ok(b, 0, VECX_AGG_MAX, nullptr, 0), 4.0);
    EXPECT_DOUBLE_EQ(agg_ok(b, 0, VECX_AGG_AVG, nullptr, 0), 1.75);

    vector_block_destroy(b);
}

/* ========================================================================
 * 4. 选择向量子集：只聚合 sel 指定的行
 * ======================================================================== */
TEST(VecxAggScalar, SelectionSubset) {
    const std::vector<int64_t> vals = {10, -4, 7, 100, 3};
    VectorBlock *b = make_int64_block(vals);
    ASSERT_NE(b, nullptr);

    const int sel[3] = {1, 3, 4};  /* 取值 -4、100、3 */
    EXPECT_DOUBLE_EQ(agg_ok(b, 0, VECX_AGG_COUNT, sel, 3), 3.0);
    EXPECT_DOUBLE_EQ(agg_ok(b, 0, VECX_AGG_SUM, sel, 3), 99.0);
    EXPECT_DOUBLE_EQ(agg_ok(b, 0, VECX_AGG_MIN, sel, 3), -4.0);
    EXPECT_DOUBLE_EQ(agg_ok(b, 0, VECX_AGG_MAX, sel, 3), 100.0);
    EXPECT_DOUBLE_EQ(agg_ok(b, 0, VECX_AGG_AVG, sel, 3), 99.0 / 3.0);

    vector_block_destroy(b);
}

/* ========================================================================
 * 5. 乱序 + 重复的选择向量：聚合不去重，同一行出现两次就算两次
 * ======================================================================== */
TEST(VecxAggScalar, SelectionUnorderedWithDuplicates) {
    const std::vector<int64_t> vals = {10, -4, 7, 100, 3};
    VectorBlock *b = make_int64_block(vals);
    ASSERT_NE(b, nullptr);

    const int sel[3] = {4, 0, 4};  /* 取值 3、10、3——第 4 行被算两次 */
    EXPECT_DOUBLE_EQ(agg_ok(b, 0, VECX_AGG_COUNT, sel, 3), 3.0);
    EXPECT_DOUBLE_EQ(agg_ok(b, 0, VECX_AGG_SUM, sel, 3), 16.0);
    EXPECT_DOUBLE_EQ(agg_ok(b, 0, VECX_AGG_MIN, sel, 3), 3.0);
    EXPECT_DOUBLE_EQ(agg_ok(b, 0, VECX_AGG_MAX, sel, 3), 10.0);
    EXPECT_DOUBLE_EQ(agg_ok(b, 0, VECX_AGG_AVG, sel, 3), 16.0 / 3.0);

    vector_block_destroy(b);
}

/* ========================================================================
 * 6. null 行跳过：COUNT 只计非 null 行，SUM/MIN/MAX 不含 null 行的值
 * ======================================================================== */
TEST(VecxAggScalar, NullRowsSkipped) {
    /* null 行故意放上极端值：若实现没跳过 null，MIN/MAX/SUM 都会被带偏 */
    const std::vector<int64_t> vals = {10, -1000, 7, 9999, 3};
    VectorBlock *b = make_int64_block(vals);
    ASSERT_NE(b, nullptr);
    vector_block_set_null(b, 1, true);  /* -1000 */
    vector_block_set_null(b, 3, true);  /* 9999 */

    /* 剩 10、7、3：count=3，sum=20，min=3，max=10，avg=20/3 */
    EXPECT_DOUBLE_EQ(agg_ok(b, 0, VECX_AGG_COUNT, nullptr, 0), 3.0);
    EXPECT_DOUBLE_EQ(agg_ok(b, 0, VECX_AGG_SUM, nullptr, 0), 20.0);
    EXPECT_DOUBLE_EQ(agg_ok(b, 0, VECX_AGG_MIN, nullptr, 0), 3.0);
    EXPECT_DOUBLE_EQ(agg_ok(b, 0, VECX_AGG_MAX, nullptr, 0), 10.0);
    EXPECT_DOUBLE_EQ(agg_ok(b, 0, VECX_AGG_AVG, nullptr, 0), 20.0 / 3.0);

    /* 选择向量里显式点名 null 行，也照样跳过：sel={1,2,3} 只剩 7 */
    const int sel[3] = {1, 2, 3};
    EXPECT_DOUBLE_EQ(agg_ok(b, 0, VECX_AGG_COUNT, sel, 3), 1.0);
    EXPECT_DOUBLE_EQ(agg_ok(b, 0, VECX_AGG_SUM, sel, 3), 7.0);
    EXPECT_DOUBLE_EQ(agg_ok(b, 0, VECX_AGG_AVG, sel, 3), 7.0);

    vector_block_destroy(b);
}

/* ========================================================================
 * 7. 全 null：COUNT=0 且有结果；SUM/MIN/MAX/AVG 均无结果（SQL NULL）
 * ======================================================================== */
TEST(VecxAggScalar, AllNullRows) {
    const std::vector<double> vals = {1.0, 2.0, 3.0};
    VectorBlock *b = make_double_block(vals);
    ASSERT_NE(b, nullptr);
    for (int i = 0; i < 3; i++) vector_block_set_null(b, i, true);

    expect_empty_set_semantics(b, 0, nullptr, 0);
    /* 选择向量路径同样是空集 */
    const int sel[3] = {0, 1, 2};
    expect_empty_set_semantics(b, 0, sel, 3);

    vector_block_destroy(b);
}

/* ========================================================================
 * 8. 空选择：sel 非 NULL 但 nsel=0 —— 合法空集，不是入参错误
 * ======================================================================== */
TEST(VecxAggScalar, EmptySelection) {
    const std::vector<int32_t> vals = {1, 2, 3, 4};
    VectorBlock *b = make_int32_block(vals);
    ASSERT_NE(b, nullptr);

    const int sel[1] = {0};  /* 指针有效，但 nsel=0 → 一行都不遍历 */
    expect_empty_set_semantics(b, 0, sel, 0);

    vector_block_destroy(b);
}

/* ========================================================================
 * 9. num_rows=0 的块（sel=NULL）
 * ======================================================================== */
TEST(VecxAggScalar, EmptyBlock) {
    VectorBlock *b = make_int64_block({});
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->num_rows, 0);

    expect_empty_set_semantics(b, 0, nullptr, 0);

    vector_block_destroy(b);
}

/* ========================================================================
 * 10. 大整数精度：必须用 int64 累加器，不能退化成 double 累加
 * ======================================================================== */
TEST(VecxAggScalar, Int64SumKeepsPrecision) {
    /* 三个约 4e18/3 的值：和不溢出 int64，但远超 2^53 */
    const int64_t a = 1333333333333333333LL;
    const int64_t bb = 1333333333333333331LL;
    const int64_t c = 1333333333333333329LL;
    const int64_t exact = a + bb + c;  /* = 3999999999999999993，未溢出 */
    VectorBlock *blk = make_int64_block({a, bb, c});
    ASSERT_NE(blk, nullptr);

    EXPECT_DOUBLE_EQ(agg_ok(blk, 0, VECX_AGG_SUM, nullptr, 0), (double)exact);
    EXPECT_DOUBLE_EQ(agg_ok(blk, 0, VECX_AGG_MIN, nullptr, 0), (double)c);
    EXPECT_DOUBLE_EQ(agg_ok(blk, 0, VECX_AGG_MAX, nullptr, 0), (double)a);
    EXPECT_DOUBLE_EQ(agg_ok(blk, 0, VECX_AGG_AVG, nullptr, 0), (double)exact / 3.0);
    vector_block_destroy(blk);

    /*
     * 上面那组量级虽大，但 double 逐个累加的舍入恰好也能落回同一个 double，
     * 并不能真正区分两种累加器。下面这组是判别性用例：
     * 2^62 的 ULP 是 1024，用 double 累加时 2^62 + 512 会"就近取偶"回到 2^62，
     * 再加 512 仍是 2^62；而 int64 精确和 2^62+1024 可被 double 精确表示。
     * 实现若改用 double 累加，这一条必挂。
     */
    const int64_t big = 4611686018427387904LL;  /* 2^62 */
    VectorBlock *blk2 = make_int64_block({big, 512, 512});
    ASSERT_NE(blk2, nullptr);
    EXPECT_DOUBLE_EQ(agg_ok(blk2, 0, VECX_AGG_SUM, nullptr, 0), 4611686018427388928.0);
    vector_block_destroy(blk2);
}

/* ========================================================================
 * 11. 负数与混合符号：MIN/MAX 不能用 0 做初值
 * ======================================================================== */
TEST(VecxAggScalar, NegativeAndMixedSigns) {
    /* 全负：若 MIN/MAX 用 0 起步，MAX 会错成 0 */
    VectorBlock *neg = make_int32_block({-5, -100, -1, -42});
    ASSERT_NE(neg, nullptr);
    EXPECT_DOUBLE_EQ(agg_ok(neg, 0, VECX_AGG_MIN, nullptr, 0), -100.0);
    EXPECT_DOUBLE_EQ(agg_ok(neg, 0, VECX_AGG_MAX, nullptr, 0), -1.0);
    EXPECT_DOUBLE_EQ(agg_ok(neg, 0, VECX_AGG_SUM, nullptr, 0), -148.0);
    EXPECT_DOUBLE_EQ(agg_ok(neg, 0, VECX_AGG_AVG, nullptr, 0), -148.0 / 4.0);
    vector_block_destroy(neg);

    /* 全正：若 MIN 用 0 起步，MIN 会错成 0 */
    VectorBlock *pos = make_double_block({3.5, 8.0, 1.25});
    ASSERT_NE(pos, nullptr);
    EXPECT_DOUBLE_EQ(agg_ok(pos, 0, VECX_AGG_MIN, nullptr, 0), 1.25);
    EXPECT_DOUBLE_EQ(agg_ok(pos, 0, VECX_AGG_MAX, nullptr, 0), 8.0);
    vector_block_destroy(pos);

    /* 混合符号 */
    VectorBlock *mix = make_int64_block({-3, 7, -20, 4, 0});
    ASSERT_NE(mix, nullptr);
    EXPECT_DOUBLE_EQ(agg_ok(mix, 0, VECX_AGG_MIN, nullptr, 0), -20.0);
    EXPECT_DOUBLE_EQ(agg_ok(mix, 0, VECX_AGG_MAX, nullptr, 0), 7.0);
    EXPECT_DOUBLE_EQ(agg_ok(mix, 0, VECX_AGG_SUM, nullptr, 0), -12.0);
    vector_block_destroy(mix);
}

/* ========================================================================
 * 12. 极值/特殊值：±inf 能被 MIN/MAX 正确返回（不是 DBL_MAX 哨兵）
 * ======================================================================== */
TEST(VecxAggScalar, InfinityExtremes) {
    VectorBlock *b = make_double_block({1.0, -INFINITY, 5.0, INFINITY});
    ASSERT_NE(b, nullptr);

    EXPECT_DOUBLE_EQ(agg_ok(b, 0, VECX_AGG_MIN, nullptr, 0), -INFINITY);
    EXPECT_DOUBLE_EQ(agg_ok(b, 0, VECX_AGG_MAX, nullptr, 0), INFINITY);
    EXPECT_DOUBLE_EQ(agg_ok(b, 0, VECX_AGG_COUNT, nullptr, 0), 4.0);
    vector_block_destroy(b);

    /* 只含 +inf 一侧时 SUM 也应是 +inf（不被 DBL_MAX 之类的哨兵截断） */
    VectorBlock *pinf = make_double_block({1.0, INFINITY, 2.0});
    ASSERT_NE(pinf, nullptr);
    EXPECT_DOUBLE_EQ(agg_ok(pinf, 0, VECX_AGG_SUM, nullptr, 0), INFINITY);
    EXPECT_DOUBLE_EQ(agg_ok(pinf, 0, VECX_AGG_MAX, nullptr, 0), INFINITY);
    EXPECT_DOUBLE_EQ(agg_ok(pinf, 0, VECX_AGG_MIN, nullptr, 0), 1.0);
    vector_block_destroy(pinf);
}

/* ========================================================================
 * 13. 入参错误：一律返回 -1，且 *has_result 被置 0
 * ======================================================================== */
TEST(VecxAggScalar, InvalidArgs) {
    VectorBlock *b = make_int32_block({1, 2, 3});
    ASSERT_NE(b, nullptr);

    double out = -12345.0;
    int has = 7;  /* 故意填脏值，验证被置 0 */

    EXPECT_EQ(vecx_agg_scalar(nullptr, 0, VECX_AGG_SUM, nullptr, 0, &out, &has), -1);
    EXPECT_EQ(has, 0);
    EXPECT_DOUBLE_EQ(out, 0.0);

    has = 7;
    EXPECT_EQ(vecx_agg_scalar(b, -1, VECX_AGG_SUM, nullptr, 0, &out, &has), -1);
    EXPECT_EQ(has, 0);

    has = 7;
    EXPECT_EQ(vecx_agg_scalar(b, 1, VECX_AGG_SUM, nullptr, 0, &out, &has), -1);
    EXPECT_EQ(has, 0);

    has = 7;
    EXPECT_EQ(vecx_agg_scalar(b, 99, VECX_AGG_SUM, nullptr, 0, &out, &has), -1);
    EXPECT_EQ(has, 0);

    /* kind 越界 */
    has = 7;
    EXPECT_EQ(vecx_agg_scalar(b, 0, (vecx_agg_kind_t)99, nullptr, 0, &out, &has), -1);
    EXPECT_EQ(has, 0);

    /* out / has_result 为 NULL：不崩溃，返回 -1 */
    EXPECT_EQ(vecx_agg_scalar(b, 0, VECX_AGG_SUM, nullptr, 0, nullptr, &has), -1);
    EXPECT_EQ(vecx_agg_scalar(b, 0, VECX_AGG_SUM, nullptr, 0, &out, nullptr), -1);

    vector_block_destroy(b);

    /* 列类型未设置（-1）→ 不支持 */
    VectorBlock *untyped = vector_block_create(4, 1);
    ASSERT_NE(untyped, nullptr);
    int32_t *col = (int32_t *)calloc(4, sizeof(int32_t));
    vector_block_set_column(untyped, 0, col, (int)sizeof(int32_t));
    vector_block_set_num_rows(untyped, 4);
    ASSERT_EQ(vector_block_get_column_type(untyped, 0), -1);
    has = 7;
    EXPECT_EQ(vecx_agg_scalar(untyped, 0, VECX_AGG_COUNT, nullptr, 0, &out, &has), -1);
    EXPECT_EQ(has, 0);
    EXPECT_DOUBLE_EQ(out, 0.0);
    vector_block_destroy(untyped);

    /* 字符串列 → 非数值，不支持聚合 */
    VectorBlock *strb = vector_block_create(2, 1);
    ASSERT_NE(strb, nullptr);
    const char **scol = (const char **)malloc(sizeof(const char *) * 2);
    scol[0] = "a";
    scol[1] = "b";
    vector_block_set_column(strb, 0, (void *)scol, (int)sizeof(const char *));
    vector_block_set_column_type(strb, 0, COLUMN_STRING);
    vector_block_set_num_rows(strb, 2);
    has = 7;
    EXPECT_EQ(vecx_agg_scalar(strb, 0, VECX_AGG_COUNT, nullptr, 0, &out, &has), -1);
    EXPECT_EQ(has, 0);
    vector_block_destroy(strb);

    /* 列类型标签正确但列缓冲缺失 */
    VectorBlock *nodata = vector_block_create(4, 1);
    ASSERT_NE(nodata, nullptr);
    vector_block_set_column_type(nodata, 0, COLUMN_INT64);
    vector_block_set_num_rows(nodata, 4);
    has = 7;
    EXPECT_EQ(vecx_agg_scalar(nodata, 0, VECX_AGG_SUM, nullptr, 0, &out, &has), -1);
    EXPECT_EQ(has, 0);
    vector_block_destroy(nodata);
}

/* ========================================================================
 * 14. 聚合是只读算子：跑完一轮后输入块逐字节不变
 * ======================================================================== */
TEST(VecxAggScalar, InputBlockUnmodified) {
    const std::vector<int64_t> vals = {10, -4, 7, 100, 3, 88};
    VectorBlock *b = make_int64_block(vals);
    ASSERT_NE(b, nullptr);
    vector_block_set_null(b, 2, true);
    vector_block_set_null(b, 5, true);

    /* 快照：行数、列内容、null 位图、类型标签、列元素大小 */
    const int rows_before = b->num_rows;
    const int cols_before = b->num_columns;
    const int elem_before = b->column_sizes[0];
    const int type_before = vector_block_get_column_type(b, 0);
    const int nwords = (b->capacity + 63) / 64;
    std::vector<int64_t> data_before(vals.size());
    memcpy(data_before.data(), b->columns[0], sizeof(int64_t) * vals.size());
    std::vector<uint64_t> null_before((size_t)nwords);
    memcpy(null_before.data(), b->null_bitmap, sizeof(uint64_t) * (size_t)nwords);

    const int sel[4] = {5, 1, 1, 0};
    const vecx_agg_kind_t kinds[5] = {VECX_AGG_COUNT, VECX_AGG_SUM, VECX_AGG_MIN,
                                      VECX_AGG_MAX, VECX_AGG_AVG};
    for (int k = 0; k < 5; k++) {
        double out = 0.0;
        int has = 0;
        EXPECT_EQ(vecx_agg_scalar(b, 0, kinds[k], nullptr, 0, &out, &has), 0);
        EXPECT_EQ(vecx_agg_scalar(b, 0, kinds[k], sel, 4, &out, &has), 0);
    }

    EXPECT_EQ(b->num_rows, rows_before);
    EXPECT_EQ(b->num_columns, cols_before);
    EXPECT_EQ(b->column_sizes[0], elem_before);
    EXPECT_EQ(vector_block_get_column_type(b, 0), type_before);
    EXPECT_EQ(memcmp(data_before.data(), b->columns[0],
                     sizeof(int64_t) * vals.size()), 0);
    EXPECT_EQ(memcmp(null_before.data(), b->null_bitmap,
                     sizeof(uint64_t) * (size_t)nwords), 0);

    vector_block_destroy(b);
}
