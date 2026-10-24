/*
 * lsh_recall.c
 *
 * Test LSH recall using SIFT dataset.
 * Uses p-stable LSH with n_hash=32, n_tables=32.
 *
 * Data format (binary):
 *   base.bin/query.bin: float32 array [count * dims]
 *   groundtruth.bin: int32 array [query_count * k]
 *   *_header.txt: count=xxx, dims=xxx, dtype=xxx
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* LSH 接口 */
#include "db/index/vector_index/lsh/lsh.h"
#include "algo-prod/distance/distance.h"

/* Read header file to get count and dims */
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

/* Read pure binary format (float32 array) */
static float *read_binary_fvecs(const char *path, int *out_count, int *out_dims) {
    /* Read header first */
    if (read_header(path, out_count, out_dims) != 0) {
        return NULL;
    }

    /* Read binary data */
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

/* Read pure binary format (int32 array for groundtruth) */
static int32_t *read_binary_groundtruth(const char *path, int *out_count, int *out_k) {
    /* Read header first */
    int dims;
    if (read_header(path, out_count, &dims) != 0) {
        return NULL;
    }
    *out_k = dims;

    /* Read binary data */
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
    /* Default: use siftsmall (10K vectors) */
    const char *data_dir = "c:/code/book/dataset/siftsmall";

    /* Allow command-line override */
    if (argc > 1) {
        data_dir = argv[1];
    }

    printf("=== LSH SIFT Recall Test ===\n\n");
    printf("Dataset: %s\n\n", data_dir);

    /* Construct file paths */
    char base_path[512], query_path[512], gt_path[512];
    snprintf(base_path, sizeof(base_path), "%s/base.bin", data_dir);
    snprintf(query_path, sizeof(query_path), "%s/query.bin", data_dir);
    snprintf(gt_path, sizeof(gt_path), "%s/groundtruth.bin", data_dir);

    /* Read data */
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

    /* Build LSH index */
    int n_hash = 32;      /* 每个表的哈希函数数量 */
    int n_tables = 32;    /* 哈希表数量 */

    printf("Building LSH index (LSH_PSTABLE, n_hash=%d, n_tables=%d)...\n", n_hash, n_tables);
    lsh_index_t *index = lsh_create(LSH_PSTABLE, n_hash, n_tables, dims);
    if (!index) {
        fprintf(stderr, "Failed to create LSH index\n");
        free(base);
        free(query);
        free(gt);
        return 1;
    }

    /* 训练 LSH */
    printf("Training LSH with %d vectors...\n", n_base);
    int ret = lsh_train(index, n_base, base);
    if (ret != 0) {
        fprintf(stderr, "Failed to train LSH: %d\n", ret);
        lsh_destroy(index);
        free(base);
        free(query);
        free(gt);
        return 1;
    }

    /* 逐条添加向量（参考 lsh_benchmark.cpp 模式） */
    printf("Adding vectors one by one...\n");
    for (int i = 0; i < n_base; i++) {
        int id = i;
        int added = lsh_add(index, 1, &base[i * dims], &id);
        if (added != 1) {
            fprintf(stderr, "Failed to add vector %d\n", i);
            break;
        }
    }

    /* 获取统计信息 */
    int n_vectors, n_tbl, d;
    lsh_stats(index, &n_vectors, &n_tbl, &d);
    printf("Index built, total vectors: %d, tables: %d, dims: %d\n\n", n_vectors, n_tbl, d);

    /* 测试 Recall@1, Recall@10, Recall@50 */
    printf("Testing recall...\n");
    for (int tk = 0; tk < 3; tk++) {
        int k = (tk == 0) ? 1 : (tk == 1) ? 10 : 50;
        if (k > gt_k) k = gt_k;

        float total_recall = 0.0f;
        for (int q = 0; q < n_query; q++) {
            float dists[100];
            int32_t ids[100];
            memset(ids, -1, sizeof(ids));
            memset(dists, 0, sizeof(dists));

            lsh_search(index, &query[q * dims], k, ids, dists);

            float recall = compute_recall_at_k(ids, &gt[q * gt_k], k);
            total_recall += recall;
        }
        float avg_recall = total_recall / n_query;
        printf("  Recall@%d: %.4f\n", k, avg_recall);
    }

    /* Cleanup */
    lsh_destroy(index);
    free(base);
    free(query);
    free(gt);

    printf("\nTest completed!\n");
    return 0;
}