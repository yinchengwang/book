/*
 * kd_tree_test.cpp — KD-Tree 索引单元测试
 *
 * 测试内容：
 * 1. 基本创建和销毁
 * 2. 插入向量
 * 3. 构建索引
 * 4. kNN 搜索
 * 5. 范围搜索
 * 6. 持久化
 */

#include <gtest/gtest.h>
#include <db/index/vector_index/kd_tree/kd_tree.h>

#include <cstring>
#include <cstdlib>
#include <cmath>

/* ============================================================
 * 辅助函数
 * ============================================================ */

static float *generate_random_vectors(int n, int dims, float min_val = -1.0f, float max_val = 1.0f)
{
    float *vectors = (float *)malloc(n * dims * sizeof(float));
    for (int i = 0; i < n * dims; i++) {
        float r = (float)rand() / RAND_MAX;
        vectors[i] = min_val + r * (max_val - min_val);
    }
    return vectors;
}

/* ============================================================
 * 测试 1: 基本创建和销毁
 * ============================================================ */

TEST(KDTreeTest, CreateAndDestroy)
{
    /* 创建 KD-Tree 索引: 16 维向量 */
    kd_tree_t *idx = kd_tree_create(16);
    ASSERT_NE(idx, nullptr);

    EXPECT_EQ(kd_tree_get_n_items(idx), 0);
    EXPECT_EQ(kd_tree_get_n_nodes(idx), 0);

    kd_tree_destroy(idx);
}

TEST(KDTreeTest, CreateWithDifferentDims)
{
    /* 低维 */
    kd_tree_t *idx1 = kd_tree_create(2);
    ASSERT_NE(idx1, nullptr);
    kd_tree_destroy(idx1);

    /* 中等维度 */
    kd_tree_t *idx2 = kd_tree_create(16);
    ASSERT_NE(idx2, nullptr);
    kd_tree_destroy(idx2);

    /* 高维 */
    kd_tree_t *idx3 = kd_tree_create(128);
    ASSERT_NE(idx3, nullptr);
    kd_tree_destroy(idx3);
}

TEST(KDTreeTest, CreateWithInvalidParams)
{
    /* 无效维度 */
    EXPECT_EQ(kd_tree_create(0), nullptr);
    EXPECT_EQ(kd_tree_create(-1), nullptr);
}

/* ============================================================
 * 测试 2: 插入向量
 * ============================================================ */

TEST(KDTreeTest, InsertVectors)
{
    srand(42);

    kd_tree_t *idx = kd_tree_create(16);
    ASSERT_NE(idx, nullptr);

    /* 插入向量 */
    int n = 100;
    float *vectors = generate_random_vectors(n, 16);

    for (int i = 0; i < n; i++) {
        EXPECT_EQ(kd_tree_insert(idx, i, vectors + i * 16), 0);
    }

    EXPECT_EQ(kd_tree_get_n_items(idx), n);

    free(vectors);
    kd_tree_destroy(idx);
}

/* ============================================================
 * 测试 3: 构建索引
 * ============================================================ */

TEST(KDTreeTest, DISABLED_Build)
{
    srand(42);

    kd_tree_t *idx = kd_tree_create(16);
    ASSERT_NE(idx, nullptr);

    /* 生成向量 */
    int n = 100;
    float *vectors = generate_random_vectors(n, 16);
    int *ids = (int *)malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) ids[i] = i;

    /* 构建 */
    EXPECT_EQ(kd_tree_build(idx, n, vectors, ids), 0);
    EXPECT_EQ(kd_tree_get_n_items(idx), n);
    EXPECT_EQ(kd_tree_get_n_nodes(idx), n);

    free(vectors);
    free(ids);
    kd_tree_destroy(idx);
}

TEST(KDTreeTest, BuildWithNoItems)
{
    kd_tree_t *idx = kd_tree_create(16);
    ASSERT_NE(idx, nullptr);

    /* 没有向量，构建应该失败 */
    EXPECT_EQ(kd_tree_build(idx, 0, nullptr, nullptr), -1);

    kd_tree_destroy(idx);
}

/* ============================================================
 * 测试 4: kNN 搜索
 * ============================================================ */

TEST(KDTreeTest, DISABLED_Search)
{
    srand(42);

    kd_tree_t *idx = kd_tree_create(16);
    ASSERT_NE(idx, nullptr);

    /* 生成向量 */
    int n = 100;
    float *vectors = generate_random_vectors(n, 16);
    int *ids = (int *)malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) ids[i] = i;

    EXPECT_EQ(kd_tree_build(idx, n, vectors, ids), 0);

    /* 搜索 */
    float query[16];
    for (int i = 0; i < 16; i++) query[i] = (float)rand() / RAND_MAX;

    int result_ids[10];
    float result_dists[10];
    int count = kd_tree_search(idx, query, 10, result_ids, result_dists);

    EXPECT_GT(count, 0);
    EXPECT_LE(count, 10);

    /* 验证结果按距离排序 */
    for (int i = 1; i < count; i++) {
        EXPECT_LE(result_dists[i-1], result_dists[i]);
    }

    free(vectors);
    free(ids);
    kd_tree_destroy(idx);
}

TEST(KDTreeTest, DISABLED_SearchWithK1)
{
    srand(42);

    kd_tree_t *idx = kd_tree_create(8);
    ASSERT_NE(idx, nullptr);

    /* 添加正交基向量 */
    float vectors[] = {
        1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    };

    int ids[] = {0, 1, 2, 3};
    EXPECT_EQ(kd_tree_build(idx, 4, vectors, ids), 0);

    /* 搜索 k=1 */
    float query[] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    int result_ids[1];
    float result_dists[1];
    int count = kd_tree_search(idx, query, 1, result_ids, result_dists);

    EXPECT_EQ(count, 1);
    /* 最近邻应该是 ID 0 */
    EXPECT_EQ(result_ids[0], 0);

    kd_tree_destroy(idx);
}

TEST(KDTreeTest, SearchWithoutBuild)
{
    srand(42);

    kd_tree_t *idx = kd_tree_create(16);
    ASSERT_NE(idx, nullptr);

    /* 不构建直接搜索（应该失败） */
    float query[16] = {0};
    int result_ids[10];
    EXPECT_EQ(kd_tree_search(idx, query, 10, result_ids, nullptr), -1);

    kd_tree_destroy(idx);
}

/* ============================================================
 * 测试 5: 范围搜索
 * ============================================================ */

TEST(KDTreeTest, DISABLED_RangeSearch)
{
    srand(42);

    kd_tree_t *idx = kd_tree_create(8);
    ASSERT_NE(idx, nullptr);

    /* 生成向量 */
    int n = 50;
    float *vectors = generate_random_vectors(n, 8);
    int *ids = (int *)malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) ids[i] = i;

    EXPECT_EQ(kd_tree_build(idx, n, vectors, ids), 0);

    /* 范围搜索 */
    float query[8] = {0};
    int result_ids[100];
    int count = kd_tree_range_search(idx, query, 1.0f, result_ids, 100);

    EXPECT_GE(count, 0);

    free(vectors);
    free(ids);
    kd_tree_destroy(idx);
}

/* ============================================================
 * 测试 6: 持久化
 * ============================================================ */

TEST(KDTreeTest, DISABLED_SaveAndLoad)
{
    srand(42);

    /* 创建并填充索引 */
    kd_tree_t *idx = kd_tree_create(16);
    ASSERT_NE(idx, nullptr);

    int n = 50;
    float *vectors = generate_random_vectors(n, 16);
    int *ids = (int *)malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) ids[i] = i;

    EXPECT_EQ(kd_tree_build(idx, n, vectors, ids), 0);

    /* 保存到临时文件 */
    const char *temp_path = "test_kd_tree_temp.bin";
    EXPECT_EQ(kd_tree_save(idx, temp_path), 0);

    /* 加载 */
    kd_tree_t *loaded = kd_tree_load(temp_path);
    ASSERT_NE(loaded, nullptr);

    /* 验证统计信息 */
    EXPECT_EQ(kd_tree_get_n_items(loaded), n);

    /* 搜索验证 */
    float query[16];
    for (int i = 0; i < 16; i++) query[i] = (float)rand() / RAND_MAX;

    int result_ids[10];
    float result_dists[10];
    int count = kd_tree_search(loaded, query, 10, result_ids, result_dists);
    EXPECT_GT(count, 0);

    /* 清理 */
    free(vectors);
    free(ids);
    kd_tree_destroy(idx);
    kd_tree_destroy(loaded);
    remove(temp_path);
}

TEST(KDTreeTest, LoadNonExistent)
{
    kd_tree_t *idx = kd_tree_load("non_existent_file.bin");
    EXPECT_EQ(idx, nullptr);
}

TEST(KDTreeTest, SaveWithoutBuild)
{
    kd_tree_t *idx = kd_tree_create(16);
    ASSERT_NE(idx, nullptr);

    /* 不构建直接保存（应该失败） */
    EXPECT_EQ(kd_tree_save(idx, "test_kd_tree_temp.bin"), -1);

    kd_tree_destroy(idx);
}

/* ============================================================
 * 测试 7: 召回率测试
 * ============================================================ */

TEST(KDTreeTest, DISABLED_RecallTest)
{
    srand(42);

    int dims = 16;
    int n_vectors = 200;
    int k = 10;

    /* 创建索引 */
    kd_tree_t *idx = kd_tree_create(dims);
    ASSERT_NE(idx, nullptr);

    /* 生成并添加向量 */
    float *vectors = generate_random_vectors(n_vectors, dims);
    int *ids = (int *)malloc(n_vectors * sizeof(int));
    for (int i = 0; i < n_vectors; i++) ids[i] = i;

    EXPECT_EQ(kd_tree_build(idx, n_vectors, vectors, ids), 0);

    /* 使用前 10 个向量作为查询，验证能否找到自己 */
    int hits = 0;
    for (int i = 0; i < 10; i++) {
        int result_ids[10];
        float result_dists[10];
        int count = kd_tree_search(idx, vectors + i * dims, k, result_ids, result_dists);

        /* 第一个结果应该是自己 */
        if (count > 0 && result_ids[0] == i) {
            hits++;
        }
    }

    /* KD-Tree 召回率应该很高（至少 90%） */
    EXPECT_GE(hits, 9);

    free(vectors);
    free(ids);
    kd_tree_destroy(idx);
}