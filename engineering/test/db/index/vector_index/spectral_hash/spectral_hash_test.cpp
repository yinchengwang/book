/*
 * spectral_hash_test.cpp — Spectral Hash 频谱哈希单元测试
 */

#include <gtest/gtest.h>
#include <db/index/vector_index/spectral_hash/spectral_hash.h>

#include <cstring>
#include <cstdlib>
#include <cmath>

/* 辅助函数 */
static float *generate_random_vectors(int n, int dims)
{
    float *vectors = (float *)malloc(n * dims * sizeof(float));
    for (int i = 0; i < n * dims; i++) {
        vectors[i] = (float)rand() / RAND_MAX * 2.0f - 1.0f;
    }
    return vectors;
}

/* ============================================================
 * 测试 1: 基本创建和销毁
 * ============================================================ */

TEST(SpectralHashTest, CreateAndDestroy)
{
    spectral_hash_t *sh = spectral_hash_create(128, 32);
    ASSERT_NE(sh, nullptr);

    int dims, n_bits;
    spectral_hash_get_info(sh, &dims, &n_bits);

    EXPECT_EQ(dims, 128);
    EXPECT_EQ(n_bits, 32);

    spectral_hash_destroy(sh);
}

TEST(SpectralHashTest, CreateWithInvalidParams)
{
    EXPECT_EQ(spectral_hash_create(0, 32), nullptr);
    EXPECT_EQ(spectral_hash_create(128, 0), nullptr);
    EXPECT_EQ(spectral_hash_create(128, 33), nullptr);  /* n_bits > 32 */
}

/* ============================================================
 * 测试 2: 训练
 * ============================================================ */

TEST(SpectralHashTest, Train)
{
    srand(42);

    spectral_hash_t *sh = spectral_hash_create(64, 16);
    ASSERT_NE(sh, nullptr);

    int n_train = 500;
    float *train_vectors = generate_random_vectors(n_train, 64);

    EXPECT_EQ(spectral_hash_train(sh, n_train, train_vectors), 0);

    free(train_vectors);
    spectral_hash_destroy(sh);
}

TEST(SpectralHashTest, TrainWithSmallDataset)
{
    srand(42);

    spectral_hash_t *sh = spectral_hash_create(8, 4);
    ASSERT_NE(sh, nullptr);

    float vectors[32] = {0};
    EXPECT_EQ(spectral_hash_train(sh, 4, vectors), 0);

    spectral_hash_destroy(sh);
}

/* ============================================================
 * 测试 3: 编码
 * ============================================================ */

TEST(SpectralHashTest, Encode)
{
    srand(42);

    spectral_hash_t *sh = spectral_hash_create(64, 16);
    ASSERT_NE(sh, nullptr);

    /* 训练 */
    int n_train = 500;
    float *train_vectors = generate_random_vectors(n_train, 64);
    EXPECT_EQ(spectral_hash_train(sh, n_train, train_vectors), 0);

    /* 编码 */
    float vector[64];
    for (int i = 0; i < 64; i++) vector[i] = (float)rand() / RAND_MAX;

    uint32_t code = spectral_hash_encode(sh, vector);
    EXPECT_NE(code, 0);

    free(train_vectors);
    spectral_hash_destroy(sh);
}

TEST(SpectralHashTest, BatchEncode)
{
    srand(42);

    spectral_hash_t *sh = spectral_hash_create(64, 16);
    ASSERT_NE(sh, nullptr);

    /* 训练 */
    int n_train = 500;
    float *train_vectors = generate_random_vectors(n_train, 64);
    spectral_hash_train(sh, n_train, train_vectors);

    /* 批量编码 */
    int n = 10;
    float *vectors = generate_random_vectors(n, 64);
    uint32_t *codes = (uint32_t *)malloc(n * sizeof(uint32_t));

    EXPECT_EQ(spectral_hash_encode_batch(sh, n, vectors, codes), 0);

    for (int i = 0; i < n; i++) {
        EXPECT_NE(codes[i], 0);
    }

    free(train_vectors);
    free(vectors);
    free(codes);
    spectral_hash_destroy(sh);
}

TEST(SpectralHashTest, EncodeWithoutTrain)
{
    spectral_hash_t *sh = spectral_hash_create(64, 16);
    ASSERT_NE(sh, nullptr);

    float vector[64] = {0};
    EXPECT_EQ(spectral_hash_encode(sh, vector), 0);

    spectral_hash_destroy(sh);
}

/* ============================================================
 * 测试 4: 汉明距离
 * ============================================================ */

TEST(SpectralHashTest, HammingDistance)
{
    /* 相同码 */
    EXPECT_EQ(spectral_hash_hamming_distance(0xFF, 0xFF, 8), 0);

    /* 完全不同 */
    EXPECT_EQ(spectral_hash_hamming_distance(0x00, 0xFF, 8), 8);

    /* 部分不同 */
    EXPECT_EQ(spectral_hash_hamming_distance(0b10101010, 0b01010101, 8), 8);
    /* 11000000 XOR 00001111 = 11001111 (6 个 1) */
    EXPECT_EQ(spectral_hash_hamming_distance(0b11000000, 0b00001111, 8), 6);
}

/* ============================================================
 * 测试 5: 持久化
 * ============================================================ */

TEST(SpectralHashTest, DISABLED_SaveAndLoad)
{
    srand(42);

    spectral_hash_t *sh = spectral_hash_create(64, 16);
    ASSERT_NE(sh, nullptr);

    /* 训练 */
    int n_train = 500;
    float *train_vectors = generate_random_vectors(n_train, 64);
    spectral_hash_train(sh, n_train, train_vectors);

    /* 保存 */
    const char *temp_path = "test_spectral_hash_temp.bin";
    EXPECT_EQ(spectral_hash_save(sh, temp_path), 0);

    /* 加载 */
    spectral_hash_t *loaded = spectral_hash_load(temp_path);
    ASSERT_NE(loaded, nullptr);

    /* 验证 */
    int dims, n_bits;
    spectral_hash_get_info(loaded, &dims, &n_bits);
    EXPECT_EQ(dims, 64);
    EXPECT_EQ(n_bits, 16);

    /* 编码验证 */
    float vector[64];
    for (int i = 0; i < 64; i++) vector[i] = (float)rand() / RAND_MAX;

    uint32_t code_orig = spectral_hash_encode(sh, vector);
    uint32_t code_loaded = spectral_hash_encode(loaded, vector);
    EXPECT_EQ(code_orig, code_loaded);

    free(train_vectors);
    spectral_hash_destroy(sh);
    spectral_hash_destroy(loaded);
    remove(temp_path);
}

TEST(SpectralHashTest, LoadNonExistent)
{
    EXPECT_EQ(spectral_hash_load("non_existent.bin"), nullptr);
}