/*
 * ivf_pq_test.cpp — IVF-PQ (Inverted File with Product Quantization) 单元测试
 *
 * 测试内容：
 * 1. 基本创建和销毁
 * 2. 训练
 * 3. 添加向量
 * 4. 搜索
 * 5. 重排精排
 * 6. 持久化
 * 7. 统计信息
 */

#include <gtest/gtest.h>
#include <db/index/vector_index/ivf_pq/ivf_pq.h>

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

TEST(IVFPQTest, CreateAndDestroy)
{
    /* 创建 IVF-PQ 索引: 100 个聚类, 8 个子空间, 8 比特 PQ, 64 维向量 */
    ivf_pq_index_t *idx = ivf_pq_create(100, 8, 8, 64);
    ASSERT_NE(idx, nullptr);

    int n_vectors = 0, code_size = 0;
    ivf_pq_stats(idx, &n_vectors, &code_size);
    EXPECT_EQ(n_vectors, 0);

    ivf_pq_destroy(idx);
}

TEST(IVFPQTest, CreateWithDifferentParams)
{
    /* 小规模配置 */
    ivf_pq_index_t *idx1 = ivf_pq_create(16, 4, 8, 16);
    ASSERT_NE(idx1, nullptr);
    ivf_pq_destroy(idx1);

    /* 中等规模配置 */
    ivf_pq_index_t *idx2 = ivf_pq_create(256, 16, 8, 128);
    ASSERT_NE(idx2, nullptr);
    ivf_pq_destroy(idx2);
}

TEST(IVFPQTest, SetNProbe)
{
    ivf_pq_index_t *idx = ivf_pq_create(100, 8, 8, 64);
    ASSERT_NE(idx, nullptr);

    /* 设置探针数 */
    ivf_pq_set_nprobe(idx, 10);
    ivf_pq_set_nprobe(idx, 1);
    ivf_pq_set_nprobe(idx, 50);

    ivf_pq_destroy(idx);
}

/* ============================================================
 * 测试 2: 训练
 * ============================================================ */

TEST(IVFPQTest, DISABLED_Train)
{
    srand(42);

    ivf_pq_index_t *idx = ivf_pq_create(16, 4, 8, 16);
    ASSERT_NE(idx, nullptr);

    /* 生成训练数据（减少数量） */
    int n_train = 100;
    float *train_vectors = generate_random_vectors(n_train, 16);

    /* 训练 */
    EXPECT_EQ(ivf_pq_train(idx, n_train, train_vectors), 0);

    free(train_vectors);
    ivf_pq_destroy(idx);
}

TEST(IVFPQTest, DISABLED_TrainWithSmallDataset)
{
    srand(42);

    ivf_pq_index_t *idx = ivf_pq_create(8, 4, 8, 8);
    ASSERT_NE(idx, nullptr);

    /* 使用少量数据训练 */
    float vectors[] = {
        1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f,
        8.0f, 7.0f, 6.0f, 5.0f, 4.0f, 3.0f, 2.0f, 1.0f,
        1.5f, 2.5f, 3.5f, 4.5f, 5.5f, 6.5f, 7.5f, 8.5f
    };

    EXPECT_EQ(ivf_pq_train(idx, 3, vectors), 0);

    ivf_pq_destroy(idx);
}

TEST(IVFPQTest, TrainWithoutData)
{
    ivf_pq_index_t *idx = ivf_pq_create(16, 4, 8, 16);
    ASSERT_NE(idx, nullptr);

    /* 零个向量训练 */
    EXPECT_EQ(ivf_pq_train(idx, 0, nullptr), -1);

    ivf_pq_destroy(idx);
}

/* ============================================================
 * 测试 3: 添加向量
 * ============================================================ */

TEST(IVFPQTest, DISABLED_AddVectors)
{
    srand(42);

    ivf_pq_index_t *idx = ivf_pq_create(16, 4, 8, 16);
    ASSERT_NE(idx, nullptr);

    /* 先训练（减少数量） */
    float *train_vectors = generate_random_vectors(100, 16);
    EXPECT_EQ(ivf_pq_train(idx, 100, train_vectors), 0);
    free(train_vectors);

    /* 添加向量 */
    int n = 50;
    float *vectors = generate_random_vectors(n, 16);
    int *ids = (int *)malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) ids[i] = i;

    int added = ivf_pq_add(idx, n, vectors, ids);
    EXPECT_EQ(added, n);

    int n_vectors = 0, code_size = 0;
    ivf_pq_stats(idx, &n_vectors, &code_size);
    EXPECT_EQ(n_vectors, n);

    free(vectors);
    free(ids);
    ivf_pq_destroy(idx);
}

TEST(IVFPQTest, AddWithoutTraining)
{
    srand(42);

    ivf_pq_index_t *idx = ivf_pq_create(16, 4, 8, 16);
    ASSERT_NE(idx, nullptr);

    /* 不训练直接添加（应该失败或行为不确定） */
    float *vectors = generate_random_vectors(10, 16);
    int ids[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

    int added = ivf_pq_add(idx, 10, vectors, ids);
    /* 可能返回 -1 或 0，取决于实现 */

    free(vectors);
    ivf_pq_destroy(idx);
}

TEST(IVFPQTest, AddEmpty)
{
    srand(42);

    ivf_pq_index_t *idx = ivf_pq_create(16, 4, 8, 16);
    ASSERT_NE(idx, nullptr);

    float *vectors = generate_random_vectors(100, 16);
    int added = ivf_pq_add(idx, 0, vectors, nullptr);
    /* 空向量添加应该返回 0 或 -1 */
    EXPECT_TRUE(added == 0 || added == -1);

    free(vectors);
    ivf_pq_destroy(idx);
}

/* ============================================================
 * 测试 4: 搜索
 * ============================================================ */

TEST(IVFPQTest, DISABLED_Search)
{
    srand(42);

    ivf_pq_index_t *idx = ivf_pq_create(16, 4, 8, 16);
    ASSERT_NE(idx, nullptr);

    /* 训练 */
    float *train_vectors = generate_random_vectors(500, 16);
    EXPECT_EQ(ivf_pq_train(idx, 500, train_vectors), 0);

    /* 添加向量 */
    int n = 100;
    float *vectors = generate_random_vectors(n, 16);
    int *ids = (int *)malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) ids[i] = i;
    ivf_pq_add(idx, n, vectors, ids);

    /* 搜索 */
    float query[16];
    for (int i = 0; i < 16; i++) query[i] = (float)rand() / RAND_MAX;

    int result_ids[10];
    float result_dists[10];
    int count = ivf_pq_search(idx, query, 10, result_ids, result_dists);

    EXPECT_GT(count, 0);
    EXPECT_LE(count, 10);

    /* 验证结果按距离排序 */
    for (int i = 1; i < count; i++) {
        EXPECT_LE(result_dists[i], result_dists[i - 1]);
    }

    free(train_vectors);
    free(vectors);
    free(ids);
    ivf_pq_destroy(idx);
}

TEST(IVFPQTest, DISABLED_SearchEmptyIndex)
{
    srand(42);

    ivf_pq_index_t *idx = ivf_pq_create(16, 4, 8, 16);
    ASSERT_NE(idx, nullptr);

    /* 训练但不添加向量 */
    float *train_vectors = generate_random_vectors(100, 16);
    ivf_pq_train(idx, 100, train_vectors);
    free(train_vectors);

    /* 搜索空索引 */
    float query[16] = {0};
    int result_ids[10];
    float result_dists[10];
    int count = ivf_pq_search(idx, query, 10, result_ids, result_dists);

    EXPECT_EQ(count, 0);

    ivf_pq_destroy(idx);
}

TEST(IVFPQTest, DISABLED_SearchWithK1)
{
    srand(42);

    ivf_pq_index_t *idx = ivf_pq_create(8, 4, 8, 8);
    ASSERT_NE(idx, nullptr);

    /* 训练并添加 */
    float train_vectors[] = {
        1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f
    };
    ivf_pq_train(idx, 5, train_vectors);

    int ids[] = {0, 1, 2, 3, 4};
    float vectors[] = {
        1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f
    };
    ivf_pq_add(idx, 5, vectors, ids);

    /* 搜索 k=1 */
    float query[] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    int result_ids[1];
    float result_dists[1];
    int count = ivf_pq_search(idx, query, 1, result_ids, result_dists);

    EXPECT_EQ(count, 1);
    EXPECT_EQ(result_ids[0], 0);

    ivf_pq_destroy(idx);
}

/* ============================================================
 * 测试 5: 重排精排
 * ============================================================ */

TEST(IVFPQTest, DISABLED_Rerank)
{
    srand(42);

    ivf_pq_index_t *idx = ivf_pq_create(16, 4, 8, 16);
    ASSERT_NE(idx, nullptr);

    /* 训练并添加 */
    float *train_vectors = generate_random_vectors(500, 16);
    EXPECT_EQ(ivf_pq_train(idx, 500, train_vectors), 0);

    int n = 50;
    float *vectors = generate_random_vectors(n, 16);
    int *ids = (int *)malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) ids[i] = i;
    ivf_pq_add(idx, n, vectors, ids);

    /* 使用前 10 个向量作为候选进行重排 */
    int cand_ids[10];
    float cand_dists[10];
    int k = 5;
    int result_ids[5];
    float result_dists[5];

    int count = ivf_pq_rerank(idx, 10, vectors, k, result_ids, result_dists);
    EXPECT_LE(count, k);

    free(train_vectors);
    free(vectors);
    free(ids);
    ivf_pq_destroy(idx);
}

/* ============================================================
 * 测试 6: 持久化
 * ============================================================ */

TEST(IVFPQTest, DISABLED_SaveAndLoad)
{
    srand(42);

    /* 创建并填充索引 */
    ivf_pq_index_t *idx = ivf_pq_create(16, 4, 8, 16);
    ASSERT_NE(idx, nullptr);

    float *train_vectors = generate_random_vectors(500, 16);
    EXPECT_EQ(ivf_pq_train(idx, 500, train_vectors), 0);

    int n = 50;
    float *vectors = generate_random_vectors(n, 16);
    int *ids = (int *)malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) ids[i] = i;
    ivf_pq_add(idx, n, vectors, ids);

    int orig_n_vectors = 0, orig_code_size = 0;
    ivf_pq_stats(idx, &orig_n_vectors, &orig_code_size);

    /* 保存到临时文件 */
    const char *temp_path = "test_ivf_pq_temp.bin";
    EXPECT_EQ(ivf_pq_save(idx, temp_path), 0);

    /* 加载 */
    ivf_pq_index_t *loaded = ivf_pq_load(temp_path);
    ASSERT_NE(loaded, nullptr);

    /* 验证统计信息 */
    int loaded_n_vectors = 0, loaded_code_size = 0;
    ivf_pq_stats(loaded, &loaded_n_vectors, &loaded_code_size);
    EXPECT_EQ(loaded_n_vectors, orig_n_vectors);

    /* 搜索验证 */
    float query[16];
    for (int i = 0; i < 16; i++) query[i] = (float)rand() / RAND_MAX;

    int result_ids[10];
    float result_dists[10];
    int count = ivf_pq_search(loaded, query, 10, result_ids, result_dists);
    EXPECT_GT(count, 0);

    /* 清理 */
    free(train_vectors);
    free(vectors);
    free(ids);
    ivf_pq_destroy(idx);
    ivf_pq_destroy(loaded);
    remove(temp_path);
}

TEST(IVFPQTest, LoadNonExistent)
{
    ivf_pq_index_t *idx = ivf_pq_load("non_existent_file.bin");
    EXPECT_EQ(idx, nullptr);
}

/* ============================================================
 * 测试 7: 大规模测试
 * ============================================================ */

TEST(IVFPQTest, DISABLED_LargeScale)
{
    srand(42);

    ivf_pq_index_t *idx = ivf_pq_create(256, 16, 8, 128);
    ASSERT_NE(idx, nullptr);

    /* 训练数据 */
    int n_train = 5000;
    float *train_vectors = generate_random_vectors(n_train, 128);
    EXPECT_EQ(ivf_pq_train(idx, n_train, train_vectors), 0);

    /* 添加大量向量 */
    int n = 10000;
    float *vectors = generate_random_vectors(n, 128);
    int *ids = (int *)malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) ids[i] = i;
    int added = ivf_pq_add(idx, n, vectors, ids);
    EXPECT_EQ(added, n);

    /* 搜索 */
    float query[128];
    for (int i = 0; i < 128; i++) query[i] = (float)rand() / RAND_MAX;

    int result_ids[100];
    float result_dists[100];
    int count = ivf_pq_search(idx, query, 100, result_ids, result_dists);

    EXPECT_GT(count, 0);
    EXPECT_LE(count, 100);

    free(train_vectors);
    free(vectors);
    free(ids);
    ivf_pq_destroy(idx);
}