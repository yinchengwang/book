/**
 * @file hashagg_test.cpp
 * @brief Gap#2 向量化执行引擎 Task5 单元测试：哈希分组聚合算子
 *
 * 覆盖：
 * - 单块基础：int64 键 + int64 度量，3 分组，count/sum/min/max/avg 手算一致
 * - 多块累积：同一 key 散在 3 块，跨块 min/max 更新正确
 * - distinct 组数：emit 返回值 == 输出块 num_rows == 期望 distinct 数
 * - 输出块 schema：6 列，类型标签 INT64/INT64/DOUBLE/DOUBLE/DOUBLE/DOUBLE
 * - null 跳过：键 null 或度量 null → 整行跳过，该 key 不出现在输出
 * - key=0 与负数 key：空槽哨兵不是 key==0
 * - 扩容：~5000 distinct 键 × 2 行，钉死 rehash 扩容
 * - 哈希冲突正确性
 * - int32 键 + float 度量：类型提升路径正确
 * - 空输入：0 块 emit → 返回 0 且 *out=NULL
 * - 全 null 块：所有行 null → 输出 0 组
 * - emit 幂等：两次 emit 结果等价
 * - 不支持的列类型：COLUMN_STRING → add_block 返回 -1，状态未破坏
 * - 入参非法：h=NULL / b=NULL / out=NULL / destroy(NULL) 不崩
 * - 不修改输入块
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
#include <map>
#include <tuple>
#include <vector>

namespace {

/**
 * 建一个 2 列的块：键列（col 0）+ 度量列（col 1）。
 * key_type / measure_type 用 COLUMN_* 枚举。
 * 所有权：调用方用 vector_block_destroy 释放。
 */
VectorBlock *make_two_col_block(
    const std::vector<int64_t> &keys,
    const std::vector<int64_t> &measures,
    int key_type,
    int measure_type) {
    int n = (int)keys.size();
    int cap = n > 0 ? n : 1;
    VectorBlock *b = vector_block_create(cap, 2);
    if (!b) return nullptr;

    if (key_type == COLUMN_INT32) {
        int32_t *col = (int32_t *)malloc(sizeof(int32_t) * (size_t)cap);
        for (int i = 0; i < n; i++) col[i] = (int32_t)keys[(size_t)i];
        vector_block_set_column(b, 0, col, (int)sizeof(int32_t));
        vector_block_set_column_type(b, 0, key_type);
    } else {
        int64_t *col = (int64_t *)malloc(sizeof(int64_t) * (size_t)cap);
        for (int i = 0; i < n; i++) col[i] = keys[(size_t)i];
        vector_block_set_column(b, 0, col, (int)sizeof(int64_t));
        vector_block_set_column_type(b, 0, key_type);
    }

    if (measure_type == COLUMN_INT32) {
        int32_t *col = (int32_t *)malloc(sizeof(int32_t) * (size_t)cap);
        for (int i = 0; i < n; i++) col[i] = (int32_t)measures[(size_t)i];
        vector_block_set_column(b, 1, col, (int)sizeof(int32_t));
        vector_block_set_column_type(b, 1, measure_type);
    } else if (measure_type == COLUMN_INT64) {
        int64_t *col = (int64_t *)malloc(sizeof(int64_t) * (size_t)cap);
        for (int i = 0; i < n; i++) col[i] = measures[(size_t)i];
        vector_block_set_column(b, 1, col, (int)sizeof(int64_t));
        vector_block_set_column_type(b, 1, measure_type);
    } else if (measure_type == COLUMN_FLOAT) {
        float *col = (float *)malloc(sizeof(float) * (size_t)cap);
        for (int i = 0; i < n; i++) col[i] = (float)measures[(size_t)i];
        vector_block_set_column(b, 1, col, (int)sizeof(float));
        vector_block_set_column_type(b, 1, measure_type);
    } else {
        double *col = (double *)malloc(sizeof(double) * (size_t)cap);
        for (int i = 0; i < n; i++) col[i] = (double)measures[(size_t)i];
        vector_block_set_column(b, 1, col, (int)sizeof(double));
        vector_block_set_column_type(b, 1, measure_type);
    }

    vector_block_set_num_rows(b, n);
    return b;
}

/** 建一个 int64 键 + int64 度量的 2 列块 */
VectorBlock *make_i64_i64_block(
    const std::vector<int64_t> &keys,
    const std::vector<int64_t> &measures) {
    return make_two_col_block(keys, measures, COLUMN_INT64, COLUMN_INT64);
}

/**
 * 建一个 int32 键 + float 度量的 2 列块。
 * 度量直接收 float，好让测试能用 1.5f 这类非整数值——整数值在 float 和
 * double 里都精确可表示，截断成整型的 bug 反而测不出来。
 */
VectorBlock *make_i32_f(
    const std::vector<int64_t> &keys,
    const std::vector<float> &measures) {
    int n = (int)keys.size();
    int cap = n > 0 ? n : 1;
    VectorBlock *b = vector_block_create(cap, 2);
    if (!b) return nullptr;

    int32_t *kcol = (int32_t *)malloc(sizeof(int32_t) * (size_t)cap);
    for (int i = 0; i < n; i++) kcol[i] = (int32_t)keys[(size_t)i];
    vector_block_set_column(b, 0, kcol, (int)sizeof(int32_t));
    vector_block_set_column_type(b, 0, COLUMN_INT32);

    float *mcol = (float *)malloc(sizeof(float) * (size_t)cap);
    for (int i = 0; i < n; i++) mcol[i] = measures[(size_t)i];
    vector_block_set_column(b, 1, mcol, (int)sizeof(float));
    vector_block_set_column_type(b, 1, COLUMN_FLOAT);

    vector_block_set_num_rows(b, n);
    return b;
}

/**
 * 把 emit 出的块读成 std::map（key → tuple<count,sum,min,max,avg>）。
 * 不依赖行顺序。
 */
std::map<int64_t, std::tuple<int64_t, double, double, double, double>>
read_output_block(VectorBlock *out) {
    std::map<int64_t, std::tuple<int64_t, double, double, double, double>> result;
    if (!out) return result;

    const int64_t *keys = (const int64_t *)out->columns[0];
    const int64_t *counts = (const int64_t *)out->columns[1];
    const double *sums = (const double *)out->columns[2];
    const double *mins = (const double *)out->columns[3];
    const double *maxs = (const double *)out->columns[4];
    const double *avgs = (const double *)out->columns[5];

    for (int i = 0; i < out->num_rows; i++) {
        result[keys[i]] = std::make_tuple(
            counts[i], sums[i], mins[i], maxs[i], avgs[i]);
    }
    return result;
}

/** 断言某 key 存在于 map 且各字段与期望值近似相等 */
void expect_group(
    const std::map<int64_t, std::tuple<int64_t, double, double, double, double>> &m,
    int64_t key, int64_t exp_count, double exp_sum,
    double exp_min, double exp_max, double exp_avg) {
    auto it = m.find(key);
    ASSERT_NE(it, m.end()) << "key=" << key << " not found in output";
    const auto &t = it->second;
    EXPECT_EQ(std::get<0>(t), exp_count) << "key=" << key;
    EXPECT_DOUBLE_EQ(std::get<1>(t), exp_sum) << "key=" << key;
    EXPECT_DOUBLE_EQ(std::get<2>(t), exp_min) << "key=" << key;
    EXPECT_DOUBLE_EQ(std::get<3>(t), exp_max) << "key=" << key;
    EXPECT_DOUBLE_EQ(std::get<4>(t), exp_avg) << "key=" << key;
}

}  // namespace

/* ========================================================================
 * 1. 单块基础：int64 键 + int64 度量，3 分组
 * ======================================================================== */
TEST(VecxHashAgg, SingleBlockBasic) {
    // key=1: (1,10), (1,20)  → count=2, sum=30, min=10, max=20, avg=15
    // key=2: (2,30)          → count=1, sum=30, min=30, max=30, avg=30
    // key=3: (3,5), (3,15), (3,25) → count=3, sum=45, min=5, max=25, avg=15
    const std::vector<int64_t> keys = {1, 1, 2, 3, 3, 3};
    const std::vector<int64_t> measures = {10, 20, 30, 5, 15, 25};

    VectorBlock *b = make_i64_i64_block(keys, measures);
    ASSERT_NE(b, nullptr);

    vecx_hashagg_t *h = vecx_hashagg_create(0, 1);
    ASSERT_NE(h, nullptr);

    EXPECT_EQ(vecx_hashagg_add_block(h, b), 0);

    VectorBlock *out = nullptr;
    int n = vecx_hashagg_emit(h, &out);
    EXPECT_EQ(n, 3);
    ASSERT_NE(out, nullptr);
    EXPECT_EQ(out->num_rows, 3);

    auto m = read_output_block(out);
    expect_group(m, 1, 2, 30.0, 10.0, 20.0, 15.0);
    expect_group(m, 2, 1, 30.0, 30.0, 30.0, 30.0);
    expect_group(m, 3, 3, 45.0, 5.0, 25.0, 15.0);

    vector_block_destroy(out);
    vecx_hashagg_destroy(h);
    vector_block_destroy(b);
}

/* ========================================================================
 * 2. 多块累积：同一 key 散在 3 块，跨块 min/max 更新正确
 * ======================================================================== */
TEST(VecxHashAgg, MultiBlockAccumulation) {
    // 块1: key=1 有 (1,10), key=2 有 (2,30)
    // 块2: key=1 有 (1,20), key=3 有 (3,5)
    // 块3: key=1 有 (1,5),  key=3 有 (3,15), (3,25)
    // 期望: key=1: count=3, sum=35, min=5, max=20, avg=35/3
    //       key=2: count=1, sum=30, min=30, max=30, avg=30
    //       key=3: count=3, sum=45, min=5, max=25, avg=15

    const std::vector<int64_t> keys1 = {1, 2};
    const std::vector<int64_t> ms1 = {10, 30};
    VectorBlock *b1 = make_i64_i64_block(keys1, ms1);

    const std::vector<int64_t> keys2 = {1, 3};
    const std::vector<int64_t> ms2 = {20, 5};
    VectorBlock *b2 = make_i64_i64_block(keys2, ms2);

    const std::vector<int64_t> keys3 = {1, 3, 3};
    const std::vector<int64_t> ms3 = {5, 15, 25};
    VectorBlock *b3 = make_i64_i64_block(keys3, ms3);

    vecx_hashagg_t *h = vecx_hashagg_create(0, 1);
    ASSERT_NE(h, nullptr);

    EXPECT_EQ(vecx_hashagg_add_block(h, b1), 0);
    EXPECT_EQ(vecx_hashagg_add_block(h, b2), 0);
    EXPECT_EQ(vecx_hashagg_add_block(h, b3), 0);

    VectorBlock *out = nullptr;
    int n = vecx_hashagg_emit(h, &out);
    EXPECT_EQ(n, 3);
    ASSERT_NE(out, nullptr);

    auto m = read_output_block(out);
    expect_group(m, 1, 3, 35.0, 5.0, 20.0, 35.0 / 3.0);
    expect_group(m, 2, 1, 30.0, 30.0, 30.0, 30.0);
    expect_group(m, 3, 3, 45.0, 5.0, 25.0, 15.0);

    vector_block_destroy(out);
    vecx_hashagg_destroy(h);
    vector_block_destroy(b1);
    vector_block_destroy(b2);
    vector_block_destroy(b3);
}

/* ========================================================================
 * 3. distinct 组数正确：emit 返回值 == 输出块 num_rows == 期望 distinct 数
 * ======================================================================== */
TEST(VecxHashAgg, DistinctGroupCount) {
    const std::vector<int64_t> keys = {10, 20, 30, 40};
    const std::vector<int64_t> ms = {1, 2, 3, 4};
    VectorBlock *b = make_i64_i64_block(keys, ms);

    vecx_hashagg_t *h = vecx_hashagg_create(0, 1);
    ASSERT_NE(h, nullptr);
    EXPECT_EQ(vecx_hashagg_add_block(h, b), 0);

    VectorBlock *out = nullptr;
    int n = vecx_hashagg_emit(h, &out);
    EXPECT_EQ(n, 4);
    ASSERT_NE(out, nullptr);
    EXPECT_EQ(out->num_rows, 4);

    vector_block_destroy(out);
    vecx_hashagg_destroy(h);
    vector_block_destroy(b);
}

/* ========================================================================
 * 4. 输出块 schema：6 列，类型标签 INT64/INT64/DOUBLE/DOUBLE/DOUBLE/DOUBLE
 * ======================================================================== */
TEST(VecxHashAgg, OutputBlockSchema) {
    const std::vector<int64_t> keys = {1};
    const std::vector<int64_t> ms = {10};
    VectorBlock *b = make_i64_i64_block(keys, ms);

    vecx_hashagg_t *h = vecx_hashagg_create(0, 1);
    ASSERT_NE(h, nullptr);
    EXPECT_EQ(vecx_hashagg_add_block(h, b), 0);

    VectorBlock *out = nullptr;
    int n = vecx_hashagg_emit(h, &out);
    ASSERT_EQ(n, 1);
    ASSERT_NE(out, nullptr);
    EXPECT_EQ(out->num_columns, 6);

    EXPECT_EQ(vector_block_get_column_type(out, 0), COLUMN_INT64);
    EXPECT_EQ(vector_block_get_column_type(out, 1), COLUMN_INT64);
    EXPECT_EQ(vector_block_get_column_type(out, 2), COLUMN_DOUBLE);
    EXPECT_EQ(vector_block_get_column_type(out, 3), COLUMN_DOUBLE);
    EXPECT_EQ(vector_block_get_column_type(out, 4), COLUMN_DOUBLE);
    EXPECT_EQ(vector_block_get_column_type(out, 5), COLUMN_DOUBLE);

    vector_block_destroy(out);
    vecx_hashagg_destroy(h);
    vector_block_destroy(b);
}

/* ========================================================================
 * 5. null 跳过：键 null 或度量 null → 整行跳过，该 key 不出现在输出
 * ======================================================================== */
TEST(VecxHashAgg, NullRowsSkipped) {
    // key=1: (1,10), (1,null), (1,20)  → null 行跳过，剩余 2 行
    // key=2: (null, 5)                 → 整行跳过，key=2 不出现
    // key=3: (3,null)                   → 整行跳过，key=3 不出现
    const std::vector<int64_t> keys = {1, 1, 1, 2, 3};
    const std::vector<int64_t> ms = {10, 0, 20, 5, 0};  // 0 作哨兵，实际 null 行用 set_null
    VectorBlock *b = make_i64_i64_block(keys, ms);
    ASSERT_NE(b, nullptr);

    // 第1行 (key=1, m=10): 正常
    // 第2行 (key=1, m=0):  null 度量
    // 第3行 (key=1, m=20): null 度量设成 false，这里用 ms 里的 0，实际 null 行是第2行
    // 重新建块
    vector_block_destroy(b);
    b = vector_block_create(5, 2);
    int64_t *kcol = (int64_t *)malloc(sizeof(int64_t) * 5);
    int64_t *mcol = (int64_t *)malloc(sizeof(int64_t) * 5);
    kcol[0] = 1; mcol[0] = 10;
    kcol[1] = 1; mcol[1] = 999;  // 会设 null
    kcol[2] = 1; mcol[2] = 999;  // 会设 null
    kcol[3] = 999; mcol[3] = 5;  // key null
    kcol[4] = 3; mcol[4] = 999;  // 会设 null
    vector_block_set_column(b, 0, kcol, (int)sizeof(int64_t));
    vector_block_set_column_type(b, 0, COLUMN_INT64);
    vector_block_set_column(b, 1, mcol, (int)sizeof(int64_t));
    vector_block_set_column_type(b, 1, COLUMN_INT64);
    vector_block_set_num_rows(b, 5);
    vector_block_set_null(b, 1, true);  // key=1 但度量 null → 整行跳过
    vector_block_set_null(b, 2, true);  // key=1 但度量 null → 整行跳过
    vector_block_set_null(b, 3, true);  // key=null → 整行跳过
    vector_block_set_null(b, 4, true);  // key=3 度量 null → 整行跳过

    vecx_hashagg_t *h = vecx_hashagg_create(0, 1);
    ASSERT_NE(h, nullptr);
    EXPECT_EQ(vecx_hashagg_add_block(h, b), 0);

    VectorBlock *out = nullptr;
    int n = vecx_hashagg_emit(h, &out);
    // 只剩 key=1 的 1 行 (10)，key=999 和 key=3 因 null 行跳出不出现
    EXPECT_EQ(n, 1);
    ASSERT_NE(out, nullptr);
    auto m = read_output_block(out);
    expect_group(m, 1, 1, 10.0, 10.0, 10.0, 10.0);
    EXPECT_EQ(m.find(999), m.end());  // key=999 整行 null → 不出现
    EXPECT_EQ(m.find(3), m.end());

    vector_block_destroy(out);
    vecx_hashagg_destroy(h);
    vector_block_destroy(b);
}

/* ========================================================================
 * 6. key=0 与负数 key：空槽哨兵不是 key==0
 * ======================================================================== */
TEST(VecxHashAgg, KeyZeroAndNegativeKeys) {
    // key=0: (0,10), (0,20)  → count=2, sum=30
    // key=-5: (-5,5)         → count=1, sum=5
    // key=-100: (-100,1)     → count=1, sum=1
    const std::vector<int64_t> keys = {0, 0, -5, -100};
    const std::vector<int64_t> ms = {10, 20, 5, 1};
    VectorBlock *b = make_i64_i64_block(keys, ms);

    vecx_hashagg_t *h = vecx_hashagg_create(0, 1);
    ASSERT_NE(h, nullptr);
    EXPECT_EQ(vecx_hashagg_add_block(h, b), 0);

    VectorBlock *out = nullptr;
    int n = vecx_hashagg_emit(h, &out);
    EXPECT_EQ(n, 3);
    ASSERT_NE(out, nullptr);

    auto m = read_output_block(out);
    expect_group(m, 0, 2, 30.0, 10.0, 20.0, 15.0);
    expect_group(m, -5, 1, 5.0, 5.0, 5.0, 5.0);
    expect_group(m, -100, 1, 1.0, 1.0, 1.0, 1.0);

    vector_block_destroy(out);
    vecx_hashagg_destroy(h);
    vector_block_destroy(b);
}

/* ========================================================================
 * 7. 扩容：~5000 distinct 键 × 2 行，钉死 rehash
 * ======================================================================== */
TEST(VecxHashAgg, ResizeUnderLoad) {
    const int NGROUPS = 5000;
    std::vector<int64_t> keys;
    std::vector<int64_t> ms;
    keys.reserve((size_t)NGROUPS * 2);
    ms.reserve((size_t)NGROUPS * 2);
    for (int i = 0; i < NGROUPS; i++) {
        keys.push_back((int64_t)i);
        ms.push_back((int64_t)i * 2);
        keys.push_back((int64_t)i);
        ms.push_back((int64_t)i * 2 + 1);
    }
    VectorBlock *b = make_i64_i64_block(keys, ms);
    ASSERT_NE(b, nullptr);

    vecx_hashagg_t *h = vecx_hashagg_create(0, 1);
    ASSERT_NE(h, nullptr);
    EXPECT_EQ(vecx_hashagg_add_block(h, b), 0);

    VectorBlock *out = nullptr;
    int n = vecx_hashagg_emit(h, &out);
    EXPECT_EQ(n, NGROUPS);
    ASSERT_NE(out, nullptr);
    EXPECT_EQ(out->num_rows, NGROUPS);

    auto m = read_output_block(out);
    // 随机抽查几个 key
    // key=0: v=0,1       → sum=1, min=0, max=1, avg=0.5
    // key=1234: v=2468,2469 → sum=4937, min=2468, max=2469, avg=4937/2=2468.5
    // key=4999: v=9998,9999 → sum=19997, min=9998, max=9999, avg=9998.5
    expect_group(m, 0, 2, 1.0, 0.0, 1.0, 0.5);
    expect_group(m, 1234, 2, 4937.0, 2468.0, 2469.0, 4937.0 / 2.0);
    expect_group(m, 4999, 2, 19997.0, 9998.0, 9999.0, 19997.0 / 2.0);

    vector_block_destroy(out);
    vecx_hashagg_destroy(h);
    vector_block_destroy(b);
}

/* ========================================================================
 * 8. 哈希冲突正确性：故意构造撞进同一槽位的键
 * ======================================================================== */

// 复刻 vecx_hashagg.c 内部的 splitmix64 finalizer，仅用于验证下面这组键
// 确实撞进同一个初始槽位。若实现换了哈希函数，本测试会立刻报出
// "不再覆盖冲突路径"，而不是悄悄退化成一个普通的四组聚合测试。
static uint64_t test_splitmix64(int64_t k) {
    uint64_t x = (uint64_t)k;
    x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27; x *= 0x94d049bb133111ebULL;
    x ^= x >> 31;
    return x;
}

TEST(VecxHashAgg, HashCollisionCorrectness) {
    // 69/143/165/205 在 splitmix64 下 % 64 都等于 0（64 = HASHAGG_INITIAL_CAP）。
    // 4 组 < 0.7*64=44.8 不触发扩容，所以这条长度 4 的线性探测链会被真实走一遍。
    const int64_t kColliding[4] = {69, 143, 165, 205};
    for (int64_t k : kColliding) {
        ASSERT_EQ(test_splitmix64(k) % 64u, 0u)
            << "key " << k << " 不再与其他键冲突，本测试已失去意义";
    }

    // 每组 2 行，两行的度量值分别是 v 和 v+1
    const std::vector<int64_t> keys = {69, 69, 143, 143, 165, 165, 205, 205};
    const std::vector<int64_t> ms = {0, 1, 2, 3, 4, 5, 6, 7};
    VectorBlock *b = make_i64_i64_block(keys, ms);

    vecx_hashagg_t *h = vecx_hashagg_create(0, 1);
    ASSERT_NE(h, nullptr);
    EXPECT_EQ(vecx_hashagg_add_block(h, b), 0);

    VectorBlock *out = nullptr;
    int n = vecx_hashagg_emit(h, &out);
    EXPECT_EQ(n, 4);
    ASSERT_NE(out, nullptr);

    auto m = read_output_block(out);
    expect_group(m, 69,  2, 1.0,  0.0, 1.0, 0.5);
    expect_group(m, 143, 2, 5.0,  2.0, 3.0, 2.5);
    expect_group(m, 165, 2, 9.0,  4.0, 5.0, 4.5);
    expect_group(m, 205, 2, 13.0, 6.0, 7.0, 6.5);

    vector_block_destroy(out);
    vecx_hashagg_destroy(h);
    vector_block_destroy(b);
}

/* ========================================================================
 * 9. int32 键 + float 度量：类型提升路径正确
 * ======================================================================== */
TEST(VecxHashAgg, Int32KeyFloatMeasure) {
    // key=1 (int32): v=1.5f, 2.5f  → count=2 sum=4.0 min=1.5 max=2.5 avg=2.0
    // key=2 (int32): v=3.5f        → count=1 sum=3.5 min=3.5 max=3.5 avg=3.5
    // 用 .5 结尾的值：float/double 都能精确表示，但若实现把度量截断成整型，
    // sum 会变成 3.0/3.0 而不是 4.0/3.5，测试立刻炸。
    const std::vector<int64_t> keys = {1, 1, 2};
    const std::vector<float> ms = {1.5f, 2.5f, 3.5f};
    VectorBlock *b = make_i32_f(keys, ms);
    ASSERT_NE(b, nullptr);

    vecx_hashagg_t *h = vecx_hashagg_create(0, 1);
    ASSERT_NE(h, nullptr);
    EXPECT_EQ(vecx_hashagg_add_block(h, b), 0);

    VectorBlock *out = nullptr;
    int n = vecx_hashagg_emit(h, &out);
    EXPECT_EQ(n, 2);
    ASSERT_NE(out, nullptr);

    auto m = read_output_block(out);
    expect_group(m, 1, 2, 4.0, 1.5, 2.5, 2.0);
    expect_group(m, 2, 1, 3.5, 3.5, 3.5, 3.5);

    vector_block_destroy(out);
    vecx_hashagg_destroy(h);
    vector_block_destroy(b);
}

/* ========================================================================
 * 10. 空输入：不 add 任何块直接 emit → 返回 0 且 *out=NULL
 * ======================================================================== */
TEST(VecxHashAgg, EmptyInput) {
    vecx_hashagg_t *h = vecx_hashagg_create(0, 1);
    ASSERT_NE(h, nullptr);

    VectorBlock *out = nullptr;
    int n = vecx_hashagg_emit(h, &out);
    EXPECT_EQ(n, 0);
    EXPECT_EQ(out, nullptr);

    vecx_hashagg_destroy(h);
}

/* ========================================================================
 * 11. 全 null 块：所有行 null → 输出 0 组
 * ======================================================================== */
TEST(VecxHashAgg, AllNullBlock) {
    VectorBlock *b = vector_block_create(3, 2);
    int64_t *kcol = (int64_t *)malloc(sizeof(int64_t) * 3);
    int64_t *mcol = (int64_t *)malloc(sizeof(int64_t) * 3);
    kcol[0] = 1; mcol[0] = 10;
    kcol[1] = 2; mcol[1] = 20;
    kcol[2] = 3; mcol[2] = 30;
    vector_block_set_column(b, 0, kcol, (int)sizeof(int64_t));
    vector_block_set_column_type(b, 0, COLUMN_INT64);
    vector_block_set_column(b, 1, mcol, (int)sizeof(int64_t));
    vector_block_set_column_type(b, 1, COLUMN_INT64);
    vector_block_set_num_rows(b, 3);
    vector_block_set_null(b, 0, true);
    vector_block_set_null(b, 1, true);
    vector_block_set_null(b, 2, true);

    vecx_hashagg_t *h = vecx_hashagg_create(0, 1);
    ASSERT_NE(h, nullptr);
    EXPECT_EQ(vecx_hashagg_add_block(h, b), 0);

    VectorBlock *out = nullptr;
    int n = vecx_hashagg_emit(h, &out);
    EXPECT_EQ(n, 0);
    EXPECT_EQ(out, nullptr);

    vecx_hashagg_destroy(h);
    vector_block_destroy(b);
}

/* ========================================================================
 * 12. emit 幂等：两次 emit 结果等价
 * ======================================================================== */
TEST(VecxHashAgg, EmitIdempotent) {
    const std::vector<int64_t> keys = {1, 2, 1, 3};
    const std::vector<int64_t> ms = {10, 20, 30, 5};
    VectorBlock *b = make_i64_i64_block(keys, ms);

    vecx_hashagg_t *h = vecx_hashagg_create(0, 1);
    ASSERT_NE(h, nullptr);
    EXPECT_EQ(vecx_hashagg_add_block(h, b), 0);

    VectorBlock *out1 = nullptr;
    int n1 = vecx_hashagg_emit(h, &out1);
    ASSERT_EQ(n1, 3);
    ASSERT_NE(out1, nullptr);

    VectorBlock *out2 = nullptr;
    int n2 = vecx_hashagg_emit(h, &out2);
    ASSERT_EQ(n2, 3);
    ASSERT_NE(out2, nullptr);

    auto m1 = read_output_block(out1);
    auto m2 = read_output_block(out2);
    EXPECT_EQ(m1.size(), m2.size());
    for (const auto &kv : m1) {
        EXPECT_EQ(kv.second, m2.at(kv.first));
    }

    vector_block_destroy(out1);
    vector_block_destroy(out2);
    vecx_hashagg_destroy(h);
    vector_block_destroy(b);
}

/* ========================================================================
 * 13. 不支持的列类型：COLUMN_STRING → add_block 返回 -1，状态未破坏
 * ======================================================================== */
TEST(VecxHashAgg, UnsupportedMeasureType) {
    // 先正常建一个块
    const std::vector<int64_t> keys = {1, 2};
    const std::vector<int64_t> ms = {10, 20};
    VectorBlock *b = make_i64_i64_block(keys, ms);

    vecx_hashagg_t *h = vecx_hashagg_create(0, 1);
    ASSERT_NE(h, nullptr);
    EXPECT_EQ(vecx_hashagg_add_block(h, b), 0);

    // 换一个度量列为 STRING 类型的块
    VectorBlock *bstr = vector_block_create(2, 2);
    int64_t *ks = (int64_t *)malloc(sizeof(int64_t) * 2);
    ks[0] = 3; ks[1] = 4;
    const char **ss = (const char **)malloc(sizeof(const char *) * 2);
    ss[0] = strdup("a");
    ss[1] = strdup("b");
    vector_block_set_column(bstr, 0, ks, (int)sizeof(int64_t));
    vector_block_set_column_type(bstr, 0, COLUMN_INT64);
    vector_block_set_column(bstr, 1, (void *)ss, (int)sizeof(const char *));
    vector_block_set_column_type(bstr, 1, COLUMN_STRING);
    vector_block_set_num_rows(bstr, 2);

    EXPECT_EQ(vecx_hashagg_add_block(h, bstr), -1);

    // 状态未破坏，emit 仍返回之前的结果
    VectorBlock *out = nullptr;
    int n = vecx_hashagg_emit(h, &out);
    EXPECT_EQ(n, 2);
    ASSERT_NE(out, nullptr);

    auto m = read_output_block(out);
    expect_group(m, 1, 1, 10.0, 10.0, 10.0, 10.0);
    expect_group(m, 2, 1, 20.0, 20.0, 20.0, 20.0);

    vector_block_destroy(out);
    vecx_hashagg_destroy(h);
    vector_block_destroy(b);
    // bstr 的 STRING 列要手动释放 strdup 的串（vector_block_destroy 不知道单个串指针），
    // 但列缓冲 ss 由 vector_block_destroy 释放，不要手动 free(ss) 否则 double-free。
    free((void *)ss[0]); free((void *)ss[1]);
    vector_block_destroy(bstr);
}

/* ========================================================================
 * 14. 入参非法：h=NULL / b=NULL / out=NULL / destroy(NULL) 不崩
 * ======================================================================== */
TEST(VecxHashAgg, InvalidArgs) {
    const std::vector<int64_t> keys = {1};
    const std::vector<int64_t> ms = {10};
    VectorBlock *b = make_i64_i64_block(keys, ms);

    // destroy(NULL) 不崩
    vecx_hashagg_destroy(nullptr);

    // create 后传 NULL 块
    vecx_hashagg_t *h = vecx_hashagg_create(0, 1);
    ASSERT_NE(h, nullptr);
    EXPECT_EQ(vecx_hashagg_add_block(h, nullptr), -1);

    // emit 时 out=NULL（出参是 NULL 指针本身）→ 入参非法，返回 -1
    VectorBlock *out = nullptr;
    EXPECT_EQ(vecx_hashagg_emit(h, nullptr), -1);

    // h=NULL 时 add_block / emit 不崩（返回 -1）
    EXPECT_EQ(vecx_hashagg_add_block(nullptr, b), -1);
    EXPECT_EQ(vecx_hashagg_emit(nullptr, &out), -1);

    vecx_hashagg_destroy(h);
    vector_block_destroy(b);
}

/* ========================================================================
 * 15. 不修改输入块：add_block 后输入块内容不变
 * ======================================================================== */
TEST(VecxHashAgg, InputBlockUnmodified) {
    const std::vector<int64_t> keys = {1, 2, 3};
    const std::vector<int64_t> ms = {10, 20, 30};
    VectorBlock *b = make_i64_i64_block(keys, ms);
    ASSERT_NE(b, nullptr);

    const int rows_before = b->num_rows;
    const int cols_before = b->num_columns;
    const int cap_before = b->capacity;
    const int type0 = vector_block_get_column_type(b, 0);
    const int type1 = vector_block_get_column_type(b, 1);
    const int sz0 = b->column_sizes[0];
    const int sz1 = b->column_sizes[1];
    const int nwords = (cap_before + 63) / 64;
    std::vector<int64_t> kdata(keys.size()), mdata(ms.size());
    memcpy(kdata.data(), b->columns[0], sizeof(int64_t) * keys.size());
    memcpy(mdata.data(), b->columns[1], sizeof(int64_t) * ms.size());
    std::vector<uint64_t> null_before((size_t)nwords);
    if (b->null_bitmap) {
        memcpy(null_before.data(), b->null_bitmap, sizeof(uint64_t) * (size_t)nwords);
    }

    vecx_hashagg_t *h = vecx_hashagg_create(0, 1);
    ASSERT_NE(h, nullptr);
    EXPECT_EQ(vecx_hashagg_add_block(h, b), 0);

    EXPECT_EQ(b->num_rows, rows_before);
    EXPECT_EQ(b->num_columns, cols_before);
    EXPECT_EQ(b->capacity, cap_before);
    EXPECT_EQ(vector_block_get_column_type(b, 0), type0);
    EXPECT_EQ(vector_block_get_column_type(b, 1), type1);
    EXPECT_EQ(b->column_sizes[0], sz0);
    EXPECT_EQ(b->column_sizes[1], sz1);
    EXPECT_EQ(memcmp(kdata.data(), b->columns[0], sizeof(int64_t) * keys.size()), 0);
    EXPECT_EQ(memcmp(mdata.data(), b->columns[1], sizeof(int64_t) * ms.size()), 0);
    if (b->null_bitmap) {
        EXPECT_EQ(memcmp(null_before.data(), b->null_bitmap,
                         sizeof(uint64_t) * (size_t)nwords), 0);
    }

    vecx_hashagg_destroy(h);
    vector_block_destroy(b);
}
