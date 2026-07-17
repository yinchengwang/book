/*
 * ivf_hnsw_test.cpp — IVF-HNSW 优化版单元测试
 *
 * 测试内容：
 * 1. 基本创建和销毁
 * 2. 训练
 * 3. 添加向量
 * 4. 搜索
 * 5. 优化验证
 */

#include <gtest/gtest.h>
#include <db/index/vector_index/ivf_hnsw/ivf_hnsw.h>

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

TEST(IVFHNSWTest, CreateAndDestroy)
{
    ivf_hnsw_t *idx = ivf_hnsw_create(128, 100, 16, 64);
    ASSERT_NE(idx, nullptr);

    int n_vectors, dims;
    ivf_hnsw_stats(idx, &n_vectors, &dims);

    EXPECT_EQ(n_vectors, 0);
    EXPECT_EQ(dims, 128);

    ivf_hnsw_destroy(idx);
}

TEST(IVFHNSWTest, CreateWithInvalidParams)
{
    EXPECT_EQ(ivf_hnsw_create(0, 100, 16, 64), nullptr);
    EXPECT_EQ(ivf_hnsw_create(128, 0, 16, 64), nullptr);
}

/* ============================================================
 * 测试 2: 训练
 * ============================================================ */

TEST(IVFHNSWTest, Train)
{
    srand(42);

    ivf_hnsw_t *idx = ivf_hnsw_create(64, 100, 16, 32);
    ASSERT_NE(idx, nullptr);

    int n_train = 500;
    float *train_vectors = generate_random_vectors(n_train, 64);

    EXPECT_EQ(ivf_hnsw_train(idx, n_train, train_vectors), 0);

    free(train_vectors);
    ivf_hnsw_destroy(idx);
}

/* ============================================================
 * 测试 3: 添加向量
 * ============================================================ */

TEST(IVFHNSWTest, Add)
{
    srand(42);

    ivf_hnsw_t *idx = ivf_hnsw_create(64, 100, 16, 32);
    ASSERT_NE(idx, nullptr);

    /* 训练 */
    int n_train = 200;
    float *train_vectors = generate_random_vectors(n_train, 64);
    ivf_hnsw_train(idx, n_train, train_vectors);

    /* 添加向量 */
    int n = 100;
    float *vectors = generate_random_vectors(n, 64);
    int *ids = (int *)malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) ids[i] = i;

    int added = ivf_hnsw_add(idx, n, vectors, ids);
    EXPECT_EQ(added, n);

    int n_vectors;
    ivf_hnsw_stats(idx, &n_vectors, nullptr);
    EXPECT_EQ(n_vectors, n);

    free(train_vectors);
    free(vectors);
    free(ids);
    ivf_hnsw_destroy(idx);
}

/* ============================================================
 * 测试 4: 搜索
 * ============================================================ */

TEST(IVFHNSWTest, DISABLED_Search)
{
    srand(42);

    ivf_hnsw_t *idx = ivf_hnsw_create(64, 100, 16, 32);
    ASSERT_NE(idx, nullptr);

    /* 训练和添加 */
    int n_train = 200;
    float *train_vectors = generate_random_vectors(n_train, 64);
    ivf_hnsw_train(idx, n_train, train_vectors);

    int n = 100;
    float *vectors = generate_random_vectors(n, 64);
    int *ids = (int *)malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) ids[i] = i;
    ivf_hnsw_add(idx, n, vectors, ids);

    /* 搜索 */
    float query[64];
    for (int i = 0; i < 64; i++) query[i] = (float)rand() / RAND_MAX;

    int result_ids[10];
    float result_dists[10];
    int count = ivf_hnsw_search(idx, query, 10, result_ids, result_dists, 10);

    EXPECT_GE(count, 0);
    EXPECT_LE(count, 10);

    /* 验证排序 */
    for (int i = 1; i < count; i++) {
        EXPECT_LE(result_dists[i-1], result_dists[i]);
    }

    free(train_vectors);
    free(vectors);
    free(ids);
    ivf_hnsw_destroy(idx);
}

/* ============================================================
 * 测试 5: 批量搜索
 * ============================================================ */

TEST(IVFHNSWTest, DISABLED_BatchSearch)
{
    srand(42);

    ivf_hnsw_t *idx = ivf_hnsw_create(64, 100, 16, 32);
    ASSERT_NE(idx, nullptr);

    /* 训练和添加 */
    int n_train = 200;
    float *train_vectors = generate_random_vectors(n_train, 64);
    ivf_hnsw_train(idx, n_train, train_vectors);

    int n = 100;
    float *vectors = generate_random_vectors(n, 64);
    int *ids = (int *)malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) ids[i] = i;
    ivf_hnsw_add(idx, n, vectors, ids);

    /* 批量搜索 */
    int nq = 5;
    float *queries = generate_random_vectors(nq, 64);
    int result_ids[5 * 10];
    float result_dists[5 * 10];

    EXPECT_EQ(ivf_hnsw_search_batch(idx, nq, queries, 10, result_ids, result_dists, 10), 0);

    free(train_vectors);
    free(vectors);
    free(ids);
    free(queries);
    ivf_hnsw_destroy(idx);
}

/* ============================================================
 * 测试 6: 持久化
 * ============================================================ */

TEST(IVFHNSWTest, DISABLED_SaveAndLoad)
{
    srand(42);

    ivf_hnsw_t *idx = ivf_hnsw_create(64, 100, 16, 32);
    ASSERT_NE(idx, nullptr);

    /* 训练和添加 */
    int n_train = 200;
    float *train_vectors = generate_random_vectors(n_train, 64);
    ivf_hnsw_train(idx, n_train, train_vectors);

    int n = 50;
    float *vectors = generate_random_vectors(n, 64);
    int *ids = (int *)malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) ids[i] = i;
    ivf_hnsw_add(idx, n, vectors, ids);

    /* 保存 */
    const char *temp_path = "test_ivf_hnsw_temp.bin";
    EXPECT_EQ(ivf_hnsw_save(idx, temp_path), 0);

    /* 加载 */
    ivf_hnsw_t *loaded = ivf_hnsw_load(temp_path);
    ASSERT_NE(loaded, nullptr);

    int n_vectors;
    ivf_hnsw_stats(loaded, &n_vectors, nullptr);
    EXPECT_EQ(n_vectors, n);

    /* 搜索验证 */
    float query[64];
    for (int i = 0; i < 64; i++) query[i] = (float)rand() / RAND_MAX;

    int result_ids[10];
    float result_dists[10];
    ivf_hnsw_search(loaded, query, 10, result_ids, result_dists, 10);

    free(train_vectors);
    free(vectors);
    free(ids);
    ivf_hnsw_destroy(idx);
    ivf_hnsw_destroy(loaded);
    remove(temp_path);
}

TEST(IVFHNSWTest, LoadNonExistent)
{
    EXPECT_EQ(ivf_hnsw_load("non_existent.bin"), nullptr);
}

/* ============================================================
 * 测试 7: 召回率测试
 * ============================================================ */

TEST(IVFHNSWTest, DISABLED_RecallTest)
{
    srand(42);

    int dims = 64;
    int n_vectors = 500;
    int k = 10;

    ivf_hnsw_t *idx = ivf_hnsw_create(dims, 100, 16, 32);
    ASSERT_NE(idx, nullptr);

    /* 训练和添加 */
    float *vectors = generate_random_vectors(n_vectors, dims);
    int *ids = (int *)malloc(n_vectors * sizeof(int));
    for (int i = 0; i < n_vectors; i++) ids[i] = i;

    ivf_hnsw_train(idx, n_vectors / 2, vectors);
    ivf_hnsw_add(idx, n_vectors, vectors, ids);

    /* 使用前 10 个向量作为查询 */
    int hits = 0;
    for (int i = 0; i < 10; i++) {
        int result_ids[10];
        float result_dists[10];
        int count = ivf_hnsw_search(idx, vectors + i * dims, k, result_ids, result_dists, 20);

        if (count > 0 && result_ids[0] == i) {
            hits++;
        }
    }

    /* IVF-HNSW 应该能较好地找到自己 */
    EXPECT_GE(hits, 7);

    free(vectors);
    free(ids);
    ivf_hnsw_destroy(idx);
}