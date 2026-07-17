/**
 * @file generate_op.c
 * @brief LLM 生成算子实现 - Wave 3-⑨
 *
 * 支持：
 * - Simple: 关键词模拟生成（用于测试/降级）
 * - Ollama: Ollama API 调用（同步和流式）
 *
 * 特性：
 * - Prompt 模板管理
 * - 同步/流式生成
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

/** GenerateOp 实现 */
typedef struct {
    rag_llm_config_t config;           /**< 配置 */
    int              is_ready;         /**< 服务是否就绪 */

    // Simple 模式专用
    char             last_query[256];  /**< 上次查询（用于模拟） */

    // Ollama 模式专用
    void            *curl_handle;      /**< CURL 句柄（预留） */
} generate_op_impl_t;

// =============================================================================
// 工具函数
// =============================================================================

/**
 * @brief 检查文本是否包含关键词
 */
static int contains_keyword(const char *text, const char *keyword) {
    if (!text || !keyword) return 0;
    return strstr(text, keyword) != NULL;
}

/**
 * @brief 生成模拟回答
 *
 * 基于关键词匹配生成简单的模拟回答
 */
static char *simple_generate(const char *prompt, const rag_llm_config_t *config) {
    if (!prompt) return NULL;

    // 定义关键词和对应的回答模板
    struct {
        const char *keyword;
        const char *response;
    } responses[] = {
        {"hello", "Hello! I'm a RAG assistant. How can I help you today?"},
        {"what is", "Based on the retrieved information, "},
        {"how to", "Here's how you can do that: "},
        {"why", "The reason is: "},
        {"who", "Based on the documents, "},
        {"when", "According to the records, "},
        {"where", "From the context, "},
        {"explain", "Let me explain: "},
        {"difference", "The main differences are: "},
        {"compare", "When comparing, we can see that "},
        {"code", "Here's an example code snippet: "},
        {"error", "This error typically occurs when "},
        {"fix", "To fix this issue, you should "},
        {"rag", "RAG (Retrieval-Augmented Generation) combines retrieval with generation. "},
        {"vector", "Vector databases store embeddings for semantic search. "},
        {"embedding", "Embeddings convert text into numerical vectors for ML models. "},
    };

    int num_responses = sizeof(responses) / sizeof(responses[0]);

    // 检查是否有匹配的关键字
    for (int i = 0; i < num_responses; i++) {
        if (contains_keyword(prompt, responses[i].keyword)) {
            // 找到匹配，构建回答
            size_t max_len = 1024;
            char *response = (char *)malloc(max_len);
            if (!response) return NULL;

            snprintf(response, max_len,
                     "%s The retrieved context provides relevant information about this topic. "
                     "The system successfully retrieved relevant documents and generated this response. "
                     "This is a simulated answer for testing purposes.",
                     responses[i].response);

            return response;
        }
    }

    // 默认回答
    char *response = (char *)malloc(512);
    if (!response) return NULL;

    snprintf(response, 512,
             "I understand your query. Based on the retrieved context, "
             "I can provide the following information. The RAG system has "
             "successfully retrieved relevant documents and generated this response. "
             "This is a placeholder answer for testing purposes. "
             "In production, a real LLM would generate a more detailed response.");

    return response;
}

// =============================================================================
// Ollama LLM 实现（预留）
// =============================================================================

/**
 * @brief Ollama 同步生成（预留）
 *
 * 实际实现需要 libcurl 依赖
 */
static char *ollama_generate(const char *prompt, const rag_llm_config_t *config) {
    (void)prompt;
    (void)config;

    // TODO: 使用 libcurl 调用 Ollama API
    // POST http://localhost:11434/api/generate
    // Body: {"model": "llama3.2", "prompt": "...", "stream": false}
    // Response: {"response": "..."}

    return NULL;
}

/**
 * @brief Ollama 流式生成（预留）
 */
static int ollama_generate_stream(const char *prompt,
                                   const rag_llm_config_t *config,
                                   rag_stream_callback_t callback,
                                   void *user_data) {
    (void)prompt;
    (void)config;
    (void)callback;
    (void)user_data;

    // TODO: 使用 libcurl 调用 Ollama API
    // POST http://localhost:11434/api/generate
    // Body: {"model": "llama3.2", "prompt": "...", "stream": true}
    // Response: SSE 流

    return -1;
}

// =============================================================================
// GenerateOp 算子实现
// =============================================================================

/**
 * @brief 创建 GenerateOp 算子
 */
rag_operator_t *rag_generate_op_create(rag_llm_strategy_t strategy,
                                        const rag_llm_config_t *config) {
    rag_operator_t *op = (rag_operator_t *)malloc(sizeof(rag_operator_t));
    if (!op) return NULL;

    memset(op, 0, sizeof(rag_operator_t));

    generate_op_impl_t *impl = (generate_op_impl_t *)calloc(1, sizeof(generate_op_impl_t));
    if (!impl) {
        free(op);
        return NULL;
    }

    op->type = RAG_OP_GENERATE;
    snprintf(op->name, sizeof(op->name) - 1, "generate_op_%d", strategy);
    op->state = impl;

    // 设置配置
    if (config) {
        impl->config = *config;
    } else {
        // 默认配置
        impl->config.strategy = RAG_LLM_SIMPLE;
        strncpy(impl->config.model, "simple", sizeof(impl->config.model) - 1);
        impl->config.max_tokens = 1024;
        impl->config.temperature = 0.7f;
        impl->config.top_p = 0.9f;
        impl->config.top_k = 40;
        impl->config.repeat_penalty = 1.1f;
    }

    // Simple 模式始终就绪
    impl->is_ready = 1;

    return op;
}

/**
 * @brief 初始化 GenerateOp 算子
 */
int rag_generate_op_init(rag_operator_t *op, rag_context_t *ctx) {
    (void)ctx;

    if (!op || op->type != RAG_OP_GENERATE) {
        return -1;
    }

    generate_op_impl_t *impl = (generate_op_impl_t *)op->state;
    if (!impl) return -1;

    // Simple 模式不需要额外初始化
    if (impl->config.strategy == RAG_LLM_SIMPLE) {
        impl->is_ready = 1;
        return 0;
    }

    // Ollama 模式需要检查服务可用性
    if (impl->config.strategy == RAG_LLM_OLLAMA) {
        // TODO: 检查 Ollama 服务
        impl->is_ready = 0;
    }

    return 0;
}

/**
 * @brief 执行 LLM 生成
 */
rag_generate_output_t *rag_generate_execute(rag_operator_t *op,
                                             const char *prompt) {
    if (!op || op->type != RAG_OP_GENERATE || !prompt) {
        return NULL;
    }

    generate_op_impl_t *impl = (generate_op_impl_t *)op->state;
    if (!impl) return NULL;

    char *text = NULL;
    int64_t start_time = 0;  // TODO: 获取当前时间

    // 执行生成
    if (impl->config.strategy == RAG_LLM_SIMPLE) {
        text = simple_generate(prompt, &impl->config);
    } else if (impl->config.strategy == RAG_LLM_OLLAMA) {
        text = ollama_generate(prompt, &impl->config);
    }

    // 降级到 Simple
    if (!text) {
        text = simple_generate(prompt, &impl->config);
    }

    if (!text) return NULL;

    // 记录上次查询
    strncpy(impl->last_query, prompt, sizeof(impl->last_query) - 1);
    impl->last_query[sizeof(impl->last_query) - 1] = '\0';

    // 构建输出
    rag_generate_output_t *output = (rag_generate_output_t *)malloc(sizeof(rag_generate_output_t));
    if (!output) {
        free(text);
        return NULL;
    }

    output->text = text;
    output->text_len = (int)strlen(text);
    output->tokens_generated = output->text_len / 4;  // 粗略估计
    output->processing_time_ms = 0;  // TODO: 计算实际时间
    output->finished = 1;
    strncpy(output->finish_reason, "stop", sizeof(output->finish_reason) - 1);

    return output;
}

/**
 * @brief 执行 LLM 流式生成
 */
int rag_generate_execute_stream(rag_operator_t *op,
                                 const char *prompt,
                                 rag_stream_callback_t callback,
                                 void *user_data) {
    if (!op || op->type != RAG_OP_GENERATE || !prompt || !callback) {
        return -1;
    }

    generate_op_impl_t *impl = (generate_op_impl_t *)op->state;
    if (!impl) return -1;

    // Simple 模式：生成完整文本后逐词回调
    if (impl->config.strategy == RAG_LLM_SIMPLE) {
        char *text = simple_generate(prompt, &impl->config);
        if (!text) {
            text = simple_generate(prompt, &impl->config);
        }
        if (!text) return -1;

        // 模拟流式输出：按单词回调
        char *token = strtok(text, " ");
        while (token) {
            callback(token, 0, user_data);
            token = strtok(NULL, " ");
        }
        callback("", 1, user_data);  // 完成

        free(text);
        return 0;
    }

    // Ollama 模式
    if (impl->config.strategy == RAG_LLM_OLLAMA) {
        return ollama_generate_stream(prompt, &impl->config, callback, user_data);
    }

    return -1;
}

/**
 * @brief 检查 LLM 服务是否就绪
 */
int rag_generate_is_ready(const rag_operator_t *op) {
    if (!op || op->type != RAG_OP_GENERATE) return 0;

    generate_op_impl_t *impl = (generate_op_impl_t *)op->state;
    if (!impl) return 0;

    return impl->is_ready;
}

/**
 * @brief 释放 LLM 生成输出
 */
void rag_generate_output_free(rag_generate_output_t *output) {
    if (!output) return;

    free(output->text);
    free(output);
}

/**
 * @brief 销毁 GenerateOp 算子
 */
static int generate_op_cleanup(rag_operator_t *op) {
    if (!op) return -1;

    generate_op_impl_t *impl = (generate_op_impl_t *)op->state;
    if (impl) {
        free(impl);
        op->state = NULL;
    }

    return 0;
}

/**
 * @brief 兼容旧接口：创建并初始化 GenerateOp
 */
rag_operator_t *rag_generate_op_create_full(rag_llm_strategy_t strategy,
                                             const rag_llm_config_t *config,
                                             rag_context_t *ctx) {
    rag_operator_t *op = rag_generate_op_create(strategy, config);
    if (!op) return NULL;

    if (rag_generate_op_init(op, ctx) != 0) {
        generate_op_cleanup(op);
        free(op);
        return NULL;
    }

    return op;
}
