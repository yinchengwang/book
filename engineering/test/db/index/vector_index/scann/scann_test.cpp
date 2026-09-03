/*
 * scann_test.cpp — ScaNN 索引单元测试
 */

#include <gtest/gtest.h>
#include <db/index/vector_index/scann/scann.h>

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

TEST(ScannTest, CreateAndDestroy)
{
    scann_t *idx = scann_create(128, 10);
    ASSERT_NE(idx, nullptr);

    int n_vectors, n_neighbors, dims;
    scann_stats(idx, &n_vectors, &n_neighbors, &dims);

    EXPECT_EQ(n_vectors, 0);
    EXPECT_EQ(n_neighbors, 10);
    EXPECT_EQ(dims, 128);

    scann_destroy(idx);
}

TEST(ScannTest, CreateWithInvalidParams)
{
    EXPECT_EQ(scann_create(0, 10), nullptr);
    EXPECT_EQ(scann_create(128, 0), nullptr);
}

/* ============================================================
 * 测试 2: 训练和添加
 * ============================================================ */

TEST(ScannTest, TrainAndAdd)
{
    srand(42);

    scann_t *idx = scann_create(64, 10);
    ASSERT_NE(idx, nullptr);

    /* 训练 */
    int n_train = 100;
    float *train_vectors = generate_random_vectors(n_train, 64);
    EXPECT_EQ(scann_train(idx, n_train, train_vectors), 0);

    /* 添加向量 */
    int n = 50;
    float *vectors = generate_random_vectors(n, 64);
    int *ids = (int *)malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) ids[i] = i;

    int added = scann_add(idx, n, vectors, ids);
    EXPECT_EQ(added, n);

    int n_vectors;
    scann_stats(idx, &n_vectors, nullptr, nullptr);
    EXPECT_EQ(n_vectors, n);

    free(train_vectors);
    free(vectors);
    free(ids);
    scann_destroy(idx);
}

/* ============================================================
 * 测试 3: kNN 搜索
 * ============================================================ */

TEST(ScannTest, DISABLED_Search)
{
    srand(42);

    scann_t *idx = scann_create(64, 10);
    ASSERT_NE(idx, nullptr);

    /* 添加向量 */
    int n = 100;
    float *vectors = generate_random_vectors(n, 64);
    int *ids = (int *)malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) ids[i] = i;

    scann_add(idx, n, vectors, ids);

    /* 搜索 */
    float query[64];
    for (int i = 0; i < 64; i++) query[i] = (float)rand() / RAND_MAX;

    int result_ids[10];
    float result_dists[10];
    int count = scann_search(idx, query, 10, result_ids, result_dists);

    EXPECT_GE(count, 0);
    EXPECT_LE(count, 10);

    free(vectors);
    free(ids);
    scann_destroy(idx);
}

TEST(ScannTest, SearchWithoutData)
{
    srand(42);

    scann_t *idx = scann_create(64, 10);
    ASSERT_NE(idx, nullptr);

    float query[64] = {0};
    int result_ids[10];

    EXPECT_EQ(scann_search(idx, query, 10, result_ids, nullptr), 0);

    scann_destroy(idx);
}

/* ============================================================
 * 测试 4: 批量搜索
 * ============================================================ */

TEST(ScannTest, DISABLED_BatchSearch)
{
    srand(42);

    scann_t *idx = scann_create(64, 10);
    ASSERT_NE(idx, nullptr);

    int n = 100;
    float *vectors = generate_random_vectors(n, 64);
    int *ids = (int *)malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) ids[i] = i;

    scann_add(idx, n, vectors, ids);

    int nq = 5;
    float *queries = generate_random_vectors(nq, 64);
    int result_ids[5 * 10];
    float result_dists[5 * 10];

    EXPECT_EQ(scann_search_batch(idx, nq, queries, 10, result_ids, result_dists), 0);

    free(vectors);
    free(ids);
    free(queries);
    scann_destroy(idx);
}

/* ============================================================
 * 测试 5: 持久化
 * ============================================================ */

TEST(ScannTest, DISABLED_SaveAndLoad)
{
    srand(42);

    scann_t *idx = scann_create(64, 10);
    ASSERT_NE(idx, nullptr);

    int n = 50;
    float *vectors = generate_random_vectors(n, 64);
    int *ids = (int *)malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) ids[i] = i;

    scann_add(idx, n, vectors, ids);

    const char *temp_path = "test_scann_temp.bin";
    EXPECT_EQ(scann_save(idx, temp_path), 0);

    scann_t *loaded = scann_load(temp_path);
    ASSERT_NE(loaded, nullptr);

    int n_vectors;
    scann_stats(loaded, &n_vectors, nullptr, nullptr);
    EXPECT_EQ(n_vectors, n);

    free(vectors);
    free(ids);
    scann_destroy(idx);
    scann_destroy(loaded);
    remove(temp_path);
}

TEST(ScannTest, LoadNonExistent)
{
    EXPECT_EQ(scann_load("non_existent.bin"), nullptr);
}