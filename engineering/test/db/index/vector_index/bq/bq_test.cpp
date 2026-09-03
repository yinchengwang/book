/**
 * @file bq_test.cpp
 * @brief BQ (Binary Quantization) 单元测试
 */

#include <gtest/gtest.h>
#include "db/index/vector_index/bq/bq.h"
#include <vector>
#include <cstring>
#include <cmath>

/**
 * @brief 生成测试向量数据
 */
static std::vector<float> generate_test_vectors(int n, int dims, float range = 1.0f)
{
    std::vector<float> vectors(n * dims);
    for (int i = 0; i < n * dims; i++) {
        vectors[i] = ((float)rand() / RAND_MAX - 0.5f) * 2.0f * range;
    }
    return vectors;
}

/**
 * @brief 生成均匀分布向量
 */
static std::vector<float> generate_uniform_vectors(int n, int dims, float min, float max)
{
    std::vector<float> vectors(n * dims);
    float range = max - min;
    for (int i = 0; i < n * dims; i++) {
        vectors[i] = min + ((float)rand() / RAND_MAX) * range;
    }
    return vectors;
}

// ============================================================
// 测试：创建和销毁
// ============================================================

TEST(BQTest, CreateAndDestroy)
{
    bq_quantizer_t *bq = bq_create(128, BQ_THRESHOLD_MEAN);
    ASSERT_NE(bq, nullptr);
    EXPECT_EQ(bq->dims, 128);
    EXPECT_EQ(bq->strategy, BQ_THRESHOLD_MEAN);
    EXPECT_EQ(bq->trained, 0);
    bq_destroy(bq);
}

TEST(BQTest, CreateInvalidDims)
{
    EXPECT_EQ(bq_create(0, BQ_THRESHOLD_MEAN), nullptr);
    EXPECT_EQ(bq_create(-1, BQ_THRESHOLD_MEAN), nullptr);
    EXPECT_EQ(bq_create(BQ_MAX_DIMS + 1, BQ_THRESHOLD_MEAN), nullptr);
}

TEST(BQTest, AllStrategies)
{
    BqThresholdStrategy_t strategies[] = {
        BQ_THRESHOLD_MEAN,
        BQ_THRESHOLD_MEDIAN,
        BQ_THRESHOLD_ADAPTIVE,
        BQ_THRESHOLD_LEARNED
    };

    for (auto strategy : strategies) {
        bq_quantizer_t *bq = bq_create(64, strategy);
        ASSERT_NE(bq, nullptr);
        EXPECT_EQ(bq->strategy, strategy);
        bq_destroy(bq);
    }
}

// ============================================================
// 测试：训练
// ============================================================

TEST(BQTest, TrainMean)
{
    const int dims = 128;
    const int n = 1000;
    auto vectors = generate_test_vectors(n, dims);

    bq_quantizer_t *bq = bq_create(dims, BQ_THRESHOLD_MEAN);
    ASSERT_NE(bq, nullptr);

    int ret = bq_train(bq, n, vectors.data());
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(bq->trained, 1);
    EXPECT_EQ(bq->n_samples, n);

    bq_destroy(bq);
}

TEST(BQTest, TrainMedian)
{
    const int dims = 64;
    const int n = 500;
    auto vectors = generate_test_vectors(n, dims);

    bq_quantizer_t *bq = bq_create(dims, BQ_THRESHOLD_MEDIAN);
    ASSERT_NE(bq, nullptr);

    int ret = bq_train(bq, n, vectors.data());
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(bq->trained, 1);

    /* 验证阈值不为零 */
    EXPECT_NE(bq_get_threshold(bq, 0), 0.0f);

    bq_destroy(bq);
}

TEST(BQTest, TrainAdaptive)
{
    const int dims = 32;
    const int n = 200;
    auto vectors = generate_test_vectors(n, dims);

    bq_quantizer_t *bq = bq_create(dims, BQ_THRESHOLD_ADAPTIVE);
    ASSERT_NE(bq, nullptr);

    int ret = bq_train(bq, n, vectors.data());
    EXPECT_EQ(ret, 0);

    /* 验证每维阈值不同（因为是自适应中位数） */
    bool all_same = true;
    float first_th = bq_get_threshold(bq, 0);
    for (int d = 1; d < dims; d++) {
        if (std::abs(bq_get_threshold(bq, d) - first_th) > 1e-6) {
            all_same = false;
            break;
        }
    }
    /* 随机数据下阈值应该大多不同 */
    EXPECT_FALSE(all_same);

    bq_destroy(bq);
}

TEST(BQTest, TrainInvalid)
{
    bq_quantizer_t *bq = bq_create(64, BQ_THRESHOLD_MEAN);
    ASSERT_NE(bq, nullptr);

    /* 空数据 */
    EXPECT_EQ(bq_train(bq, 0, nullptr), -1);
    EXPECT_EQ(bq_train(bq, 10, nullptr), -1);

    bq_destroy(bq);
}

// ============================================================
// 测试：编码
// ============================================================

TEST(BQTest, EncodeDecode)
{
    const int dims = 128;
    const int n = 100;
    auto vectors = generate_uniform_vectors(n, dims, -1.0f, 1.0f);

    bq_quantizer_t *bq = bq_create(dims, BQ_THRESHOLD_MEAN);
    ASSERT_NE(bq, nullptr);

    bq_train(bq, n, vectors.data());

    /* 编码所有向量 */
    int code_size = bq_get_code_size(bq);
    std::vector<uint8_t> codes(n * code_size);

    for (int i = 0; i < n; i++) {
        int ret = bq_encode(bq, &vectors[i * dims], &codes[i * code_size]);
        EXPECT_EQ(ret, 0);
    }

    /* 验证编码不为全零 */
    bool all_zero = true;
    for (int i = 0; i < n * code_size; i++) {
        if (codes[i] != 0) {
            all_zero = false;
            break;
        }
    }
    EXPECT_FALSE(all_zero);

    /* 验证编码大小正确 */
    EXPECT_EQ(code_size, 16);  /* 128 / 8 = 16 */

    bq_destroy(bq);
}

TEST(BQTest, EncodeBatch)
{
    const int dims = 64;
    const int n = 50;
    auto vectors = generate_test_vectors(n, dims);

    bq_quantizer_t *bq = bq_create(dims, BQ_THRESHOLD_MEAN);
    ASSERT_NE(bq, nullptr);

    bq_train(bq, n, vectors.data());

    int code_size = bq_get_code_size(bq);
    std::vector<uint8_t> codes(n * code_size);

    int encoded = bq_encode_batch(bq, n, vectors.data(), codes.data());
    EXPECT_EQ(encoded, n);

    /* 验证编码不为空 */
    int nonzero = 0;
    for (int i = 0; i < n * code_size; i++) {
        if (codes[i] != 0) nonzero++;
    }
    EXPECT_GT(nonzero, 0);

    bq_destroy(bq);
}

TEST(BQTest, EncodeBeforeTrain)
{
    const int dims = 64;
    auto vector = generate_test_vectors(1, dims);
    uint8_t code[8];

    bq_quantizer_t *bq = bq_create(dims, BQ_THRESHOLD_MEAN);
    ASSERT_NE(bq, nullptr);

    EXPECT_EQ(bq_encode(bq, vector.data(), code), -1);

    bq_destroy(bq);
}

// ============================================================
// 测试：Hamming 距离
// ============================================================

TEST(BQTest, HammingDistance)
{
    /* 8 字节: 每个字节都与自身互补，异或后全为 0xFF */
    uint8_t code1[] = {0xFF, 0x00, 0xAA, 0x55, 0x0F, 0xF0, 0xCC, 0x33};
    uint8_t code2[] = {0x00, 0xFF, 0x55, 0xAA, 0xF0, 0x0F, 0x33, 0xCC};

    /* 每个字节异或都是 0xFF = 8 bits */
    int dist = bq_hamming_distance(code1, code2, 8);
    EXPECT_EQ(dist, 64);  /* 8 bytes * 8 bits = 64 */
}

TEST(BQTest, HammingDistanceSame)
{
    uint8_t code1[] = {0xFF, 0xAA, 0x55, 0x00, 0x0F, 0xF0, 0xCC, 0x33};
    uint8_t code2[] = {0xFF, 0xAA, 0x55, 0x00, 0x0F, 0xF0, 0xCC, 0x33};

    int dist = bq_hamming_distance(code1, code2, 8);
    EXPECT_EQ(dist, 0);
}

TEST(BQTest, HammingDistancePartial)
{
    /* 0b10101010 XOR 0b11110000 = 0b01011010，有 4 个 1 */
    uint8_t code1[] = {0xAA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t code2[] = {0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    int dist = bq_hamming_distance(code1, code2, 8);
    EXPECT_EQ(dist, 4);
}

TEST(BQTest, HammingBatch)
{
    const int n = 10;
    const int dims = 64;
    int code_size = BQ_CODE_SIZE(dims);  /* 8 bytes */

    /* 查询编码 */
    uint8_t query[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    /* 随机数据库编码 */
    std::vector<uint8_t> codes(n * code_size);
    for (int i = 0; i < n * code_size; i++) {
        codes[i] = (uint8_t)(rand() & 0xFF);
    }

    /* 第一个设为全零，使其距离最小 */
    memset(codes.data(), 0, code_size);

    std::vector<int> distances(n);
    int best_idx = bq_hamming_batch(query, codes.data(), n, code_size, distances.data());

    EXPECT_EQ(best_idx, 0);
    EXPECT_EQ(distances[0], 0);

    /* 验证距离计算正确 */
    for (int i = 0; i < n; i++) {
        int expected = bq_hamming_distance(query, &codes[i * code_size], code_size);
        EXPECT_EQ(distances[i], expected);
    }
}

TEST(BQTest, DistanceTable)
{
    uint8_t query[] = {0x00, 0xFF, 0xAA};
    int code_size = 3;
    uint16_t table[3 * 256];

    bq_compute_distance_table(query, code_size, table);

    /* 验证 table[byte_idx * 256 + val] = popcount(query[byte_idx] XOR val) */
    EXPECT_EQ(table[0 * 256 + 0x00], 0);  /* 0 XOR 0 = 0 */
    EXPECT_EQ(table[0 * 256 + 0xFF], 8);  /* 0 XOR 255 = 255, popcount = 8 */
    EXPECT_EQ(table[1 * 256 + 0xFF], 0);  /* 255 XOR 255 = 0 */
    EXPECT_EQ(table[1 * 256 + 0x00], 8);  /* 255 XOR 0 = 255, popcount = 8 */

    /* 测试使用 table 计算距离 */
    uint8_t target[] = {0x00, 0x00, 0x00};
    int dist = bq_distance_from_table(table, code_size, target);
    /* dist = popcount(0^0) + popcount(255^0) + popcount(170^0) = 0 + 8 + 4 = 12 */
    EXPECT_EQ(dist, 12);
}

TEST(BQTest, DistanceTableRandom)
{
    /* 测试 table 在不同 code_size 下的正确性 */
    const int n_tests = 10;

    for (int t = 0; t < n_tests; t++) {
        int code_size = (rand() % 8) + 1;  /* 1-8 bytes */

        uint8_t *query = (uint8_t *)malloc(code_size);
        uint8_t *target = (uint8_t *)malloc(code_size);
        uint16_t *table = (uint16_t *)malloc(code_size * 256 * sizeof(uint16_t));

        for (int i = 0; i < code_size; i++) {
            query[i] = (uint8_t)(rand() & 0xFF);
            target[i] = (uint8_t)(rand() & 0xFF);
        }

        bq_compute_distance_table(query, code_size, table);
        int dist_table = bq_distance_from_table(table, code_size, target);
        int dist_direct = bq_hamming_distance(query, target, code_size);

        EXPECT_EQ(dist_table, dist_direct)
            << "code_size=" << code_size
            << ", Table distance: " << dist_table
            << ", Direct distance: " << dist_direct;

        free(query);
        free(target);
        free(table);
    }
}

// ============================================================
// 测试：压缩率
// ============================================================

TEST(BQTest, CompressionRatio)
{
    EXPECT_EQ(bq_compression_ratio(128), 32.0f);
    EXPECT_EQ(bq_compression_ratio(256), 32.0f);
    EXPECT_EQ(bq_compression_ratio(64), 32.0f);
    EXPECT_EQ(bq_compression_ratio(128), 32.0f);

    /* 验证不同维度压缩率相同（都是 float32 -> bit） */
    EXPECT_EQ(bq_compression_ratio(128), bq_compression_ratio(256));
}

TEST(BQTest, CodeSize)
{
    EXPECT_EQ(bq_code_size(128), 16);
    EXPECT_EQ(bq_code_size(64), 8);
    EXPECT_EQ(bq_code_size(128), 16);
    EXPECT_EQ(bq_code_size(100), 13);  /* ceil(100/8) = 13 */
    EXPECT_EQ(bq_code_size(1), 1);
}

// ============================================================
// 测试：学习阈值
// ============================================================

TEST(BQTest, LearnedThresholds)
{
    const int dims = 64;
    const int n = 1000;
    auto vectors = generate_test_vectors(n, dims);

    bq_learned_thresholds_t *lt = bq_learn_thresholds(dims, n, vectors.data(), 10);
    ASSERT_NE(lt, nullptr);
    EXPECT_EQ(lt->dims, dims);
    EXPECT_EQ(lt->trained, 1);

    /* 验证阈值在合理范围内 */
    for (int d = 0; d < dims; d++) {
        float th = lt->thresholds[d];
        EXPECT_GE(th, -2.0f);
        EXPECT_LE(th, 2.0f);
    }

    bq_learned_thresholds_destroy(lt);
}

// ============================================================
// 测试：一致性
// ============================================================

TEST(BQTest, EncodeConsistency)
{
    const int dims = 128;
    const int n = 100;
    auto vectors = generate_test_vectors(n, dims);

    bq_quantizer_t *bq = bq_create(dims, BQ_THRESHOLD_MEAN);
    bq_train(bq, n, vectors.data());

    int code_size = bq_get_code_size(bq);
    std::vector<uint8_t> codes1(n * code_size);
    std::vector<uint8_t> codes2(n * code_size);

    /* 两次编码应该一致 */
    for (int i = 0; i < n; i++) {
        bq_encode(bq, &vectors[i * dims], &codes1[i * code_size]);
        bq_encode(bq, &vectors[i * dims], &codes2[i * code_size]);
    }

    for (int i = 0; i < n * code_size; i++) {
        EXPECT_EQ(codes1[i], codes2[i]);
    }

    bq_destroy(bq);
}

TEST(BQTest, SameVectorSameCode)
{
    const int dims = 64;
    auto vector = generate_uniform_vectors(1, dims, -1.0f, 1.0f);

    bq_quantizer_t *bq = bq_create(dims, BQ_THRESHOLD_MEAN);
    bq_train(bq, 1, vector.data());

    uint8_t code1[8], code2[8];
    bq_encode(bq, vector.data(), code1);
    bq_encode(bq, vector.data(), code2);

    for (int i = 0; i < 8; i++) {
        EXPECT_EQ(code1[i], code2[i]);
    }

    bq_destroy(bq);
}

TEST(BQTest, DifferentVectorsDifferentCodes)
{
    const int dims = 128;
    const int n = 100;
    auto vectors = generate_test_vectors(n, dims);

    bq_quantizer_t *bq = bq_create(dims, BQ_THRESHOLD_MEAN);
    bq_train(bq, n, vectors.data());

    int code_size = bq_get_code_size(bq);

    /* 统计唯一编码数量 */
    std::vector<uint8_t> codes(n * code_size);
    for (int i = 0; i < n; i++) {
        bq_encode(bq, &vectors[i * dims], &codes[i * code_size]);
    }

    int unique = 0;
    for (int i = 0; i < n; i++) {
        bool is_unique = true;
        for (int j = 0; j < i; j++) {
            if (memcmp(&codes[i * code_size], &codes[j * code_size], code_size) == 0) {
                is_unique = false;
                break;
            }
        }
        if (is_unique) unique++;
    }

    /* 随机向量应该大多有不同的编码 */
    EXPECT_GT(unique, n * 0.5);

    bq_destroy(bq);
}

// ============================================================
// 测试：打印信息
// ============================================================

TEST(BQTest, PrintInfo)
{
    const int dims = 64;
    const int n = 100;
    auto vectors = generate_test_vectors(n, dims);

    bq_quantizer_t *bq = bq_create(dims, BQ_THRESHOLD_MEAN);
    bq_train(bq, n, vectors.data());

    /* 不崩溃即可 */
    testing::internal::CaptureStdout();
    bq_print_info(bq);
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(output.find("BQ Quantizer Info") != std::string::npos);
    EXPECT_TRUE(output.find("Dims: 64") != std::string::npos);
    EXPECT_TRUE(output.find("Compression ratio: 32.00x") != std::string::npos);

    bq_destroy(bq);
}

// ============================================================
// 测试：边界条件
// ============================================================

TEST(BQTest, EmptyVectors)
{
    const int dims = 64;
    bq_quantizer_t *bq = bq_create(dims, BQ_THRESHOLD_MEAN);
    ASSERT_NE(bq, nullptr);

    /* 空训练集 */
    std::vector<float> empty_vectors;
    EXPECT_EQ(bq_train(bq, 0, empty_vectors.data()), -1);

    bq_destroy(bq);
}

TEST(BQTest, SingleVector)
{
    const int dims = 64;
    const int n = 1;
    auto vectors = generate_test_vectors(n, dims);

    bq_quantizer_t *bq = bq_create(dims, BQ_THRESHOLD_MEAN);
    ASSERT_NE(bq, nullptr);

    EXPECT_EQ(bq_train(bq, n, vectors.data()), 0);

    uint8_t code[8];
    EXPECT_EQ(bq_encode(bq, vectors.data(), code), 0);

    bq_destroy(bq);
}

TEST(BQTest, LargeDims)
{
    const int dims = BQ_MAX_DIMS;
    const int n = 10;
    auto vectors = generate_test_vectors(n, dims);

    bq_quantizer_t *bq = bq_create(dims, BQ_THRESHOLD_MEAN);
    ASSERT_NE(bq, nullptr);

    EXPECT_EQ(bq_train(bq, n, vectors.data()), 0);

    std::vector<float> single_vector(dims);
    for (int i = 0; i < dims; i++) {
        single_vector[i] = vectors[i];
    }

    std::vector<uint8_t> code(BQ_CODE_SIZE(dims));
    EXPECT_EQ(bq_encode(bq, single_vector.data(), code.data()), 0);

    bq_destroy(bq);
}

// ============================================================
// 主函数
// ============================================================

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
