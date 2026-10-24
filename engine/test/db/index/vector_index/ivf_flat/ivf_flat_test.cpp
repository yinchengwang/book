/*
 * ivf_flat_test.cpp — IVF-Flat 索引单元测试
 *
 * 测试内容：
 * 1. 基本创建和销毁
 * 2. 训练
 * 3. 添加向量
 * 4. 搜索
 * 5. 持久化
 */

#include <gtest/gtest.h>
#include <db/index/vector_index/ivf_flat/ivf_flat.h>

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

TEST(IVFFlatTest, CreateAndDestroy)
{
    /* 创建 IVF-Flat 索引: 100 个聚类, 64 维向量 */
    ivf_flat_index_t *idx = ivf_flat_create(100, 64);
    ASSERT_NE(idx, nullptr);

    int n_vectors = 0, n_clusters = 0;
    ivf_flat_stats(idx, &n_vectors, &n_clusters);
    EXPECT_EQ(n_vectors, 0);
    EXPECT_EQ(n_clusters, 100);

    ivf_flat_destroy(idx);
}

TEST(IVFFlatTest, CreateWithDifferentParams)
{
    /* 小规模配置 */
    ivf_flat_index_t *idx1 = ivf_flat_create(16, 16);
    ASSERT_NE(idx1, nullptr);
    ivf_flat_destroy(idx1);

    /* 中等规模配置 */
    ivf_flat_index_t *idx2 = ivf_flat_create(256, 128);
    ASSERT_NE(idx2, nullptr);
    ivf_flat_destroy(idx2);

    /* 大规模配置 */
    ivf_flat_index_t *idx3 = ivf_flat_create(1024, 256);
    ASSERT_NE(idx3, nullptr);
    ivf_flat_destroy(idx3);
}

TEST(IVFFlatTest, SetNProbe)
{
    ivf_flat_index_t *idx = ivf_flat_create(100, 64);
    ASSERT_NE(idx, nullptr);

    /* 设置探针数 */
    ivf_flat_set_nprobe(idx, 10);
    ivf_flat_set_nprobe(idx, 1);
    ivf_flat_set_nprobe(idx, 50);

    /* 超出范围自动调整 */
    ivf_flat_set_nprobe(idx, 200);  /* 应该被限制为 100 */
    ivf_flat_set_nprobe(idx, 0);    /* 应该被限制为 1 */

    ivf_flat_destroy(idx);
}

/* ============================================================
 * 测试 2: 训练
 * ============================================================ */

TEST(IVFFlatTest, DISABLED_Train)
{
    srand(42);

    ivf_flat_index_t *idx = ivf_flat_create(16, 16);
    ASSERT_NE(idx, nullptr);

    /* 生成训练数据 */
    int n_train = 500;
    float *train_vectors = generate_random_vectors(n_train, 16);

    /* 训练 */
    EXPECT_EQ(ivf_flat_train(idx, n_train, train_vectors), 0);

    free(train_vectors);
    ivf_flat_destroy(idx);
}

TEST(IVFFlatTest, DISABLED_TrainWithSmallDataset)
{
    srand(42);

    ivf_flat_index_t *idx = ivf_flat_create(8, 8);
    ASSERT_NE(idx, nullptr);

    /* 使用少量数据训练 */
    float vectors[] = {
        1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f,
        8.0f, 7.0f, 6.0f, 5.0f, 4.0f, 3.0f, 2.0f, 1.0f,
        1.5f, 2.5f, 3.5f, 4.5f, 5.5f, 6.5f, 7.5f, 8.5f,
        2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f,
        9.0f, 8.0f, 7.0f, 6.0f, 5.0f, 4.0f, 3.0f, 2.0f,
        3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f,
        10.0f, 9.0f, 8.0f, 7.0f, 6.0f, 5.0f, 4.0f, 3.0f,
        4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f,
        11.0f, 10.0f, 9.0f, 8.0f, 7.0f, 6.0f, 5.0f, 4.0f
    };

    EXPECT_EQ(ivf_flat_train(idx, 9, vectors), 0);

    ivf_flat_destroy(idx);
}

TEST(IVFFlatTest, TrainWithoutData)
{
    ivf_flat_index_t *idx = ivf_flat_create(16, 16);
    ASSERT_NE(idx, nullptr);

    /* 零个向量训练 */
    EXPECT_EQ(ivf_flat_train(idx, 0, nullptr), -1);

    /* 样本数少于簇数 */
    float vectors[16] = {0};
    EXPECT_EQ(ivf_flat_train(idx, 1, vectors), -1);

    ivf_flat_destroy(idx);
}

/* ============================================================
 * 测试 3: 添加向量
 * ============================================================ */

TEST(IVFFlatTest, DISABLED_AddVectors)
{
    srand(42);

    ivf_flat_index_t *idx = ivf_flat_create(16, 16);
    ASSERT_NE(idx, nullptr);

    /* 先训练 */
    float *train_vectors = generate_random_vectors(500, 16);
    EXPECT_EQ(ivf_flat_train(idx, 500, train_vectors), 0);
    free(train_vectors);

    /* 添加向量 */
    int n = 100;
    float *vectors = generate_random_vectors(n, 16);
    int *ids = (int *)malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) ids[i] = i;

    int added = ivf_flat_add(idx, n, vectors, ids);
    EXPECT_EQ(added, n);

    int n_vectors = 0, n_clusters = 0;
    ivf_flat_stats(idx, &n_vectors, &n_clusters);
    EXPECT_EQ(n_vectors, n);

    free(vectors);
    free(ids);
    ivf_flat_destroy(idx);
}

TEST(IVFFlatTest, AddWithoutTraining)
{
    srand(42);

    ivf_flat_index_t *idx = ivf_flat_create(16, 16);
    ASSERT_NE(idx, nullptr);

    /* 不训练直接添加（应该失败） */
    float *vectors = generate_random_vectors(10, 16);
    int ids[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

    int added = ivf_flat_add(idx, 10, vectors, ids);
    EXPECT_EQ(added, -1);

    free(vectors);
    ivf_flat_destroy(idx);
}

TEST(IVFFlatTest, DISABLED_AddEmpty)
{
    srand(42);

    ivf_flat_index_t *idx = ivf_flat_create(16, 16);
    ASSERT_NE(idx, nullptr);

    float *train_vectors = generate_random_vectors(100, 16);
    ivf_flat_train(idx, 100, train_vectors);
    free(train_vectors);

    float *vectors = generate_random_vectors(10, 16);
    int added = ivf_flat_add(idx, 0, vectors, nullptr);
    EXPECT_EQ(added, -1);

    free(vectors);
    ivf_flat_destroy(idx);
}

/* ============================================================
 * 测试 4: 搜索
 * ============================================================ */

TEST(IVFFlatTest, DISABLED_Search)
{
    srand(42);

    ivf_flat_index_t *idx = ivf_flat_create(16, 16);
    ASSERT_NE(idx, nullptr);

    /* 训练 */
    float *train_vectors = generate_random_vectors(500, 16);
    EXPECT_EQ(ivf_flat_train(idx, 500, train_vectors), 0);

    /* 添加向量 */
    int n = 100;
    float *vectors = generate_random_vectors(n, 16);
    int *ids = (int *)malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) ids[i] = i;
    ivf_flat_add(idx, n, vectors, ids);

    /* 设置探针数 */
    ivf_flat_set_nprobe(idx, 8);

    /* 搜索 */
    float query[16];
    for (int i = 0; i < 16; i++) query[i] = (float)rand() / RAND_MAX;

    int result_ids[10];
    float result_dists[10];
    int count = ivf_flat_search(idx, query, 10, result_ids, result_dists);

    EXPECT_GT(count, 0);
    EXPECT_LE(count, 10);

    /* 验证结果按距离排序 */
    for (int i = 1; i < count; i++) {
        EXPECT_LE(result_dists[i-1], result_dists[i]);
    }

    free(train_vectors);
    free(vectors);
    free(ids);
    ivf_flat_destroy(idx);
}

TEST(IVFFlatTest, DISABLED_SearchEmptyIndex)
{
    srand(42);

    ivf_flat_index_t *idx = ivf_flat_create(16, 16);
    ASSERT_NE(idx, nullptr);

    /* 训练但不添加向量 */
    float *train_vectors = generate_random_vectors(100, 16);
    ivf_flat_train(idx, 100, train_vectors);
    free(train_vectors);

    /* 搜索空索引 */
    float query[16] = {0};
    int result_ids[10];
    float result_dists[10];
    int count = ivf_flat_search(idx, query, 10, result_ids, result_dists);

    EXPECT_EQ(count, 0);

    ivf_flat_destroy(idx);
}

TEST(IVFFlatTest, DISABLED_SearchWithK1)
{
    srand(42);

    ivf_flat_index_t *idx = ivf_flat_create(8, 8);
    ASSERT_NE(idx, nullptr);

    /* 训练并添加 */
    float train_vectors[] = {
        1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f
    };
    ivf_flat_train(idx, 8, train_vectors);

    int ids[] = {0, 1, 2, 3, 4, 5, 6, 7};
    ivf_flat_add(idx, 8, train_vectors, ids);

    /* 搜索 k=1 */
    float query[] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    int result_ids[1];
    float result_dists[1];
    int count = ivf_flat_search(idx, query, 1, result_ids, result_dists);

    EXPECT_EQ(count, 1);
    /* 最近邻应该是 ID 0 */
    EXPECT_EQ(result_ids[0], 0);

    ivf_flat_destroy(idx);
}

/* ============================================================
 * 测试 5: 持久化
 * ============================================================ */

TEST(IVFFlatTest, DISABLED_SaveAndLoad)
{
    srand(42);

    /* 创建并填充索引 */
    ivf_flat_index_t *idx = ivf_flat_create(16, 16);
    ASSERT_NE(idx, nullptr);

    float *train_vectors = generate_random_vectors(500, 16);
    EXPECT_EQ(ivf_flat_train(idx, 500, train_vectors), 0);

    int n = 50;
    float *vectors = generate_random_vectors(n, 16);
    int *ids = (int *)malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) ids[i] = i;
    ivf_flat_add(idx, n, vectors, ids);

    int orig_n_vectors = 0, orig_n_clusters = 0;
    ivf_flat_stats(idx, &orig_n_vectors, &orig_n_clusters);

    /* 保存到临时文件 */
    const char *temp_path = "test_ivf_flat_temp.bin";
    EXPECT_EQ(ivf_flat_save(idx, temp_path), 0);

    /* 加载 */
    ivf_flat_index_t *loaded = ivf_flat_load(temp_path);
    ASSERT_NE(loaded, nullptr);

    /* 验证统计信息 */
    int loaded_n_vectors = 0, loaded_n_clusters = 0;
    ivf_flat_stats(loaded, &loaded_n_vectors, &loaded_n_clusters);
    EXPECT_EQ(loaded_n_vectors, orig_n_vectors);
    EXPECT_EQ(loaded_n_clusters, orig_n_clusters);

    /* 搜索验证 */
    float query[16];
    for (int i = 0; i < 16; i++) query[i] = (float)rand() / RAND_MAX;

    int result_ids[10];
    float result_dists[10];
    int count = ivf_flat_search(loaded, query, 10, result_ids, result_dists);
    EXPECT_GT(count, 0);

    /* 清理 */
    free(train_vectors);
    free(vectors);
    free(ids);
    ivf_flat_destroy(idx);
    ivf_flat_destroy(loaded);
    remove(temp_path);
}

TEST(IVFFlatTest, LoadNonExistent)
{
    ivf_flat_index_t *idx = ivf_flat_load("non_existent_file.bin");
    EXPECT_EQ(idx, nullptr);
}

/* ============================================================
 * 测试 6: 召回率测试
 * ============================================================ */

TEST(IVFFlatTest, DISABLED_RecallTest)
{
    srand(42);

    int dims = 16;
    int nlist = 16;
    int n_vectors = 200;
    int k = 10;

    /* 创建索引 */
    ivf_flat_index_t *idx = ivf_flat_create(nlist, dims);
    ASSERT_NE(idx, nullptr);

    /* 生成并添加向量 */
    float *vectors = generate_random_vectors(n_vectors, dims);
    int *ids = (int *)malloc(n_vectors * sizeof(int));
    for (int i = 0; i < n_vectors; i++) ids[i] = i;

    EXPECT_EQ(ivf_flat_train(idx, n_vectors, vectors), 0);
    EXPECT_EQ(ivf_flat_add(idx, n_vectors, vectors, ids), n_vectors);

    /* 设置高探针数以提高召回率 */
    ivf_flat_set_nprobe(idx, nlist);

    /* 使用前 10 个向量作为查询，验证能否找到自己 */
    int hits = 0;
    for (int i = 0; i < 10; i++) {
        int result_ids[10];
        float result_dists[10];
        int count = ivf_flat_search(idx, vectors + i * dims, k, result_ids, result_dists);

        /* 第一个结果应该是自己 */
        if (count > 0 && result_ids[0] == i) {
            hits++;
        }
    }

    /* 召回率应该很高（至少 80%） */
    EXPECT_GE(hits, 8);

    free(vectors);
    free(ids);
    ivf_flat_destroy(idx);
}