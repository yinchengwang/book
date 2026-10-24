/**
 * @file doc_vector.c
 * @brief 文档向量检索实现
 */

#include "db/storage/doc/doc_vector.h"
#include "db/index/vector_index/hnsw.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

/* ========================================================================
 * 嵌入存储
 * ======================================================================== */

DocEmbeddingStore *doc_embedding_store_create(void *mem_pool) {
    DocEmbeddingStore *store = (DocEmbeddingStore *)calloc(1, sizeof(DocEmbeddingStore));
    if (!store) return NULL;

    store->capacity = 1024;
    store->embeddings = (DocEmbedding *)calloc(store->capacity, sizeof(DocEmbedding));
    if (!store->embeddings) {
        free(store);
        return NULL;
    }

    store->mem_pool = mem_pool;
    return store;
}

void doc_embedding_store_destroy(DocEmbeddingStore *store) {
    if (!store) return;

    for (size_t i = 0; i < store->num_embeddings; i++) {
        free(store->embeddings[i].vector);
    }
    free(store->embeddings);
    free(store);
}

int doc_embedding_store_add(DocEmbeddingStore *store,
                           const char *doc_id,
                           const char *field_name,
                           const float *vector,
                           int dimension,
                           uint32_t doc_size) {
    if (!store || !doc_id || !field_name || !vector) return -1;

    /* 检查是否已存在 */
    for (size_t i = 0; i < store->num_embeddings; i++) {
        if (strcmp(store->embeddings[i].doc_id, doc_id) == 0 &&
            strcmp(store->embeddings[i].field_name, field_name) == 0) {
            /* 更新 */
            free(store->embeddings[i].vector);
            store->embeddings[i].vector = (float *)malloc(dimension * sizeof(float));
            if (!store->embeddings[i].vector) return -1;
            memcpy(store->embeddings[i].vector, vector, dimension * sizeof(float));
            store->embeddings[i].dimension = dimension;
            store->embeddings[i].doc_size = doc_size;
            return 0;
        }
    }

    /* 扩容 */
    if (store->num_embeddings >= store->capacity) {
        store->capacity *= 2;
        DocEmbedding *new_emb = (DocEmbedding *)realloc(store->embeddings,
            store->capacity * sizeof(DocEmbedding));
        if (!new_emb) return -1;
        store->embeddings = new_emb;
    }

    /* 添加新嵌入 */
    DocEmbedding *emb = &store->embeddings[store->num_embeddings];
    snprintf(emb->doc_id, sizeof(emb->doc_id), "%s", doc_id);
    snprintf(emb->field_name, sizeof(emb->field_name), "%s", field_name);
    emb->vector = (float *)malloc(dimension * sizeof(float));
    if (!emb->vector) return -1;
    memcpy(emb->vector, vector, dimension * sizeof(float));
    emb->dimension = dimension;
    emb->doc_size = doc_size;
    emb->bm25_score = 0;
    emb->vector_score = 0;
    emb->combined_score = 0;

    store->num_embeddings++;
    return 0;
}

DocEmbedding *doc_embedding_store_get(DocEmbeddingStore *store,
                                      const char *doc_id,
                                      const char *field_name) {
    if (!store || !doc_id || !field_name) return NULL;

    for (size_t i = 0; i < store->num_embeddings; i++) {
        if (strcmp(store->embeddings[i].doc_id, doc_id) == 0 &&
            strcmp(store->embeddings[i].field_name, field_name) == 0) {
            return &store->embeddings[i];
        }
    }
    return NULL;
}

int doc_embedding_store_remove(DocEmbeddingStore *store,
                               const char *doc_id,
                               const char *field_name) {
    if (!store || !doc_id || !field_name) return -1;

    for (size_t i = 0; i < store->num_embeddings; i++) {
        if (strcmp(store->embeddings[i].doc_id, doc_id) == 0 &&
            strcmp(store->embeddings[i].field_name, field_name) == 0) {
            free(store->embeddings[i].vector);
            for (size_t j = i; j < store->num_embeddings - 1; j++) {
                store->embeddings[j] = store->embeddings[j + 1];
            }
            store->num_embeddings--;
            return 0;
        }
    }
    return -1;
}

size_t doc_embedding_store_count(DocEmbeddingStore *store) {
    return store ? store->num_embeddings : 0;
}

/* ========================================================================
 * 语义搜索
 * ======================================================================== */

static float euclidean_distance(const float *a, const float *b, int dim) {
    float sum = 0;
    for (int i = 0; i < dim; i++) {
        float d = a[i] - b[i];
        sum += d * d;
    }
    return sqrtf(sum);
}

static float cosine_similarity(const float *a, const float *b, int dim) {
    float dot = 0, norm_a = 0, norm_b = 0;
    for (int i = 0; i < dim; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }
    if (norm_a == 0 || norm_b == 0) return 0;
    return dot / (sqrtf(norm_a) * sqrtf(norm_b));
}

DocSemanticSearchResult *doc_semantic_search(DocEmbeddingStore *store,
                                            const float *query_vector,
                                            int query_dim,
                                            const DocVectorSearchOptions *options) {
    if (!store || !query_vector || !options) return NULL;

    DocSemanticSearchResult *result = (DocSemanticSearchResult *)calloc(1,
        sizeof(DocSemanticSearchResult));
    if (!result) return NULL;

    int top_k = options->top_k > 0 ? options->top_k : 10;
    result->results = (DocVectorSearchResult *)calloc(top_k, sizeof(DocVectorSearchResult));
    if (!result->results) {
        free(result);
        return NULL;
    }

    /* 计算所有文档的向量分数 */
    for (size_t i = 0; i < store->num_embeddings; i++) {
        DocEmbedding *emb = &store->embeddings[i];
        if (emb->dimension != query_dim) continue;

        float dist = euclidean_distance(query_vector, emb->vector, query_dim);
        emb->vector_score = 1.0f / (1.0f + dist);  /* 转换为相似度 */

        /* 检查阈值 */
        if (options->min_score > 0 && emb->vector_score < options->min_score) continue;

        /* 插入排序找到 Top-K */
        int insert_pos = -1;
        for (int j = 0; j < top_k; j++) {
            if (result->results[j].combined_score == 0 ||
                emb->vector_score > result->results[j].vector_score) {
                insert_pos = j;
                break;
            }
        }

        if (insert_pos >= 0) {
            for (int j = top_k - 1; j > insert_pos; j--) {
                result->results[j] = result->results[j - 1];
            }
            DocVectorSearchResult *r = &result->results[insert_pos];
            snprintf(r->doc_id, sizeof(r->doc_id), "%s", emb->doc_id);
            r->vector_score = emb->vector_score;
            r->bm25_score = emb->bm25_score;
            r->combined_score = emb->vector_score;
            r->rank = insert_pos + 1;

            if (options->include_snippets) {
                r->snippet = (char *)malloc(256);
                snprintf(r->snippet, 256, "Doc: %s, field: %s", emb->doc_id, emb->field_name);
            }
            result->num_results++;
        }
    }

    result->total_hits = store->num_embeddings;
    return result;
}

void doc_semantic_search_free(DocSemanticSearchResult *result) {
    if (!result) return;

    for (size_t i = 0; i < result->num_results; i++) {
        free(result->results[i].snippet);
    }
    free(result->results);
    free(result);
}

/* ========================================================================
 * 混合检索
 * ======================================================================== */

DocHybridSearcher *doc_hybrid_searcher_create(DocEmbeddingStore *store,
                                             const DocHybridConfig *config) {
    if (!store) return NULL;

    DocHybridSearcher *searcher = (DocHybridSearcher *)calloc(1, sizeof(DocHybridSearcher));
    if (!searcher) return NULL;

    searcher->embeddings = store;

    if (config) {
        searcher->config = *config;
    } else {
        searcher->config.mode = DOC_HYBRID_RRF;
        searcher->config.vector_weight = 0.5;
        searcher->config.bm25_weight = 0.5;
        searcher->config.rrf_k = 60;
        searcher->config.use_adaptive = false;
    }

    return searcher;
}

void doc_hybrid_searcher_destroy(DocHybridSearcher *searcher) {
    free(searcher);
}

int doc_hybrid_set_bm25_index(DocHybridSearcher *searcher, void *bm25_index) {
    if (!searcher) return -1;
    searcher->bm25_index = bm25_index;
    return 0;
}

DocSemanticSearchResult *doc_hybrid_search(DocHybridSearcher *searcher,
                                          const float *query_vector,
                                          int query_dim,
                                          const char *query_text,
                                          const DocVectorSearchOptions *options) {
    if (!searcher || !options) return NULL;

    int top_k = options->top_k > 0 ? options->top_k : 10;

    DocSemanticSearchResult *result = (DocSemanticSearchResult *)calloc(1,
        sizeof(DocSemanticSearchResult));
    if (!result) return NULL;

    result->results = (DocVectorSearchResult *)calloc(top_k * 2, sizeof(DocVectorSearchResult));

    /* 收集所有文档 ID */
    size_t num_docs = searcher->embeddings->num_embeddings;
    char **doc_ids = (char **)calloc(num_docs, sizeof(char *));
    double *vector_scores = (double *)calloc(num_docs, sizeof(double));
    double *bm25_scores = (double *)calloc(num_docs, sizeof(double));
    double *combined_scores = (double *)calloc(num_docs, sizeof(double));

    for (size_t i = 0; i < num_docs; i++) {
        doc_ids[i] = searcher->embeddings->embeddings[i].doc_id;
    }

    /* 向量搜索 */
    if (query_vector && query_dim > 0) {
        for (size_t i = 0; i < num_docs; i++) {
            DocEmbedding *emb = &searcher->embeddings->embeddings[i];
            if (emb->dimension == query_dim) {
                float dist = euclidean_distance(query_vector, emb->vector, query_dim);
                vector_scores[i] = 1.0 / (1.0 + dist);
                emb->vector_score = vector_scores[i];
            }
        }
    }

    /* BM25 分数 */
    for (size_t i = 0; i < num_docs; i++) {
        DocEmbedding *emb = &searcher->embeddings->embeddings[i];
        bm25_scores[i] = emb->bm25_score;
    }

    /* 分数融合 */
    DocHybridConfig *cfg = &searcher->config;
    if (cfg->mode == DOC_HYBRID_WEIGHTED) {
        for (size_t i = 0; i < num_docs; i++) {
            combined_scores[i] = cfg->vector_weight * vector_scores[i] +
                                cfg->bm25_weight * bm25_scores[i];
        }
    } else if (cfg->mode == DOC_HYBRID_RRF) {
        /* RRF 融合 */
        double *rank_scores = (double *)calloc(num_docs * 2, sizeof(double));
        double *v_scores = vector_scores;
        double *b_scores = bm25_scores;

        /* 向量排名 */
        int *v_rank = (int *)malloc(num_docs * sizeof(int));
        for (size_t i = 0; i < num_docs; i++) v_rank[i] = i;
        for (size_t i = 0; i < num_docs; i++) {
            for (size_t j = i + 1; j < num_docs; j++) {
                if (v_scores[v_rank[j]] > v_scores[v_rank[i]]) {
                    int tmp = v_rank[i]; v_rank[i] = v_rank[j]; v_rank[j] = tmp;
                }
            }
        }
        for (size_t i = 0; i < num_docs; i++) {
            rank_scores[v_rank[i]] += 1.0 / (cfg->rrf_k + i + 1);
        }

        /* BM25 排名 */
        int *b_rank = (int *)malloc(num_docs * sizeof(int));
        for (size_t i = 0; i < num_docs; i++) b_rank[i] = i;
        for (size_t i = 0; i < num_docs; i++) {
            for (size_t j = i + 1; j < num_docs; j++) {
                if (b_scores[b_rank[j]] > b_scores[b_rank[i]]) {
                    int tmp = b_rank[i]; b_rank[i] = b_rank[j]; b_rank[j] = tmp;
                }
            }
        }
        for (size_t i = 0; i < num_docs; i++) {
            rank_scores[b_rank[i]] += 1.0 / (cfg->rrf_k + i + 1);
        }

        for (size_t i = 0; i < num_docs; i++) {
            combined_scores[i] = rank_scores[i];
        }

        free(v_rank);
        free(b_rank);
        free(rank_scores);
    } else {
        /* 默认使用向量分数 */
        for (size_t i = 0; i < num_docs; i++) {
            combined_scores[i] = vector_scores[i];
        }
    }

    /* 选择 Top-K */
    int *indices = (int *)malloc(num_docs * sizeof(int));
    for (size_t i = 0; i < num_docs; i++) indices[i] = i;
    for (size_t i = 0; i < num_docs && (size_t)i < top_k * 2; i++) {
        for (size_t j = i + 1; j < num_docs; j++) {
            if (combined_scores[indices[j]] > combined_scores[indices[i]]) {
                int tmp = indices[i]; indices[i] = indices[j]; indices[j] = tmp;
            }
        }
    }

    for (size_t i = 0; i < num_docs && result->num_results < (size_t)top_k; i++) {
        int idx = indices[i];
        DocEmbedding *emb = &searcher->embeddings->embeddings[idx];
        DocVectorSearchResult *r = &result->results[result->num_results];

        snprintf(r->doc_id, sizeof(r->doc_id), "%s", emb->doc_id);
        r->vector_score = vector_scores[idx];
        r->bm25_score = bm25_scores[idx];
        r->combined_score = combined_scores[idx];
        r->rank = result->num_results + 1;

        if (options->include_snippets) {
            r->snippet = (char *)malloc(256);
            snprintf(r->snippet, 256, "Doc: %s", emb->doc_id);
        }

        result->num_results++;
    }

    result->total_hits = num_docs;

    free(doc_ids);
    free(vector_scores);
    free(bm25_scores);
    free(combined_scores);
    free(indices);

    return result;
}

/* ========================================================================
 * 分数融合
 * ======================================================================== */

void doc_score_fusion_rrf(const double **doc_scores,
                         size_t num_lists,
                         int k,
                         double *out_scores,
                         size_t num_docs) {
    if (!doc_scores || !out_scores || num_docs == 0) return;

    if (k <= 0) k = 60;

    memset(out_scores, 0, num_docs * sizeof(double));

    for (size_t list_idx = 0; list_idx < num_lists; list_idx++) {
        const double *scores = doc_scores[list_idx];
        /* 获取排名 */
        int *ranks = (int *)malloc(num_docs * sizeof(int));
        for (size_t i = 0; i < num_docs; i++) ranks[i] = i;
        for (size_t i = 0; i < num_docs; i++) {
            for (size_t j = i + 1; j < num_docs; j++) {
                if (scores[ranks[j]] > scores[ranks[i]]) {
                    int tmp = ranks[i]; ranks[i] = ranks[j]; ranks[j] = tmp;
                }
            }
        }
        for (size_t i = 0; i < num_docs; i++) {
            out_scores[ranks[i]] += 1.0 / (k + i + 1);
        }
        free(ranks);
    }
}

void doc_score_fusion_weighted(const double **doc_scores,
                              const double *weights,
                              size_t num_lists,
                              double *out_scores,
                              size_t num_docs) {
    if (!doc_scores || !weights || !out_scores || num_docs == 0) return;

    memset(out_scores, 0, num_docs * sizeof(double));

    for (size_t list_idx = 0; list_idx < num_lists; list_idx++) {
        for (size_t i = 0; i < num_docs; i++) {
            out_scores[i] += weights[list_idx] * doc_scores[list_idx][i];
        }
    }
}

void doc_normalize_scores(double *scores, size_t num_docs) {
    if (!scores || num_docs == 0) return;

    double min_score = scores[0], max_score = scores[0];
    for (size_t i = 1; i < num_docs; i++) {
        if (scores[i] < min_score) min_score = scores[i];
        if (scores[i] > max_score) max_score = scores[i];
    }

    double range = max_score - min_score;
    if (range > 0) {
        for (size_t i = 0; i < num_docs; i++) {
            scores[i] = (scores[i] - min_score) / range;
        }
    }
}

void doc_rank_results(DocVectorSearchResult *results,
                     size_t num_results,
                     const DocHybridConfig *config) {
    if (!results || num_results == 0) return;

    /* 计算综合分数 */
    double vw = config ? config->vector_weight : 0.5;
    double bw = config ? config->bm25_weight : 0.5;

    for (size_t i = 0; i < num_results; i++) {
        results[i].combined_score = vw * results[i].vector_score +
                                   bw * results[i].bm25_score;
    }

    /* 按综合分数排序 */
    for (size_t i = 0; i < num_results; i++) {
        for (size_t j = i + 1; j < num_results; j++) {
            if (results[j].combined_score > results[i].combined_score) {
                DocVectorSearchResult tmp = results[i];
                results[i] = results[j];
                results[j] = tmp;
            }
        }
    }

    /* 更新排名 */
    for (size_t i = 0; i < num_results; i++) {
        results[i].rank = i + 1;
    }
}

/* ========================================================================
 * SQL 函数
 * ======================================================================== */

static DocEmbeddingStore *g_default_store = NULL;

size_t doc_sql_search(const char *collection,
                     const char *query_vector,
                     const char *query_text,
                     int top_k,
                     DocVectorSearchResult **results) {
    (void)collection;

    if (!results) return 0;

    DocVectorSearchOptions opts = {
        .top_k = top_k > 0 ? top_k : 10,
        .min_score = 0,
        .include_scores = true,
        .include_snippets = true,
        .snippet_length = 256,
        .use_rerank = false
    };

    DocEmbeddingStore *store = g_default_store;
    if (!store) {
        store = doc_embedding_store_create(NULL);
        g_default_store = store;
    }

    /* 解析查询向量 */
    float *vec = NULL;
    int dim = 0;

    if (query_vector && strlen(query_vector) > 0) {
        /* 简单解析: [0.1, 0.2, ...] */
        const char *p = query_vector;
        while (*p && *p != '[') p++;
        if (*p == '[') {
            p++;
            int count = 0;
            const char *q = p;
            while (*q) {
                if (*q == ',') count++;
                q++;
            }
            dim = count + 1;
            vec = (float *)malloc(dim * sizeof(float));
            for (int i = 0; i < dim; i++) {
                vec[i] = (float)atof(p);
                while (*p && *p != ',' && *p != ']') p++;
                if (*p == ',') p++;
            }
        }
    }

    DocSemanticSearchResult *sr = doc_semantic_search(store, vec, dim, &opts);
    *results = sr ? sr->results : NULL;
    size_t count = sr ? sr->num_results : 0;

    free(vec);
    free(sr);

    return count;
}

int doc_sql_embed(const char *document,
                 const char *field_name,
                 const char *model,
                 float **out_vector,
                 int *out_dim) {
    (void)document; (void)field_name; (void)model;

    /* 简化实现：生成随机向量 */
    if (!out_vector || !out_dim) return -1;

    *out_dim = 128;  /* 默认维度 */
    *out_vector = (float *)malloc(*out_dim * sizeof(float));

    if (!*out_vector) return -1;

    for (int i = 0; i < *out_dim; i++) {
        (*out_vector)[i] = (float)rand() / RAND_MAX;
    }

    return 0;
}
