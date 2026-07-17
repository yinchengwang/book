/*
 * 验证 HNSW 向量存储是否正确
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "db/index/vector_index/hnsw/faiss_hnsw.h"

int main(void) {
    /* 读取前100个向量 */
    FILE *f = fopen("c:/code/book/dataset/siftsmall_processed/base.bin", "rb");
    if (!f) { perror("fopen"); return 1; }

    int dims = 128;
    int n_base = 0;
    float *all_base = NULL;
    float buf[128];

    while (fread(&dims, 4, 1, f) == 1) {
        fread(buf, sizeof(float), dims, f);
        if (n_base >= 100) break;
        float *v = realloc(all_base, (n_base + 1) * dims * sizeof(float));
        if (!v) { free(all_base); fclose(f); return 1; }
        all_base = v;
        memcpy(all_base + n_base * dims, buf, dims * sizeof(float));
        n_base++;
    }
    fclose(f);

    printf("=== 读取 %d 个向量, dims=%d ===\n\n", n_base, dims);

    /* 检查源数据 */
    printf("源数据前5个向量:\n");
    for (int i = 0; i < 5 && i < n_base; i++) {
        printf("  all_base[%d]: [%.2f, %.2f, %.2f, ...]\n",
               i, all_base[i*dims], all_base[i*dims+1], all_base[i*dims+2]);
    }

    /* 构建索引 */
    printf("\n=== 构建 HNSW 索引 ===\n");
    faiss_hnsw_t *index = faiss_hnsw_index_create(
        16, dims, 200,
        DISTANCE_METRIC_L2_SQUARED,
        QUANTIZATION_TYPE_NONE
    );

    if (!index) { printf("创建索引失败\n"); free(all_base); return 1; }

    int ret = faiss_hnsw_index_add(index, n_base, all_base);
    if (ret != 0) {
        printf("添加向量失败: %d\n", ret);
        free(all_base);
        faiss_hnsw_index_drop(index);
        return 1;
    }

    printf("索引构建完成: n_total=%d\n", index->n_total);

    /* 检查存储的向量 */
    printf("\n存储的向量前5个:\n");
    for (int i = 0; i < 5 && i < index->n_total; i++) {
        int nan_cnt = 0;
        for (int d = 0; d < dims; d++) {
            if (index->vectors[i*dims + d] != index->vectors[i*dims + d]) nan_cnt++;
        }
        printf("  vectors[%d]: [%.2f, %.2f, %.2f, ...]%s\n",
               i, index->vectors[i*dims], index->vectors[i*dims+1], index->vectors[i*dims+2],
               nan_cnt > 0 ? " (HAS NAN)" : "");
    }

    /* 暴力搜索 GT */
    float *query = all_base + 0 * dims;  /* 用第一个向量作为 query */
    printf("\n暴力搜索 top-5:\n");
    float best_dist = 1e30f;
    int best_ids[5] = {0, 0, 0, 0, 0};
    for (int i = 0; i < n_base; i++) {
        float dist = 0;
        for (int d = 0; d < dims; d++) {
            float diff = query[d] - all_base[i*dims + d];
            dist += diff * diff;
        }
        printf("  base[%d] vs query[0]: dist=%.2f\n", i, dist);
        if (dist < best_dist) {
            best_dist = dist;
            best_ids[0] = i;
        }
    }
    printf("暴力搜索最近邻: id=%d dist=%.2f\n", best_ids[0], best_dist);

    /* HNSW 搜索 */
    printf("\nHNSW 搜索 top-5 (ef=5):\n");
    int32_t ids[5];
    float dists[5];
    memset(ids, -1, sizeof(ids));
    memset(dists, 0, sizeof(dists));
    faiss_hnsw_index_search(index, query, 5, 5, dists, ids);
    for (int i = 0; i < 5; i++) {
        printf("  [%d]: id=%d dist=%.2f\n", i, ids[i], dists[i]);
    }

    free(all_base);
    faiss_hnsw_index_drop(index);

    printf("\n完成!\n");
    return 0;
}
