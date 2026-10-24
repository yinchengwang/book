/*
 * sharding.c - 分片支持实现
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <db/sharding/sharding.h>

/* ─────────────────────────────────────────────────────────────────
 * 分片路由器结构
 * ───────────────────────────────────────────────────────────────── */

struct shard_router {
    shard_config_t *config;        /* 分片配置 */
    shard_info_t **shards;          /* 分片数组 */
    int shard_count;               /* 分片数量 */
    int capacity;                  /* 数组容量 */
};

/* ─────────────────────────────────────────────────────────────────
 * 哈希函数（FNV-1a）
 * ───────────────────────────────────────────────────────────────── */

static uint64_t fnv_hash(const void *data, size_t len)
{
    const unsigned char *p = (const unsigned char *)data;
    uint64_t h = 14695981039346656037ULL;  /* FNV offset basis */

    for (size_t i = 0; i < len; i++) {
        h ^= p[i];
        h *= 1099511628211ULL;  /* FNV prime */
    }

    return h;
}

/* ─────────────────────────────────────────────────────────────────
 * 分片路由器实现
 * ───────────────────────────────────────────────────────────────── */

shard_router_t *shard_router_create(const shard_config_t *config)
{
    if (config == NULL) return NULL;

    shard_router_t *router = (shard_router_t *)calloc(1, sizeof(shard_router_t));
    if (router == NULL) return NULL;

    router->config = (shard_config_t *)malloc(sizeof(shard_config_t));
    if (router->config == NULL) {
        free(router);
        return NULL;
    }

    memcpy(router->config, config, sizeof(shard_config_t));
    router->shard_count = 0;
    router->capacity = 16;
    router->shards = (shard_info_t **)calloc(router->capacity, sizeof(shard_info_t *));

    if (router->shards == NULL) {
        free(router->config);
        free(router);
        return NULL;
    }

    return router;
}

void shard_router_destroy(shard_router_t *router)
{
    if (router == NULL) return;

    for (int i = 0; i < router->shard_count; i++) {
        if (router->shards[i]) {
            shard_info_destroy(router->shards[i]);
        }
    }
    free(router->shards);
    if (router->config) free(router->config);
    free(router);
}

int shard_router_add(shard_router_t *router, const shard_info_t *shard)
{
    if (router == NULL || shard == NULL) return -1;

    /* 扩展容量 */
    if (router->shard_count >= router->capacity) {
        int new_capacity = router->capacity * 2;
        shard_info_t **new_shards = (shard_info_t **)realloc(
            router->shards, new_capacity * sizeof(shard_info_t *));
        if (new_shards == NULL) return -1;
        router->shards = new_shards;
        router->capacity = new_capacity;
    }

    shard_info_t *copy = shard_info_create(shard->shard_id, shard->shard_name);
    if (copy == NULL) return -1;

    copy->host = shard->host ? strdup(shard->host) : NULL;
    copy->port = shard->port;
    copy->min_value = shard->min_value;
    copy->max_value = shard->max_value;
    copy->row_count = shard->row_count;
    copy->is_primary = shard->is_primary;

    router->shards[router->shard_count++] = copy;
    return 0;
}

int shard_router_remove(shard_router_t *router, int shard_id)
{
    if (router == NULL) return -1;

    for (int i = 0; i < router->shard_count; i++) {
        if (router->shards[i] && router->shards[i]->shard_id == shard_id) {
            shard_info_destroy(router->shards[i]);

            /* 移动数组元素 */
            for (int j = i; j < router->shard_count - 1; j++) {
                router->shards[j] = router->shards[j + 1];
            }
            router->shard_count--;
            return 0;
        }
    }
    return -1;
}

uint64_t shard_calculate_hash(const shard_router_t *router,
                              const void *key, size_t key_len)
{
    (void)router;
    return fnv_hash(key, key_len);
}

int shard_route(const shard_router_t *router, const void *key, size_t key_len)
{
    if (router == NULL || key == NULL || router->shard_count == 0) return -1;

    uint64_t hash = shard_calculate_hash(router, key, key_len);

    switch (router->config->strategy) {
        case SHARD_HASH:
            /* 一致性哈希 */
            return (int)(hash % router->shard_count);

        case SHARD_RANGE: {
            /* 范围分片 */
            int64_t int_key = *(const int64_t *)key;
            for (int i = 0; i < router->shard_count; i++) {
                if (router->shards[i] &&
                    int_key >= router->shards[i]->min_value &&
                    int_key <= router->shards[i]->max_value) {
                    return i;
                }
            }
            return 0;  /* 默认返回第一个分片 */
        }

        case SHARD_LIST:
            /* 列表分片 */
            return (int)(hash % router->shard_count);

        default:
            return (int)(hash % router->shard_count);
    }
}

int shard_route_range(const shard_router_t *router,
                      const void *min_key, const void *max_key,
                      int *shard_ids, int max_count)
{
    if (router == NULL || shard_ids == NULL || max_count <= 0) return 0;

    int count = 0;

    switch (router->config->strategy) {
        case SHARD_HASH: {
            /* Hash 分片：范围查询可能需要扫描所有分片 */
            int num_shards = router->shard_count < max_count ?
                            router->shard_count : max_count;
            for (int i = 0; i < num_shards; i++) {
                shard_ids[count++] = i;
            }
            break;
        }

        case SHARD_RANGE: {
            /* Range 分片：精确查找 */
            int64_t min_val = *(const int64_t *)min_key;
            int64_t max_val = *(const int64_t *)max_key;

            for (int i = 0; i < router->shard_count && count < max_count; i++) {
                if (router->shards[i]) {
                    /* 检查分片是否与范围重叠 */
                    if (max_val >= router->shards[i]->min_value &&
                        min_val <= router->shards[i]->max_value) {
                        shard_ids[count++] = i;
                    }
                }
            }
            break;
        }

        default:
            break;
    }

    return count;
}

const shard_info_t *shard_get_info(const shard_router_t *router, int shard_id)
{
    if (router == NULL) return NULL;

    for (int i = 0; i < router->shard_count; i++) {
        if (router->shards[i] && router->shards[i]->shard_id == shard_id) {
            return router->shards[i];
        }
    }
    return NULL;
}

int shard_get_all(const shard_router_t *router,
                  shard_info_t *shards, int max_count)
{
    if (router == NULL || shards == NULL) return 0;

    int count = router->shard_count < max_count ?
                router->shard_count : max_count;

    for (int i = 0; i < count; i++) {
        memcpy(&shards[i], router->shards[i], sizeof(shard_info_t));
    }

    return count;
}

int shard_count(const shard_router_t *router)
{
    return router ? router->shard_count : 0;
}

/* ─────────────────────────────────────────────────────────────────
 * 跨分片查询实现
 * ───────────────────────────────────────────────────────────────── */

cross_shard_query_t *cross_shard_query_create(shard_router_t *router,
                                              const void *key, size_t key_len)
{
    if (router == NULL) return NULL;

    cross_shard_query_t *query = (cross_shard_query_t *)calloc(1,
        sizeof(cross_shard_query_t));
    if (query == NULL) return NULL;

    query->shard_count = 0;
    query->shard_ids = (int *)calloc(16, sizeof(int));
    query->query_template = NULL;
    query->params = NULL;
    query->param_count = 0;

    if (query->shard_ids == NULL) {
        free(query);
        return NULL;
    }

    /* 路由主分片 */
    int shard_id = shard_route(router, key, key_len);
    query->shard_ids[query->shard_count++] = shard_id;

    return query;
}

int cross_shard_query_add_shard(cross_shard_query_t *query, int shard_id)
{
    if (query == NULL) return -1;

    /* 检查是否已存在 */
    for (int i = 0; i < query->shard_count; i++) {
        if (query->shard_ids[i] == shard_id) return 0;
    }

    query->shard_ids[query->shard_count++] = shard_id;
    return 0;
}

void cross_shard_query_destroy(cross_shard_query_t *query)
{
    if (query == NULL) return;

    if (query->shard_ids) free(query->shard_ids);
    if (query->query_template) free(query->query_template);
    if (query->params) free(query->params);
    free(query);
}

/* ─────────────────────────────────────────────────────────────────
 * 分片再平衡实现
 * ───────────────────────────────────────────────────────────────── */

bool shard_needs_rebalance(const shard_router_t *router, double skew_threshold)
{
    if (router == NULL || router->shard_count == 0) return false;

    /* 计算平均行数 */
    uint64_t total_rows = 0;
    uint64_t max_rows = 0;

    for (int i = 0; i < router->shard_count; i++) {
        if (router->shards[i]) {
            total_rows += router->shards[i]->row_count;
            if (router->shards[i]->row_count > max_rows) {
                max_rows = router->shards[i]->row_count;
            }
        }
    }

    if (total_rows == 0) return false;

    double avg_rows = (double)total_rows / router->shard_count;
    double skew = (max_rows - avg_rows) / avg_rows;

    return skew > skew_threshold;
}

int shard_do_rebalance(shard_router_t *router)
{
    if (router == NULL) return -1;

    /* TODO: 实现实际的再平衡逻辑
     * 1. 分析数据分布
     * 2. 生成迁移计划
     * 3. 执行数据迁移
     * 4. 更新路由表
     */

    return 0;
}

/* ─────────────────────────────────────────────────────────────────
 * 辅助函数实现
 * ───────────────────────────────────────────────────────────────── */

shard_config_t *shard_config_create(shard_strategy_t strategy, int num_shards)
{
    shard_config_t *config = (shard_config_t *)calloc(1, sizeof(shard_config_t));
    if (config == NULL) return NULL;

    config->strategy = strategy;
    config->num_shards = num_shards;
    config->key_type = SHARD_KEY_INT;
    config->replication_factor = 1;
    config->consistent_hashing = false;

    return config;
}

void shard_config_destroy(shard_config_t *config)
{
    free(config);
}

shard_info_t *shard_info_create(int shard_id, const char *name)
{
    shard_info_t *info = (shard_info_t *)calloc(1, sizeof(shard_info_t));
    if (info == NULL) return NULL;

    info->shard_id = shard_id;
    info->shard_name = name ? strdup(name) : NULL;
    info->host = NULL;
    info->port = 0;
    info->min_value = 0;
    info->max_value = 0;
    info->row_count = 0;
    info->is_primary = false;

    return info;
}

void shard_info_destroy(shard_info_t *info)
{
    if (info == NULL) return;

    if (info->shard_name) free(info->shard_name);
    if (info->host) free(info->host);
    free(info);
}

const char *shard_strategy_name(shard_strategy_t strategy)
{
    switch (strategy) {
        case SHARD_HASH:  return "hash";
        case SHARD_RANGE: return "range";
        case SHARD_LIST:  return "list";
        default: return "unknown";
    }
}

/* ─────────────────────────────────────────────────────────────────
 * 向量分片支持
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 计算向量的一致性哈希
 */
uint64_t vector_consistent_hash(const float *vector, int dim) {
    if (vector == NULL || dim <= 0) return 0;

    /* 使用向量前几个元素的哈希 */
    uint64_t hash = 14695981039346656037ULL;
    for (int i = 0; i < dim && i < 16; i++) {
        /* 将 float 转换为字节序列 */
        uint32_t bits = *(uint32_t *)(&vector[i]);
        hash ^= bits;
        hash *= 1099511628211ULL;
    }
    return hash;
}

/**
 * @brief 创建向量分片键
 */
vector_shard_key_t *vector_shard_key_create(const shard_router_t *router,
                                           int64_t vector_id,
                                           const float *vector,
                                           int dim) {
    (void)router;  /* 预留用于更复杂的分片策略 */

    vector_shard_key_t *key = (vector_shard_key_t *)calloc(1, sizeof(vector_shard_key_t));
    if (key == NULL) return NULL;

    key->vector_id = vector_id;
    key->hash = vector_consistent_hash(vector, dim);

    return key;
}

/**
 * @brief 路由向量搜索到分片
 *
 * 对于向量搜索，需要搜索所有分片并合并结果。
 */
int *vector_shard_route_search(const shard_router_t *router,
                               const float *query_vector,
                               int dim, int top_k) {
    (void)top_k;  /* 预留用于更复杂的策略 */
    if (router == NULL) return NULL;

    int num_shards = router->shard_count;
    if (num_shards <= 0) return NULL;

    /* 分配结果数组 */
    int *shard_ids = (int *)calloc(num_shards, sizeof(int));
    if (shard_ids == NULL) return NULL;

    /* 所有分片都需要搜索（向量搜索的特点） */
    for (int i = 0; i < num_shards; i++) {
        shard_ids[i] = i;
    }

    (void)query_vector;
    (void)dim;
    return shard_ids;
}

/**
 * @brief 合并多个分片的搜索结果
 */
vector_shard_result_t *vector_shard_merge_results(int num_shards,
                                                 vector_shard_result_t **shard_results,
                                                 int *result_counts,
                                                 int top_k) {
    if (num_shards <= 0 || shard_results == NULL || result_counts == NULL) {
        return NULL;
    }

    /* 统计总结果数 */
    int total = 0;
    for (int i = 0; i < num_shards; i++) {
        total += result_counts[i];
    }

    if (total == 0) return NULL;

    /* 分配临时数组用于排序 */
    vector_shard_result_t *all_results = (vector_shard_result_t *)malloc(
        total * sizeof(vector_shard_result_t));
    if (all_results == NULL) return NULL;

    /* 收集所有结果 */
    int idx = 0;
    for (int i = 0; i < num_shards; i++) {
        for (int j = 0; j < result_counts[i]; j++) {
            all_results[idx++] = shard_results[i][j];
        }
    }

    /* 简单的插入排序（按距离升序） */
    for (int i = 1; i < total; i++) {
        vector_shard_result_t key = all_results[i];
        int j = i - 1;
        while (j >= 0 && all_results[j].distance > key.distance) {
            all_results[j + 1] = all_results[j];
            j--;
        }
        all_results[j + 1] = key;
    }

    /* 分配最终结果 */
    vector_shard_result_t *results = (vector_shard_result_t *)malloc(
        top_k * sizeof(vector_shard_result_t));
    if (results == NULL) {
        free(all_results);
        return NULL;
    }

    /* 取前 top_k 个结果 */
    int count = total < top_k ? total : top_k;
    for (int i = 0; i < count; i++) {
        results[i] = all_results[i];
    }

    free(all_results);
    return results;
}