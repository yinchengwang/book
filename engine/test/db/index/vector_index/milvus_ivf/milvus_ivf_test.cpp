/*
 * milvus_ivf_test.cpp — Milvus IVF 风格索引单元测试
 */

#include <gtest/gtest.h>
#include <db/index/vector_index/milvus_ivf/milvus_ivf.h>

#include <cstring>
#include <cstdlib>

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

TEST(MilvusIVFTest, CreateAndDestroy)
{
    milvus_ivf_t *idx = milvus_ivf_create(128, 256);
    ASSERT_NE(idx, nullptr);

    int n_vectors, dims, nlist;
    milvus_ivf_stats(idx, &n_vectors, &dims, &nlist);

    EXPECT_EQ(n_vectors, 0);
    EXPECT_EQ(dims, 128);
    EXPECT_EQ(nlist, 256);

    milvus_ivf_destroy(idx);
}

TEST(MilvusIVFTest, CreateWithInvalidParams)
{
    EXPECT_EQ(milvus_ivf_create(0, 256), nullptr);
    EXPECT_EQ(milvus_ivf_create(128, 0), nullptr);
}

/* ============================================================
 * 测试 2: 训练
 * ============================================================ */

TEST(MilvusIVFTest, Train)
{
    srand(42);

    milvus_ivf_t *idx = milvus_ivf_create(64, 64);
    ASSERT_NE(idx, nullptr);

    int n_train = 500;
    float *train_vectors = generate_random_vectors(n_train, 64);

    EXPECT_EQ(milvus_ivf_train(idx, n_train, train_vectors), 0);

    free(train_vectors);
    milvus_ivf_destroy(idx);
}

/* ============================================================
 * 测试 3: 添加
 * ============================================================ */

TEST(MilvusIVFTest, Add)
{
    srand(42);

    milvus_ivf_t *idx = milvus_ivf_create(64, 64);
    ASSERT_NE(idx, nullptr);

    int n_train = 200;
    float *train_vectors = generate_random_vectors(n_train, 64);
    milvus_ivf_train(idx, n_train, train_vectors);

    int n = 100;
    float *vectors = generate_random_vectors(n, 64);
    int *ids = (int *)malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) ids[i] = i;

    int added = milvus_ivf_add(idx, n, vectors, ids);
    EXPECT_EQ(added, n);

    free(train_vectors);
    free(vectors);
    free(ids);
    milvus_ivf_destroy(idx);
}

/* ============================================================
 * 测试 4: 搜索
 * ============================================================ */

TEST(MilvusIVFTest, DISABLED_Search)
{
    srand(42);

    milvus_ivf_t *idx = milvus_ivf_create(64, 64);
    ASSERT_NE(idx, nullptr);

    int n_train = 200;
    float *train_vectors = generate_random_vectors(n_train, 64);
    milvus_ivf_train(idx, n_train, train_vectors);

    int n = 100;
    float *vectors = generate_random_vectors(n, 64);
    int *ids = (int *)malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) ids[i] = i;
    milvus_ivf_add(idx, n, vectors, ids);

    float query[64];
    for (int i = 0; i < 64; i++) query[i] = (float)rand() / RAND_MAX;

    int result_ids[10];
    int count = milvus_ivf_search(idx, query, 10, result_ids, 10);

    EXPECT_GE(count, 0);

    free(train_vectors);
    free(vectors);
    free(ids);
    milvus_ivf_destroy(idx);
}

TEST(MilvusIVFTest, SearchWithoutTrain)
{
    milvus_ivf_t *idx = milvus_ivf_create(64, 64);
    ASSERT_NE(idx, nullptr);

    float query[64] = {0};
    int result_ids[10];

    EXPECT_EQ(milvus_ivf_search(idx, query, 10, result_ids, 10), -1);

    milvus_ivf_destroy(idx);
}

/* ============================================================
 * 测试 5: 持久化
 * ============================================================ */

TEST(MilvusIVFTest, DISABLED_SaveAndLoad)
{
    srand(42);

    milvus_ivf_t *idx = milvus_ivf_create(64, 64);
    ASSERT_NE(idx, nullptr);

    int n_train = 200;
    float *train_vectors = generate_random_vectors(n_train, 64);
    milvus_ivf_train(idx, n_train, train_vectors);

    int n = 50;
    float *vectors = generate_random_vectors(n, 64);
    int *ids = (int *)malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) ids[i] = i;
    milvus_ivf_add(idx, n, vectors, ids);

    const char *temp_path = "test_milvus_ivf_temp.bin";
    EXPECT_EQ(milvus_ivf_save(idx, temp_path), 0);

    milvus_ivf_t *loaded = milvus_ivf_load(temp_path);
    ASSERT_NE(loaded, nullptr);

    int n_vectors;
    milvus_ivf_stats(loaded, &n_vectors, nullptr, nullptr);
    EXPECT_EQ(n_vectors, n);

    free(train_vectors);
    free(vectors);
    free(ids);
    milvus_ivf_destroy(idx);
    milvus_ivf_destroy(loaded);
    remove(temp_path);
}

TEST(MilvusIVFTest, LoadNonExistent)
{
    EXPECT_EQ(milvus_ivf_load("non_existent.bin"), nullptr);
}