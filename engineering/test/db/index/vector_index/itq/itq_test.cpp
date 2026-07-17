/*
 * itq_test.cpp — ITQ 迭代量化单元测试
 */

#include <gtest/gtest.h>
#include <db/index/vector_index/itq/itq.h>

#include <cstring>
#include <cstdlib>

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

TEST(ITQTest, CreateAndDestroy)
{
    itq_t *itq = itq_create(128, 32);
    ASSERT_NE(itq, nullptr);

    int dims, n_bits;
    itq_get_info(itq, &dims, &n_bits);

    EXPECT_EQ(dims, 128);
    EXPECT_EQ(n_bits, 32);

    itq_destroy(itq);
}

TEST(ITQTest, CreateWithInvalidParams)
{
    EXPECT_EQ(itq_create(0, 32), nullptr);
    EXPECT_EQ(itq_create(128, 0), nullptr);
    EXPECT_EQ(itq_create(128, 33), nullptr);
}

/* ============================================================
 * 测试 2: 训练
 * ============================================================ */

TEST(ITQTest, Train)
{
    srand(42);

    itq_t *itq = itq_create(64, 16);
    ASSERT_NE(itq, nullptr);

    int n_train = 500;
    float *train_vectors = generate_random_vectors(n_train, 64);

    EXPECT_EQ(itq_train(itq, n_train, train_vectors), 0);

    free(train_vectors);
    itq_destroy(itq);
}

/* ============================================================
 * 测试 3: 编码
 * ============================================================ */

TEST(ITQTest, Encode)
{
    srand(42);

    itq_t *itq = itq_create(64, 16);
    ASSERT_NE(itq, nullptr);

    int n_train = 500;
    float *train_vectors = generate_random_vectors(n_train, 64);
    itq_train(itq, n_train, train_vectors);

    float vector[64];
    for (int i = 0; i < 64; i++) vector[i] = (float)rand() / RAND_MAX;

    uint32_t code = itq_encode(itq, vector);
    EXPECT_NE(code, 0);

    free(train_vectors);
    itq_destroy(itq);
}

TEST(ITQTest, BatchEncode)
{
    srand(42);

    itq_t *itq = itq_create(64, 16);
    ASSERT_NE(itq, nullptr);

    int n_train = 500;
    float *train_vectors = generate_random_vectors(n_train, 64);
    itq_train(itq, n_train, train_vectors);

    int n = 10;
    float *vectors = generate_random_vectors(n, 64);
    uint32_t *codes = (uint32_t *)malloc(n * sizeof(uint32_t));

    EXPECT_EQ(itq_encode_batch(itq, n, vectors, codes), 0);

    for (int i = 0; i < n; i++) {
        EXPECT_NE(codes[i], 0);
    }

    free(train_vectors);
    free(vectors);
    free(codes);
    itq_destroy(itq);
}

TEST(ITQTest, EncodeWithoutTrain)
{
    itq_t *itq = itq_create(64, 16);
    ASSERT_NE(itq, nullptr);

    float vector[64] = {0};
    EXPECT_EQ(itq_encode(itq, vector), 0);

    itq_destroy(itq);
}

/* ============================================================
 * 测试 4: 汉明距离
 * ============================================================ */

TEST(ITQTest, HammingDistance)
{
    EXPECT_EQ(itq_hamming_distance(0xFF, 0xFF), 0);
    EXPECT_EQ(itq_hamming_distance(0x00, 0xFF), 8);
    EXPECT_EQ(itq_hamming_distance(0b10101010, 0b01010101), 8);
}

/* ============================================================
 * 测试 5: 持久化
 * ============================================================ */

TEST(ITQTest, DISABLED_SaveAndLoad)
{
    srand(42);

    itq_t *itq = itq_create(64, 16);
    ASSERT_NE(itq, nullptr);

    int n_train = 500;
    float *train_vectors = generate_random_vectors(n_train, 64);
    itq_train(itq, n_train, train_vectors);

    const char *temp_path = "test_itq_temp.bin";
    EXPECT_EQ(itq_save(itq, temp_path), 0);

    itq_t *loaded = itq_load(temp_path);
    ASSERT_NE(loaded, nullptr);

    float vector[64];
    for (int i = 0; i < 64; i++) vector[i] = (float)rand() / RAND_MAX;

    uint32_t code_orig = itq_encode(itq, vector);
    uint32_t code_loaded = itq_encode(loaded, vector);
    EXPECT_EQ(code_orig, code_loaded);

    free(train_vectors);
    itq_destroy(itq);
    itq_destroy(loaded);
    remove(temp_path);
}

TEST(ITQTest, LoadNonExistent)
{
    EXPECT_EQ(itq_load("non_existent.bin"), nullptr);
}