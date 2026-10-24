/**
 * @file dist_hnsw.c
 * @brief 分布式 HNSW 索引实现
 */

#include "db/index/vector_index/dist_hnsw/dist_hnsw.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

/*** 常量 ***/
#define DIST_HNSW_MAGIC 0x44480000

/*** 内部结构 ***/
typedef struct {
    uint64_t shard_id;
    uint64_t start_key;
    uint64_t end_key;
    dist_hnsw_shard_state_t state;
    uint64_t num_vectors;
    size_t size_bytes;
    char address[256];
    uint16_t port;
    void *hnsw_index;
    pthread_mutex_t lock;
} shard_t;

typedef struct {
    uint64_t node_id;
    char address[256];
    uint16_t port;
    bool is_local;
} node_t;

struct dist_hnsw {
    uint32_t magic;
    dist_hnsw_config_t config;
    pthread_mutex_t lock;

    /* 分片 */
    shard_t **shards;
    uint32_t num_shards;
    uint32_t max_shards;

    /* 节点 */
    node_t **nodes;
    uint32_t num_nodes;
    uint32_t max_nodes;

    /* 统计 */
    uint64_t total_vectors;
    uint64_t search_requests;
    double total_latency_ms;
};

/*** 工具函数 ***/
static uint64_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static uint32_t hash_shard(dist_hnsw_t *index, uint64_t id) {
    return id % index->num_shards;
}

/*** 生命周期 ***/
dist_hnsw_t *dist_hnsw_create(const dist_hnsw_config_t *config) {
    if (!config) return NULL;

    dist_hnsw_t *index = calloc(1, sizeof(dist_hnsw_t));
    if (!index) return NULL;

    index->magic = DIST_HNSW_MAGIC;
    index->config = *config;
    pthread_mutex_init(&index->lock, NULL);

    index->max_shards = config->num_shards > 0 ? config->num_shards : DIST_HNSW_DEFAULT_NUM_SHARDS;
    index->shards = calloc(index->max_shards, sizeof(shard_t *));
    index->num_shards = 0;

    index->max_nodes = DIST_HNSW_MAX_NODES;
    index->nodes = calloc(index->max_nodes, sizeof(node_t *));
    index->num_nodes = 0;

    return index;
}

void dist_hnsw_close(dist_hnsw_t *index) {
    if (!index) return;

    for (uint32_t i = 0; i < index->num_shards; i++) {
        if (index->shards[i]) {
            pthread_mutex_destroy(&index->shards[i]->lock);
            free(index->shards[i]);
        }
    }
    free(index->shards);

    for (uint32_t i = 0; i < index->num_nodes; i++) {
        free(index->nodes[i]);
    }
    free(index->nodes);

    pthread_mutex_destroy(&index->lock);
    free(index);
}

/*** 向量操作 ***/
int dist_hnsw_insert(dist_hnsw_t *index, uint64_t id, const float *vector,
                     const void *metadata, size_t metadata_size) {
    if (!index || !vector) return -1;

    pthread_mutex_lock(&index->lock);

    uint32_t shard_id = hash_shard(index, id);

    if (!index->shards[shard_id]) {
        index->shards[shard_id] = calloc(1, sizeof(shard_t));
        index->shards[shard_id]->shard_id = shard_id;
        index->shards[shard_id]->start_key = shard_id * (UINT64_MAX / index->max_shards);
        index->shards[shard_id]->end_key = (shard_id + 1) * (UINT64_MAX / index->max_shards) - 1;
        index->shards[shard_id]->state = SHARD_STATE_ACTIVE;
        pthread_mutex_init(&index->shards[shard_id]->lock, NULL);
        index->num_shards++;
    }

    index->shards[shard_id]->num_vectors++;
    index->total_vectors++;

    pthread_mutex_unlock(&index->lock);
    return 0;
}

size_t dist_hnsw_batch_insert(dist_hnsw_t *index, const uint64_t *ids,
                                const float *vectors, size_t count) {
    if (!index || !ids || !vectors) return 0;

    size_t inserted = 0;
    for (size_t i = 0; i < count; i++) {
        if (dist_hnsw_insert(index, ids[i], vectors + i * index->config.dimension, NULL, 0) == 0) {
            inserted++;
        }
    }
    return inserted;
}

int dist_hnsw_delete(dist_hnsw_t *index, uint64_t id) {
    if (!index) return -1;

    pthread_mutex_lock(&index->lock);
    uint32_t shard_id = hash_shard(index, id);

    if (index->shards[shard_id] && index->shards[shard_id]->num_vectors > 0) {
        index->shards[shard_id]->num_vectors--;
        index->total_vectors--;
    }

    pthread_mutex_unlock(&index->lock);
    return 0;
}

int dist_hnsw_update(dist_hnsw_t *index, uint64_t id, const float *vector) {
    if (!index) return -1;
    /* 更新等同删除后插入 */
    dist_hnsw_delete(index, id);
    return dist_hnsw_insert(index, id, vector, NULL, 0);
}

/*** 搜索操作 ***/
size_t dist_hnsw_search(dist_hnsw_t *index, const float *query, size_t k,
                         dist_hnsw_search_result_t *results) {
    if (!index || !query || !results) return 0;

    uint64_t start = now_ms();
    pthread_mutex_lock(&index->lock);

    /* 并行搜索所有分片（简化实现：模拟结果） */
    size_t total_results = 0;
    for (uint32_t i = 0; i < index->num_shards && total_results < k; i++) {
        if (index->shards[i] && index->shards[i]->state == SHARD_STATE_ACTIVE) {
            /* TODO: 实际调用 HNSW 搜索 */
            for (size_t j = 0; j < k / index->num_shards && total_results < k; j++) {
                results[total_results].id = i * 1000 + j;
                results[total_results].distance = (float)(total_results + 1) * 0.1f;
                results[total_results].metadata = NULL;
                total_results++;
            }
        }
    }

    index->search_requests++;
    index->total_latency_ms += (now_ms() - start);

    pthread_mutex_unlock(&index->lock);
    return total_results;
}

size_t dist_hnsw_range_search(dist_hnsw_t *index, const float *query,
                                float radius, dist_hnsw_search_result_t *results,
                                size_t max_results) {
    return dist_hnsw_search(index, query, max_results, results);
}

size_t dist_hnsw_search_with_filter(dist_hnsw_t *index, const float *query,
                                      size_t k, const uint64_t *filter_ids,
                                      size_t filter_count,
                                      dist_hnsw_search_result_t *results) {
    /* TODO: 实现过滤搜索 */
    return dist_hnsw_search(index, query, k, results);
}

void dist_hnsw_free_results(dist_hnsw_search_result_t *results, size_t count) {
    /* 结果数组由调用者管理，此处无需释放 */
    (void)results;
    (void)count;
}

/*** 分片管理 ***/
int dist_hnsw_add_shard(dist_hnsw_t *index, uint32_t shard_id,
                        const char *address, uint16_t port) {
    if (!index || shard_id >= index->max_shards) return -1;

    pthread_mutex_lock(&index->lock);

    if (!index->shards[shard_id]) {
        index->shards[shard_id] = calloc(1, sizeof(shard_t));
        pthread_mutex_init(&index->shards[shard_id]->lock, NULL);
        index->num_shards++;
    }

    index->shards[shard_id]->shard_id = shard_id;
    index->shards[shard_id]->state = SHARD_STATE_ACTIVE;
    strncpy(index->shards[shard_id]->address, address ? address : "", 255);
    index->shards[shard_id]->port = port;

    pthread_mutex_unlock(&index->lock);
    return 0;
}

int dist_hnsw_remove_shard(dist_hnsw_t *index, uint32_t shard_id) {
    if (!index || shard_id >= index->max_shards) return -1;

    pthread_mutex_lock(&index->lock);

    if (index->shards[shard_id]) {
        index->shards[shard_id]->state = SHARD_STATE_OFFLINE;
    }

    pthread_mutex_unlock(&index->lock);
    return 0;
}

const dist_hnsw_shard_info_t *dist_hnsw_get_shard_info(const dist_hnsw_t *index,
                                                        uint32_t shard_id) {
    if (!index || shard_id >= index->max_shards) return NULL;
    return (const dist_hnsw_shard_info_t *)index->shards[shard_id];
}

dist_hnsw_shard_info_t *dist_hnsw_get_all_shards(const dist_hnsw_t *index,
                                                  uint32_t *out_count) {
    if (!index || !out_count) return NULL;

    pthread_mutex_lock((pthread_mutex_t *)&index->lock);

    *out_count = index->num_shards;
    dist_hnsw_shard_info_t *shards = calloc(index->num_shards, sizeof(dist_hnsw_shard_info_t));

    uint32_t count = 0;
    for (uint32_t i = 0; i < index->max_shards && count < index->num_shards; i++) {
        if (index->shards[i]) {
            shards[count].shard_id = index->shards[i]->shard_id;
            shards[count].start_key = index->shards[i]->start_key;
            shards[count].end_key = index->shards[i]->end_key;
            shards[count].state = index->shards[i]->state;
            shards[count].num_vectors = index->shards[i]->num_vectors;
            shards[count].size_bytes = index->shards[i]->size_bytes;
            count++;
        }
    }

    pthread_mutex_unlock((pthread_mutex_t *)&index->lock);
    return shards;
}

int dist_hnsw_rebalance(dist_hnsw_t *index) {
    if (!index) return -1;
    /* TODO: 实现分片再平衡 */
    return 0;
}

/*** 节点管理 ***/
int dist_hnsw_add_node(dist_hnsw_t *index, uint64_t node_id,
                       const char *address, uint16_t port) {
    if (!index || index->num_nodes >= index->max_nodes) return -1;

    pthread_mutex_lock(&index->lock);

    node_t *node = calloc(1, sizeof(node_t));
    node->node_id = node_id;
    strncpy(node->address, address ? address : "", 255);
    node->port = port;
    node->is_local = (index->num_nodes == 0);

    index->nodes[index->num_nodes++] = node;

    pthread_mutex_unlock(&index->lock);
    return 0;
}

int dist_hnsw_remove_node(dist_hnsw_t *index, uint64_t node_id) {
    if (!index) return -1;

    pthread_mutex_lock(&index->lock);

    for (uint32_t i = 0; i < index->num_nodes; i++) {
        if (index->nodes[i]->node_id == node_id) {
            free(index->nodes[i]);
            index->num_nodes--;
            break;
        }
    }

    pthread_mutex_unlock(&index->lock);
    return 0;
}

uint32_t dist_hnsw_get_num_nodes(const dist_hnsw_t *index) {
    return index ? index->num_nodes : 0;
}

/*** 统计信息 ***/
void dist_hnsw_get_stats(const dist_hnsw_t *index, dist_hnsw_stats_t *stats) {
    if (!index || !stats) return;

    memset(stats, 0, sizeof(dist_hnsw_stats_t));

    stats->num_vectors = index->total_vectors;
    stats->num_shards = index->num_shards;
    stats->num_nodes = index->num_nodes;
    stats->search_requests = index->search_requests;

    if (index->search_requests > 0) {
        stats->avg_search_latency_ms = index->total_latency_ms / index->search_requests;
    }
}

/*** 持久化 ***/
int dist_hnsw_save(dist_hnsw_t *index) {
    if (!index) return -1;
    /* TODO: 实现持久化 */
    return 0;
}

int dist_hnsw_load(dist_hnsw_t *index) {
    if (!index) return -1;
    /* TODO: 实现加载 */
    return 0;
}

void dist_hnsw_save_async(dist_hnsw_t *index) {
    if (index) dist_hnsw_save(index);
}

/* Getters */
const dist_hnsw_config_t *dist_hnsw_get_config(const dist_hnsw_t *index) {
    return index ? &index->config : NULL;
}

uint64_t dist_hnsw_get_num_vectors(const dist_hnsw_t *index) {
    return index ? index->total_vectors : 0;
}
