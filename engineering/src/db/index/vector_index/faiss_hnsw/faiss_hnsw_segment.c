/**
 * @file faiss_hnsw_segment.c
 * @brief faiss_hnsw COW segment + collection 实装（C5.1-C5.3）
 */
#include "db/index/vector_index/hnsw/faiss_hnsw_segment.h"
#include "db/core/log.h"

#include <stdlib.h>
#include <string.h>

#define SEGS_INIT_CAP 4

faiss_hnsw_collection_t *faiss_hnsw_collection_create(int32_t M, int32_t dims,
                                                       int32_t ef_construction,
                                                       distance_metric_t metric,
                                                       int32_t segment_threshold) {
    if (M <= 0 || dims <= 0) return NULL;
    faiss_hnsw_collection_t *col = calloc(1, sizeof(*col));
    if (!col) return NULL;
    col->M = M;
    col->dims = dims;
    col->metric = metric;
    col->cap_segs = SEGS_INIT_CAP;
    col->segs = calloc(col->cap_segs, sizeof(faiss_hnsw_segment_t));
    if (!col->segs) { free(col); return NULL; }
    /* 创建初始段（empty） */
    col->segs[0].index = faiss_hnsw_index_create(M, dims, ef_construction, metric,
                                                  QUANTIZATION_TYPE_NONE);
    if (!col->segs[0].index) { free(col->segs); free(col); return NULL; }
    col->segs[0].start_id = 0;
    col->segs[0].end_id = 0;
    col->segs[0].n_at_compact = 0;
    col->n_segs = 1;
    col->n_total = 0;
    col->next_id = 0;
    col->buf_threshold = segment_threshold > 0 ? segment_threshold : 1024;
    col->buf_cap = col->buf_threshold * 2;
    col->buf_vectors = malloc(sizeof(float) * (size_t)col->buf_cap * (size_t)dims);
    col->buf_ids = malloc(sizeof(int32_t) * (size_t)col->buf_cap);
    if (!col->buf_vectors || !col->buf_ids) {
        free(col->buf_vectors); free(col->buf_ids);
        free(col->segs); free(col);
        return NULL;
    }
    col->buf_count = 0;
    return col;
}

void faiss_hnsw_collection_destroy(faiss_hnsw_collection_t *col) {
    if (!col) return;
    for (int32_t i = 0; i < col->n_segs; ++i) {
        faiss_hnsw_index_drop(col->segs[i].index);
    }
    free(col->segs);
    free(col->buf_vectors);
    free(col->buf_ids);
    free(col);
}

static int grow_segments(faiss_hnsw_collection_t *col) {
    int32_t new_cap = col->cap_segs * 2;
    faiss_hnsw_segment_t *new_segs = realloc(col->segs,
                                              sizeof(faiss_hnsw_segment_t) * (size_t)new_cap);
    if (!new_segs) return -1;
    col->segs = new_segs;
    col->cap_segs = new_cap;
    return 0;
}

/* compact：将缓冲转为新 segment */
int faiss_hnsw_collection_compact(faiss_hnsw_collection_t *col) {
    if (!col || col->buf_count == 0) return 0;

    /* 分配新段 */
    if (col->n_segs >= col->cap_segs) {
        if (grow_segments(col) != 0) return -1;
    }
    faiss_hnsw_segment_t *seg = &col->segs[col->n_segs];
    seg->index = faiss_hnsw_index_create(col->M, col->dims, col->M * 2,
                                          (distance_metric_t)col->metric,
                                          QUANTIZATION_TYPE_NONE);
    if (!seg->index) return -1;

    /* 演进式：单次 add 全部缓冲 */
    for (int32_t i = 0; i < col->buf_count; ++i) {
        const float *v = col->buf_vectors + (size_t)i * (size_t)col->dims;
        faiss_hnsw_index_add(seg->index, 1, v);
    }
    seg->start_id = col->next_id - col->buf_count;
    seg->end_id = col->next_id - 1;
    seg->n_at_compact = col->buf_count;
    col->n_segs++;

    LOG_INFO("faiss_hnsw_collection compact: 创建 segment %d（%d 个向量）",
             col->n_segs - 1, col->buf_count);
    col->buf_count = 0;
    return 0;
}

int faiss_hnsw_collection_add(faiss_hnsw_collection_t *col,
                             int32_t n, const float *vectors) {
    if (!col || !vectors || n <= 0) return -1;
    /* 累积到缓冲 */
    if (col->buf_count + n > col->buf_cap) {
        /* 扩容 */
        int32_t new_cap = col->buf_cap * 2;
        float *nbv = realloc(col->buf_vectors,
                            sizeof(float) * (size_t)new_cap * (size_t)col->dims);
        int32_t *nbi = realloc(col->buf_ids, sizeof(int32_t) * (size_t)new_cap);
        if (!nbv || !nbi) return -1;
        col->buf_vectors = nbv;
        col->buf_ids = nbi;
        col->buf_cap = new_cap;
    }
    memcpy(col->buf_vectors + (size_t)col->buf_count * (size_t)col->dims,
           vectors, sizeof(float) * (size_t)n * (size_t)col->dims);
    for (int32_t i = 0; i < n; ++i) {
        col->buf_ids[col->buf_count + i] = (int32_t)col->next_id++;
    }
    col->buf_count += n;

    /* 超阈值触发 compact */
    if (col->buf_count >= col->buf_threshold) {
        if (faiss_hnsw_collection_compact(col) != 0) {
            LOG_WARN("compact 失败，下次 add 再试");
            return -1;
        }
    }
    return 0;
}

/* 在单个 segment 内做 top-k 线性扫描 */
static void segment_topk(faiss_hnsw_segment_t *seg, const float *query, int32_t k,
                        float *out_dist, int32_t *out_id, int32_t *base_offset) {
    int32_t n = (int32_t)faiss_hnsw_index_ntotal(seg->index);
    if (n <= 0) return;
    float *d = malloc(sizeof(float) * (size_t)n);
    int32_t *ids = malloc(sizeof(int32_t) * (size_t)n);
    if (!d || !ids) { free(d); free(ids); return; }

    /* 单点对所有向量的距离（无 SIMD，简单循环） */
    for (int32_t i = 0; i < n; ++i) {
        const float *v = seg->index->vectors + (size_t)i * (size_t)seg->index->dims;
        float dot = 0.0f;
        for (int32_t j = 0; j < seg->index->dims; ++j) dot += query[j] * v[j];
        /* distance = -dot for inner product / dot for cosine */
        if (seg->index->metric == 2) d[i] = -dot;  /* IP：越大越相似 */
        else if (seg->index->metric == 1) {
            float dist = 0.0f;
            for (int32_t j = 0; j < seg->index->dims; ++j) {
                float diff = query[j] - v[j];
                dist += diff * diff;
            }
            d[i] = dist;
        } else d[i] = -dot;  /* fallback */
        ids[i] = (int32_t)(seg->start_id + i);
    }
    /* 选 top-k（简单选择排序） */
    int32_t take = k < n ? k : n;
    for (int32_t i = 0; i < take; ++i) {
        int32_t best = i;
        for (int32_t j = i + 1; j < n; ++j) {
            if (d[j] < d[best]) best = j;
        }
        if (best != i) {
            float td = d[i]; d[i] = d[best]; d[best] = td;
            int32_t ti = ids[i]; ids[i] = ids[best]; ids[best] = ti;
        }
        out_dist[*base_offset + i] = d[i];
        out_id[*base_offset + i] = ids[i];
    }
    *base_offset += take;
    free(d);
    free(ids);
}

int32_t faiss_hnsw_collection_search(faiss_hnsw_collection_t *col,
                                     const float *query, int32_t k,
                                     float *distances, int32_t *ids) {
    if (!col || !query || k <= 0) return 0;
    /* 简化：单段足够大时直查，否则多段各查后 RRF-like 合并 */
    int32_t total_candidates = 0;
    for (int32_t i = 0; i < col->n_segs; ++i) {
        total_candidates += (int32_t)col->segs[i].index->n_total;
    }
    total_candidates += col->buf_count;

    /* 简化：直接遍历所有 segment（含缓冲），按距离取 top-k */
    float *all_d = malloc(sizeof(float) * (size_t)k);
    int32_t *all_i = malloc(sizeof(int32_t) * (size_t)k);
    if (!all_d || !all_i) { free(all_d); free(all_i); return 0; }
    for (int32_t i = 0; i < k; ++i) { all_d[i] = 1e30f; all_i[i] = -1; }

    /* 对每个 segment + 缓冲分别算 top-k，merge */
    int32_t per_seg_topk = k * 2;
    int32_t offset = 0;
    for (int32_t i = 0; i < col->n_segs; ++i) {
        segment_topk(&col->segs[i], query, per_seg_topk, all_d, all_i, &offset);
    }
    /* 缓冲中的向量（未 compact） */
    for (int32_t i = 0; i < col->buf_count; ++i) {
        const float *v = col->buf_vectors + (size_t)i * (size_t)col->dims;
        float dot = 0.0f;
        for (int32_t j = 0; j < col->dims; ++j) dot += query[j] * v[j];
        float dist = (col->metric == 1) ? 0 : -dot;
        if (col->metric == 1) {
            for (int32_t j = 0; j < col->dims; ++j) {
                float diff = query[j] - v[j];
                dist += diff * diff;
            }
        }
        /* 简化插入 */
        int32_t pos = offset < k ? offset++ : -1;
        if (pos >= 0 && dist < all_d[pos]) {
            all_d[pos] = dist; all_i[pos] = col->buf_ids[i];
        }
    }

    /* 输出 top-k */
    for (int32_t i = 0; i < k && i < offset; ++i) {
        distances[i] = all_d[i];
        ids[i] = all_i[i];
    }
    free(all_d); free(all_i);
    return (i < k ? i : k);
}

int32_t faiss_hnsw_collection_ntotal(const faiss_hnsw_collection_t *col) {
    if (!col) return 0;
    int32_t total = 0;
    for (int32_t i = 0; i < col->n_segs; ++i) {
        total += (int32_t)col->segs[i].index->n_total;
    }
    return total + col->buf_count;
}