/*
 * hnsw_recall_test.c
 *
 * HNSW 召回率测试（使用 faiss_hnsw 索引）
 * 使用 SIFT Small 数据集验证召回率
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

#include <db/index/vector_index/faiss_hnsw/faiss_hnsw.h>
#include <algo-prod/distance/distance.h>

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
        fprintf(stderr, "警告：找不到头文件: %s\n", header_path);
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
        fprintf(stderr, "错误：无效的头文件\n");
        return -1;
    }

    return 0;
}

/* 读取纯二进制格式（float32 数组） */
static float *read_binary_fvecs(const char *path, int *out_count, int *out_dims) {
    if (read_header(path, out_count, out_dims) != 0) {
        return NULL;
    }

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
        fprintf(stderr, "错误：读取 %zu 个 float，期望 %zu\n", read_count, total_floats);
        free(vectors);
        return NULL;
    }

    printf("读取 %s: %d 个向量，维度 %d\n", path, *out_count, *out_dims);
    return vectors;
}

/* 读取纯二进制格式（int32 数组，用于 groundtruth） */
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

    size_t total_ints = (size_t)(*out_count) * (*out_k);
    int32_t *gt = (int32_t *)malloc(total_ints * sizeof(int32_t));
    if (!gt) {
        fclose(f);
        return NULL;
    }

    size_t read_count = fread(gt, sizeof(int32_t), total_ints, f);
    fclose(f);

    if (read_count != total_ints) {
        fprintf(stderr, "错误：读取 %zu 个 int，期望 %zu\n", read_count, total_ints);
        free(gt);
        return NULL;
    }

    printf("读取 %s: %d 个查询，K=%d\n", path, *out_count, *out_k);
    return gt;
}

/* 计算召回率 (Recall@K) */
static float compute_recall_at_k(const int32_t *result, const int32_t *gt, int k, int gt_k) {
    int hit = 0;
    for (int i = 0; i < k; i++) {
        if (result[i] < 0) continue;  /* 跳过无效结果 */
        for (int j = 0; j < gt_k; j++) {
            if (result[i] == gt[j]) {
                hit++;
                break;
            }
        }
    }
    return (float)hit / k;
}

int main(int argc, char *argv[]) {
    /* 默认使用 siftsmall 数据集 */
    const char *data_dir = "c:/code/book/dataset/siftsmall";

    /* 允许命令行参数覆盖 */
    if (argc > 1) {
        data_dir = argv[1];
    }

    printf("=== HNSW 召回率测试 ===\n\n");
    printf("数据目录: %s\n\n", data_dir);

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
        fprintf(stderr, "加载数据失败\n");
        return 1;
    }

    printf("\n数据集摘要：\n");
    printf("  Base: %d 个向量，维度 %d\n", n_base, dims);
    printf("  Query: %d 个向量\n", n_query);
    printf("  Ground Truth: K=%d\n\n", gt_k);

    /* HNSW 参数 */
    int M = 16;
    int ef_construction = 128;
    int k = 10;
    int ef_search_values[] = {5, 10, 20, 40, 80, 200};
    int num_ef = sizeof(ef_search_values) / sizeof(ef_search_values[0]);

    /* 创建索引 */
    printf("创建 HNSW 索引 (M=%d, efConstruction=%d)...\n", M, ef_construction);
    faiss_hnsw_t *index = faiss_hnsw_index_create(
        M,
        dims,
        ef_construction,
        DISTANCE_METRIC_L2_SQUARED,
        QUANTIZATION_TYPE_NONE
    );

    if (!index) {
        fprintf(stderr, "创建索引失败\n");
        free(base);
        free(query);
        free(gt);
        return 1;
    }

    /* 插入向量 */
    printf("插入 %d 个向量...\n", n_base);
    int added = faiss_hnsw_index_add(index, n_base, base);
    printf("DEBUG: faiss_hnsw_index_add 返回值 %d，期望值 %d\n", added, n_base);
    if (added < 0) {
        fprintf(stderr, "插入向量失败：返回负数 %d\n", added);
        faiss_hnsw_index_drop(index);
        free(base);
        free(query);
        free(gt);
        return 1;
    } else if (added != n_base) {
        fprintf(stderr, "插入数量不匹配：实际 %d，期望 %d\n", added, n_base);
        faiss_hnsw_index_drop(index);
        free(base);
        free(query);
        free(gt);
        return 1;
    } else {
        printf("插入成功：%d 个向量\n", added);
    }

    printf("索引构建完成，总向量数: %d，入口点: %d，最大层: %d\n\n",
           faiss_hnsw_index_size(index),
           faiss_hnsw_index_entry_point(index),
           faiss_hnsw_index_max_level(index));

    /* 测试不同 ef_search 值的召回率 */
    printf("----------------------------------------\n");
    printf("ef_search  |  Recall@%d\n", k);
    printf("----------------------------------------\n");

    float recall_40 = 0.0f;

    for (int ei = 0; ei < num_ef; ei++) {
        int ef = ef_search_values[ei];

        float total_recall = 0.0f;
        for (int q = 0; q < n_query; q++) {
            float dists[100];
            int32_t ids[100];
            memset(ids, -1, sizeof(ids));
            memset(dists, 0, sizeof(dists));

            faiss_hnsw_index_search(index, &query[q * dims], k, ef, dists, ids);

            float recall = compute_recall_at_k(ids, &gt[q * gt_k], k, gt_k);
            total_recall += recall;
        }

        float avg_recall = total_recall / n_query;
        printf("  %3d       |  %.4f\n", ef, avg_recall);

        if (ef == 40) {
            recall_40 = avg_recall;
        }
    }

    printf("----------------------------------------\n");

    /* 输出最终结果 */
    printf("\nef_search=40, Recall@%d = %.4f\n", k, recall_40);

    /* 清理 */
    faiss_hnsw_index_drop(index);
    free(base);
    free(query);
    free(gt);

    /* 验证召回率 */
    if (recall_40 >= 0.80f) {
        printf("PASS: 召回率 %.4f >= 0.80\n", recall_40);
        return 0;
    } else {
        fprintf(stderr, "FAIL: 召回率 %.4f < 0.80\n", recall_40);
        return 1;
    }
}