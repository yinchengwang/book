/**
 * @file hnsw_pq.c
 * @brief HNSW-PQ 混合索引实现
 *
 * 核心流程：
 * 1. 构建：插入向量 → PQ 编码 → 构建图连接
 * 2. 搜索：ADC 导航 → 收集候选 → 重排序
 */

#include "db/index/vector_index/hnsw_pq/hnsw_pq.h"
#include "db/index/vector_index/pq/pq.h"
#include "db/index/heap/heap_vector_store.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ========== 内部结构 ========== */

typedef struct {
    int32_t id;
    float distance;
} hnsw_pq_candidate_t;

/* ========== 辅助函数 ========== */

static int candidate_cmp(const void *a, const void *b) {
    const hnsw_pq_candidate_t *ca = (const hnsw_pq_candidate_t *)a;
    const hnsw_pq_candidate_t *cb = (const hnsw_pq_candidate_t *)b;
    if (ca->distance < cb->distance) return -1;
    if (ca->distance > cb->distance) return 1;
    return 0;
}

/* ADC 距离计算：查询向量与节点 PQ 编码的距离 */
static float compute_adc_distance(hnsw_pq_index_t *index,
                                   const float *query,
                                   const uint8_t *code,
                                   const float *distance_table) {
    if (!index->pq || !code || !distance_table) return INFINITY;
    return pq_adc_distance(index->pq, code, distance_table);
}

/* 从 heap_store 获取原始向量 */
static int get_vector_from_heap(hnsw_pq_index_t *index, int32_t node_id, float *out_vec) {
    if (index->use_heap_store && index->heap_store) {
        vector_ref_t ref = {node_id};
        return heap_vector_get(index->heap_store, &ref, out_vec);
    }
    return -1;
}

/* ========== 公开 API 实现 ========== */

hnsw_pq_index_t *hnsw_pq_index_create(const hnsw_pq_config_t *config) {
    if (!config) return NULL;

    hnsw_pq_index_t *index = (hnsw_pq_index_t *)calloc(1, sizeof(hnsw_pq_index_t));
    if (!index) return NULL;

    memcpy(&index->config, config, sizeof(hnsw_pq_config_t));

    /* 默认参数 */
    if (index->config.ef_search == 0) {
        index->config.ef_search = index->config.ef_construction;
    }
    if (index->config.rerank_k == 0) {
        index->config.rerank_k = 100;  /* 默认重排序 100 个候选 */
    }

    /* 分配节点数组 */
    index->capacity = 1024;
    index->nodes = (hnsw_pq_node_t *)calloc(index->capacity, sizeof(hnsw_pq_node_t));
    index->entry_point = -1;

    return index;
}

void hnsw_pq_index_destroy(hnsw_pq_index_t *index) {
    if (!index) return;

    if (index->nodes) {
        for (int32_t i = 0; i < index->n_nodes; i++) {
            if (index->nodes[i].neighbors) {
                free(index->nodes[i].neighbors);
            }
            if (index->nodes[i].pq_code) {
                free(index->nodes[i].pq_code);
            }
        }
        free(index->nodes);
    }

    free(index);
}

void hnsw_pq_index_set_pq(hnsw_pq_index_t *index, pq_quantizer_t *pq) {
    if (index) index->pq = pq;
}

void hnsw_pq_index_set_heap_store(hnsw_pq_index_t *index, heap_vector_store_t *store) {
    if (index) {
        index->heap_store = store;
        index->use_heap_store = (store != NULL);
    }
}

int hnsw_pq_index_train(hnsw_pq_index_t *index, int n, const float *vectors) {
    if (!index || !index->pq || !vectors || n <= 0) return -1;
    return pq_train(index->pq, n, vectors);
}

int32_t hnsw_pq_index_insert(hnsw_pq_index_t *index, const float *vector) {
    if (!index || !vector) return -1;

    /* 检查容量 */
    if (index->n_nodes >= index->capacity) {
        int32_t new_capacity = index->capacity * 2;
        hnsw_pq_node_t *new_nodes = (hnsw_pq_node_t *)realloc(
            index->nodes, new_capacity * sizeof(hnsw_pq_node_t));
        if (!new_nodes) return -1;
        memset(new_nodes + index->capacity, 0,
               (new_capacity - index->capacity) * sizeof(hnsw_pq_node_t));
        index->nodes = new_nodes;
        index->capacity = new_capacity;
    }

    int32_t new_id = index->n_nodes;

    /* 存储原始向量到 heap_store */
    if (index->use_heap_store && index->heap_store) {
        vector_ref_t ref = heap_vector_insert(index->heap_store, vector);
        (void)ref;
    }

    /* 创建新节点 */
    hnsw_pq_node_t *node = &index->nodes[new_id];
    node->id = new_id;
    node->neighbors = NULL;
    node->n_neighbors = 0;

    /* PQ 编码 */
    if (index->pq && pq_is_trained(index->pq)) {
        node->pq_code = (uint8_t *)malloc(pq_code_size(index->pq));
        if (node->pq_code) {
            pq_encode(index->pq, vector, node->pq_code);
        }
    }

    index->n_nodes++;

    /* 第一个节点作为入口点 */
    if (index->entry_point < 0) {
        index->entry_point = new_id;
        return new_id;
    }

    /* 简化实现：随机连接到几个节点 */
    int32_t max_neighbors = index->config.m;
    int32_t n_neighbors = 0;

    /* 连接到已有节点 */
    for (int32_t i = 0; i < new_id && n_neighbors < max_neighbors / 2; i++) {
        hnsw_pq_node_t *other = &index->nodes[i];

        /* 扩展对方的邻居列表 */
        if (other->n_neighbors >= max_neighbors) continue;

        if (other->neighbors == NULL) {
            other->neighbors = (int32_t *)malloc(max_neighbors * sizeof(int32_t));
        }
        if (other->neighbors) {
            other->neighbors[other->n_neighbors++] = new_id;
        }
        n_neighbors++;
    }

    /* 设置新节点的邻居 */
    if (n_neighbors > 0) {
        node->neighbors = (int32_t *)malloc(max_neighbors * sizeof(int32_t));
        if (node->neighbors) {
            node->n_neighbors = 0;
            for (int32_t i = 0; i < new_id && node->n_neighbors < max_neighbors / 2; i++) {
                node->neighbors[node->n_neighbors++] = i;
            }
        }
    }

    return new_id;
}

int32_t hnsw_pq_index_insert_batch(hnsw_pq_index_t *index, int n, const float *vectors) {
    if (!index || !vectors) return 0;

    int32_t inserted = 0;
    for (int i = 0; i < n; i++) {
        if (hnsw_pq_index_insert(index, &vectors[i * index->config.dims]) >= 0) {
            inserted++;
        }
    }
    return inserted;
}

int32_t hnsw_pq_index_search(hnsw_pq_index_t *index, const float *query,
                              int32_t k, int32_t *result_ids, float *result_dists) {
    if (!index || !query || !result_ids) return 0;
    if (index->n_nodes == 0) return 0;

    index->n_searches++;

    /* 计算查询的距离表 */
    float *distance_table = NULL;
    if (index->pq && pq_is_trained(index->pq)) {
        distance_table = (float *)malloc(pq_distance_table_size(index->pq) * sizeof(float));
        if (distance_table) {
            pq_compute_distance_table(index->pq, query, distance_table);
        }
    }

    /* 收集候选 */
    int32_t max_candidates = index->config.rerank_k;
    hnsw_pq_candidate_t *candidates = (hnsw_pq_candidate_t *)malloc(
        max_candidates * sizeof(hnsw_pq_candidate_t));
    int32_t n_candidates = 0;

    /* 简化搜索：从入口点 BFS */
    int8_t *visited = (int8_t *)calloc(index->capacity, sizeof(int8_t));
    int32_t *queue = (int32_t *)malloc(index->capacity * sizeof(int32_t));
    int32_t queue_head = 0, queue_tail = 0;

    queue[queue_tail++] = index->entry_point;
    visited[index->entry_point] = 1;

    while (queue_head < queue_tail && n_candidates < max_candidates) {
        int32_t current = queue[queue_head++];
        hnsw_pq_node_t *node = &index->nodes[current];

        /* 计算 ADC 距离 */
        float dist = INFINITY;
        if (node->pq_code && distance_table) {
            dist = compute_adc_distance(index, query, node->pq_code, distance_table);
        }

        candidates[n_candidates].id = current;
        candidates[n_candidates].distance = dist;
        n_candidates++;

        /* 扩展邻居 */
        for (int32_t i = 0; i < node->n_neighbors; i++) {
            int32_t neighbor = node->neighbors[i];
            if (!visited[neighbor]) {
                visited[neighbor] = 1;
                queue[queue_tail++] = neighbor;
            }
        }

        index->total_hops++;
    }

    /* 按 ADC 距离排序 */
    qsort(candidates, n_candidates, sizeof(hnsw_pq_candidate_t), candidate_cmp);

    /* 重排序：使用精确距离 */
    int32_t n_results = (n_candidates < k) ? n_candidates : k;

    if (index->use_heap_store && index->heap_store) {
        /* 使用原始向量重排序 */
        for (int32_t i = 0; i < n_candidates && i < max_candidates; i++) {
            float vec[index->config.dims];
            if (get_vector_from_heap(index, candidates[i].id, vec) == 0) {
                /* 精确距离计算 */
                float exact_dist = 0.0f;
                for (int d = 0; d < index->config.dims; d++) {
                    float diff = query[d] - vec[d];
                    exact_dist += diff * diff;
                }
                candidates[i].distance = exact_dist;
            }
        }
        index->total_reranks += n_candidates;

        /* 重新排序 */
        qsort(candidates, n_candidates, sizeof(hnsw_pq_candidate_t), candidate_cmp);
    }

    /* 复制结果 */
    for (int32_t i = 0; i < n_results; i++) {
        result_ids[i] = candidates[i].id;
        if (result_dists) {
            result_dists[i] = candidates[i].distance;
        }
    }

    free(visited);
    free(queue);
    free(candidates);
    free(distance_table);

    return n_results;
}

int32_t hnsw_pq_index_size(const hnsw_pq_index_t *index) {
    return index ? index->n_nodes : 0;
}

float hnsw_pq_index_avg_out_degree(const hnsw_pq_index_t *index) {
    if (!index || index->n_nodes == 0) return 0.0f;

    int64_t total = 0;
    for (int32_t i = 0; i < index->n_nodes; i++) {
        total += index->nodes[i].n_neighbors;
    }

    return (float)total / index->n_nodes;
}