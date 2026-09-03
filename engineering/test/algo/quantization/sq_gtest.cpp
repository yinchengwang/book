#include <gtest/gtest.h>

extern "C" {
#include <algo-prod/quantization/quantization.h>
#include <algo-prod/quantization/sq.h>
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

// SQ 8-bit 配置验证测试
TEST(QuantizationSQ, ConfigValidation8Bit)
{
    quantizer_config_t config;
    quantizer_config_init(&config, 128, QUANTIZATION_TYPE_SQ);
    config.sq_bits = 8;
    EXPECT_EQ(quantizer_config_validate(&config), 1);
}

// SQ 4-bit 配置验证测试
TEST(QuantizationSQ, ConfigValidation4Bit)
{
    quantizer_config_t config;
    quantizer_config_init(&config, 128, QUANTIZATION_TYPE_SQ);
    config.sq_bits = 4;
    EXPECT_EQ(quantizer_config_validate(&config), 1);
}

// SQ 无效 bits 测试
TEST(QuantizationSQ, ConfigValidationInvalidBits)
{
    quantizer_config_t config;
    quantizer_config_init(&config, 128, QUANTIZATION_TYPE_SQ);
    config.sq_bits = 6;  // 无效，SQ 只支持 4 或 8
    EXPECT_EQ(quantizer_config_validate(&config), 0);
}

// SQ 创建与训练测试
TEST(QuantizationSQ, CreateAndTrain)
{
    quantizer_config_t config;
    quantizer_config_init(&config, 128, QUANTIZATION_TYPE_SQ);
    config.sq_bits = 8;

    quantizer_t *q = quantizer_create(&config);
    ASSERT_NE(q, nullptr);
    EXPECT_FALSE(quantizer_is_trained(q));

    // 训练
    auto vectors = generate_vectors(1000, 128);
    EXPECT_EQ(quantizer_train(q, 1000, vectors.data()), 0);
    EXPECT_TRUE(quantizer_is_trained(q));

    quantizer_drop(q);
}

// SQ 编码测试
TEST(QuantizationSQ, Encode)
{
    quantizer_config_t config;
    quantizer_config_init(&config, 8, QUANTIZATION_TYPE_SQ);
    config.sq_bits = 8;

    quantizer_t *q = quantizer_create(&config);
    ASSERT_NE(q, nullptr);

    // 训练
    std::vector<float> vectors = {
        0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f,
        0.5f, 1.5f, 2.5f, 3.5f, 4.5f, 5.5f, 6.5f, 7.5f
    };
    EXPECT_EQ(quantizer_train(q, 2, vectors.data()), 0);

    // 编码
    uint8_t code[16];
    memset(code, 0, sizeof(code));
    std::vector<float> vec = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
    EXPECT_EQ(quantizer_encode(q, vec.data(), code), 0);

    // 验证码字不为空
    bool has_nonzero = false;
    for (int i = 0; i < 8; ++i) {
        if (code[i] != 0) {
            has_nonzero = true;
            break;
        }
    }
    EXPECT_TRUE(has_nonzero);

    quantizer_drop(q);
}

// SQ 码字大小测试
TEST(QuantizationSQ, CodeSize)
{
    quantizer_config_t config;
    quantizer_config_init(&config, 128, QUANTIZATION_TYPE_SQ);
    config.sq_bits = 8;

    quantizer_t *q = quantizer_create(&config);
    ASSERT_NE(q, nullptr);

    // 8-bit: 8 (header) + 128 (数据) = 136
    EXPECT_EQ(quantizer_code_size(q), 136);

    quantizer_drop(q);
}

// SQ 4-bit 码字大小测试
TEST(QuantizationSQ, CodeSize4Bit)
{
    quantizer_config_t config;
    quantizer_config_init(&config, 128, QUANTIZATION_TYPE_SQ);
    config.sq_bits = 4;

    quantizer_t *q = quantizer_create(&config);
    ASSERT_NE(q, nullptr);

    // 4-bit: 8 (header) + ceil(128*4/8) = 8 + 64 = 72
    EXPECT_EQ(quantizer_code_size(q), 72);

    quantizer_drop(q);
}

// SQ 距离表大小测试
TEST(QuantizationSQ, DistanceTableSize)
{
    quantizer_config_t config;
    quantizer_config_init(&config, 128, QUANTIZATION_TYPE_SQ);
    config.sq_bits = 8;

    quantizer_t *q = quantizer_create(&config);
    ASSERT_NE(q, nullptr);

    // SQ 的距离表大小 = dims
    EXPECT_EQ(quantizer_distance_table_size(q), 128);

    quantizer_drop(q);
}

// SQ 距离计算测试
TEST(QuantizationSQ, DistanceComputation)
{
    quantizer_config_t config;
    quantizer_config_init(&config, 128, QUANTIZATION_TYPE_SQ);
    config.sq_bits = 8;

    quantizer_t *q = quantizer_create(&config);
    ASSERT_NE(q, nullptr);

    // 训练
    auto vectors = generate_vectors(1000, 128, 10.0f);
    EXPECT_EQ(quantizer_train(q, 1000, vectors.data()), 0);

    // 编码一个向量
    uint8_t *code = (uint8_t *)malloc(quantizer_code_size(q));
    ASSERT_NE(code, nullptr);
    EXPECT_EQ(quantizer_encode(q, vectors.data(), code), 0);

    // 计算距离表
    std::vector<float> table(quantizer_distance_table_size(q));
    EXPECT_EQ(quantizer_compute_distance_table(q, DISTANCE_METRIC_L2_SQUARED, vectors.data(), table.data()), 0);

    // 计算 ADC 距离
    float dist = quantizer_adc_distance(q, code, table.data());
    EXPECT_GE(dist, 0.0f);

    free(code);
    quantizer_drop(q);
}

// SQ 模型导出导入测试
TEST(QuantizationSQ, ModelExportImport)
{
    quantizer_config_t config;
    quantizer_config_init(&config, 128, QUANTIZATION_TYPE_SQ);
    config.sq_bits = 8;

    quantizer_t *q1 = quantizer_create(&config);
    ASSERT_NE(q1, nullptr);

    // 使用负值和正值，确保 min/max 不为零
    std::vector<float> vectors(128 * 10);
    for (int32_t i = 0; i < 128 * 10; ++i) {
        vectors[i] = (float)(rand() % 100) / 10.0f - 5.0f;  // 范围 -5 到 5
    }
    EXPECT_EQ(quantizer_train(q1, 10, vectors.data()), 0);

    // 导出模型
    float params[2];
    EXPECT_EQ(sq_export_model(q1, params), 2);
    EXPECT_LT(params[0], 0.0f);  // global_min 应为负数
    EXPECT_GT(params[1], 0.0f);  // scale 应为正数

    // 从模型创建量化器
    quantizer_t *q2 = quantizer_create_from_model(&config, 0, params, 2);
    ASSERT_NE(q2, nullptr);
    EXPECT_TRUE(quantizer_is_trained(q2));

    // 验证编码结果近似一致 (允许少量舍入差异)
    uint8_t code1[136], code2[136];
    EXPECT_EQ(quantizer_encode(q1, vectors.data(), code1), 0);
    EXPECT_EQ(quantizer_encode(q2, vectors.data(), code2), 0);

    int32_t diff_count = 0;
    for (int32_t i = 0; i < 136; ++i) {
        int32_t d = (int32_t)code1[i] - (int32_t)code2[i];
        if (d < 0) d = -d;
        if (d > 1) {
            diff_count++;
        }
    }
    EXPECT_LT(diff_count, 10);  // 允许少量舍入差异

    quantizer_drop(q1);
    quantizer_drop(q2);
}

// SQ 批量编码测试
TEST(QuantizationSQ, BatchEncode)
{
    quantizer_config_t config;
    quantizer_config_init(&config, 64, QUANTIZATION_TYPE_SQ);
    config.sq_bits = 8;

    quantizer_t *q = quantizer_create(&config);
    ASSERT_NE(q, nullptr);

    // 训练
    auto vectors = generate_vectors(100, 64);
    EXPECT_EQ(quantizer_train(q, 100, vectors.data()), 0);

    // 批量编码
    int32_t n = 10;
    int32_t cs = quantizer_code_size(q);
    uint8_t *codes = (uint8_t *)malloc((size_t)n * cs);
    ASSERT_NE(codes, nullptr);
    memset(codes, 0, (size_t)n * cs);

    EXPECT_EQ(quantizer_encode_batch(q, n, vectors.data(), codes), 0);

    // 验证码字不为空
    bool has_nonzero = false;
    for (int32_t i = 0; i < n * cs; ++i) {
        if (codes[i] != 0) {
            has_nonzero = true;
            break;
        }
    }
    EXPECT_TRUE(has_nonzero);

    free(codes);
    quantizer_drop(q);
}