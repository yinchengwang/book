/*
 * kd_tree_recall.c
 *
 * KD-Tree 召回率测试 - 使用 SIFT Small 数据集
 *
 * KD-Tree 是精确最近邻搜索，召回率应接近 1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* KD-Tree 接口 */
#include "db/index/vector_index/kd_tree/kd_tree.h"
#include "algo-prod/distance/distance.h"

/* 距离计算：L2 平方距离 */
static inline float l2sqr(const float *a, const float *b, int dims) {
    float dist = 0.0f;
    for (int i = 0; i < dims; i++) {
        float d = a[i] - b[i];
        dist += d * d;
    }
    return dist;
}

/* 读取头文件获取 count 和 dims */
static int read_header(const char *bin_path, int *out_count, int *out_dims) {
    char header_path[512];
    snprintf(header_path, sizeof(header_path), "%s", bin_path);
    char *dot = strrchr(header_path, '.');
    if (dot) {
        strcpy(dot, "_header.txt");
    } else {
        strcat(header_path, "_header.txt");
    }

    FILE *f = fopen(header_path, "r");
    if (!f) {
        fprintf(stderr, "Warning: header file not found: %s\n", header_path);
        return -1;
    }

    char line[256];
    *out_count = 0;
    *out_dims = 0;

    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "count=%d", out_count) == 1) continue;
        if (sscanf(line, "dims=%d", out_dims) == 1) continue;
    }

    fclose(f);

    if (*out_count == 0 || *out_dims == 0) {
        fprintf(stderr, "Error: invalid header file\n");
        return -1;
    }

    return 0;
}

/* 读取纯二进制格式（float32 数组）*/
static float *read_binary_fvecs(const char *path, int *out_count, int *out_dims) {
    /* 先读取头文件 */
    if (read_header(path, out_count, out_dims) != 0) {
        return NULL;
    }

    /* 读取二进制数据 */
    FILE *f = fopen(path, "rb");
    if (!f) {
        perror("fopen");
        return NULL;
    }

    size_t total_floats = (size_t)(*out_count) * (*out_dims);
    float *vectors = (float *)malloc(total_floats * sizeof(float));
    if (!vectors) {
        fclose(f);
        return NULL;
    }

    size_t read_count = fread(vectors, sizeof(float), total_floats, f);
    fclose(f);

    if (read_count != total_floats) {
        fprintf(stderr, "Error: read %zu floats, expected %zu\n", read_count, total_floats);
        free(vectors);
        return NULL;
    }

    printf("Read %s: %d vectors, dimension %d\n", path, *out_count, *out_dims);
    return vectors;
}

/* 读取纯二进制格式（int32 数组，用于 groundtruth）*/
static int32_t *read_binary_groundtruth(const char *path, int *out_count, int *out_k) {
    /* 先读取头文件 */
    int dims;
    if (read_header(path, out_count, &dims) != 0) {
        return NULL;
    }
    *out_k = dims;

    /* 读取二进制数据 */
    FILE *f = fopen(path, "rb");
    if (!f) {
        perror("fopen");
        return NULL;
    }

    size_t total_ints = (size_t)(*out_count) * (*out_k);
    int32_t *gt = (int32_t *)malloc(total_ints * sizeof(int32_t));
    if (!gt) {
        fclose(f);
        return NULL;
    }

    size_t read_count = fread(gt, sizeof(int32_t), total_ints, f);
    fclose(f);

    if (read_count != total_ints) {
        fprintf(stderr, "Error: read %zu ints, expected %zu\n", read_count, total_ints);
        free(gt);
        return NULL;
    }

    printf("Read %s: %d queries, K=%d\n", path, *out_count, *out_k);
    return gt;
}

/* 计算召回率 (Recall@K) */
static float compute_recall_at_k(const int32_t *result, const int32_t *gt, int k) {
    int hit = 0;
    for (int i = 0; i < k; i++) {
        for (int j = 0; j < k; j++) {
            if (result[i] == gt[j]) { hit++; break; }
        }
    }
    return (float)hit / k;
}

int main(int argc, char *argv[]) {
    /* 默认使用 siftsmall（10K 向量）*/
    const char *data_dir = "c:/code/book/dataset/siftsmall";

    /* 允许命令行覆盖 */
    if (argc > 1) {
        data_dir = argv[1];
    }

    printf("=== KD-Tree SIFT Recall Test ===\n\n");
    printf("Dataset: %s\n\n", data_dir);

    /* 构建文件路径 */
    char base_path[512], query_path[512], gt_path[512];
    snprintf(base_path, sizeof(base_path), "%s/base.bin", data_dir);
    snprintf(query_path, sizeof(query_path), "%s/query.bin", data_dir);
    snprintf(gt_path, sizeof(gt_path), "%s/groundtruth.bin", data_dir);

    /* 读取数据 */
    int n_base, dims, n_query, gt_k;
    float *base = read_binary_fvecs(base_path, &n_base, &dims);
    float *query = read_binary_fvecs(query_path, &n_query, &dims);
    int32_t *gt = read_binary_groundtruth(gt_path, &n_query, &gt_k);

    if (!base || !query || !gt) {
        fprintf(stderr, "Failed to read data\n");
        return 1;
    }

    printf("\nDataset summary:\n");
    printf("  Base: %d vectors, dimension %d\n", n_base, dims);
    printf("  Query: %d vectors\n", n_query);
    printf("  Ground Truth: K=%d\n\n", gt_k);

    /* 构建 KD-Tree 索引 */
    printf("Building KD-Tree index (dims=%d)...\n", dims);
    kd_tree_t *index = kd_tree_create(dims);
    if (!index) {
        fprintf(stderr, "Failed to create KD-Tree index\n");
        free(base);
        free(query);
        free(gt);
        return 1;
    }

    /* 构造 ids 数组 */
    int *ids = (int *)malloc(n_base * sizeof(int));
    if (!ids) {
        fprintf(stderr, "Failed to allocate ids array\n");
        kd_tree_destroy(index);
        free(base);
        free(query);
        free(gt);
        return 1;
    }
    for (int i = 0; i < n_base; i++) {
        ids[i] = i;
    }

    /* 批量构建索引 */
    int ret = kd_tree_build(index, n_base, base, ids);
    if (ret != 0) {
        fprintf(stderr, "Failed to build KD-Tree index: %d\n", ret);
        kd_tree_destroy(index);
        free(ids);
        free(base);
        free(query);
        free(gt);
        return 1;
    }

    printf("Index built, total items: %d, total nodes: %d\n\n",
           kd_tree_get_n_items(index), kd_tree_get_n_nodes(index));

    /* 测试不同 K 值的召回率 */
    printf("Testing recall (KD-Tree is exact, expect recall ~1.0)...\n");

    int k_values[] = {1, 10, 50};
    int n_k = sizeof(k_values) / sizeof(k_values[0]);

    for (int tk = 0; tk < n_k; tk++) {
        int k = k_values[tk];
        if (k > gt_k) k = gt_k;

        float total_recall = 0.0f;
        for (int q = 0; q < n_query; q++) {
            float dists[100];
            int32_t ids_out[100];
            memset(ids_out, -1, sizeof(ids_out));
            memset(dists, 0, sizeof(dists));

            int n_found = kd_tree_search(index, &query[q * dims], k, ids_out, dists);
            if (n_found < k) {
                /* 如果返回结果不足，继续处理但记录警告 */
                static int warned = 0;
                if (!warned) {
                    fprintf(stderr, "Warning: kd_tree_search returned %d results, expected %d\n",
                            n_found, k);
                    warned = 1;
                }
            }

            float recall = compute_recall_at_k(ids_out, &gt[q * gt_k], k);
            total_recall += recall;
        }
        float avg_recall = total_recall / n_query;
        printf("  Recall@%d: %.4f\n", k, avg_recall);
    }

    /* 清理 */
    kd_tree_destroy(index);
    free(ids);
    free(base);
    free(query);
    free(gt);

    printf("\nTest completed!\n");
    return 0;
}