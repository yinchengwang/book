/**
 * @file pq_test.cpp
 * @brief PQ 量化器单元测试
 */

#include <gtest/gtest.h>
#include <vector>
#include <cmath>
#include <algorithm>

extern "C" {
#include "db/index/vector_index/pq/pq.h"
}

class PQTest : public ::testing::Test {
protected:
    void SetUp() override {
        dims_ = 128;
        m_ = 16;
        bits_ = 8;
        n_train_ = 1000;
        n_test_ = 100;

        /* 生成随机训练数据 */
        train_vectors_.resize(n_train_ * dims_);
        test_vectors_.resize(n_test_ * dims_);

        for (int i = 0; i < n_train_ * dims_; i++) {
            train_vectors_[i] = (float)rand() / RAND_MAX;
        }

        for (int i = 0; i < n_test_ * dims_; i++) {
            test_vectors_[i] = (float)rand() / RAND_MAX;
        }
    }

    int dims_;
    int m_;
    int bits_;
    int n_train_;
    int n_test_;
    std::vector<float> train_vectors_;
    std::vector<float> test_vectors_;
};

/* 测试：创建和销毁 */
TEST_F(PQTest, CreateDestroy) {
    pq_quantizer_t *pq = pq_create(dims_, m_, bits_);
    ASSERT_NE(pq, nullptr);

    EXPECT_EQ(pq->dims, dims_);
    EXPECT_EQ(pq->m, m_);
    EXPECT_EQ(pq->bits, bits_);
    EXPECT_EQ(pq->ks, 256);  /* 2^8 */
    EXPECT_EQ(pq->sub_dim, dims_ / m_);
    EXPECT_EQ(pq_code_size(pq), m_);
    EXPECT_EQ(pq_distance_table_size(pq), m_ * 256);

    pq_destroy(pq);
}

/* 测试：训练 */
TEST_F(PQTest, Train) {
    pq_quantizer_t *pq = pq_create(dims_, m_, bits_);
    ASSERT_NE(pq, nullptr);

    EXPECT_EQ(pq_is_trained(pq), 0);

    int ret = pq_train(pq, n_train_, train_vectors_.data());
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(pq_is_trained(pq), 1);

    pq_destroy(pq);
}

/* 测试：编码和解码 */
TEST_F(PQTest, EncodeDecode) {
    pq_quantizer_t *pq = pq_create(dims_, m_, bits_);
    ASSERT_NE(pq, nullptr);

    pq_train(pq, n_train_, train_vectors_.data());

    /* 编码单个向量 */
    uint8_t code[16];
    int ret = pq_encode(pq, test_vectors_.data(), code);
    EXPECT_EQ(ret, 0);

    /* 解码 */
    float decoded[128];
    ret = pq_decode(pq, code, decoded);
    EXPECT_EQ(ret, 0);

    /* 检查解码结果是否合理（量化误差应该有限） */
    float error = 0.0f;
    for (int i = 0; i < dims_; i++) {
        float diff = test_vectors_[i] - decoded[i];
        error += diff * diff;
    }
    error = sqrtf(error / dims_);

    /* 平均量化误差应该小于 0.5（经验值） */
    EXPECT_LT(error, 0.5f);

    pq_destroy(pq);
}

/* 测试：批量编码 */
TEST_F(PQTest, BatchEncode) {
    pq_quantizer_t *pq = pq_create(dims_, m_, bits_);
    ASSERT_NE(pq, nullptr);

    pq_train(pq, n_train_, train_vectors_.data());

    std::vector<uint8_t> codes(n_test_ * m_);
    int encoded = pq_encode_batch(pq, n_test_, test_vectors_.data(), codes.data());

    EXPECT_EQ(encoded, n_test_);

    pq_destroy(pq);
}

/* 测试：距离表计算和 ADC 距离 */
TEST_F(PQTest, DistanceTableAndADC) {
    pq_quantizer_t *pq = pq_create(dims_, m_, bits_);
    ASSERT_NE(pq, nullptr);

    pq_train(pq, n_train_, train_vectors_.data());

    /* 编码一个查询向量 */
    uint8_t code[16];
    pq_encode(pq, test_vectors_.data(), code);

    /* 计算距离表 */
    std::vector<float> table(m_ * 256);
    int ret = pq_compute_distance_table(pq, test_vectors_.data(), table.data());
    EXPECT_EQ(ret, 0);

    /* ADC 距离应该接近 0（查询自身） */
    float adc_dist = pq_adc_distance(pq, code, table.data());
    /* 量化误差导致的自身距离，放宽阈值 */
    EXPECT_LT(adc_dist, 5.0f);  /* 自身距离应该较小（允许量化误差） */

    /* 编码另一个向量 */
    uint8_t code2[16];
    pq_encode(pq, &test_vectors_[dims_], code2);

    /* 不同向量的距离应该更大 */
    float adc_dist2 = pq_adc_distance(pq, code2, table.data());
    EXPECT_GT(adc_dist2, adc_dist);

    pq_destroy(pq);
}

/* 测试：SDT 距离 */
TEST_F(PQTest, SDTDistance) {
    pq_quantizer_t *pq = pq_create(dims_, m_, bits_);
    ASSERT_NE(pq, nullptr);

    pq_train(pq, n_train_, train_vectors_.data());

    uint8_t code1[16], code2[16];
    pq_encode(pq, test_vectors_.data(), code1);
    pq_encode(pq, &test_vectors_[dims_], code2);

    /* 自身距离应该为 0 */
    float self_dist = pq_sdt_distance(pq, code1, code1);
    EXPECT_LT(self_dist, 1e-6f);

    /* 不同编码距离应该为正 */
    float diff_dist = pq_sdt_distance(pq, code1, code2);
    EXPECT_GT(diff_dist, 0.0f);

    pq_destroy(pq);
}

/* 测试：压缩比验证 */
TEST_F(PQTest, CompressionRatio) {
    pq_quantizer_t *pq = pq_create(dims_, m_, bits_);
    ASSERT_NE(pq, nullptr);

    /* 原始向量大小：dims * 4 字节 */
    size_t original_size = dims_ * sizeof(float);

    /* 压缩后大小：m 字节 */
    size_t compressed_size = pq_code_size(pq);

    /* 压缩比 */
    float ratio = (float)original_size / compressed_size;
    printf("PQ 压缩比: %.2fx (%zu -> %zu 字节)\n",
           ratio, original_size, compressed_size);

    /* 对于 dims=128, m=16：压缩比 = 512 / 16 = 32x */
    EXPECT_GT(ratio, 10.0f);

    pq_destroy(pq);
}

/* 测试：量化误差分析 */
TEST_F(PQTest, QuantizationError) {
    pq_quantizer_t *pq = pq_create(dims_, m_, bits_);
    ASSERT_NE(pq, nullptr);

    pq_train(pq, n_train_, train_vectors_.data());

    float total_error = 0.0f;
    int n_samples = 100;

    for (int i = 0; i < n_samples; i++) {
        uint8_t code[16];
        pq_encode(pq, &test_vectors_[i * dims_], code);

        float decoded[128];
        pq_decode(pq, code, decoded);

        /* 计算 MSE */
        for (int j = 0; j < dims_; j++) {
            float diff = test_vectors_[i * dims_ + j] - decoded[j];
            total_error += diff * diff;
        }
    }

    float mse = total_error / (n_samples * dims_);
    float rmse = sqrtf(mse);

    printf("PQ 量化 RMSE: %.4f\n", rmse);

    /* RMSE 应该合理（经验阈值） */
    EXPECT_LT(rmse, 0.3f);

    pq_destroy(pq);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}