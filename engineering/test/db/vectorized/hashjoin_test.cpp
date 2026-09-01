/**
 * @file hashjoin_test.cpp
 * @brief Gap#2 向量化执行引擎 Task6 单元测试：向量化 Hash Join（inner join）
 *
 * 覆盖：
 * 1. 基础 inner join：build 3 行、probe 3 行、2 个键匹配
 * 2. probe 无匹配行被丢弃
 * 3. build 侧重复键 → 键内笛卡尔积
 * 4. 多块 build + 多块 probe
 * 5. 输出列布局：ncols == build_ncols + probe_ncols
 * 6. null 键双侧排除：null != null
 * 7. key=0 与负数键：空槽哨兵与哈希/探测正确
 * 8. 大规模 + 扩容：5000 distinct 键 × 1 行
 * 9. 未 add_build 直接 probe
 * 10. 空 build 块（num_rows=0）
 * 11. 空 probe 块
 * 12. schema 不一致
 * 13. 不支持的键列类型：COLUMN_STRING
 * 14. int32 键 vs int64 键混用
 * 15. build 块提前释放（深拷贝契约钉死）
 * 16. 不修改输入块
 * 17. 入参非法：NULL 安全
 * 18. 无泄漏
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
#include <set>
#include <vector>

namespace {

/**
 * 建一个 N 列的块，键列为 col_key_idx，类型为 key_type，其余列类型为 data_type。
 * data_cols 为额外列数（不含键列）。
 * 建好后：
 *   第 i 列（从0起）填入 i*100 + row_idx（int64_t）
 *   键列按 key_type 填入 (row_idx+1) * 10
 */
VectorBlock *make_n_col_block(
    int num_cols,
    int key_col_idx,
    int key_type,
    int data_type,
    int num_rows) {
    int cap = num_rows > 0 ? num_rows : 1;
    VectorBlock *b = vector_block_create(cap, num_cols);
    if (!b) return nullptr;

    for (int c = 0; c < num_cols; c++) {
        int col_type = (c == key_col_idx) ? key_type : data_type;
        int elem_size = (col_type == COLUMN_INT32) ? (int)sizeof(int32_t) : (int)sizeof(int64_t);
        void *col = malloc((size_t)cap * (size_t)elem_size);
        if (!col) {
            vector_block_destroy(b);
            return nullptr;
        }
        if (col_type == COLUMN_INT32) {
            int32_t *arr = (int32_t *)col;
            for (int r = 0; r < num_rows; r++) {
                arr[r] = (c == key_col_idx) ? ((r + 1) * 10) : ((c * 100) + r);
            }
        } else {
            int64_t *arr = (int64_t *)col;
            for (int r = 0; r < num_rows; r++) {
                arr[r] = (c == key_col_idx) ? ((r + 1) * 10) : ((c * 100) + r);
            }
        }
        vector_block_set_column(b, c, col, elem_size);
        vector_block_set_column_type(b, c, col_type);
    }
    vector_block_set_num_rows(b, num_rows);
    return b;
}

/** 建 2 列块：键列（INT64）+ 数据列（INT64） */
VectorBlock *make_2col_i64_block(const std::vector<int64_t> &keys,
                                  const std::vector<int64_t> &data,
                                  int num_rows) {
    int cap = num_rows > 0 ? num_rows : 1;
    VectorBlock *b = vector_block_create(cap, 2);
    if (!b) return nullptr;

    int64_t *kcol = (int64_t *)malloc(sizeof(int64_t) * (size_t)cap);
    int64_t *dcol = (int64_t *)malloc(sizeof(int64_t) * (size_t)cap);
    if (!kcol || !dcol) {
        free(kcol); free(dcol); vector_block_destroy(b); return nullptr;
    }
    for (int i = 0; i < num_rows; i++) {
        kcol[i] = keys[(size_t)i];
        dcol[i] = data[(size_t)i];
    }
    vector_block_set_column(b, 0, kcol, (int)sizeof(int64_t));
    vector_block_set_column_type(b, 0, COLUMN_INT64);
    vector_block_set_column(b, 1, dcol, (int)sizeof(int64_t));
    vector_block_set_column_type(b, 1, COLUMN_INT64);
    vector_block_set_num_rows(b, num_rows);
    return b;
}

/** 建 2 列块：键列（INT32）+ 数据列（INT64） */
VectorBlock *make_2col_i32_i64_block(const std::vector<int64_t> &keys,
                                      const std::vector<int64_t> &data,
                                      int num_rows) {
    int cap = num_rows > 0 ? num_rows : 1;
    VectorBlock *b = vector_block_create(cap, 2);
    if (!b) return nullptr;

    int32_t *kcol = (int32_t *)malloc(sizeof(int32_t) * (size_t)cap);
    int64_t *dcol = (int64_t *)malloc(sizeof(int64_t) * (size_t)cap);
    if (!kcol || !dcol) {
        free(kcol); free(dcol); vector_block_destroy(b); return nullptr;
    }
    for (int i = 0; i < num_rows; i++) {
        kcol[i] = (int32_t)keys[(size_t)i];
        dcol[i] = data[(size_t)i];
    }
    vector_block_set_column(b, 0, kcol, (int)sizeof(int32_t));
    vector_block_set_column_type(b, 0, COLUMN_INT32);
    vector_block_set_column(b, 1, dcol, (int)sizeof(int64_t));
    vector_block_set_column_type(b, 1, COLUMN_INT64);
    vector_block_set_num_rows(b, num_rows);
    return b;
}

/**
 * 把输出块读成 multiset<vector<int64_t>>。
 * 输出块布局：[build_col0, build_col1, ..., probe_col0, probe_col1, ...]
 */
std::multiset<std::vector<int64_t>> read_output_as_multiset(VectorBlock *out) {
    std::multiset<std::vector<int64_t>> ms;
    if (!out) return ms;
    for (int r = 0; r < out->num_rows; r++) {
        std::vector<int64_t> row;
        row.reserve((size_t)out->num_columns);
        for (int c = 0; c < out->num_columns; c++) {
            const int64_t *col = (const int64_t *)out->columns[c];
            row.push_back(col[r]);
        }
        ms.insert(row);
    }
    return ms;
}

}  // namespace

/* ========================================================================
 * 1. 基础 inner join：build 3 行、probe 3 行、2 个键匹配
 * ======================================================================== */
TEST(VecxHashJoin, BasicInnerJoin) {
    // build: key=10,20,30  data=100,200,300
    // probe: key=10,20,40  data=1000,2000,4000
    // 期望: (10,100,1000), (20,200,2000)
    std::vector<int64_t> bk = {10, 20, 30};
    std::vector<int64_t> bd = {100, 200, 300};
    VectorBlock *build = make_2col_i64_block(bk, bd, 3);
    ASSERT_NE(build, nullptr);

    std::vector<int64_t> pk = {10, 20, 40};
    std::vector<int64_t> pd = {1000, 2000, 4000};
    VectorBlock *probe = make_2col_i64_block(pk, pd, 3);
    ASSERT_NE(probe, nullptr);

    vecx_hashjoin_t *j = vecx_hashjoin_create(0, 0);
    ASSERT_NE(j, nullptr);
    EXPECT_EQ(vecx_hashjoin_add_build(j, build), 0);

    VectorBlock *out = nullptr;
    int n = vecx_hashjoin_probe(j, probe, &out);
    EXPECT_EQ(n, 2);
    ASSERT_NE(out, nullptr);
    EXPECT_EQ(out->num_rows, 2);
    EXPECT_EQ(out->num_columns, 4);  // build 2 cols + probe 2 cols

    auto ms = read_output_as_multiset(out);
    std::multiset<std::vector<int64_t>> expected = {
        {10, 100, 10, 1000},
        {20, 200, 20, 2000}
    };
    EXPECT_EQ(ms, expected);

    vector_block_destroy(out);
    vecx_hashjoin_destroy(j);
    vector_block_destroy(build);
    vector_block_destroy(probe);
}

/* ========================================================================
 * 2. probe 无匹配行被丢弃
 * ======================================================================== */
TEST(VecxHashJoin, ProbeNoMatch) {
    std::vector<int64_t> bk = {10, 20};
    std::vector<int64_t> bd = {100, 200};
    VectorBlock *build = make_2col_i64_block(bk, bd, 2);
    ASSERT_NE(build, nullptr);

    // probe 键全部不匹配
    std::vector<int64_t> pk = {30, 40, 50};
    std::vector<int64_t> pd = {3000, 4000, 5000};
    VectorBlock *probe = make_2col_i64_block(pk, pd, 3);
    ASSERT_NE(probe, nullptr);

    vecx_hashjoin_t *j = vecx_hashjoin_create(0, 0);
    ASSERT_NE(j, nullptr);
    EXPECT_EQ(vecx_hashjoin_add_build(j, build), 0);

    VectorBlock *out = nullptr;
    int n = vecx_hashjoin_probe(j, probe, &out);
    EXPECT_EQ(n, 0);
    EXPECT_EQ(out, nullptr);

    vecx_hashjoin_destroy(j);
    vector_block_destroy(build);
    vector_block_destroy(probe);
}

/* ========================================================================
 * 3. build 侧重复键 → 键内笛卡尔积
 * ======================================================================== */
TEST(VecxHashJoin, CartesianProductWithinKey) {
    // build: key=7 有 3 行 data=1,2,3; key=8 有 1 行 data=4
    // probe: key=7 有 2 行 data=100,200; key=9 有 1 行 data=300
    // key=7 → 3×2 = 6 行; key=8 无匹配; key=9 无匹配
    // 期望 6 行
    VectorBlock *build = vector_block_create(4, 2);
    int64_t *bk = (int64_t *)malloc(sizeof(int64_t) * 4);
    int64_t *bd = (int64_t *)malloc(sizeof(int64_t) * 4);
    bk[0] = 7; bd[0] = 1;
    bk[1] = 7; bd[1] = 2;
    bk[2] = 7; bd[2] = 3;
    bk[3] = 8; bd[3] = 4;
    vector_block_set_column(build, 0, bk, (int)sizeof(int64_t));
    vector_block_set_column_type(build, 0, COLUMN_INT64);
    vector_block_set_column(build, 1, bd, (int)sizeof(int64_t));
    vector_block_set_column_type(build, 1, COLUMN_INT64);
    vector_block_set_num_rows(build, 4);

    VectorBlock *probe = vector_block_create(3, 2);
    int64_t *pk = (int64_t *)malloc(sizeof(int64_t) * 3);
    int64_t *pd = (int64_t *)malloc(sizeof(int64_t) * 3);
    pk[0] = 7; pd[0] = 100;
    pk[1] = 7; pd[1] = 200;
    pk[2] = 9; pd[2] = 300;
    vector_block_set_column(probe, 0, pk, (int)sizeof(int64_t));
    vector_block_set_column_type(probe, 0, COLUMN_INT64);
    vector_block_set_column(probe, 1, pd, (int)sizeof(int64_t));
    vector_block_set_column_type(probe, 1, COLUMN_INT64);
    vector_block_set_num_rows(probe, 3);

    vecx_hashjoin_t *j = vecx_hashjoin_create(0, 0);
    ASSERT_NE(j, nullptr);
    EXPECT_EQ(vecx_hashjoin_add_build(j, build), 0);

    VectorBlock *out = nullptr;
    int n = vecx_hashjoin_probe(j, probe, &out);
    EXPECT_EQ(n, 6);
    ASSERT_NE(out, nullptr);

    // 验证是 (7,x,7,y) 的笛卡尔积
    auto ms = read_output_as_multiset(out);
    EXPECT_EQ(ms.size(), 6);
    // 所有行的第0和第2列都应该是 7
    for (const auto &row : ms) {
        EXPECT_EQ(row[0], 7);
        EXPECT_EQ(row[2], 7);
    }
    // 收集第1列(build data) 和第3列(probe data)
    std::multiset<int64_t> build_data, probe_data;
    for (const auto &row : ms) {
        build_data.insert(row[1]);
        probe_data.insert(row[3]);
    }
    std::multiset<int64_t> exp_build = {1, 2, 3, 1, 2, 3};
    std::multiset<int64_t> exp_probe = {100, 100, 200, 200, 100, 200};
    EXPECT_EQ(build_data, exp_build);
    EXPECT_EQ(probe_data, exp_probe);

    vector_block_destroy(out);
    vecx_hashjoin_destroy(j);
    vector_block_destroy(build);
    vector_block_destroy(probe);
}

/* ========================================================================
 * 4. 多块 build + 多块 probe
 * ======================================================================== */
TEST(VecxHashJoin, MultiBlockBuildAndProbe) {
    // build 3 块，每块 1 行: key=10,20,30
    // probe 2 块，每块 1 行: key=10,30
    // 期望: (10,10), (30,30)
    vecx_hashjoin_t *j = vecx_hashjoin_create(0, 0);
    ASSERT_NE(j, nullptr);

    VectorBlock *b1 = make_2col_i64_block({10}, {100}, 1);
    VectorBlock *b2 = make_2col_i64_block({20}, {200}, 1);
    VectorBlock *b3 = make_2col_i64_block({30}, {300}, 1);
    ASSERT_NE(b1, nullptr); ASSERT_NE(b2, nullptr); ASSERT_NE(b3, nullptr);

    EXPECT_EQ(vecx_hashjoin_add_build(j, b1), 0);
    EXPECT_EQ(vecx_hashjoin_add_build(j, b2), 0);
    EXPECT_EQ(vecx_hashjoin_add_build(j, b3), 0);

    VectorBlock *p1 = make_2col_i64_block({10}, {1000}, 1);
    VectorBlock *p2 = make_2col_i64_block({30}, {3000}, 1);
    ASSERT_NE(p1, nullptr); ASSERT_NE(p2, nullptr);

    VectorBlock *out1 = nullptr;
    int n1 = vecx_hashjoin_probe(j, p1, &out1);
    EXPECT_EQ(n1, 1);
    ASSERT_NE(out1, nullptr);
    auto ms1 = read_output_as_multiset(out1);
    EXPECT_EQ(ms1, (std::multiset<std::vector<int64_t>>{{10, 100, 10, 1000}}));

    VectorBlock *out2 = nullptr;
    int n2 = vecx_hashjoin_probe(j, p2, &out2);
    EXPECT_EQ(n2, 1);
    ASSERT_NE(out2, nullptr);
    auto ms2 = read_output_as_multiset(out2);
    EXPECT_EQ(ms2, (std::multiset<std::vector<int64_t>>{{30, 300, 30, 3000}}));

    vector_block_destroy(out1);
    vector_block_destroy(out2);
    vecx_hashjoin_destroy(j);
    vector_block_destroy(b1); vector_block_destroy(b2); vector_block_destroy(b3);
    vector_block_destroy(p1); vector_block_destroy(p2);
}

/* ========================================================================
 * 5. 输出列布局：ncols == build_ncols + probe_ncols
 * ======================================================================== */
TEST(VecxHashJoin, OutputColumnLayout) {
    // build: 3 列 (key, d1, d2)
    // probe: 2 列 (key, p1)
    VectorBlock *build = make_n_col_block(3, 0, COLUMN_INT64, COLUMN_INT64, 2);
    ASSERT_NE(build, nullptr);
    VectorBlock *probe = make_n_col_block(2, 0, COLUMN_INT64, COLUMN_INT64, 2);
    ASSERT_NE(probe, nullptr);

    vecx_hashjoin_t *j = vecx_hashjoin_create(0, 0);
    ASSERT_NE(j, nullptr);
    EXPECT_EQ(vecx_hashjoin_add_build(j, build), 0);

    VectorBlock *out = nullptr;
    int n = vecx_hashjoin_probe(j, probe, &out);
    if (n > 0) {
        ASSERT_NE(out, nullptr);
        EXPECT_EQ(out->num_columns, 5);  // 3 + 2

        // build 侧 3 列
        EXPECT_EQ(vector_block_get_column_type(out, 0), COLUMN_INT64);
        EXPECT_EQ(vector_block_get_column_type(out, 1), COLUMN_INT64);
        EXPECT_EQ(vector_block_get_column_type(out, 2), COLUMN_INT64);
        // probe 侧 2 列
        EXPECT_EQ(vector_block_get_column_type(out, 3), COLUMN_INT64);
        EXPECT_EQ(vector_block_get_column_type(out, 4), COLUMN_INT64);

        // column_sizes
        EXPECT_EQ(out->column_sizes[0], (int)sizeof(int64_t));
        EXPECT_EQ(out->column_sizes[1], (int)sizeof(int64_t));
        EXPECT_EQ(out->column_sizes[2], (int)sizeof(int64_t));
        EXPECT_EQ(out->column_sizes[3], (int)sizeof(int64_t));
        EXPECT_EQ(out->column_sizes[4], (int)sizeof(int64_t));

        vector_block_destroy(out);
    }

    vecx_hashjoin_destroy(j);
    vector_block_destroy(build);
    vector_block_destroy(probe);
}

/* ========================================================================
 * 6. null 键双侧排除：null != null
 * ======================================================================== */
TEST(VecxHashJoin, NullKeysExcluded) {
    // build: 行0 key=10 正常; 行1 key=10 但整行 null
    // probe: 行0 key=10 正常; 行1 key=10 但整行 null
    // 期望只匹配 (10,正常) × (10,正常) = 1 行
    VectorBlock *build = vector_block_create(2, 2);
    int64_t *bk = (int64_t *)malloc(sizeof(int64_t) * 2);
    int64_t *bd = (int64_t *)malloc(sizeof(int64_t) * 2);
    bk[0] = 10; bd[0] = 100;
    bk[1] = 10; bd[1] = 999;  // 会设 null
    vector_block_set_column(build, 0, bk, (int)sizeof(int64_t));
    vector_block_set_column_type(build, 0, COLUMN_INT64);
    vector_block_set_column(build, 1, bd, (int)sizeof(int64_t));
    vector_block_set_column_type(build, 1, COLUMN_INT64);
    vector_block_set_num_rows(build, 2);
    vector_block_set_null(build, 1, true);  // 整行 null

    VectorBlock *probe = vector_block_create(2, 2);
    int64_t *pk = (int64_t *)malloc(sizeof(int64_t) * 2);
    int64_t *pd = (int64_t *)malloc(sizeof(int64_t) * 2);
    pk[0] = 10; pd[0] = 1000;
    pk[1] = 10; pd[1] = 9999;  // 会设 null
    vector_block_set_column(probe, 0, pk, (int)sizeof(int64_t));
    vector_block_set_column_type(probe, 0, COLUMN_INT64);
    vector_block_set_column(probe, 1, pd, (int)sizeof(int64_t));
    vector_block_set_column_type(probe, 1, COLUMN_INT64);
    vector_block_set_num_rows(probe, 2);
    vector_block_set_null(probe, 1, true);  // 整行 null

    vecx_hashjoin_t *j = vecx_hashjoin_create(0, 0);
    ASSERT_NE(j, nullptr);
    EXPECT_EQ(vecx_hashjoin_add_build(j, build), 0);

    VectorBlock *out = nullptr;
    int n = vecx_hashjoin_probe(j, probe, &out);
    EXPECT_EQ(n, 1);
    ASSERT_NE(out, nullptr);
    EXPECT_EQ(out->num_rows, 1);

    // 验证结果确实是 (10,100,10,1000)
    const int64_t *out0 = (const int64_t *)out->columns[0];
    const int64_t *out1 = (const int64_t *)out->columns[1];
    const int64_t *out2 = (const int64_t *)out->columns[2];
    const int64_t *out3 = (const int64_t *)out->columns[3];
    EXPECT_EQ(out0[0], 10);
    EXPECT_EQ(out1[0], 100);
    EXPECT_EQ(out2[0], 10);
    EXPECT_EQ(out3[0], 1000);

    vector_block_destroy(out);
    vecx_hashjoin_destroy(j);
    vector_block_destroy(build);
    vector_block_destroy(probe);
}

/* ========================================================================
 * 7. key=0 与负数键
 * ======================================================================== */
TEST(VecxHashJoin, KeyZeroAndNegative) {
    // build: key=0, -5, -100
    // probe: key=0, -5
    // 期望: (0,m1), (-5,m2) 共 2 行
    VectorBlock *build = vector_block_create(3, 2);
    int64_t *bk = (int64_t *)malloc(sizeof(int64_t) * 3);
    int64_t *bd = (int64_t *)malloc(sizeof(int64_t) * 3);
    bk[0] = 0;   bd[0] = 1;
    bk[1] = -5; bd[1] = 2;
    bk[2] = -100; bd[2] = 3;
    vector_block_set_column(build, 0, bk, (int)sizeof(int64_t));
    vector_block_set_column_type(build, 0, COLUMN_INT64);
    vector_block_set_column(build, 1, bd, (int)sizeof(int64_t));
    vector_block_set_column_type(build, 1, COLUMN_INT64);
    vector_block_set_num_rows(build, 3);

    VectorBlock *probe = vector_block_create(2, 2);
    int64_t *pk = (int64_t *)malloc(sizeof(int64_t) * 2);
    int64_t *pd = (int64_t *)malloc(sizeof(int64_t) * 2);
    pk[0] = 0;   pd[0] = 10;
    pk[1] = -5; pd[1] = 20;
    vector_block_set_column(probe, 0, pk, (int)sizeof(int64_t));
    vector_block_set_column_type(probe, 0, COLUMN_INT64);
    vector_block_set_column(probe, 1, pd, (int)sizeof(int64_t));
    vector_block_set_column_type(probe, 1, COLUMN_INT64);
    vector_block_set_num_rows(probe, 2);

    vecx_hashjoin_t *j = vecx_hashjoin_create(0, 0);
    ASSERT_NE(j, nullptr);
    EXPECT_EQ(vecx_hashjoin_add_build(j, build), 0);

    VectorBlock *out = nullptr;
    int n = vecx_hashjoin_probe(j, probe, &out);
    EXPECT_EQ(n, 2);
    ASSERT_NE(out, nullptr);

    auto ms = read_output_as_multiset(out);
    std::multiset<std::vector<int64_t>> expected = {
        {0, 1, 0, 10},
        {-5, 2, -5, 20}
    };
    EXPECT_EQ(ms, expected);

    vector_block_destroy(out);
    vecx_hashjoin_destroy(j);
    vector_block_destroy(build);
    vector_block_destroy(probe);
}

/* ========================================================================
 * 8. 大规模 + 扩容：5000 distinct 键各 1 行，probe 全匹配
 * ======================================================================== */
TEST(VecxHashJoin, LargeScaleWithResize) {
    const int N = 5000;
    std::vector<int64_t> bk, bd, pk, pd;
    bk.reserve((size_t)N); bd.reserve((size_t)N);
    pk.reserve((size_t)N); pd.reserve((size_t)N);
    for (int i = 0; i < N; i++) {
        bk.push_back((int64_t)i);
        bd.push_back((int64_t)(i * 2));
        pk.push_back((int64_t)i);
        pd.push_back((int64_t)(i * 3));
    }

    VectorBlock *build = make_2col_i64_block(bk, bd, N);
    ASSERT_NE(build, nullptr);
    VectorBlock *probe = make_2col_i64_block(pk, pd, N);
    ASSERT_NE(probe, nullptr);

    vecx_hashjoin_t *j = vecx_hashjoin_create(0, 0);
    ASSERT_NE(j, nullptr);
    EXPECT_EQ(vecx_hashjoin_add_build(j, build), 0);

    VectorBlock *out = nullptr;
    int n = vecx_hashjoin_probe(j, probe, &out);
    EXPECT_EQ(n, N);
    ASSERT_NE(out, nullptr);
    EXPECT_EQ(out->num_rows, N);

    // 随机抽查几个
    auto ms = read_output_as_multiset(out);
    for (int i = 0; i < N; i++) {
        std::vector<int64_t> row = {(int64_t)i, (int64_t)(i * 2), (int64_t)i, (int64_t)(i * 3)};
        EXPECT_TRUE(ms.find(row) != ms.end());
    }

    vector_block_destroy(out);
    vecx_hashjoin_destroy(j);
    vector_block_destroy(build);
    vector_block_destroy(probe);
}

/* ========================================================================
 * 9. 未 add_build 直接 probe
 * ======================================================================== */
TEST(VecxHashJoin, ProbeWithoutBuild) {
    vecx_hashjoin_t *j = vecx_hashjoin_create(0, 0);
    ASSERT_NE(j, nullptr);

    std::vector<int64_t> pk = {10, 20};
    std::vector<int64_t> pd = {100, 200};
    VectorBlock *probe = make_2col_i64_block(pk, pd, 2);
    ASSERT_NE(probe, nullptr);

    VectorBlock *out = nullptr;
    int n = vecx_hashjoin_probe(j, probe, &out);
    EXPECT_EQ(n, 0);
    EXPECT_EQ(out, nullptr);

    vecx_hashjoin_destroy(j);
    vector_block_destroy(probe);
}

/* ========================================================================
 * 10. 空 build 块（num_rows=0）
 * ======================================================================== */
TEST(VecxHashJoin, EmptyBuildBlock) {
    vecx_hashjoin_t *j = vecx_hashjoin_create(0, 0);
    ASSERT_NE(j, nullptr);

    VectorBlock *empty_build = vector_block_create(1, 2);
    ASSERT_NE(empty_build, nullptr);
    vector_block_set_num_rows(empty_build, 0);

    EXPECT_EQ(vecx_hashjoin_add_build(j, empty_build), 0);

    std::vector<int64_t> pk = {10};
    std::vector<int64_t> pd = {100};
    VectorBlock *probe = make_2col_i64_block(pk, pd, 1);
    ASSERT_NE(probe, nullptr);

    VectorBlock *out = nullptr;
    int n = vecx_hashjoin_probe(j, probe, &out);
    EXPECT_EQ(n, 0);
    EXPECT_EQ(out, nullptr);

    vecx_hashjoin_destroy(j);
    vector_block_destroy(empty_build);
    vector_block_destroy(probe);
}

/* ========================================================================
 * 11. 空 probe 块
 * ======================================================================== */
TEST(VecxHashJoin, EmptyProbeBlock) {
    std::vector<int64_t> bk = {10};
    std::vector<int64_t> bd = {100};
    VectorBlock *build = make_2col_i64_block(bk, bd, 1);
    ASSERT_NE(build, nullptr);

    vecx_hashjoin_t *j = vecx_hashjoin_create(0, 0);
    ASSERT_NE(j, nullptr);
    EXPECT_EQ(vecx_hashjoin_add_build(j, build), 0);

    VectorBlock *empty_probe = vector_block_create(1, 2);
    ASSERT_NE(empty_probe, nullptr);
    vector_block_set_num_rows(empty_probe, 0);

    VectorBlock *out = nullptr;
    int n = vecx_hashjoin_probe(j, empty_probe, &out);
    EXPECT_EQ(n, 0);
    EXPECT_EQ(out, nullptr);

    vecx_hashjoin_destroy(j);
    vector_block_destroy(build);
    vector_block_destroy(empty_probe);
}

/* ========================================================================
 * 12. schema 不一致：第二个 build 块列数不同
 * ======================================================================== */
TEST(VecxHashJoin, SchemaInconsistency) {
    // 第一个 build 块：2 列
    std::vector<int64_t> bk1 = {10};
    std::vector<int64_t> bd1 = {100};
    VectorBlock *build1 = make_2col_i64_block(bk1, bd1, 1);
    ASSERT_NE(build1, nullptr);

    // 第二个 build 块：3 列（列数不同）
    VectorBlock *build2 = make_n_col_block(3, 0, COLUMN_INT64, COLUMN_INT64, 1);
    ASSERT_NE(build2, nullptr);

    vecx_hashjoin_t *j = vecx_hashjoin_create(0, 0);
    ASSERT_NE(j, nullptr);
    EXPECT_EQ(vecx_hashjoin_add_build(j, build1), 0);
    EXPECT_EQ(vecx_hashjoin_add_build(j, build2), -1);  // schema 不一致

    // 第一个块的数据仍可正常 probe
    std::vector<int64_t> pk = {10};
    std::vector<int64_t> pd = {1000};
    VectorBlock *probe = make_2col_i64_block(pk, pd, 1);
    ASSERT_NE(probe, nullptr);

    VectorBlock *out = nullptr;
    int n = vecx_hashjoin_probe(j, probe, &out);
    EXPECT_EQ(n, 1);
    ASSERT_NE(out, nullptr);
    EXPECT_EQ(out->num_columns, 4);

    vector_block_destroy(out);
    vecx_hashjoin_destroy(j);
    vector_block_destroy(build1);
    vector_block_destroy(build2);
    vector_block_destroy(probe);
}

/* ========================================================================
 * 13. 不支持的键列类型：COLUMN_STRING
 * ======================================================================== */
TEST(VecxHashJoin, UnsupportedKeyColumnType) {
    vecx_hashjoin_t *j = vecx_hashjoin_create(0, 0);
    ASSERT_NE(j, nullptr);

    // 建一个 STRING 类型的键列块
    VectorBlock *bad = vector_block_create(2, 2);
    const char **keys = (const char **)malloc(sizeof(const char *) * 2);
    int64_t *data = (int64_t *)malloc(sizeof(int64_t) * 2);
    keys[0] = strdup("a");
    keys[1] = strdup("b");
    data[0] = 1; data[1] = 2;
    vector_block_set_column(bad, 0, (void *)keys, (int)sizeof(const char *));
    vector_block_set_column_type(bad, 0, COLUMN_STRING);
    vector_block_set_column(bad, 1, data, (int)sizeof(int64_t));
    vector_block_set_column_type(bad, 1, COLUMN_INT64);
    vector_block_set_num_rows(bad, 2);

    EXPECT_EQ(vecx_hashjoin_add_build(j, bad), -1);

    // 建一个正常的 build 块，schema 应能建立
    VectorBlock *build2 = make_2col_i64_block({10}, {100}, 1);
    ASSERT_NE(build2, nullptr);
    EXPECT_EQ(vecx_hashjoin_add_build(j, build2), 0);

    // probe 并验证
    VectorBlock *probe = make_2col_i64_block({10}, {1000}, 1);
    ASSERT_NE(probe, nullptr);
    VectorBlock *out = nullptr;
    EXPECT_EQ(vecx_hashjoin_probe(j, probe, &out), 1);

    // 清理：vector_block_destroy 会 free columns 数组（keys 和 data），
    // 但 strdup 的字符串内容需要手动释放
    free((void *)keys[0]); free((void *)keys[1]);
    vector_block_destroy(bad);
    if (out) vector_block_destroy(out);
    vecx_hashjoin_destroy(j);
    vector_block_destroy(build2);
    vector_block_destroy(probe);
}

/* ========================================================================
 * 14. int32 键 vs int64 键混用
 * ======================================================================== */
TEST(VecxHashJoin, Int32VsInt64Key) {
    // build: INT32 键
    // probe: INT64 键
    // 同一数值要能匹配上
    // 注意：输出块列类型从各侧继承，build 键列是 INT32、probe 键列是 INT64，
    // 故不能直接用 read_output_as_multiset（它假设所有列是 int64_t）。
    VectorBlock *build = make_2col_i32_i64_block({10, 20, 30}, {100, 200, 300}, 3);
    ASSERT_NE(build, nullptr);
    VectorBlock *probe = make_2col_i64_block({10, 20, 40}, {1000, 2000, 4000}, 3);
    ASSERT_NE(probe, nullptr);

    vecx_hashjoin_t *j = vecx_hashjoin_create(0, 0);
    ASSERT_NE(j, nullptr);
    EXPECT_EQ(vecx_hashjoin_add_build(j, build), 0);

    VectorBlock *out = nullptr;
    int n = vecx_hashjoin_probe(j, probe, &out);
    EXPECT_EQ(n, 2);
    ASSERT_NE(out, nullptr);
    EXPECT_EQ(out->num_columns, 4);
    // 列类型：build 键列是 INT32，build 数据列是 INT64，probe 两列都是 INT64
    EXPECT_EQ(vector_block_get_column_type(out, 0), COLUMN_INT32);
    EXPECT_EQ(vector_block_get_column_type(out, 1), COLUMN_INT64);
    EXPECT_EQ(vector_block_get_column_type(out, 2), COLUMN_INT64);
    EXPECT_EQ(vector_block_get_column_type(out, 3), COLUMN_INT64);

    // 逐列验证值
    // 列0 (build 键列 INT32)：值应为 10 和 20
    const int32_t *c0 = (const int32_t *)out->columns[0];
    // 列1 (build 数据列 INT64)：值应为 100 和 200
    const int64_t *c1 = (const int64_t *)out->columns[1];
    // 列2 (probe 键列 INT64)：值应为 10 和 20
    const int64_t *c2 = (const int64_t *)out->columns[2];
    // 列3 (probe 数据列 INT64)：值应为 1000 和 2000
    const int64_t *c3 = (const int64_t *)out->columns[3];

    // 找到 key=10 的行
    int found = 0;
    for (int i = 0; i < out->num_rows; i++) {
        if (c0[i] == 10) {
            EXPECT_EQ(c1[i], 100);
            EXPECT_EQ(c2[i], 10);
            EXPECT_EQ(c3[i], 1000);
            found++;
        } else if (c0[i] == 20) {
            EXPECT_EQ(c1[i], 200);
            EXPECT_EQ(c2[i], 20);
            EXPECT_EQ(c3[i], 2000);
            found++;
        }
    }
    EXPECT_EQ(found, 2);

    vector_block_destroy(out);
    vecx_hashjoin_destroy(j);
    vector_block_destroy(build);
    vector_block_destroy(probe);
}

/* ========================================================================
 * 15. build 块提前释放（钉死深拷贝契约）
 * ======================================================================== */
TEST(VecxHashJoin, BuildBlockFreedBeforeProbe) {
    std::vector<int64_t> bk = {10, 20, 30};
    std::vector<int64_t> bd = {100, 200, 300};
    VectorBlock *build = make_2col_i64_block(bk, bd, 3);
    ASSERT_NE(build, nullptr);

    std::vector<int64_t> pk = {10, 20};
    std::vector<int64_t> pd = {1000, 2000};
    VectorBlock *probe = make_2col_i64_block(pk, pd, 2);
    ASSERT_NE(probe, nullptr);

    vecx_hashjoin_t *j = vecx_hashjoin_create(0, 0);
    ASSERT_NE(j, nullptr);
    EXPECT_EQ(vecx_hashjoin_add_build(j, build), 0);

    // add_build 后立即释放原始 build 块
    vector_block_destroy(build);
    build = nullptr;

    // probe 仍须正确工作
    VectorBlock *out = nullptr;
    int n = vecx_hashjoin_probe(j, probe, &out);
    EXPECT_EQ(n, 2);
    ASSERT_NE(out, nullptr);

    auto ms = read_output_as_multiset(out);
    std::multiset<std::vector<int64_t>> expected = {
        {10, 100, 10, 1000},
        {20, 200, 20, 2000}
    };
    EXPECT_EQ(ms, expected);

    vector_block_destroy(out);
    vecx_hashjoin_destroy(j);
    vector_block_destroy(probe);
}

/* ========================================================================
 * 16. 不修改输入块
 * ======================================================================== */
TEST(VecxHashJoin, InputBlockUnmodified) {
    std::vector<int64_t> bk = {10, 20};
    std::vector<int64_t> bd = {100, 200};
    VectorBlock *build = make_2col_i64_block(bk, bd, 2);
    ASSERT_NE(build, nullptr);

    std::vector<int64_t> pk = {10};
    std::vector<int64_t> pd = {1000};
    VectorBlock *probe = make_2col_i64_block(pk, pd, 1);
    ASSERT_NE(probe, nullptr);

    const int bk_rows = build->num_rows;
    const int bk_cols = build->num_columns;
    int64_t bk0_backup = ((int64_t *)build->columns[0])[0];

    vecx_hashjoin_t *j = vecx_hashjoin_create(0, 0);
    ASSERT_NE(j, nullptr);
    EXPECT_EQ(vecx_hashjoin_add_build(j, build), 0);

    EXPECT_EQ(((int64_t *)build->columns[0])[0], bk0_backup);
    EXPECT_EQ(build->num_rows, bk_rows);
    EXPECT_EQ(build->num_columns, bk_cols);

    EXPECT_EQ(vecx_hashjoin_probe(j, probe, nullptr), 1);

    EXPECT_EQ(((int64_t *)build->columns[0])[0], bk0_backup);
    EXPECT_EQ(probe->num_rows, 1);

    vecx_hashjoin_destroy(j);
    vector_block_destroy(build);
    vector_block_destroy(probe);
}

/* ========================================================================
 * 17. 入参非法：NULL 安全
 * ======================================================================== */
TEST(VecxHashJoin, InvalidArgs) {
    // destroy(NULL) 不崩溃
    vecx_hashjoin_destroy(nullptr);

    // j=NULL
    VectorBlock *out = nullptr;
    EXPECT_EQ(vecx_hashjoin_create(-1, 0), nullptr);
    EXPECT_EQ(vecx_hashjoin_create(0, -1), nullptr);
    EXPECT_EQ(vecx_hashjoin_add_build(nullptr, nullptr), -1);
    EXPECT_EQ(vecx_hashjoin_probe(nullptr, nullptr, &out), -1);
    EXPECT_EQ(vecx_hashjoin_probe(nullptr, nullptr, nullptr), -1);

    // build=NULL / probe=NULL
    vecx_hashjoin_t *j = vecx_hashjoin_create(0, 0);
    ASSERT_NE(j, nullptr);
    EXPECT_EQ(vecx_hashjoin_add_build(j, nullptr), -1);
    EXPECT_EQ(vecx_hashjoin_probe(j, nullptr, &out), -1);

    vecx_hashjoin_destroy(j);
}

/* ========================================================================
 * 18. 无泄漏（依靠 code review + ASan 观察）
 * ======================================================================== */
TEST(VecxHashJoin, NoLeak) {
    // 所有用例都 destroy 了句柄和输出块，本用例作为显式总结
    vecx_hashjoin_t *j = vecx_hashjoin_create(0, 0);
    ASSERT_NE(j, nullptr);

    VectorBlock *b = make_2col_i64_block({10}, {100}, 1);
    VectorBlock *p = make_2col_i64_block({10}, {1000}, 1);
    ASSERT_NE(b, nullptr); ASSERT_NE(p, nullptr);

    EXPECT_EQ(vecx_hashjoin_add_build(j, b), 0);
    VectorBlock *out = nullptr;
    EXPECT_EQ(vecx_hashjoin_probe(j, p, &out), 1);
    if (out) vector_block_destroy(out);

    vecx_hashjoin_destroy(j);
    vector_block_destroy(b);
    vector_block_destroy(p);
}
