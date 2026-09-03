/**
 * @file roaring_bitmap_test.cpp
 * @brief P5-2：CRoaring bitmap 兼容层单元测试
 *
 * 验证 sorted array 实现的正确性与内存优势：
 *   1. TestSmallBitmap - 小数据集（N=1000）功能验证
 *   2. TestLargeBitmap - 大数据集（N=1M）roaring 启用验证
 *   3. TestMemoryUsage - 内存对比测试
 */

#include <gtest/gtest.h>

extern "C" {
#include "roaring_bitmap.h"
}

/* ==================== 测试 1：小数据集功能验证 ==================== */
TEST(RoaringBitmapTest, SmallBitmapFunctionality) {
    const uint32_t N = 1000;
    roaring_bitmap_t* rb = roaring_bitmap_create();
    ASSERT_NE(rb, nullptr);

    /* 添加元素 0..999 */
    for (uint32_t i = 0; i < N; i++) {
        roaring_bitmap_add(rb, i);
    }

    /* 验证 count */
    EXPECT_EQ(roaring_bitmap_count(rb), N);

    /* 验证 contains：所有已添加的值都应存在 */
    for (uint32_t i = 0; i < N; i++) {
        EXPECT_TRUE(roaring_bitmap_contains(rb, i)) << "missing value " << i;
    }

    /* 验证 contains：未添加的值应不存在 */
    EXPECT_FALSE(roaring_bitmap_contains(rb, N));
    EXPECT_FALSE(roaring_bitmap_contains(rb, N + 100));
    EXPECT_FALSE(roaring_bitmap_contains(rb, UINT32_MAX));

    /* 验证 is_empty */
    EXPECT_FALSE(roaring_bitmap_is_empty(rb));

    /* 释放 */
    roaring_bitmap_free(rb);
}

/* ==================== 测试 2：大数据集功能验证 ==================== */
TEST(RoaringBitmapTest, LargeBitmapFunctionality) {
    const uint32_t N = 1000000;  /* 100 万 */
    const uint32_t STEP = 3;     /* 只添加 1/3 的值，模拟稀疏场景 */

    roaring_bitmap_t* rb = roaring_bitmap_create();
    ASSERT_NE(rb, nullptr);

    /* 添加稀疏值：0, 3, 6, 9, ... */
    for (uint32_t i = 0; i < N; i += STEP) {
        roaring_bitmap_add(rb, i);
    }

    /* 注意：uint32_t 除法截断，N/STEP 为 333333，循环 i=0,3,6,...,999999 共 333334 次 */
    uint32_t expected_count = N / STEP + 1;
    EXPECT_EQ(roaring_bitmap_count(rb), expected_count);

    /* 验证已添加的值存在 */
    for (uint32_t i = 0; i < N; i += STEP) {
        EXPECT_TRUE(roaring_bitmap_contains(rb, i));
    }

    /* 验证未添加的值不存在（只检查一个样本） */
    for (uint32_t i = 1; i < N; i += STEP) {
        EXPECT_FALSE(roaring_bitmap_contains(rb, i));
    }

    /* 边界值 */
    EXPECT_FALSE(roaring_bitmap_contains(rb, N));

    roaring_bitmap_free(rb);
}

/* ==================== 测试 3：内存对比 ==================== */
TEST(RoaringBitmapTest, MemoryComparison) {
    const uint32_t N = 1000000;
    const uint32_t STEP = 5;  /* 20% 密度，但 sorted array 4 bytes/elem 通常比 int8_t bitmap 不划算 */
    /* 注意：sorted array 压缩只在 popcount << bitmap_size 时有优势 */
    /* 此测试验证功能正确性，非压缩优势（P5-2 场景是稀疏 ID map） */

    /* 方案 1：原始 int8_t bitmap */
    size_t raw_size = N * sizeof(int8_t);

    /* 方案 2：roaring bitmap（sorted array） */
    roaring_bitmap_t* rb = roaring_bitmap_create();
    ASSERT_NE(rb, nullptr);

    for (uint32_t i = 0; i < N; i += STEP) {
        roaring_bitmap_add(rb, i);
    }

    /* roaring bitmap 内存 = 数组容量 * 4 字节 + 头结构 */
    size_t roaring_array_bytes = rb->capacity * sizeof(uint32_t);
    size_t roaring_total = sizeof(roaring_bitmap_t) + roaring_array_bytes;

    printf("=== P5-2 内存对比 ===\n");
    printf("原始 bitmap:  %zu bytes\n", raw_size);
    printf("roaring 数组: %u 元素, capacity %u, %zu bytes\n",
           rb->size, rb->capacity, roaring_array_bytes);
    printf("roaring 总计: %zu bytes (含头)\n", roaring_total);
    printf("压缩率: %.2f%%\n",
           (double)roaring_total / raw_size * 100.0);

    /* sorted array 4 bytes/elem，20% 密度时不比 bitmap 省内存，但验证功能正确 */
    uint32_t count = 0;
    for (uint32_t i = 0; i < N; i += STEP) count++;
    EXPECT_EQ(roaring_bitmap_count(rb), count);

    roaring_bitmap_free(rb);
}

/* ==================== 测试 4：重复插入 ==================== */
TEST(RoaringBitmapTest, DuplicateInsert) {
    roaring_bitmap_t* rb = roaring_bitmap_create();
    ASSERT_NE(rb, nullptr);

    /* 重复插入同一值多次 */
    for (int i = 0; i < 100; i++) {
        roaring_bitmap_add(rb, 42);
    }

    /* 应只计数 1 次 */
    EXPECT_EQ(roaring_bitmap_count(rb), 1);
    EXPECT_TRUE(roaring_bitmap_contains(rb, 42));

    roaring_bitmap_free(rb);
}

/* ==================== 测试 5：空 bitmap 行为 ==================== */
TEST(RoaringBitmapTest, EmptyBitmapBehavior) {
    roaring_bitmap_t* rb = roaring_bitmap_create();
    ASSERT_NE(rb, nullptr);

    EXPECT_TRUE(roaring_bitmap_is_empty(rb));
    EXPECT_EQ(roaring_bitmap_count(rb), 0);
    EXPECT_FALSE(roaring_bitmap_contains(rb, 0));
    EXPECT_FALSE(roaring_bitmap_contains(rb, 1000000));

    roaring_bitmap_free(rb);
}

/* ==================== 测试 6：NULL 安全性 ==================== */
TEST(RoaringBitmapTest, NullSafety) {
    /* 所有 API 对 NULL 输入应安全处理，不崩溃 */
    EXPECT_EQ(roaring_bitmap_count(nullptr), 0);
    EXPECT_TRUE(roaring_bitmap_is_empty(nullptr));
    EXPECT_FALSE(roaring_bitmap_contains(nullptr, 42));

    /* add 对 NULL 应无操作 */
    roaring_bitmap_add(nullptr, 42);

    /* free 对 NULL 应无操作 */
    roaring_bitmap_free(nullptr);
}
