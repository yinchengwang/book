// DiskANN 召回率简单测试
// 独立运行，不需要完整的集成测试框架

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>
#include <chrono>

extern "C" {
#include "db/index/vector_index/diskann/diskann.h"
#include "algo-prod/distance/distance.h"
}

// 测试规模
const int N_VECTORS = 5000;
const int DIMS = 64;
const int N_QUERIES = 50;
const int K = 10;
const int SEARCH_L = 50;

// 生成随机向量
void generate_random_vectors(float *vectors, int n, int dims) {
    for (int i = 0; i < n * dims; i++) {
        vectors[i] = (float)rand() / RAND_MAX;
    }
}

// 计算召回率
float compute_recall(const int32_t *result_ids, const int32_t *ground_truth, int k) {
    int hits = 0;
    for (int i = 0; i < k; i++) {
        for (int j = 0; j < k; j++) {
            if (result_ids[i] == ground_truth[j]) {
                hits++;
                break;
            }
        }
    }
    return (float)hits / (float)k;
}

// 暴力搜索获取真值
void bruteforce_topk(const float *query, const float *vectors, int n, int dims,
                    int k, int32_t *out_ids) {
    std::vector<std::pair<float, int>> scored;
    scored.reserve(n);
    for (int i = 0; i < n; i++) {
        float dist = distance_l2sqr(query, &vectors[i * dims], dims);
        scored.emplace_back(dist, i);
    }
    std::sort(scored.begin(), scored.end());
    for (int i = 0; i < k && i < (int)scored.size(); i++) {
        out_ids[i] = scored[i].second;
    }
}

int main() {
    printf("=== DiskANN 召回率测试 ===\n");
    printf("规模: %d 向量, %d 维度, %d 查询\n\n", N_VECTORS, DIMS, N_QUERIES);

    srand(42);

    // 分配内存
    float *vectors = (float *)malloc(N_VECTORS * DIMS * sizeof(float));
    float *queries = (float *)malloc(N_QUERIES * DIMS * sizeof(float));
    int32_t *result_ids = (int32_t *)malloc(K * sizeof(int32_t));
    float *result_dists = (float *)malloc(K * sizeof(float));
    int32_t *ground_truth = (int32_t *)malloc(K * sizeof(int32_t));

    if (!vectors || !queries || !result_ids || !result_dists || !ground_truth) {
        printf("内存分配失败\n");
        return 1;
    }

    // 生成测试数据
    printf("生成测试数据...\n");
    generate_random_vectors(vectors, N_VECTORS, DIMS);
    generate_random_vectors(queries, N_QUERIES, DIMS);

    // 创建索引
    printf("创建索引...\n");
    diskann_t *index = diskann_index_create(
        DIMS,       // dims
        16,         // R: 目标邻居数
        50,         // L: 构图候选数
        DISTANCE_METRIC_L2_SQUARED);

    if (!index) {
        printf("索引创建失败\n");
        return 1;
    }

    // 添加向量
    printf("添加 %d 个向量...\n", N_VECTORS);
    if (diskann_index_add(index, N_VECTORS, vectors) != 0) {
        printf("添加向量失败\n");
        return 1;
    }

    // 构建索引
    printf("构建索引...\n");
    auto build_start = std::chrono::high_resolution_clock::now();
    if (diskann_index_build(index) != 0) {
        printf("索引构建失败\n");
        return 1;
    }
    auto build_end = std::chrono::high_resolution_clock::now();
    double build_ms = std::chrono::duration<double, std::milli>(build_end - build_start).count();
    printf("构建耗时: %.2f ms\n", build_ms);

    // 搜索测试
    printf("\n执行 %d 次搜索...\n", N_QUERIES);
    float total_recall = 0.0f;
    double total_latency = 0.0;

    for (int q = 0; q < N_QUERIES; q++) {
        // 近似搜索
        auto search_start = std::chrono::high_resolution_clock::now();
        diskann_index_search(
            index,
            &queries[q * DIMS],
            K,
            SEARCH_L,
            10,  // max_iterations
            result_dists,
            result_ids
        );
        auto search_end = std::chrono::high_resolution_clock::now();
        total_latency += std::chrono::duration<double, std::micro>(search_end - search_start).count();

        // 暴力搜索获取真值
        bruteforce_topk(&queries[q * DIMS], vectors, N_VECTORS, DIMS, K, ground_truth);

        // 计算召回率
        total_recall += compute_recall(result_ids, ground_truth, K);
    }

    float avg_recall = total_recall / N_QUERIES;
    double avg_latency_us = total_latency / N_QUERIES;

    printf("\n=== 测试结果 ===\n");
    printf("平均召回率: %.3f (阈值: 0.40)\n", avg_recall);
    printf("平均延迟: %.2f us (%.3f ms)\n", avg_latency_us, avg_latency_us / 1000.0);

    // 验证
    bool passed = avg_recall >= 0.40f;
    printf("测试: %s\n", passed ? "通过" : "失败");

    // 持久化测试
    printf("\n=== 持久化测试 ===\n");
    const char *test_file = "test_diskann.bin";
    if (diskann_index_save(index, test_file) == 0) {
        printf("保存成功: %s\n", test_file);

        diskann_index_drop(index);

        printf("加载索引...\n");
        index = diskann_index_load(test_file);
        if (index) {
            printf("加载成功!\n");
        }
        remove(test_file);
    }

    // 清理
    diskann_index_drop(index);
    free(vectors);
    free(queries);
    free(result_ids);
    free(result_dists);
    free(ground_truth);

    printf("\n测试完成。\n");
    return passed ? 0 : 1;
}
