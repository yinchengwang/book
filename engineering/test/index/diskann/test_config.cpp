/*
 * test_config.cpp
 *
 * DiskANN 统一配置测试。
 */

#include <gtest/gtest.h>
#include <index/vector_index/diskann/diskann.h>

namespace {

/* ============================================================================
 * 配置创建测试
 * ============================================================================ */

TEST(ConfigTest, CreateDefaultConfig)
{
    diskann_config_t *config = diskann_config_default(128, DISTANCE_METRIC_L2);

    ASSERT_NE(nullptr, config);
    EXPECT_EQ(128, config->dims);
    EXPECT_EQ(32, config->index_size);
    EXPECT_EQ(48, config->build_search_list_size);
    EXPECT_EQ(DISTANCE_METRIC_L2, config->metric);

    /* 默认不启用任何优化 */
    EXPECT_FALSE(config->quantization.enabled);
    EXPECT_FALSE(config->merge.enabled);
    EXPECT_FALSE(config->spann.enabled);
    EXPECT_FALSE(config->fresh.enabled);

    diskann_config_free(config);
}

TEST(ConfigTest, CreateFullConfig)
{
    diskann_config_t *config = diskann_config_create(256, 64, 96, DISTANCE_METRIC_COSINE);

    ASSERT_NE(nullptr, config);
    EXPECT_EQ(256, config->dims);
    EXPECT_EQ(64, config->index_size);
    EXPECT_EQ(96, config->build_search_list_size);
    EXPECT_EQ(DISTANCE_METRIC_COSINE, config->metric);

    diskann_config_free(config);
}

/* ============================================================================
 * 配置验证测试
 * ============================================================================ */

TEST(ConfigTest, ValidateDefaultConfig)
{
    diskann_config_t *config = diskann_config_default(128, DISTANCE_METRIC_L2);
    ASSERT_NE(nullptr, config);

    EXPECT_EQ(1, diskann_config_validate(config));

    diskann_config_free(config);
}

TEST(ConfigTest, ValidateQuantizationConfig)
{
    diskann_config_t *config = diskann_config_default(128, DISTANCE_METRIC_L2);
    ASSERT_NE(nullptr, config);

    /* 无效的 bits */
    config->quantization.enabled = true;
    config->quantization.bits = 3; /* 无效 */
    EXPECT_EQ(0, diskann_quantization_config_validate(&config->quantization));

    /* 有效的 bits */
    config->quantization.bits = 8;
    EXPECT_EQ(1, diskann_quantization_config_validate(&config->quantization));

    diskann_config_free(config);
}

TEST(ConfigTest, ValidateMergeConfig)
{
    diskann_config_t *config = diskann_config_default(128, DISTANCE_METRIC_L2);
    ASSERT_NE(nullptr, config);

    /* 无效的分区数 */
    config->merge.enabled = true;
    config->merge.partition_count = 1; /* 无效，需要 >= 2 */
    EXPECT_EQ(0, diskann_merge_config_validate(&config->merge));

    /* 无效的 overlap_ratio */
    config->merge.partition_count = 4;
    config->merge.overlap_ratio = 1.0f; /* 无效，需要 < 1.0 */
    EXPECT_EQ(0, diskann_merge_config_validate(&config->merge));

    /* 有效的配置 */
    config->merge.overlap_ratio = 0.15f;
    EXPECT_EQ(1, diskann_merge_config_validate(&config->merge));

    diskann_config_free(config);
}

TEST(ConfigTest, ValidateSpannConfig)
{
    diskann_config_t *config = diskann_config_default(128, DISTANCE_METRIC_L2);
    ASSERT_NE(nullptr, config);

    /* 无效的最大分区数 */
    config->spann.enabled = true;
    config->spann.max_partitions = 0; /* 无效 */
    EXPECT_EQ(0, diskann_spann_config_validate(&config->spann));

    /* 无效的搜索分区数 */
    config->spann.max_partitions = 128;
    config->spann.search_partitions = 256; /* 超过最大值 */
    EXPECT_EQ(0, diskann_spann_config_validate(&config->spann));

    /* 有效的配置 */
    config->spann.search_partitions = 8;
    EXPECT_EQ(1, diskann_spann_config_validate(&config->spann));

    diskann_config_free(config);
}

TEST(ConfigTest, ValidateFreshConfig)
{
    diskann_config_t *config = diskann_config_default(128, DISTANCE_METRIC_L2);
    ASSERT_NE(nullptr, config);

    /* 无效的容量 */
    config->fresh.enabled = true;
    config->fresh.fresh_capacity = 0; /* 无效 */
    EXPECT_EQ(0, diskann_fresh_config_validate(&config->fresh));

    /* 无效的合并阈值 */
    config->fresh.fresh_capacity = 100000;
    config->fresh.merge_threshold = 200000; /* 超过容量 */
    EXPECT_EQ(0, diskann_fresh_config_validate(&config->fresh));

    /* 有效的配置 */
    config->fresh.merge_threshold = 80000;
    EXPECT_EQ(1, diskann_fresh_config_validate(&config->fresh));

    diskann_config_free(config);
}

/* ============================================================================
 * 配置克隆测试
 * ============================================================================ */

TEST(ConfigTest, CloneConfig)
{
    diskann_config_t *config = diskann_config_default(128, DISTANCE_METRIC_L2);
    ASSERT_NE(nullptr, config);

    /* 修改原始配置 */
    config->quantization.enabled = true;
    config->quantization.bits = 8;
    config->merge.enabled = true;
    config->merge.partition_count = 8;

    /* 克隆配置 */
    diskann_config_t *clone = diskann_config_clone(config);
    ASSERT_NE(nullptr, clone);

    /* 验证克隆值 */
    EXPECT_EQ(config->dims, clone->dims);
    EXPECT_EQ(config->index_size, clone->index_size);
    EXPECT_EQ(config->quantization.enabled, clone->quantization.enabled);
    EXPECT_EQ(config->quantization.bits, clone->quantization.bits);
    EXPECT_EQ(config->merge.enabled, clone->merge.enabled);
    EXPECT_EQ(config->merge.partition_count, clone->merge.partition_count);

    /* 修改克隆不影响原始 */
    clone->quantization.bits = 4;
    EXPECT_NE(config->quantization.bits, clone->quantization.bits);

    diskann_config_free(config);
    diskann_config_free(clone);
}

/* ============================================================================
 * 配置与旧参数互转测试
 * ============================================================================ */

TEST(ConfigTest, FromParams)
{
    diskann_quantization_params_t pq_params = {16, 8, 4096, true};
    diskann_storage_params_t storage_params = {4096, 4};

    diskann_config_t *config = diskann_config_from_params(128, 32, 48, DISTANCE_METRIC_L2,
                                                          &pq_params, &storage_params);
    ASSERT_NE(nullptr, config);

    EXPECT_EQ(128, config->dims);
    EXPECT_EQ(32, config->index_size);
    EXPECT_TRUE(config->quantization.enabled);
    EXPECT_EQ(16, config->quantization.subquantizers);
    EXPECT_EQ(8, config->quantization.bits);
    EXPECT_EQ(4096, config->storage.page_size);

    diskann_config_free(config);
}

TEST(ConfigTest, ToParams)
{
    diskann_config_t *config = diskann_config_default(128, DISTANCE_METRIC_L2);
    ASSERT_NE(nullptr, config);

    config->quantization.enabled = true;
    config->quantization.subquantizers = 16;
    config->quantization.bits = 8;
    config->quantization.train_max_vectors = 4096;

    diskann_quantization_params_t pq_params;
    diskann_storage_params_t storage_params;

    EXPECT_EQ(0, diskann_config_to_params(config, &pq_params, &storage_params));
    EXPECT_TRUE(pq_params.enabled);
    EXPECT_EQ(16, pq_params.pq_m);
    EXPECT_EQ(8, pq_params.pq_bits);

    diskann_config_free(config);
}

/* ============================================================================
 * 使用统一配置创建索引测试
 * ============================================================================ */

TEST(ConfigTest, CreateIndexWithConfig)
{
    diskann_config_t *config = diskann_config_default(128, DISTANCE_METRIC_L2);
    ASSERT_NE(nullptr, config);

    diskann_t *index = diskann_index_create_with_config(config);
    ASSERT_NE(nullptr, index);

    EXPECT_EQ(128, diskann_index_dims(index));
    EXPECT_FALSE(diskann_index_is_built(index));

    diskann_index_drop(index);
    diskann_config_free(config);
}

TEST(ConfigTest, CreateIndexWithQuantization)
{
    diskann_config_t *config = diskann_config_default(128, DISTANCE_METRIC_L2);
    ASSERT_NE(nullptr, config);

    config->quantization.enabled = true;
    config->quantization.bits = 8;

    diskann_t *index = diskann_index_create_with_config(config);
    ASSERT_NE(nullptr, index);

    EXPECT_FALSE(diskann_index_has_pq(index)); /* 需要训练后才启用 */

    diskann_index_drop(index);
    diskann_config_free(config);
}

TEST(ConfigTest, CreateIndexWithFreshEnabled)
{
    diskann_config_t *config = diskann_config_default(128, DISTANCE_METRIC_L2);
    ASSERT_NE(nullptr, config);

    config->fresh.enabled = true;
    config->fresh.fresh_capacity = 1000;
    config->fresh.merge_threshold = 800;

    diskann_t *index = diskann_index_create_with_config(config);
    ASSERT_NE(nullptr, index);

    /* Fresh 索引可以正常创建 */
    EXPECT_EQ(128, diskann_index_dims(index));

    diskann_index_drop(index);
    diskann_config_free(config);
}

} // namespace
