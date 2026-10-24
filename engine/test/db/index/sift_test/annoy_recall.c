/*
 * annoy_recall.c
 *
 * Annoy 召回率测试，使用 SIFT Small 数据集。
 *
 * 数据格式（二进制）：
 *   base.bin/query.bin: float32 数组 [count * dims]
 *   groundtruth.bin: int32 数组 [query_count * k]
 *   *_header.txt: count=xxx, dims=xxx, dtype=xxx
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Annoy 接口 */
#include "db/index/vector_index/annoy/annoy.h"
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

/* 读取 header 文件获取 count 和 dims */
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

/* 读取纯二进制格式（float32 数组） */
static float *read_binary_fvecs(const char *path, int *out_count, int *out_dims) {
    /* 先读取 header */
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

/* 读取纯二进制格式（int32 数组，用于 groundtruth） */
static int32_t *read_binary_groundtruth(const char *path, int *out_count, int *out_k) {
    /* 先读取 header */
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
    /* 默认使用 siftsmall（10K 向量） */
    const char *data_dir = "c:/code/book/dataset/siftsmall";

    /* 允许命令行覆盖 */
    if (argc > 1) {
        data_dir = argv[1];
    }

    printf("=== Annoy SIFT Recall Test ===\n\n");
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

    /* 测试不同的 n_trees 值 */
    int n_trees_values[] = {10, 30, 50};
    int n_nt = sizeof(n_trees_values) / sizeof(n_trees_values[0]);

    for (int t = 0; t < n_nt; t++) {
        int n_trees = n_trees_values[t];
        printf("--- n_trees = %d ---\n", n_trees);

        /* 创建 Annoy 索引 */
        annoy_index_t *idx = annoy_create(dims, "euclidean");
        if (!idx) {
            fprintf(stderr, "Failed to create Annoy index\n");
            free(base);
            free(query);
            free(gt);
            return 1;
        }

        /* 逐条添加向量 */
        printf("Adding %d vectors...\n", n_base);
        for (int i = 0; i < n_base; i++) {
            int ret = annoy_add_item(idx, i, &base[i * dims]);
            if (ret != 0) {
                fprintf(stderr, "Failed to add vector %d\n", i);
                annoy_destroy(idx);
                free(base);
                free(query);
                free(gt);
                return 1;
            }
        }
        printf("Added %d vectors\n", n_base);

        /* 构建索引（构建随机投影树） */
        printf("Building index with n_trees=%d...\n", n_trees);
        int ret = annoy_build(idx, n_trees);
        if (ret != 0) {
            fprintf(stderr, "Failed to build index\n");
            annoy_destroy(idx);
            free(base);
            free(query);
            free(gt);
            return 1;
        }
        printf("Index built, n_items=%d, n_trees=%d\n\n",
               annoy_get_n_items(idx), annoy_get_n_trees(idx));

        /* 测试 Recall@1, Recall@10, Recall@50 */
        for (int tk = 0; tk < 3; tk++) {
            int k = (tk == 0) ? 1 : (tk == 1) ? 10 : 50;
            if (k > gt_k) k = gt_k;

            float total_recall = 0.0f;
            for (int q = 0; q < n_query; q++) {
                float dists[100];
                int32_t ids[100];
                memset(ids, -1, sizeof(ids));
                memset(dists, 0, sizeof(dists));

                /* search_k=-1 表示使用默认值 n_trees * k */
                int n_found = annoy_search(
                    idx,
                    &query[q * dims],
                    k,
                    ids,
                    dists,
                    -1  /* search_k=-1：使用默认值 */
                );

                if (n_found < 0) {
                    fprintf(stderr, "Search failed for query %d\n", q);
                    continue;
                }

                float recall = compute_recall_at_k(ids, &gt[q * gt_k], k);
                total_recall += recall;
            }
            float avg_recall = total_recall / n_query;
            printf("  Recall@%d: %.4f\n", k, avg_recall);
        }

        /* 清理当前索引 */
        annoy_destroy(idx);
        printf("\n");
    }

    /* 清理数据 */
    free(base);
    free(query);
    free(gt);

    printf("Test completed!\n");
    return 0;
}
