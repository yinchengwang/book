/*
 * test_merge.cpp
 *
 * DiskANN Merge-Vamana 测试。
 */

#include <gtest/gtest.h>
#include <index/vector_index/diskann/diskann.h>
#include <index/vector_index/diskann/diskann_merge.h>
#include <cstdlib>

namespace {

constexpr int32_t TEST_DIMS = 8;
constexpr int32_t TEST_N = 100;

/* ============================================================================
 * Merge 配置验证测试
 * ============================================================================ */

TEST(MergeTest, ConfigValidation)
{
    diskann_merge_config_t config;

    /* 有效配置 */
    config.enabled = true;
    config.partition_method = DISKANN_PARTITION_KMEANS;
    config.partition_count = 4;
    config.overlap_ratio = 0.15f;
    EXPECT_EQ(1, diskann_merge_config_validate(&config));

    /* 无效：分区数 < 2 */
    config.partition_count = 1;
    EXPECT_EQ(0, diskann_merge_config_validate(&config));
    config.partition_count = 4;

    /* 无效：overlap_ratio >= 1.0 */
    config.overlap_ratio = 1.0f;
    EXPECT_EQ(0, diskann_merge_config_validate(&config));
    config.overlap_ratio = -0.1f;
    EXPECT_EQ(0, diskann_merge_config_validate(&config));
}

/* ============================================================================
 * Merge 上下文创建/销毁测试
 * ============================================================================ */

TEST(MergeTest, ContextCreateFree)
{
    diskann_merge_config_t config = {
        true,
        DISKANN_PARTITION_KMEANS,
        4,
        0.1f
    };

    diskann_merge_context_t *ctx = diskann_merge_context_create(&config);
    ASSERT_NE(nullptr, ctx);
    EXPECT_EQ(4, ctx->config.partition_count);

    diskann_merge_context_free(ctx);
}

TEST(MergeTest, ContextCreateWithNull)
{
    diskann_merge_context_t *ctx = diskann_merge_context_create(nullptr);
    EXPECT_EQ(nullptr, ctx);
}

/* ============================================================================
 * Merge 分区测试
 * ============================================================================ */

TEST(MergeTest, PartitionData)
{
    float *vectors = (float *)malloc(TEST_N * TEST_DIMS * sizeof(float));
    ASSERT_NE(nullptr, vectors);

    /* 生成测试数据 */
    for (int32_t i = 0; i < TEST_N * TEST_DIMS; ++i) {
        vectors[i] = (float)rand() / RAND_MAX;
    }

    diskann_merge_config_t config = {
        true,
        DISKANN_PARTITION_KMEANS,
        4,
        0.1f
    };

    diskann_merge_context_t *ctx = diskann_merge_context_create(&config);
    ASSERT_NE(nullptr, ctx);

    EXPECT_EQ(0, diskann_merge_partition_data(ctx, vectors, TEST_N, TEST_DIMS));

    diskann_merge_context_free(ctx);
    free(vectors);
}

/* ============================================================================
 * Merge 子图构建测试
 * ============================================================================ */

TEST(MergeTest, BuildSubgraphs)
{
    float *vectors = (float *)malloc(TEST_N * TEST_DIMS * sizeof(float));
    ASSERT_NE(nullptr, vectors);

    /* 生成测试数据 */
    for (int32_t i = 0; i < TEST_N * TEST_DIMS; ++i) {
        vectors[i] = (float)rand() / RAND_MAX;
    }

    diskann_merge_config_t config = {
        true,
        DISKANN_PARTITION_KMEANS,
        4,
        0.1f
    };

    diskann_merge_context_t *ctx = diskann_merge_context_create(&config);
    ASSERT_NE(nullptr, ctx);

    /* 先分区 */
    EXPECT_EQ(0, diskann_merge_partition_data(ctx, vectors, TEST_N, TEST_DIMS));

    /* 构建子图 */
    diskann_t **subgraphs = nullptr;
    int32_t subgraph_count = 0;
    EXPECT_EQ(0, diskann_merge_build_subgraphs(ctx, vectors, TEST_N, TEST_DIMS, 16, 24,
                                            &subgraphs, &subgraph_count));

    if (subgraphs) {
        EXPECT_EQ(4, subgraph_count);
        for (int32_t i = 0; i < subgraph_count; ++i) {
            if (subgraphs[i]) {
                diskann_index_drop(subgraphs[i]);
            }
        }
        free(subgraphs);
    }

    diskann_merge_context_free(ctx);
    free(vectors);
}

/* ============================================================================
 * ID 映射测试
 * ============================================================================ */

TEST(MergeTest, IdMapping)
{
    diskann_merge_config_t config = {
        true,
        DISKANN_PARTITION_KMEANS,
        2,
        0.1f
    };

    diskann_merge_context_t *ctx = diskann_merge_context_create(&config);
    ASSERT_NE(nullptr, ctx);

    /* 模拟已有一些节点 */
    ctx->global_node_count = 100;
    ctx->global_node_id_map = (int32_t *)malloc(100 * sizeof(int32_t));
    ctx->reverse_map = (int32_t *)malloc(100 * sizeof(int32_t));

    for (int32_t i = 0; i < 100; ++i) {
        ctx->global_node_id_map[i] = i;
        ctx->reverse_map[i] = i;
    }

    /* 测试 ID 映射 */
    EXPECT_EQ(50, diskann_merge_to_global_id(ctx, 50));
    EXPECT_EQ(50, diskann_merge_to_original_id(ctx, 50));
    EXPECT_EQ(-1, diskann_merge_to_global_id(ctx, -1));
    EXPECT_EQ(-1, diskann_merge_to_global_id(ctx, 200));

    diskann_merge_context_free(ctx);
}

/* ============================================================================
 * 图合并测试
 * ============================================================================ */

TEST(MergeTest, MergeGraphs)
{
    float *vectors = (float *)malloc(TEST_N * TEST_DIMS * sizeof(float));
    ASSERT_NE(nullptr, vectors);

    /* 生成测试数据 */
    for (int32_t i = 0; i < TEST_N * TEST_DIMS; ++i) {
        vectors[i] = (float)rand() / RAND_MAX;
    }

    diskann_merge_config_t config = {
        true,
        DISKANN_PARTITION_KMEANS,
        2,
        0.1f
    };

    diskann_merge_context_t *ctx = diskann_merge_context_create(&config);
    ASSERT_NE(nullptr, ctx);

    /* 先分区 */
    EXPECT_EQ(0, diskann_merge_partition_data(ctx, vectors, TEST_N, TEST_DIMS));

    /* 构建子图 */
    diskann_t **subgraphs = nullptr;
    int32_t subgraph_count = 0;
    EXPECT_EQ(0, diskann_merge_build_subgraphs(ctx, vectors, TEST_N, TEST_DIMS, 16, 24,
                                            &subgraphs, &subgraph_count));

    if (subgraphs) {
        /* 合并子图 */
        diskann_t *merged = nullptr;
        EXPECT_EQ(0, diskann_merge_graphs(ctx, subgraphs, subgraph_count,
                                       vectors, TEST_N, TEST_DIMS, &merged));

        if (merged) {
            EXPECT_GE(diskann_index_size(merged), TEST_N);
            diskann_index_drop(merged);
        }

        for (int32_t i = 0; i < subgraph_count; ++i) {
            if (subgraphs[i]) {
                diskann_index_drop(subgraphs[i]);
            }
        }
        free(subgraphs);
    }

    diskann_merge_context_free(ctx);
    free(vectors);
}

} // namespace
