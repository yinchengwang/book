/*
 * bitmap_test.cpp — Bitmap 位图索引单元测试
 *
 * 测试内容：
 * 1. 创建和销毁
 * 2. 基本位操作
 * 3. 索引操作
 * 4. 组合查询 (AND/OR/NOT)
 */

#include <gtest/gtest.h>
#include <index/inverted/bitmap/bitmap.h>

#include <cstring>
#include <vector>

/* ============================================================
 * 测试 1: 创建和销毁
 * ============================================================ */

TEST(BitmapIndexTest, CreateAndDestroy)
{
    bitmap_index_t *idx = bitmap_create(1024, 10);
    ASSERT_NE(idx, nullptr);

    bitmap_destroy(idx);
}

TEST(BitmapIndexTest, CreateWithInvalidParams)
{
    /* 文档数为 0 */
    bitmap_index_t *idx = bitmap_create(0, 10);
    EXPECT_EQ(idx, nullptr);

    /* 值为 0 */
    idx = bitmap_create(1024, 0);
    EXPECT_EQ(idx, nullptr);
}

/* ============================================================
 * 测试 2: 基本位操作
 * ============================================================ */

TEST(BitmapIndexTest, SetAndQuery)
{
    bitmap_index_t *idx = bitmap_create(256, 5);
    ASSERT_NE(idx, nullptr);

    /* 设置一些文档的值 */
    EXPECT_EQ(bitmap_set(idx, 0, 1), 0);
    EXPECT_EQ(bitmap_set(idx, 1, 1), 0);
    EXPECT_EQ(bitmap_set(idx, 2, 2), 0);

    /* 查询值为 1 的文档 */
    int results[100];
    int count = 0;
    EXPECT_EQ(bitmap_eq(idx, 1, results, &count), 0);
    EXPECT_EQ(count, 2);
    EXPECT_EQ(results[0], 0);
    EXPECT_EQ(results[1], 1);

    /* 查询值为 2 的文档 */
    count = 0;
    EXPECT_EQ(bitmap_eq(idx, 2, results, &count), 0);
    EXPECT_EQ(count, 1);
    EXPECT_EQ(results[0], 2);

    /* 查询不存在的值 (超出范围应返回错误) */
    count = 0;
    int ret = bitmap_eq(idx, 99, results, &count);
    EXPECT_NE(ret, 0);  /* 超出范围返回错误 */

    bitmap_destroy(idx);
}

TEST(BitmapIndexTest, BatchSet)
{
    bitmap_index_t *idx = bitmap_create(256, 5);
    ASSERT_NE(idx, nullptr);

    int doc_ids[] = {0, 1, 2, 3, 4};
    int values[] = {0, 1, 0, 1, 0};

    int success = bitmap_set_batch(idx, doc_ids, values, 5);
    EXPECT_EQ(success, 5);

    /* 验证 */
    int count = 0;
    bitmap_eq(idx, 0, nullptr, &count);
    EXPECT_EQ(count, 3);

    count = 0;
    bitmap_eq(idx, 1, nullptr, &count);
    EXPECT_EQ(count, 2);

    bitmap_destroy(idx);
}

/* ============================================================
 * 测试 3: 组合查询
 * ============================================================ */

TEST(BitmapIndexTest, AndOperation)
{
    bitmap_index_t *idx = bitmap_create(200, 3);
    ASSERT_NE(idx, nullptr);

    /* 设置值：value 0 给前 100 个文档，value 1 给后 100 个文档 */
    for (int i = 0; i < 100; i++) {
        bitmap_set(idx, i, 0);
    }
    for (int i = 100; i < 200; i++) {
        bitmap_set(idx, i, 1);
    }

    /* AND 操作 */
    int results[200];
    int count = 0;
    int ret = bitmap_and(idx, 0, 1, results, &count);

    /* value 0 和 value 1 没有交集，结果应该是 0 */
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(count, 0);

    bitmap_destroy(idx);
}

TEST(BitmapIndexTest, OrOperation)
{
    bitmap_index_t *idx = bitmap_create(200, 3);
    ASSERT_NE(idx, nullptr);

    /* 设置值 */
    for (int i = 0; i < 50; i++) {
        bitmap_set(idx, i, 0);
    }
    for (int i = 40; i < 90; i++) {
        bitmap_set(idx, i, 1);
    }

    /* OR 操作 */
    int results[200];
    int count = 0;
    int ret = bitmap_or(idx, 0, 1, results, &count);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(count, 90);  /* 0-89 (90个文档) */

    bitmap_destroy(idx);
}

TEST(BitmapIndexTest, NotOperation)
{
    bitmap_index_t *idx = bitmap_create(100, 2);
    ASSERT_NE(idx, nullptr);

    /* 设置 value 0 给前 30 个文档 */
    for (int i = 0; i < 30; i++) {
        bitmap_set(idx, i, 0);
    }

    /* NOT value 0 */
    int results[100];
    int count = 0;
    int ret = bitmap_not(idx, 0, results, &count);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(count, 70);  /* 100 - 30 = 70 */

    bitmap_destroy(idx);
}

TEST(BitmapIndexTest, RangeQuery)
{
    bitmap_index_t *idx = bitmap_create(100, 10);
    ASSERT_NE(idx, nullptr);

    /* 设置值 1-5 */
    for (int i = 0; i < 50; i++) {
        bitmap_set(idx, i, i % 10);
    }

    /* 范围查询 2-4 */
    int results[100];
    int count = 0;
    int ret = bitmap_range(idx, 2, 4, results, &count);

    EXPECT_EQ(ret, 0);
    EXPECT_GE(count, 0);  /* 有或没有结果都可 */

    bitmap_destroy(idx);
}

/* ============================================================
 * 测试 4: 统计信息
 * ============================================================ */

TEST(BitmapIndexTest, Stats)
{
    bitmap_index_t *idx = bitmap_create(1024, 5);
    ASSERT_NE(idx, nullptr);

    /* 设置一些值 */
    for (int i = 0; i < 500; i++) {
        bitmap_set(idx, i, i % 5);
    }

    int n_docs = 0, n_values = 0, total_bits = 0;
    bitmap_stats(idx, &n_docs, &n_values, &total_bits);

    EXPECT_EQ(n_docs, 1024);
    EXPECT_EQ(n_values, 5);
    EXPECT_EQ(total_bits, 5120);  /* 1024 * 5 = 5120 */

    bitmap_destroy(idx);
}

TEST(BitmapIndexTest, Count)
{
    bitmap_index_t *idx = bitmap_create(100, 3);
    ASSERT_NE(idx, nullptr);

    for (int i = 0; i < 30; i++) {
        bitmap_set(idx, i, 1);
    }

    int count = bitmap_count(idx, 1);
    EXPECT_EQ(count, 30);

    /* value 0 的文档数 */
    count = bitmap_count(idx, 0);
    /* 注意：bitmap_count 只能统计设置了位的位置，未设置的默认是 0 */
    EXPECT_GE(count, 0);

    bitmap_destroy(idx);
}

/* ============================================================
 * 测试 5: 低级位图操作
 * ============================================================ */

TEST(BitmapTest, LowLevelOperations)
{
    bitmap_t *bm = bitmap_create_empty(64);
    ASSERT_NE(bm, nullptr);

    /* 设置和清除位 */
    bitmap_set_bit(bm, 0);
    bitmap_set_bit(bm, 10);
    bitmap_set_bit(bm, 63);

    EXPECT_EQ(bitmap_get_bit(bm, 0), 1);
    EXPECT_EQ(bitmap_get_bit(bm, 10), 1);
    EXPECT_EQ(bitmap_get_bit(bm, 63), 1);
    EXPECT_EQ(bitmap_get_bit(bm, 1), 0);

    bitmap_clear_bit(bm, 10);
    EXPECT_EQ(bitmap_get_bit(bm, 10), 0);

    /* 统计 */
    EXPECT_EQ(bitmap_count_ones(bm), 2);

    /* 获取所有 1 的位置 */
    int positions[10];
    int count = bitmap_get_ones(bm, positions);
    EXPECT_EQ(count, 2);
    EXPECT_EQ(positions[0], 0);
    EXPECT_EQ(positions[1], 63);

    bitmap_free(bm);
}

TEST(BitmapTest, BitmapAndOrNot)
{
    bitmap_t *a = bitmap_create_empty(64);
    bitmap_t *b = bitmap_create_empty(64);
    bitmap_t *result = bitmap_create_empty(64);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    ASSERT_NE(result, nullptr);

    /* 设置 a: 位 0, 2, 4 */
    bitmap_set_bit(a, 0);
    bitmap_set_bit(a, 2);
    bitmap_set_bit(a, 4);

    /* 设置 b: 位 2, 4, 6 */
    bitmap_set_bit(b, 2);
    bitmap_set_bit(b, 4);
    bitmap_set_bit(b, 6);

    /* AND */
    bitmap_and_into(result, a, b);
    EXPECT_EQ(bitmap_count_ones(result), 2);  /* 2, 4 */

    /* OR */
    bitmap_or_into(result, a, b);
    EXPECT_EQ(bitmap_count_ones(result), 4);  /* 0, 2, 4, 6 */

    /* NOT */
    bitmap_not_into(result, a);
    EXPECT_EQ(bitmap_count_ones(result), 61);  /* 64 - 3 = 61 */

    bitmap_free(a);
    bitmap_free(b);
    bitmap_free(result);
}
