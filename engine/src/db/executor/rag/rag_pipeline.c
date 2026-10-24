/**
 * @file rag_pipeline.c
 * @brief 完整 RAG 管线实现 - Wave 3-⑨
 *
 * 端到端 RAG 流程：query -> embed -> retrieve -> generate
 *
 * 特性：
 * - 可配置 Embedding 和 LLM 策略
 * - Prompt 模板管理
 * - 统计信息收集
 */
#include "db/rag_executor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

// =============================================================================
// 内部类型定义
// =============================================================================

/** RAG 管线内部状态 */
typedef struct {
    rag_pipeline_config_t  config;     /**< 管线配置 */
    rag_operator_t        *embed_op;   /**< Embedding 算子 */
    rag_operator_t        *generate_op; /**< 生成算子 */
    rag_operator_t        *chunk_op;   /**< 分块算子（预留） */
    rag_operator_t        *retrieve_op; /**< 检索算子（预留） */

    /** 统计信息 */
    int64_t                total_queries;
    int64_t                total_time_ms;
    int                    cache_hits;
    int                    cache_misses;
} rag_pipeline_impl_t;

// =============================================================================
// Prompt 模板管理
// =============================================================================

/**
 * @brief 设置默认 Prompt 模板
 */
void rag_prompt_template_defaults(rag_prompt_template_t *tmpl) {
    if (!tmpl) return;

    // 默认系统提示词
    snprintf(tmpl->system_prompt, sizeof(tmpl->system_prompt) - 1,
             "You are a helpful AI assistant. Use the following context to answer the user's question.");

    // 默认用户模板
    snprintf(tmpl->user_template, sizeof(tmpl->user_template) - 1,
             "Context:\n{context}\n\nQuestion: {query}\n\nAnswer:");

    // 默认分隔符
    snprintf(tmpl->context_separator, sizeof(tmpl->context_separator) - 1,
             "\n---\n");
}

/**
 * @brief 构建带上下文的 Prompt
 */
int rag_build_prompt(const rag_prompt_template_t *tmpl,
                     const char *query,
                     const rag_chunk_t *chunks,
                     int nchunks,
                     char *output,
                     int output_size) {
    if (!tmpl || !query || !output || output_size <= 0) {
        return -1;
    }

    // 构建上下文字符串
    char context[8192] = {0};
    int context_len = 0;

    if (chunks && nchunks > 0) {
        for (int i = 0; i < nchunks && i < 10; i++) {  // 最多 10 个块
            const rag_chunk_t *chunk = &chunks[i];
            if (chunk->content && chunk->content_len > 0) {
                int written = snprintf(context + context_len,
                                       sizeof(context) - context_len,
                                       "[%d] %.*s%s",
                                       i + 1,
                                       chunk->content_len,
                                       chunk->content,
                                       tmpl->context_separator);

                if (written < 0 || written >= (int)(sizeof(context) - context_len)) {
                    break;
                }
                context_len += written;
            }
        }
    } else {
        snprintf(context, sizeof(context), "(No relevant context found)");
    }

    // 构建完整 Prompt
    int len = snprintf(output, output_size,
                       "%s\n\n%s",
                       tmpl->system_prompt,
                       tmpl->user_template);

    if (len < 0 || len >= output_size) {
        return -1;
    }

    // 替换占位符
    // 替换 {context}
    char *ptr = strstr(output, "{context}");
    if (ptr) {
        size_t prefix_len = ptr - output;
        size_t suffix_len = len - prefix_len - strlen("{context}");
        char suffix[1024] = {0};
        if (suffix_len > 0) {
            strncpy(suffix, ptr + strlen("{context}"), suffix_len);
        }

        int new_len = snprintf(ptr, output_size - prefix_len,
                               "%s%s", context, suffix);
        if (new_len < 0) return -1;
        len = prefix_len + new_len;
    }

    // 替换 {query}
    ptr = strstr(output, "{query}");
    if (ptr) {
        size_t prefix_len = ptr - output;
        size_t suffix_len = len - prefix_len - strlen("{query}");
        char suffix[1024] = {0};
        if (suffix_len > 0) {
            strncpy(suffix, ptr + strlen("{query}"), suffix_len);
        }

        int new_len = snprintf(ptr, output_size - prefix_len,
                               "%s%s", query, suffix);
        if (new_len < 0) return -1;
    }

    return 0;
}

// =============================================================================
// 模拟检索（用于测试）
// =============================================================================

/**
 * @brief 模拟检索结果
 *
 * 实际应用中，这里应该调用 vector_query 或 BM25 检索
 */
static retrieve_output_t *simulate_retrieve(const char *query, int top_k) {
    retrieve_output_t *output = (retrieve_output_t *)malloc(sizeof(retrieve_output_t));
    if (!output) return NULL;

    memset(output, 0, sizeof(retrieve_output_t));

    // 分配结果数组
    output->results = (retrieve_result_t *)malloc(top_k * sizeof(retrieve_result_t));
    if (!output->results) {
        free(output);
        return NULL;
    }

    output->nresults = top_k;
    output->nresults_capacity = top_k;

    // 模拟生成结果
    for (int i = 0; i < top_k; i++) {
        retrieve_result_t *result = &output->results[i];

        // 模拟 Chunk
        snprintf(result->chunk.id, sizeof(result->chunk.id), "chunk_%d", i);
        snprintf(result->chunk.document_id, sizeof(result->chunk.document_id), "doc_%d", i / 3);
        result->chunk.content = (char *)malloc(256);
        if (result->chunk.content) {
            snprintf(result->chunk.content, 256,
                     "This is simulated context for query '%s'. "
                     "This chunk contains relevant information about the topic. "
                     "Generated for testing purposes with rank %d.",
                     query, i + 1);
            result->chunk.content_len = (int)strlen(result->chunk.content);
        }
        result->chunk.chunk_index = i;

        // 模拟分数
        result->score = 1.0f - (float)i / (top_k + 1);
        result->vector_score = result->score * 0.7f;
        result->bm25_score = result->score * 0.3f;
        result->rank = i + 1;
        snprintf(result->source, sizeof(result->source), "simulated");
    }

    return output;
}

/**
 * @brief 释放检索输出（包含 chunk 内容）
 */
void retrieve_output_free_full(retrieve_output_t *output) {
    if (!output) return;

    if (output->results) {
        for (int i = 0; i < output->nresults; i++) {
            free(output->results[i].chunk.content);
        }
        free(output->results);
    }

    free(output);
}

// =============================================================================
// 完整 RAG 管线实现
// =============================================================================

/**
 * @brief 创建完整 RAG 管线
 */
rag_full_pipeline_t *rag_full_pipeline_create(rag_context_t *ctx,
                                               const rag_pipeline_config_t *config) {
    (void)ctx;

    rag_full_pipeline_t *pipeline = (rag_full_pipeline_t *)malloc(sizeof(rag_full_pipeline_t));
    if (!pipeline) return NULL;

    memset(pipeline, 0, sizeof(rag_full_pipeline_t));

    rag_pipeline_impl_t *impl = (rag_pipeline_impl_t *)calloc(1, sizeof(rag_pipeline_impl_t));
    if (!impl) {
        free(pipeline);
        return NULL;
    }

    pipeline->context = ctx;
    pipeline->embed_op = NULL;
    pipeline->generate_op = NULL;
    pipeline->chunk_op = NULL;
    pipeline->retrieve_op = NULL;
    pipeline->state = RAG_PIPELINE_IDLE;

    // 设置配置
    if (config) {
        impl->config = *config;
    } else {
        // 默认配置
        rag_pipeline_config_t default_config = {0};
        default_config.embed_config.strategy = RAG_EMBED_SIMPLE;
        default_config.embed_config.dimension = 768;
        default_config.embed_config.cache_size = 1000;
        default_config.llm_config.strategy = RAG_LLM_SIMPLE;
        default_config.llm_config.max_tokens = 1024;
        default_config.llm_config.temperature = 0.7f;
        default_config.retrieve_strategy = RAG_RETRIEVE_HNSW;
        default_config.top_k = 10;
        default_config.enable_cache = 1;
        rag_prompt_template_defaults(&default_config.prompt_template);
        impl->config = default_config;
    }

    // 创建 Embedding 算子
    impl->embed_op = rag_embed_op_create(impl->config.embed_config.strategy,
                                          &impl->config.embed_config);
    if (!impl->embed_op) {
        free(impl);
        free(pipeline);
        return NULL;
    }
    rag_embed_op_init(impl->embed_op, ctx);

    // 创建 LLM 生成算子
    impl->generate_op = rag_generate_op_create(impl->config.llm_config.strategy,
                                                &impl->config.llm_config);
    if (!impl->generate_op) {
        rag_operator_destroy(impl->embed_op);
        free(impl);
        free(pipeline);
        return NULL;
    }
    rag_generate_op_init(impl->generate_op, ctx);

    pipeline->embed_op = impl->embed_op;
    pipeline->generate_op = impl->generate_op;
    pipeline->impl = impl;

    return pipeline;
}

/**
 * @brief 销毁完整 RAG 管线
 */
void rag_full_pipeline_destroy(rag_full_pipeline_t *pipeline) {
    if (!pipeline) return;

    rag_pipeline_impl_t *impl = (rag_pipeline_impl_t *)pipeline->impl;
    if (impl) {
        if (impl->embed_op) {
            rag_operator_destroy(impl->embed_op);
        }
        if (impl->generate_op) {
            rag_operator_destroy(impl->generate_op);
        }
        if (impl->chunk_op) {
            rag_operator_destroy(impl->chunk_op);
        }
        if (impl->retrieve_op) {
            rag_operator_destroy(impl->retrieve_op);
        }
        free(impl);
    }

    free(pipeline);
}

/**
 * @brief 执行完整 RAG 查询
 *
 * 流程：
 * 1. 检索相关 Chunk
 * 2. 构建 Prompt
 * 3. 调用 LLM 生成
 */
rag_generate_output_t *rag_full_pipeline_execute(rag_full_pipeline_t *pipeline,
                                                  const char *query,
                                                  int top_k) {
    if (!pipeline || !query) return NULL;

    rag_pipeline_impl_t *impl = (rag_pipeline_impl_t *)pipeline->impl;
    if (!impl) return NULL;

    pipeline->state = RAG_PIPELINE_RUNNING;
    int64_t start_time = 0;  // TODO: 获取当前时间

    // Step 1: 检索相关 Chunk
    retrieve_output_t *retrieve_result = rag_full_pipeline_retrieve(pipeline, query, top_k);

    // Step 2: 构建 Prompt
    char prompt[8192] = {0};

    // 转换为 rag_chunk_t 数组
    rag_chunk_t *chunks = NULL;
    int nchunks = 0;

    if (retrieve_result && retrieve_result->results && retrieve_result->nresults > 0) {
        chunks = (rag_chunk_t *)malloc(retrieve_result->nresults * sizeof(rag_chunk_t));
        if (chunks) {
            nchunks = retrieve_result->nresults;
            for (int i = 0; i < nchunks; i++) {
                chunks[i] = retrieve_result->results[i].chunk;
                // 复制 content 指针（因为它是 malloc 的）
                chunks[i].content = retrieve_result->results[i].chunk.content;
            }
        }
    }

    rag_build_prompt(&impl->config.prompt_template, query,
                     chunks, nchunks, prompt, sizeof(prompt));

    // Step 3: 调用 LLM 生成
    rag_generate_output_t *gen_output = rag_generate_execute(impl->generate_op, prompt);

    // 清理
    free(chunks);
    if (retrieve_result) {
        retrieve_output_free_full(retrieve_result);
    }

    // 更新统计
    pipeline->total_queries++;
    impl->total_queries++;

    // 获取缓存统计
    rag_embed_cache_stats_t cache_stats = {0};
    if (impl->embed_op) {
        rag_embed_cache_stats(impl->embed_op, &cache_stats);
    }
    pipeline->cache_hits = cache_stats.hits;
    pipeline->cache_misses = cache_stats.misses;

    pipeline->state = gen_output ? RAG_PIPELINE_DONE : RAG_PIPELINE_ERROR;

    return gen_output;
}

/**
 * @brief 执行 RAG 检索（仅检索，不生成）
 */
retrieve_output_t *rag_full_pipeline_retrieve(rag_full_pipeline_t *pipeline,
                                               const char *query,
                                               int top_k) {
    if (!pipeline || !query) return NULL;

    rag_pipeline_impl_t *impl = (rag_pipeline_impl_t *)pipeline->impl;
    if (!impl) return NULL;

    // 如果配置了检索算子，使用它
    if (impl->retrieve_op && impl->config.retrieve_strategy != RAG_RETRIEVE_HNSW) {
        // TODO: 调用实际检索算子
        // retrieve_input_t input = {0};
        // input.query = query;
        // input.top_k = top_k > 0 ? top_k : impl->config.top_k;
        // void *output = NULL;
        // rag_operator_execute(impl->retrieve_op, &input, &output);
        // return (retrieve_output_t *)output;
    }

    // 使用模拟检索
    int k = top_k > 0 ? top_k : impl->config.top_k;
    return simulate_retrieve(query, k);
}

/**
 * @brief 获取管线统计信息
 */
void rag_full_pipeline_stats(const rag_full_pipeline_t *pipeline,
                             int64_t *total_queries,
                             int64_t *total_time_ms,
                             int *cache_hits,
                             int *cache_misses) {
    if (!pipeline) return;

    if (total_queries) *total_queries = pipeline->total_queries;
    if (total_time_ms) *total_time_ms = pipeline->total_time_ms;
    if (cache_hits) *cache_hits = pipeline->cache_hits;
    if (cache_misses) *cache_misses = pipeline->cache_misses;
}

/**
 * @brief 设置 Prompt 模板
 */
int rag_full_pipeline_set_prompt(rag_full_pipeline_t *pipeline,
                                  const char *system_prompt,
                                  const char *user_template) {
    if (!pipeline) return -1;

    rag_pipeline_impl_t *impl = (rag_pipeline_impl_t *)pipeline->impl;
    if (!impl) return -1;

    if (system_prompt) {
        strncpy(impl->config.prompt_template.system_prompt, system_prompt,
                sizeof(impl->config.prompt_template.system_prompt) - 1);
    }

    if (user_template) {
        strncpy(impl->config.prompt_template.user_template, user_template,
                sizeof(impl->config.prompt_template.user_template) - 1);
    }

    return 0;
}

// =============================================================================
// 便捷函数
// =============================================================================

/**
 * @brief 完整的 RAG 查询（单步调用）
 */
rag_generate_output_t *rag_query_full(const char *query, int top_k,
                                       const rag_pipeline_config_t *config) {
    // 创建管线
    rag_full_pipeline_t *pipeline = rag_full_pipeline_create(NULL, config);
    if (!pipeline) return NULL;

    // 执行查询
    rag_generate_output_t *output = rag_full_pipeline_execute(pipeline, query, top_k);

    // 销毁管线
    rag_full_pipeline_destroy(pipeline);

    return output;
}

// =============================================================================
// 事务集成（Wave 3-⑨ 任务 4.x）
// =============================================================================

/**
 * @brief RAG 事务内部状态
 */
typedef struct {
    void *db_txn;               /**< 底层数据库事务 */
    int  in_transaction;        /**< 是否在事务中 */
    int  cache_cleared;         /**< 缓存是否已清除 */
} rag_transaction_impl_t;

/**
 * @brief 提交 RAG 事务
 */
static int rag_txn_commit(rag_transaction_t *txn) {
    if (!txn) return -1;

    rag_transaction_impl_t *impl = (rag_transaction_impl_t *)txn->impl;
    if (!impl) return -1;

    // TODO: 调用底层数据库事务提交
    // if (impl->db_txn) db_txn_commit(impl->db_txn);

    impl->in_transaction = 0;
    return 0;
}

/**
 * @brief 回滚 RAG 事务
 */
static int rag_txn_rollback(rag_transaction_t *txn) {
    if (!txn) return -1;

    rag_transaction_impl_t *impl = (rag_transaction_impl_t *)txn->impl;
    if (!impl) return -1;

    // 清除 Embedding 缓存
    if (!impl->cache_cleared) {
        // TODO: 清除管线中的缓存
        impl->cache_cleared = 1;
    }

    // TODO: 调用底层数据库事务回滚
    // if (impl->db_txn) db_txn_rollback(impl->db_txn);

    impl->in_transaction = 0;
    return 0;
}

/**
 * @brief 清除事务内的 Embedding 缓存
 */
static void rag_txn_clear_cache(rag_transaction_t *txn) {
    if (!txn) return;

    rag_transaction_impl_t *impl = (rag_transaction_impl_t *)txn->impl;
    if (!impl) return;

    // TODO: 清除所有 Embedding 缓存条目
    impl->cache_cleared = 1;
}

/**
 * @brief 创建 RAG 事务
 */
rag_transaction_t *rag_transaction_create(void *db_txn) {
    rag_transaction_t *txn = (rag_transaction_t *)malloc(sizeof(rag_transaction_t));
    if (!txn) return NULL;

    rag_transaction_impl_t *impl = (rag_transaction_impl_t *)calloc(1, sizeof(rag_transaction_impl_t));
    if (!impl) {
        free(txn);
        return NULL;
    }

    impl->db_txn = db_txn;
    impl->in_transaction = 1;
    impl->cache_cleared = 0;

    txn->db_txn = db_txn;
    txn->commit = rag_txn_commit;
    txn->rollback = rag_txn_rollback;
    txn->clear_cache = rag_txn_clear_cache;
    txn->impl = impl;

    return txn;
}

/**
 * @brief 销毁 RAG 事务
 */
void rag_transaction_destroy(rag_transaction_t *txn) {
    if (!txn) return;

    rag_transaction_impl_t *impl = (rag_transaction_impl_t *)txn->impl;
    if (impl) {
        if (impl->in_transaction) {
            // 自动回滚未提交的事务
            rag_txn_rollback(txn);
        }
        free(impl);
    }

    free(txn);
}
