/**
 * @file retrieve_op.c
 * @brief 检索算子实现
 *
 * 移植自 RAG 系统的 Retriever：
 * - HNSWRetriever：向量检索
 * - BM25Retriever：全文检索
 * - HybridRetriever：混合检索 + RRF 融合
 */

#include "db/rag_executor.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ============================================================
 * 辅助函数
 * ============================================================ */

/**
 * @brief 计算余弦相似度
 */
static float cosine_similarity(const float *a, const float *b, int dim) {
    if (!a || !b || dim <= 0) return 0.0f;

    float dot = 0.0f;
    float norm_a = 0.0f;
    float norm_b = 0.0f;

    for (int i = 0; i < dim; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    norm_a = sqrtf(norm_a);
    norm_b = sqrtf(norm_b);

    if (norm_a < 1e-10f || norm_b < 1e-10f) {
        return 0.0f;
    }

    return dot / (norm_a * norm_b);
}

/**
 * @brief 简化的文本哈希（用于生成 Chunk ID））
 */
static void hash_string(const char *str, char *buf, size_t bufsize) {
    unsigned int hash = 5381;
    int c;

    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }

    snprintf(buf, bufsize, "%08x", hash);
}

/* ============================================================
 * HNSW 检索器状态
 * ============================================================ */

typedef struct hnsw_retriever_state {
    rag_config_t config;
    int          embedding_dim;    /**< 向量维度 */
    int          top_k;            /**< 返回结果数 */
} hnsw_retriever_state_t;

static int hnsw_retriever_init(rag_operator_t *op, rag_context_t *ctx) {
    if (!op || !ctx) return -1;

    hnsw_retriever_state_t *state = (hnsw_retriever_state_t *)calloc(1, sizeof(*state));
    if (!state) return -1;

    state->config = ctx->config;
    state->embedding_dim = 768;  /* 默认维度，后续从向量引擎获取 */
    state->top_k = ctx->config.top_k > 0 ? ctx->config.top_k : RAG_DEFAULT_TOP_K;

    op->state = state;
    op->context = ctx;

    return 0;
}

/**
 * @brief HNSW 检索执行
 *
 * 注意：这里需要调用现有的向量查询接口
 * 简化实现：直接使用 vector_query 接口
 */
static int hnsw_retriever_execute(rag_operator_t *op, void *input, void **output) {
    if (!op || !input || !output) return -1;

    retrieve_input_t *in = (retrieve_input_t *)input;
    retrieve_output_t *out = (retrieve_output_t *)calloc(1, sizeof(*out));
    if (!out) return -1;

    hnsw_retriever_state_t *state = (hnsw_retriever_state_t *)op->state;
    int top_k = (in->top_k > 0) ? in->top_k : state->top_k;

    /* 预分配结果数组 */
    out->results = (retrieve_result_t *)calloc(top_k, sizeof(retrieve_result_t));
    if (!out->results) {
        free(out);
        return -1;
    }

    /* 简化实现：生成模拟结果
     * 实际实现需要调用向量引擎的查询接口 */
    const char *query = in->query;
    int query_len = (in->query_len > 0) ? in->query_len : (int)strlen(query);

    /* 预留：调用 ctx->vector_engine 进行 HNSW 检索 */
    /* vector_query_search(ctx->vector_engine, query_vec, top_k) */

    /* 简化：创建一个模拟的检索结果 */
    /* 在真实实现中，这里会从向量引擎获取实际的检索结果 */
    out->nresults = 0;
    out->processing_time_ms = 1;
    out->hnsw_results_count = 0;

    *output = out;
    return 0;
}

static int hnsw_retriever_cleanup(rag_operator_t *op) {
    if (!op) return -1;
    if (op->state) {
        free(op->state);
        op->state = NULL;
    }
    return 0;
}

/* ============================================================
 * BM25 检索器状态
 * ============================================================ */

typedef struct bm25_retriever_state {
    rag_config_t config;
    int          top_k;
} bm25_retriever_state_t;

static int bm25_retriever_init(rag_operator_t *op, rag_context_t *ctx) {
    if (!op || !ctx) return -1;

    bm25_retriever_state_t *state = (bm25_retriever_state_t *)calloc(1, sizeof(*state));
    if (!state) return -1;

    state->config = ctx->config;
    state->top_k = ctx->config.top_k > 0 ? ctx->config.top_k : RAG_DEFAULT_TOP_K;

    op->state = state;
    op->context = ctx;

    return 0;
}

/**
 * @brief BM25 分数计算（简化实现）
 */
static float bm25_score(const char *query, const char *document,
                        float k1, float b) {
    /* 简化实现：基于词匹配的 BM25 */
    /* 实际实现需要完整的 TF-IDF 计算 */

    if (!query || !document) return 0.0f;

    int query_len = (int)strlen(query);
    int doc_len = (int)strlen(document);

    if (query_len == 0 || doc_len == 0) return 0.0f;

    /* 简单计数匹配词数 */
    int matches = 0;
    const char *p = query;

    while (*p) {
        /* 跳过空白 */
        while (*p && (*p == ' ' || *p == '\t' || *p == '\n')) p++;
        if (!*p) break;

        /* 找词尾 */
        const char *word_end = p;
        while (*word_end && *word_end != ' ' && *word_end != '\t' && *word_end != '\n') {
            word_end++;
        }

        size_t word_len = word_end - p;

        /* 在文档中查找 */
        const char *doc_p = document;
        while ((doc_p = strstr(doc_p, p)) != NULL) {
            /* 检查是否是完整词匹配 */
            char before = (doc_p == document) ? ' ' : doc_p[-1];
            char after = doc_p[word_len];

            if ((before == ' ' || before == '\t' || before == '\n' || before == '\0') &&
                (after == ' ' || after == '\t' || after == '\n' || after == '\0' || after == '.' || after == ',')) {
                matches++;
                break;
            }
            doc_p++;
        }

        p = word_end;
    }

    /* 简化的 BM25 公式 */
    float tf = (float)matches / (doc_len / 10.0f);
    float score = tf / (k1 * (1 - b + b * doc_len / 500.0f) + tf);

    return score;
}

/**
 * @brief BM25 检索执行
 */
static int bm25_retriever_execute(rag_operator_t *op, void *input, void **output) {
    if (!op || !input || !output) return -1;

    retrieve_input_t *in = (retrieve_input_t *)input;
    retrieve_output_t *out = (retrieve_output_t *)calloc(1, sizeof(*out));
    if (!out) return -1;

    bm25_retriever_state_t *state = (bm25_retriever_state_t *)op->state;
    int top_k = (in->top_k > 0) ? in->top_k : state->top_k;

    /* 预分配结果数组 */
    out->results = (retrieve_result_t *)calloc(top_k, sizeof(retrieve_result_t));
    if (!out->results) {
        free(out);
        return -1;
    }

    /* 预留：调用 ctx->doc_engine 进行 BM25 检索 */
    /* doc_engine_search(ctx->doc_engine, query, top_k) */

    /* 简化：创建一个空的检索结果 */
    out->nresults = 0;
    out->processing_time_ms = 1;
    out->bm25_results_count = 0;

    *output = out;
    return 0;
}

static int bm25_retriever_cleanup(rag_operator_t *op) {
    if (!op) return -1;
    if (op->state) {
        free(op->state);
        op->state = NULL;
    }
    return 0;
}

/* ============================================================
 * Hybrid 检索器状态
 * ============================================================ */

typedef struct hybrid_retriever_state {
    rag_config_t     config;
    int              top_k;
    float            rrf_k;
    float            hybrid_weight;

    /* 子检索器状态 */
    hnsw_retriever_state_t hnsw_state;
    bm25_retriever_state_t bm25_state;
} hybrid_retriever_state_t;

static int hybrid_retriever_init(rag_operator_t *op, rag_context_t *ctx) {
    if (!op || !ctx) return -1;

    hybrid_retriever_state_t *state = (hybrid_retriever_state_t *)calloc(1, sizeof(*state));
    if (!state) return -1;

    state->config = ctx->config;
    state->top_k = ctx->config.top_k > 0 ? ctx->config.top_k : RAG_DEFAULT_TOP_K;
    state->rrf_k = ctx->config.rrf_k > 0 ? ctx->config.rrf_k : RAG_DEFAULT_RRF_K;
    state->hybrid_weight = ctx->config.hybrid_weight > 0 ?
                           ctx->config.hybrid_weight : RAG_DEFAULT_HYBRID_WEIGHT;

    /* 初始化子检索器状态 */
    state->hnsw_state.config = ctx->config;
    state->hnsw_state.embedding_dim = 768;
    state->hnsw_state.top_k = state->top_k * 2;

    state->bm25_state.config = ctx->config;
    state->bm25_state.top_k = state->top_k * 2;

    op->state = state;
    op->context = ctx;

    return 0;
}

/**
 * @brief RRF 融合算法
 *
 * RRF(k) = sum(1 / (k + rank)) for each result
 * 详见：https://plg.uwaterloo.ca/~gvcormac/cormacksigir09-rrf.pdf
 */
static void rrf_fusion(retrieve_result_t *hnsw_results, int hnsw_count,
                       retrieve_result_t *bm25_results, int bm25_count,
                       float rrf_k, float hybrid_weight,
                       retrieve_result_t *out_results, int *out_count, int top_k) {
    if (!out_results || !out_count) return;

    /* 使用哈希表存储 RRF 分数 */
    #define MAX_RRF_ITEMS 256
    typedef struct {
        char chunk_id[64];
        float rrf_score;
        retrieve_result_t result;
    } rrf_item_t;

    rrf_item_t items[MAX_RRF_ITEMS];
    int nitems = 0;

    /* 添加 HNSW 结果 */
    for (int i = 0; i < hnsw_count && nitems < MAX_RRF_ITEMS; i++) {
        retrieve_result_t *r = &hnsw_results[i];
        float score = 1.0f / (rrf_k + i + 1);

        /* 检查是否已存在 */
        int found = -1;
        for (int j = 0; j < nitems; j++) {
            if (strcmp(items[j].chunk_id, r->chunk.id) == 0) {
                found = j;
                break;
            }
        }

        if (found >= 0) {
            items[found].rrf_score += score * hybrid_weight;
            items[found].result.vector_score = r->score;
        } else {
            strcpy(items[nitems].chunk_id, r->chunk.id);
            items[nitems].rrf_score = score * hybrid_weight;
            items[nitems].result = *r;
            items[nitems].result.vector_score = r->score;
            items[nitems].result.bm25_score = 0;
            nitems++;
        }
    }

    /* 添加 BM25 结果 */
    for (int i = 0; i < bm25_count && nitems < MAX_RRF_ITEMS; i++) {
        retrieve_result_t *r = &bm25_results[i];
        float score = 1.0f / (rrf_k + i + 1);

        /* 检查是否已存在 */
        int found = -1;
        for (int j = 0; j < nitems; j++) {
            if (strcmp(items[j].chunk_id, r->chunk.id) == 0) {
                found = j;
                break;
            }
        }

        if (found >= 0) {
            items[found].rrf_score += score * (1.0f - hybrid_weight);
            items[found].result.bm25_score = r->score;
        } else {
            strcpy(items[nitems].chunk_id, r->chunk.id);
            items[nitems].rrf_score = score * (1.0f - hybrid_weight);
            items[nitems].result = *r;
            items[nitems].result.vector_score = 0;
            items[nitems].result.bm25_score = r->score;
            nitems++;
        }
    }

    /* 按 RRF 分数排序（冒泡排序简化版） */
    for (int i = 0; i < nitems - 1; i++) {
        for (int j = 0; j < nitems - i - 1; j++) {
            if (items[j].rrf_score < items[j + 1].rrf_score) {
                rrf_item_t tmp = items[j];
                items[j] = items[j + 1];
                items[j + 1] = tmp;
            }
        }
    }

    /* 截取 Top-K */
    *out_count = (nitems < top_k) ? nitems : top_k;
    for (int i = 0; i < *out_count; i++) {
        out_results[i] = items[i].result;
        out_results[i].score = items[i].rrf_score;
        out_results[i].rank = i + 1;
        strcpy(out_results[i].source, "hybrid");
    }
}

/**
 * @brief 加权融合算法
 */
static void weighted_fusion(retrieve_result_t *hnsw_results, int hnsw_count,
                            retrieve_result_t *bm25_results, int bm25_count,
                            float hybrid_weight,
                            retrieve_result_t *out_results, int *out_count, int top_k) {
    if (!out_results || !out_count) return;

    /* 使用哈希表存储加权分数 */
    #define MAX_FUSION_ITEMS 256
    typedef struct {
        char chunk_id[64];
        float hnsw_score;
        float bm25_score;
        retrieve_result_t result;
    } fusion_item_t;

    fusion_item_t items[MAX_FUSION_ITEMS];
    int nitems = 0;

    /* 添加 HNSW 结果 */
    for (int i = 0; i < hnsw_count && nitems < MAX_FUSION_ITEMS; i++) {
        retrieve_result_t *r = &hnsw_results[i];

        int found = -1;
        for (int j = 0; j < nitems; j++) {
            if (strcmp(items[j].chunk_id, r->chunk.id) == 0) {
                found = j;
                break;
            }
        }

        if (found >= 0) {
            items[found].hnsw_score = r->score;
            items[found].result.vector_score = r->score;
        } else {
            strcpy(items[nitems].chunk_id, r->chunk.id);
            items[nitems].hnsw_score = r->score;
            items[nitems].bm25_score = 0;
            items[nitems].result = *r;
            items[nitems].result.vector_score = r->score;
            nitems++;
        }
    }

    /* 添加 BM25 结果 */
    for (int i = 0; i < bm25_count && nitems < MAX_FUSION_ITEMS; i++) {
        retrieve_result_t *r = &bm25_results[i];

        int found = -1;
        for (int j = 0; j < nitems; j++) {
            if (strcmp(items[j].chunk_id, r->chunk.id) == 0) {
                found = j;
                break;
            }
        }

        if (found >= 0) {
            items[found].bm25_score = r->score;
            items[found].result.bm25_score = r->score;
        } else {
            strcpy(items[nitems].chunk_id, r->chunk.id);
            items[nitems].hnsw_score = 0;
            items[nitems].bm25_score = r->score;
            items[nitems].result = *r;
            items[nitems].result.vector_score = 0;
            items[nitems].result.bm25_score = r->score;
            nitems++;
        }
    }

    /* 计算加权分数并排序 */
    for (int i = 0; i < nitems; i++) {
        items[i].result.score = hybrid_weight * items[i].hnsw_score +
                               (1.0f - hybrid_weight) * items[i].bm25_score;
    }

    for (int i = 0; i < nitems - 1; i++) {
        for (int j = 0; j < nitems - i - 1; j++) {
            if (items[j].result.score < items[j + 1].result.score) {
                fusion_item_t tmp = items[j];
                items[j] = items[j + 1];
                items[j + 1] = tmp;
            }
        }
    }

    /* 截取 Top-K */
    *out_count = (nitems < top_k) ? nitems : top_k;
    for (int i = 0; i < *out_count; i++) {
        out_results[i] = items[i].result;
        out_results[i].rank = i + 1;
        strcpy(out_results[i].source, "hybrid");
    }
}

static int hybrid_retriever_execute(rag_operator_t *op, void *input, void **output) {
    if (!op || !input || !output) return -1;

    retrieve_input_t *in = (retrieve_input_t *)input;
    retrieve_output_t *out = (retrieve_output_t *)calloc(1, sizeof(*out));
    if (!out) return -1;

    hybrid_retriever_state_t *state = (hybrid_retriever_state_t *)op->state;
    int top_k = (in->top_k > 0) ? in->top_k : state->top_k;

    /* 预分配结果数组 */
    out->results = (retrieve_result_t *)calloc(top_k, sizeof(retrieve_result_t));
    if (!out->results) {
        free(out);
        return -1;
    }

    /* 预留：调用 ctx->vector_engine 和 ctx->doc_engine 进行检索 */
    /* 简化实现：直接创建空结果 */

    /* 执行 HNSW 检索 */
    retrieve_result_t *hnsw_results = (retrieve_result_t *)calloc(top_k * 2, sizeof(retrieve_result_t));
    int hnsw_count = 0;

    /* 执行 BM25 检索 */
    retrieve_result_t *bm25_results = (retrieve_result_t *)calloc(top_k * 2, sizeof(retrieve_result_t));
    int bm25_count = 0;

    /* RRF 融合或加权融合 */
    if (state->config.fusion_strategy == RAG_FUSION_RRF) {
        rrf_fusion(hnsw_results, hnsw_count, bm25_results, bm25_count,
                   state->rrf_k, state->hybrid_weight,
                   out->results, &out->nresults, top_k);
    } else {
        weighted_fusion(hnsw_results, hnsw_count, bm25_results, bm25_count,
                        state->hybrid_weight,
                        out->results, &out->nresults, top_k);
    }

    free(hnsw_results);
    free(bm25_results);

    out->processing_time_ms = 1;
    out->hnsw_results_count = hnsw_count;
    out->bm25_results_count = bm25_count;

    *output = out;
    return 0;
}

static int hybrid_retriever_cleanup(rag_operator_t *op) {
    if (!op) return -1;
    if (op->state) {
        free(op->state);
        op->state = NULL;
    }
    return 0;
}

/* ============================================================
 * 公共 API 实现
 * ============================================================ */

rag_operator_t *rag_retrieve_op_create(rag_retrieve_strategy_t strategy,
                                        const rag_config_t *config,
                                        rag_context_t *ctx) {
    (void)config;  /* 未使用，配置通过 ctx 传递 */
    (void)ctx;     /* 未使用，预留 */

    rag_operator_t *op = (rag_operator_t *)calloc(1, sizeof(*op));
    if (!op) return NULL;

    op->type = RAG_OP_RETRIEVE;
    op->context = NULL;
    op->state = NULL;

    switch (strategy) {
        case RAG_RETRIEVE_HNSW:
            strcpy(op->name, "HNSWRetriever");
            op->init = hnsw_retriever_init;
            op->execute = hnsw_retriever_execute;
            op->cleanup = hnsw_retriever_cleanup;
            break;

        case RAG_RETRIEVE_BM25:
            strcpy(op->name, "BM25Retriever");
            op->init = bm25_retriever_init;
            op->execute = bm25_retriever_execute;
            op->cleanup = bm25_retriever_cleanup;
            break;

        case RAG_RETRIEVE_HYBRID:
            strcpy(op->name, "HybridRetriever");
            op->init = hybrid_retriever_init;
            op->execute = hybrid_retriever_execute;
            op->cleanup = hybrid_retriever_cleanup;
            break;

        default:
            strcpy(op->name, "HybridRetriever");
            op->init = hybrid_retriever_init;
            op->execute = hybrid_retriever_execute;
            op->cleanup = hybrid_retriever_cleanup;
            break;
    }

    return op;
}

void retrieve_output_free(retrieve_output_t *output) {
    if (!output) return;

    if (output->results) {
        for (int i = 0; i < output->nresults; i++) {
            if (output->results[i].chunk.content) {
                free(output->results[i].chunk.content);
            }
        }
        free(output->results);
    }

    free(output);
}

retrieve_output_t *rag_retrieve(rag_context_t *ctx,
                                 const char *query,
                                 int top_k) {
    if (!ctx || !query) return NULL;

    rag_operator_t *op = rag_retrieve_op_create(ctx->config.retrieve_strategy,
                                                 &ctx->config, ctx);
    if (!op) return NULL;

    if (rag_operator_init(op, ctx) != 0) {
        rag_operator_destroy(op);
        return NULL;
    }

    retrieve_input_t input;
    memset(&input, 0, sizeof(input));
    input.query = query;
    input.top_k = top_k > 0 ? top_k : ctx->config.top_k;
    input.enable_rerank = ctx->config.enable_rerank;
    input.enable_mmr = ctx->config.enable_mmr;
    input.mmr_lambda = ctx->config.mmr_lambda;

    void *output = NULL;
    if (rag_operator_execute(op, &input, &output) != 0) {
        rag_operator_cleanup(op);
        rag_operator_destroy(op);
        return NULL;
    }

    rag_operator_cleanup(op);
    rag_operator_destroy(op);

    return (retrieve_output_t *)output;
}
