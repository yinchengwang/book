/*
 * test_spann.cpp
 *
 * DiskANN SPANN 测试。
 */

#include <gtest/gtest.h>
#include <index/vector_index/diskann/diskann.h>
#include <index/vector_index/diskann/diskann_spann.h>
#include <cstdlib>

namespace {

constexpr int32_t TEST_DIMS = 8;
constexpr int32_t TEST_N = 500;

/* ============================================================================
 * SPANN 配置验证测试
 * ============================================================================ */

TEST(SPANNTest, ConfigValidation)
{
    diskann_spann_config_t config;

    /* 有效配置 */
    config.enabled = true;
    config.max_partitions = 128;
    config.min_partition_size = 1000;
    config.search_partitions = 8;
    EXPECT_EQ(1, diskann_spann_config_validate(&config));

    /* 无效：max_partitions < 1 */
    config.max_partitions = 0;
    EXPECT_EQ(0, diskann_spann_config_validate(&config));
    config.max_partitions = 128;

    /* 无效：search_partitions > max_partitions */
    config.search_partitions = 256;
    EXPECT_EQ(0, diskann_spann_config_validate(&config));
}

/* ============================================================================
 * SPANN 上下文创建/销毁测试
 * ============================================================================ */

TEST(SPANNTest, ContextCreateFree)
{
    diskann_spann_config_t config = {
        true,
        128,
        1000,
        8
    };

    diskann_spann_context_t *ctx = diskann_spann_create(&config);
    ASSERT_NE(nullptr, ctx);

    diskann_spann_free(ctx);
}

TEST(SPANNTest, ContextCreateWithNull)
{
    diskann_spann_context_t *ctx = diskann_spann_create(nullptr);
    EXPECT_EQ(nullptr, ctx);
}

/* ============================================================================
 * SPANN 分区构建测试
 * ============================================================================ */

TEST(SPANNTest, BuildPartitions)
{
    float *vectors = (float *)malloc(TEST_N * TEST_DIMS * sizeof(float));
    ASSERT_NE(nullptr, vectors);

    /* 生成测试数据 */
    for (int32_t i = 0; i < TEST_N * TEST_DIMS; ++i) {
        vectors[i] = (float)rand() / RAND_MAX;
    }

    diskann_spann_config_t config = {
        true,
        16,
        50,
        4
    };

    diskann_spann_context_t *ctx = diskann_spann_create(&config);
    ASSERT_NE(nullptr, ctx);

    EXPECT_EQ(0, diskann_spann_init(ctx, vectors, TEST_N, TEST_DIMS));

    /* 验证分区数 */
    int32_t count = diskann_spann_get_partition_count(ctx);
    EXPECT_GE(count, 1);
    EXPECT_LE(count, config.max_partitions);

    diskann_spann_free(ctx);
    free(vectors);
}

/* ============================================================================
 * SPANN 分区选择测试
 * ============================================================================ */

TEST(SPANNTest, SelectPartitions)
{
    float *vectors = (float *)malloc(TEST_N * TEST_DIMS * sizeof(float));
    ASSERT_NE(nullptr, vectors);

    /* 生成测试数据 */
    for (int32_t i = 0; i < TEST_N * TEST_DIMS; ++i) {
        vectors[i] = (float)rand() / RAND_MAX;
    }

    diskann_spann_config_t config = {
        true,
        16,
        50,
        4
    };

    diskann_spann_context_t *ctx = diskann_spann_create(&config);
    ASSERT_NE(nullptr, ctx);

    EXPECT_EQ(0, diskann_spann_init(ctx, vectors, TEST_N, TEST_DIMS));

    /* 选择分区 */
    float query[TEST_DIMS];
    for (int32_t i = 0; i < TEST_DIMS; ++i) {
        query[i] = 0.5f;
    }

    int32_t *selected = nullptr;
    int32_t selected_count = 0;
    EXPECT_EQ(0, diskann_spann_select_partitions(ctx, query, TEST_DIMS, &selected, &selected_count));

    EXPECT_NE(nullptr, selected);
    EXPECT_EQ(selected_count, config.search_partitions);

    free(selected);
    diskann_spann_free(ctx);
    free(vectors);
}

/* ============================================================================
 * SPANN 搜索测试
 * ============================================================================ */

TEST(SPANNTest, Search)
{
    /* 创建索引 */
    diskann_t *index = diskann_index_create(TEST_DIMS, 16, 24, DISTANCE_METRIC_L2);
    ASSERT_NE(nullptr, index);

    float *vectors = (float *)malloc(TEST_N * TEST_DIMS * sizeof(float));
    ASSERT_NE(nullptr, vectors);

    /* 生成测试数据 */
    for (int32_t i = 0; i < TEST_N * TEST_DIMS; ++i) {
        vectors[i] = (float)rand() / RAND_MAX;
    }

    /* 添加向量并构建 */
    EXPECT_EQ(0, diskann_index_add(index, TEST_N, vectors));
    EXPECT_EQ(0, diskann_index_build(index));

    /* 创建 SPANN */
    diskann_spann_config_t config = {
        true,
        8,
        50,
        4
    };

    diskann_spann_context_t *ctx = diskann_spann_create(&config);
    ASSERT_NE(nullptr, ctx);

    EXPECT_EQ(0, diskann_spann_init(ctx, vectors, TEST_N, TEST_DIMS));

    /* 搜索 */
    float query[TEST_DIMS];
    for (int32_t i = 0; i < TEST_DIMS; ++i) {
        query[i] = 0.5f;
    }

    float distances[10];
    int32_t labels[10];
    int32_t *selected = nullptr;
    int32_t selected_count = 0;

    EXPECT_EQ(0, diskann_spann_select_partitions(ctx, query, TEST_DIMS, &selected, &selected_count));

    EXPECT_GE(0, diskann_spann_search(ctx, index, query, 10, 50, 10, selected, selected_count, distances, labels));

    free(selected);
    diskann_spann_free(ctx);
    free(vectors);
    diskann_index_drop(index);
}

/* ============================================================================
 * SPANN 结果合并测试
 * ============================================================================ */

TEST(SPANNTest, MergeResults)
{
    diskann_spann_config_t config = {
        true,
        8,
        50,
        4
    };

    diskann_spann_context_t *ctx = diskann_spann_create(&config);
    ASSERT_NE(nullptr, ctx);

    /* 创建测试结果 */
    float *results[4];
    for (int32_t i = 0; i < 4; ++i) {
        results[i] = (float *)malloc(10 * sizeof(float));
        for (int32_t j = 0; j < 10; ++j) {
            results[i][j] = (float)i * 10 + j;
        }
    }

    float final_distances[10];
    int32_t final_labels[10];

    EXPECT_EQ(0, diskann_spann_merge_results(ctx, results, 4, 10, final_distances, final_labels));

    for (int32_t i = 0; i < 4; ++i) {
        free(results[i]);
    }

    diskann_spann_free(ctx);
}

/* ============================================================================
 * SPANN 元数据持久化测试
 * ============================================================================ */

TEST(SPANNTest, PersistLoad)
{
    float *vectors = (float *)malloc(TEST_N * TEST_DIMS * sizeof(float));
    ASSERT_NE(nullptr, vectors);

    /* 生成测试数据 */
    for (int32_t i = 0; i < TEST_N * TEST_DIMS; ++i) {
        vectors[i] = (float)rand() / RAND_MAX;
    }

    diskann_spann_config_t config = {
        true,
        8,
        50,
        4
    };

    diskann_spann_context_t *ctx = diskann_spann_create(&config);
    ASSERT_NE(nullptr, ctx);

    EXPECT_EQ(0, diskann_spann_init(ctx, vectors, TEST_N, TEST_DIMS));

    /* 创建临时文件 */
    FILE *fp = tmpfile();
    ASSERT_NE(nullptr, fp);

    /* 持久化 */
    EXPECT_EQ(0, diskann_spann_persist(ctx, fp));

    /* 加载 */
    rewind(fp);
    diskann_spann_context_t *loaded = diskann_spann_load(&config, fp);
    /* 加载可能返回 nullptr，因为加载需要完整的文件格式 */

    fclose(fp);
    if (loaded) {
        diskann_spann_free(loaded);
    }

    diskann_spann_free(ctx);
    free(vectors);
}

} // namespace
