/*
 * milvus_ivf_recall.c
 *
 * Milvus IVF 索引 SIFT 数据集召回率测试
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

/* Milvus IVF 接口 */
#include "db/index/vector_index/milvus_ivf/milvus_ivf.h"
#include "algo-prod/distance/distance.h"

/* ── 距离计算：L2 平方距离 ── */
static inline float l2sqr(const float *a, const float *b, int dims) {
    float dist = 0.0f;
    for (int i = 0; i < dims; i++) {
        float d = a[i] - b[i];
        dist += d * d;
    }
    return dist;
}

/* ── 读取头文件获取向量数量和维度 ── */
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

/* ── 读取纯二进制格式（float32 数组）── */
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

/* ── 读取纯二进制格式（int32 数组，用于 groundtruth）── */
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

/* ── 计算召回率 (Recall@K) ── */
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

    printf("=== Milvus IVF SIFT Recall Test ===\n\n");
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

    /* 创建 Milvus IVF 索引 */
    int nlist = 100;  /* 簇数量 */

    printf("Creating Milvus IVF index (nlist=%d, dims=%d)...\n", nlist, dims);
    milvus_ivf_t *index = milvus_ivf_create(dims, nlist);
    if (!index) {
        fprintf(stderr, "Failed to create Milvus IVF index\n");
        free(base);
        free(query);
        free(gt);
        return 1;
    }

    /* 训练索引 */
    printf("Training index with %d vectors...\n", n_base);
    int ret = milvus_ivf_train(index, n_base, base);
    if (ret != 0) {
        fprintf(stderr, "Failed to train index: %d\n", ret);
        milvus_ivf_destroy(index);
        free(base);
        free(query);
        free(gt);
        return 1;
    }

    /* 构造 ids 数组 */
    int *ids = (int *)malloc(n_base * sizeof(int));
    if (!ids) {
        fprintf(stderr, "Failed to allocate ids array\n");
        milvus_ivf_destroy(index);
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
    ret = milvus_ivf_add(index, n_base, base, ids);
    if (ret != n_base) {
        fprintf(stderr, "Warning: added %d vectors, expected %d\n", ret, n_base);
    }

    int n_vectors, out_dims, out_nlist;
    milvus_ivf_stats(index, &n_vectors, &out_dims, &out_nlist);
    printf("Index built, total vectors: %d, nlist: %d\n\n", n_vectors, out_nlist);

    free(ids);

    /* 测试不同 nprobe 值 */
    printf("Testing recall with different nprobe values...\n");
    int nprobe_values[] = {1, 5, 10};
    int n_np = sizeof(nprobe_values) / sizeof(nprobe_values[0]);

    for (int np = 0; np < n_np; np++) {
        int nprobe = nprobe_values[np];
        printf("--- nprobe = %d ---\n", nprobe);

        /* 测试 Recall@1, Recall@10, Recall@50 */
        for (int tk = 0; tk < 3; tk++) {
            int k = (tk == 0) ? 1 : (tk == 1) ? 10 : 50;
            if (k > gt_k) k = gt_k;

            float total_recall = 0.0f;
            for (int q = 0; q < n_query; q++) {
                int32_t ids_out[100];
                memset(ids_out, -1, sizeof(ids_out));

                /* 注意：milvus_ivf_search 没有 distances 参数，只有 ids */
                milvus_ivf_search(
                    index,
                    &query[q * dims],
                    k,
                    ids_out,
                    nprobe
                );

                float recall = compute_recall_at_k(ids_out, &gt[q * gt_k], k);
                total_recall += recall;
            }
            float avg_recall = total_recall / n_query;
            printf("  Recall@%d: %.4f\n", k, avg_recall);
        }
    }

    /* 清理 */
    milvus_ivf_destroy(index);
    free(base);
    free(query);
    free(gt);

    printf("\nTest completed!\n");
    return 0;
}