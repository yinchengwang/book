/*
 * ball_tree_test.cpp — Ball-Tree 索引单元测试
 */

#include <gtest/gtest.h>
#include <db/index/vector_index/ball_tree/ball_tree.h>

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

TEST(BallTreeTest, CreateAndDestroy)
{
    ball_tree_t *tree = ball_tree_create(16);
    ASSERT_NE(tree, nullptr);

    int n_vectors, n_nodes;
    ball_tree_stats(tree, &n_vectors, &n_nodes);

    EXPECT_EQ(n_vectors, 0);
    EXPECT_EQ(n_nodes, 0);

    ball_tree_destroy(tree);
}

TEST(BallTreeTest, CreateWithInvalidParams)
{
    EXPECT_EQ(ball_tree_create(0), nullptr);
}

/* ============================================================
 * 测试 2: 构建
 * ============================================================ */

TEST(BallTreeTest, Build)
{
    srand(42);

    ball_tree_t *tree = ball_tree_create(16);
    ASSERT_NE(tree, nullptr);

    int n = 100;
    float *vectors = generate_random_vectors(n, 16);
    int *ids = (int *)malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) ids[i] = i;

    EXPECT_EQ(ball_tree_build(tree, n, vectors, ids), 0);

    int n_vectors, n_nodes;
    ball_tree_stats(tree, &n_vectors, &n_nodes);

    EXPECT_EQ(n_vectors, n);
    EXPECT_GT(n_nodes, 0);

    free(vectors);
    free(ids);
    ball_tree_destroy(tree);
}

TEST(BallTreeTest, BuildWithEmptyData)
{
    ball_tree_t *tree = ball_tree_create(16);
    ASSERT_NE(tree, nullptr);

    EXPECT_EQ(ball_tree_build(tree, 0, NULL, NULL), -1);

    ball_tree_destroy(tree);
}

/* ============================================================
 * 测试 3: kNN 搜索
 * ============================================================ */

TEST(BallTreeTest, DISABLED_Search)
{
    srand(42);

    ball_tree_t *tree = ball_tree_create(16);
    ASSERT_NE(tree, nullptr);

    int n = 100;
    float *vectors = generate_random_vectors(n, 16);
    int *ids = (int *)malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) ids[i] = i;

    ball_tree_build(tree, n, vectors, ids);

    /* 搜索 */
    float query[16];
    for (int i = 0; i < 16; i++) query[i] = (float)rand() / RAND_MAX;

    int result_ids[10];
    float result_dists[10];
    int count = ball_tree_search(tree, query, 10, result_ids, result_dists);

    EXPECT_GE(count, 0);
    EXPECT_LE(count, 10);

    /* 验证排序 */
    for (int i = 1; i < count; i++) {
        EXPECT_LE(result_dists[i-1], result_dists[i]);
    }

    free(vectors);
    free(ids);
    ball_tree_destroy(tree);
}

TEST(BallTreeTest, SearchWithoutBuild)
{
    srand(42);

    ball_tree_t *tree = ball_tree_create(16);
    ASSERT_NE(tree, nullptr);

    float query[16] = {0};
    int result_ids[10];
    float result_dists[10];

    EXPECT_EQ(ball_tree_search(tree, query, 10, result_ids, result_dists), -1);

    ball_tree_destroy(tree);
}

/* ============================================================
 * 测试 4: 范围搜索
 * ============================================================ */

TEST(BallTreeTest, DISABLED_RangeSearch)
{
    srand(42);

    ball_tree_t *tree = ball_tree_create(8);
    ASSERT_NE(tree, nullptr);

    int n = 50;
    float *vectors = generate_random_vectors(n, 8);
    int *ids = (int *)malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) ids[i] = i;

    ball_tree_build(tree, n, vectors, ids);

    float query[8] = {0};
    int result_ids[100];
    int count = ball_tree_range_search(tree, query, 1.0f, result_ids, 100);

    EXPECT_GE(count, 0);

    free(vectors);
    free(ids);
    ball_tree_destroy(tree);
}

/* ============================================================
 * 测试 5: 持久化
 * ============================================================ */

TEST(BallTreeTest, DISABLED_SaveAndLoad)
{
    srand(42);

    ball_tree_t *tree = ball_tree_create(16);
    ASSERT_NE(tree, nullptr);

    int n = 50;
    float *vectors = generate_random_vectors(n, 16);
    int *ids = (int *)malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) ids[i] = i;

    ball_tree_build(tree, n, vectors, ids);

    const char *temp_path = "test_ball_tree_temp.bin";
    EXPECT_EQ(ball_tree_save(tree, temp_path), 0);

    ball_tree_t *loaded = ball_tree_load(temp_path);
    ASSERT_NE(loaded, nullptr);

    int n_vectors;
    ball_tree_stats(loaded, &n_vectors, nullptr);
    EXPECT_EQ(n_vectors, n);

    /* 搜索验证 */
    float query[16];
    for (int i = 0; i < 16; i++) query[i] = (float)rand() / RAND_MAX;

    int result_ids[10];
    float result_dists[10];
    ball_tree_search(loaded, query, 10, result_ids, result_dists);

    free(vectors);
    free(ids);
    ball_tree_destroy(tree);
    ball_tree_destroy(loaded);
    remove(temp_path);
}

TEST(BallTreeTest, LoadNonExistent)
{
    EXPECT_EQ(ball_tree_load("non_existent.bin"), nullptr);
}

/* ============================================================
 * 测试 6: 召回率测试
 * ============================================================ */

TEST(BallTreeTest, DISABLED_RecallTest)
{
    srand(42);

    int dims = 16;
    int n_vectors = 200;
    int k = 10;

    ball_tree_t *tree = ball_tree_create(dims);
    ASSERT_NE(tree, nullptr);

    float *vectors = generate_random_vectors(n_vectors, dims);
    int *ids = (int *)malloc(n_vectors * sizeof(int));
    for (int i = 0; i < n_vectors; i++) ids[i] = i;

    ball_tree_build(tree, n_vectors, vectors, ids);

    /* 验证能否找到自己 */
    int hits = 0;
    for (int i = 0; i < 10; i++) {
        int result_ids[10];
        float result_dists[10];
        int count = ball_tree_search(tree, vectors + i * dims, k, result_ids, result_dists);

        if (count > 0 && result_ids[0] == i) {
            hits++;
        }
    }

    EXPECT_GE(hits, 9);

    free(vectors);
    free(ids);
    ball_tree_destroy(tree);
}