/*
 * HNSW 图边修复实现
 */

#include <stdlib.h>
#include <string.h>

#include <db/index/vector_index/hnsw/faiss_hnsw.h>
#include <db/index/vector_index/hnsw/faiss_hnsw_utils.h>
#include <db/index/vector_index/delete/graph_repair.h>

void graph_repair_config_init_default(graph_repair_config_t *config)
{
    if (!config) return;

    config->max_repair_neighbors = 16;
    config->ef_search = 64;
    config->update_entry_point = true;
}

/*
 * 内部辅助函数：检查一个 ID 是否在数组中
 */
static bool contains_id(const int32_t *ids, int32_t n, int32_t id)
{
    for (int32_t i = 0; i < n; i++) {
        if (ids[i] == id) {
            return true;
        }
    }
    return false;
}

/*
 * 内部辅助函数：移除邻居列表中的指定 ID
 * 注意：这个实现是简化版，实际的 HNSW 邻居列表是扁平数组
 */
static int32_t remove_neighbor_from_list(faiss_hnsw_t *index,
                                        int32_t node_id,
                                        int32_t removed_id,
                                        int32_t level)
{
    /* 获取邻居范围 */
    size_t begin, end;
    faiss_neighbor_range(index, node_id, level, &begin, &end);

    /* 查找并移除 */
    for (size_t i = begin; i < end; i++) {
        if (index->nbs[i] == removed_id) {
            /* 将后续元素前移 */
            for (size_t j = i; j < end - 1; j++) {
                index->nbs[j] = index->nbs[j + 1];
            }
            return 0;
        }
    }

    return -1;  /* 未找到 */
}

/*
 * 查找最近的非删除邻居
 */
static int32_t find_nearest_valid_neighbor(faiss_hnsw_t *index,
                                           const int32_t *deleted_ids,
                                           int32_t n_deleted,
                                           const float *query_vec,
                                           int32_t exclude_id)
{
    float best_dist = -1.0f;
    int32_t best_id = -1;

    for (int32_t i = 0; i < index->n_total; i++) {
        /* 跳过已删除或排除的节点 */
        if (i == exclude_id || contains_id(deleted_ids, n_deleted, i)) {
            continue;
        }

        float dist = distance_compute_from_query(
            index->metric, query_vec, index->vectors, index->dims, i);

        if (best_id < 0 || dist < best_dist) {
            best_dist = dist;
            best_id = i;
        }
    }

    return best_id;
}

int32_t graph_repair_deleted_neighbors(void *hnsw_index,
                                       const int32_t *deleted_ids,
                                       int32_t n_deleted,
                                       const graph_repair_config_t *config,
                                       graph_repair_stats_t *stats)
{
    if (!hnsw_index || !deleted_ids || n_deleted <= 0) {
        return -1;
    }

    faiss_hnsw_t *index = (faiss_hnsw_t *)hnsw_index;
    graph_repair_config_t cfg;
    if (config) {
        cfg = *config;
    } else {
        graph_repair_config_init_default(&cfg);
    }

    int32_t repaired = 0;

    /* 遍历所有节点，移除指向已删除节点的边 */
    for (int32_t node_id = 0; node_id < index->n_total; node_id++) {
        /* 跳过已删除的节点 */
        if (contains_id(deleted_ids, n_deleted, node_id)) {
            continue;
        }

        /* 遍历所有层 */
        for (int32_t level = 0; level <= index->max_level; level++) {
            /* 获取邻居范围 */
            size_t begin, end;
            faiss_neighbor_range(index, node_id, level, &begin, &end);

            /* 检查每个邻居 */
            for (size_t i = begin; i < end; i++) {
                int32_t neighbor = index->nbs[i];

                if (contains_id(deleted_ids, n_deleted, neighbor)) {
                    /* 标记为无效（用 -1 表示） */
                    index->nbs[i] = -1;
                    repaired++;
                }
            }
        }
    }

    /* 压缩邻居列表（移除 -1） */
    for (int32_t node_id = 0; node_id < index->n_total; node_id++) {
        if (contains_id(deleted_ids, n_deleted, node_id)) {
            continue;
        }

        for (int32_t level = 0; level <= index->max_level; level++) {
            size_t begin, end;
            faiss_neighbor_range(index, node_id, level, &begin, &end);

            /* 压缩：将非 -1 的元素前移 */
            size_t write_idx = begin;
            for (size_t read_idx = begin; read_idx < end; read_idx++) {
                if (index->nbs[read_idx] != -1) {
                    if (write_idx != read_idx) {
                        index->nbs[write_idx] = index->nbs[read_idx];
                    }
                    write_idx++;
                }
            }

            /* 清除尾部的旧数据 */
            for (size_t i = write_idx; i < end; i++) {
                index->nbs[i] = -1;
            }
        }
    }

    /* 更新统计 */
    if (stats) {
        stats->deleted_count = n_deleted;
        stats->repaired_count = repaired;
        stats->orphan_count = 0;
    }

    return 0;
}

int32_t graph_repair_orphans(void *hnsw_index)
{
    if (!hnsw_index) {
        return -1;
    }

    faiss_hnsw_t *index = (faiss_hnsw_t *)hnsw_index;
    int32_t orphans = 0;

    /* 检查每个节点是否可被访问 */
    for (int32_t node_id = 0; node_id < index->n_total; node_id++) {
        /* 入口点永远可达 */
        if (node_id == index->entry_pointer) {
            continue;
        }

        /* 检查是否有邻居 */
        bool has_neighbor = false;
        for (int32_t level = 0; level <= index->max_level && !has_neighbor; level++) {
            size_t begin, end;
            faiss_neighbor_range(index, node_id, level, &begin, &end);

            for (size_t i = begin; i < end; i++) {
                if (index->nbs[i] >= 0 && index->nbs[i] != node_id) {
                    has_neighbor = true;
                    break;
                }
            }
        }

        /* 如果没有邻居，从入口点出发尝试可达 */
        if (!has_neighbor) {
            orphans++;
        }
    }

    return orphans;
}

int32_t graph_repair_update_entry_point(void *hnsw_index)
{
    if (!hnsw_index) {
        return -1;
    }

    faiss_hnsw_t *index = (faiss_hnsw_t *)hnsw_index;

    /* 检查入口点是否有效 */
    if (index->entry_pointer >= 0 && index->entry_pointer < index->n_total) {
        return 0;  /* 入口点有效 */
    }

    /* 入口点无效，需要重新选择 */
    /* 策略：选择 ID 最小的有效节点作为入口点 */
    for (int32_t i = 0; i < index->n_total; i++) {
        index->entry_pointer = i;
        index->max_level = 0;
        return 1;  /* 已更新 */
    }

    return -1;  /* 没有有效节点 */
}
