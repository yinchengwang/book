/*
 * sift_recall_bruteforce_gt.c
 *
 * 使用暴力搜索作为 ground truth，测试 HNSW 召回率。
 * 这样可以排除数据集不匹配的问题。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#include "db/index/vector_index/hnsw/faiss_hnsw.h"
#include "algo-prod/distance/distance.h"

static float *read_fvecs(const char *path, int *out_count, int *out_dims) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror("fopen"); return NULL; }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    int count = 0;
    int dims = 0;
    while (ftell(f) < file_size) {
        int32_t dim;
        if (fread(&dim, sizeof(int32_t), 1, f) != 1) break;
        fseek(f, dim * sizeof(float), SEEK_CUR);
        if (count == 0) dims = dim;
        count++;
    }

    fseek(f, 0, SEEK_SET);
    float *vectors = (float *)malloc(count * dims * sizeof(float));
    if (!vectors) { fclose(f); return NULL; }

    for (int i = 0; i < count; i++) {
        int32_t dim;
        fread(&dim, sizeof(int32_t), 1, f);
        fread(&vectors[i * dims], sizeof(float), dim, f);
    }
    fclose(f);

    *out_count = count;
    *out_dims = dims;
    return vectors;
}

/* 暴力搜索 top-k */
static void bruteforce_topk(const float *query, const float *base,
                            int n_base, int dims, int k, int32_t *out_ids, float *out_dist) {
    float *dists = (float *)malloc(n_base * sizeof(float));
    for (int i = 0; i < n_base; i++) {
        dists[i] = 0;
        for (int d = 0; d < dims; d++) {
            float diff = query[d] - base[i * dims + d];
            dists[i] += diff * diff;
        }
    }

    /* 选择排序 top-k */
    for (int i = 0; i < k; i++) {
        int min_idx = i;
        for (int j = i + 1; j < n_base; j++) {
            if (dists[j] < dists[min_idx]) min_idx = j;
        }
        out_ids[i] = min_idx;
        out_dist[i] = dists[min_idx];
        dists[min_idx] = 1e30f; /* 标记为已选 */
    }
    free(dists);
}

/* 计算召回率 */
static float recall_at_k(const int32_t *result, const int32_t *gt, int k) {
    int hit = 0;
    for (int i = 0; i < k; i++) {
        for (int j = 0; j < k; j++) {
            if (result[i] == gt[j]) { hit++; break; }
        }
    }
    return (float)hit / k;
}

int main(void) {
    int n_base, dims, n_query;
    float *base = read_fvecs("c:/code/book/dataset/siftsmall_processed/base.bin", &n_base, &dims);
    float *query = read_fvecs("c:/code/book/dataset/siftsmall_processed/query.bin", &n_query, &dims);

    if (!base || !query) {
        fprintf(stderr, "读取数据失败\n");
        return 1;
    }

    printf("=== HNSW 召回率测试 (暴力搜索为 GT) ===\n");
    printf("数据集: %d 向量, 维度 %d, %d 查询\n\n", n_base, dims, n_query);

    /* 构建 HNSW 索引 */
    faiss_hnsw_t *index = faiss_hnsw_index_create(
        16, dims, 500,  /* 增加 ef_construction 到 500 */
        DISTANCE_METRIC_L2_SQUARED,
        QUANTIZATION_TYPE_NONE
    );
    if (!index) {
        fprintf(stderr, "创建索引失败\n");
        return 1;
    }

    int ret = faiss_hnsw_index_add(index, n_base, base);
    if (ret != 0) {
        fprintf(stderr, "添加向量失败: %d\n", ret);
        return 1;
    }
    printf("索引构建完成\n\n");

    /* 测试不同 ef_search 值 */
    int ef_values[] = {5, 10, 20, 50, 100, 200};
    int n_ef = sizeof(ef_values) / sizeof(ef_values[0]);
    int k_values[] = {1, 10, 50};
    int n_k = sizeof(k_values) / sizeof(k_values[0]);

    for (int e = 0; e < n_ef; e++) {
        int ef = ef_values[e];
        printf("--- ef_search = %d ---\n", ef);

        for (int tk = 0; tk < n_k; tk++) {
            int k = k_values[tk];

            float total_recall = 0.0f;
            int n_queries = n_query < 20 ? n_query : 20;  /* 用前 20 个查询加速 */

            for (int q = 0; q < n_queries; q++) {
                /* 暴力搜索 ground truth */
                int32_t gt_ids[100];
                float gt_dist[100];
                bruteforce_topk(&query[q * dims], base, n_base, dims, k, gt_ids, gt_dist);

                /* HNSW 搜索 */
                float dists[100];
                int32_t ids[100];
                memset(ids, -1, sizeof(ids));
                memset(dists, 0, sizeof(dists));
                faiss_hnsw_index_search(index, &query[q * dims], k, ef, dists, ids);

                /* 计算召回率 */
                float recall = recall_at_k(ids, gt_ids, k);
                total_recall += recall;

                if (q == 0) {
                    printf("  query[0] top-%d HNSW: ", k);
                    for (int i = 0; i < k && i < 10; i++) printf("%d ", ids[i]);
                    printf("\n");
                    printf("  query[0] top-%d GT:  ", k);
                    for (int i = 0; i < k && i < 10; i++) printf("%d ", gt_ids[i]);
                    printf("\n");
                }
            }
            float avg_recall = total_recall / n_queries;
            printf("  Recall@%d: %.4f\n", k, avg_recall);
        }
    }

    free(base);
    free(query);
    faiss_hnsw_index_drop(index);

    printf("\n测试完成！\n");
    return 0;
}
