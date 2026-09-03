/*
 * diskann_vamana.c
 *
 * DiskANN Vamana 图算法实现。
 *
 * Vamana 算法是 DiskANN 的核心，用于构建单层可导航小世界图。
 * 主要特点：
 * - 贪心搜索 + 剪枝
 * - Robust Prune 多轮 α 递增剪枝
 * - 支持增量插入和批量构建
 */

#include "diskann_private.h"

#include <db/index/heap/heap_vector_store.h>

/* ============================================================================
 * 常量定义
 * ============================================================================ */

/** 默认 α 参数（剪枝强度） */
#define DISKANN_DEFAULT_ALPHA 1.2f

/** α 递增因子 */
#define DISKANN_ALPHA_INCREMENT 1.2f

/** 每轮最小剪枝候选数 */
#define DISKANN_MIN_PRUNE_CANDIDATES 4

/* ============================================================================
 * 内部辅助函数
 * ============================================================================ */

/**
 * @brief 计算剪枝轮数
 *
 * 基于目标 α 计算需要执行多少轮剪枝：
 * rounds = ceil(log(alpha) / log(1.2))
 */
static int32_t diskann_compute_prune_rounds(float alpha)
{
    int32_t rounds;
    float current_alpha;

    if (alpha <= 1.0f) {
        return 1;
    }

    rounds = 0;
    current_alpha = 1.0f;
    while (current_alpha < alpha) {
        current_alpha *= DISKANN_ALPHA_INCREMENT;
        rounds++;
    }

    return rounds > 0 ? rounds : 1;
}

/**
 * @brief 计算两点间的 L2 平方距离
 */
static float diskann_l2sqr(const float *a, const float *b, int32_t dims)
{
    float dist = 0.0f;
    int32_t i;
    for (i = 0; i < dims; i++) {
        float diff = a[i] - b[i];
        dist += diff * diff;
    }
    return dist;
}

/**
 * @brief 从存储加载向量
 */
static const float *diskann_get_vector(const diskann_t *index, int32_t id)
{
    if (id < 0 || id >= index->n_total) {
        return NULL;
    }
    return &index->vectors[id * index->dims];
}

/* ============================================================================
 * Vamana 贪心搜索
 * ============================================================================ */

/**
 * @brief 贪心搜索最近邻
 *
 * 从入口点出发，沿着距离递减的方向贪心遍历图，
 * 直到找不到更近的邻居或达到最大迭代次数。
 *
 * @param[in] index 索引
 * @param[in] query 查询向量
 * @param[out] best_id 最近邻 ID
 * @param[out] best_dist 最近邻距离
 * @param[in] max_iterations 最大迭代次数
 * @return 0 成功，非 0 失败
 */
int diskann_greedy_search(const diskann_t *index,
                         const float *query,
                         int32_t *best_id,
                         float *best_dist,
                         int32_t max_iterations)
{
    int32_t current;
    int32_t iterations;
    float current_dist;
    const float *current_vec;
    const float *query_vec = query;

    if (!index || !query || !best_id || !best_dist) {
        return -1;
    }

    if (index->entry_point < 0 || index->active_count <= 0) {
        *best_id = -1;
        *best_dist = FLT_MAX;
        return 0;
    }

    /* 从入口点开始 */
    current = index->entry_point;
    current_vec = diskann_get_vector(index, current);
    if (!current_vec) {
        *best_id = -1;
        *best_dist = FLT_MAX;
        return -1;
    }
    current_dist = diskann_l2sqr(current_vec, query_vec, index->dims);
    *best_id = current;
    *best_dist = current_dist;

    /* 贪心遍历 */
    for (iterations = 0; iterations < max_iterations; iterations++) {
        int32_t *neighbors;
        int32_t neighbor_count;
        int32_t i;
        bool improved = false;

        neighbors = index->neighbors + (size_t)current * (size_t)index->index_size;
        neighbor_count = index->neighbor_counts[current];

        /* 遍历所有邻居，寻找更近的点 */
        for (i = 0; i < neighbor_count; i++) {
            int32_t neighbor_id = neighbors[i];
            if (neighbor_id < 0 || neighbor_id >= index->n_total || index->deleted[neighbor_id]) {
                continue;
            }

            current_vec = diskann_get_vector(index, neighbor_id);
            if (!current_vec) {
                continue;
            }

            {
                float dist = diskann_l2sqr(current_vec, query_vec, index->dims);
                if (dist < current_dist) {
                    current = neighbor_id;
                    current_dist = dist;
                    improved = true;
                }
            }
        }

        if (!improved) {
            break;
        }

        *best_id = current;
        *best_dist = current_dist;
    }

    return 0;
}

/* ============================================================================
 * Vamana 图构建（批量）
 * ============================================================================ */

/**
 * @brief 使用 Vamana 算法构建图
 *
 * 批量构建模式：遍历所有节点，逐个执行 link 操作。
 * 节点按随机顺序插入，保证图的连通性。
 *
 * @param[inout] index 索引
 * @return 0 成功，非 0 失败
 */
int diskann_vamana_build_graph(diskann_t *index)
{
    int32_t *active_ids;
    int32_t active_count;
    int32_t i;

    if (!index) {
        return -1;
    }

    /* 收集活跃节点 */
    if (diskann_collect_active_ids(index, &active_ids, &active_count) != 0) {
        return -1;
    }

    if (active_count <= 0) {
        free(active_ids);
        index->built = true;
        return 0;
    }

    /* 随机打乱顺序（Fisher-Yates） */
    for (i = active_count - 1; i > 0; i--) {
        int32_t j = rand() % (i + 1);
        int32_t tmp = active_ids[i];
        active_ids[i] = active_ids[j];
        active_ids[j] = tmp;
    }

    /* 选择入口点（第一个节点） */
    index->entry_point = active_ids[0];

    /* 逐个节点 link */
    for (i = 0; i < active_count; i++) {
        if (diskann_link_node(index, active_ids[i]) != 0) {
            free(active_ids);
            return -1;
        }
    }

    free(active_ids);
    index->built = true;
    return 0;
}

/**
 * @brief 设置图构建参数
 *
 * @param[inout] index 索引
 * @param[in] alpha 剪枝参数（1.0-2.0）
 * @param[in] index_size 目标邻居数
 * @param[in] build_search_list_size 构图搜索候选数
 * @return 0 成功，非 0 失败
 */
int diskann_vamana_set_params(diskann_t *index,
                              float alpha,
                              int32_t index_size,
                              int32_t build_search_list_size)
{
    if (!index) {
        return -1;
    }

    if (alpha < 1.0f || alpha > 2.0f) {
        return -1;
    }

    if (index_size <= 0 || build_search_list_size <= 0) {
        return -1;
    }

    index->alpha = alpha;
    index->index_size = index_size;
    index->build_search_list_size = build_search_list_size;

    return 0;
}

/**
 * @brief 获取图构建统计信息
 */
int diskann_vamana_get_stats(const diskann_t *index,
                             int32_t *total_nodes,
                             int32_t *active_nodes,
                             float *avg_degree,
                             int32_t *max_degree)
{
    int32_t total;
    int32_t active;
    int32_t sum_degree;
    int32_t max_deg;
    int32_t i;

    if (!index) {
        return -1;
    }

    total = index->n_total;
    active = index->active_count;
    sum_degree = 0;
    max_deg = 0;

    for (i = 0; i < index->n_total; i++) {
        if (!index->deleted[i]) {
            int32_t deg = index->neighbor_counts[i];
            sum_degree += deg;
            if (deg > max_deg) {
                max_deg = deg;
            }
        }
    }

    if (total_nodes) {
        *total_nodes = total;
    }
    if (active_nodes) {
        *active_nodes = active;
    }
    if (avg_degree) {
        *avg_degree = active > 0 ? (float)sum_degree / (float)active : 0.0f;
    }
    if (max_degree) {
        *max_degree = max_deg;
    }

    return 0;
}

/* ============================================================================
 * Vamana 增量插入
 * ============================================================================ */

/**
 * @brief 增量插入节点到图
 *
 * @param[inout] index 索引
 * @param[in] node_id 节点 ID
 * @param[in] vector 节点向量
 * @return 0 成功，非 0 失败
 */
int diskann_vamana_insert_node(diskann_t *index, int32_t node_id, const float *vector)
{
    (void)vector;

    if (!index || node_id < 0) {
        return -1;
    }

    if (node_id >= index->n_total) {
        return -1;
    }

    if (index->deleted[node_id]) {
        return -1;
    }

    /* 如果图尚未构建，只更新计数 */
    if (!index->built) {
        return 0;
    }

    /* 执行 link 操作 */
    return diskann_link_node(index, node_id);
}

/**
 * @brief 增量删除节点
 *
 * @param[inout] index 索引
 * @param[in] node_id 节点 ID
 * @return 0 成功，非 0 失败
 */
int diskann_vamana_delete_node(diskann_t *index, int32_t node_id)
{
    if (!index || node_id < 0 || node_id >= index->n_total) {
        return -1;
    }

    if (index->deleted[node_id]) {
        return 0;
    }

    /* 标记为已删除 */
    index->deleted[node_id] = 1;
    index->active_count--;

    /* 如果图尚未构建，无需修复 */
    if (!index->built) {
        return 0;
    }

    /* 收集受影响节点 */
    {
        bool *affected;
        int32_t *victim_neighbors;
        int32_t victim_count;
        int32_t *neighbors;
        int32_t i;

        affected = (bool *)calloc((size_t)index->n_total, sizeof(bool));
        if (!affected) {
            return -1;
        }

        /* 标记受害节点及其邻居 */
        neighbors = index->neighbors + (size_t)node_id * (size_t)index->index_size;
        victim_count = index->neighbor_counts[node_id];
        victim_neighbors = (int32_t *)malloc((size_t)victim_count * sizeof(int32_t));
        if (!victim_neighbors) {
            free(affected);
            return -1;
        }

        for (i = 0; i < victim_count; i++) {
            victim_neighbors[i] = neighbors[i];
            if (victim_neighbors[i] >= 0 && victim_neighbors[i] < index->n_total) {
                affected[victim_neighbors[i]] = true;
            }
        }

        /* 从受害节点邻居列表中移除被删除节点 */
        for (i = 0; i < victim_count; i++) {
            diskann_remove_id_from_row(index, victim_neighbors[i], node_id);
        }

        /* 修复受害节点 */
        diskann_repair_neighbors_after_delete(index,
                                               affected,
                                               index->n_total,
                                               victim_neighbors,
                                               victim_count);

        free(affected);
        free(victim_neighbors);
    }

    return 0;
}

/* ============================================================================
 * Vamana 搜索优化
 * ============================================================================ */

/**
 * @brief beam 搜索（多起点并行贪心搜索）
 *
 * @param[in] index 索引
 * @param[in] query 查询向量
 * @param[in] beam_width beam 宽度
 * @param[out] best_id 最佳节点 ID
 * @param[out] best_dist 最佳距离
 * @return 0 成功，非 0 失败
 */
int diskann_beam_search(const diskann_t *index,
                       const float *query,
                       int32_t beam_width,
                       int32_t *best_id,
                       float *best_dist)
{
    int32_t *candidates;
    float *distances;
    int32_t i;
    int32_t best_local_id;
    float best_local_dist;
    int32_t best_local_beam;

    if (!index || !query || beam_width <= 0) {
        return -1;
    }

    if (best_id) {
        *best_id = -1;
    }
    if (best_dist) {
        *best_dist = FLT_MAX;
    }

    if (index->active_count <= 0) {
        return 0;
    }

    candidates = (int32_t *)malloc((size_t)beam_width * sizeof(int32_t));
    distances = (float *)malloc((size_t)beam_width * sizeof(float));
    if (!candidates || !distances) {
        free(candidates);
        free(distances);
        return -1;
    }

    /* 初始化 beam：从 frozen points 选择最近的 beam_width 个 */
    {
        int32_t count = 0;
        int32_t fp = index->storage_params.frozen_point_count;

        if (fp > 0 && index->frozen_points) {
            for (i = 0; i < fp && count < beam_width; i++) {
                int32_t id = index->frozen_points[i];
                if (id >= 0 && !index->deleted[id]) {
                    const float *vec = diskann_get_vector(index, id);
                    if (vec) {
                        candidates[count] = id;
                        distances[count] = diskann_l2sqr(vec, query, index->dims);
                        count++;
                    }
                }
            }
        }

        /* 如果 frozen points 不足，从任意活跃节点补充 */
        if (count < beam_width) {
            for (i = 0; i < index->n_total && count < beam_width; i++) {
                if (!index->deleted[i]) {
                    bool is_frozen = false;
                    int32_t j;
                    for (j = 0; j < fp; j++) {
                        if (index->frozen_points && i == index->frozen_points[j]) {
                            is_frozen = true;
                            break;
                        }
                    }
                    if (!is_frozen) {
                        const float *vec = diskann_get_vector(index, i);
                        if (vec) {
                            candidates[count] = i;
                            distances[count] = diskann_l2sqr(vec, query, index->dims);
                            count++;
                        }
                    }
                }
            }
        }

        if (count == 0) {
            free(candidates);
            free(distances);
            return 0;
        }

        /* 找最近的 */
        best_local_id = candidates[0];
        best_local_dist = distances[0];
        best_local_beam = 0;

        for (i = 1; i < count; i++) {
            if (distances[i] < best_local_dist) {
                best_local_dist = distances[i];
                best_local_id = candidates[i];
                best_local_beam = i;
            }
        }
    }

    /* Beam 扩展 */
    {
        bool *visited = (bool *)calloc((size_t)index->n_total, sizeof(bool));
        if (!visited) {
            free(candidates);
            free(distances);
            return -1;
        }

        for (i = 0; i < beam_width; i++) {
            if (candidates[i] >= 0) {
                visited[candidates[i]] = true;
            }
        }

        /* 迭代扩展 beam */
        for (i = 0; i < beam_width * 2; i++) {
            int32_t cur_id = candidates[i % beam_width];
            int32_t *neighbors;
            int32_t deg;
            int32_t j;

            if (cur_id < 0 || cur_id >= index->n_total) {
                continue;
            }

            neighbors = index->neighbors + (size_t)cur_id * (size_t)index->index_size;
            deg = index->neighbor_counts[cur_id];

            for (j = 0; j < deg; j++) {
                int32_t nid = neighbors[j];
                if (nid < 0 || nid >= index->n_total || index->deleted[nid] || visited[nid]) {
                    continue;
                }

                {
                    const float *vec = diskann_get_vector(index, nid);
                    if (vec) {
                        float dist = diskann_l2sqr(vec, query, index->dims);

                        /* 检查是否应该替换 */
                        if (dist < best_local_dist) {
                            best_local_dist = dist;
                            best_local_id = nid;
                            best_local_beam = i % beam_width;
                            candidates[best_local_beam] = nid;
                            distances[best_local_beam] = dist;
                        }
                        visited[nid] = true;
                    }
                }
            }
        }

        free(visited);
    }

    if (best_id) {
        *best_id = best_local_id;
    }
    if (best_dist) {
        *best_dist = best_local_dist;
    }

    free(candidates);
    free(distances);
    return 0;
}

/**
 * @brief 重置图结构
 *
 * @param[inout] index 索引
 */
void diskann_vamana_reset_graph(diskann_t *index)
{
    if (!index) {
        return;
    }

    index->built = false;
    index->entry_point = -1;
    diskann_reset_graph(index);
}
