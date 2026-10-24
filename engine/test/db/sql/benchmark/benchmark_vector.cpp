/**
 * @file benchmark_vector.cpp
 * @brief 向量索引性能基准测试
 *
 * 测试场景：
 * 1. SearchK10 - 向量搜索 K=10（目标 QPS > 1000）
 * 2. SearchK100 - 向量搜索 K=100
 */

#include <gtest/gtest.h>
#include <vector>
#include <cmath>
#include <algorithm>

#include "benchmark_config.h"

extern "C" {
#include "db/index/vector_index/hnsw/faiss_hnsw.h"
#include "algo-prod/distance/distance.h"
}

namespace {

/* 向量维度 */
constexpr int32_t VECTOR_DIMS = 128;

/* 测试向量数量 */
constexpr int32_t N_VECTORS = 10000;

/* 查询向量数量 */
constexpr int32_t N_QUERIES = 100;

/* HNSW 参数 */
constexpr int32_t HNSW_M = 16;
constexpr int32_t HNSW_EF_CONSTRUCTION = 200;
constexpr int32_t HNSW_EF_SEARCH = 64;

/**
 * @brief 生成随机向量
 */
static void generate_random_vectors(float *vectors, int32_t n, int32_t dims) {
    for (int32_t i = 0; i < n * dims; i++) {
        vectors[i] = static_cast<float>(rand()) / RAND_MAX;
    }
}

/**
 * @brief 向量基准测试环境
 */
class BenchmarkVectorTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* 固定随机种子 */
        srand(42);

        /* 分配向量内存 */
        vectors_.resize(static_cast<size_t>(N_VECTORS) * VECTOR_DIMS);
        queries_.resize(static_cast<size_t>(N_QUERIES) * VECTOR_DIMS);

        /* 生成随机向量 */
        generate_random_vectors(vectors_.data(), N_VECTORS, VECTOR_DIMS);
        generate_random_vectors(queries_.data(), N_QUERIES, VECTOR_DIMS);
    }

    void TearDown() override {
        vectors_.clear();
        queries_.clear();
    }

    std::vector<float> vectors_;
    std::vector<float> queries_;
};

/**
 * @brief 基准测试: 向量搜索 K=10
 *
 * 目标：QPS > 1000（平均延迟 < 1ms）
 */
TEST_F(BenchmarkVectorTest, SearchK10) {
    const int32_t k = 10;

    /* 创建 HNSW 索引 */
    faiss_hnsw_t *index = faiss_hnsw_index_create(
        HNSW_M,
        VECTOR_DIMS,
        HNSW_EF_CONSTRUCTION,
        DISTANCE_METRIC_L2_SQUARED,
        QUANTIZATION_TYPE_NONE
    );
    ASSERT_NE(index, nullptr) << "HNSW 索引创建失败";

    /* 添加向量 */
    for (int32_t i = 0; i < N_VECTORS; i++) {
        int32_t ret = faiss_hnsw_index_add(index, 1, &vectors_[i * VECTOR_DIMS]);
        ASSERT_EQ(ret, 0) << "添加向量失败: i=" << i;
    }

    /* 准备结果缓冲区 */
    std::vector<int32_t> result_ids(N_QUERIES * k);
    std::vector<float> result_distances(N_QUERIES * k);

    /* 测量搜索性能 */
    BenchmarkTimer timer;
    timer.Start();

    for (int32_t i = 0; i < N_QUERIES; i++) {
        faiss_hnsw_index_search(
            index,
            &queries_[i * VECTOR_DIMS],
            k,
            HNSW_EF_SEARCH,
            &result_distances[i * k],
            &result_ids[i * k]
        );
    }

    timer.Stop();

    /* 计算性能指标 */
    double duration_ms = timer.GetElapsedMs();
    double avg_latency_ms = duration_ms / N_QUERIES;
    double qps = (N_QUERIES * 1000.0) / duration_ms;

    printf("向量搜索 K=10 结果:\n");
    printf("  总耗时: %.3f ms\n", duration_ms);
    printf("  平均延迟: %.3f ms\n", avg_latency_ms);
    printf("  QPS: %.0f\n", qps);

    /* 性能目标：QPS > 1000（平均延迟 < 1ms） */
    EXPECT_GT(qps, 1000.0) << "QPS 未达标（目标 > 1000）";

    /* 清理 */
    faiss_hnsw_index_drop(index);
}

/**
 * @brief 基准测试: 向量搜索 K=100
 *
 * 目标：测试大批量结果返回的性能
 */
TEST_F(BenchmarkVectorTest, SearchK100) {
    const int32_t k = 100;

    /* 创建 HNSW 索引 */
    faiss_hnsw_t *index = faiss_hnsw_index_create(
        HNSW_M,
        VECTOR_DIMS,
        HNSW_EF_CONSTRUCTION,
        DISTANCE_METRIC_L2_SQUARED,
        QUANTIZATION_TYPE_NONE
    );
    ASSERT_NE(index, nullptr) << "HNSW 索引创建失败";

    /* 添加向量 */
    for (int32_t i = 0; i < N_VECTORS; i++) {
        int32_t ret = faiss_hnsw_index_add(index, 1, &vectors_[i * VECTOR_DIMS]);
        ASSERT_EQ(ret, 0) << "添加向量失败: i=" << i;
    }

    /* 准备结果缓冲区 */
    std::vector<int32_t> result_ids(N_QUERIES * k);
    std::vector<float> result_distances(N_QUERIES * k);

    /* 测量搜索性能 */
    BenchmarkTimer timer;
    timer.Start();

    for (int32_t i = 0; i < N_QUERIES; i++) {
        faiss_hnsw_index_search(
            index,
            &queries_[i * VECTOR_DIMS],
            k,
            HNSW_EF_SEARCH,
            &result_distances[i * k],
            &result_ids[i * k]
        );
    }

    timer.Stop();

    /* 计算性能指标 */
    double duration_ms = timer.GetElapsedMs();
    double avg_latency_ms = duration_ms / N_QUERIES;
    double qps = (N_QUERIES * 1000.0) / duration_ms;

    printf("向量搜索 K=100 结果:\n");
    printf("  总耗时: %.3f ms\n", duration_ms);
    printf("  平均延迟: %.3f ms\n", avg_latency_ms);
    printf("  QPS: %.0f\n", qps);

    /* 验证返回结果数量 */
    bool all_valid = true;
    for (int32_t i = 0; i < N_QUERIES; i++) {
        for (int32_t j = 0; j < k; j++) {
            if (result_ids[i * k + j] < 0 || result_ids[i * k + j] >= N_VECTORS) {
                all_valid = false;
                break;
            }
        }
    }
    EXPECT_TRUE(all_valid) << "搜索结果包含无效 ID";

    /* 清理 */
    faiss_hnsw_index_drop(index);
}

}  // namespace