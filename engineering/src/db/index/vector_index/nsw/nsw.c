/**
 * @file nsw.c
 * @brief NSW（Navigable Small World）单层图索引实现
 */

#include "db/index/vector_index/nsw/nsw.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "algo-prod/distance/distance.h"
#include "db/index/heap/heap_vector_store.h"

/* 默认参数 */
#define NSW_DEFAULT_M 16
#define NSW_DEFAULT_EF_CONSTRUCTION 200
#define NSW_DEFAULT_EF_SEARCH 50
#define NSW_INITIAL_CAPACITY 1024

/* 候选节点（用于搜索队列） */
typedef struct nsw_candidate {
    int32_t id;
    float dist;
} nsw_candidate_t;

/* 比较函数：按距离升序 */
static int compare_candidates_asc(const void *a, const void *b) {
    float diff = ((nsw_candidate_t *)a)->dist - ((nsw_candidate_t *)b)->dist;
    return (diff > 0) ? 1 : ((diff < 0) ? -1 : 0);
}

/* 比较函数：按距离降序 */
static int compare_candidates_desc(const void *a, const void *b) {
    float diff = ((nsw_candidate_t *)b)->dist - ((nsw_candidate_t *)a)->dist;
    return (diff > 0) ? 1 : ((diff < 0) ? -1 : 0);
}

/* 计算两个节点之间的距离 */
static float compute_distance(const nsw_index_t *index,
                              int32_t id1,
                              int32_t id2) {
    float vec1[index->config.dims];
    float vec2[index->config.dims];

    /* 获取向量 */
    if (index->heap_store != NULL) {
        heap_vector_get(index->heap_store, &index->vector_refs[id1], vec1);
        heap_vector_get(index->heap_store, &index->vector_refs[id2], vec2);
    } else {
        memcpy(vec1, &index->vectors[id1 * index->config.dims],
               index->config.dims * sizeof(float));
        memcpy(vec2, &index->vectors[id2 * index->config.dims],
               index->config.dims * sizeof(float));
    }

    /* 计算距离 */
    switch (index->config.metric) {
        case DISTANCE_METRIC_L2_SQUARED:
            return distance_l2sqr(vec1, vec2, index->config.dims);
        case DISTANCE_METRIC_COSINE:
            /* Cosine 距离 = 1 - inner_product / (norm1 * norm2) */
            /* 简化：使用 L2_SQUARED 作为默认 */
            return distance_l2sqr(vec1, vec2, index->config.dims);
        case DISTANCE_METRIC_INNER_PRODUCT:
            return -distance_compute(index->config.metric, vec1, vec2, index->config.dims);
        default:
            return distance_l2sqr(vec1, vec2, index->config.dims);
    }
}

/* 计算查询向量与节点之间的距离 */
static float compute_distance_to_query(const nsw_index_t *index,
                                       const float *query,
                                       int32_t id) {
    float vec[index->config.dims];

    if (index->heap_store != NULL) {
        heap_vector_get(index->heap_store, &index->vector_refs[id], vec);
    } else {
        memcpy(vec, &index->vectors[id * index->config.dims],
               index->config.dims * sizeof(float));
    }

    switch (index->config.metric) {
        case DISTANCE_METRIC_L2_SQUARED:
            return distance_l2sqr(query, vec, index->config.dims);
        case DISTANCE_METRIC_COSINE:
            return distance_l2sqr(query, vec, index->config.dims);
        case DISTANCE_METRIC_INNER_PRODUCT:
            return -distance_compute(index->config.metric, query, vec, index->config.dims);
        default:
            return distance_l2sqr(query, vec, index->config.dims);
    }
}

/* 创建节点 */
static nsw_node_t *nsw_node_create(int32_t id, int32_t max_neighbors) {
    nsw_node_t *node = (nsw_node_t *)malloc(sizeof(nsw_node_t));
    if (node == NULL) return NULL;

    node->id = id;
    node->neighbors = (int32_t *)malloc(max_neighbors * sizeof(int32_t));
    if (node->neighbors == NULL) {
        free(node);
        return NULL;
    }

    node->n_neighbors = 0;
    node->max_neighbors = max_neighbors;

    return node;
}

/* 释放节点 */
static void nsw_node_destroy(nsw_node_t *node) {
    if (node != NULL) {
        free(node->neighbors);
        free(node);
    }
}

/* 添加邻居 */
static int nsw_node_add_neighbor(nsw_node_t *node, int32_t neighbor_id) {
    if (node->n_neighbors >= node->max_neighbors) {
        return -1;  /* 已达到最大邻居数 */
    }

    node->neighbors[node->n_neighbors++] = neighbor_id;
    return 0;
}

/* 贪心搜索入口点 */
static int32_t greedy_search(const nsw_index_t *index,
                             const float *query,
                             int32_t ef,
                             nsw_candidate_t *candidates,
                             int32_t *n_candidates) {
    if (index->n_nodes == 0) {
        *n_candidates = 0;
        return -1;
    }

    /* 访问标记（简化版：使用数组） */
    bool *visited = (bool *)calloc(index->n_nodes, sizeof(bool));
    if (visited == NULL) {
        *n_candidates = 0;
        return -1;
    }

    /* 候选队列 */
    nsw_candidate_t *queue = (nsw_candidate_t *)malloc(ef * sizeof(nsw_candidate_t));
    if (queue == NULL) {
        free(visited);
        *n_candidates = 0;
        return -1;
    }

    int32_t queue_size = 0;

    /* 从入口点开始 */
    int32_t current = index->entry_point;
    float current_dist = compute_distance_to_query(index, query, current);

    visited[current] = true;
    queue[queue_size].id = current;
    queue[queue_size].dist = current_dist;
    queue_size++;

    /* 贪心搜索 */
    while (true) {
        /* 找到距离最小的未探索节点 */
        int32_t best_idx = -1;
        float best_dist = INFINITY;

        for (int32_t i = 0; i < queue_size; i++) {
            if (!visited[queue[i].id]) continue;

            nsw_node_t *node = &index->nodes[queue[i].id];
            for (int32_t j = 0; j < node->n_neighbors; j++) {
                int32_t neighbor = node->neighbors[j];
                if (visited[neighbor]) continue;

                float dist = compute_distance_to_query(index, query, neighbor);
                if (dist < best_dist) {
                    best_dist = dist;
                    best_idx = neighbor;
                }
            }
        }

        if (best_idx == -1) break;  /* 所有节点都已访问 */

        /* 添加到候选队列 */
        if (queue_size < ef) {
            queue[queue_size].id = best_idx;
            queue[queue_size].dist = best_dist;
            queue_size++;
        }

        visited[best_idx] = true;
    }

    /* 复制结果 */
    memcpy(candidates, queue, queue_size * sizeof(nsw_candidate_t));
    *n_candidates = queue_size;

    /* 找到最近的节点作为返回值 */
    int32_t nearest = -1;
    float min_dist = INFINITY;
    for (int32_t i = 0; i < queue_size; i++) {
        if (candidates[i].dist < min_dist) {
            min_dist = candidates[i].dist;
            nearest = candidates[i].id;
        }
    }

    free(queue);
    free(visited);

    return nearest;
}

/* 选择邻居（简单选择最近的 M 个） */
static int select_neighbors_simple(nsw_candidate_t *candidates,
                                   int32_t n_candidates,
                                   int32_t *neighbors,
                                   int32_t max_neighbors) {
    /* 按距离排序 */
    qsort(candidates, n_candidates, sizeof(nsw_candidate_t), compare_candidates_asc);

    /* 选择前 M 个 */
    int32_t n = (n_candidates < max_neighbors) ? n_candidates : max_neighbors;
    for (int32_t i = 0; i < n; i++) {
        neighbors[i] = candidates[i].id;
    }

    return n;
}

nsw_index_t *nsw_index_create(const nsw_config_t *config) {
    if (config == NULL || config->dims <= 0 || config->M <= 0) {
        return NULL;
    }

    nsw_index_t *index = (nsw_index_t *)calloc(1, sizeof(nsw_index_t));
    if (index == NULL) return NULL;

    /* 复制配置 */
    index->config = *config;
    if (index->config.M == 0) index->config.M = NSW_DEFAULT_M;
    if (index->config.ef_construction == 0) index->config.ef_construction = NSW_DEFAULT_EF_CONSTRUCTION;
    if (index->config.ef_search == 0) index->config.ef_search = NSW_DEFAULT_EF_SEARCH;

    /* 分配节点数组 */
    index->max_nodes = NSW_INITIAL_CAPACITY;
    index->nodes = (nsw_node_t *)malloc(index->max_nodes * sizeof(nsw_node_t));
    if (index->nodes == NULL) {
        free(index);
        return NULL;
    }
    memset(index->nodes, 0, index->max_nodes * sizeof(nsw_node_t));

    /* 分配向量存储（兼容模式） */
    index->vectors = (float *)malloc(index->max_nodes * index->config.dims * sizeof(float));
    if (index->vectors == NULL) {
        free(index->nodes);
        free(index);
        return NULL;
    }

    index->n_nodes = 0;
    index->entry_point = -1;
    index->heap_store = NULL;
    index->vector_refs = NULL;
    index->vector_refs_size = 0;

    return index;
}

void nsw_index_destroy(nsw_index_t *index) {
    if (index == NULL) return;

    for (int32_t i = 0; i < index->n_nodes; i++) {
        free(index->nodes[i].neighbors);
    }

    free(index->nodes);
    free(index->vectors);
    free(index->vector_refs);
    free(index);
}

int32_t nsw_index_insert(nsw_index_t *index, const float *vector) {
    if (index == NULL || vector == NULL) {
        return -1;
    }

    /* 扩容检查 */
    if (index->n_nodes >= index->max_nodes) {
        int32_t new_capacity = index->max_nodes * 2;
        nsw_node_t *new_nodes = (nsw_node_t *)realloc(index->nodes,
                                                       new_capacity * sizeof(nsw_node_t));
        if (new_nodes == NULL) return -1;

        index->nodes = new_nodes;
        index->max_nodes = new_capacity;

        /* 扩展向量存储 */
        float *new_vectors = (float *)realloc(index->vectors,
                                               new_capacity * index->config.dims * sizeof(float));
        if (new_vectors == NULL) return -1;
        index->vectors = new_vectors;
    }

    int32_t new_id = index->n_nodes;

    /* 存储向量 */
    if (index->heap_store != NULL) {
        /* Heap 模式：写入 Heap 并记录引用 */
        vector_ref_t ref = heap_vector_insert(index->heap_store, vector);
        if (index->vector_refs_size <= new_id) {
            uint32_t new_size = new_id + 1;
            vector_ref_t *new_refs = (vector_ref_t *)realloc(index->vector_refs,
                                                              new_size * sizeof(vector_ref_t));
            if (new_refs == NULL) return -1;
            index->vector_refs = new_refs;
            index->vector_refs_size = new_size;
        }
        index->vector_refs[new_id] = ref;
    } else {
        /* 兼容模式：直接存入 vectors 数组 */
        memcpy(&index->vectors[new_id * index->config.dims], vector,
               index->config.dims * sizeof(float));
    }

    /* 创建节点 */
    index->nodes[new_id].id = new_id;
    index->nodes[new_id].neighbors = (int32_t *)malloc(index->config.M * sizeof(int32_t));
    if (index->nodes[new_id].neighbors == NULL) return -1;
    index->nodes[new_id].n_neighbors = 0;
    index->nodes[new_id].max_neighbors = index->config.M;

    index->n_nodes++;

    /* 第一个节点作为入口点 */
    if (index->entry_point == -1) {
        index->entry_point = new_id;
        return new_id;
    }

    /* 搜索最近邻 */
    nsw_candidate_t *candidates = (nsw_candidate_t *)malloc(
        index->config.ef_construction * sizeof(nsw_candidate_t));
    if (candidates == NULL) return -1;

    int32_t n_candidates;
    greedy_search(index, vector, index->config.ef_construction,
                  candidates, &n_candidates);

    /* 选择邻居 */
    int32_t neighbors[index->config.M];
    int32_t n_neighbors = select_neighbors_simple(candidates, n_candidates,
                                                   neighbors, index->config.M);

    /* 建立双向连接 */
    for (int32_t i = 0; i < n_neighbors; i++) {
        int32_t neighbor_id = neighbors[i];

        /* 新节点添加邻居 */
        index->nodes[new_id].neighbors[index->nodes[new_id].n_neighbors++] = neighbor_id;

        /* 邻居添加新节点 */
        nsw_node_t *neighbor = &index->nodes[neighbor_id];
        if (neighbor->n_neighbors < neighbor->max_neighbors) {
            neighbor->neighbors[neighbor->n_neighbors++] = new_id;
        } else {
            /* 简化处理：替换最远的邻居 */
            int32_t farthest_idx = 0;
            float farthest_dist = compute_distance(index, neighbor_id,
                                                   neighbor->neighbors[0]);

            for (int32_t j = 1; j < neighbor->n_neighbors; j++) {
                float dist = compute_distance(index, neighbor_id,
                                              neighbor->neighbors[j]);
                if (dist > farthest_dist) {
                    farthest_dist = dist;
                    farthest_idx = j;
                }
            }

            float new_dist = compute_distance_to_query(index, vector, neighbor_id);
            if (new_dist < farthest_dist) {
                neighbor->neighbors[farthest_idx] = new_id;
            }
        }
    }

    free(candidates);

    return new_id;
}

int32_t nsw_index_insert_batch(nsw_index_t *index, int32_t n, const float *vectors) {
    if (index == NULL || vectors == NULL || n <= 0) {
        return -1;
    }

    int32_t inserted = 0;
    for (int32_t i = 0; i < n; i++) {
        if (nsw_index_insert(index, &vectors[i * index->config.dims]) >= 0) {
            inserted++;
        }
    }

    return inserted;
}

int32_t nsw_index_search(const nsw_index_t *index,
                         const float *query,
                         int32_t k,
                         int32_t *result_ids,
                         float *result_dists) {
    if (index == NULL || query == NULL || k <= 0 ||
        result_ids == NULL || result_dists == NULL) {
        return -1;
    }

    if (index->n_nodes == 0) {
        return 0;
    }

    /* 搜索候选 */
    nsw_candidate_t *candidates = (nsw_candidate_t *)malloc(
        index->config.ef_search * sizeof(nsw_candidate_t));
    if (candidates == NULL) return -1;

    int32_t n_candidates;
    greedy_search(index, query, index->config.ef_search,
                  candidates, &n_candidates);

    /* 按距离排序 */
    qsort(candidates, n_candidates, sizeof(nsw_candidate_t), compare_candidates_asc);

    /* 返回前 k 个 */
    int32_t n_results = (n_candidates < k) ? n_candidates : k;
    for (int32_t i = 0; i < n_results; i++) {
        result_ids[i] = candidates[i].id;
        result_dists[i] = candidates[i].dist;
    }

    free(candidates);

    return n_results;
}

int32_t nsw_index_set_heap_store(nsw_index_t *index,
                                  heap_vector_store_t *heap_store) {
    if (index == NULL || heap_store == NULL) {
        return -1;
    }

    index->heap_store = heap_store;
    return 0;
}

int32_t nsw_index_size(const nsw_index_t *index) {
    return (index != NULL) ? index->n_nodes : 0;
}