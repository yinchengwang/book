/*
 * lvq_test.cpp — LVQ 学习向量量化器单元测试
 */

#include <gtest/gtest.h>
#include <db/index/vector_index/lvq/lvq.h>

#include <cstring>
#include <cstdlib>
#include <cmath>

/* 辅助函数 */
static float *generate_labeled_vectors(int n, int dims, int n_labels, int *labels)
{
    float *vectors = (float *)malloc(n * dims * sizeof(float));

    for (int i = 0; i < n; i++) {
        /* 每个类别的向量聚集在不同的区域 */
        int label = i % n_labels;
        labels[i] = label;

        float center = (float)label * 10.0f;
        for (int d = 0; d < dims; d++) {
            vectors[i * dims + d] = center + ((float)rand() / RAND_MAX - 0.5f) * 2.0f;
        }
    }

    return vectors;
}

/* ============================================================
 * 测试 1: 基本创建和销毁
 * ============================================================ */

TEST(LVQTest, CreateAndDestroy)
{
    lvq_t *lvq = lvq_create(128, 256);
    ASSERT_NE(lvq, nullptr);

    int dims, n_prototypes, code_size;
    lvq_get_info(lvq, &dims, &n_prototypes, &code_size);

    EXPECT_EQ(dims, 128);
    EXPECT_EQ(n_prototypes, 256);
    EXPECT_EQ(code_size, 4);

    lvq_destroy(lvq);
}

TEST(LVQTest, CreateWithInvalidParams)
{
    EXPECT_EQ(lvq_create(0, 256), nullptr);
    EXPECT_EQ(lvq_create(128, 0), nullptr);
}

/* ============================================================
 * 测试 2: 训练
 * ============================================================ */

TEST(LVQTest, Train)
{
    srand(42);

    lvq_t *lvq = lvq_create(16, 64);
    ASSERT_NE(lvq, nullptr);

    int n = 200;
    int *labels = (int *)malloc(n * sizeof(int));
    float *vectors = generate_labeled_vectors(n, 16, 4, labels);

    EXPECT_EQ(lvq_train(lvq, n, vectors, labels), 0);

    free(labels);
    free(vectors);
    lvq_destroy(lvq);
}

TEST(LVQTest, TrainWithDifferentLabels)
{
    srand(42);

    lvq_t *lvq = lvq_create(8, 32);
    ASSERT_NE(lvq, nullptr);

    int n = 100;
    int *labels = (int *)malloc(n * sizeof(int));
    float *vectors = generate_labeled_vectors(n, 8, 8, labels);

    EXPECT_EQ(lvq_train(lvq, n, vectors, labels), 0);

    free(labels);
    free(vectors);
    lvq_destroy(lvq);
}

/* ============================================================
 * 测试 3: 编码
 * ============================================================ */

TEST(LVQTest, Encode)
{
    srand(42);

    lvq_t *lvq = lvq_create(16, 64);
    ASSERT_NE(lvq, nullptr);

    /* 训练 */
    int n = 200;
    int *labels = (int *)malloc(n * sizeof(int));
    float *vectors = generate_labeled_vectors(n, 16, 4, labels);
    EXPECT_EQ(lvq_train(lvq, n, vectors, labels), 0);

    /* 编码 */
    float vector[16];
    for (int d = 0; d < 16; d++) vector[d] = 5.0f;  /* 接近 label=0 */

    int code = lvq_encode(lvq, vector);
    EXPECT_GE(code, 0);
    EXPECT_LT(code, 64);

    /* 验证原型标签 */
    int proto_label = lvq_get_label(lvq, code);
    EXPECT_GE(proto_label, 0);

    free(labels);
    free(vectors);
    lvq_destroy(lvq);
}

TEST(LVQTest, BatchEncode)
{
    srand(42);

    lvq_t *lvq = lvq_create(16, 64);
    ASSERT_NE(lvq, nullptr);

    /* 训练 */
    int n = 200;
    int *labels = (int *)malloc(n * sizeof(int));
    float *vectors = generate_labeled_vectors(n, 16, 4, labels);
    lvq_train(lvq, n, vectors, labels);

    /* 批量编码 */
    int *codes = (int *)malloc(n * sizeof(int));
    EXPECT_EQ(lvq_encode_batch(lvq, n, vectors, codes), 0);

    for (int i = 0; i < n; i++) {
        EXPECT_GE(codes[i], 0);
        EXPECT_LT(codes[i], 64);
    }

    free(labels);
    free(vectors);
    free(codes);
    lvq_destroy(lvq);
}

TEST(LVQTest, EncodeWithoutTrain)
{
    lvq_t *lvq = lvq_create(16, 64);
    ASSERT_NE(lvq, nullptr);

    float vector[16] = {0};
    EXPECT_EQ(lvq_encode(lvq, vector), -1);

    lvq_destroy(lvq);
}

/* ============================================================
 * 测试 4: 获取原型
 * ============================================================ */

TEST(LVQTest, GetPrototype)
{
    srand(42);

    lvq_t *lvq = lvq_create(8, 16);
    ASSERT_NE(lvq, nullptr);

    /* 训练 */
    int n = 100;
    int *labels = (int *)malloc(n * sizeof(int));
    float *vectors = generate_labeled_vectors(n, 8, 4, labels);
    lvq_train(lvq, n, vectors, labels);

    /* 获取原型 */
    float proto[8];
    lvq_get_prototype(lvq, 0, proto);

    for (int d = 0; d < 8; d++) {
        /* 原型向量应该有值 */
        EXPECT_NE(proto[d], 0.0f);
    }

    /* 获取原型标签 */
    int label = lvq_get_label(lvq, 0);
    EXPECT_GE(label, 0);
    EXPECT_LT(label, 4);

    free(labels);
    free(vectors);
    lvq_destroy(lvq);
}

/* ============================================================
 * 测试 5: 持久化
 * ============================================================ */

TEST(LVQTest, DISABLED_SaveAndLoad)
{
    srand(42);

    lvq_t *lvq = lvq_create(16, 32);
    ASSERT_NE(lvq, nullptr);

    /* 训练 */
    int n = 100;
    int *labels = (int *)malloc(n * sizeof(int));
    float *vectors = generate_labeled_vectors(n, 16, 4, labels);
    lvq_train(lvq, n, vectors, labels);

    /* 保存 */
    const char *temp_path = "test_lvq_temp.bin";
    EXPECT_EQ(lvq_save(lvq, temp_path), 0);

    /* 加载 */
    lvq_t *loaded = lvq_load(temp_path);
    ASSERT_NE(loaded, nullptr);

    /* 验证 */
    int dims, n_prototypes, code_size;
    lvq_get_info(loaded, &dims, &n_prototypes, &code_size);
    EXPECT_EQ(dims, 16);
    EXPECT_EQ(n_prototypes, 32);

    /* 编码验证 */
    float vector[16];
    for (int d = 0; d < 16; d++) vector[d] = 5.0f;

    int code_orig = lvq_encode(lvq, vector);
    int code_loaded = lvq_encode(loaded, vector);
    EXPECT_EQ(code_orig, code_loaded);

    free(labels);
    free(vectors);
    lvq_destroy(lvq);
    lvq_destroy(loaded);
    remove(temp_path);
}

TEST(LVQTest, LoadNonExistent)
{
    EXPECT_EQ(lvq_load("non_existent.bin"), nullptr);
}