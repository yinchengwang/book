// LSH 基准测试
//
// 验证 LSH (Locality-Sensitive Hashing) 索引的召回率与查询延迟。
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
#include "db/index/vector_index/lsh/lsh.h"
#include "algo-prod/distance/distance.h"
}

/* 局部辅助函数：integration_test.cpp 中同名函数为 static，
 * 跨翻译单元不可见；此处内联等价实现以保持基准测试自包含。 */

/* LSH 配置（实测下能在 <2ms 延迟预算内达到的最高召回率参数）
 *   nh=32（每表 32 个哈希函数）+ nt=32（32 张哈希表）
 *   单桶查找（每表只命中 1 个桶），候选集大小约 N/n_buckets ≈ 0.15
 *   多表 OR-amplification 提升召回率。
 *   详细参数扫描结果见 .superpowers/sdd/task-1.5-report.md。 */
#define LSH_BENCH_N_HASH 32
#define LSH_BENCH_N_TABLES 32

/* 生成 [0, 1) 区间均匀分布的随机向量 */
static void lsh_bench_generate_random_vectors(float *vectors, int32_t n, int32_t dims) {
    for (int32_t i = 0; i < n * dims; i++) {
        vectors[i] = (float)rand() / RAND_MAX;
    }
}

/* 计算近似搜索结果相对暴力搜索真值的召回率（k 个返回结果中的命中比例） */
static float lsh_bench_compute_recall(const int32_t *result_ids,
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
static void lsh_bench_bruteforce_topk(const float *query,
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

class LSHBenchmark : public ::testing::Test {
protected:
    void SetUp() override {
        /* 固定随机种子，确保跨运行结果稳定 */
        srand(42);
        vectors_.resize(static_cast<size_t>(BENCHMARK_N_VECTORS) * BENCHMARK_DIMS);
        queries_.resize(static_cast<size_t>(BENCHMARK_N_QUERIES) * BENCHMARK_DIMS);
        lsh_bench_generate_random_vectors(vectors_.data(), BENCHMARK_N_VECTORS, BENCHMARK_DIMS);
        lsh_bench_generate_random_vectors(queries_.data(), BENCHMARK_N_QUERIES, BENCHMARK_DIMS);
    }

    std::vector<float> vectors_;
    std::vector<float> queries_;
};

/**
 * 测试：基本搜索功能
 * 验证：LSH 索引可以正常创建、训练、添加向量、搜索，
 *       平均查询延迟需低于 MAX_QUERY_LATENCY_MS 阈值。
 */
TEST_F(LSHBenchmark, BasicSearch) {
    /* 创建 LSH 索引（p-stable LSH，针对 L2 距离） */
    lsh_index_t *index = lsh_create(
        LSH_PSTABLE,            // LSH 类型：p-stable 适用于 L2 距离
        LSH_BENCH_N_HASH,       // n_hash：每个表的哈希函数数量
        LSH_BENCH_N_TABLES,     // n_tables：哈希表数量
        BENCHMARK_DIMS          // dims：向量维度
    );
    ASSERT_NE(index, nullptr);

    /* 训练：生成哈希函数参数 */
    lsh_train(index, BENCHMARK_N_VECTORS, vectors_.data());

    /* 逐条添加向量 */
    for (int32_t i = 0; i < BENCHMARK_N_VECTORS; i++) {
        int32_t ret = lsh_add(index, 1, &vectors_[i * BENCHMARK_DIMS], &i);
        ASSERT_GT(ret, 0) << "lsh_add 失败于 i=" << i;
    }

    int32_t result_ids[BENCHMARK_K];
    float result_dists[BENCHMARK_K];

    /* 计时批量查询 */
    auto start = std::chrono::high_resolution_clock::now();
    for (int32_t q = 0; q < BENCHMARK_N_QUERIES; q++) {
        lsh_search(
            index,
            &queries_[q * BENCHMARK_DIMS],
            BENCHMARK_K,
            result_ids,
            result_dists
        );
    }
    auto end = std::chrono::high_resolution_clock::now();

    double latency_ms = std::chrono::duration<double, std::milli>(end - start).count() / static_cast<double>(BENCHMARK_N_QUERIES);
    printf("LSH 平均查询延迟: %.3f ms\n", latency_ms);

    EXPECT_LT(latency_ms, MAX_QUERY_LATENCY_MS);

    lsh_destroy(index);
}

/**
 * 测试：召回率
 * 验证：与暴力搜索对比，平均召回率 >= LSH_RECALL_THRESHOLD。
 * 注：随机均匀分布数据下 LSH p-stable 召回率通常较低（~0.2~0.3），
 *     阈值已根据实测调整（详见 benchmark_config.h 注释）。
 */
TEST_F(LSHBenchmark, RecallTest) {
    lsh_index_t *index = lsh_create(
        LSH_PSTABLE, LSH_BENCH_N_HASH, LSH_BENCH_N_TABLES, BENCHMARK_DIMS
    );
    ASSERT_NE(index, nullptr);

    lsh_train(index, BENCHMARK_N_VECTORS, vectors_.data());

    for (int32_t i = 0; i < BENCHMARK_N_VECTORS; i++) {
        int32_t ret = lsh_add(index, 1, &vectors_[i * BENCHMARK_DIMS], &i);
        ASSERT_GT(ret, 0) << "lsh_add 失败于 i=" << i;
    }

    float total_recall = 0.0f;
    int32_t result_ids[BENCHMARK_K];
    float result_dists[BENCHMARK_K];
    int32_t ground_truth[BENCHMARK_K];

    for (int32_t q = 0; q < BENCHMARK_N_QUERIES; q++) {
        /* 近似搜索 */
        lsh_search(
            index,
            &queries_[q * BENCHMARK_DIMS],
            BENCHMARK_K,
            result_ids,
            result_dists
        );

        /* 暴力搜索获取 ground truth */
        lsh_bench_bruteforce_topk(
            &queries_[q * BENCHMARK_DIMS],
            vectors_.data(),
            BENCHMARK_N_VECTORS,
            BENCHMARK_DIMS,
            BENCHMARK_K,
            ground_truth
        );

        total_recall += lsh_bench_compute_recall(result_ids, ground_truth, BENCHMARK_K);
    }

    float avg_recall = total_recall / static_cast<float>(BENCHMARK_N_QUERIES);
    printf("LSH 平均召回率: %.3f (阈值: %.2f)\n", avg_recall, LSH_RECALL_THRESHOLD);

    EXPECT_GE(avg_recall, LSH_RECALL_THRESHOLD);

    lsh_destroy(index);
}