/**
 * @file rag_executor.c
 * @brief RAG 执行器核心实现
 *
 * 包含：
 * - RAG 上下文管理
 * - RAG 管线调度器
 */

#include "db/rag_executor.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================
 * RAG 上下文实现
 * ============================================================ */

rag_context_t *rag_context_create(void) {
    rag_context_t *ctx = (rag_context_t *)calloc(1, sizeof(*ctx));
    if (!ctx) return NULL;

    /* 设置默认配置 */
    rag_config_init_defaults(&ctx->config);

    /* 初始化状态 */
    ctx->state.in_transaction = 0;
    ctx->state.txn_id = 0;
    ctx->state.queries_executed = 0;
    ctx->state.error_msg = NULL;

    return ctx;
}

void rag_context_destroy(rag_context_t *ctx) {
    if (!ctx) return;

    if (ctx->state.error_msg) {
        free(ctx->state.error_msg);
    }

    /* 预留：清理向量引擎、文档引擎等 */
    if (ctx->vector_engine) {
        /* vector_engine_destroy(ctx->vector_engine); */
        ctx->vector_engine = NULL;
    }

    if (ctx->doc_engine) {
        /* doc_engine_destroy(ctx->doc_engine); */
        ctx->doc_engine = NULL;
    }

    free(ctx);
}

int rag_context_init(rag_context_t *ctx, const rag_config_t *config) {
    if (!ctx) return -1;

    if (config) {
        ctx->config = *config;
    } else {
        rag_config_init_defaults(&ctx->config);
    }

    /* 预留：初始化向量引擎 */
    /* if (ctx->config.retrieve_strategy != RAG_RETRIEVE_BM25) {
     *     ctx->vector_engine = vector_engine_create();
     *     if (!ctx->vector_engine) return -1;
     * }
     */

    /* 预留：初始化文档引擎 */
    /* if (ctx->config.retrieve_strategy != RAG_RETRIEVE_HNSW) {
     *     ctx->doc_engine = doc_engine_create();
     *     if (!ctx->doc_engine) return -1;
     * }
     */

    return 0;
}

void rag_config_init_defaults(rag_config_t *config) {
    if (!config) return;

    memset(config, 0, sizeof(*config));

    /* 分块配置默认值 */
    config->chunk_strategy = RAG_CHUNK_RECURSIVE;
    config->chunk_size = RAG_DEFAULT_CHUNK_SIZE;
    config->chunk_overlap = RAG_DEFAULT_CHUNK_OVERLAP;
    config->min_chunk_size = RAG_DEFAULT_MIN_CHUNK_SIZE;

    /* 检索配置默认值 */
    config->retrieve_strategy = RAG_RETRIEVE_HYBRID;
    config->top_k = RAG_DEFAULT_TOP_K;
    config->recall_k = RAG_DEFAULT_RECALL_K;
    config->hybrid_weight = RAG_DEFAULT_HYBRID_WEIGHT;
    config->rrf_k = RAG_DEFAULT_RRF_K;

    /* 增强功能默认值 */
    config->enable_rerank = 0;
    config->enable_mmr = 0;
    config->mmr_lambda = 0.5f;

    /* 融合策略默认值 */
    config->fusion_strategy = RAG_FUSION_RRF;
}

const char *rag_context_errmsg(const rag_context_t *ctx) {
    return ctx ? ctx->state.error_msg : NULL;
}

/* ============================================================
 * RAG 管线实现
 * ============================================================ */

rag_pipeline_t *rag_pipeline_create(rag_context_t *ctx,
                                     const rag_config_t *chunk_config,
                                     const rag_config_t *retrieve_config) {
    if (!ctx) return NULL;

    rag_pipeline_t *pipeline = (rag_pipeline_t *)calloc(1, sizeof(*pipeline));
    if (!pipeline) return NULL;

    pipeline->context = ctx;
    pipeline->state = RAG_PIPELINE_IDLE;
    pipeline->step = 0;

    /* 创建分块算子 */
    rag_config_t effective_chunk_config = chunk_config ? *chunk_config : ctx->config;
    pipeline->chunk_op = rag_chunk_op_create(effective_chunk_config.chunk_strategy,
                                              &effective_chunk_config);
    if (!pipeline->chunk_op) {
        free(pipeline);
        return NULL;
    }

    /* 创建检索算子 */
    rag_config_t effective_retrieve_config = retrieve_config ? *retrieve_config : ctx->config;
    pipeline->retrieve_op = rag_retrieve_op_create(effective_retrieve_config.retrieve_strategy,
                                                    &effective_retrieve_config, ctx);
    if (!pipeline->retrieve_op) {
        rag_operator_destroy(pipeline->chunk_op);
        free(pipeline);
        return NULL;
    }

    return pipeline;
}

void rag_pipeline_destroy(rag_pipeline_t *pipeline) {
    if (!pipeline) return;

    if (pipeline->chunk_op) {
        rag_operator_destroy(pipeline->chunk_op);
        pipeline->chunk_op = NULL;
    }

    if (pipeline->retrieve_op) {
        rag_operator_destroy(pipeline->retrieve_op);
        pipeline->retrieve_op = NULL;
    }

    free(pipeline);
}

/**
 * @brief 执行管线步骤（循环直到完成）
 */
static int rag_pipeline_step(rag_pipeline_t *pipeline) {
    if (!pipeline || !pipeline->context) return -1;

    int done = 0;
    while (!done) {
        switch (pipeline->step) {
            case 0:
                /* 初始化分块算子 */
                if (rag_operator_init(pipeline->chunk_op, pipeline->context) != 0) {
                    pipeline->state = RAG_PIPELINE_ERROR;
                    return -1;
                }
                pipeline->step = 1;
                break;

            case 1:
                /* 初始化检索算子 */
                if (rag_operator_init(pipeline->retrieve_op, pipeline->context) != 0) {
                    pipeline->state = RAG_PIPELINE_ERROR;
                    return -1;
                }
                pipeline->step = 2;
                done = 1;
                break;

            default:
                done = 1;
                break;
        }
    }

    return 0;
}

/**
 * @brief 执行端到端 RAG 查询
 *
 * 流程：
 * 1. 输入查询文本
 * 2. 分块算子处理（如果需要）
 * 3. 生成向量（预留：调用 Embedding 服务）
 * 4. 检索算子检索相关 Chunk
 * 5. 返回检索结果
 */
int rag_pipeline_execute(rag_pipeline_t *pipeline,
                          const char *query,
                          int query_len,
                          retrieve_output_t **results) {
    if (!pipeline || !query || !results) return -1;

    pipeline->state = RAG_PIPELINE_RUNNING;
    pipeline->step = 0;

    /* 确保算子已初始化 */
    if (rag_pipeline_step(pipeline) != 0) {
        pipeline->state = RAG_PIPELINE_ERROR;
        return -1;
    }

    /* 预留：分块处理（如果需要） */
    /* chunk_input_t chunk_input = {0};
     * chunk_input.text = query;
     * chunk_input.query_len = query_len;
     *
     * chunk_output_t *chunk_output = NULL;
     * if (rag_operator_execute(pipeline->chunk_op, &chunk_input, (void **)&chunk_output) != 0) {
     *     pipeline->state = RAG_PIPELINE_ERROR;
     *     return -1;
     * }
     */

    /* 预留：生成向量（调用 Embedding 服务） */
    /* float *query_vector = embed_service_->encode(query); */

    /* 执行检索 */
    retrieve_input_t retrieve_input;
    memset(&retrieve_input, 0, sizeof(retrieve_input));
    retrieve_input.query = query;
    retrieve_input.query_len = query_len;
    retrieve_input.top_k = pipeline->context->config.top_k;
    retrieve_input.enable_rerank = pipeline->context->config.enable_rerank;
    retrieve_input.enable_mmr = pipeline->context->config.enable_mmr;
    retrieve_input.mmr_lambda = pipeline->context->config.mmr_lambda;

    retrieve_output_t *retrieve_output = NULL;
    if (rag_operator_execute(pipeline->retrieve_op, &retrieve_input, (void **)&retrieve_output) != 0) {
        pipeline->state = RAG_PIPELINE_ERROR;
        /* 清理分块结果 */
        /* chunk_output_free(chunk_output); */
        return -1;
    }

    /* 清理中间结果 */
    /* chunk_output_free(chunk_output); */

    *results = retrieve_output;
    pipeline->state = RAG_PIPELINE_DONE;

    return 0;
}

/* ============================================================
 * 便捷 API 实现
 * ============================================================ */

retrieve_output_t *rag_query(rag_context_t *ctx,
                              const char *query,
                              int top_k) {
    if (!ctx || !query) return NULL;

    /* 创建管线 */
    rag_pipeline_t *pipeline = rag_pipeline_create(ctx, NULL, NULL);
    if (!pipeline) return NULL;

    /* 覆盖 top_k 配置 */
    if (top_k > 0) {
        ctx->config.top_k = top_k;
    }

    /* 执行查询 */
    retrieve_output_t *results = NULL;
    if (rag_pipeline_execute(pipeline, query, 0, &results) != 0) {
        rag_pipeline_destroy(pipeline);
        return NULL;
    }

    rag_pipeline_destroy(pipeline);
    return results;
}
