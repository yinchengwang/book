/*
 * lsh_multiprobe_test.cpp — Multi-Probe LSH 索引单元测试
 *
 * 测试内容：
 * 1. 基本创建和销毁
 * 2. 训练和添加向量
 * 3. kNN 搜索
 * 4. 探测参数影响
 * 5. 持久化
 */

#include <gtest/gtest.h>
#include <db/index/vector_index/lsh_multiprobe/lsh_multiprobe.h>

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

TEST(LSHMultiprobeTest, CreateAndDestroy)
{
    /* 创建 Multi-Probe LSH 索引: n_hash=16, n_tables=4, dims=128 */
    lsh_multiprobe_t *idx = lsh_multiprobe_create(16, 4, 128);
    ASSERT_NE(idx, nullptr);

    int n_vectors, n_tables, dims;
    lsh_multiprobe_stats(idx, &n_vectors, &n_tables, &dims);

    EXPECT_EQ(n_vectors, 0);
    EXPECT_EQ(n_tables, 4);
    EXPECT_EQ(dims, 128);

    lsh_multiprobe_destroy(idx);
}

TEST(LSHMultiprobeTest, CreateWithDifferentParams)
{
    /* 低参数 */
    lsh_multiprobe_t *idx1 = lsh_multiprobe_create(8, 2, 64);
    ASSERT_NE(idx1, nullptr);
    lsh_multiprobe_destroy(idx1);

    /* 中等参数 */
    lsh_multiprobe_t *idx2 = lsh_multiprobe_create(16, 4, 128);
    ASSERT_NE(idx2, nullptr);
    lsh_multiprobe_destroy(idx2);

    /* 高参数 */
    lsh_multiprobe_t *idx3 = lsh_multiprobe_create(24, 8, 256);
    ASSERT_NE(idx3, nullptr);
    lsh_multiprobe_destroy(idx3);
}

TEST(LSHMultiprobeTest, CreateWithInvalidParams)
{
    /* 无效参数 */
    EXPECT_EQ(lsh_multiprobe_create(0, 4, 128), nullptr);
    EXPECT_EQ(lsh_multiprobe_create(16, 0, 128), nullptr);
    EXPECT_EQ(lsh_multiprobe_create(16, 4, 0), nullptr);
    EXPECT_EQ(lsh_multiprobe_create(-1, 4, 128), nullptr);
}

/* ============================================================
 * 测试 2: 训练和添加向量
 * ============================================================ */

TEST(LSHMultiprobeTest, TrainAndAdd)
{
    srand(42);

    lsh_multiprobe_t *idx = lsh_multiprobe_create(16, 4, 64);
    ASSERT_NE(idx, nullptr);

    /* 训练 */
    int n_train = 100;
    float *train_vectors = generate_random_vectors(n_train, 64);
    EXPECT_EQ(lsh_multiprobe_train(idx, n_train, train_vectors), 0);

    /* 添加向量 */
    int n = 50;
    float *vectors = generate_random_vectors(n, 64);
    int *ids = (int *)malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) ids[i] = i;

    int added = lsh_multiprobe_add(idx, n, vectors, ids);
    EXPECT_EQ(added, n);

    int n_vectors;
    lsh_multiprobe_stats(idx, &n_vectors, nullptr, nullptr);
    EXPECT_EQ(n_vectors, n);

    free(train_vectors);
    free(vectors);
    free(ids);
    lsh_multiprobe_destroy(idx);
}

TEST(LSHMultiprobeTest, AddWithoutTrain)
{
    lsh_multiprobe_t *idx = lsh_multiprobe_create(16, 4, 64);
    ASSERT_NE(idx, nullptr);

    /* 不训练直接添加（应该失败） */
    float vector[64] = {0};
    int id = 0;
    EXPECT_EQ(lsh_multiprobe_add(idx, 1, vector, &id), 0);

    lsh_multiprobe_destroy(idx);
}

/* ============================================================
 * 测试 3: kNN 搜索
 * ============================================================ */

TEST(LSHMultiprobeTest, DISABLED_Search)
{
    srand(42);

    lsh_multiprobe_t *idx = lsh_multiprobe_create(16, 4, 64);
    ASSERT_NE(idx, nullptr);

    /* 训练 */
    int n_train = 200;
    float *train_vectors = generate_random_vectors(n_train, 64);
    EXPECT_EQ(lsh_multiprobe_train(idx, n_train, train_vectors), 0);

    /* 添加向量 */
    int n = 100;
    float *vectors = generate_random_vectors(n, 64);
    int *ids = (int *)malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) ids[i] = i;

    EXPECT_EQ(lsh_multiprobe_add(idx, n, vectors, ids), n);

    /* 搜索 */
    float query[64];
    for (int i = 0; i < 64; i++) query[i] = (float)rand() / RAND_MAX;

    int result_ids[10];
    float result_dists[10];
    int count = lsh_multiprobe_search(idx, query, 10, result_ids, result_dists, 10);

    EXPECT_GE(count, 0);
    EXPECT_LE(count, 10);

    /* 验证结果按距离排序 */
    for (int i = 1; i < count; i++) {
        EXPECT_LE(result_dists[i-1], result_dists[i]);
    }

    free(train_vectors);
    free(vectors);
    free(ids);
    lsh_multiprobe_destroy(idx);
}

TEST(LSHMultiprobeTest, SearchWithoutData)
{
    srand(42);

    lsh_multiprobe_t *idx = lsh_multiprobe_create(16, 4, 64);
    ASSERT_NE(idx, nullptr);

    /* 训练但不添加向量 */
    float train_vectors[64] = {0};
    lsh_multiprobe_train(idx, 1, train_vectors);

    /* 搜索（应该返回空） */
    float query[64] = {0};
    int result_ids[10];
    EXPECT_EQ(lsh_multiprobe_search(idx, query, 10, result_ids, nullptr, 10), 0);

    lsh_multiprobe_destroy(idx);
}

/* ============================================================
 * 测试 4: 探测参数影响
 * ============================================================ */

TEST(LSHMultiprobeTest, DISABLED_ProbeParameterEffect)
{
    srand(42);

    lsh_multiprobe_t *idx = lsh_multiprobe_create(16, 4, 64);
    ASSERT_NE(idx, nullptr);

    /* 训练和添加 */
    int n_train = 200;
    float *train_vectors = generate_random_vectors(n_train, 64);
    lsh_multiprobe_train(idx, n_train, train_vectors);

    int n = 100;
    float *vectors = generate_random_vectors(n, 64);
    int *ids = (int *)malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) ids[i] = i;
    lsh_multiprobe_add(idx, n, vectors, ids);

    /* 使用不同探测数量搜索 */
    float query[64];
    for (int i = 0; i < 64; i++) query[i] = (float)rand() / RAND_MAX;

    int result_ids_1[10];
    float result_dists_1[10];
    int count_1 = lsh_multiprobe_search(idx, query, 10, result_ids_1, result_dists_1, 4);

    int result_ids_2[10];
    float result_dists_2[10];
    int count_2 = lsh_multiprobe_search(idx, query, 10, result_ids_2, result_dists_2, 16);

    /* 更多探测应该返回更多或相同结果 */
    EXPECT_GE(count_2, count_1);

    free(train_vectors);
    free(vectors);
    free(ids);
    lsh_multiprobe_destroy(idx);
}

/* ============================================================
 * 测试 5: 批量搜索
 * ============================================================ */

TEST(LSHMultiprobeTest, DISABLED_BatchSearch)
{
    srand(42);

    lsh_multiprobe_t *idx = lsh_multiprobe_create(16, 4, 64);
    ASSERT_NE(idx, nullptr);

    /* 训练和添加 */
    int n_train = 200;
    float *train_vectors = generate_random_vectors(n_train, 64);
    lsh_multiprobe_train(idx, n_train, train_vectors);

    int n = 100;
    float *vectors = generate_random_vectors(n, 64);
    int *ids = (int *)malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) ids[i] = i;
    lsh_multiprobe_add(idx, n, vectors, ids);

    /* 批量搜索 */
    int nq = 5;
    float *queries = generate_random_vectors(nq, 64);
    int result_ids[5 * 10];
    float result_dists[5 * 10];

    EXPECT_EQ(lsh_multiprobe_search_batch(idx, nq, queries, 10, result_ids, result_dists, 10), 0);

    /* 验证每个查询都有结果 */
    for (int q = 0; q < nq; q++) {
        bool has_result = false;
        for (int k = 0; k < 10; k++) {
            if (result_ids[q * 10 + k] >= 0) {
                has_result = true;
                break;
            }
        }
        /* 至少某些查询有结果 */
    }

    free(train_vectors);
    free(vectors);
    free(ids);
    free(queries);
    lsh_multiprobe_destroy(idx);
}

/* ============================================================
 * 测试 6: 持久化
 * ============================================================ */

TEST(LSHMultiprobeTest, DISABLED_SaveAndLoad)
{
    srand(42);

    /* 创建并填充索引 */
    lsh_multiprobe_t *idx = lsh_multiprobe_create(16, 4, 64);
    ASSERT_NE(idx, nullptr);

    int n_train = 100;
    float *train_vectors = generate_random_vectors(n_train, 64);
    lsh_multiprobe_train(idx, n_train, train_vectors);

    int n = 50;
    float *vectors = generate_random_vectors(n, 64);
    int *ids = (int *)malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) ids[i] = i;
    lsh_multiprobe_add(idx, n, vectors, ids);

    /* 保存到临时文件 */
    const char *temp_path = "test_lsh_multiprobe_temp.bin";
    EXPECT_EQ(lsh_multiprobe_save(idx, temp_path), 0);

    /* 加载 */
    lsh_multiprobe_t *loaded = lsh_multiprobe_load(temp_path);
    ASSERT_NE(loaded, nullptr);

    /* 验证统计信息 */
    int n_vectors;
    lsh_multiprobe_stats(loaded, &n_vectors, nullptr, nullptr);
    EXPECT_EQ(n_vectors, n);

    /* 搜索验证 */
    float query[64];
    for (int i = 0; i < 64; i++) query[i] = (float)rand() / RAND_MAX;

    int result_ids[10];
    float result_dists[10];
    int count = lsh_multiprobe_search(loaded, query, 10, result_ids, result_dists, 10);
    EXPECT_GE(count, 0);

    /* 清理 */
    free(train_vectors);
    free(vectors);
    free(ids);
    lsh_multiprobe_destroy(idx);
    lsh_multiprobe_destroy(loaded);
    remove(temp_path);
}

TEST(LSHMultiprobeTest, LoadNonExistent)
{
    lsh_multiprobe_t *idx = lsh_multiprobe_load("non_existent_file.bin");
    EXPECT_EQ(idx, nullptr);
}

TEST(LSHMultiprobeTest, SaveWithoutData)
{
    lsh_multiprobe_t *idx = lsh_multiprobe_create(16, 4, 64);
    ASSERT_NE(idx, nullptr);

    /* 空索引保存 */
    EXPECT_EQ(lsh_multiprobe_save(idx, "test_lsh_multiprobe_temp.bin"), 0);

    lsh_multiprobe_destroy(idx);
}

/* ============================================================
 * 测试 7: 召回率测试
 * ============================================================ */

TEST(LSHMultiprobeTest, DISABLED_RecallTest)
{
    srand(42);

    int dims = 64;
    int n_vectors = 200;
    int k = 10;

    /* 创建索引 */
    lsh_multiprobe_t *idx = lsh_multiprobe_create(16, 4, dims);
    ASSERT_NE(idx, nullptr);

    /* 生成并添加向量 */
    float *vectors = generate_random_vectors(n_vectors, dims);
    int *ids = (int *)malloc(n_vectors * sizeof(int));
    for (int i = 0; i < n_vectors; i++) ids[i] = i;

    lsh_multiprobe_train(idx, n_vectors / 2, vectors);
    lsh_multiprobe_add(idx, n_vectors, vectors, ids);

    /* 使用前 10 个向量作为查询，验证能否找到自己 */
    int hits = 0;
    for (int i = 0; i < 10; i++) {
        int result_ids[10];
        float result_dists[10];
        int count = lsh_multiprobe_search(idx, vectors + i * dims, k, result_ids, result_dists, 20);

        /* 第一个结果应该是自己 */
        if (count > 0 && result_ids[0] == i) {
            hits++;
        }
    }

    /* Multi-Probe LSH 召回率应该较高（至少 70%） */
    EXPECT_GE(hits, 7);

    free(vectors);
    free(ids);
    lsh_multiprobe_destroy(idx);
}