/**
 * @file bq_recall_test.c
 * @brief BQ (Binary Quantization) 召回率测试
 *
 * 使用 SIFT Small 数据集测试 BQ 的召回率性能。
 * 期望召回率：≥ 0.90 @10K 数据集
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include "db/index/vector_index/bq/bq.h"

/* ============================================================
 * 数据集路径配置
 * ============================================================ */

#ifndef SIFT_DATA_DIR
#define SIFT_DATA_DIR "D:/code/book/engineering/test_data/sift"
#endif

#define SIFT_BASE_FILE    SIFT_DATA_DIR "/sift_base.fvecs"
#define SIFT_QUERY_FILE   SIFT_DATA_DIR "/sift_query.fvecs"
#define SIFT_GROUNDTRUTH_FILE SIFT_DATA_DIR "/sift_groundtruth.ivecs"

/* ============================================================
 * 工具函数
 * ============================================================ */

/**
 * @brief 获取当前时间（毫秒）
 */
static double get_time_ms(void)
{
#ifdef _WIN32
    return (double)GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
#endif
}

/**
 * @brief 读取 fvecs 文件（float 向量）
 */
static float *read_fvecs(const char *filename, int *n, int *d)
{
    FILE *f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Error: Cannot open file %s\n", filename);
        return NULL;
    }

    /* 读取维度 */
    int dims;
    if (fread(&dims, 4, 1, f) != 1) {
        fprintf(stderr, "Error: Cannot read dimensions from %s\n", filename);
        fclose(f);
        return NULL;
    }

    /* 获取文件大小 */
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 4, SEEK_SET);

    /* 计算向量数量 */
    *d = dims;
    *n = (int)(file_size / ((dims + 1) * 4));

    /* 分配内存 */
    float *data = (float *)malloc((*n) * (*d) * sizeof(float));
    if (!data) {
        fprintf(stderr, "Error: Cannot allocate memory for vectors\n");
        fclose(f);
        return NULL;
    }

    /* 读取所有向量（跳过每行的维度） */
    for (int i = 0; i < *n; i++) {
        int vec_dims;
        fread(&vec_dims, 4, 1, f);
        fread(&data[i * (*d)], 4, dims, f);
    }

    fclose(f);
    return data;
}

/**
 * @brief 读取 ivecs 文件（int 向量）
 */
static int *read_ivecs(const char *filename, int *n, int *d)
{
    FILE *f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Error: Cannot open file %s\n", filename);
        return NULL;
    }

    /* 读取维度 */
    int dims;
    if (fread(&dims, 4, 1, f) != 1) {
        fprintf(stderr, "Error: Cannot read dimensions from %s\n", filename);
        fclose(f);
        return NULL;
    }

    /* 获取文件大小 */
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 4, SEEK_SET);

    /* 计算向量数量 */
    *d = dims;
    *n = (int)(file_size / ((dims + 1) * 4));

    /* 分配内存 */
    int *data = (int *)malloc((*n) * (*d) * sizeof(int));
    if (!data) {
        fprintf(stderr, "Error: Cannot allocate memory for groundtruth\n");
        fclose(f);
        return NULL;
    }

    /* 读取所有向量 */
    for (int i = 0; i < *n; i++) {
        int vec_dims;
        fread(&vec_dims, 4, 1, f);
        fread(&data[i * (*d)], 4, dims, f);
    }

    fclose(f);
    return data;
}

/**
 * @brief 计算两个向量的 L2 距离
 */
static float compute_l2_distance(const float *a, const float *b, int d)
{
    float dist = 0.0f;
    for (int i = 0; i < d; i++) {
        float diff = a[i] - b[i];
        dist += diff * diff;
    }
    return sqrtf(dist);
}

/**
 * @brief 计算召回率
 *
 * @param k 查询的 Top-K
 * @param gt 地面真值（每查询的 Top-K 最近邻索引）
 * @param results 查询结果（每查询的 Top-K 索引）
 * @param nq 查询数量
 * @return 平均召回率
 */
static float compute_recall(int k, const int *gt, const int *results, int nq)
{
    int total_hits = 0;
    int total_expected = k * nq;

    for (int i = 0; i < nq; i++) {
        const int *gt_row = &gt[i * k];
        const int *res_row = &results[i * k];

        for (int j = 0; j < k; j++) {
            for (int t = 0; t < k; t++) {
                if (gt_row[j] == res_row[t]) {
                    total_hits++;
                    break;
                }
            }
        }
    }

    return (float)total_hits / (float)total_expected;
}

/**
 * @brief 使用 BQ 编码进行暴力搜索
 */
static int *bq_brute_force_search(const float *base,
                                   int nb,
                                   const uint8_t *codes,
                                   const float *queries,
                                   int nq,
                                   int d,
                                   int k,
                                   int code_size,
                                   float *query_time_ms)
{
    int *results = (int *)malloc(nq * k * sizeof(int));
    if (!results) return NULL;

    double start = get_time_ms();

    for (int i = 0; i < nq; i++) {
        const float *query = &queries[i * d];

        /* 计算与所有基向量的 Hamming 距离 */
        int *dists = (int *)malloc(nb * sizeof(int));
        if (!dists) {
            free(results);
            return NULL;
        }

        for (int j = 0; j < nb; j++) {
            dists[j] = bq_hamming_distance(&codes[j * code_size],
                                            codes,  /* query code */
                                            code_size);
        }

        /* 找 Top-K */
        for (int j = 0; j < k; j++) {
            int best_idx = 0;
            int best_dist = dists[0];

            for (int t = 1; t < nb; t++) {
                if (dists[t] < best_dist) {
                    best_dist = dists[t];
                    best_idx = t;
                }
            }

            results[i * k + j] = best_idx;
            dists[best_idx] = INT_MAX;  /* 排除已选中的 */
        }

        free(dists);
    }

    *query_time_ms = (float)(get_time_ms() - start);
    return results;
}

/* ============================================================
 * 主测试
 * ============================================================ */

int main(int argc, char **argv)
{
    printf("===========================================\n");
    printf("BQ (Binary Quantization) 召回率测试\n");
    printf("===========================================\n\n");

    /* 读取数据集 */
    printf("加载数据集...\n");

    int nb, db_d;
    float *base = read_fvecs(SIFT_BASE_FILE, &nb, &db_d);
    if (!base) {
        fprintf(stderr, "Error: Cannot load base vectors from %s\n", SIFT_BASE_FILE);
        fprintf(stderr, "请确保 SIFT 数据集存在于 %s\n", SIFT_DATA_DIR);
        return 1;
    }

    int nq, q_d;
    float *queries = read_fvecs(SIFT_QUERY_FILE, &nq, &q_d);
    if (!queries) {
        fprintf(stderr, "Error: Cannot load query vectors from %s\n", SIFT_QUERY_FILE);
        free(base);
        return 1;
    }

    int gt_k, gt_d;
    int *groundtruth = read_ivecs(SIFT_GROUNDTRUTH_FILE, &gt_k, &gt_d);
    if (!groundtruth) {
        fprintf(stderr, "Error: Cannot load groundtruth from %s\n", SIFT_GROUNDTRUTH_FILE);
        free(base);
        free(queries);
        return 1;
    }

    printf("数据集信息：\n");
    printf("  基向量数量: %d\n", nb);
    printf("  查询数量: %d\n", nq);
    printf("  向量维度: %d\n", db_d);
    printf("  地面真值 K: %d\n", gt_k);
    printf("\n");

    /* 创建 BQ 量化器并训练 */
    printf("训练 BQ 量化器...\n");

    bq_quantizer_t *bq = bq_create(db_d, BQ_THRESHOLD_MEAN);
    if (!bq) {
        fprintf(stderr, "Error: Cannot create BQ quantizer\n");
        free(base);
        free(queries);
        free(groundtruth);
        return 1;
    }

    double train_start = get_time_ms();

    /* 使用一部分基向量训练 */
    int n_train = nb < 10000 ? nb : 10000;
    if (bq_train(bq, n_train, base) != 0) {
        fprintf(stderr, "Error: Training failed\n");
        bq_destroy(bq);
        free(base);
        free(queries);
        free(groundtruth);
        return 1;
    }

    float train_time = (float)(get_time_ms() - train_start);
    printf("训练完成，耗时: %.2f ms\n", train_time);
    printf("训练样本数: %d\n", n_train);
    bq_print_info(bq);
    printf("\n");

    /* 编码所有基向量 */
    printf("编码基向量...\n");

    int code_size = bq_get_code_size(bq);
    uint8_t *codes = (uint8_t *)malloc(nb * code_size);
    if (!codes) {
        fprintf(stderr, "Error: Cannot allocate memory for codes\n");
        bq_destroy(bq);
        free(base);
        free(queries);
        free(groundtruth);
        return 1;
    }

    double encode_start = get_time_ms();
    int n_encoded = bq_encode_batch(bq, nb, base, codes);
    float encode_time = (float)(get_time_ms() - encode_start);

    printf("编码完成，耗时: %.2f ms\n", encode_time);
    printf("编码向量数: %d / %d\n", n_encoded, nb);
    printf("压缩率: %.2fx\n", bq_compression_ratio(db_d));
    printf("\n");

    /* 测试不同阈值策略 */
    printf("===========================================\n");
    printf("测试不同阈值策略的召回率\n");
    printf("===========================================\n\n");

    BqThresholdStrategy_t strategies[] = {
        BQ_THRESHOLD_MEAN,
        BQ_THRESHOLD_MEDIAN,
        BQ_THRESHOLD_ADAPTIVE,
        BQ_THRESHOLD_LEARNED
    };

    const char *strategy_names[] = {
        "MEAN",
        "MEDIAN",
        "ADAPTIVE",
        "LEARNED"
    };

    for (int s = 0; s < 4; s++) {
        printf("测试策略: %s\n", strategy_names[s]);

        /* 创建新的量化器并重新训练 */
        bq_quantizer_t *bq_test = bq_create(db_d, strategies[s]);
        if (!bq_test) continue;

        /* 使用相同的训练数据重新训练 */
        if (bq_train(bq_test, n_train, base) != 0) {
            bq_destroy(bq_test);
            continue;
        }

        /* 重新编码 */
        uint8_t *codes_test = (uint8_t *)malloc(nb * code_size);
        if (!codes_test) {
            bq_destroy(bq_test);
            continue;
        }
        bq_encode_batch(bq_test, nb, base, codes_test);

        /* 编码查询向量（用于 Hamming 距离） */
        uint8_t *query_codes = (uint8_t *)malloc(nq * code_size);
        if (!query_codes) {
            free(codes_test);
            bq_destroy(bq_test);
            continue;
        }
        bq_encode_batch(bq_test, nq, queries, query_codes);

        /* 执行搜索 */
        int *results = bq_brute_force_search(base, nb, codes_test,
                                              queries, nq, db_d, 10,
                                              code_size, &encode_time);
        if (!results) {
            free(query_codes);
            free(codes_test);
            bq_destroy(bq_test);
            continue;
        }

        /* 计算召回率 */
        float recall_at_1 = compute_recall(1, groundtruth, results, nq);
        float recall_at_10 = compute_recall(10, groundtruth, results, nq);
        float recall_at_100 = compute_recall(100, groundtruth, results, nq);

        printf("  Recall@1:  %.4f\n", recall_at_1);
        printf("  Recall@10: %.4f\n", recall_at_10);
        printf("  Recall@100: %.4f\n", recall_at_100);
        printf("\n");

        free(results);
        free(query_codes);
        free(codes_test);
        bq_destroy(bq_test);
    }

    /* 使用默认策略进行更详细的测试 */
    printf("===========================================\n");
    printf("详细召回率测试（Mean 策略）\n");
    printf("===========================================\n\n");

    /* 编码查询向量 */
    uint8_t *query_codes = (uint8_t *)malloc(nq * code_size);
    bq_encode_batch(bq, nq, queries, query_codes);

    /* 搜索 Top-1 */
    printf("测试 Recall@1...\n");
    int *results_1 = bq_brute_force_search(base, nb, codes,
                                            queries, nq, db_d, 1,
                                            code_size, &encode_time);
    if (results_1) {
        float recall = compute_recall(1, groundtruth, results_1, nq);
        printf("  Recall@1: %.4f\n", recall);
        free(results_1);
    }

    /* 搜索 Top-10 */
    printf("测试 Recall@10...\n");
    int *results_10 = bq_brute_force_search(base, nb, codes,
                                             queries, nq, db_d, 10,
                                             code_size, &encode_time);
    if (results_10) {
        float recall = compute_recall(10, groundtruth, results_10, nq);
        printf("  Recall@10: %.4f\n", recall);
        printf("  (目标: ≥ 0.90)\n");
        free(results_10);
    }

    /* 搜索 Top-100 */
    printf("测试 Recall@100...\n");
    int *results_100 = bq_brute_force_search(base, nb, codes,
                                              queries, nq, db_d, 100,
                                              code_size, &encode_time);
    if (results_100) {
        float recall = compute_recall(100, groundtruth, results_100, nq);
        printf("  Recall@100: %.4f\n", recall);
        free(results_100);
    }

    printf("\n");

    /* 清理资源 */
    free(query_codes);
    free(codes);
    bq_destroy(bq);
    free(base);
    free(queries);
    free(groundtruth);

    printf("===========================================\n");
    printf("测试完成\n");
    printf("===========================================\n");

    return 0;
}
