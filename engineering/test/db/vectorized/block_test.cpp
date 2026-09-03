/**
 * @file block_test.cpp
 * @brief Gap#2 向量化执行引擎 Task1 单元测试
 *
 * 覆盖：
 * - VectorBlock 每列类型标签的设置/读取/越界/默认值（-1=未知）
 * - 位图基础操作：计数（含跨字与 nrows 截断）、位图转选择向量、and/or
 * - vecx_block_gather：按选择向量深拷贝压缩块（值、列类型、null 重映射）
 * - 空/边界入参
 */
#include <gtest/gtest.h>

extern "C" {
#include "db/core/vector_exec.h"
#include "db/core/columnar_store.h"
#include "db/vectorized/vectorized.h"
}

#include <cstdint>
#include <cstring>
#include <vector>

/* ========================================================================
 * 1. 类型标签往返
 * ======================================================================== */
TEST(VectorBlockType, SetGetRoundtrip) {
    VectorBlock *b = vector_block_create(16, 3);
    ASSERT_NE(b, nullptr);

    /* 未设置类型的列一律为 -1（未知），不能是 calloc 出的 0=COLUMN_INT8 */
    EXPECT_EQ(vector_block_get_column_type(b, 0), -1);
    EXPECT_EQ(vector_block_get_column_type(b, 1), -1);
    EXPECT_EQ(vector_block_get_column_type(b, 2), -1);

    vector_block_set_column_type(b, 1, COLUMN_INT64);
    EXPECT_EQ(vector_block_get_column_type(b, 1), COLUMN_INT64);
    /* 其余列不受影响 */
    EXPECT_EQ(vector_block_get_column_type(b, 0), -1);
    EXPECT_EQ(vector_block_get_column_type(b, 2), -1);

    /* 可覆盖 */
    vector_block_set_column_type(b, 1, COLUMN_DOUBLE);
    EXPECT_EQ(vector_block_get_column_type(b, 1), COLUMN_DOUBLE);

    /* 越界与非法入参返回 -1 */
    EXPECT_EQ(vector_block_get_column_type(b, -1), -1);
    EXPECT_EQ(vector_block_get_column_type(b, 3), -1);
    EXPECT_EQ(vector_block_get_column_type(nullptr, 0), -1);
    vector_block_set_column_type(b, -1, COLUMN_INT32);  /* 不崩溃即可 */
    vector_block_set_column_type(b, 3, COLUMN_INT32);
    vector_block_set_column_type(nullptr, 0, COLUMN_INT32);
    EXPECT_EQ(vector_block_get_column_type(b, 0), -1);

    vector_block_destroy(b);
}

/* ========================================================================
 * 2. 位图计数（跨字 + nrows 截断）
 * ======================================================================== */
TEST(BitmapOps, Count) {
    /* 5 个字覆盖行 0..319；置位 0、63、64、65、127、200、256 */
    uint64_t bm[5];
    memset(bm, 0, sizeof(bm));
    const int bits[] = {0, 63, 64, 65, 127, 200, 256};
    for (int bit : bits) {
        bm[bit / 64] |= (1ULL << (bit % 64));
    }

    /* nrows=257：行 0..256，全部 7 位计入 */
    EXPECT_EQ(vecx_bitmap_count(bm, 257), 7);
    /* nrows=200：行 200 与 256 被截断，只剩 5 位 */
    EXPECT_EQ(vecx_bitmap_count(bm, 200), 5);
    /* nrows=64：行 0、63 均在 [0,64) → 2 位 */
    EXPECT_EQ(vecx_bitmap_count(bm, 64), 2);
    /* nrows=63：仅行 0 → 1 位（行 63 被截断） */
    EXPECT_EQ(vecx_bitmap_count(bm, 63), 1);

    /* 空位图 */
    uint64_t empty[2] = {0, 0};
    EXPECT_EQ(vecx_bitmap_count(empty, 128), 0);
    /* NULL 安全 */
    EXPECT_EQ(vecx_bitmap_count(nullptr, 128), 0);
}

/* ========================================================================
 * 3. 位图转选择向量（升序、与置位集合一致）
 * ======================================================================== */
TEST(BitmapOps, ToSelection) {
    uint64_t bm[5];
    memset(bm, 0, sizeof(bm));
    const int bits[] = {0, 63, 64, 65, 127, 200, 256};
    for (int bit : bits) {
        bm[bit / 64] |= (1ULL << (bit % 64));
    }

    int sel[320];
    memset(sel, 0xCC, sizeof(sel));
    int n = vecx_bitmap_to_selection(bm, 257, sel);
    ASSERT_EQ(n, 7);
    for (int k = 0; k < n; k++) {
        EXPECT_EQ(sel[k], bits[k]);  /* 升序输出 */
    }
    /* 数量与计数一致 */
    EXPECT_EQ(n, vecx_bitmap_count(bm, 257));

    /* nrows 截断：行 200、256 被排除 */
    int sel2[320];
    int n2 = vecx_bitmap_to_selection(bm, 200, sel2);
    ASSERT_EQ(n2, 5);
    const int expect2[] = {0, 63, 64, 65, 127};
    for (int k = 0; k < n2; k++) {
        EXPECT_EQ(sel2[k], expect2[k]);
    }

    /* NULL 安全 */
    EXPECT_EQ(vecx_bitmap_to_selection(nullptr, 257, sel), 0);
    EXPECT_EQ(vecx_bitmap_to_selection(bm, 257, nullptr), 0);
}

/* ========================================================================
 * 4. 位图 and/or
 * ======================================================================== */
TEST(BitmapOps, AndOr) {
    const uint64_t a[3] = {0xFF00FF00FF00FF00ULL, 0xAAAAAAAAAAAAAAAAULL, 0x123456789ABCDEF0ULL};
    const uint64_t b[3] = {0x0F0F0F0F0F0F0F0FULL, 0x5555555555555555ULL, 0xFEDCBA9876543210ULL};
    uint64_t out[3];

    memset(out, 0, sizeof(out));
    vecx_bitmap_and(a, b, 3, out);
    for (int i = 0; i < 3; i++) {
        EXPECT_EQ(out[i], a[i] & b[i]);
    }

    memset(out, 0, sizeof(out));
    vecx_bitmap_or(a, b, 3, out);
    for (int i = 0; i < 3; i++) {
        EXPECT_EQ(out[i], a[i] | b[i]);
    }

    /* nwords=0 与 NULL 入参不崩溃 */
    vecx_bitmap_and(a, b, 0, out);
    vecx_bitmap_or(nullptr, b, 3, out);
    vecx_bitmap_and(a, nullptr, 3, out);
    vecx_bitmap_and(a, b, 3, nullptr);
}

/* ========================================================================
 * 5. gather 深拷贝压缩
 * ======================================================================== */
TEST(BlockGather, DeepCopyCompress) {
    const int CAP = 8;
    const int NROWS = 6;
    VectorBlock *src = vector_block_create(CAP, 2);
    ASSERT_NE(src, nullptr);

    int64_t *col0 = (int64_t *)malloc(sizeof(int64_t) * CAP);
    double  *col1 = (double  *)malloc(sizeof(double)  * CAP);
    for (int i = 0; i < NROWS; i++) {
        col0[i] = 100 + i;
        col1[i] = 1.5 * (i + 1);
    }
    vector_block_set_column(src, 0, col0, sizeof(int64_t));
    vector_block_set_column(src, 1, col1, sizeof(double));
    vector_block_set_column_type(src, 0, COLUMN_INT64);
    vector_block_set_column_type(src, 1, COLUMN_DOUBLE);
    /* 第 1、3、4、5 行设为 null（其中选中行 4、5 为 null，覆盖两个选中空值） */
    vector_block_set_null(src, 1, true);
    vector_block_set_null(src, 3, true);
    vector_block_set_null(src, 4, true);
    vector_block_set_null(src, 5, true);
    vector_block_set_num_rows(src, NROWS);

    const int sel[] = {0, 2, 4, 5};
    const int nsel = 4;
    VectorBlock *dst = vecx_block_gather(src, sel, nsel);
    ASSERT_NE(dst, nullptr);

    /* 形状 */
    EXPECT_EQ(dst->num_rows, nsel);
    EXPECT_EQ(dst->num_columns, 2);

    /* 列类型被复制 */
    EXPECT_EQ(vector_block_get_column_type(dst, 0), COLUMN_INT64);
    EXPECT_EQ(vector_block_get_column_type(dst, 1), COLUMN_DOUBLE);

    /* 逐行值等于源块 sel[j] 行 */
    int64_t *d0 = (int64_t *)vector_block_get_column(dst, 0);
    double  *d1 = (double  *)vector_block_get_column(dst, 1);
    ASSERT_NE(d0, nullptr);
    ASSERT_NE(d1, nullptr);
    for (int j = 0; j < nsel; j++) {
        EXPECT_EQ(d0[j], col0[sel[j]]);
        EXPECT_DOUBLE_EQ(d1[j], col1[sel[j]]);
    }

    /* null 重映射：源 0/2 非 null，4/5 为 null → 新块行 0、1 非 null，行 2、3 null */
    EXPECT_FALSE(vector_block_is_null(dst, 0));
    EXPECT_FALSE(vector_block_is_null(dst, 1));
    EXPECT_TRUE(vector_block_is_null(dst, 2));
    EXPECT_TRUE(vector_block_is_null(dst, 3));
    /* 与源块逐行一致 */
    for (int j = 0; j < nsel; j++) {
        EXPECT_EQ(vector_block_is_null(dst, j), vector_block_is_null(src, sel[j]));
    }

    /* 深拷贝：改 dst 不影响 src */
    d0[0] = -999;
    EXPECT_EQ(col0[0], 100);

    /* 两个块都能正常销毁（无二次释放） */
    vector_block_destroy(dst);
    vector_block_destroy(src);
}

/* ========================================================================
 * 6. 空/边界入参
 * ======================================================================== */
TEST(BlockGather, NullAndEmpty) {
    int sel[] = {0, 1};
    VectorBlock *src = vector_block_create(4, 1);
    ASSERT_NE(src, nullptr);

    EXPECT_EQ(vecx_block_gather(nullptr, sel, 2), nullptr);
    EXPECT_EQ(vecx_block_gather(src, nullptr, 2), nullptr);
    EXPECT_EQ(vecx_block_gather(src, sel, 0), nullptr);
    EXPECT_EQ(vecx_block_gather(src, sel, -1), nullptr);

    vector_block_destroy(src);
}
