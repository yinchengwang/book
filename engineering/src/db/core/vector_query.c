/**
 * @file vector_query.c
 * @brief 向量查询执行器实现
 *
 * 实现两阶段检索管道：粗排 → 精排 → 混合融合
 */
#include <db/core/vector_query.h>
#include <db/index/vector_index/hnsw/faiss_hnsw.h>
#include <db/index/vector_index/ivf/IndexIVF.h>
#include <db/index/vector_index/reranker/reranker.h>
#include <db/index/vector_index/BM25/bm25.h>
#include <db/log.h>
#include <db/storage/vector/vector_persist.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>

/* ========================================================================
 * Reranker Stub 实现 (当 reranker 模块未实现时使用)
 * ======================================================================== */

static RerankerConfig g_default_reranker_config = {
    .type = RERANKER_PRECISE,
    .dims = 128,
    .metric = DISTANCE_METRIC_L2_SQUARED,
    .precise = { .use_simd = true },
    .multi_metric = { .num_metrics = 0 },
    .mmr = { .lambda = 0.5f, .diversity_dim = 0 }
};

RerankerConfig reranker_config_default(RerankerType type, int32_t dims) {
    (void)type;
    g_default_reranker_config.dims = dims;
    return g_default_reranker_config;
}

typedef struct RerankerStub_s {
    RerankerType type;
    RerankerConfig config;
} RerankerStub;

Reranker *reranker_create(RerankerType type, const RerankerConfig *config) {
    RerankerStub *stub = (RerankerStub *)calloc(1, sizeof(RerankerStub));
    if (stub) {
        stub->type = type;
        if (config) {
            stub->config = *config;
        }
    }
    return (Reranker *)stub;
}

void reranker_destroy(Reranker *reranker) {
    free(reranker);
}

int32_t reranker_rerank(Reranker *reranker,
                        const float *query,
                        const float *candidates,
                        int32_t n_candidates,
                        int32_t k,
                        float *scores,
                        int32_t *reranked_ids) {
    (void)reranker;
    (void)query;
    (void)candidates;

    if (!scores || !reranked_ids || n_candidates <= 0 || k <= 0) {
        return -1;
    }

    int32_t count = n_candidates < k ? n_candidates : k;
    for (int32_t i = 0; i < count; i++) {
        reranked_ids[i] = i;
        scores[i] = (float)i / count;
    }
    return count;
}

RerankerType reranker_get_type(const Reranker *reranker) {
    if (!reranker) return RERANKER_PRECISE;
    return ((RerankerStub *)reranker)->type;
}

const char *reranker_get_name(const Reranker *reranker) {
    static const char *names[] = {"precise", "multi_metric", "mmr", "custom"};
    if (!reranker) return "unknown";
    RerankerType type = ((RerankerStub *)reranker)->type;
    if (type >= 0 && type <= RERANKER_CUSTOM) {
        return names[type];
    }
    return "unknown";
}

int32_t reranker_register(const char *name, rerank_func_t func, void *user_data) {
    (void)name; (void)func; (void)user_data;
    return 0;
}

int32_t reranker_apply(const char *name, const float *query, const float *candidates,
                       int32_t n_candidates, int32_t k, float *scores, int32_t *reranked_ids) {
    (void)name;
    return reranker_rerank(NULL, query, candidates, n_candidates, k, scores, reranked_ids);
}

int32_t reranker_list_registered(char names[][64], int32_t max_names) {
    (void)names; (void)max_names;
    return 0;
}

/* ========================================================================
 * 内部数据结构
 * ======================================================================== */

/** 索引注册项 */
typedef struct IndexRegistryEntry_s {
    char name[64];
    VectorIndexType index_type;
    void *index_impl;
    vector_index_search_fn search_fn;
    vector_index_get_vector_fn get_vector_fn;
    struct IndexRegistryEntry_s *next;
} IndexRegistryEntry;

/** 执行器全局状态 */
typedef struct VectorQueryEngine_s {
    VectorQueryState state;
    IndexRegistryEntry *index_registry;
    int registry_count;

    /* 计划缓存 */
    VectorQueryPlan *plan_cache[VECTOR_QUERY_PLAN_CACHE_SIZE];
    int cache_count;
    uint64_t cache_hits;
    uint64_t cache_misses;

    /* 统计信息 */
    uint64_t total_queries;
    uint64_t successful_queries;
    uint64_t failed_queries;
    uint64_t cached_queries;
    uint64_t total_latency_us;
    uint64_t min_latency_us;
    uint64_t max_latency_us;

    /* 剖析 */
    bool profiling_enabled;
    VectorQueryProfileEntry profiles[16];
    int profile_count;

    /* 取消标志 */
    volatile bool cancelled;
} VectorQueryEngine;

static VectorQueryEngine g_engine = {
    .state = VECTOR_QUERY_UNINIT,
    .index_registry = NULL,
    .registry_count = 0,
    .cache_count = 0,
    .cache_hits = 0,
    .cache_misses = 0,
    .total_queries = 0,
    .successful_queries = 0,
    .failed_queries = 0,
    .cached_queries = 0,
    .total_latency_us = 0,
    .min_latency_us = UINT64_MAX,
    .max_latency_us = 0,
    .profiling_enabled = false,
    .profile_count = 0,
    .cancelled = false
};

/* ========================================================================
 * 工具函数
 * ======================================================================== */

/**
 * @brief 获取当前时间 (微秒)
 */
static int64_t get_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

/**
 * @brief 简单哈希函数
 */
static uint64_t simple_hash(const char *str) {
    uint64_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

/* ========================================================================
 * 查询计划 API 实现
 * ======================================================================== */

VectorQueryPlan *vector_query_plan_create(void) {
    VectorQueryPlan *plan = (VectorQueryPlan *)calloc(1, sizeof(VectorQueryPlan));
    if (!plan) {
        LOG_ERROR("分配查询计划失败");
        return NULL;
    }

    /* 设置默认值 */
    plan->coarse_index_type = VECTOR_INDEX_HNSW;
    plan->coarse_ef_search = 100;
    plan->coarse_max_candidates = VECTOR_QUERY_DEFAULT_CANDIDATES;
    plan->reranker_type = RERANKER_PRECISE;
    plan->reranker_config = reranker_config_default(RERANKER_PRECISE, 128);
    plan->top_k = VECTOR_QUERY_DEFAULT_TOPK;
    plan->with_scores = true;
    plan->hybrid_enabled = false;
    plan->vector_weight = 1.0f;
    plan->text_weight = 0.0f;
    plan->fusion_type = FUSION_RRF;
    plan->plan_id = 0;
    plan->created_at = get_time_us();
    plan->hit_count = 0;

    return plan;
}

void vector_query_plan_destroy(VectorQueryPlan *plan) {
    if (plan) {
        free(plan);
    }
}

VectorQueryPlan *vector_query_plan_copy(const VectorQueryPlan *plan) {
    if (!plan) return NULL;
    VectorQueryPlan *copy = (VectorQueryPlan *)malloc(sizeof(VectorQueryPlan));
    if (copy) {
        memcpy(copy, plan, sizeof(VectorQueryPlan));
    }
    return copy;
}

void vector_query_plan_set_coarse(VectorQueryPlan *plan,
                                  VectorIndexType index_type,
                                  int32_t ef_search,
                                  int32_t max_candidates) {
    if (!plan) return;
    plan->coarse_index_type = index_type;
    plan->coarse_ef_search = ef_search > 0 ? ef_search : 100;
    plan->coarse_max_candidates = max_candidates > 0 ?
        (max_candidates > VECTOR_QUERY_MAX_CANDIDATES ? VECTOR_QUERY_MAX_CANDIDATES : max_candidates) :
        VECTOR_QUERY_DEFAULT_CANDIDATES;
}

void vector_query_plan_set_rerank(VectorQueryPlan *plan,
                                  RerankerType reranker_type,
                                  int32_t k) {
    if (!plan) return;
    plan->reranker_type = reranker_type;
    plan->reranker_config = reranker_config_default(reranker_type, 128);
}

void vector_query_plan_set_hybrid(VectorQueryPlan *plan,
                                  bool enabled,
                                  float vector_weight,
                                  float text_weight,
                                  VectorFusionType fusion_type) {
    if (!plan) return;
    plan->hybrid_enabled = enabled;
    plan->vector_weight = vector_weight;
    plan->text_weight = text_weight;
    plan->fusion_type = fusion_type;
}

void vector_query_plan_set_topk(VectorQueryPlan *plan, int32_t top_k) {
    if (!plan) return;
    plan->top_k = top_k > 0 ? top_k : VECTOR_QUERY_DEFAULT_TOPK;
}

void vector_query_plan_set_offset(VectorQueryPlan *plan, int32_t offset) {
    if (!plan) return;
    plan->offset = offset >= 0 ? offset : 0;
}

/* ========================================================================
 * 算子节点构建实现 (新增)
 * ======================================================================== */

/** VectorScan 算子私有数据 */
typedef struct VectorScanPrivate_s {
    VectorIndexType index_type;
    int32_t ef_search;
    int32_t max_candidates;
    char index_name[64];
    DistanceMetric metric;
} VectorScanPrivate;

/** VectorFilter 算子私有数据 */
typedef struct VectorFilterPrivate_s {
    int32_t column_idx;
    CompareOp op;
    void *value;
} VectorFilterPrivate;

/** VectorLimit 算子私有数据 */
typedef struct VectorLimitPrivate_s {
    int32_t limit;
    int32_t offset;
} VectorLimitPrivate;

/** VectorJoin 算子私有数据 */
typedef struct VectorJoinPrivate_s {
    VectorFusionType fusion_type;
    float vector_weight;
    float text_weight;
    int32_t rrf_k;
} VectorJoinPrivate;

PlanNode *create_vector_scan_node(VectorIndexType index_type,
                                   int32_t ef_search,
                                   int32_t max_candidates) {
    PlanNode *node = (PlanNode *)calloc(1, sizeof(PlanNode));
    if (!node) return NULL;

    VectorScanPrivate *priv = (VectorScanPrivate *)calloc(1, sizeof(VectorScanPrivate));
    if (!priv) {
        free(node);
        return NULL;
    }

    node->type = PLAN_NODE_VECTOR_SCAN;
    node->left = NULL;
    node->right = NULL;
    node->cost_estimate = max_candidates * 10;  /* 估算代价 */
    node->rows_estimate = max_candidates;

    priv->index_type = index_type;
    priv->ef_search = ef_search;
    priv->max_candidates = max_candidates;
    priv->metric = DISTANCE_METRIC_L2_SQUARED;

    node->private_data = priv;

    return node;
}

PlanNode *create_vector_filter_node(int32_t column_idx,
                                     CompareOp op,
                                     void *value) {
    PlanNode *node = (PlanNode *)calloc(1, sizeof(PlanNode));
    if (!node) return NULL;

    VectorFilterPrivate *priv = (VectorFilterPrivate *)calloc(1, sizeof(VectorFilterPrivate));
    if (!priv) {
        free(node);
        return NULL;
    }

    node->type = PLAN_NODE_VECTOR_FILTER;
    node->left = NULL;
    node->right = NULL;
    node->cost_estimate = 1;
    node->rows_estimate = 100;  /* 估算过滤后行数 */

    priv->column_idx = column_idx;
    priv->op = op;
    priv->value = value;

    node->private_data = priv;

    return node;
}

PlanNode *create_vector_limit_node(int32_t limit, int32_t offset) {
    PlanNode *node = (PlanNode *)calloc(1, sizeof(PlanNode));
    if (!node) return NULL;

    VectorLimitPrivate *priv = (VectorLimitPrivate *)calloc(1, sizeof(VectorLimitPrivate));
    if (!priv) {
        free(node);
        return NULL;
    }

    node->type = PLAN_NODE_VECTOR_LIMIT;
    node->left = NULL;
    node->right = NULL;
    node->cost_estimate = 1;
    node->rows_estimate = limit;

    priv->limit = limit;
    priv->offset = offset;

    node->private_data = priv;

    return node;
}

PlanNode *create_vector_join_node(VectorFusionType fusion_type,
                                   float vector_weight,
                                   float text_weight) {
    PlanNode *node = (PlanNode *)calloc(1, sizeof(PlanNode));
    if (!node) return NULL;

    VectorJoinPrivate *priv = (VectorJoinPrivate *)calloc(1, sizeof(VectorJoinPrivate));
    if (!priv) {
        free(node);
        return NULL;
    }

    node->type = PLAN_NODE_VECTOR_JOIN;
    node->left = NULL;
    node->right = NULL;
    node->cost_estimate = 5;
    node->rows_estimate = 100;

    priv->fusion_type = fusion_type;
    priv->vector_weight = vector_weight;
    priv->text_weight = text_weight;
    priv->rrf_k = 60;  /* RRF 默认参数 */

    node->private_data = priv;

    return node;
}

/* 释放算子节点 */
void destroy_plan_node(PlanNode *node) {
    if (!node) return;

    /* 先释放子节点 */
    if (node->left) destroy_plan_node(node->left);
    if (node->right) destroy_plan_node(node->right);

    /* 释放私有数据 */
    if (node->private_data) {
        free(node->private_data);
    }

    free(node);
}

/* ========================================================================
 * 查询计划构建实现 (新增)
 * ======================================================================== */

VectorQueryPlan *build_simple_query_plan(VectorIndexType index_type, int32_t k) {
    VectorQueryPlan *plan = vector_query_plan_create();
    if (!plan) return NULL;

    /* 构建 Scan → Limit 算子链 */
    PlanNode *scan = create_vector_scan_node(index_type, 100, k * 10);
    PlanNode *limit = create_vector_limit_node(k, 0);

    if (!scan || !limit) {
        if (scan) destroy_plan_node(scan);
        if (limit) destroy_plan_node(limit);
        vector_query_plan_destroy(plan);
        return NULL;
    }

    limit->left = scan;
    plan->root_node = limit;
    plan->num_nodes = 2;

    plan->coarse_index_type = index_type;
    plan->coarse_max_candidates = k * 10;
    plan->top_k = k;

    return plan;
}

VectorQueryPlan *build_hybrid_query_plan(int32_t k,
                                          float vector_weight,
                                          float text_weight) {
    VectorQueryPlan *plan = vector_query_plan_create();
    if (!plan) return NULL;

    /* 构建 VectorScan + BM25 → Join → Limit 算子链 */
    PlanNode *vec_scan = create_vector_scan_node(VECTOR_INDEX_HNSW, 100, k * 10);
    PlanNode *bm25_scan = create_vector_scan_node(VECTOR_INDEX_HNSW, 100, k * 10);  /* BM25 占位 */
    PlanNode *join = create_vector_join_node(FUSION_RRF, vector_weight, text_weight);
    PlanNode *limit = create_vector_limit_node(k, 0);

    if (!vec_scan || !bm25_scan || !join || !limit) {
        if (vec_scan) destroy_plan_node(vec_scan);
        if (bm25_scan) destroy_plan_node(bm25_scan);
        if (join) destroy_plan_node(join);
        if (limit) destroy_plan_node(limit);
        vector_query_plan_destroy(plan);
        return NULL;
    }

    join->left = vec_scan;
    join->right = bm25_scan;
    limit->left = join;
    plan->root_node = limit;
    plan->num_nodes = 4;

    plan->hybrid_enabled = true;
    plan->vector_weight = vector_weight;
    plan->text_weight = text_weight;
    plan->fusion_type = FUSION_RRF;
    plan->top_k = k;

    return plan;
}

/* ========================================================================
 * 执行上下文实现 (新增)
 * ======================================================================== */

ExecContext *exec_context_create(const float *query, int32_t dims, DistanceMetric metric) {
    ExecContext *ctx = (ExecContext *)calloc(1, sizeof(ExecContext));
    if (!ctx) return NULL;

    ctx->query_vector = query;
    ctx->query_dims = dims;
    ctx->metric = metric;
    ctx->exec_mode = VECTOR_EXEC_BATCH;
    ctx->batch_size = 1024;
    ctx->timeout_ms = 30000;  /* 30 秒 */
    ctx->state = VECTOR_QUERY_READY;
    ctx->start_time_us = get_time_us();
    ctx->profiling_enabled = false;
    ctx->error_msg[0] = '\0';
    ctx->error_code = 0;

    return ctx;
}

void exec_context_destroy(ExecContext *ctx) {
    if (ctx) {
        free(ctx);
    }
}

/* ========================================================================
 * 查询结果 API 实现
 * ======================================================================== */

void vector_query_result_free(VectorQueryResult *result) {
    if (result) {
        free(result->ids);
        free(result->distances);
        free(result->scores);
        free(result->metadata);
        free(result);
    }
}

char *vector_query_result_to_json(const VectorQueryResult *result) {
    if (!result) return NULL;

    /* 分配足够大的缓冲区 */
    size_t buf_size = 2048 + result->count * 256;
    char *json = (char *)malloc(buf_size);
    if (!json) return NULL;

    char *ptr = json;
    size_t remaining = buf_size;
    int written;

    /* 头部信息 */
    written = snprintf(ptr, remaining, "{\"success\":true,");
    if (written < 0 || (size_t)written >= remaining) { free(json); return NULL; }
    ptr += written; remaining -= written;

    written = snprintf(ptr, remaining, "\"count\":%d,", result->count);
    if (written < 0 || (size_t)written >= remaining) { free(json); return NULL; }
    ptr += written; remaining -= written;

    written = snprintf(ptr, remaining, "\"offset\":%d,", result->offset);
    if (written < 0 || (size_t)written >= remaining) { free(json); return NULL; }
    ptr += written; remaining -= written;

    written = snprintf(ptr, remaining, "\"total_candidates\":%d,", result->total_candidates);
    if (written < 0 || (size_t)written >= remaining) { free(json); return NULL; }
    ptr += written; remaining -= written;

    /* 执行统计 */
    written = snprintf(ptr, remaining, "\"stats\":{");
    if (written < 0 || (size_t)written >= remaining) { free(json); return NULL; }
    ptr += written; remaining -= written;

    written = snprintf(ptr, remaining, "\"coarse_time_us\":%ld,", (long)result->coarse_time_us);
    if (written < 0 || (size_t)written >= remaining) { free(json); return NULL; }
    ptr += written; remaining -= written;

    written = snprintf(ptr, remaining, "\"rerank_time_us\":%ld,", (long)result->rerank_time_us);
    if (written < 0 || (size_t)written >= remaining) { free(json); return NULL; }
    ptr += written; remaining -= written;

    if (result->fusion_time_us > 0) {
        written = snprintf(ptr, remaining, "\"fusion_time_us\":%ld,", (long)result->fusion_time_us);
        if (written < 0 || (size_t)written >= remaining) { free(json); return NULL; }
        ptr += written; remaining -= written;
    }
    written = snprintf(ptr, remaining, "\"total_time_us\":%ld", (long)result->total_time_us);
    if (written < 0 || (size_t)written >= remaining) { free(json); return NULL; }
    ptr += written; remaining -= written;

    written = snprintf(ptr, remaining, "},");
    if (written < 0 || (size_t)written >= remaining) { free(json); return NULL; }
    ptr += written; remaining -= written;

    /* 结果列表 */
    written = snprintf(ptr, remaining, "\"results\":[");
    if (written < 0 || (size_t)written >= remaining) { free(json); return NULL; }
    ptr += written; remaining -= written;

    for (int i = 0; i < result->count; i++) {
        if (i > 0) {
            written = snprintf(ptr, remaining, ",");
            if (written < 0 || (size_t)written >= remaining) { free(json); return NULL; }
            ptr += written; remaining -= written;
        }
        written = snprintf(ptr, remaining, "{\"id\":%ld", (long)result->ids[i]);
        if (written < 0 || (size_t)written >= remaining) { free(json); return NULL; }
        ptr += written; remaining -= written;

        /* 距离 (支持不同度量) */
        written = snprintf(ptr, remaining, ",\"distance\":%.8f", result->distances[i]);
        if (written < 0 || (size_t)written >= remaining) { free(json); return NULL; }
        ptr += written; remaining -= written;

        /* 分数 (如果有) */
        if (result->scores) {
            written = snprintf(ptr, remaining, ",\"score\":%.8f", result->scores[i]);
            if (written < 0 || (size_t)written >= remaining) { free(json); return NULL; }
            ptr += written; remaining -= written;
        }

        /* 元数据 (如果有) */
        if (result->metadata && result->metadata_count > 0 && result->metadata[i]) {
            written = snprintf(ptr, remaining, ",\"metadata\":%s", (const char *)result->metadata[i]);
            if (written < 0 || (size_t)written >= remaining) { free(json); return NULL; }
            ptr += written; remaining -= written;
        }

        written = snprintf(ptr, remaining, "}");
        if (written < 0 || (size_t)written >= remaining) { free(json); return NULL; }
        ptr += written; remaining -= written;
    }
    written = snprintf(ptr, remaining, "]");
    if (written < 0 || (size_t)written >= remaining) { free(json); return NULL; }
    ptr += written; remaining -= written;

    /* 剖析信息 (如果有) */
    written = snprintf(ptr, remaining, ",\"profile\":{");
    if (written < 0 || (size_t)written >= remaining) { free(json); return NULL; }
    ptr += written; remaining -= written;

    written = snprintf(ptr, remaining, "\"candidates\":%d", result->candidates);
    if (written < 0 || (size_t)written >= remaining) { free(json); return NULL; }
    ptr += written; remaining -= written;

    written = snprintf(ptr, remaining, "}}");
    if (written < 0 || (size_t)written >= remaining) { free(json); return NULL; }

    return json;
}

/* ========================================================================
 * 执行器核心 API 实现
 * ======================================================================== */

int vector_query_init(void) {
    if (g_engine.state != VECTOR_QUERY_UNINIT) {
        LOG_WARN("向量查询执行器已初始化");
        return 0;
    }

    memset(&g_engine, 0, sizeof(g_engine));
    g_engine.state = VECTOR_QUERY_READY;
    g_engine.min_latency_us = UINT64_MAX;

    LOG_INFO("向量查询执行器初始化成功");
    return 0;
}

void vector_query_shutdown(void) {
    if (g_engine.state == VECTOR_QUERY_UNINIT) {
        return;
    }

    /* 清理索引注册表 */
    IndexRegistryEntry *entry = g_engine.index_registry;
    while (entry) {
        IndexRegistryEntry *next = entry->next;
        free(entry);
        entry = next;
    }

    /* 清理计划缓存 */
    for (int i = 0; i < g_engine.cache_count; i++) {
        if (g_engine.plan_cache[i]) {
            vector_query_plan_destroy(g_engine.plan_cache[i]);
        }
    }

    g_engine.state = VECTOR_QUERY_UNINIT;
    LOG_INFO("向量查询执行器已关闭");
}

bool vector_query_is_ready(void) {
    return g_engine.state == VECTOR_QUERY_READY;
}

/* ========================================================================
 * 索引注册 API 实现
 * ======================================================================== */

int vector_query_register_index(const char *name,
                                VectorIndexType index_type,
                                void *index_impl,
                                vector_index_search_fn search_fn,
                                vector_index_get_vector_fn get_vector_fn) {
    if (!name || !index_impl || !search_fn) {
        LOG_ERROR("注册索引参数无效");
        return -1;
    }

    /* 检查是否已存在 */
    IndexRegistryEntry *entry = g_engine.index_registry;
    while (entry) {
        if (strcmp(entry->name, name) == 0) {
            LOG_WARN("索引已存在: %s", name);
            return -1;
        }
        entry = entry->next;
    }

    /* 创建新条目 */
    entry = (IndexRegistryEntry *)malloc(sizeof(IndexRegistryEntry));
    if (!entry) {
        LOG_ERROR("分配索引注册条目失败");
        return -1;
    }

    strncpy(entry->name, name, sizeof(entry->name) - 1);
    entry->index_type = index_type;
    entry->index_impl = index_impl;
    entry->search_fn = search_fn;
    entry->get_vector_fn = get_vector_fn;
    entry->next = g_engine.index_registry;
    g_engine.index_registry = entry;
    g_engine.registry_count++;

    LOG_INFO("注册索引成功: %s", name);
    return 0;
}

int vector_query_unregister_index(const char *name) {
    IndexRegistryEntry **prev = &g_engine.index_registry;
    IndexRegistryEntry *entry = g_engine.index_registry;

    while (entry) {
        if (strcmp(entry->name, name) == 0) {
            *prev = entry->next;
            free(entry);
            g_engine.registry_count--;
            LOG_INFO("注销索引: %s", name);
            return 0;
        }
        prev = &entry->next;
        entry = entry->next;
    }

    LOG_WARN("索引不存在: %s", name);
    return -1;
}

void *vector_query_get_index(const char *name) {
    IndexRegistryEntry *entry = g_engine.index_registry;
    while (entry) {
        if (strcmp(entry->name, name) == 0) {
            return entry->index_impl;
        }
        entry = entry->next;
    }
    return NULL;
}

/* ========================================================================
 * 计划缓存 API 实现
 * ======================================================================== */

VectorQueryPlan *vector_query_get_cached_plan(const char *query_signature) {
    if (!query_signature) return NULL;

    uint64_t hash = simple_hash(query_signature) % VECTOR_QUERY_PLAN_CACHE_SIZE;

    for (int i = 0; i < g_engine.cache_count; i++) {
        VectorQueryPlan *plan = g_engine.plan_cache[i];
        if (plan) {
            char sig[128];
            vector_query_generate_signature(plan, 128, sig, sizeof(sig));
            if (strcmp(sig, query_signature) == 0) {
                g_engine.cache_hits++;
                plan->hit_count++;
                return vector_query_plan_copy(plan);
            }
        }
    }

    g_engine.cache_misses++;
    return NULL;
}

void vector_query_cache_plan(const char *query_signature,
                             const VectorQueryPlan *plan) {
    if (!query_signature || !plan) return;

    uint64_t hash = simple_hash(query_signature) % VECTOR_QUERY_PLAN_CACHE_SIZE;

    /* 如果已有计划，替换 */
    if (g_engine.plan_cache[hash]) {
        vector_query_plan_destroy(g_engine.plan_cache[hash]);
    } else {
        g_engine.cache_count++;
    }

    g_engine.plan_cache[hash] = vector_query_plan_copy(plan);
}

void vector_query_clear_plan_cache(void) {
    for (int i = 0; i < VECTOR_QUERY_PLAN_CACHE_SIZE; i++) {
        if (g_engine.plan_cache[i]) {
            vector_query_plan_destroy(g_engine.plan_cache[i]);
            g_engine.plan_cache[i] = NULL;
        }
    }
    g_engine.cache_count = 0;
    g_engine.cache_hits = 0;
    g_engine.cache_misses = 0;
}

float vector_query_get_cache_hit_rate(void) {
    uint64_t total = g_engine.cache_hits + g_engine.cache_misses;
    return total > 0 ? (float)g_engine.cache_hits / total : 0.0f;
}

/* ========================================================================
 * 统计和剖析 API 实现
 * ======================================================================== */

void vector_query_get_stats(VectorQueryStats *stats) {
    if (!stats) return;

    stats->total_queries = g_engine.total_queries;
    stats->successful_queries = g_engine.successful_queries;
    stats->failed_queries = g_engine.failed_queries;
    stats->cached_queries = g_engine.cached_queries;
    stats->total_latency_us = g_engine.total_latency_us;
    stats->avg_latency_us = g_engine.total_queries > 0 ?
        g_engine.total_latency_us / g_engine.total_queries : 0;
    stats->min_latency_us = g_engine.min_latency_us == UINT64_MAX ? 0 : g_engine.min_latency_us;
    stats->max_latency_us = g_engine.max_latency_us;
}

void vector_query_reset_stats(void) {
    g_engine.total_queries = 0;
    g_engine.successful_queries = 0;
    g_engine.failed_queries = 0;
    g_engine.cached_queries = 0;
    g_engine.total_latency_us = 0;
    g_engine.min_latency_us = UINT64_MAX;
    g_engine.max_latency_us = 0;
}

int vector_query_get_profile(VectorQueryProfileEntry *entries, int max_entries) {
    if (!entries || max_entries <= 0) return 0;

    int count = g_engine.profile_count < max_entries ? g_engine.profile_count : max_entries;
    for (int i = 0; i < count; i++) {
        entries[i] = g_engine.profiles[i];
    }
    g_engine.profile_count = 0;
    return count;
}

void vector_query_set_profiling(bool enabled) {
    g_engine.profiling_enabled = enabled;
}

bool vector_query_is_profiling(void) {
    return g_engine.profiling_enabled;
}

/* ========================================================================
 * 工具函数实现
 * ======================================================================== */

void vector_query_generate_signature(const VectorQueryPlan *plan,
                                     int32_t dims,
                                     char *out_signature,
                                     size_t sig_size) {
    if (!plan || !out_signature) return;

    const char *index_name;
    switch (plan->coarse_index_type) {
        case VECTOR_INDEX_HNSW: index_name = "hnsw"; break;
        case VECTOR_INDEX_DISKANN: index_name = "diskann"; break;
        case VECTOR_INDEX_IVF: index_name = "ivf"; break;
        default: index_name = "auto"; break;
    }

    const char *reranker_name;
    switch (plan->reranker_type) {
        case RERANKER_PRECISE: reranker_name = "precise"; break;
        case RERANKER_MMR: reranker_name = "mmr"; break;
        case RERANKER_MULTI_METRIC: reranker_name = "multi"; break;
        default: reranker_name = "none"; break;
    }

    snprintf(out_signature, sig_size, "%s_%d_%s_%d",
             index_name, dims, reranker_name, plan->top_k);
}

void vector_fusion_rrf(const float **distances,
                       const int32_t *counts,
                       int num_results,
                       int32_t k,
                       float *out_scores) {
    if (!distances || !counts || !out_scores || num_results <= 0) return;

    /* 初始化分数 */
    int total_items = 0;
    for (int i = 0; i < num_results; i++) {
        total_items += counts[i];
    }

    for (int i = 0; i < total_items; i++) {
        out_scores[i] = 0.0f;
    }

    /* 计算 RRF 分数 */
    for (int r = 0; r < num_results; r++) {
        for (int i = 0; i < counts[r]; i++) {
            int idx = i;  /* 简化：假设结果按 ID 顺序 */
            if (idx < total_items) {
                out_scores[idx] += 1.0f / (k + i + 1);
            }
        }
    }
}

void vector_fusion_weighted(const float *distances,
                            const float *weights,
                            int num_items,
                            float *out_scores) {
    if (!distances || !weights || !out_scores || num_items <= 0) return;

    for (int i = 0; i < num_items; i++) {
        out_scores[i] = distances[i] * weights[i];
    }
}

void vector_normalize_distances(const float *distances,
                                int num_items,
                                float *out_normalized) {
    if (!distances || !out_normalized || num_items <= 0) return;

    /* 找最大值和最小值 */
    float min_d = distances[0];
    float max_d = distances[0];
    for (int i = 1; i < num_items; i++) {
        if (distances[i] < min_d) min_d = distances[i];
        if (distances[i] > max_d) max_d = distances[i];
    }

    float range = max_d - min_d;
    if (range < 1e-6f) range = 1.0f;

    /* 归一化到 [0, 1] */
    for (int i = 0; i < num_items; i++) {
        out_normalized[i] = (distances[i] - min_d) / range;
    }
}

/* ========================================================================
 * 查询参数支持实现 (新增)
 * ======================================================================== */

/** 支持距离度量类型 */
static DistanceMetric parse_distance_metric(const char *metric_name) {
    if (!metric_name) return DISTANCE_METRIC_L2_SQUARED;

    if (strcmp(metric_name, "l2") == 0 || strcmp(metric_name, "euclidean") == 0) {
        return DISTANCE_METRIC_L2_SQUARED;
    } else if (strcmp(metric_name, "cosine") == 0) {
        return DISTANCE_METRIC_COSINE;
    } else if (strcmp(metric_name, "dot") == 0 || strcmp(metric_name, "ip") == 0) {
        return DISTANCE_METRIC_INNER_PRODUCT;
    } else if (strcmp(metric_name, "manhattan") == 0) {
        return DISTANCE_METRIC_HAMMING;  /* 使用 Hamming 作为近似 */
    }

    return DISTANCE_METRIC_L2_SQUARED;
}

/** 设置距离度量 */
void vector_query_plan_set_metric(VectorQueryPlan *plan, DistanceMetric metric) {
    if (!plan) return;

    /* 更新 Reranker 配置中的度量 */
    plan->reranker_config.metric = metric;

    /* 更新 Scan 算子私有数据 (如果已创建) */
    if (plan->root_node) {
        PlanNode *node = plan->root_node;
        while (node) {
            if (node->type == PLAN_NODE_VECTOR_SCAN && node->private_data) {
                VectorScanPrivate *priv = (VectorScanPrivate *)node->private_data;
                priv->metric = metric;
                break;
            }
            node = node->left;
        }
    }
}

/** 设置超时时间 */
void vector_query_plan_set_timeout(VectorQueryPlan *plan, int32_t timeout_ms) {
    if (!plan) return;
    /* 超时配置存储在执行上下文中，这里预留接口 */
}

/** 设置批次大小 */
void vector_query_plan_set_batch_size(VectorQueryPlan *plan, int32_t batch_size) {
    if (!plan) return;
    /* 批次大小存储在执行上下文中，这里预留接口 */
}

/* ========================================================================
 * 查询优化实现 (新增)
 * ======================================================================== */

/** 索引选择策略 */
typedef struct IndexSelectionStrategy_s {
    VectorIndexType recommended_index;
    int32_t recommended_ef;
    int32_t estimated_latency_ms;
    const char *reason;
} IndexSelectionStrategy;

/** 基于数据集大小选择索引 */
static IndexSelectionStrategy select_index_by_size(int32_t num_vectors, int32_t dims) {
    IndexSelectionStrategy strategy = {
        .recommended_index = VECTOR_INDEX_HNSW,
        .recommended_ef = 100,
        .estimated_latency_ms = 10,
        .reason = "default"
    };

    if (num_vectors < 10000) {
        strategy.recommended_index = VECTOR_INDEX_BRUTE_FORCE;
        strategy.recommended_ef = num_vectors;
        strategy.estimated_latency_ms = 5;
        strategy.reason = "small_dataset_brute_force";
    } else if (num_vectors < 100000) {
        strategy.recommended_index = VECTOR_INDEX_HNSW;
        strategy.recommended_ef = 100;
        strategy.estimated_latency_ms = 10;
        strategy.reason = "medium_dataset_hnsw";
    } else if (num_vectors < 1000000) {
        strategy.recommended_index = VECTOR_INDEX_HNSW;
        strategy.recommended_ef = 200;
        strategy.estimated_latency_ms = 20;
        strategy.reason = "large_dataset_hnsw_high_ef";
    } else {
        strategy.recommended_index = VECTOR_INDEX_DISKANN;
        strategy.recommended_ef = 200;
        strategy.estimated_latency_ms = 50;
        strategy.reason = "huge_dataset_diskann";
    }

    return strategy;
}

/** 估算最优 ef_search 参数 */
static int32_t estimate_optimal_ef(int32_t k, int32_t num_vectors) {
    /* 规则：ef >= k * 2，且随数据规模增长 */
    int32_t ef = k * 2;

    if (num_vectors > 100000) {
        ef = k * 3;
    }
    if (num_vectors > 1000000) {
        ef = k * 5;
    }

    /* 上限 */
    return ef > 1000 ? 1000 : ef;
}

/** 查询优化入口函数 */
VectorQueryPlan *optimize_query_plan(VectorQueryPlan *plan,
                                      int32_t num_vectors,
                                      int32_t dims,
                                      DistanceMetric metric) {
    if (!plan) return NULL;

    /* 如果已有算子链，不覆盖 */
    if (plan->root_node) {
        return plan;
    }

    /* 索引选择 */
    IndexSelectionStrategy strategy = select_index_by_size(num_vectors, dims);

    /* 调整参数 */
    plan->coarse_index_type = strategy.recommended_index;
    if (plan->coarse_ef_search <= 0) {
        plan->coarse_ef_search = estimate_optimal_ef(plan->top_k, num_vectors);
    }
    if (plan->coarse_max_candidates <= 0) {
        plan->coarse_max_candidates = plan->top_k * 10;
    }

    /* 构建算子链 */
    if (!plan->hybrid_enabled) {
        plan->root_node = create_vector_limit_node(plan->top_k, plan->offset);
        if (plan->root_node) {
            PlanNode *scan = create_vector_scan_node(
                plan->coarse_index_type,
                plan->coarse_ef_search,
                plan->coarse_max_candidates
            );
            plan->root_node->left = scan;
            plan->num_nodes = 2;
        }
    } else {
        plan->root_node = create_vector_limit_node(plan->top_k, plan->offset);
        if (plan->root_node) {
            PlanNode *join = create_vector_join_node(
                plan->fusion_type,
                plan->vector_weight,
                plan->text_weight
            );
            plan->root_node->left = join;

            if (join) {
                PlanNode *vec_scan = create_vector_scan_node(
                    plan->coarse_index_type,
                    plan->coarse_ef_search,
                    plan->coarse_max_candidates
                );
                PlanNode *text_scan = create_vector_scan_node(
                    VECTOR_INDEX_HNSW,  /* BM25 占位 */
                    plan->coarse_ef_search,
                    plan->coarse_max_candidates
                );
                join->left = vec_scan;
                join->right = text_scan;
            }
            plan->num_nodes = 4;
        }
    }

    LOG_DEBUG("查询优化完成: index=%d, ef=%d, candidates=%d, reason=%s",
              plan->coarse_index_type, plan->coarse_ef_search,
              plan->coarse_max_candidates, strategy.reason);

    return plan;
}

/* ========================================================================
 * 算子执行实现 (新增)
 * ======================================================================== */

/** 执行 VectorScan 算子 */
static VectorBatch *exec_vector_scan(PlanNode *node, ExecContext *ctx) {
    if (!node || node->type != PLAN_NODE_VECTOR_SCAN || !ctx) {
        return NULL;
    }

    VectorScanPrivate *priv = (VectorScanPrivate *)node->private_data;

    /* 从注册的索引中获取索引实现 */
    IndexRegistryEntry *entry = g_engine.index_registry;
    void *index_impl = NULL;
    vector_index_search_fn search_fn = NULL;

    /* 查找匹配的索引 (简化实现：使用第一个注册的索引) */
    if (entry) {
        index_impl = entry->index_impl;
        search_fn = entry->search_fn;
    }

    /* 分配候选缓冲区 */
    int32_t max_candidates = priv->max_candidates;
    int64_t *candidate_ids = (int64_t *)malloc(max_candidates * sizeof(int64_t));
    float *candidate_dists = (float *)malloc(max_candidates * sizeof(float));

    if (!candidate_ids || !candidate_dists) {
        free(candidate_ids);
        free(candidate_dists);
        return NULL;
    }

    int32_t found = 0;

    /* 调用索引搜索函数 */
    if (search_fn && index_impl) {
        found = search_fn(index_impl, ctx->query_vector, max_candidates,
                          candidate_ids, candidate_dists);
    } else {
        /* 占位实现：生成模拟数据 */
        found = max_candidates < 10 ? max_candidates : 10;
        for (int i = 0; i < found; i++) {
            candidate_ids[i] = i;
            candidate_dists[i] = (float)i / found;
        }
    }

    /* 构建 VectorBatch (简化实现：使用单一 VectorBlock) */
    VectorBatch *batch = vector_batch_create(found);
    VectorBlock *block = vector_block_create(found, 2);

    if (!batch || !block) {
        free(candidate_ids);
        free(candidate_dists);
        if (batch) vector_batch_destroy(batch);
        if (block) vector_block_destroy(block);
        return NULL;
    }

    /* 设置列数据 */
    vector_block_set_column(block, 0, candidate_ids, sizeof(int64_t));
    vector_block_set_column(block, 1, candidate_dists, sizeof(float));
    vector_block_set_num_rows(block, found);

    vector_batch_add_block(batch, block);

    free(candidate_ids);
    free(candidate_dists);

    LOG_DEBUG("VectorScan 完成: %d 候选", found);
    return batch;
}

/** 执行 VectorFilter 算子 */
static VectorBatch *exec_vector_filter(PlanNode *node, ExecContext *ctx, VectorBatch *input) {
    if (!node || node->type != PLAN_NODE_VECTOR_FILTER || !ctx || !input) {
        return NULL;
    }

    VectorFilterPrivate *priv = (VectorFilterPrivate *)node->private_data;
    if (!priv) {
        return input;  /* 无过滤条件，直接返回 */
    }

    /* 获取第一个块进行过滤 */
    VectorBlock *block = input->blocks[0];
    if (!block || block->num_rows == 0) {
        return input;
    }

    /* 执行过滤 */
    VectorFilterResult *filter_result = vector_filter_execute(
        block, priv->column_idx, priv->value, priv->op);

    if (!filter_result || filter_result->num_matches == 0) {
        if (filter_result) vector_filter_result_free(filter_result);
        vector_batch_destroy(input);
        return NULL;
    }

    /* 构建过滤后的批次 */
    int32_t match_count = filter_result->num_matches;
    VectorBatch *output = vector_batch_create(match_count);
    VectorBlock *out_block = vector_block_create(match_count, block->num_columns);

    if (!output || !out_block) {
        if (output) vector_batch_destroy(output);
        if (out_block) vector_block_destroy(out_block);
        vector_filter_result_free(filter_result);
        vector_batch_destroy(input);
        return NULL;
    }

    /* 复制匹配的行 */
    for (int col = 0; col < block->num_columns; col++) {
        int elem_size = block->column_sizes[col];
        char *src_col = (char *)block->columns[col];
        char *dst_col = (char *)malloc(match_count * elem_size);

        if (!dst_col) {
            vector_batch_destroy(output);
            vector_block_destroy(out_block);
            vector_filter_result_free(filter_result);
            vector_batch_destroy(input);
            return NULL;
        }

        for (int i = 0; i < match_count; i++) {
            int64_t src_idx = filter_result->matches[i];
            memcpy(dst_col + i * elem_size, src_col + src_idx * elem_size, elem_size);
        }

        vector_block_set_column(out_block, col, dst_col, elem_size);
    }

    vector_block_set_num_rows(out_block, match_count);
    vector_batch_add_block(output, out_block);

    /* 清理 */
    vector_filter_result_free(filter_result);
    vector_batch_destroy(input);

    LOG_DEBUG("VectorFilter 完成: %d 行匹配", match_count);
    return output;
}

/** 执行 VectorLimit 算子 */
static VectorBatch *exec_vector_limit(PlanNode *node, ExecContext *ctx) {
    if (!node || node->type != PLAN_NODE_VECTOR_LIMIT || !ctx) {
        return NULL;
    }

    VectorLimitPrivate *priv = (VectorLimitPrivate *)node->private_data;

    /* 执行上游算子 */
    VectorBatch *input = NULL;
    if (node->left) {
        if (node->left->type == PLAN_NODE_VECTOR_SCAN) {
            input = exec_vector_scan(node->left, ctx);
        } else if (node->left->type == PLAN_NODE_VECTOR_FILTER) {
            VectorBatch *scan_output = NULL;
            if (node->left->left && node->left->left->type == PLAN_NODE_VECTOR_SCAN) {
                scan_output = exec_vector_scan(node->left->left, ctx);
            }
            input = exec_vector_filter(node->left, ctx, scan_output);
        }
    }

    if (!input) {
        return NULL;
    }

    /* 应用 Limit */
    int32_t limit = priv->limit;
    int32_t offset = priv->offset;

    /* 获取输入批次信息 */
    int32_t total_rows = input->num_rows;
    int32_t skip = offset < total_rows ? offset : total_rows;
    int32_t take = (total_rows - skip) < limit ? (total_rows - skip) : limit;

    if (take <= 0) {
        vector_batch_destroy(input);
        return NULL;
    }

    /* 构建 limit 后的批次 */
    VectorBatch *output = vector_batch_create(take);
    VectorBlock *out_block = vector_block_create(take, input->blocks[0]->num_columns);

    if (!output || !out_block) {
        if (output) vector_batch_destroy(output);
        if (out_block) vector_block_destroy(out_block);
        vector_batch_destroy(input);
        return NULL;
    }

    /* 复制 [offset, offset+limit) 范围的数据 */
    VectorBlock *src_block = input->blocks[0];
    for (int col = 0; col < src_block->num_columns; col++) {
        int elem_size = src_block->column_sizes[col];
        char *src_col = (char *)src_block->columns[col];
        char *dst_col = (char *)malloc(take * elem_size);

        if (!dst_col) {
            vector_batch_destroy(output);
            vector_block_destroy(out_block);
            vector_batch_destroy(input);
            return NULL;
        }

        memcpy(dst_col, src_col + skip * elem_size, take * elem_size);
        vector_block_set_column(out_block, col, dst_col, elem_size);
    }

    vector_block_set_num_rows(out_block, take);
    vector_batch_add_block(output, out_block);

    /* 清理输入 */
    vector_batch_destroy(input);

    LOG_DEBUG("VectorLimit 完成: offset=%d, limit=%d, 实际返回=%d", offset, limit, take);
    return output;
}

/* ========================================================================
 * 算子链执行实现 (新增)
 * ======================================================================== */

VectorQueryResult *execute_plan(VectorQueryPlan *plan, ExecContext *ctx) {
    if (!plan || !ctx || !plan->root_node) {
        LOG_ERROR("无效的执行计划或上下文");
        return NULL;
    }

    int64_t start_time = get_time_us();
    ctx->start_time_us = start_time;
    g_engine.total_queries++;

    /* 创建结果结构 */
    VectorQueryResult *result = (VectorQueryResult *)calloc(1, sizeof(VectorQueryResult));
    if (!result) {
        g_engine.failed_queries++;
        return NULL;
    }

    result->capacity = plan->top_k;
    result->ids = (int64_t *)malloc(result->capacity * sizeof(int64_t));
    result->distances = (float *)malloc(result->capacity * sizeof(float));
    result->scores = (float *)malloc(result->capacity * sizeof(float));
    result->offset = plan->offset;

    if (!result->ids || !result->distances || !result->scores) {
        vector_query_result_free(result);
        g_engine.failed_queries++;
        return NULL;
    }

    /* 执行算子链 */
    VectorBatch *batch = NULL;
    PlanNode *root = plan->root_node;

    switch (root->type) {
        case PLAN_NODE_VECTOR_LIMIT:
            batch = exec_vector_limit(root, ctx);
            break;
        case PLAN_NODE_VECTOR_SCAN:
            batch = exec_vector_scan(root, ctx);
            break;
        case PLAN_NODE_VECTOR_FILTER:
            /* Filter 需要上游数据，从 Scan 开始 */
            if (root->left && root->left->type == PLAN_NODE_VECTOR_SCAN) {
                VectorBatch *scan_batch = exec_vector_scan(root->left, ctx);
                batch = exec_vector_filter(root, ctx, scan_batch);
            }
            break;
        default:
            LOG_ERROR("未实现的算子类型: %d", root->type);
            break;
    }

    /* 从批次构建结果 */
    if (batch && batch->blocks[0]) {
        VectorBlock *block = batch->blocks[0];
        int64_t *ids = (int64_t *)vector_block_get_column(block, 0);
        float *dists = (float *)vector_block_get_column(block, 1);

        result->count = block->num_rows < plan->top_k ? block->num_rows : plan->top_k;
        result->candidates = batch->num_rows;

        memcpy(result->ids, ids, result->count * sizeof(int64_t));
        memcpy(result->distances, dists, result->count * sizeof(float));

        for (int i = 0; i < result->count; i++) {
            result->scores[i] = 1.0f - dists[i];  /* 简化分数计算 */
        }

        vector_batch_destroy(batch);
    }

    /* 统计信息 */
    int64_t end_time = get_time_us();
    result->total_time_us = end_time - start_time;

    g_engine.successful_queries++;
    g_engine.total_latency_us += result->total_time_us;

    if (result->total_time_us < g_engine.min_latency_us) {
        g_engine.min_latency_us = result->total_time_us;
    }
    if (result->total_time_us > g_engine.max_latency_us) {
        g_engine.max_latency_us = result->total_time_us;
    }

    LOG_DEBUG("算子链执行完成: count=%d, time=%ldus",
              result->count, result->total_time_us);

    return result;
}

/* ========================================================================
 * 查询执行实现
 * ======================================================================== */

VectorQueryResult *vector_query_execute(const VectorQueryPlan *plan,
                                        const float *query,
                                        int32_t dims) {
    return vector_query_execute_filtered(plan, query, dims, NULL);
}

VectorQueryResult *vector_query_execute_filtered(const VectorQueryPlan *plan,
                                                  const float *query,
                                                  int32_t dims,
                                                  const FilterExpr *filter) {
    if (!vector_query_is_ready()) {
        LOG_ERROR("执行器未初始化");
        return NULL;
    }

    if (!plan || !query || dims <= 0) {
        LOG_ERROR("无效的查询参数");
        return NULL;
    }

    int64_t start_time = get_time_us();
    g_engine.total_queries++;

    /* 记录剖析 */
    int profile_idx = 0;
    if (g_engine.profiling_enabled) {
        g_engine.profiles[profile_idx].stage = STAGE_PLAN;
        g_engine.profiles[profile_idx].start_us = start_time;
    }

    /* 创建结果结构 */
    VectorQueryResult *result = (VectorQueryResult *)calloc(1, sizeof(VectorQueryResult));
    if (!result) {
        g_engine.failed_queries++;
        return NULL;
    }

    result->capacity = plan->top_k;
    result->ids = (int64_t *)malloc(result->capacity * sizeof(int64_t));
    result->distances = (float *)malloc(result->capacity * sizeof(float));
    result->scores = (float *)malloc(result->capacity * sizeof(float));

    if (!result->ids || !result->distances || !result->scores) {
        vector_query_result_free(result);
        g_engine.failed_queries++;
        return NULL;
    }

    /* ========== 阶段 1: 粗排 ========== */
    int64_t coarse_start = get_time_us();

    /* TODO: 从注册的索引中获取并执行搜索 */
    /* 目前使用占位实现 */
    result->candidates = plan->coarse_max_candidates;
    for (int i = 0; i < result->candidates && i < plan->top_k; i++) {
        result->ids[i] = i;
        result->distances[i] = (float)i / plan->top_k;
    }
    result->count = plan->top_k < result->candidates ? plan->top_k : result->candidates;

    result->coarse_time_us = get_time_us() - coarse_start;

    if (g_engine.profiling_enabled) {
        g_engine.profiles[profile_idx].end_us = get_time_us();
        g_engine.profiles[profile_idx].duration_us = result->coarse_time_us;
        profile_idx++;
    }

    /* ========== 阶段 2: 精排 ========== */
    int64_t rerank_start = get_time_us();

    /* 创建 ReRanker */
    Reranker *reranker = reranker_create(plan->reranker_type, &plan->reranker_config);
    if (reranker) {
        /* 构建候选向量数组 (简化实现) */
        float *candidates = (float *)malloc(result->count * dims * sizeof(float));
        if (candidates) {
            /* TODO: 从持久化层获取实际向量数据 */
            for (int i = 0; i < result->count; i++) {
                for (int d = 0; d < dims; d++) {
                    candidates[i * dims + d] = (float)(i + d);
                }
            }

            reranker_rerank(reranker, query, candidates, result->count,
                           plan->top_k, result->scores, (int32_t *)result->ids);

            free(candidates);
        }
        reranker_destroy(reranker);
    }

    result->rerank_time_us = get_time_us() - rerank_start;

    if (g_engine.profiling_enabled) {
        g_engine.profiles[profile_idx].stage = STAGE_RERANK;
        g_engine.profiles[profile_idx].start_us = rerank_start;
        g_engine.profiles[profile_idx].end_us = get_time_us();
        g_engine.profiles[profile_idx].duration_us = result->rerank_time_us;
        profile_idx++;
    }

    /* 总耗时 */
    int64_t end_time = get_time_us();
    result->total_time_us = end_time - start_time;

    /* 更新统计 */
    g_engine.successful_queries++;
    g_engine.total_latency_us += result->total_time_us;
    if (result->total_time_us < g_engine.min_latency_us) {
        g_engine.min_latency_us = result->total_time_us;
    }
    if (result->total_time_us > g_engine.max_latency_us) {
        g_engine.max_latency_us = result->total_time_us;
    }

    if (g_engine.profiling_enabled) {
        g_engine.profile_count = profile_idx;
    }

    LOG_DEBUG("查询执行完成: candidates=%d, result=%d, time=%ldus",
              result->candidates, result->count, result->total_time_us);

    return result;
}

int vector_query_cancel(void) {
    if (g_engine.cancelled) {
        LOG_WARN("查询已经在取消中");
        return -1;
    }

    g_engine.cancelled = true;
    LOG_INFO("查询已标记为取消");
    return 0;
}
