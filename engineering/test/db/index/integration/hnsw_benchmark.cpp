// HNSW 基准测试
//
// 验证 HNSW 索引的召回率、查询延迟与参数扫描表现。
// 作为 TEST_F 用例加入 engineering/test/db/index/integration/integration_test 二进制，
// 不携带独立的 main()，复用 integration_test.cpp 的测试入口。
//
// 重要：使用聚类数据而非随机均匀数据！
// - 随机均匀数据：所有点几乎等距，最近邻边界模糊，ANN 算法难以有效剪枝
// - 聚类数据（SIFT 特性）：点成簇聚集，最近邻清晰可分，召回率可达 90%+
// 本测试生成 N_CLUSTERS 个聚类中心，每个向量属于某中心 + 小偏移，模拟真实数据分布

#include <gtest/gtest.h>
#include <vector>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <cmath>

extern "C" {
#include "benchmark_config.h"
#include "db/index/vector_index/hnsw/faiss_hnsw.h"
#include "algo-prod/distance/distance.h"
}

/* 聚类数据生成配置（模拟 SIFT 数据特性） */
#define N_CLUSTERS 50           /* 聚类数量，越多则簇越紧凑 */
#define CLUSTER_SPREAD 8.0f     /* 簇内标准差，越小则簇越紧凑 */

/* 生成聚类数据：N_CLUSTERS 个聚类中心，每个向量属于某中心 + 小偏移。
 * 这种分布下最近邻非常明确，ANN 算法能有效剪枝。 */
static void hnsw_bench_generate_clustered_vectors(float *vectors, int32_t n, int32_t dims) {
    /* 先固定聚类中心（用单位球面分布，避免原点聚集） */
    std::vector<float> centers(N_CLUSTERS * dims);
    for (int32_t c = 0; c < N_CLUSTERS; c++) {
        float norm = 0.0f;
        for (int32_t d = 0; d < dims; d++) {
            /* 随机方向，半径 5~10 */
            float v = (float)rand() / RAND_MAX * 2.0f - 1.0f;
            centers[c * dims + d] = v;
            norm += v * v;
        }
        norm = sqrtf(norm);
        /* 归一化后乘以半径 (5 + cluster_id * 0.1) 使聚类中心分散在球壳上 */
        float radius = 5.0f + c * 0.1f;
        for (int32_t d = 0; d < dims; d++) {
            centers[c * dims + d] = centers[c * dims + d] / norm * radius;
        }
    }

    /* 每个向量分配到一个聚类，加上小偏移 */
    for (int32_t i = 0; i < n; i++) {
        int32_t cluster = rand() % N_CLUSTERS;
        for (int32_t d = 0; d < dims; d++) {
            /* 高斯分布偏移（Box-Muller） */
            float u1 = (float)rand() / RAND_MAX;
            float u2 = (float)rand() / RAND_MAX;
            float z = sqrtf(-2.0f * logf(u1 + 1e-10f)) * cosf(2.0f * M_PI * u2);
            vectors[i * dims + d] = centers[cluster * dims + d] + z * CLUSTER_SPREAD;
        }
    }
}

/* 计算近似搜索结果相对暴力搜索真值的召回率（k 个返回结果中的命中比例） */
static float hnsw_bench_compute_recall(const int32_t *result_ids,
                                       const int32_t *ground_truth_ids,
                                       int32_t k) {
    int32_t hit = 0;
    for (int32_t i = 0; i < k; i++) {
        for (int32_t j = 0; j < k; j++) {
            if (result_ids[i] == ground_truth_ids[j]) {
                hit++;
                break;
            }
        }
    }
    return (float)hit / (float)k;
}

/* 暴力搜索真值：按 L2 平方距离升序取前 k 个向量 id */
static void hnsw_bench_bruteforce_topk(const float *query,
                                       const float *vectors,
                                       int32_t n_vectors,
                                       int32_t dims,
                                       int32_t k,
                                       int32_t *out_ids) {
    std::vector<std::pair<float, int32_t>> scored;
    scored.reserve(n_vectors);
    for (int32_t i = 0; i < n_vectors; i++) {
        float dist = distance_l2sqr(query, &vectors[i * dims], dims);
        scored.emplace_back(dist, i);
    }
    std::sort(scored.begin(), scored.end());
    for (int32_t i = 0; i < k; i++) {
        out_ids[i] = scored[i].second;
    }
}

class HNSWBenchmark : public ::testing::Test {
protected:
    void SetUp() override {
        /* 固定随机种子以确保跨运行结果可复现 */
        srand(42);
        vectors_.resize(static_cast<size_t>(BENCHMARK_N_VECTORS) * BENCHMARK_DIMS);
        queries_.resize(static_cast<size_t>(BENCHMARK_N_QUERIES) * BENCHMARK_DIMS);
        hnsw_bench_generate_clustered_vectors(vectors_.data(), BENCHMARK_N_VECTORS, BENCHMARK_DIMS);
        hnsw_bench_generate_clustered_vectors(queries_.data(), BENCHMARK_N_QUERIES, BENCHMARK_DIMS);
    }

    std::vector<float> vectors_;
    std::vector<float> queries_;
};

/**
 * 测试：基本搜索功能
 * 验证：HNSW 索引可以正常创建、添加向量、搜索，
 *       平均查询延迟需低于 MAX_QUERY_LATENCY_MS 阈值。
 */
TEST_F(HNSWBenchmark, BasicSearch) {
    faiss_hnsw_t *index = faiss_hnsw_index_create(
        16,                  // M：每个节点的最大邻居数
        BENCHMARK_DIMS,
        200,                 // ef_construction：构建期搜索宽度
        DISTANCE_METRIC_L2_SQUARED,
        QUANTIZATION_TYPE_NONE
    );
    ASSERT_NE(index, nullptr);

    const int32_t kBatchSize = 1000;
    const int kRounds = (BENCHMARK_N_VECTORS + kBatchSize - 1) / kBatchSize;

    for (int round = 0; round < kRounds; round++) {
        int32_t start = round * kBatchSize;
        int32_t count = (start + kBatchSize <= BENCHMARK_N_VECTORS) ? kBatchSize : (BENCHMARK_N_VECTORS - start);
        int32_t ret = faiss_hnsw_index_add(index, count, &vectors_[start * BENCHMARK_DIMS]);
        ASSERT_EQ(ret, 0) << "faiss_hnsw_index_add 失败于 round=" << round;
    }
    EXPECT_EQ(index->n_total, BENCHMARK_N_VECTORS);

    int32_t result_ids[BENCHMARK_K];
    float result_dists[BENCHMARK_K];

    /* 计时批量查询 */
    auto start = std::chrono::high_resolution_clock::now();
    for (int32_t q = 0; q < BENCHMARK_N_QUERIES; q++) {
        faiss_hnsw_index_search(
            index,
            &queries_[q * BENCHMARK_DIMS],
            BENCHMARK_K,
            50,               // ef_search：搜索期动态候选宽度
            result_dists,
            result_ids
        );
    }
    auto end = std::chrono::high_resolution_clock::now();

    double latency_ms = std::chrono::duration<double, std::milli>(end - start).count() / BENCHMARK_N_QUERIES;
    printf("HNSW 平均查询延迟: %.3f ms\n", latency_ms);

    EXPECT_LT(latency_ms, MAX_QUERY_LATENCY_MS);

    faiss_hnsw_index_drop(index);
}

/**
 * 测试：召回率
 * 验证：与暴力搜索对比，平均召回率 >= HNSW_RECALL_THRESHOLD (0.90)。
 */
TEST_F(HNSWBenchmark, RecallTest) {
    faiss_hnsw_t *index = faiss_hnsw_index_create(
        16, BENCHMARK_DIMS, 200, DISTANCE_METRIC_L2_SQUARED, QUANTIZATION_TYPE_NONE
    );
    ASSERT_NE(index, nullptr);

    /* 批量添加向量（更高效） */
    int32_t add_ret = faiss_hnsw_index_add(index, BENCHMARK_N_VECTORS, vectors_.data());
    ASSERT_EQ(add_ret, 0);

    float total_recall = 0.0f;
    int32_t result_ids[BENCHMARK_K];
    float result_dists[BENCHMARK_K];
    int32_t ground_truth[BENCHMARK_K];

    for (int32_t q = 0; q < BENCHMARK_N_QUERIES; q++) {
        /* 近似搜索：使用更大的 ef_search 以提高召回率 */
        faiss_hnsw_index_search(
            index,
            &queries_[q * BENCHMARK_DIMS],
            BENCHMARK_K,
            100,              // ef_search：增加到 100
            result_dists,
            result_ids
        );

        /* 暴力搜索获取 ground truth */
        hnsw_bench_bruteforce_topk(
            &queries_[q * BENCHMARK_DIMS],
            vectors_.data(),
            BENCHMARK_N_VECTORS,
            BENCHMARK_DIMS,
            BENCHMARK_K,
            ground_truth
        );

        total_recall += hnsw_bench_compute_recall(result_ids, ground_truth, BENCHMARK_K);
    }

    float avg_recall = total_recall / static_cast<float>(BENCHMARK_N_QUERIES);
    printf("HNSW 平均召回率: %.3f (阈值: %.2f)\n", avg_recall, HNSW_RECALL_THRESHOLD);

    EXPECT_GE(avg_recall, HNSW_RECALL_THRESHOLD);

    faiss_hnsw_index_drop(index);
}

/**
 * 测试：参数扫描
 * 验证：M=8/16/32 三个取值下索引均能构建并搜索，
 *       且查询延迟均低于 MAX_QUERY_LATENCY_MS 阈值。
 */
TEST_F(HNSWBenchmark, ParameterSweep) {
    const int m_values[] = {8, 16, 32};
    const int n_m = sizeof(m_values) / sizeof(m_values[0]);

    for (int mi = 0; mi < n_m; mi++) {
        const int M = m_values[mi];

        faiss_hnsw_t *index = faiss_hnsw_index_create(
            M, BENCHMARK_DIMS, 200, DISTANCE_METRIC_L2_SQUARED, QUANTIZATION_TYPE_NONE
        );
        ASSERT_NE(index, nullptr);

        int32_t add_ret = faiss_hnsw_index_add(index, BENCHMARK_N_VECTORS, vectors_.data());
        ASSERT_EQ(add_ret, 0);

        int32_t result_ids[BENCHMARK_K];
        float result_dists[BENCHMARK_K];

        /* 计时批量查询 */
        auto start = std::chrono::high_resolution_clock::now();
        for (int32_t q = 0; q < BENCHMARK_N_QUERIES; q++) {
            faiss_hnsw_index_search(
                index,
                &queries_[q * BENCHMARK_DIMS],
                BENCHMARK_K,
                50,
                result_dists,
                result_ids
            );
        }
        auto end = std::chrono::high_resolution_clock::now();
        double latency_ms = std::chrono::duration<double, std::milli>(end - start).count() / BENCHMARK_N_QUERIES;

        printf("M=%d: 延迟=%.3f ms\n", M, latency_ms);
        EXPECT_LT(latency_ms, MAX_QUERY_LATENCY_MS);

        faiss_hnsw_index_drop(index);
    }
}