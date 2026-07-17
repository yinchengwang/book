// IVF 基准测试
//
// 验证 IVF (Inverted File Index) 索引的召回率、查询延迟。
// 作为 TEST_F 用例加入 engineering/test/db/index/integration/integration_test 二进制，
// 不携带独立的 main()，复用 integration_test.cpp 的测试入口。

#include <gtest/gtest.h>
#include <vector>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <algorithm>

extern "C" {
#include "benchmark_config.h"
#include "db/index/vector_index/ivf/IndexIVF.h"
#include "algo-prod/distance/distance.h"
}

/* 局部辅助函数：integration_test.cpp 中同名函数为 static，
 * 跨翻译单元不可见；此处内联等价实现以保持基准测试自包含。 */

/* 生成 [0, 1) 区间均匀分布的随机向量 */
static void ivf_bench_generate_random_vectors(float *vectors, int32_t n, int32_t dims) {
    for (int32_t i = 0; i < n * dims; i++) {
        vectors[i] = (float)rand() / RAND_MAX;
    }
}

/* 计算近似搜索结果相对暴力搜索真值的召回率（k 个返回结果中的命中比例） */
static float ivf_bench_compute_recall(const int32_t *result_ids,
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
static void ivf_bench_bruteforce_topk(const float *query,
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

class IVFBenchmark : public ::testing::Test {
protected:
    void SetUp() override {
        vectors_.resize(static_cast<size_t>(BENCHMARK_N_VECTORS) * BENCHMARK_DIMS);
        queries_.resize(static_cast<size_t>(BENCHMARK_N_QUERIES) * BENCHMARK_DIMS);
        ivf_bench_generate_random_vectors(vectors_.data(), BENCHMARK_N_VECTORS, BENCHMARK_DIMS);
        ivf_bench_generate_random_vectors(queries_.data(), BENCHMARK_N_QUERIES, BENCHMARK_DIMS);
    }

    std::vector<float> vectors_;
    std::vector<float> queries_;
};

/**
 * 测试：基本搜索功能
 * 验证：IVF 索引可以正常创建、训练、批量添加向量、搜索，
 *       平均查询延迟需低于 MAX_QUERY_LATENCY_MS 阈值。
 */
TEST_F(IVFBenchmark, BasicSearch) {
    faiss_ivf_t *index = faiss_ivf_index_create(
        BENCHMARK_DIMS,  // dims：向量维度
        100,             // nlist：一级簇数量
        10,              // nlist2：二级桶数量
        20,              // nprobe：搜索时访问的簇数
        DISTANCE_METRIC_L2_SQUARED,
        QUANTIZATION_TYPE_NONE
    );
    ASSERT_NE(index, nullptr);

    /* 训练：基于样本向量学习聚类中心 */
    faiss_ivf_index_train(index, BENCHMARK_N_VECTORS, vectors_.data());
    ASSERT_TRUE(faiss_ivf_index_is_trained(index));

    /* 批量添加向量 */
    faiss_ivf_index_add(index, BENCHMARK_N_VECTORS, vectors_.data());

    int32_t result_ids[BENCHMARK_K];
    float result_dists[BENCHMARK_K];

    /* 计时批量查询 */
    auto start = std::chrono::high_resolution_clock::now();
    for (int32_t q = 0; q < BENCHMARK_N_QUERIES; q++) {
        faiss_ivf_index_search(
            index,
            &queries_[q * BENCHMARK_DIMS],
            BENCHMARK_K,
            result_dists,
            result_ids
        );
    }
    auto end = std::chrono::high_resolution_clock::now();

    double latency_ms = std::chrono::duration<double, std::milli>(end - start).count() / static_cast<double>(BENCHMARK_N_QUERIES);
    printf("IVF 平均查询延迟: %.3f ms\n", latency_ms);

    EXPECT_LT(latency_ms, MAX_QUERY_LATENCY_MS);

    faiss_ivf_index_drop(index);
}

/**
 * 测试：召回率
 * 验证：与暴力搜索对比，平均召回率 >= IVF_RECALL_THRESHOLD。
 */
TEST_F(IVFBenchmark, RecallTest) {
    faiss_ivf_t *index = faiss_ivf_index_create(
        BENCHMARK_DIMS, 100, 10, 20,
        DISTANCE_METRIC_L2_SQUARED,
        QUANTIZATION_TYPE_NONE
    );
    ASSERT_NE(index, nullptr);

    faiss_ivf_index_train(index, BENCHMARK_N_VECTORS, vectors_.data());
    faiss_ivf_index_add(index, BENCHMARK_N_VECTORS, vectors_.data());

    float total_recall = 0.0f;
    int32_t result_ids[BENCHMARK_K];
    float result_dists[BENCHMARK_K];
    int32_t ground_truth[BENCHMARK_K];

    for (int32_t q = 0; q < BENCHMARK_N_QUERIES; q++) {
        /* 近似搜索 */
        faiss_ivf_index_search(
            index,
            &queries_[q * BENCHMARK_DIMS],
            BENCHMARK_K,
            result_dists,
            result_ids
        );

        /* 暴力搜索获取 ground truth */
        ivf_bench_bruteforce_topk(
            &queries_[q * BENCHMARK_DIMS],
            vectors_.data(),
            BENCHMARK_N_VECTORS,
            BENCHMARK_DIMS,
            BENCHMARK_K,
            ground_truth
        );

        total_recall += ivf_bench_compute_recall(result_ids, ground_truth, BENCHMARK_K);
    }

    float avg_recall = total_recall / static_cast<float>(BENCHMARK_N_QUERIES);
    printf("IVF 平均召回率: %.3f (阈值: %.2f)\n", avg_recall, IVF_RECALL_THRESHOLD);

    EXPECT_GE(avg_recall, IVF_RECALL_THRESHOLD);

    faiss_ivf_index_drop(index);
}
