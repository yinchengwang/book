/**
 * @file gpu_benchmark.cpp
 * @brief GPU 向量索引性能基准测试
 *
 * 测试 GPU-HNSW、GPU-IVF、GPU-IVF-PQ 索引的搜索性能。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <vector>
#include "db/index/vector_index/gpu/gpu_vector_index.h"

/* ========================================================================
 * 工具函数
 * ======================================================================== */

/**
 * @brief 获取当前时间（微秒）
 */
static inline double get_time_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000.0 + ts.tv_nsec / 1000.0;
}

/**
 * @brief 生成随机向量
 */
static void generate_random_vectors(float *vectors, int32_t n, int32_t dim)
{
    for (int i = 0; i < n * dim; i++) {
        vectors[i] = (float)rand() / RAND_MAX;
    }
}

/**
 * @brief 打印统计信息
 */
static void print_stats(const char *name, int32_t n, int32_t dim, int32_t k,
                        double total_time, int32_t iterations)
{
    double avg_time = total_time / iterations;
    double qps = 1000000.0 / avg_time;  /* 每秒查询数 */

    printf("\n=== %s 性能统计 ===\n", name);
    printf("向量维度: %d\n", dim);
    printf("向量数量: %d\n", n);
    printf("Top-K: %d\n", k);
    printf("测试迭代: %d\n", iterations);
    printf("总耗时: %.2f ms\n", total_time / 1000.0);
    printf("平均延迟: %.3f ms\n", avg_time / 1000.0);
    printf("QPS: %.0f\n", qps);
    printf("====================\n\n");
}

/* ========================================================================
 * HNSW 基准测试
 * ======================================================================== */

static void benchmark_hnsw(int32_t n, int32_t dim, int32_t k, int32_t iterations)
{
    printf("\n>>> GPU-HNSW 基准测试\n");

    /* 创建索引 */
    gpu_hnsw_config_t config;
    config.dim = dim;
    config.M = 16;
    config.ef_construction = 200;
    config.ef_search = 100;
    config.max_elements = n + iterations;  /* 预留空间 */
    config.metric = 0;

    gpu_hnsw_index_t *index = gpu_hnsw_create(&config);
    if (index == NULL) {
        printf("HNSW 索引创建失败\n");
        return;
    }

    /* 生成并插入向量 */
    std::vector<float> vectors(n * dim);
    generate_random_vectors(vectors.data(), n, dim);

    double start = get_time_us();
    int32_t inserted = gpu_hnsw_insert(index, vectors.data(), n, NULL);
    double end = get_time_us();

    printf("插入 %d 个向量，耗时 %.2f ms\n", inserted, (end - start) / 1000.0);

    /* 生成查询向量 */
    std::vector<float> queries(iterations * dim);
    for (int i = 0; i < iterations * dim; i++) {
        queries[i] = vectors[i];  /* 使用已有向量作为查询 */
    }

    /* 预热 */
    gpu_search_results_t *warmup = gpu_hnsw_search(index, queries.data(), k);
    if (warmup) gpu_free_results(warmup);

    /* 基准测试 */
    double total_time = 0.0;
    for (int i = 0; i < iterations; i++) {
        start = get_time_us();
        gpu_search_results_t *results = gpu_hnsw_search(
            index, queries.data() + i * dim, k);
        end = get_time_us();

        if (results) {
            total_time += (end - start);
            gpu_free_results(results);
        }
    }

    print_stats("GPU-HNSW", n, dim, k, total_time, iterations);

    gpu_hnsw_destroy(index);
}

/* ========================================================================
 * IVF 基准测试
 * ======================================================================== */

static void benchmark_ivf(int32_t n, int32_t dim, int32_t k, int32_t iterations)
{
    printf("\n>>> GPU-IVF 基准测试\n");

    /* 创建索引 */
    gpu_ivf_config_t config;
    config.dim = dim;
    config.nlist = 100;  /* 聚类中心数 */
    config.nprobe = 10;  /* 探针数 */
    config.max_elements = n + iterations;
    config.metric = 0;

    gpu_ivf_index_t *index = gpu_ivf_create(&config);
    if (index == NULL) {
        printf("IVF 索引创建失败\n");
        return;
    }

    /* 生成并训练 */
    std::vector<float> vectors(n * dim);
    generate_random_vectors(vectors.data(), n, dim);

    double start = get_time_us();
    gpu_ivf_train(index, vectors.data(), n);
    int32_t inserted = gpu_ivf_insert(index, vectors.data(), n, NULL);
    double end = get_time_us();

    printf("训练并插入 %d 个向量，耗时 %.2f ms\n", inserted, (end - start) / 1000.0);

    /* 生成查询向量 */
    std::vector<float> queries(iterations * dim);
    for (int i = 0; i < iterations * dim; i++) {
        queries[i] = vectors[i];
    }

    /* 预热 */
    gpu_search_results_t *warmup = gpu_ivf_search(index, queries.data(), k);
    if (warmup) gpu_free_results(warmup);

    /* 基准测试 */
    double total_time = 0.0;
    for (int i = 0; i < iterations; i++) {
        start = get_time_us();
        gpu_search_results_t *results = gpu_ivf_search(
            index, queries.data() + i * dim, k);
        end = get_time_us();

        if (results) {
            total_time += (end - start);
            gpu_free_results(results);
        }
    }

    print_stats("GPU-IVF", n, dim, k, total_time, iterations);

    gpu_ivf_destroy(index);
}

/* ========================================================================
 * IVF-PQ 基准测试
 * ======================================================================== */

static void benchmark_ivf_pq(int32_t n, int32_t dim, int32_t k, int32_t iterations)
{
    printf("\n>>> GPU-IVF-PQ 基准测试\n");

    /* 创建索引 */
    gpu_ivf_pq_config_t config;
    config.dim = dim;
    config.nlist = 100;
    config.nprobe = 10;
    config.pq_m = 8;       /* 8 个子空间 */
    config.pq_nbits = 8;   /* 256 个聚类中心 */
    config.max_elements = n + iterations;
    config.metric = 0;

    gpu_ivf_pq_index_t *index = gpu_ivf_pq_create(&config);
    if (index == NULL) {
        printf("IVF-PQ 索引创建失败\n");
        return;
    }

    /* 生成并训练 */
    std::vector<float> vectors(n * dim);
    generate_random_vectors(vectors.data(), n, dim);

    double start = get_time_us();
    gpu_ivf_pq_train(index, vectors.data(), n);
    int32_t inserted = gpu_ivf_pq_insert(index, vectors.data(), n, NULL);
    double end = get_time_us();

    printf("训练并插入 %d 个向量，耗时 %.2f ms\n", inserted, (end - start) / 1000.0);

    /* 生成查询向量 */
    std::vector<float> queries(iterations * dim);
    for (int i = 0; i < iterations * dim; i++) {
        queries[i] = vectors[i];
    }

    /* 预热 */
    gpu_search_results_t *warmup = gpu_ivf_pq_search(index, queries.data(), k);
    if (warmup) gpu_free_results(warmup);

    /* 基准测试 */
    double total_time = 0.0;
    for (int i = 0; i < iterations; i++) {
        start = get_time_us();
        gpu_search_results_t *results = gpu_ivf_pq_search(
            index, queries.data() + i * dim, k);
        end = get_time_us();

        if (results) {
            total_time += (end - start);
            gpu_free_results(results);
        }
    }

    print_stats("GPU-IVF-PQ", n, dim, k, total_time, iterations);

    gpu_ivf_pq_destroy(index);
}

/* ========================================================================
 * 主函数
 * ======================================================================== */

int main(int argc, char *argv[])
{
    /* 解析参数 */
    int32_t n = 10000;       /* 向量数量 */
    int32_t dim = 128;       /* 向量维度 */
    int32_t k = 10;          /* Top-K */
    int32_t iterations = 100; /* 测试迭代次数 */

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            n = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            dim = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-k") == 0 && i + 1 < argc) {
            k = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-i") == 0 && i + 1 < argc) {
            iterations = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-h") == 0) {
            printf("用法: %s [-n 向量数] [-d 维度] [-k Top-K] [-i 迭代次数]\n", argv[0]);
            printf("默认值: -n 10000 -d 128 -k 10 -i 100\n");
            return 0;
        }
    }

    printf("==============================================\n");
    printf("     GPU 向量索引性能基准测试\n");
    printf("==============================================\n");
    printf("配置: n=%d, dim=%d, k=%d, iterations=%d\n", n, dim, k, iterations);

    /* 初始化 GPU */
    gpu_init();

    /* 运行基准测试 */
    benchmark_hnsw(n, dim, k, iterations);
    benchmark_ivf(n, dim, k, iterations);
    benchmark_ivf_pq(n, dim, k, iterations);

    /* 清理 */
    gpu_shutdown();

    printf("\n基准测试完成\n");
    return 0;
}
