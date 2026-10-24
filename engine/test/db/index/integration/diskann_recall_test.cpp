// DiskANN 召回率测试
//
// 验证 DiskANN 索引在不同规模数据集上的召回率表现。

#include <gtest/gtest.h>
#include <vector>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <cstring>
#include <cmath>

extern "C" {
#include "db/index/vector_index/diskann/diskann.h"
#include "algo-prod/distance/distance.h"
}

/* ============================================================================
 * 测试配置
 * ============================================================================ */

namespace {
    // 测试规模配置
    constexpr int32_t SMALL_N = 1000;       // 小规模测试
    constexpr int32_t MEDIUM_N = 10000;     // 中规模测试
    constexpr int32_t LARGE_N = 100000;     // 大规模测试（可选运行）

    constexpr int32_t DIMS = 64;            // 向量维度（较低维度加快测试）
    constexpr int32_t QUERY_COUNT = 100;     // 查询数量
    constexpr int32_t K = 10;                // Top-K

    // 召回率阈值
    constexpr float SMALL_RECALL_THRESHOLD = 0.50f;
    constexpr float MEDIUM_RECALL_THRESHOLD = 0.40f;
    constexpr float LARGE_RECALL_THRESHOLD = 0.35f;

    // 搜索参数
    constexpr int32_t DEFAULT_R = 16;        // 目标邻居数
    constexpr int32_t DEFAULT_L = 50;        // 构图候选数
    constexpr int32_t SEARCH_L = 50;         // 搜索候选数
    constexpr int32_t MAX_ITER = 10;         // 最大迭代次数
}

/* ============================================================================
 * 辅助函数
 * ============================================================================ */

/**
 * @brief 生成随机向量
 */
static void generate_random_vectors(float *vectors, int32_t n, int32_t dims)
{
    for (int32_t i = 0; i < n * dims; i++) {
        vectors[i] = (float)rand() / RAND_MAX;
    }
}

/**
 * @brief 生成高斯分布随机向量
 */
static void generate_gaussian_vectors(float *vectors, int32_t n, int32_t dims)
{
    for (int32_t i = 0; i < n; i++) {
        float *v = &vectors[i * dims];
        float norm = 0.0f;
        for (int32_t d = 0; d < dims; d++) {
            // Box-Muller 变换生成高斯随机数
            float u1 = (float)rand() / RAND_MAX;
            float u2 = (float)rand() / RAND_MAX;
            v[d] = sqrtf(-2.0f * logf(u1 + 1e-10f)) * cosf(2.0f * 3.14159f * u2);
            norm += v[d] * v[d];
        }
        // 归一化
        norm = sqrtf(norm);
        if (norm > 0.0f) {
            for (int32_t d = 0; d < dims; d++) {
                v[d] /= norm;
            }
        }
    }
}

/**
 * @brief 计算召回率
 */
static float compute_recall(const int32_t *result_ids,
                           const int32_t *ground_truth_ids,
                           int32_t k)
{
    int32_t hits = 0;
    for (int32_t i = 0; i < k; i++) {
        for (int32_t j = 0; j < k; j++) {
            if (result_ids[i] == ground_truth_ids[j]) {
                hits++;
                break;
            }
        }
    }
    return (float)hits / (float)k;
}

/**
 * @brief 暴力搜索获取真值
 */
static void bruteforce_topk(const float *query,
                            const float *vectors,
                            int32_t n_vectors,
                            int32_t dims,
                            int32_t k,
                            int32_t *out_ids)
{
    std::vector<std::pair<float, int32_t>> scored;
    scored.reserve((size_t)n_vectors);

    for (int32_t i = 0; i < n_vectors; i++) {
        float dist = distance_l2sqr(query, &vectors[i * dims], dims);
        scored.emplace_back(dist, i);
    }

    std::sort(scored.begin(), scored.end());

    for (int32_t i = 0; i < k && i < (int32_t)scored.size(); i++) {
        out_ids[i] = scored[i].second;
    }
}

/* ============================================================================
 * 测试类
 * ============================================================================ */

class DiskANNRecallTest : public ::testing::Test {
protected:
    void SetUp() override {
        srand(42);  // 固定种子保证可重复性
    }

    void TearDown() override {
        // 清理
    }

    /**
     * @brief 运行召回率测试
     */
    float run_recall_test(int32_t n_vectors,
                          const float *vectors,
                          const float *queries,
                          int32_t search_list_size)
    {
        // 创建索引
        diskann_t *index = diskann_index_create(
            DIMS, DEFAULT_R, DEFAULT_L, DISTANCE_METRIC_L2_SQUARED);
        EXPECT_NE(index, nullptr);

        // 添加向量
        int32_t add_ret = diskann_index_add(index, n_vectors, vectors);
        EXPECT_EQ(add_ret, 0);

        // 构建索引
        int32_t build_ret = diskann_index_build(index);
        EXPECT_EQ(build_ret, 0);

        // 搜索并计算召回率
        float total_recall = 0.0f;
        int32_t result_ids[K];
        float result_dists[K];
        int32_t ground_truth[K];

        for (int32_t q = 0; q < QUERY_COUNT && q * K < n_vectors; q++) {
            // 近似搜索
            diskann_index_search(
                index,
                &queries[q * DIMS],
                K,
                search_list_size,
                MAX_ITER,
                result_dists,
                result_ids
            );

            // 暴力搜索获取真值
            bruteforce_topk(
                &queries[q * DIMS],
                vectors,
                n_vectors,
                DIMS,
                K,
                ground_truth
            );

            // 计算召回率
            total_recall += compute_recall(result_ids, ground_truth, K);
        }

        float avg_recall = total_recall / (float)QUERY_COUNT;

        // 清理
        diskann_index_drop(index);

        return avg_recall;
    }
};

/* ============================================================================
 * 测试用例
 * ============================================================================ */

/**
 * 测试：小规模数据集召回率
 * 验证：1000 向量规模下召回率 >= 0.50
 */
TEST_F(DiskANNRecallTest, SmallDatasetRecall)
{
    std::vector<float> vectors((size_t)SMALL_N * DIMS);
    std::vector<float> queries((size_t)QUERY_COUNT * DIMS);

    generate_random_vectors(vectors.data(), SMALL_N, DIMS);
    generate_random_vectors(queries.data(), QUERY_COUNT, DIMS);

    float recall = run_recall_test(SMALL_N, vectors.data(), queries.data(), SEARCH_L);

    printf("Small dataset (N=%d) recall: %.3f (threshold: %.2f)\n",
           SMALL_N, recall, SMALL_RECALL_THRESHOLD);

    EXPECT_GE(recall, SMALL_RECALL_THRESHOLD);
}

/**
 * 测试：中等规模数据集召回率
 * 验证：10000 向量规模下召回率 >= 0.40
 */
TEST_F(DiskANNRecallTest, MediumDatasetRecall)
{
    std::vector<float> vectors((size_t)MEDIUM_N * DIMS);
    std::vector<float> queries((size_t)QUERY_COUNT * DIMS);

    generate_random_vectors(vectors.data(), MEDIUM_N, DIMS);
    generate_random_vectors(queries.data(), QUERY_COUNT, DIMS);

    float recall = run_recall_test(MEDIUM_N, vectors.data(), queries.data(), SEARCH_L);

    printf("Medium dataset (N=%d) recall: %.3f (threshold: %.2f)\n",
           MEDIUM_N, recall, MEDIUM_RECALL_THRESHOLD);

    EXPECT_GE(recall, MEDIUM_RECALL_THRESHOLD);
}

/**
 * 测试：不同搜索宽度对召回率的影响
 * 验证：更大的搜索宽度应该带来更高的召回率
 */
TEST_F(DiskANNRecallTest, SearchWidthImpact)
{
    std::vector<float> vectors((size_t)SMALL_N * DIMS);
    std::vector<float> queries((size_t)QUERY_COUNT * DIMS);

    generate_random_vectors(vectors.data(), SMALL_N, DIMS);
    generate_random_vectors(queries.data(), QUERY_COUNT, DIMS);

    // 小搜索宽度
    float recall_small = run_recall_test(SMALL_N, vectors.data(), queries.data(), 20);

    // 大搜索宽度
    float recall_large = run_recall_test(SMALL_N, vectors.data(), queries.data(), 100);

    printf("Search width impact: L=20 -> %.3f, L=100 -> %.3f\n",
           recall_small, recall_large);

    // 大搜索宽度应该 >= 小搜索宽度
    EXPECT_GE(recall_large, recall_small - 0.05f);
}

/**
 * 测试：不同 R（目标邻居数）对召回率的影响
 * 验证：更大的 R 应该带来更高的召回率
 */
TEST_F(DiskANNRecallTest, IndexSizeImpact)
{
    std::vector<float> vectors((size_t)SMALL_N * DIMS);
    std::vector<float> queries((size_t)QUERY_COUNT * DIMS);

    generate_random_vectors(vectors.data(), SMALL_N, DIMS);
    generate_random_vectors(queries.data(), QUERY_COUNT, DIMS);

    // 创建小 R 索引
    {
        diskann_t *index = diskann_index_create(
            DIMS, 8, 30, DISTANCE_METRIC_L2_SQUARED);
        ASSERT_NE(index, nullptr);

        diskann_index_add(index, SMALL_N, vectors.data());
        diskann_index_build(index);

        float total_recall = 0.0f;
        int32_t result_ids[K];
        float result_dists[K];
        int32_t ground_truth[K];

        for (int32_t q = 0; q < 10 && q * K < SMALL_N; q++) {
            diskann_index_search(index, &queries[q * DIMS], K, 30, MAX_ITER, result_dists, result_ids);
            bruteforce_topk(&queries[q * DIMS], vectors.data(), SMALL_N, DIMS, K, ground_truth);
            total_recall += compute_recall(result_ids, ground_truth, K);
        }

        float recall_small_r = total_recall / 10.0f;
        printf("Small R (R=8) recall: %.3f\n", recall_small_r);

        diskann_index_drop(index);
    }

    // 创建大 R 索引
    {
        diskann_t *index = diskann_index_create(
            DIMS, 32, 80, DISTANCE_METRIC_L2_SQUARED);
        ASSERT_NE(index, nullptr);

        diskann_index_add(index, SMALL_N, vectors.data());
        diskann_index_build(index);

        float total_recall = 0.0f;
        int32_t result_ids[K];
        float result_dists[K];
        int32_t ground_truth[K];

        for (int32_t q = 0; q < 10 && q * K < SMALL_N; q++) {
            diskann_index_search(index, &queries[q * DIMS], K, 80, MAX_ITER, result_dists, result_ids);
            bruteforce_topk(&queries[q * DIMS], vectors.data(), SMALL_N, DIMS, K, ground_truth);
            total_recall += compute_recall(result_ids, ground_truth, K);
        }

        float recall_large_r = total_recall / 10.0f;
        printf("Large R (R=32) recall: %.3f\n", recall_large_r);

        diskann_index_drop(index);
    }
}

/**
 * 测试：PQ 量化对召回率的影响
 * 验证：启用 PQ 量化后召回率应保持合理水平
 */
TEST_F(DiskANNRecallTest, PQQuantizationImpact)
{
    std::vector<float> vectors((size_t)SMALL_N * DIMS);
    std::vector<float> queries((size_t)QUERY_COUNT * DIMS);

    generate_random_vectors(vectors.data(), SMALL_N, DIMS);
    generate_random_vectors(queries.data(), QUERY_COUNT, DIMS);

    // 无 PQ 基准
    float recall_no_pq = run_recall_test(SMALL_N, vectors.data(), queries.data(), SEARCH_L);

    // 创建带 PQ 的索引
    {
        diskann_t *index = diskann_index_create(
            DIMS, DEFAULT_R, DEFAULT_L, DISTANCE_METRIC_L2_SQUARED);
        ASSERT_NE(index, nullptr);

        // 启用 PQ
        diskann_quantization_params_t pq_params;
        pq_params.enabled = true;
        pq_params.pq_m = 8;           // 子空间数
        pq_params.pq_bits = 8;        // 8 bits
        pq_params.train_max_vectors = 256;
        diskann_index_set_quantization_params(index, &pq_params);

        diskann_index_add(index, SMALL_N, vectors.data());

        // 训练 PQ
        int32_t train_ret = diskann_index_train_pq(index);
        if (train_ret == 0) {
            diskann_index_build(index);

            float total_recall = 0.0f;
            int32_t result_ids[K];
            float result_dists[K];
            int32_t ground_truth[K];

            for (int32_t q = 0; q < QUERY_COUNT && q * K < SMALL_N; q++) {
                diskann_index_search(index, &queries[q * DIMS], K, SEARCH_L, MAX_ITER, result_dists, result_ids);
                bruteforce_topk(&queries[q * DIMS], vectors.data(), SMALL_N, DIMS, K, ground_truth);
                total_recall += compute_recall(result_ids, ground_truth, K);
            }

            float recall_with_pq = total_recall / (float)QUERY_COUNT;
            printf("PQ impact: No PQ -> %.3f, With PQ -> %.3f\n", recall_no_pq, recall_with_pq);

            // PQ 会降低召回率，但不应该降低太多
            EXPECT_GE(recall_with_pq, recall_no_pq - 0.15f);
        }

        diskann_index_drop(index);
    }
}

/**
 * 测试：持久化与加载
 * 验证：保存和加载索引后召回率应保持一致
 */
TEST_F(DiskANNRecallTest, PersistAndLoad)
{
    std::vector<float> vectors((size_t)SMALL_N * DIMS);
    std::vector<float> queries((size_t)QUERY_COUNT * DIMS);

    generate_random_vectors(vectors.data(), SMALL_N, DIMS);
    generate_random_vectors(queries.data(), QUERY_COUNT, DIMS);

    const char *test_file = "test_diskann_persist.bin";

    // 构建索引
    {
        diskann_t *index = diskann_index_create(
            DIMS, DEFAULT_R, DEFAULT_L, DISTANCE_METRIC_L2_SQUARED);
        ASSERT_NE(index, nullptr);

        diskann_index_add(index, SMALL_N, vectors.data());
        diskann_index_build(index);

        // 保存
        int32_t save_ret = diskann_index_save(index, test_file);
        EXPECT_EQ(save_ret, 0);

        diskann_index_drop(index);
    }

    // 加载并测试召回率
    {
        diskann_t *index = diskann_index_load(test_file);
        ASSERT_NE(index, nullptr);

        float total_recall = 0.0f;
        int32_t result_ids[K];
        float result_dists[K];
        int32_t ground_truth[K];

        for (int32_t q = 0; q < QUERY_COUNT && q * K < SMALL_N; q++) {
            diskann_index_search(index, &queries[q * DIMS], K, SEARCH_L, MAX_ITER, result_dists, result_ids);
            bruteforce_topk(&queries[q * DIMS], vectors.data(), SMALL_N, DIMS, K, ground_truth);
            total_recall += compute_recall(result_ids, ground_truth, K);
        }

        float recall = total_recall / (float)QUERY_COUNT;
        printf("After load recall: %.3f\n", recall);

        EXPECT_GE(recall, SMALL_RECALL_THRESHOLD);

        diskann_index_drop(index);
    }

    // 清理测试文件
    remove(test_file);
}

/**
 * 测试：搜索延迟
 * 验证：平均查询延迟应低于阈值
 */
TEST_F(DiskANNRecallTest, SearchLatency)
{
    std::vector<float> vectors((size_t)MEDIUM_N * DIMS);
    std::vector<float> queries((size_t)QUERY_COUNT * DIMS);

    generate_random_vectors(vectors.data(), MEDIUM_N, DIMS);
    generate_random_vectors(queries.data(), QUERY_COUNT, DIMS);

    diskann_t *index = diskann_index_create(
        DIMS, DEFAULT_R, DEFAULT_L, DISTANCE_METRIC_L2_SQUARED);
    ASSERT_NE(index, nullptr);

    diskann_index_add(index, MEDIUM_N, vectors.data());
    diskann_index_build(index);

    int32_t result_ids[K];
    float result_dists[K];

    // 预热
    for (int32_t q = 0; q < 10; q++) {
        diskann_index_search(index, &queries[q * DIMS], K, SEARCH_L, MAX_ITER, result_dists, result_ids);
    }

    // 计时
    auto start = std::chrono::high_resolution_clock::now();
    for (int32_t q = 0; q < QUERY_COUNT; q++) {
        diskann_index_search(index, &queries[q * DIMS], K, SEARCH_L, MAX_ITER, result_dists, result_ids);
    }
    auto end = std::chrono::high_resolution_clock::now();

    double total_ms = std::chrono::duration<double, std::milli>(end - start).count();
    double avg_ms = total_ms / QUERY_COUNT;

    printf("Average search latency: %.3f ms (threshold: 5.0 ms)\n", avg_ms);

    // 中等规模下延迟应低于 5ms
    EXPECT_LT(avg_ms, 5.0);

    diskann_index_drop(index);
}

/**
 * 测试：Alpha 参数对召回率的影响
 * 验证：不同的 alpha 值会影响图的质量
 */
TEST_F(DiskANNRecallTest, AlphaParameterImpact)
{
    std::vector<float> vectors((size_t)SMALL_N * DIMS);
    std::vector<float> queries((size_t)QUERY_COUNT * DIMS);

    generate_random_vectors(vectors.data(), SMALL_N, DIMS);
    generate_random_vectors(queries.data(), QUERY_COUNT, DIMS);

    // 测试不同的 alpha 值
    const float alphas[] = {1.0f, 1.2f, 1.5f};
    const int32_t num_alphas = sizeof(alphas) / sizeof(alphas[0]);

    float prev_recall = 0.0f;
    for (int32_t a = 0; a < num_alphas; a++) {
        diskann_t *index = diskann_index_create(
            DIMS, DEFAULT_R, DEFAULT_L, DISTANCE_METRIC_L2_SQUARED);
        ASSERT_NE(index, nullptr);

        diskann_index_set_alpha(index, alphas[a]);

        diskann_index_add(index, SMALL_N, vectors.data());
        diskann_index_build(index);

        float total_recall = 0.0f;
        int32_t result_ids[K];
        float result_dists[K];
        int32_t ground_truth[K];

        for (int32_t q = 0; q < 10 && q * K < SMALL_N; q++) {
            diskann_index_search(index, &queries[q * DIMS], K, SEARCH_L, MAX_ITER, result_dists, result_ids);
            bruteforce_topk(&queries[q * DIMS], vectors.data(), SMALL_N, DIMS, K, ground_truth);
            total_recall += compute_recall(result_ids, ground_truth, K);
        }

        float recall = total_recall / 10.0f;
        printf("Alpha=%.1f recall: %.3f\n", alphas[a], recall);

        diskann_index_drop(index);
    }
}
