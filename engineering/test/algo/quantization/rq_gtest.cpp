#include <gtest/gtest.h>

extern "C" {
#include <algo-prod/quantization/quantization.h>
#include <algo-prod/quantization/rq.h>
}

#include <cstdlib>
#include <cstring>
#include <vector>

// 测试工具：生成随机向量
static std::vector<float> generate_vectors(int32_t n, int32_t dims, float range = 10.0f)
{
    std::vector<float> vectors((size_t)n * dims);
    for (int32_t i = 0; i < n * dims; ++i) {
        vectors[i] = (float)(rand() % 1000) / 1000.0f * range;
    }
    return vectors;
}

// RQ 8+1 bit 配置验证
TEST(QuantizationRQ, ConfigValidation8Plus1)
{
    quantizer_config_t config;
    quantizer_config_init(&config, 128, QUANTIZATION_TYPE_RQ);
    config.rq_pq_bits       = 8;
    config.rq_residual_bits = 1;
    EXPECT_EQ(quantizer_config_validate(&config), 1);
}

// RQ 无效 PQ bits
TEST(QuantizationRQ, ConfigValidationInvalidPQBits)
{
    quantizer_config_t config;
    quantizer_config_init(&config, 128, QUANTIZATION_TYPE_RQ);
    config.rq_pq_bits       = 5;  // 无效
    config.rq_residual_bits = 1;
    EXPECT_EQ(quantizer_config_validate(&config), 0);
}

// RQ 无效残差 bits
TEST(QuantizationRQ, ConfigValidationInvalidResidualBits)
{
    quantizer_config_t config;
    quantizer_config_init(&config, 128, QUANTIZATION_TYPE_RQ);
    config.rq_pq_bits       = 8;
    config.rq_residual_bits = 3;  // 无效
    EXPECT_EQ(quantizer_config_validate(&config), 0);
}

// RQ 创建测试
TEST(QuantizationRQ, Create)
{
    quantizer_config_t config;
    quantizer_config_init(&config, 128, QUANTIZATION_TYPE_RQ);
    config.rq_pq_bits       = 8;
    config.rq_residual_bits = 1;

    quantizer_t *q = quantizer_create(&config);
    ASSERT_NE(q, nullptr);
    EXPECT_FALSE(quantizer_is_trained(q));
    quantizer_drop(q);
}

// RQ 训练测试
TEST(QuantizationRQ, Train)
{
    quantizer_config_t config;
    quantizer_config_init(&config, 128, QUANTIZATION_TYPE_RQ);
    config.rq_pq_bits       = 8;
    config.rq_residual_bits = 1;

    quantizer_t *q = quantizer_create(&config);
    ASSERT_NE(q, nullptr);

    auto vectors = generate_vectors(500, 128);
    EXPECT_EQ(quantizer_train(q, 500, vectors.data()), 0);
    EXPECT_TRUE(quantizer_is_trained(q));

    quantizer_drop(q);
}

// RQ 码字大小测试
// dims=128, default_pq_subquantizers 返回 8 (因为 128%8==0)
TEST(QuantizationRQ, CodeSize)
{
    quantizer_config_t config;
    quantizer_config_init(&config, 128, QUANTIZATION_TYPE_RQ);
    config.rq_pq_bits       = 8;
    config.rq_residual_bits = 1;

    quantizer_t *q = quantizer_create(&config);
    ASSERT_NE(q, nullptr);

    // m=8, rq_bits=1: 8 + ceil(8*1/8) = 8 + 1 = 9
    EXPECT_EQ(quantizer_code_size(q), 9);

    quantizer_drop(q);
}

// RQ 2-bit 码字大小
TEST(QuantizationRQ, CodeSize2Bit)
{
    quantizer_config_t config;
    quantizer_config_init(&config, 128, QUANTIZATION_TYPE_RQ);
    config.rq_pq_bits       = 8;
    config.rq_residual_bits = 2;

    quantizer_t *q = quantizer_create(&config);
    ASSERT_NE(q, nullptr);

    // m=8, rq_bits=2: 8 + ceil(8*2/8) = 8 + 2 = 10
    EXPECT_EQ(quantizer_code_size(q), 10);

    quantizer_drop(q);
}

// RQ 编码测试
TEST(QuantizationRQ, Encode)
{
    quantizer_config_t config;
    quantizer_config_init(&config, 64, QUANTIZATION_TYPE_RQ);
    config.rq_pq_bits       = 8;
    config.rq_residual_bits = 1;

    quantizer_t *q = quantizer_create(&config);
    ASSERT_NE(q, nullptr);

    auto vectors = generate_vectors(200, 64);
    EXPECT_EQ(quantizer_train(q, 200, vectors.data()), 0);

    uint8_t code[20];
    EXPECT_EQ(quantizer_encode(q, vectors.data(), code), 0);

    quantizer_drop(q);
}

// RQ 距离计算测试
TEST(QuantizationRQ, DistanceComputation)
{
    quantizer_config_t config;
    quantizer_config_init(&config, 64, QUANTIZATION_TYPE_RQ);
    config.rq_pq_bits       = 8;
    config.rq_residual_bits = 1;

    quantizer_t *q = quantizer_create(&config);
    ASSERT_NE(q, nullptr);

    auto vectors = generate_vectors(200, 64);
    EXPECT_EQ(quantizer_train(q, 200, vectors.data()), 0);

    int32_t cs = quantizer_code_size(q);
    uint8_t *code = (uint8_t *)malloc(cs);
    ASSERT_NE(code, nullptr);
    EXPECT_EQ(quantizer_encode(q, vectors.data(), code), 0);

    std::vector<float> table(quantizer_distance_table_size(q));
    EXPECT_EQ(quantizer_compute_distance_table(q, DISTANCE_METRIC_L2_SQUARED, vectors.data(), table.data()), 0);

    float dist = quantizer_adc_distance(q, code, table.data());
    EXPECT_GE(dist, 0.0f);

    free(code);
    quantizer_drop(q);
}

// RQ 批量编码
TEST(QuantizationRQ, BatchEncode)
{
    quantizer_config_t config;
    quantizer_config_init(&config, 64, QUANTIZATION_TYPE_RQ);
    config.rq_pq_bits       = 8;
    config.rq_residual_bits = 1;

    quantizer_t *q = quantizer_create(&config);
    ASSERT_NE(q, nullptr);

    auto vectors = generate_vectors(200, 64);
    EXPECT_EQ(quantizer_train(q, 200, vectors.data()), 0);

    int32_t n  = 10;
    int32_t cs = quantizer_code_size(q);
    uint8_t *codes = (uint8_t *)malloc((size_t)n * cs);
    ASSERT_NE(codes, nullptr);

    EXPECT_EQ(quantizer_encode_batch(q, n, vectors.data(), codes), 0);

    free(codes);
    quantizer_drop(q);
}

// RQ 模型导出测试
TEST(QuantizationRQ, ModelExport)
{
    quantizer_config_t config;
    quantizer_config_init(&config, 64, QUANTIZATION_TYPE_RQ);
    config.rq_pq_bits       = 8;
    config.rq_residual_bits = 1;

    quantizer_t *q = quantizer_create(&config);
    ASSERT_NE(q, nullptr);

    auto vectors = generate_vectors(200, 64);
    EXPECT_EQ(quantizer_train(q, 200, vectors.data()), 0);

    int32_t model_count = quantizer_model_float_count(q);
    EXPECT_GT(model_count, 0);

    std::vector<float> model(model_count);
    int32_t codebook_size;
    EXPECT_EQ(quantizer_export_model(q, model.data(), model_count, &codebook_size), model_count);

    quantizer_drop(q);
}