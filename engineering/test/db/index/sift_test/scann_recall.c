#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <float.h>

#include "db/index/vector_index/scann/scann.h"
#include "algo-prod/distance/distance.h"

static int read_header(const char *bin_path, int *out_count, int *out_dims) {
    char header_path[512];
    snprintf(header_path, sizeof(header_path), "%s", bin_path);
    char *dot = strrchr(header_path, '.');
    if (dot) strcpy(dot, "_header.txt");
    else strcat(header_path, "_header.txt");
    FILE *f = fopen(header_path, "r");
    if (!f) { fprintf(stderr, "Warning: header not found: %s\n", header_path); return -1; }
    char line[256]; *out_count = 0; *out_dims = 0;
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "count=%d", out_count) == 1) continue;
        if (sscanf(line, "dims=%d", out_dims) == 1) continue;
    }
    fclose(f);
    if (*out_count == 0 || *out_dims == 0) { fprintf(stderr, "Error: invalid header\n"); return -1; }
    return 0;
}

static float *read_binary_fvecs(const char *path, int *out_count, int *out_dims) {
    if (read_header(path, out_count, out_dims) != 0) return NULL;
    FILE *f = fopen(path, "rb");
    if (!f) { perror("fopen"); return NULL; }
    size_t total = (size_t)(*out_count) * (*out_dims);
    float *v = (float *)malloc(total * sizeof(float));
    if (!v) { fclose(f); return NULL; }
    size_t r = fread(v, sizeof(float), total, f);
    fclose(f);
    if (r != total) { free(v); return NULL; }
    printf("Read %s: %d vectors, dimension %d\n", path, *out_count, *out_dims);
    return v;
}

static int32_t *read_binary_groundtruth(const char *path, int *out_count, int *out_k) {
    int dims;
    if (read_header(path, out_count, &dims) != 0) return NULL;
    *out_k = dims;
    FILE *f = fopen(path, "rb");
    if (!f) { perror("fopen"); return NULL; }
    size_t total = (size_t)(*out_count) * (*out_k);
    int32_t *gt = (int32_t *)malloc(total * sizeof(int32_t));
    if (!gt) { fclose(f); return NULL; }
    size_t r = fread(gt, sizeof(int32_t), total, f);
    fclose(f);
    if (r != total) { free(gt); return NULL; }
    printf("Read %s: %d queries, K=%d\n", path, *out_count, *out_k);
    return gt;
}

static float compute_recall_at_k(const int32_t *result, const int32_t *gt, int k) {
    int hit = 0;
    for (int i = 0; i < k; i++)
        for (int j = 0; j < k; j++)
            if (result[i] == gt[j]) { hit++; break; }
    return (float)hit / k;
}

int main(int argc, char *argv[]) {
    const char *data_dir = "c:/code/book/dataset/siftsmall";
    int n_base, dims, n_query, gt_k;

    char base_path[512], query_path[512], gt_path[512];
    snprintf(base_path, sizeof(base_path), "%s/base.bin", data_dir);
    snprintf(query_path, sizeof(query_path), "%s/query.bin", data_dir);
    snprintf(gt_path, sizeof(gt_path), "%s/groundtruth.bin", data_dir);

    printf("=== ScaNN SIFT Recall Test ===\n\n");
    printf("Dataset: %s\n\n", data_dir);

    float *base = read_binary_fvecs(base_path, &n_base, &dims);
    float *query = read_binary_fvecs(query_path, &n_query, &dims);
    int32_t *gt = read_binary_groundtruth(gt_path, &n_query, &gt_k);

    if (!base || !query || !gt) {
        fprintf(stderr, "Error: failed to load dataset\n");
        free(base); free(query); free(gt);
        return 1;
    }

    printf("\nDataset summary:\n");
    printf("  Base: %d vectors, dimension %d\n", n_base, dims);
    printf("  Query: %d vectors\n", n_query);
    printf("  Ground Truth: K=%d\n\n", gt_k);

    int n_neighbors = 10;
    printf("Building ScaNN index (n_neighbors=%d)...\n", n_neighbors);

    scann_t *idx = scann_create(dims, n_neighbors);
    if (!idx) {
        fprintf(stderr, "Error: failed to create ScaNN index\n");
        free(base); free(query); free(gt);
        return 1;
    }

    int ret = scann_train(idx, n_base, base);
    if (ret != 0) {
        fprintf(stderr, "Error: training failed\n");
        scann_destroy(idx);
        free(base); free(query); free(gt);
        return 1;
    }

    int *ids = (int *)malloc(n_base * sizeof(int));
    for (int i = 0; i < n_base; i++) ids[i] = i;
    scann_add(idx, n_base, base, ids);
    free(ids);

    printf("Index built, total vectors: %d\n\n", n_base);

    /* 搜索测试 - K=1,10,50 */
    int k_values[] = {1, 10, 50};
    int nk = sizeof(k_values) / sizeof(k_values[0]);

    printf("Testing recall...\n");
    for (int tk = 0; tk < nk; tk++) {
        int k = k_values[tk];
        if (k > gt_k) k = gt_k;

        float total_recall = 0.0f;
        for (int q = 0; q < n_query; q++) {
            float dists[100];
            int32_t ids_out[100];
            memset(ids_out, -1, sizeof(ids_out));
            memset(dists, 0, sizeof(dists));

            scann_search(idx, &query[q * dims], k, ids_out, dists);
            total_recall += compute_recall_at_k(ids_out, &gt[q * gt_k], k);
        }
        printf("  Recall@%d: %.4f\n", k, total_recall / n_query);
    }

    scann_destroy(idx);
    free(base); free(query); free(gt);
    printf("\nTest completed!\n");
    return 0;
}