/*
 * test_fresh.cpp
 *
 * DiskANN FreshDiskANN 测试。
 */

#include <gtest/gtest.h>
#include <index/vector_index/diskann/diskann.h>
#include <index/vector_index/diskann/diskann_fresh.h>
#include <cstdlib>
#include <cstring>

namespace {

constexpr int32_t TEST_DIMS = 8;

/* ============================================================================
 * Fresh 配置验证测试
 * ============================================================================ */

TEST(FreshTest, ConfigValidation)
{
    diskann_fresh_config_t config;

    /* 有效配置 */
    config.enabled = true;
    config.fresh_capacity = 100000;
    config.merge_threshold = 80000;
    EXPECT_EQ(1, diskann_fresh_config_validate(&config));

    /* 无效：fresh_capacity <= 0 */
    config.fresh_capacity = 0;
    EXPECT_EQ(0, diskann_fresh_config_validate(&config));
    config.fresh_capacity = 100000;

    /* 无效：merge_threshold > fresh_capacity */
    config.merge_threshold = 200000;
    EXPECT_EQ(0, diskann_fresh_config_validate(&config));
}

/* ============================================================================
 * Fresh 上下文创建/销毁测试
 * ============================================================================ */

TEST(FreshTest, ContextCreateFree)
{
    diskann_fresh_config_t config = {
        true,
        1000,
        800
    };

    diskann_fresh_context_t *ctx = diskann_fresh_create(&config, TEST_DIMS);
    ASSERT_NE(nullptr, ctx);

    diskann_fresh_free(ctx);
}

TEST(FreshTest, ContextCreateWithNull)
{
    diskann_fresh_context_t *ctx = diskann_fresh_create(nullptr, TEST_DIMS);
    EXPECT_EQ(nullptr, ctx);
}

/* ============================================================================
 * Fresh 初始化测试
 * ============================================================================ */

TEST(FreshTest, Init)
{
    diskann_fresh_config_t config = {
        true,
        100,
        80
    };

    diskann_fresh_context_t *ctx = diskann_fresh_create(&config, TEST_DIMS);
    ASSERT_NE(nullptr, ctx);

    EXPECT_EQ(0, diskann_fresh_init(ctx, TEST_DIMS));
    EXPECT_EQ(0, diskann_fresh_count(ctx));
    EXPECT_EQ(100, diskann_fresh_capacity(ctx));

    diskann_fresh_free(ctx);
}

/* ============================================================================
 * Fresh 插入测试
 * ============================================================================ */

TEST(FreshTest, Insert)
{
    diskann_fresh_config_t config = {
        true,
        100,
        80
    };

    diskann_fresh_context_t *ctx = diskann_fresh_create(&config, TEST_DIMS);
    ASSERT_NE(nullptr, ctx);

    EXPECT_EQ(0, diskann_fresh_init(ctx, TEST_DIMS));

    /* 插入向量 */
    float vector[TEST_DIMS];
    for (int32_t i = 0; i < TEST_DIMS; ++i) {
        vector[i] = (float)rand() / RAND_MAX;
    }

    int32_t label = -1;
    EXPECT_EQ(0, diskann_fresh_insert(ctx, vector, &label));
    EXPECT_EQ(0, label);
    EXPECT_EQ(1, diskann_fresh_count(ctx));

    /* 插入多个向量 */
    for (int32_t i = 1; i < 10; ++i) {
        for (int32_t j = 0; j < TEST_DIMS; ++j) {
            vector[j] = (float)rand() / RAND_MAX;
        }
        EXPECT_EQ(0, diskann_fresh_insert(ctx, vector, nullptr));
    }
    EXPECT_EQ(10, diskann_fresh_count(ctx));

    diskann_fresh_free(ctx);
}

TEST(FreshTest, InsertCapacityFull)
{
    diskann_fresh_config_t config = {
        true,
        5,
        4
    };

    diskann_fresh_context_t *ctx = diskann_fresh_create(&config, TEST_DIMS);
    ASSERT_NE(nullptr, ctx);

    EXPECT_EQ(0, diskann_fresh_init(ctx, TEST_DIMS));

    /* 插入满容量 */
    float vector[TEST_DIMS];
    for (int32_t i = 0; i < 5; ++i) {
        for (int32_t j = 0; j < TEST_DIMS; ++j) {
            vector[j] = (float)rand() / RAND_MAX;
        }
        EXPECT_EQ(0, diskann_fresh_insert(ctx, vector, nullptr));
    }

    /* 再插入应该返回容量满 */
    EXPECT_EQ(1, diskann_fresh_insert(ctx, vector, nullptr));

    diskann_fresh_free(ctx);
}

/* ============================================================================
 * Fresh 构建测试
 * ============================================================================ */

TEST(FreshTest, Build)
{
    diskann_fresh_config_t config = {
        true,
        100,
        80
    };

    diskann_fresh_context_t *ctx = diskann_fresh_create(&config, TEST_DIMS);
    ASSERT_NE(nullptr, ctx);

    EXPECT_EQ(0, diskann_fresh_init(ctx, TEST_DIMS));

    /* 插入向量 */
    float vector[TEST_DIMS];
    for (int32_t i = 0; i < 50; ++i) {
        for (int32_t j = 0; j < TEST_DIMS; ++j) {
            vector[j] = (float)rand() / RAND_MAX;
        }
        EXPECT_EQ(0, diskann_fresh_insert(ctx, vector, nullptr));
    }

    /* 构建图 */
    EXPECT_EQ(0, diskann_fresh_build(ctx, 16, 24));

    diskann_fresh_free(ctx);
}

/* ============================================================================
 * Fresh 搜索测试
 * ============================================================================ */

TEST(FreshTest, Search)
{
    diskann_fresh_config_t config = {
        true,
        100,
        80
    };

    diskann_fresh_context_t *ctx = diskann_fresh_create(&config, TEST_DIMS);
    ASSERT_NE(nullptr, ctx);

    EXPECT_EQ(0, diskann_fresh_init(ctx, TEST_DIMS));

    /* 插入向量 */
    float vector[TEST_DIMS];
    for (int32_t i = 0; i < 50; ++i) {
        for (int32_t j = 0; j < TEST_DIMS; ++j) {
            vector[j] = (float)i / 50.0f; /* 有一定规律 */
        }
        EXPECT_EQ(0, diskann_fresh_insert(ctx, vector, nullptr));
    }

    /* 构建图 */
    EXPECT_EQ(0, diskann_fresh_build(ctx, 16, 24));

    /* 搜索 */
    float query[TEST_DIMS];
    for (int32_t i = 0; i < TEST_DIMS; ++i) {
        query[i] = 0.5f;
    }

    float distances[10];
    int32_t labels[10];
    EXPECT_EQ(0, diskann_fresh_search(ctx, query, 10, 50, distances, labels));

    /* 验证结果 */
    for (int32_t i = 0; i < 10; ++i) {
        EXPECT_GE(labels[i], 0);
        EXPECT_LT(labels[i], 50);
    }

    diskann_fresh_free(ctx);
}

TEST(FreshTest, SearchEmpty)
{
    diskann_fresh_config_t config = {
        true,
        100,
        80
    };

    diskann_fresh_context_t *ctx = diskann_fresh_create(&config, TEST_DIMS);
    ASSERT_NE(nullptr, ctx);

    EXPECT_EQ(0, diskann_fresh_init(ctx, TEST_DIMS));

    /* 搜索空索引 */
    float query[TEST_DIMS] = {0};
    float distances[10];
    int32_t labels[10];

    EXPECT_EQ(0, diskann_fresh_search(ctx, query, 10, 50, distances, labels));

    /* 应该返回空结果 */
    for (int32_t i = 0; i < 10; ++i) {
        EXPECT_EQ(-1, labels[i]);
    }

    diskann_fresh_free(ctx);
}

/* ============================================================================
 * Fresh 合并检查测试
 * ============================================================================ */

TEST(FreshTest, ShouldMerge)
{
    diskann_fresh_config_t config = {
        true,
        10,
        5
    };

    diskann_fresh_context_t *ctx = diskann_fresh_create(&config, TEST_DIMS);
    ASSERT_NE(nullptr, ctx);

    EXPECT_EQ(0, diskann_fresh_init(ctx, TEST_DIMS));
    EXPECT_EQ(0, diskann_fresh_should_merge(ctx));

    /* 插入向量直到超过阈值 */
    float vector[TEST_DIMS];
    for (int32_t i = 0; i < 6; ++i) {
        for (int32_t j = 0; j < TEST_DIMS; ++j) {
            vector[j] = (float)rand() / RAND_MAX;
        }
        diskann_fresh_insert(ctx, vector, nullptr);
    }

    /* 应该需要合并 */
    EXPECT_EQ(1, diskann_fresh_should_merge(ctx));

    diskann_fresh_free(ctx);
}

/* ============================================================================
 * Fresh 清空测试
 * ============================================================================ */

TEST(FreshTest, Clear)
{
    diskann_fresh_config_t config = {
        true,
        100,
        80
    };

    diskann_fresh_context_t *ctx = diskann_fresh_create(&config, TEST_DIMS);
    ASSERT_NE(nullptr, ctx);

    EXPECT_EQ(0, diskann_fresh_init(ctx, TEST_DIMS));

    /* 插入向量 */
    float vector[TEST_DIMS];
    for (int32_t i = 0; i < 50; ++i) {
        for (int32_t j = 0; j < TEST_DIMS; ++j) {
            vector[j] = (float)rand() / RAND_MAX;
        }
        diskann_fresh_insert(ctx, vector, nullptr);
    }

    EXPECT_EQ(50, diskann_fresh_count(ctx));

    /* 清空 */
    EXPECT_EQ(0, diskann_fresh_clear(ctx));
    EXPECT_EQ(0, diskann_fresh_count(ctx));

    diskann_fresh_free(ctx);
}

/* ============================================================================
 * Fresh 获取向量测试
 * ============================================================================ */

TEST(FreshTest, GetVectors)
{
    diskann_fresh_config_t config = {
        true,
        100,
        80
    };

    diskann_fresh_context_t *ctx = diskann_fresh_create(&config, TEST_DIMS);
    ASSERT_NE(nullptr, ctx);

    EXPECT_EQ(0, diskann_fresh_init(ctx, TEST_DIMS));

    /* 插入向量 */
    float vector[TEST_DIMS];
    for (int32_t i = 0; i < 10; ++i) {
        for (int32_t j = 0; j < TEST_DIMS; ++j) {
            vector[j] = (float)i / 10.0f;
        }
        EXPECT_EQ(0, diskann_fresh_insert(ctx, vector, nullptr));
    }

    /* 获取向量 */
    float *vectors = nullptr;
    int32_t count = 0;
    EXPECT_EQ(0, diskann_fresh_get_vectors(ctx, &vectors, &count));
    EXPECT_EQ(10, count);
    EXPECT_NE(nullptr, vectors);

    diskann_fresh_free(ctx);
}

/* ============================================================================
 * Fresh 持久化测试
 * ============================================================================ */

TEST(FreshTest, PersistLoad)
{
    diskann_fresh_config_t config = {
        true,
        100,
        80
    };

    diskann_fresh_context_t *ctx = diskann_fresh_create(&config, TEST_DIMS);
    ASSERT_NE(nullptr, ctx);

    EXPECT_EQ(0, diskann_fresh_init(ctx, TEST_DIMS));

    /* 插入向量 */
    float vector[TEST_DIMS];
    for (int32_t i = 0; i < 50; ++i) {
        for (int32_t j = 0; j < TEST_DIMS; ++j) {
            vector[j] = (float)i / 50.0f;
        }
        diskann_fresh_insert(ctx, vector, nullptr);
    }

    /* 持久化 */
    FILE *fp = tmpfile();
    ASSERT_NE(nullptr, fp);
    EXPECT_EQ(0, diskann_fresh_persist(ctx, fp));

    /* 加载 */
    rewind(fp);
    diskann_fresh_context_t *loaded = diskann_fresh_load(&config, TEST_DIMS, fp);
    if (loaded) {
        EXPECT_EQ(50, diskann_fresh_count(loaded));
        diskann_fresh_free(loaded);
    }

    fclose(fp);
    diskann_fresh_free(ctx);
}

} // namespace
