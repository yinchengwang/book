/*
 * faiss_ivf_compat_test.cpp — Faiss IVF 兼容层单元测试
 */

#include <gtest/gtest.h>
#include <db/index/vector_index/faiss_ivf_compat/faiss_ivf_compat.h>

#include <cstdlib>

static float *gen_vectors(int n, int dims)
{
    float *v = (float *)malloc(n * dims * sizeof(float));
    for (int i = 0; i < n * dims; i++) v[i] = (float)rand() / RAND_MAX;
    return v;
}

TEST(FaissCompatTest, CreateAndDestroy)
{
    faiss_ivf_compat_t *idx = faiss_ivf_compat_create(128, 256);
    ASSERT_NE(idx, nullptr);
    faiss_ivf_compat_destroy(idx);
}

TEST(FaissCompatTest, CreateWithInvalidParams)
{
    EXPECT_EQ(faiss_ivf_compat_create(0, 256), nullptr);
    EXPECT_EQ(faiss_ivf_compat_create(128, 0), nullptr);
}

TEST(FaissCompatTest, Train)
{
    srand(42);
    faiss_ivf_compat_t *idx = faiss_ivf_compat_create(64, 64);
    ASSERT_NE(idx, nullptr);

    float *train = gen_vectors(500, 64);
    EXPECT_EQ(faiss_ivf_compat_train(idx, 500, train), 0);

    free(train);
    faiss_ivf_compat_destroy(idx);
}

TEST(FaissCompatTest, Add)
{
    srand(42);
    faiss_ivf_compat_t *idx = faiss_ivf_compat_create(64, 64);
    ASSERT_NE(idx, nullptr);

    float *train = gen_vectors(200, 64);
    faiss_ivf_compat_train(idx, 200, train);

    float *vecs = gen_vectors(100, 64);
    int *ids = (int *)malloc(100 * sizeof(int));
    for (int i = 0; i < 100; i++) ids[i] = i;

    EXPECT_EQ(faiss_ivf_compat_add(idx, 100, vecs, ids), 100);

    free(train); free(vecs); free(ids);
    faiss_ivf_compat_destroy(idx);
}

TEST(FaissCompatTest, DISABLED_Search)
{
    srand(42);
    faiss_ivf_compat_t *idx = faiss_ivf_compat_create(64, 64);
    ASSERT_NE(idx, nullptr);

    float *train = gen_vectors(200, 64);
    faiss_ivf_compat_train(idx, 200, train);

    float *vecs = gen_vectors(100, 64);
    int *ids = (int *)malloc(100 * sizeof(int));
    for (int i = 0; i < 100; i++) ids[i] = i;
    faiss_ivf_compat_add(idx, 100, vecs, ids);

    float query[64];
    for (int i = 0; i < 64; i++) query[i] = (float)rand() / RAND_MAX;

    int result_ids[10];
    float result_dists[10];
    int count = faiss_ivf_compat_search(idx, query, 10, result_ids, result_dists, 10);

    EXPECT_GE(count, 0);

    free(train); free(vecs); free(ids);
    faiss_ivf_compat_destroy(idx);
}

TEST(FaissCompatTest, DISABLED_SaveAndLoad)
{
    srand(42);
    faiss_ivf_compat_t *idx = faiss_ivf_compat_create(64, 64);
    ASSERT_NE(idx, nullptr);

    float *train = gen_vectors(200, 64);
    faiss_ivf_compat_train(idx, 200, train);

    float *vecs = gen_vectors(50, 64);
    int *ids = (int *)malloc(50 * sizeof(int));
    for (int i = 0; i < 50; i++) ids[i] = i;
    faiss_ivf_compat_add(idx, 50, vecs, ids);

    EXPECT_EQ(faiss_ivf_compat_save(idx, "test_faiss_temp.bin"), 0);

    faiss_ivf_compat_t *loaded = faiss_ivf_compat_load("test_faiss_temp.bin");
    ASSERT_NE(loaded, nullptr);

    free(train); free(vecs); free(ids);
    faiss_ivf_compat_destroy(idx);
    faiss_ivf_compat_destroy(loaded);
    remove("test_faiss_temp.bin");
}

TEST(FaissCompatTest, LoadNonExistent)
{
    EXPECT_EQ(faiss_ivf_compat_load("non_existent.bin"), nullptr);
}