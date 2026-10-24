/**
 * @file test_opq.cpp
 * @brief OPQ 旋转优化和量化器序列化测试
 */
#include <gtest/gtest.h>
#include <algo-prod/quantization/quantization.h>
#include <cstdlib>
#include <cstring>

class QuantizerOPQTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* 创建 PQ 量化器配置 */
        quantizer_config_init(&config, 128, QUANTIZATION_TYPE_PQ);
        config.pq_subquantizers = 16;
        config.pq_bits = 8;

        quantizer = quantizer_create(&config);
    }

    void TearDown() override {
        if (quantizer) {
            quantizer_drop(quantizer);
        }
        remove("test_quantizer.bin");
    }

    quantizer_config_t config;
    quantizer_t *quantizer = nullptr;
};

TEST_F(QuantizerOPQTest, EnableOPQ) {
    ASSERT_NE(nullptr, quantizer);

    /* 启用 OPQ */
    int32_t ret = quantizer_enable_opq(quantizer);
    EXPECT_EQ(0, ret);

    /* 验证 OPQ 已启用 */
    EXPECT_TRUE(quantizer->opq_enabled);
    EXPECT_NE(nullptr, quantizer->rotation_matrix);

    /* 获取旋转矩阵 */
    float rotation[128 * 128];
    ret = quantizer_get_opq_rotation(quantizer, rotation, 128 * 128);
    EXPECT_EQ(0, ret);
}

TEST_F(QuantizerOPQTest, EnableOPQNonPQ) {
    /* 创建 SQ 量化器 */
    quantizer_config_t sq_config;
    quantizer_config_init(&sq_config, 128, QUANTIZATION_TYPE_SQ);
    quantizer_t *sq = quantizer_create(&sq_config);
    ASSERT_NE(nullptr, sq);

    /* SQ 不支持 OPQ */
    int32_t ret = quantizer_enable_opq(sq);
    EXPECT_EQ(-1, ret);

    quantizer_drop(sq);
}

TEST_F(QuantizerOPQTest, SaveAndLoad) {
    /* 训练量化器 */
    float vectors[100 * 128];
    for (int i = 0; i < 100 * 128; i++) {
        vectors[i] = (float)(rand() % 100) / 100.0f;
    }
    int32_t ret = quantizer_train(quantizer, 100, vectors);
    EXPECT_EQ(0, ret);
    EXPECT_TRUE(quantizer_is_trained(quantizer));

    /* 保存量化器 */
    ret = quantizer_save(quantizer, "test_quantizer.bin");
    EXPECT_EQ(0, ret);

    /* 释放原量化器 */
    quantizer_drop(quantizer);
    quantizer = nullptr;

    /* 加载量化器 */
    quantizer = quantizer_load("test_quantizer.bin");
    ASSERT_NE(nullptr, quantizer);
    EXPECT_TRUE(quantizer_is_trained(quantizer));

    /* 验证配置 */
    EXPECT_EQ(quantizer_type(quantizer), QUANTIZATION_TYPE_PQ);
    EXPECT_EQ(quantizer->config.dims, 128);
    EXPECT_EQ(quantizer->config.pq_subquantizers, 16);
    EXPECT_EQ(quantizer->config.pq_bits, 8);
}

TEST_F(QuantizerOPQTest, SaveAndLoadWithOPQ) {
    /* 启用 OPQ */
    quantizer_enable_opq(quantizer);

    /* 训练量化器 */
    float vectors[100 * 128];
    for (int i = 0; i < 100 * 128; i++) {
        vectors[i] = (float)(rand() % 100) / 100.0f;
    }
    quantizer_train(quantizer, 100, vectors);

    /* 保存量化器 */
    quantizer_save(quantizer, "test_quantizer.bin");
    quantizer_drop(quantizer);
    quantizer = nullptr;

    /* 加载量化器 */
    quantizer = quantizer_load("test_quantizer.bin");
    ASSERT_NE(nullptr, quantizer);
    EXPECT_TRUE(quantizer->opq_enabled);
    EXPECT_NE(nullptr, quantizer->rotation_matrix);
}

TEST_F(QuantizerOPQTest, EncodeAfterLoad) {
    /* 训练量化器 */
    float vectors[100 * 128];
    for (int i = 0; i < 100 * 128; i++) {
        vectors[i] = (float)(rand() % 100) / 100.0f;
    }
    quantizer_train(quantizer, 100, vectors);

    /* 保存并重新加载 */
    quantizer_save(quantizer, "test_quantizer.bin");
    quantizer_drop(quantizer);
    quantizer = quantizer_load("test_quantizer.bin");
    ASSERT_NE(nullptr, quantizer);

    /* 编码应该工作 */
    uint8_t code[16];
    float vector[128];
    for (int i = 0; i < 128; i++) {
        vector[i] = (float)(rand() % 100) / 100.0f;
    }

    ret = quantizer_encode(quantizer, vector, code);
    EXPECT_GE(ret, 0);

    quantizer_drop(quantizer);
    quantizer = nullptr;
}

TEST_F(QuantizerOPQTest, DistanceTableAfterLoad) {
    /* 训练量化器 */
    float vectors[100 * 128];
    for (int i = 0; i < 100 * 128; i++) {
        vectors[i] = (float)(rand() % 100) / 100.0f;
    }
    quantizer_train(quantizer, 100, vectors);

    /* 保存并重新加载 */
    quantizer_save(quantizer, "test_quantizer.bin");
    quantizer_drop(quantizer);
    quantizer = quantizer_load("test_quantizer.bin");
    ASSERT_NE(nullptr, quantizer);

    /* 距离表计算应该工作 */
    float table[16 * 256];  /* pq_subquantizers * (2^pq_bits) */
    float query[128];
    for (int i = 0; i < 128; i++) {
        query[i] = (float)(rand() % 100) / 100.0f;
    }

    ret = quantizer_compute_distance_table(quantizer, DISTANCE_METRIC_L2_SQUARED,
                                           query, table);
    EXPECT_GE(ret, 0);

    quantizer_drop(quantizer);
    quantizer = nullptr;
}

TEST_F(QuantizerOPQTest, InvalidLoad) {
    /* 加载不存在的文件 */
    quantizer_t *q = quantizer_load("nonexistent.bin");
    EXPECT_EQ(nullptr, q);
}

TEST_F(QuantizerOPQTest, SQSaveLoad) {
    /* 创建 SQ 量化器 */
    quantizer_config_t sq_config;
    quantizer_config_init(&sq_config, 64, QUANTIZATION_TYPE_SQ);
    sq_config.sq_bits = 8;
    quantizer_t *sq = quantizer_create(&sq_config);
    ASSERT_NE(nullptr, sq);

    /* 训练 */
    float vectors[50 * 64];
    for (int i = 0; i < 50 * 64; i++) {
        vectors[i] = (float)(rand() % 100);
    }
    quantizer_train(sq, 50, vectors);

    /* 保存并加载 */
    quantizer_save(sq, "test_sq.bin");
    quantizer_drop(sq);
    sq = quantizer_load("test_sq.bin");
    ASSERT_NE(nullptr, sq);
    EXPECT_EQ(quantizer_type(sq), QUANTIZATION_TYPE_SQ);

    quantizer_drop(sq);
    remove("test_sq.bin");
}
