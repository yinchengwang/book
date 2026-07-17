/**
 * @file vec_page_test.cpp
 * @brief VecPage 向量页管理模块单元测试
 */
#include "db/storage/vector/vec_page.h"
#include <gtest/gtest.h>
#include <cstdlib>
#include <cstring>

class VecPageTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* 使用临时目录 */
        data_dir = "test_vec_page_data";
    }

    void TearDown() override {
        /* 清理测试数据 */
        remove((data_dir + "/vector_pages.dat").c_str());
        remove((data_dir + "/vector_pages.meta").c_str());
        rmdir(data_dir.c_str());
    }

    std::string data_dir;
};

TEST_F(VecPageTest, CreateAndDestroy) {
    /* 创建页池 */
    vector_page_pool_t *pool = vector_page_pool_create(
        data_dir.c_str(), 10, 4096, 128);
    ASSERT_NE(pool, nullptr);

    /* 验证初始化参数 */
    EXPECT_EQ(pool->max_pages, 10);
    EXPECT_EQ(pool->page_size, 4096);
    EXPECT_EQ(pool->vector_dim, 128);
    EXPECT_EQ(pool->page_count, 0);

    /* 销毁页池 */
    vector_page_pool_destroy(pool);
}

TEST_F(VecPageTest, AppendVectors) {
    vector_page_pool_t *pool = vector_page_pool_create(
        data_dir.c_str(), 10, 4096, 4);
    ASSERT_NE(pool, nullptr);

    /* 追加向量 */
    float v1[] = {1.0f, 2.0f, 3.0f, 4.0f};
    float v2[] = {5.0f, 6.0f, 7.0f, 8.0f};
    float v3[] = {9.0f, 10.0f, 11.0f, 12.0f};

    int32_t id1 = vector_page_append(pool, v1, -1);
    EXPECT_GE(id1, 0);

    int32_t id2 = vector_page_append(pool, v2, 100);
    EXPECT_EQ(id2, 100);

    int32_t id3 = vector_page_append(pool, v3, -1);
    EXPECT_GE(id3, 0);

    /* 验证统计 */
    EXPECT_EQ(pool->page_count, 1);
    EXPECT_EQ(pool->vectors_per_page * pool->page_count, 3);

    vector_page_pool_destroy(pool);
}

TEST_F(VecPageTest, AppendBatch) {
    vector_page_pool_t *pool = vector_page_pool_create(
        data_dir.c_str(), 10, 4096, 4);
    ASSERT_NE(pool, nullptr);

    /* 批量追加 */
    float vectors[10 * 4] = {0};
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 4; j++) {
            vectors[i * 4 + j] = (float)(i * 4 + j);
        }
    }

    int32_t count = vector_page_append_batch(pool, vectors, 10, -1);
    EXPECT_EQ(count, 10);
    EXPECT_EQ(pool->page_count, 1);

    vector_page_pool_destroy(pool);
}

TEST_F(VecPageTest, GetVector) {
    vector_page_pool_t *pool = vector_page_pool_create(
        data_dir.c_str(), 10, 4096, 4);
    ASSERT_NE(pool, nullptr);

    /* 追加并读取 */
    float v1[] = {1.0f, 2.0f, 3.0f, 4.0f};
    int32_t id = vector_page_append(pool, v1, 42);
    ASSERT_GE(id, 0);

    float out[4] = {0};
    int ret = vector_page_get_vector(pool, id, out);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(out[0], 1.0f);
    EXPECT_EQ(out[1], 2.0f);
    EXPECT_EQ(out[2], 3.0f);
    EXPECT_EQ(out[3], 4.0f);

    vector_page_pool_destroy(pool);
}

TEST_F(VecPageTest, PageEviction) {
    /* 创建只能容纳少量向量的页池 */
    vector_page_pool_t *pool = vector_page_pool_create(
        data_dir.c_str(), 2, 256, 4);  /* 每页只能存约 16 个向量 */
    ASSERT_NE(pool, nullptr);

    /* 追加超过容量的向量 */
    float v[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    for (int i = 0; i < 100; i++) {
        int32_t id = vector_page_append(pool, v, i);
        EXPECT_GE(id, 0);
    }

    /* 验证有驱逐发生 */
    EXPECT_GE(pool->evictions, 1u);

    /* 验证总向量数 */
    vector_page_stats_t stats;
    vector_page_get_stats(pool, &stats);
    EXPECT_EQ(stats.vectors_total, 100);

    vector_page_pool_destroy(pool);
}

TEST_F(VecPageTest, FlushAndLoad) {
    vector_page_pool_t *pool = vector_page_pool_create(
        data_dir.c_str(), 10, 4096, 4);
    ASSERT_NE(pool, nullptr);

    /* 追加数据 */
    float v1[] = {1.0f, 2.0f, 3.0f, 4.0f};
    vector_page_append(pool, v1, 0);

    /* 刷盘 */
    int ret = vector_page_flush_all(pool);
    EXPECT_EQ(ret, 0);

    /* 重新加载 */
    vector_page_pool_t *pool2 = vector_page_pool_create(
        data_dir.c_str(), 10, 4096, 4);
    ASSERT_NE(pool2, nullptr);

    ret = vector_page_pool_load(pool2);
    EXPECT_EQ(ret, 0);

    vector_page_pool_destroy(pool);
    vector_page_pool_destroy(pool2);
}

TEST_F(VecPageTest, Checksum) {
    vector_page_pool_t *pool = vector_page_pool_create(
        data_dir.c_str(), 10, 4096, 4);
    ASSERT_NE(pool, nullptr);

    /* 启用 CRC32 校验和 */
    vector_page_set_checksum(pool, VECTOR_PAGE_CHECKSUM_CRC32);

    /* 追加向量 */
    float v1[] = {1.0f, 2.0f, 3.0f, 4.0f};
    vector_page_append(pool, v1, 0);

    /* 验证页完整性 */
    EXPECT_TRUE(vector_page_verify(pool, 0));

    vector_page_pool_destroy(pool);
}

TEST_F(VecPageTest, HelperFunctions) {
    /* 测试容量计算 */
    int32_t cap = vector_page_capacity(4096, 128);
    EXPECT_GT(cap, 0);

    /* 测试页 ID 计算 */
    int32_t page_id = vector_page_id(100, 10);
    EXPECT_EQ(page_id, 10);

    /* 测试偏移计算 */
    int32_t offset = vector_page_offset(105, 10);
    EXPECT_EQ(offset, 5);
}

TEST_F(VecPageTest, Stats) {
    vector_page_pool_t *pool = vector_page_pool_create(
        data_dir.c_str(), 10, 4096, 4);
    ASSERT_NE(pool, nullptr);

    /* 追加一些向量并触发未命中 */
    float v[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    for (int i = 0; i < 10; i++) {
        vector_page_append(pool, v, i);
    }

    /* 获取统计 */
    vector_page_stats_t stats;
    vector_page_get_stats(pool, &stats);
    EXPECT_EQ(stats.vectors_total, 10);
    EXPECT_EQ(stats.page_count, 1);

    /* 重置统计 */
    vector_page_reset_stats(pool);
    vector_page_get_stats(pool, &stats);
    EXPECT_EQ(stats.hits, 0);
    EXPECT_EQ(stats.misses, 0);

    vector_page_pool_destroy(pool);
}
