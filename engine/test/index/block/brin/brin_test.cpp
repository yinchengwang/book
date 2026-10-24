/*
 * brin_test.cpp — BRIN 块范围索引单元测试
 *
 * 测试内容：
 * 1. 创建和销毁
 * 2. 基本插入
 * 3. 范围查询
 * 4. 页面扫描
 */

#include <gtest/gtest.h>
#include <index/block/brin/brin.h>

#include <cstring>
#include <vector>

/* ============================================================
 * 测试 1: 创建和销毁
 * ============================================================ */

TEST(BRINIndexTest, CreateAndDestroy)
{
    brin_index_t *idx = brin_create(8192, 128);
    ASSERT_NE(idx, nullptr);

    brin_destroy(idx);
}

TEST(BRINIndexTest, CreateWithInvalidParams)
{
    /* 页面大小为 0 */
    brin_index_t *idx = brin_create(0, 128);
    EXPECT_EQ(idx, nullptr);

    /* 每范围页面数为 0 */
    idx = brin_create(8192, 0);
    EXPECT_EQ(idx, nullptr);
}

/* ============================================================
 * 测试 2: 基本插入
 * ============================================================ */

TEST(BRINIndexTest, BasicInsert)
{
    brin_index_t *idx = brin_create(8192, 16);
    ASSERT_NE(idx, nullptr);

    /* 插入记录 */
    const char *key1 = "aaa";
    const char *key2 = "bbb";
    const char *key3 = "ccc";

    EXPECT_EQ(brin_insert(idx, 0, key1, 1), 0);
    EXPECT_EQ(brin_insert(idx, 1, key2, 2), 0);
    EXPECT_EQ(brin_insert(idx, 2, key3, 3), 0);

    /* 验证插入成功 */
    int n_ranges, n_pages, n_records;
    brin_stats(idx, &n_ranges, &n_pages, &n_records);
    EXPECT_GE(n_ranges, 1);
    EXPECT_GE(n_records, 3);

    brin_destroy(idx);
}

TEST(BRINIndexTest, BatchInsert)
{
    brin_index_t *idx = brin_create(8192, 16);
    ASSERT_NE(idx, nullptr);

    /* 批量插入 */
    const int page_nums[] = {0, 1, 2, 3, 4};
    const char *keys[] = {"key1", "key2", "key3", "key4", "key5"};
    const int doc_ids[] = {101, 102, 103, 104, 105};

    int success = brin_insert_batch(idx, page_nums, (const void **)keys, doc_ids, 5);
    EXPECT_EQ(success, 5);

    brin_destroy(idx);
}

/* ============================================================
 * 测试 3: 范围查询
 * ============================================================ */

TEST(BRINIndexTest, RangeQuery)
{
    brin_index_t *idx = brin_create(8192, 16);
    ASSERT_NE(idx, nullptr);

    /* 插入一些记录 */
    const char *keys[] = {"aaa", "bbb", "ccc", "ddd", "eee"};
    for (int i = 0; i < 5; i++) {
        brin_insert(idx, i, keys[i], i + 1);
    }

    /* 范围查询 "bbb" 到 "ddd" */
    int results[10];
    int count = 0;
    int ret = brin_range_query(idx, "bbb", "ddd", results, &count);

    EXPECT_EQ(ret, 0);
    EXPECT_GE(count, 1);  /* 至少包含 bbb, ccc, ddd 对应的记录 */

    brin_destroy(idx);
}

/* ============================================================
 * 测试 4: 页面扫描
 * ============================================================ */

TEST(BRINIndexTest, PageScan)
{
    brin_index_t *idx = brin_create(8192, 16);
    ASSERT_NE(idx, nullptr);

    /* 插入记录 */
    for (int i = 0; i < 10; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%03d", i + 10);
        EXPECT_EQ(brin_insert(idx, i + 10, key, i), 0);
    }

    /* 扫描已插入的页面范围 */
    int results[50];
    int count = 0;
    int ret = brin_scan(idx, 10, 15, results, &count);

    EXPECT_EQ(ret, 0);
    EXPECT_GE(count, 1);  /* 至少有一些结果 */

    brin_destroy(idx);
}

/* ============================================================
 * 测试 5: 统计信息
 * ============================================================ */

TEST(BRINIndexTest, Stats)
{
    brin_index_t *idx = brin_create(8192, 16);
    ASSERT_NE(idx, nullptr);

    /* 插入少量记录 */
    for (int i = 0; i < 10; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key%03d", i);
        brin_insert(idx, i, key, i);
    }

    int n_ranges = 0, n_pages = 0, n_records = 0;
    brin_stats(idx, &n_ranges, &n_pages, &n_records);

    EXPECT_GE(n_ranges, 1);
    EXPECT_GE(n_records, 1);

    brin_destroy(idx);
}