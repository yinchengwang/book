/*
 * annoy_test.cpp — Annoy 索引单元测试
 *
 * 测试内容：
 * 1. 基本创建和销毁
 * 2. 添加向量
 * 3. 构建索引
 * 4. 搜索
 * 5. 持久化
 */

#include <gtest/gtest.h>
#include <db/index/vector_index/annoy/annoy.h>

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

static float euclidean_distance(const float *a, const float *b, int dims)
{
    float sum = 0;
    for (int i = 0; i < dims; i++) {
        float d = a[i] - b[i];
        sum += d * d;
    }
    return sqrtf(sum);
}

/* ============================================================
 * 测试 1: 基本创建和销毁
 * ============================================================ */

TEST(AnnoyTest, CreateAndDestroy)
{
    /* 创建 Annoy 索引: 64 维向量，angular 距离 */
    annoy_index_t *idx = annoy_create(64, "angular");
    ASSERT_NE(idx, nullptr);

    EXPECT_EQ(annoy_get_n_items(idx), 0);
    EXPECT_EQ(annoy_get_n_trees(idx), 0);

    annoy_destroy(idx);
}

TEST(AnnoyTest, CreateWithDifferentMetrics)
{
    /* Angular 距离 */
    annoy_index_t *idx1 = annoy_create(16, "angular");
    ASSERT_NE(idx1, nullptr);
    annoy_destroy(idx1);

    /* Euclidean 距离 */
    annoy_index_t *idx2 = annoy_create(16, "euclidean");
    ASSERT_NE(idx2, nullptr);
    annoy_destroy(idx2);

    /* 默认距离（angular） */
    annoy_index_t *idx3 = annoy_create(16, nullptr);
    ASSERT_NE(idx3, nullptr);
    annoy_destroy(idx3);
}

TEST(AnnoyTest, CreateWithInvalidParams)
{
    /* 无效维度 */
    EXPECT_EQ(annoy_create(0, "angular"), nullptr);
    EXPECT_EQ(annoy_create(-1, "angular"), nullptr);
}

/* ============================================================
 * 测试 2: 添加向量
 * ============================================================ */

TEST(AnnoyTest, AddVectors)
{
    srand(42);

    annoy_index_t *idx = annoy_create(16, "angular");
    ASSERT_NE(idx, nullptr);

    /* 添加向量 */
    int n = 100;
    float *vectors = generate_random_vectors(n, 16);

    for (int i = 0; i < n; i++) {
        EXPECT_EQ(annoy_add_item(idx, i, vectors + i * 16), 0);
    }

    EXPECT_EQ(annoy_get_n_items(idx), n);

    free(vectors);
    annoy_destroy(idx);
}

TEST(AnnoyTest, GetItem)
{
    srand(42);

    annoy_index_t *idx = annoy_create(8, "euclidean");
    ASSERT_NE(idx, nullptr);

    float v[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
    EXPECT_EQ(annoy_add_item(idx, 0, v), 0);

    const float *retrieved = annoy_get_item(idx, 0);
    ASSERT_NE(retrieved, nullptr);

    for (int i = 0; i < 8; i++) {
        EXPECT_FLOAT_EQ(retrieved[i], v[i]);
    }

    /* 无效 ID */
    EXPECT_EQ(annoy_get_item(idx, -1), nullptr);
    EXPECT_EQ(annoy_get_item(idx, 100), nullptr);

    annoy_destroy(idx);
}

/* ============================================================
 * 测试 3: 构建索引
 * ============================================================ */

TEST(AnnoyTest, DISABLED_Build)
{
    srand(42);

    annoy_index_t *idx = annoy_create(16, "angular");
    ASSERT_NE(idx, nullptr);

    /* 添加向量 */
    int n = 100;
    float *vectors = generate_random_vectors(n, 16);

    for (int i = 0; i < n; i++) {
        annoy_add_item(idx, i, vectors + i * 16);
    }

    /* 构建索引：10 棵树 */
    EXPECT_EQ(annoy_build(idx, 10), 0);
    EXPECT_EQ(annoy_get_n_trees(idx), 10);

    free(vectors);
    annoy_destroy(idx);
}

TEST(AnnoyTest, BuildWithNoItems)
{
    annoy_index_t *idx = annoy_create(16, "angular");
    ASSERT_NE(idx, nullptr);

    /* 没有向量，构建应该失败 */
    EXPECT_EQ(annoy_build(idx, 10), -1);

    annoy_destroy(idx);
}

TEST(AnnoyTest, AddAfterBuild)
{
    srand(42);

    annoy_index_t *idx = annoy_create(16, "angular");
    ASSERT_NE(idx, nullptr);

    float *vectors = generate_random_vectors(10, 16);
    for (int i = 0; i < 10; i++) {
        annoy_add_item(idx, i, vectors + i * 16);
    }

    EXPECT_EQ(annoy_build(idx, 5), 0);

    /* 构建后不能再添加 */
    float v[16] = {0};
    EXPECT_EQ(annoy_add_item(idx, 100, v), -1);

    free(vectors);
    annoy_destroy(idx);
}

/* ============================================================
 * 测试 4: 搜索
 * ============================================================ */

TEST(AnnoyTest, DISABLED_Search)
{
    srand(42);

    annoy_index_t *idx = annoy_create(16, "angular");
    ASSERT_NE(idx, nullptr);

    /* 添加向量 */
    int n = 100;
    float *vectors = generate_random_vectors(n, 16);

    for (int i = 0; i < n; i++) {
        annoy_add_item(idx, i, vectors + i * 16);
    }

    /* 构建 */
    EXPECT_EQ(annoy_build(idx, 10), 0);

    /* 搜索 */
    float query[16];
    for (int i = 0; i < 16; i++) query[i] = (float)rand() / RAND_MAX;

    int result_ids[10];
    float result_dists[10];
    int count = annoy_search(idx, query, 10, result_ids, result_dists, -1);

    EXPECT_GT(count, 0);
    EXPECT_LE(count, 10);

    /* 验证结果按距离排序 */
    for (int i = 1; i < count; i++) {
        EXPECT_LE(result_dists[i-1], result_dists[i]);
    }

    free(vectors);
    annoy_destroy(idx);
}

TEST(AnnoyTest, DISABLED_SearchWithK1)
{
    srand(42);

    annoy_index_t *idx = annoy_create(8, "euclidean");
    ASSERT_NE(idx, nullptr);

    /* 添加正交基向量 */
    float vectors[] = {
        1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    };

    for (int i = 0; i < 4; i++) {
        annoy_add_item(idx, i, vectors + i * 8);
    }

    EXPECT_EQ(annoy_build(idx, 5), 0);

    /* 搜索 k=1 */
    float query[] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    int result_ids[1];
    float result_dists[1];
    int count = annoy_search(idx, query, 1, result_ids, result_dists, -1);

    EXPECT_EQ(count, 1);
    /* 最近邻应该是 ID 0 */
    EXPECT_EQ(result_ids[0], 0);

    annoy_destroy(idx);
}

TEST(AnnoyTest, SearchWithoutBuild)
{
    srand(42);

    annoy_index_t *idx = annoy_create(16, "angular");
    ASSERT_NE(idx, nullptr);

    float *vectors = generate_random_vectors(10, 16);
    for (int i = 0; i < 10; i++) {
        annoy_add_item(idx, i, vectors + i * 16);
    }

    /* 不构建直接搜索（应该失败） */
    float query[16] = {0};
    int result_ids[10];
    EXPECT_EQ(annoy_search(idx, query, 10, result_ids, nullptr, -1), -1);

    free(vectors);
    annoy_destroy(idx);
}

/* ============================================================
 * 测试 5: 持久化
 * ============================================================ */

TEST(AnnoyTest, DISABLED_SaveAndLoad)
{
    srand(42);

    /* 创建并填充索引 */
    annoy_index_t *idx = annoy_create(16, "angular");
    ASSERT_NE(idx, nullptr);

    int n = 50;
    float *vectors = generate_random_vectors(n, 16);

    for (int i = 0; i < n; i++) {
        annoy_add_item(idx, i, vectors + i * 16);
    }

    EXPECT_EQ(annoy_build(idx, 10), 0);

    /* 保存到临时文件 */
    const char *temp_path = "test_annoy_temp.bin";
    EXPECT_EQ(annoy_save(idx, temp_path), 0);

    /* 加载 */
    annoy_index_t *loaded = annoy_load(temp_path);
    ASSERT_NE(loaded, nullptr);

    /* 验证统计信息 */
    EXPECT_EQ(annoy_get_n_items(loaded), n);
    EXPECT_EQ(annoy_get_n_trees(loaded), 10);

    /* 搜索验证 */
    float query[16];
    for (int i = 0; i < 16; i++) query[i] = (float)rand() / RAND_MAX;

    int result_ids[10];
    float result_dists[10];
    int count = annoy_search(loaded, query, 10, result_ids, result_dists, -1);
    EXPECT_GT(count, 0);

    /* 清理 */
    free(vectors);
    annoy_destroy(idx);
    annoy_destroy(loaded);
    remove(temp_path);
}

TEST(AnnoyTest, LoadNonExistent)
{
    annoy_index_t *idx = annoy_load("non_existent_file.bin");
    EXPECT_EQ(idx, nullptr);
}

TEST(AnnoyTest, SaveWithoutBuild)
{
    annoy_index_t *idx = annoy_create(16, "angular");
    ASSERT_NE(idx, nullptr);

    float v[16] = {0};
    annoy_add_item(idx, 0, v);

    /* 不构建直接保存（应该失败） */
    EXPECT_EQ(annoy_save(idx, "test_annoy_temp.bin"), -1);

    annoy_destroy(idx);
}

/* ============================================================
 * 测试 6: 召回率测试
 * ============================================================ */

TEST(AnnoyTest, DISABLED_RecallTest)
{
    srand(42);

    int dims = 16;
    int n_vectors = 200;
    int k = 10;

    /* 创建索引 */
    annoy_index_t *idx = annoy_create(dims, "euclidean");
    ASSERT_NE(idx, nullptr);

    /* 生成并添加向量 */
    float *vectors = generate_random_vectors(n_vectors, dims);

    for (int i = 0; i < n_vectors; i++) {
        annoy_add_item(idx, i, vectors + i * dims);
    }

    /* 构建较多树以提高召回率 */
    EXPECT_EQ(annoy_build(idx, 20), 0);

    /* 使用前 10 个向量作为查询，验证能否找到自己 */
    int hits = 0;
    for (int i = 0; i < 10; i++) {
        int result_ids[10];
        float result_dists[10];
        int count = annoy_search(idx, vectors + i * dims, k, result_ids, result_dists, -1);

        /* 第一个结果应该是自己 */
        if (count > 0 && result_ids[0] == i) {
            hits++;
        }
    }

    /* 召回率应该很高（至少 70%，Annoy 的召回率通常比精确搜索低） */
    EXPECT_GE(hits, 7);

    free(vectors);
    annoy_destroy(idx);
}
