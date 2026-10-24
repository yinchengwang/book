/*
 * hnsw_sq_test.cpp — HNSW-SQ 混合索引单元测试
 */

#include <gtest/gtest.h>
#include <db/index/vector_index/hnsw_sq/hnsw_sq.h>

#include <cstring>
#include <cstdlib>
#include <cmath>

/* 辅助函数 */
static float *generate_random_vectors(int n, int dims)
{
    float *vectors = (float *)malloc(n * dims * sizeof(float));
    for (int i = 0; i < n * dims; i++) {
        vectors[i] = (float)rand() / RAND_MAX;
    }
    return vectors;
}

/* ============================================================
 * 测试 1: 基本创建和销毁
 * ============================================================ */

TEST(HNSWSQTest, CreateAndDestroy)
{
    hnsw_sq_t *idx = hnsw_sq_create(128, 16, 64);
    ASSERT_NE(idx, nullptr);

    int n_vectors, dims;
    hnsw_sq_stats(idx, &n_vectors, &dims);

    EXPECT_EQ(n_vectors, 0);
    EXPECT_EQ(dims, 128);

    hnsw_sq_destroy(idx);
}

TEST(HNSWSQTest, CreateWithInvalidParams)
{
    EXPECT_EQ(hnsw_sq_create(0, 16, 64), nullptr);
    EXPECT_EQ(hnsw_sq_create(128, 0, 64), nullptr);
}

/* ============================================================
 * 测试 2: 训练和添加
 * ============================================================ */

TEST(HNSWSQTest, TrainAndAdd)
{
    srand(42);

    hnsw_sq_t *idx = hnsw_sq_create(64, 16, 32);
    ASSERT_NE(idx, nullptr);

    /* 训练 */
    int n_train = 200;
    float *train_vectors = generate_random_vectors(n_train, 64);
    EXPECT_EQ(hnsw_sq_train(idx, n_train, train_vectors), 0);

    /* 添加向量 */
    int n = 100;
    float *vectors = generate_random_vectors(n, 64);
    int *ids = (int *)malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) ids[i] = i;

    int added = hnsw_sq_add(idx, n, vectors, ids);
    EXPECT_EQ(added, n);

    int n_vectors;
    hnsw_sq_stats(idx, &n_vectors, nullptr);
    EXPECT_EQ(n_vectors, n);

    free(train_vectors);
    free(vectors);
    free(ids);
    hnsw_sq_destroy(idx);
}

/* ============================================================
 * 测试 3: kNN 搜索
 * ============================================================ */

TEST(HNSWSQTest, DISABLED_Search)
{
    srand(42);

    hnsw_sq_t *idx = hnsw_sq_create(64, 16, 32);
    ASSERT_NE(idx, nullptr);

    /* 训练和添加 */
    int n = 100;
    float *vectors = generate_random_vectors(n, 64);
    int *ids = (int *)malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) ids[i] = i;

    hnsw_sq_train(idx, n / 2, vectors);
    hnsw_sq_add(idx, n, vectors, ids);

    /* 搜索 */
    float query[64];
    for (int i = 0; i < 64; i++) query[i] = (float)rand() / RAND_MAX;

    int result_ids[10];
    float result_dists[10];
    int count = hnsw_sq_search(idx, query, 10, result_ids, result_dists);

    EXPECT_GE(count, 0);
    EXPECT_LE(count, 10);

    free(vectors);
    free(ids);
    hnsw_sq_destroy(idx);
}

TEST(HNSWSQTest, SearchWithoutData)
{
    srand(42);

    hnsw_sq_t *idx = hnsw_sq_create(64, 16, 32);
    ASSERT_NE(idx, nullptr);

    /* 不训练不添加 */
    float train_vectors[64] = {0};
    hnsw_sq_train(idx, 1, train_vectors);

    float query[64] = {0};
    int result_ids[10];

    EXPECT_EQ(hnsw_sq_search(idx, query, 10, result_ids, nullptr), 0);

    hnsw_sq_destroy(idx);
}

/* ============================================================
 * 测试 4: 持久化
 * ============================================================ */

TEST(HNSWSQTest, DISABLED_SaveAndLoad)
{
    srand(42);

    hnsw_sq_t *idx = hnsw_sq_create(64, 16, 32);
    ASSERT_NE(idx, nullptr);

    int n = 50;
    float *vectors = generate_random_vectors(n, 64);
    int *ids = (int *)malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) ids[i] = i;

    hnsw_sq_train(idx, n / 2, vectors);
    hnsw_sq_add(idx, n, vectors, ids);

    const char *temp_path = "test_hnsw_sq_temp.bin";
    EXPECT_EQ(hnsw_sq_save(idx, temp_path), 0);

    hnsw_sq_t *loaded = hnsw_sq_load(temp_path);
    ASSERT_NE(loaded, nullptr);

    int n_vectors;
    hnsw_sq_stats(loaded, &n_vectors, nullptr);
    EXPECT_EQ(n_vectors, n);

    free(vectors);
    free(ids);
    hnsw_sq_destroy(idx);
    hnsw_sq_destroy(loaded);
    remove(temp_path);
}

TEST(HNSWSQTest, LoadNonExistent)
{
    EXPECT_EQ(hnsw_sq_load("non_existent.bin"), nullptr);
}