/*
 * sift_recall.c
 *
 * Test HNSW recall using SIFT dataset.
 * Supports both SIFT Small (10K vectors) and full SIFT 1M (1M vectors).
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
#include <math.h>

/* HNSW 接口 */
#include "db/index/vector_index/hnsw/faiss_hnsw.h"
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

/* 简单的暴力搜索 top-k */
static void bruteforce_topk(const float *query, const float *base,
                            int n_base, int dims, int k, int32_t *out_ids) {
    float *dists = (float *)malloc(n_base * sizeof(float));
    for (int i = 0; i < n_base; i++) {
        dists[i] = l2sqr(query, &base[i * dims], dims);
    }

    /* 简单选择排序取 top-k */
    for (int i = 0; i < k; i++) {
        int min_idx = i;
        for (int j = i + 1; j < n_base; j++) {
            if (dists[j] < dists[min_idx]) min_idx = j;
        }
        out_ids[i] = min_idx;
        dists[min_idx] = dists[i]; /* 标记为已选 */
    }
    free(dists);
}

int main(int argc, char *argv[]) {
    /* Default: use siftsmall (10K vectors) */
    const char *data_dir = "c:/code/book/dataset/siftsmall";

    /* Allow command-line override */
    if (argc > 1) {
        data_dir = argv[1];
    }

    printf("=== SIFT HNSW Recall Test ===\n\n");
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

    /* Build HNSW index */
    printf("Building HNSW index (M=16, efConstruction=200)...\n");
    faiss_hnsw_t *index = faiss_hnsw_index_create(
        16,                 /* M: 每个节点的最大邻居数 */
        dims,               /* dims */
        200,                /* efConstruction */
        DISTANCE_METRIC_L2_SQUARED,
        QUANTIZATION_TYPE_NONE
    );
    if (!index) {
        fprintf(stderr, "Failed to create HNSW index\n");
        return 1;
    }

    /* Add vectors in batch */
    int ret = faiss_hnsw_index_add(index, n_base, base);
    if (ret != 0) {
        fprintf(stderr, "Failed to add vectors: %d\n", ret);
        return 1;
    }
    printf("Index built, total vectors: %d, entry_point: %d, max_level: %d\n\n", index->n_total, index->entry_pointer, index->max_level);

    /* Test different ef_search values */
    printf("Testing recall with different ef_search values...\n");
    int ef_values[] = {5, 10, 20, 50};
    int n_ef = sizeof(ef_values) / sizeof(ef_values[0]);

    for (int e = 0; e < n_ef; e++) {
        int ef = ef_values[e];
        printf("--- ef_search = %d ---\n", ef);

        for (int tk = 0; tk < 3; tk++) {
            int k = (tk == 0) ? 1 : (tk == 1) ? 10 : 50;
            if (k > gt_k) k = gt_k;

            float total_recall = 0.0f;
            for (int q = 0; q < n_query; q++) {
                float dists[100];
                int32_t ids[100];
                memset(ids, -1, sizeof(ids));
                memset(dists, 0, sizeof(dists));
                faiss_hnsw_index_search(index, &query[q * dims], k, ef, dists, ids);
                float recall = compute_recall_at_k(ids, &gt[q * gt_k], k);
                total_recall += recall;
                if (q == 0 && ef == 5) {
                    printf("      query[0] top-%d ids: ", k);
                    for (int p = 0; p < k; p++) printf("%d ", ids[p]);
                    printf("\n");
                    printf("      query[0] top-%d dists: ", k);
                    for (int p = 0; p < k; p++) printf("%.2f ", dists[p]);
                    printf("\n");
                    printf("      query[0] top-%d gt:  ", k);
                    for (int p = 0; p < k; p++) printf("%d ", gt[q * gt_k + p]);
                    printf("\n");
                }
            }
            float avg_recall = total_recall / n_query;
            printf("  Recall@%d: %.4f\n", k, avg_recall);
        }
    }

    /* Verify: bruteforce search recall (should be 1.0) */
    printf("\n--- Verification: bruteforce search recall (should be 1.0) ---\n");
    {
        float total_recall = 0.0f;
        for (int q = 0; q < n_query; q++) {
            int32_t bf_result[100];
            bruteforce_topk(&query[q * dims], base, n_base, dims, gt_k, bf_result);
            float recall = compute_recall_at_k(bf_result, &gt[q * gt_k], gt_k);
            total_recall += recall;
        }
        printf("暴力搜索 vs GroundTruth: %.4f\n", total_recall / n_query);
    }

    faiss_hnsw_index_drop(index);
    free(base);
    free(query);
    free(gt);

    printf("\nTest completed!\n");
    return 0;
}
