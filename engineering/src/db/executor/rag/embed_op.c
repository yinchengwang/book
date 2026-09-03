/**
 * @file embed_op.c
 * @brief Embedding 算子实现 - Wave 3-⑨
 *
 * 支持：
 * - Simple: 确定性种子向量（用于测试/降级）
 * - Ollama: Ollama API 调用
 *
 * 特性：
 * - LRU 缓存减少 API 调用
 * - 批量编码优化
 */
#include "db/rag_executor.h"
#include "db/storage/vector/vector_engine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>

// =============================================================================
// 内部类型定义
// =============================================================================

/** EmbedOp 实现 */
typedef struct {
    rag_embed_config_t config;          /**< 配置 */
    void              *cache;           /**< LRU 缓存 */
    int                cache_hits;      /**< 缓存命中数 */
    int                cache_misses;    /**< 缓存未命中数 */
    int                is_ready;        /**< 服务是否就绪 */

    // Simple 模式专用
    int                simple_dimension;

    // Ollama 模式专用
    void              *curl_handle;     /**< CURL 句柄（预留） */
} embed_op_impl_t;

/** LRU 缓存节点 */
typedef struct embed_cache_node {
    char              text_hash[64];    /**< 文本哈希 */
    float            *vector;           /**< 向量数据 */
    int               dimension;        /**< 向量维度 */
    struct embed_cache_node *prev;
    struct embed_cache_node *next;
} embed_cache_node_t;

/** LRU 缓存 */
typedef struct {
    embed_cache_node_t **buckets;       /**< 哈希桶 */
    int                 num_buckets;    /**< 桶数量 */
    int                 size;           /**< 当前条目数 */
    int                 max_size;       /**< 最大条目数 */
    embed_cache_node_t *head;           /**< LRU head (most recent) */
    embed_cache_node_t *tail;           /**< LRU tail (least recent) */
} embed_lru_cache_t;

// =============================================================================
// 工具函数
// =============================================================================

/**
 * @brief 简单的哈希函数
 */
static unsigned int simple_hash(const char *str) {
    unsigned int hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

/**
 * @brief 生成确定性种子向量
 *
 * 使用文本内容生成确定性随机向量，确保相同文本产生相同向量
 */
static void generate_deterministic_vector(const char *text, int text_len,
                                          float *vector, int dimension) {
    // 使用文本内容初始化伪随机数生成器
    unsigned int seed = 5381;
    for (int i = 0; i < text_len; i++) {
        seed = ((seed << 5) + seed) + (unsigned char)text[i];
    }

    // 生成向量
    for (int i = 0; i < dimension; i++) {
        // 简单的确定性伪随机数生成
        seed = seed * 1103515245 + 12345;
        float rand_val = (float)((seed >> 16) & 0x7FFF) / 32768.0f;
        vector[i] = rand_val;
    }

    // L2 归一化
    float norm = 0.0f;
    for (int i = 0; i < dimension; i++) {
        norm += vector[i] * vector[i];
    }
    norm = sqrtf(norm);
    if (norm > 0.0f) {
        for (int i = 0; i < dimension; i++) {
            vector[i] /= norm;
        }
    }
}

/**
 * @brief 计算文本哈希
 */
static void text_hash(const char *text, int text_len, char *hash_out) {
    unsigned int hash = simple_hash(text);
    snprintf(hash_out, 64, "%08x", hash);
}

// =============================================================================
// LRU 缓存实现
// =============================================================================

/**
 * @brief 创建 LRU 缓存
 */
static embed_lru_cache_t *lru_cache_create(int max_size) {
    embed_lru_cache_t *cache = (embed_lru_cache_t *)calloc(1, sizeof(embed_lru_cache_t));
    if (!cache) return NULL;

    cache->num_buckets = max_size > 0 ? max_size * 2 : 256;
    cache->buckets = (embed_cache_node_t **)calloc(cache->num_buckets, sizeof(embed_cache_node_t *));
    if (!cache->buckets) {
        free(cache);
        return NULL;
    }

    cache->max_size = max_size > 0 ? max_size : 1000;
    cache->head = NULL;
    cache->tail = NULL;

    return cache;
}

/**
 * @brief 销毁 LRU 缓存
 */
static void lru_cache_destroy(embed_lru_cache_t *cache) {
    if (!cache) return;

    // 释放所有节点
    for (int i = 0; i < cache->num_buckets; i++) {
        embed_cache_node_t *node = cache->buckets[i];
        while (node) {
            embed_cache_node_t *next = node->next;
            free(node->vector);
            free(node);
            node = next;
        }
    }

    free(cache->buckets);
    free(cache);
}

/**
 * @brief 将节点移到链表头部（最近使用）
 */
static void lru_move_to_head(embed_lru_cache_t *cache, embed_cache_node_t *node) {
    if (!node || node == cache->head) return;

    // 从原位置移除
    if (node->prev) node->prev->next = node->next;
    if (node->next) node->next->prev = node->prev;
    if (node == cache->tail) cache->tail = node->prev;

    // 插入到头部
    node->prev = NULL;
    node->next = cache->head;
    if (cache->head) cache->head->prev = node;
    cache->head = node;
    if (!cache->tail) cache->tail = node;
}

/**
 * @brief 从缓存获取
 */
static float *lru_cache_get(embed_lru_cache_t *cache, const char *text,
                            int text_len, int dimension) {
    if (!cache || !text) return NULL;

    char hash_str[64];
    text_hash(text, text_len, hash_str);
    unsigned int bucket = simple_hash(hash_str) % cache->num_buckets;

    embed_cache_node_t *node = cache->buckets[bucket];
    while (node) {
        if (strcmp(node->text_hash, hash_str) == 0) {
            // 命中
            lru_move_to_head(cache, node);
            return node->vector;
        }
        node = node->next;
    }

    return NULL;
}

/**
 * @brief 设置缓存
 */
static bool lru_cache_set(embed_lru_cache_t *cache, const char *text,
                          int text_len, const float *vector, int dimension) {
    if (!cache || !text || !vector) return false;

    char hash_str[64];
    text_hash(text, text_len, hash_str);
    unsigned int bucket = simple_hash(hash_str) % cache->num_buckets;

    // 检查是否已存在
    embed_cache_node_t *node = cache->buckets[bucket];
    while (node) {
        if (strcmp(node->text_hash, hash_str) == 0) {
            // 更新现有节点
            if (node->dimension == dimension) {
                memcpy(node->vector, vector, dimension * sizeof(float));
                lru_move_to_head(cache, node);
            }
            return true;
        }
        node = node->next;
    }

    // 如果缓存已满，删除最久未使用的
    if (cache->size >= cache->max_size && cache->tail) {
        embed_cache_node_t *to_remove = cache->tail;
        lru_move_to_head(cache, to_remove);  // 先移动 tail

        // 从哈希表移除
        unsigned int rm_bucket = simple_hash(to_remove->text_hash) % cache->num_buckets;
        embed_cache_node_t **prev_ptr = &cache->buckets[rm_bucket];
        while (*prev_ptr && *prev_ptr != to_remove) {
            prev_ptr = &(*prev_ptr)->next;
        }
        if (*prev_ptr) *prev_ptr = to_remove->next;

        // 从链表移除
        if (to_remove->prev) to_remove->prev->next = to_remove->next;
        if (to_remove->next) to_remove->next->prev = to_remove->prev;
        if (to_remove == cache->head) cache->head = to_remove->next;
        if (to_remove == cache->tail) cache->tail = to_remove->prev;

        free(to_remove->vector);
        free(to_remove);
        cache->size--;
    }

    // 创建新节点
    node = (embed_cache_node_t *)malloc(sizeof(embed_cache_node_t));
    if (!node) return false;

    node->vector = (float *)malloc(dimension * sizeof(float));
    if (!node->vector) {
        free(node);
        return false;
    }

    strncpy(node->text_hash, hash_str, sizeof(node->text_hash) - 1);
    node->text_hash[sizeof(node->text_hash) - 1] = '\0';
    memcpy(node->vector, vector, dimension * sizeof(float));
    node->dimension = dimension;
    node->prev = NULL;
    node->next = cache->head;
    if (cache->head) cache->head->prev = node;
    cache->head = node;
    if (!cache->tail) cache->tail = node;

    // 添加到哈希桶
    node->next = cache->buckets[bucket];
    cache->buckets[bucket] = node;

    cache->size++;

    return true;
}

/**
 * @brief 清除缓存
 */
static void lru_cache_clear(embed_lru_cache_t *cache) {
    if (!cache) return;

    for (int i = 0; i < cache->num_buckets; i++) {
        embed_cache_node_t *node = cache->buckets[i];
        while (node) {
            embed_cache_node_t *next = node->next;
            free(node->vector);
            free(node);
            node = next;
        }
        cache->buckets[i] = NULL;
    }

    cache->head = NULL;
    cache->tail = NULL;
    cache->size = 0;
}

/**
 * @brief 获取缓存统计
 */
static void lru_cache_stats(embed_lru_cache_t *cache, int *size, int *hits, int *misses) {
    if (cache) {
        if (size) *size = cache->size;
    }
    (void)hits;
    (void)misses;
}

// =============================================================================
// Simple Embedding 实现
// =============================================================================

/**
 * @brief Simple 模式编码
 */
static float *simple_encode(const char *text, int text_len, int dimension) {
    if (!text) return NULL;

    float *vector = (float *)malloc(dimension * sizeof(float));
    if (!vector) return NULL;

    if (text_len == 0) {
        text_len = (int)strlen(text);
    }

    generate_deterministic_vector(text, text_len, vector, dimension);

    return vector;
}

// =============================================================================
// Ollama Embedding 实现（预留）
// =============================================================================

/**
 * @brief Ollama 模式编码（预留，需要 libcurl）
 *
 * 实际实现需要 libcurl 依赖，暂时返回 NULL 表示不可用
 */
static float *ollama_encode(const char *text, int text_len,
                            const rag_embed_config_t *config) {
    (void)text;
    (void)text_len;
    (void)config;

    // TODO: 使用 libcurl 调用 Ollama API
    // POST http://localhost:11434/api/embeddings
    // Body: {"model": "nomic-embed-text", "prompt": "..."}
    // Response: {"embedding": [...]}

    return NULL;
}

// =============================================================================
// EmbedOp 算子实现
// =============================================================================

/**
 * @brief 创建 EmbedOp 算子
 */
rag_operator_t *rag_embed_op_create(rag_embed_strategy_t strategy,
                                     const rag_embed_config_t *config) {
    rag_operator_t *op = (rag_operator_t *)malloc(sizeof(rag_operator_t));
    if (!op) return NULL;

    memset(op, 0, sizeof(rag_operator_t));

    embed_op_impl_t *impl = (embed_op_impl_t *)calloc(1, sizeof(embed_op_impl_t));
    if (!impl) {
        free(op);
        return NULL;
    }

    op->type = RAG_OP_EMBED;
    snprintf(op->name, sizeof(op->name) - 1, "embed_op_%d", strategy);
    op->state = impl;

    // 设置配置
    if (config) {
        impl->config = *config;
    } else {
        // 默认配置
        impl->config.strategy = RAG_EMBED_SIMPLE;
        strncpy(impl->config.model, "simple", sizeof(impl->config.model) - 1);
        impl->config.dimension = 768;
        impl->config.cache_size = 1000;
    }

    // 设置维度
    impl->simple_dimension = impl->config.dimension > 0 ? impl->config.dimension : 768;

    // 创建缓存
    impl->cache = lru_cache_create(impl->config.cache_size);
    impl->is_ready = 1;  // Simple 模式始终就绪

    return op;
}

/**
 * @brief 初始化 EmbedOp 算子
 */
int rag_embed_op_init(rag_operator_t *op, rag_context_t *ctx) {
    (void)ctx;

    if (!op || op->type != RAG_OP_EMBED) {
        return -1;
    }

    embed_op_impl_t *impl = (embed_op_impl_t *)op->state;
    if (!impl) return -1;

    // Simple 模式不需要额外初始化
    if (impl->config.strategy == RAG_EMBED_SIMPLE) {
        impl->is_ready = 1;
        return 0;
    }

    // Ollama 模式需要检查服务可用性
    if (impl->config.strategy == RAG_EMBED_OLLAMA) {
        // TODO: 检查 Ollama 服务
        impl->is_ready = 0;
    }

    return 0;
}

/**
 * @brief 执行 Embedding
 */
rag_embed_output_t *rag_embed_execute(rag_operator_t *op,
                                       const char *text,
                                       int use_cache) {
    if (!op || op->type != RAG_OP_EMBED || !text) {
        return NULL;
    }

    embed_op_impl_t *impl = (embed_op_impl_t *)op->state;
    if (!impl) return NULL;

    int text_len = (int)strlen(text);
    int dimension = impl->config.dimension > 0 ? impl->config.dimension : impl->simple_dimension;

    // 检查缓存
    if (use_cache && impl->cache) {
        float *cached = lru_cache_get((embed_lru_cache_t *)impl->cache, text, text_len, dimension);
        if (cached) {
            impl->cache_hits++;

            rag_embed_output_t *output = (rag_embed_output_t *)malloc(sizeof(rag_embed_output_t));
            if (!output) return NULL;

            output->vector = (float *)malloc(dimension * sizeof(float));
            if (!output->vector) {
                free(output);
                return NULL;
            }

            memcpy(output->vector, cached, dimension * sizeof(float));
            output->dimension = dimension;
            output->processing_time_ms = 0;
            output->cache_hit = 1;

            return output;
        }
    }

    impl->cache_misses++;

    // 执行编码
    float *vector = NULL;
    int64_t start_time = 0;  // TODO: 获取当前时间

    if (impl->config.strategy == RAG_EMBED_SIMPLE) {
        vector = simple_encode(text, text_len, dimension);
    } else if (impl->config.strategy == RAG_EMBED_OLLAMA) {
        vector = ollama_encode(text, text_len, &impl->config);
    }

    if (!vector) {
        // 降级到 Simple
        vector = simple_encode(text, text_len, dimension);
    }

    if (!vector) return NULL;

    // 设置缓存
    if (use_cache && impl->cache) {
        lru_cache_set((embed_lru_cache_t *)impl->cache, text, text_len, vector, dimension);
    }

    // 构建输出
    rag_embed_output_t *output = (rag_embed_output_t *)malloc(sizeof(rag_embed_output_t));
    if (!output) {
        free(vector);
        return NULL;
    }

    output->vector = vector;
    output->dimension = dimension;
    output->processing_time_ms = 0;  // TODO: 计算实际时间
    output->cache_hit = 0;

    return output;
}

/**
 * @brief 批量执行 Embedding
 */
int rag_embed_execute_batch(rag_operator_t *op,
                             const char **texts,
                             int ntexts,
                             float *vectors) {
    if (!op || !texts || !vectors) return -1;

    embed_op_impl_t *impl = (embed_op_impl_t *)op->state;
    if (!impl) return -1;

    int dimension = impl->config.dimension > 0 ? impl->config.dimension : impl->simple_dimension;

    for (int i = 0; i < ntexts; i++) {
        float *vector = simple_encode(texts[i], 0, dimension);
        if (!vector) return -1;

        memcpy(vectors + (size_t)i * dimension, vector, dimension * sizeof(float));
        free(vector);
    }

    return 0;
}

/**
 * @brief 获取 Embedding 向量维度
 */
int rag_embed_dimension(const rag_operator_t *op) {
    if (!op || op->type != RAG_OP_EMBED) return 0;

    embed_op_impl_t *impl = (embed_op_impl_t *)op->state;
    if (!impl) return 0;

    return impl->config.dimension > 0 ? impl->config.dimension : impl->simple_dimension;
}

/**
 * @brief 检查 Embedding 服务是否就绪
 */
int rag_embed_is_ready(const rag_operator_t *op) {
    if (!op || op->type != RAG_OP_EMBED) return 0;

    embed_op_impl_t *impl = (embed_op_impl_t *)op->state;
    if (!impl) return 0;

    return impl->is_ready;
}

/**
 * @brief 获取 Embedding 缓存统计
 */
void rag_embed_cache_stats(const rag_operator_t *op,
                           rag_embed_cache_stats_t *stats) {
    if (!op || !stats) return;

    embed_op_impl_t *impl = (embed_op_impl_t *)op->state;
    if (!impl) return;

    memset(stats, 0, sizeof(rag_embed_cache_stats_t));

    if (impl->cache) {
        lru_cache_stats((embed_lru_cache_t *)impl->cache, &stats->size, NULL, NULL);
    }

    stats->hits = impl->cache_hits;
    stats->misses = impl->cache_misses;

    int total = stats->hits + stats->misses;
    if (total > 0) {
        stats->hit_rate = (double)stats->hits / total;
    }
}

/**
 * @brief 清除 Embedding 缓存
 */
void rag_embed_cache_clear(rag_operator_t *op) {
    if (!op || op->type != RAG_OP_EMBED) return;

    embed_op_impl_t *impl = (embed_op_impl_t *)op->state;
    if (!impl || !impl->cache) return;

    lru_cache_clear((embed_lru_cache_t *)impl->cache);
}

/**
 * @brief 释放 Embedding 输出
 */
void rag_embed_output_free(rag_embed_output_t *output) {
    if (!output) return;

    free(output->vector);
    free(output);
}

/**
 * @brief 销毁 EmbedOp 算子
 */
static int embed_op_cleanup(rag_operator_t *op) {
    if (!op) return -1;

    embed_op_impl_t *impl = (embed_op_impl_t *)op->state;
    if (impl) {
        if (impl->cache) {
            lru_cache_destroy((embed_lru_cache_t *)impl->cache);
        }
        free(impl);
        op->state = NULL;
    }

    return 0;
}

/**
 * @brief 兼容旧接口：创建并初始化 EmbedOp
 */
rag_operator_t *rag_embed_op_create_full(rag_embed_strategy_t strategy,
                                          const rag_embed_config_t *config,
                                          rag_context_t *ctx) {
    rag_operator_t *op = rag_embed_op_create(strategy, config);
    if (!op) return NULL;

    if (rag_embed_op_init(op, ctx) != 0) {
        // 清理
        embed_op_cleanup(op);
        free(op);
        return NULL;
    }

    return op;
}
