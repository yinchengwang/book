/*
 * ivf_flat_recall.c
 *
 * IVF-Flat 索引 SIFT 召回率测试
 *
 * 数据格式 (binary):
 *   base.bin/query.bin: float32 array [count * dims]
 *   groundtruth.bin: int32 array [query_count * k]
 *   *_header.txt: count=xxx, dims=xxx, dtype=xxx
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* IVF-Flat 接口 */
#include "db/index/vector_index/ivf_flat/ivf_flat.h"
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

int main(int argc, char *argv[]) {
    /* Default: use siftsmall (10K vectors) */
    const char *data_dir = "c:/code/book/dataset/siftsmall";

    /* Allow command-line override */
    if (argc > 1) {
        data_dir = argv[1];
    }

    printf("=== IVF-Flat SIFT Recall Test ===\n\n");
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

    /* Build IVF-Flat index */
    int nlist = 100;

    printf("Creating IVF-Flat index (nlist=%d, dims=%d)...\n", nlist, dims);
    ivf_flat_index_t *idx = ivf_flat_create(nlist, dims);
    if (!idx) {
        fprintf(stderr, "Failed to create IVF-Flat index\n");
        free(base);
        free(query);
        free(gt);
        return 1;
    }

    /* Train index with K-Means */
    printf("Training index with %d vectors...\n", n_base);
    int ret = ivf_flat_train(idx, n_base, base);
    if (ret != 0) {
        fprintf(stderr, "Failed to train index: %d\n", ret);
        ivf_flat_destroy(idx);
        free(base);
        free(query);
        free(gt);
        return 1;
    }
    printf("Training completed.\n");

    /* Prepare IDs array */
    int *ids = (int *)malloc(n_base * sizeof(int));
    if (!ids) {
        fprintf(stderr, "Failed to allocate IDs array\n");
        ivf_flat_destroy(idx);
        free(base);
        free(query);
        free(gt);
        return 1;
    }
    for (int i = 0; i < n_base; i++) {
        ids[i] = i;
    }

    /* Add vectors to index */
    printf("Adding %d vectors to index...\n", n_base);
    ret = ivf_flat_add(idx, n_base, base, ids);
    if (ret != n_base) {
        fprintf(stderr, "Failed to add vectors: expected %d, got %d\n", n_base, ret);
        free(ids);
        ivf_flat_destroy(idx);
        free(base);
        free(query);
        free(gt);
        return 1;
    }
    printf("Added %d vectors.\n\n", ret);
    free(ids);

    /* Test different nprobe values */
    printf("Testing recall with different nprobe values...\n");
    int nprobe_values[] = {1, 5, 10};
    int n_np = sizeof(nprobe_values) / sizeof(nprobe_values[0]);

    for (int np = 0; np < n_np; np++) {
        int nprobe = nprobe_values[np];
        ivf_flat_set_nprobe(idx, nprobe);
        printf("--- nprobe = %d ---\n", nprobe);

        /* 测试 Recall@1, Recall@10, Recall@50 */
        for (int tk = 0; tk < 3; tk++) {
            int k = (tk == 0) ? 1 : (tk == 1) ? 10 : 50;
            if (k > gt_k) k = gt_k;

            float total_recall = 0.0f;
            for (int q = 0; q < n_query; q++) {
                float dists[100];
                int32_t ids_out[100];
                memset(ids_out, -1, sizeof(ids_out));
                memset(dists, 0, sizeof(dists));

                ivf_flat_search(idx, &query[q * dims], k, ids_out, dists);

                float recall = compute_recall_at_k(ids_out, &gt[q * gt_k], k);
                total_recall += recall;
            }
            float avg_recall = total_recall / n_query;
            printf("  Recall@%d: %.4f\n", k, avg_recall);
        }
    }

    /* Cleanup */
    ivf_flat_destroy(idx);
    free(base);
    free(query);
    free(gt);

    printf("\nTest completed!\n");
    return 0;
}
