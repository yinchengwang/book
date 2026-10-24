/*
 * test_partition.cpp
 *
 * DiskANN 分区策略测试。
 */

#include <gtest/gtest.h>
#include <index/vector_index/diskann/diskann_partition.h>
#include <cstdlib>
#include <cstring>

namespace {

constexpr int32_t TEST_DIMS = 16;
constexpr int32_t TEST_N = 1000;
constexpr int32_t TEST_PARTITIONS = 4;

/* 生成测试向量 */
static void generate_test_vectors(float *vectors, int32_t n, int32_t dims)
{
    for (int32_t i = 0; i < n; ++i) {
        for (int32_t j = 0; j < dims; ++j) {
            vectors[i * dims + j] = (float)rand() / RAND_MAX;
        }
    }
}

/* ============================================================================
 * 参数验证测试
 * ============================================================================ */

TEST(PartitionTest, ValidateParams)
{
    EXPECT_EQ(1, diskann_partition_validate_params(DISKANN_PARTITION_RANDOM, 4, 0.1f));
    EXPECT_EQ(1, diskann_partition_validate_params(DISKANN_PARTITION_KMEANS, 8, 0.2f));
    EXPECT_EQ(1, diskann_partition_validate_params(DISKANN_PARTITION_AUTO, 2, 0.0f));

    /* 无效的分区数 */
    EXPECT_EQ(0, diskann_partition_validate_params(DISKANN_PARTITION_RANDOM, 1, 0.1f));
    EXPECT_EQ(0, diskann_partition_validate_params(DISKANN_PARTITION_RANDOM, 0, 0.1f));

    /* 无效的 overlap_ratio */
    EXPECT_EQ(0, diskann_partition_validate_params(DISKANN_PARTITION_RANDOM, 4, -0.1f));
    EXPECT_EQ(0, diskann_partition_validate_params(DISKANN_PARTITION_RANDOM, 4, 1.0f));
}

/* ============================================================================
 * 随机分区测试
 * ============================================================================ */

TEST(PartitionTest, RandomPartition)
{
    float *vectors = (float *)malloc(TEST_N * TEST_DIMS * sizeof(float));
    int32_t *partition_ids = (int32_t *)malloc(TEST_N * sizeof(int32_t));

    ASSERT_NE(nullptr, vectors);
    ASSERT_NE(nullptr, partition_ids);

    generate_test_vectors(vectors, TEST_N, TEST_DIMS);

    EXPECT_EQ(0, diskann_partition_random(vectors, TEST_N, TEST_DIMS, TEST_PARTITIONS, partition_ids));

    /* 验证所有向量都有分区 ID */
    for (int32_t i = 0; i < TEST_N; ++i) {
        EXPECT_GE(partition_ids[i], 0);
        EXPECT_LT(partition_ids[i], TEST_PARTITIONS);
    }

    free(vectors);
    free(partition_ids);
}

TEST(PartitionTest, RandomPartitionInvalidInput)
{
    float *vectors = nullptr;
    int32_t partition_ids[10];

    EXPECT_NE(0, diskann_partition_random(vectors, TEST_N, TEST_DIMS, TEST_PARTITIONS, partition_ids));
}

/* ============================================================================
 * K-Means 分区测试
 * ============================================================================ */

TEST(PartitionTest, KMeansPartition)
{
    float *vectors = (float *)malloc(TEST_N * TEST_DIMS * sizeof(float));
    int32_t *partition_ids = (int32_t *)malloc(TEST_N * sizeof(int32_t));
    float *centers = (float *)malloc(TEST_PARTITIONS * TEST_DIMS * sizeof(float));

    ASSERT_NE(nullptr, vectors);
    ASSERT_NE(nullptr, partition_ids);
    ASSERT_NE(nullptr, centers);

    generate_test_vectors(vectors, TEST_N, TEST_DIMS);

    EXPECT_EQ(0, diskann_partition_kmeans(vectors, TEST_N, TEST_DIMS, TEST_PARTITIONS, partition_ids, centers));

    /* 验证所有向量都有分区 ID */
    for (int32_t i = 0; i < TEST_N; ++i) {
        EXPECT_GE(partition_ids[i], 0);
        EXPECT_LT(partition_ids[i], TEST_PARTITIONS);
    }

    /* 验证中心点有效 */
    for (int32_t i = 0; i < TEST_PARTITIONS; ++i) {
        for (int32_t j = 0; j < TEST_DIMS; ++j) {
            EXPECT_LE(centers[i * TEST_DIMS + j], 1.0f);
            EXPECT_GE(centers[i * TEST_DIMS + j], 0.0f);
        }
    }

    free(vectors);
    free(partition_ids);
    free(centers);
}

/* ============================================================================
 * 坐标范围分区测试
 * ============================================================================ */

TEST(PartitionTest, CoordinateRangePartition)
{
    float *vectors = (float *)malloc(TEST_N * TEST_DIMS * sizeof(float));
    int32_t *partition_ids = (int32_t *)malloc(TEST_N * sizeof(int32_t));

    ASSERT_NE(nullptr, vectors);
    ASSERT_NE(nullptr, partition_ids);

    generate_test_vectors(vectors, TEST_N, TEST_DIMS);

    EXPECT_EQ(0, diskann_partition_coordinate_range(vectors, TEST_N, TEST_DIMS, TEST_PARTITIONS, partition_ids));

    /* 验证所有向量都有分区 ID */
    for (int32_t i = 0; i < TEST_N; ++i) {
        EXPECT_GE(partition_ids[i], 0);
        EXPECT_LT(partition_ids[i], TEST_PARTITIONS);
    }

    free(vectors);
    free(partition_ids);
}

/* ============================================================================
 * 自动选择分区策略测试
 * ============================================================================ */

TEST(PartitionTest, AutoPartition)
{
    float *vectors = (float *)malloc(TEST_N * TEST_DIMS * sizeof(float));
    int32_t *partition_ids = (int32_t *)malloc(TEST_N * sizeof(int32_t));

    ASSERT_NE(nullptr, vectors);
    ASSERT_NE(nullptr, partition_ids);

    generate_test_vectors(vectors, TEST_N, TEST_DIMS);

    /* 使用自动选择 */
    EXPECT_EQ(0, diskann_partition_data(vectors, TEST_N, TEST_DIMS,
                                        DISKANN_PARTITION_AUTO, TEST_PARTITIONS, partition_ids));

    /* 验证所有向量都有分区 ID */
    for (int32_t i = 0; i < TEST_N; ++i) {
        EXPECT_GE(partition_ids[i], 0);
        EXPECT_LT(partition_ids[i], TEST_PARTITIONS);
    }

    free(vectors);
    free(partition_ids);
}

/* ============================================================================
 * 分区结果管理测试
 * ============================================================================ */

TEST(PartitionTest, PartitionResultCreate)
{
    float *vectors = (float *)malloc(TEST_N * TEST_DIMS * sizeof(float));
    int32_t *partition_ids = (int32_t *)malloc(TEST_N * sizeof(int32_t));

    ASSERT_NE(nullptr, vectors);
    ASSERT_NE(nullptr, partition_ids);

    generate_test_vectors(vectors, TEST_N, TEST_DIMS);

    /* 先执行分区 */
    diskann_partition_random(vectors, TEST_N, TEST_DIMS, TEST_PARTITIONS, partition_ids);

    /* 创建分区结果 */
    diskann_partition_result_t *result = diskann_partition_result_create(
        vectors, TEST_N, TEST_DIMS, partition_ids, TEST_PARTITIONS);

    ASSERT_NE(nullptr, result);
    EXPECT_EQ(TEST_PARTITIONS, result->partition_count);
    EXPECT_NE(nullptr, result->partition_ids);

    /* 释放结果 */
    diskann_partition_result_free(result);

    free(vectors);
    free(partition_ids);
}

/* ============================================================================
 * 分区距离计算测试
 * ============================================================================ */

TEST(PartitionTest, DistanceToCenter)
{
    float vector[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float center[4] = {1.0f, 2.0f, 3.0f, 4.0f};

    float dist = diskann_partition_distance_to_center(vector, center, 4, DISTANCE_METRIC_L2);
    EXPECT_FLOAT_EQ(0.0f, dist);

    center[0] = 2.0f;
    dist = diskann_partition_distance_to_center(vector, center, 4, DISTANCE_METRIC_L2);
    EXPECT_GT(dist, 0.0f);
}

TEST(PartitionTest, GetOverlapVectors)
{
    float *vectors = (float *)malloc(TEST_N * TEST_DIMS * sizeof(float));
    int32_t *partition_ids = (int32_t *)malloc(TEST_N * sizeof(int32_t));

    ASSERT_NE(nullptr, vectors);
    ASSERT_NE(nullptr, partition_ids);

    generate_test_vectors(vectors, TEST_N, TEST_DIMS);
    diskann_partition_random(vectors, TEST_N, TEST_DIMS, TEST_PARTITIONS, partition_ids);

    int32_t overlap_count = 0;
    int32_t *overlap = diskann_partition_get_overlap_vectors(
        vectors, TEST_N, TEST_DIMS, partition_ids, TEST_PARTITIONS, 0.1f, &overlap_count);

    if (overlap) {
        /* 重叠向量数量可能为 0 */
        EXPECT_GE(overlap_count, 0);
        free(overlap);
    }

    free(vectors);
    free(partition_ids);
}

} // namespace
