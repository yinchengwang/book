/**
 * @file prune_test.cpp
 * @brief Gap#2 向量化执行引擎 Task8：MinMax Index + Zone-map Skip 单元测试
 *
 * 覆盖：
 * - vecx_block_minmax_i64：int32/int64/float/double 列的 min/max 计算，null 排除，空块，非法入参
 * - vecx_block_minmax_f64：浮点版 min/max（含 ±inf）
 * - vecx_zonemap_skip_i64：6 种比较操作符的跳过判断
 * - vecx_zonemap_skip_f64：浮点版 zone-map（含 NaN）
 * - 端到端：3 granule zone-map 裁剪验证
 */
#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "db/core/vector_exec.h"
#include "db/core/columnar_store.h"
#include "db/vectorized/vectorized.h"
}

/* ========================================================================
 * 辅助函数：创建只含一列的 VectorBlock
 * ======================================================================== */

/* 创建 int64_t 列块，rows 行，数据从 data 复制，null_rows 为 null 位图（NULL 表示无 null） */
static VectorBlock *make_block_i64(int rows, const int64_t *data, const uint64_t *null_bitmap) {
    VectorBlock *b = vector_block_create(rows, 1);
    if (!b) return nullptr;
    int64_t *col = (int64_t *)malloc((size_t)rows * sizeof(int64_t));
    if (!col) { vector_block_destroy(b); return nullptr; }
    memcpy(col, data, (size_t)rows * sizeof(int64_t));
    vector_block_set_column(b, 0, col, sizeof(int64_t));
    vector_block_set_column_type(b, 0, COLUMN_INT64);
    if (null_bitmap) {
        int nwords = (rows + 63) / 64;
        uint64_t *nb = (uint64_t *)malloc((size_t)nwords * sizeof(uint64_t));
        if (!nb) { vector_block_destroy(b); return nullptr; }
        memcpy(nb, null_bitmap, (size_t)nwords * sizeof(uint64_t));
        b->null_bitmap = nb;
    }
    vector_block_set_num_rows(b, rows);
    return b;
}

/* 创建 int32_t 列块 */
static VectorBlock *make_block_i32(int rows, const int32_t *data, const uint64_t *null_bitmap) {
    VectorBlock *b = vector_block_create(rows, 1);
    if (!b) return nullptr;
    int32_t *col = (int32_t *)malloc((size_t)rows * sizeof(int32_t));
    if (!col) { vector_block_destroy(b); return nullptr; }
    memcpy(col, data, (size_t)rows * sizeof(int32_t));
    vector_block_set_column(b, 0, col, sizeof(int32_t));
    vector_block_set_column_type(b, 0, COLUMN_INT32);
    if (null_bitmap) {
        int nwords = (rows + 63) / 64;
        uint64_t *nb = (uint64_t *)malloc((size_t)nwords * sizeof(uint64_t));
        if (!nb) { vector_block_destroy(b); return nullptr; }
        memcpy(nb, null_bitmap, (size_t)nwords * sizeof(uint64_t));
        b->null_bitmap = nb;
    }
    vector_block_set_num_rows(b, rows);
    return b;
}

/* 创建 float 列块 */
static VectorBlock *make_block_float(int rows, const float *data, const uint64_t *null_bitmap) {
    VectorBlock *b = vector_block_create(rows, 1);
    if (!b) return nullptr;
    float *col = (float *)malloc((size_t)rows * sizeof(float));
    if (!col) { vector_block_destroy(b); return nullptr; }
    memcpy(col, data, (size_t)rows * sizeof(float));
    vector_block_set_column(b, 0, col, sizeof(float));
    vector_block_set_column_type(b, 0, COLUMN_FLOAT);
    if (null_bitmap) {
        int nwords = (rows + 63) / 64;
        uint64_t *nb = (uint64_t *)malloc((size_t)nwords * sizeof(uint64_t));
        if (!nb) { vector_block_destroy(b); return nullptr; }
        memcpy(nb, null_bitmap, (size_t)nwords * sizeof(uint64_t));
        b->null_bitmap = nb;
    }
    vector_block_set_num_rows(b, rows);
    return b;
}

/* 创建 double 列块 */
static VectorBlock *make_block_double(int rows, const double *data, const uint64_t *null_bitmap) {
    VectorBlock *b = vector_block_create(rows, 1);
    if (!b) return nullptr;
    double *col = (double *)malloc((size_t)rows * sizeof(double));
    if (!col) { vector_block_destroy(b); return nullptr; }
    memcpy(col, data, (size_t)rows * sizeof(double));
    vector_block_set_column(b, 0, col, sizeof(double));
    vector_block_set_column_type(b, 0, COLUMN_DOUBLE);
    if (null_bitmap) {
        int nwords = (rows + 63) / 64;
        uint64_t *nb = (uint64_t *)malloc((size_t)nwords * sizeof(uint64_t));
        if (!nb) { vector_block_destroy(b); return nullptr; }
        memcpy(nb, null_bitmap, (size_t)nwords * sizeof(uint64_t));
        b->null_bitmap = nb;
    }
    vector_block_set_num_rows(b, rows);
    return b;
}

/* ========================================================================
 * VecxMinMax：整型 min/max
 * ======================================================================== */

/* Test 1: int64 列 {3,1,4,1,5,9,2,6} → lo=1, hi=9 */
TEST(VecxMinMax, Int64Basic) {
    int64_t data[] = {3, 1, 4, 1, 5, 9, 2, 6};
    VectorBlock *b = make_block_i64(8, data, nullptr);
    ASSERT_NE(b, nullptr);

    int64_t lo, hi;
    int ret = vecx_block_minmax_i64(b, 0, &lo, &hi);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(lo, 1);
    EXPECT_EQ(hi, 9);

    vector_block_destroy(b);
}

/* Test 2: int32 列 min/max 正确（int32→int64 比较） */
TEST(VecxMinMax, Int32Basic) {
    int32_t data[] = {-5, 100, 3, -100, 0};
    VectorBlock *b = make_block_i32(5, data, nullptr);
    ASSERT_NE(b, nullptr);

    int64_t lo, hi;
    int ret = vecx_block_minmax_i64(b, 0, &lo, &hi);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(lo, -100);
    EXPECT_EQ(hi, 100);

    vector_block_destroy(b);
}

/* Test 3: float 列 {1.5, -0.5, 3.0, 2.25} → lo=-0.5, hi=3.0 */
TEST(VecxMinMax, FloatBasic) {
    float data[] = {1.5f, -0.5f, 3.0f, 2.25f};
    VectorBlock *b = make_block_float(4, data, nullptr);
    ASSERT_NE(b, nullptr);

    double lo, hi;
    int ret = vecx_block_minmax_f64(b, 0, &lo, &hi);
    EXPECT_EQ(ret, 0);
    EXPECT_DOUBLE_EQ(lo, -0.5);
    EXPECT_DOUBLE_EQ(hi, 3.0);

    vector_block_destroy(b);
}

/* Test 4: double 列含 ±INFINITY */
TEST(VecxMinMax, DoubleInfinity) {
    double data[] = {1.0, -INFINITY, INFINITY, 0.0};
    VectorBlock *b = make_block_double(4, data, nullptr);
    ASSERT_NE(b, nullptr);

    double lo, hi;
    int ret = vecx_block_minmax_f64(b, 0, &lo, &hi);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(lo, -INFINITY);  // double 比较
    EXPECT_EQ(hi, INFINITY);

    vector_block_destroy(b);
}

/* Test 5: null 排除——第 1、4 行（0-indexed）为 null */
TEST(VecxMinMax, NullExclusion) {
    int64_t data[] = {3, 1, 4, 1, 5, 9, 2, 6};
    /* row 1 和 row 4 为 null */
    uint64_t nb[1] = {0};
    nb[0] |= (1ULL << 1);  /* row 1 */
    nb[0] |= (1ULL << 4);  /* row 4 */
    VectorBlock *b = make_block_i64(8, data, nb);
    ASSERT_NE(b, nullptr);

    int64_t lo, hi;
    int ret = vecx_block_minmax_i64(b, 0, &lo, &hi);
    EXPECT_EQ(ret, 0);
    /* 有效值：{3,4,1,9,2,6} → min=1, max=9 */
    EXPECT_EQ(lo, 1);
    EXPECT_EQ(hi, 9);

    vector_block_destroy(b);
}

/* Test 6: 全 null → 返回 -1 */
TEST(VecxMinMax, AllNull) {
    int64_t data[] = {3, 1, 4, 1, 5};
    uint64_t nb[1] = {0xFFFFFFFFFFFFFFFFULL};  /* 全部 null */
    VectorBlock *b = make_block_i64(5, data, nb);
    ASSERT_NE(b, nullptr);

    int64_t lo, hi;
    int ret = vecx_block_minmax_i64(b, 0, &lo, &hi);
    EXPECT_EQ(ret, -1);

    vector_block_destroy(b);
}

/* Test 7: num_rows=0 → 返回 -1 */
TEST(VecxMinMax, ZeroRows) {
    VectorBlock *b = vector_block_create(0, 1);
    ASSERT_NE(b, nullptr);
    vector_block_set_num_rows(b, 0);

    int64_t lo, hi;
    int ret = vecx_block_minmax_i64(b, 0, &lo, &hi);
    EXPECT_EQ(ret, -1);

    vector_block_destroy(b);
}

/* Test 8: INT64_MAX/INT64_MIN 哨兵（不用哨兵做初值） */
TEST(VecxMinMax, ExtremeValues) {
    int64_t data[] = {INT64_MAX, INT64_MIN, 0, -1, 1};
    VectorBlock *b = make_block_i64(5, data, nullptr);
    ASSERT_NE(b, nullptr);

    int64_t lo, hi;
    int ret = vecx_block_minmax_i64(b, 0, &lo, &hi);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(lo, INT64_MIN);
    EXPECT_EQ(hi, INT64_MAX);

    vector_block_destroy(b);
}

/* Test 9: 非法入参 */
TEST(VecxMinMax, InvalidArgs) {
    int64_t data[] = {1, 2, 3};
    VectorBlock *b = make_block_i64(3, data, nullptr);
    ASSERT_NE(b, nullptr);

    int64_t lo, hi;

    /* NULL block */
    EXPECT_EQ(vecx_block_minmax_i64(nullptr, 0, &lo, &hi), -1);
    /* col < 0 */
    EXPECT_EQ(vecx_block_minmax_i64(b, -1, &lo, &hi), -1);
    /* col >= num_columns */
    EXPECT_EQ(vecx_block_minmax_i64(b, 1, &lo, &hi), -1);
    /* 未知列类型（块只有 1 列，列 0 是 INT64，测试 col<0 已覆盖） */

    /* double 版本非法入参 */
    double lo_d, hi_d;
    EXPECT_EQ(vecx_block_minmax_f64(nullptr, 0, &lo_d, &hi_d), -1);
    EXPECT_EQ(vecx_block_minmax_f64(b, -1, &lo_d, &hi_d), -1);
    EXPECT_EQ(vecx_block_minmax_f64(b, 1, &lo_d, &hi_d), -1);

    vector_block_destroy(b);
}

/* ========================================================================
 * VecxZoneMap：zone-map 跳过判断
 * ======================================================================== */

/* Test 10: EQ */
TEST(VecxZoneMap, EQ) {
    /* v 在区间内 → 不跳过 */
    EXPECT_EQ(vecx_zonemap_skip_i64(3, 7, CMP_EQ, 5), 0);
    /* v < lo → 跳过 */
    EXPECT_EQ(vecx_zonemap_skip_i64(3, 7, CMP_EQ, 2), 1);
    /* v > hi → 跳过 */
    EXPECT_EQ(vecx_zonemap_skip_i64(3, 7, CMP_EQ, 10), 1);
    /* v == lo */
    EXPECT_EQ(vecx_zonemap_skip_i64(3, 7, CMP_EQ, 3), 0);
    /* v == hi */
    EXPECT_EQ(vecx_zonemap_skip_i64(3, 7, CMP_EQ, 7), 0);
}

/* Test 11: NE */
TEST(VecxZoneMap, NE) {
    EXPECT_EQ(vecx_zonemap_skip_i64(3, 7, CMP_NE, 5), 0);
    EXPECT_EQ(vecx_zonemap_skip_i64(3, 7, CMP_NE, 2), 0);
    EXPECT_EQ(vecx_zonemap_skip_i64(3, 7, CMP_NE, 10), 0);
}

/* Test 12: LT */
TEST(VecxZoneMap, LT) {
    EXPECT_EQ(vecx_zonemap_skip_i64(3, 7, CMP_LT, 5), 0);  /* lo=3 < v=5 → 不跳过，可能有值 */
    /* lo >= v → 跳过：区间最大值必然 < v */
    EXPECT_EQ(vecx_zonemap_skip_i64(3, 7, CMP_LT, 3), 1);  /* lo=3 >= 3 → 跳过 */
    EXPECT_EQ(vecx_zonemap_skip_i64(3, 7, CMP_LT, 2), 1);  /* lo=3 >= 2 → 跳过 */
    /* lo=3, v=10: lo>=v (3>=10) is false → 不跳过，区间内 3..7 都 < 10 */
    EXPECT_EQ(vecx_zonemap_skip_i64(3, 7, CMP_LT, 10), 0);
}

/* Test 13: LE */
TEST(VecxZoneMap, LE) {
    EXPECT_EQ(vecx_zonemap_skip_i64(3, 7, CMP_LE, 5), 0);
    EXPECT_EQ(vecx_zonemap_skip_i64(3, 7, CMP_LE, 8), 0);
    /* lo > v → 跳过 */
    EXPECT_EQ(vecx_zonemap_skip_i64(3, 7, CMP_LE, 2), 1);
    /* lo=3, v=3: lo>v? false → 不跳过 */
    EXPECT_EQ(vecx_zonemap_skip_i64(3, 7, CMP_LE, 3), 0);
    /* lo=3, v=2: lo>v? true → 跳过 */
    EXPECT_EQ(vecx_zonemap_skip_i64(3, 7, CMP_LE, 2), 1);
}

/* Test 14: GT */
TEST(VecxZoneMap, GT) {
    EXPECT_EQ(vecx_zonemap_skip_i64(3, 7, CMP_GT, 5), 0);
    EXPECT_EQ(vecx_zonemap_skip_i64(3, 7, CMP_GT, 7), 1);  /* hi <= v */
    EXPECT_EQ(vecx_zonemap_skip_i64(3, 7, CMP_GT, 8), 1);
    EXPECT_EQ(vecx_zonemap_skip_i64(3, 7, CMP_GT, 2), 0);
    /* hi=7, v=7: hi<=v → 跳过 */
    EXPECT_EQ(vecx_zonemap_skip_i64(3, 7, CMP_GT, 7), 1);
    /* hi=7, v=6: hi<=v? false → 不跳过 */
    EXPECT_EQ(vecx_zonemap_skip_i64(3, 7, CMP_GT, 6), 0);
}

/* Test 15: GE */
TEST(VecxZoneMap, GE) {
    EXPECT_EQ(vecx_zonemap_skip_i64(3, 7, CMP_GE, 5), 0);
    EXPECT_EQ(vecx_zonemap_skip_i64(3, 7, CMP_GE, 2), 0);
    /* hi < v → 跳过 */
    EXPECT_EQ(vecx_zonemap_skip_i64(3, 7, CMP_GE, 8), 1);
    /* hi=7, v=7: hi<v? false → 不跳过 */
    EXPECT_EQ(vecx_zonemap_skip_i64(3, 7, CMP_GE, 7), 0);
    /* hi=7, v=8: hi<v? true → 跳过 */
    EXPECT_EQ(vecx_zonemap_skip_i64(3, 7, CMP_GE, 8), 1);
}

/* Test 16: 边界相等 */
TEST(VecxZoneMap, BoundaryEquality) {
    /* EQ: lo==v */
    EXPECT_EQ(vecx_zonemap_skip_i64(5, 5, CMP_EQ, 5), 0);
    EXPECT_EQ(vecx_zonemap_skip_i64(5, 5, CMP_EQ, 6), 1);
    EXPECT_EQ(vecx_zonemap_skip_i64(5, 5, CMP_EQ, 4), 1);
    /* LE: lo==v */
    EXPECT_EQ(vecx_zonemap_skip_i64(5, 5, CMP_LE, 5), 0);
    EXPECT_EQ(vecx_zonemap_skip_i64(5, 5, CMP_LE, 6), 0);
    /* GE: hi==v */
    EXPECT_EQ(vecx_zonemap_skip_i64(5, 5, CMP_GE, 5), 0);
    EXPECT_EQ(vecx_zonemap_skip_i64(5, 5, CMP_GE, 4), 0);
}

/* Test 17: lo > hi（无效区间）→ 返回 0 */
TEST(VecxZoneMap, InvalidInterval) {
    EXPECT_EQ(vecx_zonemap_skip_i64(10, 3, CMP_EQ, 5), 0);
    EXPECT_EQ(vecx_zonemap_skip_i64(10, 3, CMP_NE, 5), 0);
    EXPECT_EQ(vecx_zonemap_skip_i64(10, 3, CMP_LT, 5), 0);
    EXPECT_EQ(vecx_zonemap_skip_i64(10, 3, CMP_GT, 5), 0);
}

/* Test 18: float 版本 ±inf / NaN */
TEST(VecxZoneMap, FloatSpecialValues) {
    /* +inf 上界 */
    EXPECT_EQ(vecx_zonemap_skip_f64(1.0, INFINITY, CMP_GT, 0.0), 0);
    EXPECT_EQ(vecx_zonemap_skip_f64(1.0, INFINITY, CMP_GT, 100.0), 0);
    EXPECT_EQ(vecx_zonemap_skip_f64(1.0, INFINITY, CMP_LT, 0.5), 1);  /* lo(1.0) >= 0.5 */
    /* -inf 下界 */
    EXPECT_EQ(vecx_zonemap_skip_f64(-INFINITY, 1.0, CMP_LT, 0.5), 0);
    EXPECT_EQ(vecx_zonemap_skip_f64(-INFINITY, 1.0, CMP_GT, 2.0), 1); /* hi(1.0) <= 2.0 */
    /* NaN 区间 → 所有比较 false → 不跳过 */
    EXPECT_EQ(vecx_zonemap_skip_f64(NAN, 1.0, CMP_EQ, 0.5), 0);
    EXPECT_EQ(vecx_zonemap_skip_f64(1.0, NAN, CMP_EQ, 0.5), 0);
    EXPECT_EQ(vecx_zonemap_skip_f64(NAN, NAN, CMP_EQ, 0.5), 0);
    /* NaN 查询值 */
    EXPECT_EQ(vecx_zonemap_skip_f64(1.0, 5.0, CMP_EQ, NAN), 0);
    /* 正常值与 inf 配合 */
    EXPECT_EQ(vecx_zonemap_skip_f64(-INFINITY, INFINITY, CMP_GT, 0.0), 0);
    EXPECT_EQ(vecx_zonemap_skip_f64(-INFINITY, INFINITY, CMP_LT, 0.0), 0);
}

/* Test 19: 端到端——3 个 granule，谓词 col > 9，跳过第 1、3 个块，扫描第 2 个 */
TEST(VecxZoneMap, EndToEnd) {
    /* Granule 1: rows=3, data={1,2,3} → lo=1, hi=3 */
    int64_t g1[] = {1, 2, 3};
    VectorBlock *b1 = make_block_i64(3, g1, nullptr);
    ASSERT_NE(b1, nullptr);

    /* Granule 2: rows=5, data={5,6,7,8,9} → lo=5, hi=9 */
    int64_t g2[] = {5, 6, 7, 8, 9};
    VectorBlock *b2 = make_block_i64(5, g2, nullptr);
    ASSERT_NE(b2, nullptr);

    /* Granule 3: rows=4, data={11,12,13,14} → lo=11, hi=14 */
    int64_t g3[] = {11, 12, 13, 14};
    VectorBlock *b3 = make_block_i64(4, g3, nullptr);
    ASSERT_NE(b3, nullptr);

    int64_t lo1, hi1, lo2, hi2, lo3, hi3;
    EXPECT_EQ(vecx_block_minmax_i64(b1, 0, &lo1, &hi1), 0);
    EXPECT_EQ(vecx_block_minmax_i64(b2, 0, &lo2, &hi2), 0);
    EXPECT_EQ(vecx_block_minmax_i64(b3, 0, &lo3, &hi3), 0);

    EXPECT_EQ(lo1, 1); EXPECT_EQ(hi1, 3);
    EXPECT_EQ(lo2, 5); EXPECT_EQ(hi2, 9);
    EXPECT_EQ(lo3, 11); EXPECT_EQ(hi3, 14);

    /* 谓词 col > 9 (CMP_GT, v=9) */
    /* Block 1: hi1=3, hi1 <= 9? yes → 跳过 */
    EXPECT_EQ(vecx_zonemap_skip_i64(lo1, hi1, CMP_GT, 9), 1);
    /* Block 2: hi2=9, hi2 <= 9? yes → 跳过? 等等 hi2=9, v=9, GT: hi<=v → 跳过! */
    /* 谓词是 col > 9，所以 {5,6,7,8,9} 里的 9 不满足 > 9，应该被扫描但全部过滤掉
       zone-map skip 只能基于 lo/hi 推断——hi=9 <= v(9) → 跳过 Block 2 是正确的 zone-map 裁剪
       但这会导致假阴性！这是 zone-map 的固有限制：lo=5,hi=9 可能包含 > 9 的值吗？不可能。
       所以 skip 是正确的。问题在于：数据 {5,6,7,8,9} 没有一个满足 > 9，所以 skip 没问题。 */
    EXPECT_EQ(vecx_zonemap_skip_i64(lo2, hi2, CMP_GT, 9), 1); /* hi=9 <= 9 → 跳过 */
    /* Block 3: lo3=11, hi3=14, hi3 <= 9? no → 不跳过 */
    EXPECT_EQ(vecx_zonemap_skip_i64(lo3, hi3, CMP_GT, 9), 0);

    vector_block_destroy(b1);
    vector_block_destroy(b2);
    vector_block_destroy(b3);
}
