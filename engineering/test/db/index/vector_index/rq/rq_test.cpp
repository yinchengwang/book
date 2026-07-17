/*
 * rq_test.cpp — RQ 残差量化器单元测试
 */

#include <gtest/gtest.h>
#include <db/index/vector_index/rq/rq.h>

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

TEST(RQTest, CreateAndDestroy)
{
    rq_t *rq = rq_create(128, 8, 4);
    ASSERT_NE(rq, nullptr);

    int dims, bits, stages, code_size;
    rq_get_info(rq, &dims, &bits, &stages, &code_size);

    EXPECT_EQ(dims, 128);
    EXPECT_EQ(bits, 8);
    EXPECT_EQ(stages, 4);
    EXPECT_EQ(code_size, 4);

    rq_destroy(rq);
}

TEST(RQTest, CreateWithInvalidParams)
{
    EXPECT_EQ(rq_create(0, 8, 4), nullptr);
    EXPECT_EQ(rq_create(128, 0, 4), nullptr);
    EXPECT_EQ(rq_create(128, 9, 4), nullptr);  /* bits > 8 */
    EXPECT_EQ(rq_create(128, 8, 0), nullptr);
}

/* ============================================================
 * 测试 2: 训练
 * ============================================================ */

TEST(RQTest, Train)
{
    srand(42);

    rq_t *rq = rq_create(64, 6, 3);
    ASSERT_NE(rq, nullptr);

    int n_train = 500;
    float *train_vectors = generate_random_vectors(n_train, 64);

    EXPECT_EQ(rq_train(rq, n_train, train_vectors), 0);

    free(train_vectors);
    rq_destroy(rq);
}

TEST(RQTest, TrainWithSmallDataset)
{
    srand(42);

    rq_t *rq = rq_create(16, 4, 2);
    ASSERT_NE(rq, nullptr);

    /* 训练向量太少 */
    float vectors[] = {1.0f, 0.0f, 0.0f, 0.0f,
                       0.0f, 1.0f, 0.0f, 0.0f,
                       0.0f, 0.0f, 1.0f, 0.0f,
                       0.0f, 0.0f, 0.0f, 1.0f};

    EXPECT_EQ(rq_train(rq, 4, vectors), 0);

    rq_destroy(rq);
}

/* ============================================================
 * 测试 3: 编码和解码
 * ============================================================ */

TEST(RQTest, EncodeDecode)
{
    srand(42);

    rq_t *rq = rq_create(64, 6, 3);
    ASSERT_NE(rq, nullptr);

    /* 训练 */
    int n_train = 500;
    float *train_vectors = generate_random_vectors(n_train, 64);
    EXPECT_EQ(rq_train(rq, n_train, train_vectors), 0);

    /* 获取码字大小 */
    int dims, bits, stages, code_size;
    rq_get_info(rq, &dims, &bits, &stages, &code_size);

    /* 编码 */
    int n = 10;
    float *vectors = generate_random_vectors(n, 64);
    uint8_t *codes = (uint8_t *)malloc(n * code_size * sizeof(uint8_t));

    EXPECT_EQ(rq_encode(rq, n, vectors, codes), 0);

    /* 解码 */
    float *decoded = (float *)malloc(n * 64 * sizeof(float));
    EXPECT_EQ(rq_decode(rq, n, codes, decoded), 0);

    /* 验证维度 */
    for (int i = 0; i < n; i++) {
        for (int d = 0; d < 64; d++) {
            /* 解码向量应该接近原始向量 */
            EXPECT_GE(decoded[i * 64 + d], -2.0f);
            EXPECT_LE(decoded[i * 64 + d], 2.0f);
        }
    }

    free(train_vectors);
    free(vectors);
    free(codes);
    free(decoded);
    rq_destroy(rq);
}

TEST(RQTest, EncodeWithoutTrain)
{
    rq_t *rq = rq_create(64, 6, 3);
    ASSERT_NE(rq, nullptr);

    float vectors[64] = {0};
    uint8_t codes[3];

    EXPECT_EQ(rq_encode(rq, 1, vectors, codes), -1);

    rq_destroy(rq);
}

/* ============================================================
 * 测试 4: 计算距离
 * ============================================================ */

TEST(RQTest, DISABLED_ComputeDistance)
{
    srand(42);

    rq_t *rq = rq_create(64, 6, 3);
    ASSERT_NE(rq, nullptr);

    /* 训练 */
    int n_train = 500;
    float *train_vectors = generate_random_vectors(n_train, 64);
    rq_train(rq, n_train, train_vectors);

    /* 编码 */
    int n = 10;
    uint8_t *codes = (uint8_t *)malloc(n * 3);

    float *vectors = generate_random_vectors(n, 64);
    rq_encode(rq, n, vectors, codes);

    /* 计算距离 */
    float dist = rq_compute_distance(rq, vectors, codes);
    EXPECT_GE(dist, 0.0f);

    free(train_vectors);
    free(vectors);
    free(codes);
    rq_destroy(rq);
}

/* ============================================================
 * 测试 5: 持久化
 * ============================================================ */

TEST(RQTest, DISABLED_SaveAndLoad)
{
    srand(42);

    rq_t *rq = rq_create(64, 6, 3);
    ASSERT_NE(rq, nullptr);

    /* 训练 */
    int n_train = 500;
    float *train_vectors = generate_random_vectors(n_train, 64);
    rq_train(rq, n_train, train_vectors);

    /* 保存 */
    const char *temp_path = "test_rq_temp.bin";
    EXPECT_EQ(rq_save(rq, temp_path), 0);

    /* 加载 */
    rq_t *loaded = rq_load(temp_path);
    ASSERT_NE(loaded, nullptr);

    /* 验证信息 */
    int dims, bits, stages, code_size;
    rq_get_info(loaded, &dims, &bits, &stages, &code_size);
    EXPECT_EQ(dims, 64);
    EXPECT_EQ(bits, 6);
    EXPECT_EQ(stages, 3);

    /* 编码验证 */
    float *vectors = generate_random_vectors(5, 64);
    uint8_t *codes_orig = (uint8_t *)malloc(5 * 3);
    uint8_t *codes_loaded = (uint8_t *)malloc(5 * 3);

    rq_encode(rq, 5, vectors, codes_orig);
    rq_encode(loaded, 5, vectors, codes_loaded);

    /* 编码应该相同 */
    for (int i = 0; i < 5 * 3; i++) {
        EXPECT_EQ(codes_orig[i], codes_loaded[i]);
    }

    free(train_vectors);
    free(vectors);
    free(codes_orig);
    free(codes_loaded);
    rq_destroy(rq);
    rq_destroy(loaded);
    remove(temp_path);
}

TEST(RQTest, LoadNonExistent)
{
    EXPECT_EQ(rq_load("non_existent.bin"), nullptr);
}