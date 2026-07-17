/*
 * vector_index_c_api.c
 *
 * 向量索引 C API 导出层 - 供 Python ctypes 调用
 *
 * 当前支持: HNSW (通过 vector_index 静态库)
 * 暂不支持: DiskANN (diskann_fresh.h 是空存根)
 */

#include "vector_index_c_api.h"
#include <db/index/vector_index/faiss_hnsw/faiss_hnsw.h>
#include <db/index/vector_index/delete/vector_delete_bitmap.h>

#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * HNSW 索引包装器
 * ============================================================================ */

typedef struct hnsw_context {
    faiss_hnsw_t *index;
    int32_t ef_search;
} hnsw_context_t;

void* hnsw_create(int32_t dim, int32_t M, int32_t ef_construction, int32_t metric) {
    distance_metric_t dist_metric = (metric == 0) ? DISTANCE_L2 : DISTANCE_IP;
    faiss_hnsw_t *index = faiss_hnsw_index_create(M, dim, ef_construction, dist_metric, QUANTIZATION_TYPE_NONE);
    if (index == NULL) return NULL;

    hnsw_context_t *ctx = (hnsw_context_t *)malloc(sizeof(hnsw_context_t));
    if (ctx == NULL) {
        faiss_hnsw_index_drop(index);
        return NULL;
    }
    ctx->index = index;
    ctx->ef_search = 64;
    return (void *)ctx;
}

int32_t hnsw_build(void* ctx_ptr, int32_t n, const float* vectors) {
    if (ctx_ptr == NULL || vectors == NULL) return -1;
    return faiss_hnsw_index_add(((hnsw_context_t*)ctx_ptr)->index, n, vectors);
}

int32_t hnsw_search(void* ctx_ptr, const float* query, int32_t k, int32_t ef_search,
                    int32_t* ids, float* distances) {
    if (ctx_ptr == NULL || query == NULL || ids == NULL || distances == NULL) return -1;
    hnsw_context_t *ctx = (hnsw_context_t *)ctx_ptr;
    int32_t search_ef = (ef_search > 0) ? ef_search : ctx->ef_search;
    return faiss_hnsw_index_search(ctx->index, query, k, search_ef, distances, ids);
}

int32_t hnsw_search_batch(void* ctx_ptr, const float* queries, int32_t n_queries,
                          int32_t k, int32_t ef_search, int32_t* ids, float* distances) {
    if (ctx_ptr == NULL || queries == NULL || ids == NULL || distances == NULL) return -1;
    hnsw_context_t *ctx = (hnsw_context_t *)ctx_ptr;
    int32_t search_ef = (ef_search > 0) ? ef_search : ctx->ef_search;
    int32_t success_count = 0;
    for (int32_t i = 0; i < n_queries; i++) {
        if (faiss_hnsw_index_search(ctx->index, queries + i * ctx->index->dims, k, search_ef,
                                    distances + i * k, ids + i * k) >= 0) success_count++;
    }
    return success_count;
}

int64_t hnsw_get_size(void* ctx_ptr) {
    if (ctx_ptr == NULL) return 0;
    faiss_hnsw_t *index = ((hnsw_context_t*)ctx_ptr)->index;
    int64_t size = 0;
    if (index->vectors && index->n_total > 0) size += index->n_total * index->dims * sizeof(float);
    if (index->levels) size += index->levels_size * sizeof(int32_t);
    if (index->nbs) size += index->nb_size * sizeof(int32_t);
    if (index->offsets) size += index->offsets_size * sizeof(int32_t);
    return size;
}

void hnsw_destroy(void* ctx_ptr) {
    if (ctx_ptr == NULL) return;
    hnsw_context_t *ctx = (hnsw_context_t *)ctx_ptr;
    if (ctx->index) faiss_hnsw_index_drop(ctx->index);
    free(ctx);
}

/* ============================================================================
 * IVF-PQ 索引 (简化内存实现，等效暴力搜索)
 * ============================================================================ */

#include <math.h>

typedef struct ivf_pq_context {
    float *vectors;
    int32_t *ids;
    int32_t dims;
    int32_t n_vectors;
    int32_t capacity;
} ivf_pq_context_t;

void* ivf_pq_create(int32_t nlist, int32_t pq_m, int32_t pq_bits, int32_t dim) {
    (void)nlist; (void)pq_m; (void)pq_bits;
    ivf_pq_context_t *ctx = (ivf_pq_context_t *)malloc(sizeof(ivf_pq_context_t));
    if (!ctx) return NULL;
    ctx->dims = dim;
    ctx->n_vectors = 0;
    ctx->capacity = 1024;
    ctx->vectors = (float *)malloc(ctx->capacity * dim * sizeof(float));
    ctx->ids = (int32_t *)malloc(ctx->capacity * sizeof(int32_t));
    return (void *)ctx;
}

void ivf_pq_set_nprobe(void* ctx, int32_t nprobe) { (void)ctx; (void)nprobe; }
int32_t ivf_pq_train(void* ctx, int32_t n, const float* v) { (void)ctx; (void)n; (void)v; return 0; }

int32_t ivf_pq_add(void* ctx_ptr, int32_t n, const float* vectors, const int32_t* ids) {
    if (!ctx_ptr || !vectors) return -1;
    ivf_pq_context_t *ctx = (ivf_pq_context_t *)ctx_ptr;
    for (int32_t i = 0; i < n; i++) {
        if (ctx->n_vectors >= ctx->capacity) {
            ctx->capacity *= 2;
            ctx->vectors = (float *)realloc(ctx->vectors, ctx->capacity * ctx->dims * sizeof(float));
            ctx->ids = (int32_t *)realloc(ctx->ids, ctx->capacity * sizeof(int32_t));
        }
        memcpy(ctx->vectors + ctx->n_vectors * ctx->dims, vectors + i * ctx->dims, ctx->dims * sizeof(float));
        ctx->ids[ctx->n_vectors] = ids ? ids[i] : ctx->n_vectors;
        ctx->n_vectors++;
    }
    return n;
}

static float l2_dist(const float *a, const float *b, int32_t dim) {
    float sum = 0.0f;
    for (int32_t i = 0; i < dim; i++) { float d = a[i] - b[i]; sum += d * d; }
    return sqrtf(sum);
}

int32_t ivf_pq_search(void* ctx_ptr, const float* query, int32_t k, int32_t* ids, float* distances) {
    if (!ctx_ptr || !query || !ids || !distances) return -1;
    ivf_pq_context_t *ctx = (ivf_pq_context_t *)ctx_ptr;
    if (ctx->n_vectors == 0) return 0;
    for (int32_t i = 0; i < k; i++) ids[i] = -1, distances[i] = 1e9f;
    for (int32_t i = 0; i < ctx->n_vectors; i++) {
        float dist = l2_dist(query, ctx->vectors + i * ctx->dims, ctx->dims);
        if (dist < distances[k-1]) {
            distances[k-1] = dist;
            ids[k-1] = ctx->ids[i];
            for (int32_t j = k-2; j >= 0; j--) {
                if (distances[j+1] < distances[j]) {
                    float td = distances[j]; distances[j] = distances[j+1]; distances[j+1] = td;
                    int32_t ti = ids[j]; ids[j] = ids[j+1]; ids[j+1] = ti;
                }
            }
        }
    }
    return k;
}

int32_t ivf_pq_search_batch(void* ctx, const float* q, int32_t n, int32_t k, int32_t* ids, float* d) {
    for (int32_t i = 0; i < n; i++) ivf_pq_search(ctx, q + i * ((ivf_pq_context_t*)ctx)->dims, k, ids + i * k, d + i * k);
    return n;
}

int64_t ivf_pq_get_size(void* ctx) { return ctx ? ((ivf_pq_context_t*)ctx)->n_vectors * ((ivf_pq_context_t*)ctx)->dims * sizeof(float) : 0; }

void ivf_pq_destroy(void* ctx) {
    if (!ctx) return;
    ivf_pq_context_t *c = (ivf_pq_context_t *)ctx;
    if (c->vectors) free(c->vectors);
    if (c->ids) free(c->ids);
    free(c);
}

/* ============================================================================
 * LSH 索引
 * ============================================================================ */

typedef struct lsh_context {
    int32_t dim, num_hash, table_size;
    float **hash_planes;
    int32_t *table_ids, *table_counts;
    float **vectors;
    int32_t *vec_ids;
    int32_t n_vectors, capacity;
} lsh_context_t;

void* lsh_create(int32_t dim, int32_t num_hash, int32_t table_size) {
    lsh_context_t *ctx = (lsh_context_t *)malloc(sizeof(lsh_context_t));
    if (!ctx) return NULL;
    ctx->dim = dim; ctx->num_hash = num_hash; ctx->table_size = table_size;
    ctx->n_vectors = 0; ctx->capacity = 1024;
    ctx->hash_planes = (float **)malloc(num_hash * sizeof(float *));
    for (int32_t i = 0; i < num_hash; i++) {
        ctx->hash_planes[i] = (float *)malloc(dim * sizeof(float));
        for (int32_t j = 0; j < dim; j++) ctx->hash_planes[i][j] = ((float)rand() / RAND_MAX - 0.5f) * 2.0f;
    }
    ctx->table_ids = (int32_t *)calloc(table_size * num_hash, sizeof(int32_t));
    ctx->table_counts = (int32_t *)calloc(table_size, sizeof(int32_t));
    ctx->vectors = (float **)malloc(ctx->capacity * sizeof(float *));
    ctx->vec_ids = (int32_t *)malloc(ctx->capacity * sizeof(int32_t));
    for (int32_t i = 0; i < ctx->capacity; i++) ctx->vectors[i] = (float *)malloc(dim * sizeof(float));
    return (void *)ctx;
}

static int32_t lsh_hash(lsh_context_t *ctx, const float *v) {
    int32_t bucket = 0;
    for (int32_t i = 0; i < ctx->num_hash; i++) {
        float dot = 0.0f;
        for (int32_t j = 0; j < ctx->dim; j++) dot += v[j] * ctx->hash_planes[i][j];
        if (dot > 0) bucket |= (1 << i);
    }
    return bucket % ctx->table_size;
}

int32_t lsh_add(void* ctx_ptr, int32_t n, const float* vectors, const int32_t* ids) {
    if (!ctx_ptr || !vectors) return -1;
    lsh_context_t *ctx = (lsh_context_t *)ctx_ptr;
    int32_t added = 0;
    for (int32_t i = 0; i < n; i++) {
        if (ctx->n_vectors >= ctx->capacity) {
            ctx->capacity *= 2;
            ctx->vectors = (float **)realloc(ctx->vectors, ctx->capacity * sizeof(float *));
            ctx->vec_ids = (int32_t *)realloc(ctx->vec_ids, ctx->capacity * sizeof(int32_t));
            for (int32_t j = ctx->n_vectors; j < ctx->capacity; j++) ctx->vectors[j] = (float *)malloc(ctx->dim * sizeof(float));
        }
        memcpy(ctx->vectors[ctx->n_vectors], vectors + i * ctx->dim, ctx->dim * sizeof(float));
        ctx->vec_ids[ctx->n_vectors] = ids ? ids[i] : ctx->n_vectors;
        int32_t bucket = lsh_hash(ctx, vectors + i * ctx->dim);
        int32_t offset = bucket * ctx->num_hash + ctx->table_counts[bucket];
        if (ctx->table_counts[bucket] < ctx->num_hash) {
            ctx->table_ids[offset] = ctx->n_vectors;
            ctx->table_counts[bucket]++;
            ctx->n_vectors++;
            added++;
        }
    }
    return added;
}

int32_t lsh_search(void* ctx_ptr, const float* query, int32_t k, int32_t* ids, float* distances) {
    if (!ctx_ptr || !query || !ids || !distances) return -1;
    lsh_context_t *ctx = (lsh_context_t *)ctx_ptr;
    int32_t candidates[1024], n_candidates = 0;
    int32_t bucket = lsh_hash(ctx, query);
    for (int32_t b = bucket - 1; b <= bucket + 1; b++) {
        int32_t ab = ((b % ctx->table_size) + ctx->table_size) % ctx->table_size;
        for (int32_t i = 0; i < ctx->table_counts[ab] && n_candidates < 1024; i++)
            candidates[n_candidates++] = ctx->table_ids[ab * ctx->num_hash + i];
    }
    typedef struct { int32_t id; float dist; } result_t;
    result_t results[1024];
    for (int32_t i = 0; i < n_candidates; i++) {
        results[i].id = ctx->vec_ids[candidates[i]];
        results[i].dist = l2_dist(query, ctx->vectors[candidates[i]], ctx->dim);
    }
    for (int32_t i = 0; i < k && i < n_candidates; i++) {
        int32_t min_idx = i;
        for (int32_t j = i + 1; j < n_candidates; j++) if (results[j].dist < results[min_idx].dist) min_idx = j;
        ids[i] = results[min_idx].id;
        distances[i] = results[min_idx].dist;
        result_t tmp = results[i]; results[i] = results[min_idx]; results[min_idx] = tmp;
    }
    return (k < n_candidates) ? k : n_candidates;
}

int32_t lsh_search_batch(void* ctx, const float* q, int32_t n, int32_t k, int32_t* ids, float* d) {
    for (int32_t i = 0; i < n; i++) lsh_search(ctx, q + i * ((lsh_context_t*)ctx)->dim, k, ids + i * k, d + i * k);
    return n;
}

int64_t lsh_get_size(void* ctx) { return ctx ? ((lsh_context_t*)ctx)->n_vectors * ((lsh_context_t*)ctx)->dim * sizeof(float) : 0; }

void lsh_destroy(void* ctx) {
    if (!ctx) return;
    lsh_context_t *c = (lsh_context_t *)ctx;
    for (int32_t i = 0; i < c->num_hash; i++) free(c->hash_planes[i]);
    free(c->hash_planes);
    free(c->table_ids);
    free(c->table_counts);
    for (int32_t i = 0; i < c->capacity; i++) free(c->vectors[i]);
    free(c->vectors);
    free(c->vec_ids);
    free(c);
}
