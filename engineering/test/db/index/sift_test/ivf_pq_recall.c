/*
 * ivf_pq_recall.c
 *
 * IVF-PQ 索引 SIFT 召回率测试
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "db/index/vector_index/ivf_pq/ivf_pq.h"
#include "algo-prod/distance/distance.h"

/* L2 距离平方计算 */
static inline float l2sqr(const float *a, const float *b, int dims) {
    float dist = 0.0f;
    for (int i = 0; i < dims; i++) {
        float d = a[i] - b[i];
        dist += d * d;
    }
    return dist;
}

/* 读取头文件信息 */
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
        fprintf(stderr, "Warning: header not found: %s\n", header_path);
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
        fprintf(stderr, "Error: invalid header\n");
        return -1;
    }
    return 0;
}

/* 读取二进制向量文件 */
static float *read_binary_fvecs(const char *path, int *out_count, int *out_dims) {
    if (read_header(path, out_count, out_dims) != 0) {
        return NULL;
    }
    FILE *f = fopen(path, "rb");
    if (!f) {
        perror("fopen");
        return NULL;
    }
    size_t total = (size_t)(*out_count) * (*out_dims);
    float *v = (float *)malloc(total * sizeof(float));
    if (!v) {
        fclose(f);
        return NULL;
    }
    size_t r = fread(v, sizeof(float), total, f);
    fclose(f);
    if (r != total) {
        free(v);
        return NULL;
    }
    printf("Read %s: %d vectors, dimension %d\n", path, *out_count, *out_dims);
    return v;
}

/* 读取真值文件 */
static int32_t *read_binary_groundtruth(const char *path, int *out_count, int *out_k) {
    int dims;
    if (read_header(path, out_count, &dims) != 0) {
        return NULL;
    }
    *out_k = dims;
    FILE *f = fopen(path, "rb");
    if (!f) {
        perror("fopen");
        return NULL;
    }
    size_t total = (size_t)(*out_count) * (*out_k);
    int32_t *gt = (int32_t *)malloc(total * sizeof(int32_t));
    if (!gt) {
        fclose(f);
        return NULL;
    }
    size_t r = fread(gt, sizeof(int32_t), total, f);
    fclose(f);
    if (r != total) {
        free(gt);
        return NULL;
    }
    printf("Read %s: %d queries, K=%d\n", path, *out_count, *out_k);
    return gt;
}

/* 计算召回率 */
static float compute_recall_at_k(const int32_t *result, const int32_t *gt, int k) {
    int hit = 0;
    for (int i = 0; i < k; i++) {
        for (int j = 0; j < k; j++) {
            if (result[i] == gt[j]) {
                hit++;
                break;
            }
        }
    }
    return (float)hit / k;
}

int main(int argc, char *argv[]) {
    const char *data_dir = "c:/code/book/dataset/siftsmall";
    char base_path[512], query_path[512], gt_path[512];
    snprintf(base_path, sizeof(base_path), "%s/base.bin", data_dir);
    snprintf(query_path, sizeof(query_path), "%s/query.bin", data_dir);
    snprintf(gt_path, sizeof(gt_path), "%s/groundtruth.bin", data_dir);

    /* 加载数据 */
    int n_base, dims, n_query, query_dims, n_gt, gt_k;
    float *base = read_binary_fvecs(base_path, &n_base, &dims);
    float *query = read_binary_fvecs(query_path, &n_query, &query_dims);
    int32_t *gt = read_binary_groundtruth(gt_path, &n_gt, &gt_k);

    if (!base || !query || !gt) {
        fprintf(stderr, "Failed to load data\n");
        if (base) free(base);
        if (query) free(query);
        if (gt) free(gt);
        return 1;
    }

    printf("Base: %d vectors, Query: %d vectors, GT K=%d\n", n_base, n_query, gt_k);
    printf("Dimensions: %d\n", dims);

    /* IVF-PQ 参数 */
    int nlist = 100;
    int pq_m = 8;
    int pq_bits = 8;

    printf("\n=== IVF-PQ Recall Test ===\n");
    printf("nlist=%d, pq_m=%d, pq_bits=%d\n", nlist, pq_m, pq_bits);

    /* 创建索引 */
    ivf_pq_index_t *idx = ivf_pq_create(nlist, pq_m, pq_bits, dims);
    if (!idx) {
        fprintf(stderr, "Failed to create IVF-PQ index\n");
        free(base);
        free(query);
        free(gt);
        return 1;
    }

    /* 训练 */
    printf("Training...\n");
    int train_ret = ivf_pq_train(idx, n_base, base);
    if (train_ret != 0) {
        fprintf(stderr, "Training failed: %d\n", train_ret);
        ivf_pq_destroy(idx);
        free(base);
        free(query);
        free(gt);
        return 1;
    }
    printf("Training done.\n");

    /* 构造 ids 数组 */
    int *ids = (int *)malloc(n_base * sizeof(int));
    if (!ids) {
        ivf_pq_destroy(idx);
        free(base);
        free(query);
        free(gt);
        return 1;
    }
    for (int i = 0; i < n_base; i++) {
        ids[i] = i;
    }

    /* 添加向量 */
    printf("Adding %d vectors...\n", n_base);
    int add_ret = ivf_pq_add(idx, n_base, base, ids);
    if (add_ret != 0) {
        fprintf(stderr, "Add failed: %d\n", add_ret);
        free(ids);
        ivf_pq_destroy(idx);
        free(base);
        free(query);
        free(gt);
        return 1;
    }
    printf("Add done.\n");
    free(ids);

    /* 测试不同 nprobe 值 */
    int nprobe_values[] = {1, 5, 10};
    int k_values[] = {1, 10, 50};

    for (int np = 0; np < 3; np++) {
        int nprobe = nprobe_values[np];
        ivf_pq_set_nprobe(idx, nprobe);
        printf("\n--- nprobe = %d ---\n", nprobe);

        for (int ki = 0; ki < 3; ki++) {
            int k = k_values[ki];
            if (k > gt_k) {
                k = gt_k;
            }

            float total_recall = 0.0f;
            for (int q = 0; q < n_query; q++) {
                float dists[100];
                int32_t ids_out[100];
                memset(ids_out, -1, sizeof(ids_out));
                memset(dists, 0, sizeof(dists));
                ivf_pq_search(idx, &query[q * dims], k, (int *)ids_out, dists);
                total_recall += compute_recall_at_k(ids_out, &gt[q * gt_k], k);
            }
            printf("  Recall@%d: %.4f\n", k, total_recall / n_query);
        }
    }

    /* 清理 */
    ivf_pq_destroy(idx);
    free(base);
    free(query);
    free(gt);

    printf("\nIVF-PQ recall test completed.\n");
    return 0;
}