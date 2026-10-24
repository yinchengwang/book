/**
 * @file ssg.c
 * @brief SSG 索引实现
 *
 * 实现 Synthesized Sparse Graph（合成稀疏图），核心算法：
 * - Robust Prune：遮挡剪枝策略，选择最优邻居子集
 * - 贪心搜索：从入口点导航到目标区域
 */

#include "db/index/vector_index/ssg/ssg.h"
#include "db/index/heap/heap_vector_store.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ========== 内部结构 ========== */

/* 候选点（用于搜索和剪枝） */
typedef struct {
    int32_t id;
    float distance;
} ssg_candidate_t;

/* ========== 辅助函数 ========== */

/* 比较函数：按距离升序 */
static int candidate_cmp(const void *a, const void *b) {
    const ssg_candidate_t *ca = (const ssg_candidate_t *)a;
    const ssg_candidate_t *cb = (const ssg_candidate_t *)b;
    if (ca->distance < cb->distance) return -1;
    if (ca->distance > cb->distance) return 1;
    return 0;
}

/* 计算两向量距离 */
static float compute_distance(ssg_index_t *index, const float *vec1, const float *vec2) {
    if (index->use_heap_store) {
        return distance_l2sqr(vec1, vec2, index->config.dims);
    }
    return distance_compute(index->config.metric, vec1, vec2, index->config.dims);
}

/* 计算查询到指定节点的距离 */
static float compute_distance_to_query(ssg_index_t *index, const float *query, int32_t node_id) {
    if (index->use_heap_store && index->heap_store) {
        vector_ref_t ref = {node_id};
        float vec[index->config.dims];
        if (heap_vector_get(index->heap_store, &ref, vec) == 0) {
            return compute_distance(index, query, vec);
        }
    }
    return INFINITY;
}

/* 获取节点向量（需要调用方提供缓冲区） */
static int get_node_vector(ssg_index_t *index, int32_t node_id, float *out_vec) {
    if (index->use_heap_store && index->heap_store) {
        /* node_id 对应 heap_store 中的第 node_id 个向量 */
        /* 需要通过 heap_vector_count 推断 ref */
        /* 简化实现：假设 node_id 就是 heap 中的顺序索引 */
        /* 实际应该维护 node_id -> vector_ref 的映射 */

        /* 临时方案：直接用 node_id 构造 ref（假设顺序插入） */
        int32_t page_idx = node_id / index->vectors_per_page_hint;
        int32_t slot_idx = node_id % index->vectors_per_page_hint;

        vector_ref_t ref;
        ref.heap_page_id = page_idx;
        ref.offset = HEAP_VECTOR_PAGE_HEADER_SIZE + slot_idx * index->config.dims * sizeof(float);
        ref.length = index->config.dims * sizeof(float);

        return heap_vector_get(index->heap_store, &ref, out_vec);
    }
    return -1;
}

/* ========== Robust Prune 算法 ========== */

/**
 * @brief Robust Prune 剪枝
 *
 * 从候选池 C 中选择最多 R 个邻居，使得从节点 v 出发能快速到达 C 中的每个点。
 * 遮挡原理：如果 dist(v, p*) ≤ α × dist(p*, p)，则 p 被 p* "遮挡"。
 */
static void robust_prune(ssg_index_t *index, ssg_node_t *node,
                         ssg_candidate_t *candidates, int32_t n_candidates) {
    if (n_candidates == 0) return;

    /* 按距离排序 */
    qsort(candidates, n_candidates, sizeof(ssg_candidate_t), candidate_cmp);

    int32_t R = index->config.R;
    float alpha = index->config.alpha;

    /* 初始化结果集 */
    int32_t *selected = (int32_t *)malloc(n_candidates * sizeof(int32_t));
    int32_t n_selected = 0;

    /* 标记是否被遮挡 */
    int8_t *occluded = (int8_t *)calloc(n_candidates, sizeof(int8_t));

    /* 获取节点 v 的向量 */
    float v_vec[index->config.dims];
    if (get_node_vector(index, node->id, v_vec) != 0) {
        free(selected);
        free(occluded);
        return;
    }

    /* 遍历候选 */
    for (int32_t i = 0; i < n_candidates && n_selected < R; i++) {
        if (occluded[i]) continue;

        /* 选择最近的未被遮挡点 */
        int32_t p_star = candidates[i].id;
        selected[n_selected++] = p_star;

        /* 标记被 p_star 遮挡的点 */
        float p_star_vec[index->config.dims];
        if (get_node_vector(index, p_star, p_star_vec) != 0) continue;

        for (int32_t j = i + 1; j < n_candidates; j++) {
            if (occluded[j]) continue;

            float p_vec[index->config.dims];
            if (get_node_vector(index, candidates[j].id, p_vec) != 0) continue;

            float dist_pstar_p = compute_distance(index, p_star_vec, p_vec);
            float dist_v_p = candidates[j].distance;

            if (alpha * dist_pstar_p <= dist_v_p) {
                occluded[j] = 1;  /* p 被 p_star 遮挡 */
            }
        }
    }

    /* 更新邻居列表 */
    if (node->neighbors) {
        free(node->neighbors);
    }
    node->neighbors = (int32_t *)malloc(n_selected * sizeof(int32_t));
    memcpy(node->neighbors, selected, n_selected * sizeof(int32_t));
    node->n_neighbors = n_selected;

    free(selected);
    free(occluded);
}

/* ========== 贪心搜索 ========== */

/**
 * @brief 贪心搜索
 *
 * 从入口点出发，每步跳到距离查询最近的邻居，直到无法再接近为止。
 */
static int32_t greedy_search(ssg_index_t *index, const float *query,
                              ssg_candidate_t *results, int32_t max_results) {
    if (index->n_nodes == 0) return 0;

    int32_t L = index->config.max_search_L;
    int8_t *visited = (int8_t *)calloc(index->capacity, sizeof(int8_t));

    /* 候选集（小顶堆模拟） */
    ssg_candidate_t *candidates = (ssg_candidate_t *)malloc(L * sizeof(ssg_candidate_t));
    int32_t n_candidates = 0;

    /* 结果集 */
    ssg_candidate_t *top_k = (ssg_candidate_t *)malloc(L * sizeof(ssg_candidate_t));
    int32_t n_top_k = 0;

    /* 从入口点开始 */
    int32_t entry = index->entry_point;
    float entry_dist = compute_distance_to_query(index, query, entry);

    candidates[n_candidates].id = entry;
    candidates[n_candidates].distance = entry_dist;
    n_candidates++;
    visited[entry] = 1;

    int32_t total_hops = 0;

    while (n_candidates > 0) {
        /* 取最近的候选 */
        qsort(candidates, n_candidates, sizeof(ssg_candidate_t), candidate_cmp);
        ssg_candidate_t nearest = candidates[0];

        /* 移动到结果集 */
        if (n_top_k < L) {
            top_k[n_top_k++] = nearest;
        }

        /* 从候选集移除 */
        n_candidates--;
        if (n_candidates > 0) {
            memmove(candidates, candidates + 1, n_candidates * sizeof(ssg_candidate_t));
        }

        total_hops++;

        /* 扩展邻居 */
        ssg_node_t *node = &index->nodes[nearest.id];
        for (int32_t i = 0; i < node->n_neighbors; i++) {
            int32_t neighbor_id = node->neighbors[i];
            if (visited[neighbor_id]) continue;

            float dist = compute_distance_to_query(index, query, neighbor_id);
            visited[neighbor_id] = 1;

            if (n_candidates < L) {
                candidates[n_candidates].id = neighbor_id;
                candidates[n_candidates].distance = dist;
                n_candidates++;
            }
        }
    }

    /* 排序结果 */
    qsort(top_k, n_top_k, sizeof(ssg_candidate_t), candidate_cmp);

    /* 复制到输出 */
    int32_t n_results = (n_top_k < max_results) ? n_top_k : max_results;
    memcpy(results, top_k, n_results * sizeof(ssg_candidate_t));

    index->total_hops += total_hops;

    free(visited);
    free(candidates);
    free(top_k);

    return n_results;
}

/* ========== 公开 API ========== */

ssg_index_t *ssg_index_create(const ssg_config_t *config) {
    ssg_index_t *index = (ssg_index_t *)calloc(1, sizeof(ssg_index_t));
    if (!index) return NULL;

    memcpy(&index->config, config, sizeof(ssg_config_t));

    /* 默认参数 */
    if (index->config.max_search_L == 0) {
        index->config.max_search_L = index->config.L;
    }
    if (index->config.alpha < 1.0f) {
        index->config.alpha = 1.2f;  /* 默认遮挡参数 */
    }

    /* 分配节点数组 */
    index->capacity = 1024;
    index->nodes = (ssg_node_t *)calloc(index->capacity, sizeof(ssg_node_t));
    index->entry_point = -1;

    return index;
}

void ssg_index_destroy(ssg_index_t *index) {
    if (!index) return;

    if (index->nodes) {
        for (int32_t i = 0; i < index->n_nodes; i++) {
            if (index->nodes[i].neighbors) {
                free(index->nodes[i].neighbors);
            }
        }
        free(index->nodes);
    }

    free(index);
}

void ssg_index_set_heap_store(ssg_index_t *index, heap_vector_store_t *store) {
    index->heap_store = store;
    index->use_heap_store = (store != NULL);

    /* 计算每页向量数提示（使用保守估计） */
    if (store && index->config.dims > 0) {
        size_t vector_bytes = index->config.dims * sizeof(float);
        /* 假设默认页面大小 8KB */
        size_t page_size = 8192;
        index->vectors_per_page_hint = (int)((page_size - HEAP_VECTOR_PAGE_HEADER_SIZE) / vector_bytes);
        if (index->vectors_per_page_hint <= 0) {
            index->vectors_per_page_hint = 1;
        }
    }
}

int32_t ssg_index_insert(ssg_index_t *index, const float *vector) {
    if (!index || !vector) return -1;

    /* 检查容量 */
    if (index->n_nodes >= index->capacity) {
        int32_t new_capacity = index->capacity * 2;
        ssg_node_t *new_nodes = (ssg_node_t *)realloc(index->nodes,
                                                        new_capacity * sizeof(ssg_node_t));
        if (!new_nodes) return -1;
        memset(new_nodes + index->capacity, 0,
               (new_capacity - index->capacity) * sizeof(ssg_node_t));
        index->nodes = new_nodes;
        index->capacity = new_capacity;
    }

    int32_t new_id = index->n_nodes;

    /* 插入向量到 heap_store */
    if (index->use_heap_store && index->heap_store) {
        vector_ref_t ref = heap_vector_insert(index->heap_store, vector);
        (void)ref;  /* SSG 使用 node id 作为向量标识 */
    }

    /* 创建新节点 */
    ssg_node_t *node = &index->nodes[new_id];
    node->id = new_id;
    node->neighbors = NULL;
    node->n_neighbors = 0;
    node->capacity = 0;

    index->n_nodes++;

    /* 第一个节点作为入口点 */
    if (index->entry_point < 0) {
        index->entry_point = new_id;
        return new_id;
    }

    /* 搜索候选邻居 */
    ssg_candidate_t candidates[256];
    int32_t n_candidates = greedy_search(index, vector, candidates,
                                          index->config.L);

    /* Robust Prune 剪枝 */
    robust_prune(index, node, candidates, n_candidates);

    /* 反向连接：添加新节点到候选邻居的邻居列表 */
    for (int32_t i = 0; i < node->n_neighbors && i < index->config.R; i++) {
        int32_t neighbor_id = node->neighbors[i];
        ssg_node_t *neighbor = &index->nodes[neighbor_id];

        /* 扩展邻居列表 */
        if (neighbor->n_neighbors >= neighbor->capacity) {
            int32_t new_cap = neighbor->capacity == 0 ? index->config.R : neighbor->capacity * 2;
            int32_t *new_neighbors = (int32_t *)realloc(neighbor->neighbors,
                                                         new_cap * sizeof(int32_t));
            if (!new_neighbors) continue;
            neighbor->neighbors = new_neighbors;
            neighbor->capacity = new_cap;
        }

        /* 添加双向连接 */
        neighbor->neighbors[neighbor->n_neighbors++] = new_id;
    }

    return new_id;
}

int32_t ssg_index_insert_batch(ssg_index_t *index, int32_t n_vectors,
                                const float *vectors) {
    if (!index || !vectors) return 0;

    int32_t inserted = 0;
    for (int32_t i = 0; i < n_vectors; i++) {
        if (ssg_index_insert(index, &vectors[i * index->config.dims]) >= 0) {
            inserted++;
        }
    }
    return inserted;
}

int32_t ssg_index_search(ssg_index_t *index, const float *query, int32_t k,
                         int32_t *result_ids, float *result_dists) {
    if (!index || !query || !result_ids) return 0;
    if (index->n_nodes == 0) return 0;

    index->n_searches++;

    ssg_candidate_t results[256];
    int32_t n_results = greedy_search(index, query, results, k);

    /* 复制结果 */
    for (int32_t i = 0; i < n_results; i++) {
        result_ids[i] = results[i].id;
        if (result_dists) {
            result_dists[i] = results[i].distance;
        }
    }

    return n_results;
}

int32_t ssg_index_size(const ssg_index_t *index) {
    return index ? index->n_nodes : 0;
}

float ssg_index_avg_out_degree(const ssg_index_t *index) {
    if (!index || index->n_nodes == 0) return 0.0f;

    int64_t total_neighbors = 0;
    for (int32_t i = 0; i < index->n_nodes; i++) {
        total_neighbors += index->nodes[i].n_neighbors;
    }

    return (float)total_neighbors / index->n_nodes;
}
