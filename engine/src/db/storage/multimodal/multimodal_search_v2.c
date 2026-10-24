/**
 * @file multimodal_search_v2.c
 * @brief 多模态检索完整路径（C5.5-C5.7）
 *
 * per-named-vector 串行搜索 + RRF 融合 + metadata filter
 */
#include "db/multimodal_object.h"
#include "db/index/vector_index/hnsw/faiss_hnsw_segment.h"
#include "db/core/log.h"

#include <stdlib.h>
#include <string.h>

#define MAX_NAMED_VECTORS_PER_QUERY 8
#define RRF_K 60

typedef struct {
    int32_t id;          /* 候选 object id（这里简化为 segment offset + local idx） */
    double rrf_score;    /* RRF 累积得分 */
} mm_candidate_t;

static int compare_candidates(const void *a, const void *b) {
    double sa = ((const mm_candidate_t *)a)->rrf_score;
    double sb = ((const mm_candidate_t *)b)->rrf_score;
    if (sa > sb) return -1;
    if (sa < sb) return 1;
    return 0;
}

/* mm_multimodal_search_v2：对 query 对象的每个 named vector，
 * 在对应 collection 上做 top-k 查询，按 RRF 融合取整体 top-k
 *
 * 参数：
 *   query - 查询对象（含每个 named vector 的向量）
 *   collections - 每个 named vector 对应的 faiss_hnsw_collection_t 指针数组
 *   n_spaces - named vector 数量
 *   top_k - 返回数量
 *   filter_json - 元数据过滤关键词（NULL=不过滤）
 *   out_ids / out_scores - 输出
 */
int mm_multimodal_search_v2(const mm_multimodal_object_t *query,
                           void *const *collections,
                           int n_spaces,
                           int top_k,
                           const char *filter_json,
                           int32_t *out_ids,
                           double *out_scores) {
    if (!query || !collections || n_spaces <= 0 || top_k <= 0 || !out_ids) return -1;
    if (n_spaces > MAX_NAMED_VECTORS_PER_QUERY) n_spaces = MAX_NAMED_VECTORS_PER_QUERY;

    mm_candidate_t *cands = calloc((size_t)query->n_vectors * (size_t)top_k, sizeof(mm_candidate_t));
    if (!cands) return -1;
    int cand_capacity = query->n_vectors * top_k;
    int n_cands = 0;

    /* 每个 named vector 空间：取 top-k 候选，按 RRF 累分 */
    for (int i = 0; i < query->n_vectors && i < n_spaces; ++i) {
        if (!collections[i] || !query->vectors[i].data) continue;

        faiss_hnsw_collection_t *col = (faiss_hnsw_collection_t *)collections[i];
        float *dists = malloc(sizeof(float) * (size_t)top_k);
        int32_t *ids = malloc(sizeof(int32_t) * (size_t)top_k);
        if (!dists || !ids) { free(dists); free(ids); continue; }

        int32_t n = faiss_hnsw_collection_search(col,
                                              query->vectors[i].data,
                                              top_k, dists, ids);
        /* RRF 累分：1/(k_rrf + rank) */
        for (int32_t r = 0; r < n; ++r) {
            int32_t id = ids[r];
            if (id < 0) continue;
            /* 查是否已在 cands（O(n²)，空间小可接受） */
            int found = -1;
            for (int j = 0; j < n_cands; ++j) {
                if (cands[j].id == id) { found = j; break; }
            }
            double add = 1.0 / (double)(RRF_K + r + 1);
            if (found >= 0) {
                cands[found].rrf_score += add;
            } else if (n_cands < cand_capacity) {
                cands[n_cands].id = id;
                cands[n_cands].rrf_score = add;
                n_cands++;
            }
        }
        free(dists); free(ids);
    }

    /* metadata filter（占位：若 filter_json 非空则过滤掉无 metadata 匹配的对象） */
    if (filter_json && n_cands > 0 && query->metadata) {
        int j = 0;
        for (int i = 0; i < n_cands; ++i) {
            if (memmem(query->metadata, query->metadata_len,
                       filter_json, strlen(filter_json))) {
                cands[j++] = cands[i];
            }
        }
        n_cands = j;
    }

    /* 按 RRF 分数排序 */
    qsort(cands, (size_t)n_cands, sizeof(mm_candidate_t), compare_candidates);

    /* 输出 top-k */
    int n_out = n_cands < top_k ? n_cands : top_k;
    for (int i = 0; i < n_out; ++i) {
        out_ids[i] = cands[i].id;
        if (out_scores) out_scores[i] = cands[i].rrf_score;
    }
    free(cands);
    return n_out;
}
