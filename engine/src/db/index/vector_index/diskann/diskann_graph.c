/* diskann graph helpers */

#include "diskann_private.h"

/* basic graph row helpers */

int diskann_has_neighbor(const diskann_t *index, int32_t node_id, int32_t neighbor_id)
{
    int32_t i;
    int32_t *row;

    if (node_id < 0 || node_id >= index->n_total) {
        return 0;
    }

    row = index->neighbors + (size_t)node_id * (size_t)index->index_size;
    for (i = 0; i < index->neighbor_counts[node_id]; ++i) {
        if (row[i] == neighbor_id) {
            return 1;
        }
    }

    return 0;
}

int diskann_append_unique_candidate(diskann_scored_t *pool,
                                          int32_t *count,
                                          int32_t capacity,
                                          int32_t id,
                                          float distance)
{
    int32_t i;

    if (!pool || !count || id < 0 || *count > capacity) {
        return -1;
    }

    for (i = 0; i < *count; ++i) {
        if (pool[i].id == id) {
            if (distance < pool[i].distance) {
                pool[i].distance = distance;
            }
            return 0;
        }
    }

    if (*count >= capacity) {
        return -1;
    }

    pool[*count].id = id;
    pool[*count].distance = distance;
    pool[*count].expanded = false;
    *count += 1;
    return 0;
}

/* prune and edge maintenance */

int diskann_robust_prune(const diskann_t *index,
                               int32_t node_id,
                               diskann_scored_t *pool,
                               int32_t pool_count,
                               int32_t *result_ids,
                               int32_t *result_count,
                               int32_t occlude_rounds)
{
    bool *occlude_factor;
    bool *selected;
    int32_t chosen;
    int32_t rounds;
    int32_t round;
    int32_t i;

    if (!index || !pool || !result_ids || !result_count) {
        return -1;
    }

    *result_count = 0;
    if (pool_count <= 0) {
        return 0;
    }

    /* 先按”离 node_id 的距离”从近到远排序候选池。 */
    qsort(pool, (size_t)pool_count, sizeof(diskann_scored_t), diskann_compare_scored);

    /* 计算多轮轮数：0 表示自动计算 */
    if (occlude_rounds > 0) {
        rounds = occlude_rounds;
    } else {
        float target = diskann_max_i32(1, (int32_t)index->alpha) > 0 ? index->alpha : 1.2f;
        rounds = (int32_t)ceil(logf(target) / logf(1.2f));
        if (rounds < 1) {
            rounds = 1;
        }
    }

    occlude_factor = (bool *)calloc((size_t)pool_count, sizeof(bool));
    selected = (bool *)calloc((size_t)pool_count, sizeof(bool));
    if (!occlude_factor || !selected) {
        free(occlude_factor);
        free(selected);
        return -1;
    }

    chosen = 0;
    for (round = 0; round < rounds && chosen < index->index_size; ++round) {
        float cur_alpha;
        int32_t j;

        /* 递增 alpha：从 1.0 开始，每轮 ×1.2，最后一轮 clamp 到目标值 */
        cur_alpha = 1.0f;
        for (j = 0; j < round; ++j) {
            cur_alpha *= 1.2f;
        }
        if (round == rounds - 1) {
            cur_alpha = index->alpha;
        }

        memset(occlude_factor, 0, (size_t)pool_count * sizeof(bool));

        for (i = 0; i < pool_count && chosen < index->index_size; ++i) {
            if (occlude_factor[i] || pool[i].id == node_id) {
                continue;
            }

            if (!selected[i]) {
                result_ids[chosen++] = pool[i].id;
                selected[i] = true;
            }

            for (j = i + 1; j < pool_count; ++j) {
                float between;

                if (occlude_factor[j] || pool[j].id == node_id) {
                    continue;
                }

                between = diskann_fast_l2_distance(index, pool[i].id, pool[j].id);
                if (cur_alpha * between <= pool[j].distance) {
                    occlude_factor[j] = true;
                }
            }
        }
    }

    /* 如果剪枝后邻居还不够，再按距离顺序补齐，避免图过稀。 */
    if (chosen < index->index_size) {
        for (i = 0; i < pool_count && chosen < index->index_size; ++i) {
            int32_t k;
            bool duplicate = false;

            if (pool[i].id == node_id) {
                continue;
            }

            for (k = 0; k < chosen; ++k) {
                if (result_ids[k] == pool[i].id) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate) {
                result_ids[chosen++] = pool[i].id;
            }
        }
    }

    *result_count = chosen;
    free(occlude_factor);
    free(selected);
    return 0;
}

void diskann_set_neighbors_for_node(diskann_t *index, int32_t node_id, const int32_t *neighbors, int32_t count)
{
    int32_t *row;

    row = index->neighbors + (size_t)node_id * (size_t)index->index_size;
    memset(row, -1, (size_t)index->index_size * sizeof(int32_t));
    if (count > 0) {
        memcpy(row, neighbors, (size_t)count * sizeof(int32_t));
    }
    index->neighbor_counts[node_id] = count;
}

int diskann_add_reverse_edge(diskann_t *index, int32_t from_id, int32_t to_id)
{
    diskann_scored_t *pool;
    int32_t *result_ids;
    int32_t *row;
    int32_t pool_count;
    int32_t result_count;
    int32_t i;

    /* 反向边维护保证图近似双向，有利于后续搜索连通性。 */
    if (!index || index->deleted[to_id] || diskann_has_neighbor(index, to_id, from_id)) {
        return 0;
    }

    row = index->neighbors + (size_t)to_id * (size_t)index->index_size;
    if (index->neighbor_counts[to_id] < index->index_size) {
        row[index->neighbor_counts[to_id]++] = from_id;
        return 0;
    }

    pool = (diskann_scored_t *)malloc((size_t)(index->index_size + 1) * sizeof(diskann_scored_t));
    result_ids = (int32_t *)malloc((size_t)index->index_size * sizeof(int32_t));
    if (!pool || !result_ids) {
        free(pool);
        free(result_ids);
        return -1;
    }

    /* 目标节点已满时，把“旧邻居 + 新邻居”一起重新做一次 robust prune。 */
    pool_count = 0;
    for (i = 0; i < index->neighbor_counts[to_id]; ++i) {
        if (row[i] >= 0 && !index->deleted[row[i]]) {
            diskann_append_unique_candidate(pool,
                                                  &pool_count,
                                                  index->index_size + 1,
                                                  row[i],
                                                  diskann_fast_l2_distance(index, to_id, row[i]));
        }
    }
    diskann_append_unique_candidate(pool,
                                          &pool_count,
                                          index->index_size + 1,
                                          from_id,
                                          diskann_fast_l2_distance(index, to_id, from_id));

    if (diskann_robust_prune(index, to_id, pool, pool_count, result_ids, &result_count, 0) != 0) {
        free(pool);
        free(result_ids);
        return -1;
    }

    diskann_set_neighbors_for_node(index, to_id, result_ids, result_count);
    free(pool);
    free(result_ids);
    return 0;
}

/* delete repair helpers */

int diskann_build_repair_candidate_pool(const diskann_t *index,
                                              int32_t node_id,
                                              const int32_t *victim_neighbors,
                                              int32_t victim_neighbor_count,
                                              diskann_scored_t *pool,
                                              int32_t pool_capacity,
                                              int32_t *pool_count)
{
    int32_t *row;
    int32_t degree;
    int32_t i;
    int32_t j;

    if (!index || !pool || !pool_count || node_id < 0 || node_id >= index->n_total) {
        return -1;
    }

    *pool_count = 0;
    row = index->neighbors + (size_t)node_id * (size_t)index->index_size;
    degree = index->neighbor_counts[node_id];

    /* 删除修复时，候选池来自三部分: 自己旧邻居、受害节点邻居、二跳邻居。 */
    for (i = 0; i < degree; ++i) {
        int32_t candidate = row[i];
        if (candidate >= 0 && !index->deleted[candidate]) {
            diskann_append_unique_candidate(pool,
                                                  pool_count,
                                                  pool_capacity,
                                                  candidate,
                                                  diskann_fast_l2_distance(index, node_id, candidate));
        }
    }

    for (i = 0; i < victim_neighbor_count; ++i) {
        int32_t candidate = victim_neighbors[i];
        if (candidate >= 0 && candidate != node_id && !index->deleted[candidate]) {
            diskann_append_unique_candidate(pool,
                                                  pool_count,
                                                  pool_capacity,
                                                  candidate,
                                                  diskann_fast_l2_distance(index, node_id, candidate));
        }
    }

    for (i = 0; i < degree; ++i) {
        int32_t neighbor = row[i];
        int32_t *neighbor_row;
        int32_t neighbor_degree;

        if (neighbor < 0 || index->deleted[neighbor]) {
            continue;
        }
        neighbor_row = index->neighbors + (size_t)neighbor * (size_t)index->index_size;
        neighbor_degree = index->neighbor_counts[neighbor];
        for (j = 0; j < neighbor_degree; ++j) {
            int32_t candidate = neighbor_row[j];
            if (candidate >= 0 && candidate != node_id && !index->deleted[candidate]) {
                diskann_append_unique_candidate(pool,
                                                      pool_count,
                                                      pool_capacity,
                                                      candidate,
                                                      diskann_fast_l2_distance(index, node_id, candidate));
            }
        }
    }

    for (i = 0; i < victim_neighbor_count; ++i) {
        int32_t neighbor = victim_neighbors[i];
        int32_t *neighbor_row;
        int32_t neighbor_degree;

        if (neighbor < 0 || neighbor == node_id || index->deleted[neighbor]) {
            continue;
        }
        neighbor_row = index->neighbors + (size_t)neighbor * (size_t)index->index_size;
        neighbor_degree = index->neighbor_counts[neighbor];
        for (j = 0; j < neighbor_degree; ++j) {
            int32_t candidate = neighbor_row[j];
            if (candidate >= 0 && candidate != node_id && !index->deleted[candidate]) {
                diskann_append_unique_candidate(pool,
                                                      pool_count,
                                                      pool_capacity,
                                                      candidate,
                                                      diskann_fast_l2_distance(index, node_id, candidate));
            }
        }
    }

    for (i = 0; i < index->storage_params.frozen_point_count; ++i) {
        int32_t candidate = index->frozen_points ? index->frozen_points[i] : -1;
        if (candidate >= 0 && candidate != node_id && !index->deleted[candidate]) {
            diskann_append_unique_candidate(pool,
                                                  pool_count,
                                                  pool_capacity,
                                                  candidate,
                                                  diskann_fast_l2_distance(index, node_id, candidate));
        }
    }

    return 0;
}

int diskann_repair_neighbors_after_delete(diskann_t *index,
                                                const bool *affected,
                                                int32_t affected_count,
                                                const int32_t *victim_neighbors,
                                                int32_t victim_neighbor_count)
{
    diskann_scored_t *pool;
    int32_t *result_ids;
    int32_t i;

    if (!index || !affected) {
        return -1;
    }

    if (index->index_size <= 2 || affected_count > diskann_max_i32(1, index->index_size)) {
        return diskann_index_build(index);
    }

    if (affected_count > diskann_max_i32(8, index->active_count / 2)) {
        return diskann_index_build(index);
    }

    pool = (diskann_scored_t *)malloc((size_t)diskann_max_i32(index->active_count, 1) * sizeof(diskann_scored_t));
    result_ids = (int32_t *)malloc((size_t)index->index_size * sizeof(int32_t));
    if (!pool || !result_ids) {
        free(pool);
        free(result_ids);
        return -1;
    }

    for (i = 0; i < index->n_total; ++i) {
        int32_t pool_count = 0;
        int32_t result_count = 0;

        if (!affected[i] || index->deleted[i]) {
            continue;
        }

        if (diskann_build_repair_candidate_pool(index,
                                                      i,
                                                      victim_neighbors,
                                                      victim_neighbor_count,
                                                      pool,
                                                      diskann_max_i32(index->active_count, 1),
                                                      &pool_count) != 0) {
            free(pool);
            free(result_ids);
            return -1;
        }

        if (diskann_robust_prune(index, i, pool, pool_count, result_ids, &result_count, 0) != 0) {
            free(pool);
            free(result_ids);
            return -1;
        }

        diskann_set_neighbors_for_node(index, i, result_ids, result_count);
    }

    for (i = 0; i < index->n_total; ++i) {
        int32_t *row;
        int32_t degree;
        int32_t j;

        if (!affected[i] || index->deleted[i]) {
            continue;
        }

        row = index->neighbors + (size_t)i * (size_t)index->index_size;
        degree = index->neighbor_counts[i];
        for (j = 0; j < degree; ++j) {
            int32_t neighbor = row[j];
            if (neighbor >= 0 && !index->deleted[neighbor] && diskann_add_reverse_edge(index, i, neighbor) != 0) {
                free(pool);
                free(result_ids);
                return -1;
            }
        }
    }

    free(pool);
    free(result_ids);
    return diskann_rebuild_frozen_points(index);
}

int diskann_iterate_to_fixed_point(const diskann_t *index,
                                    int32_t node_id,
                                    diskann_scored_t *pool,
                                    int32_t *pool_count)
{
    bool *visited;
    diskann_heap_t heap;
    int32_t capacity;
    int32_t expansions;
    int32_t max_expansions;
    int32_t i;

    if (!index || !pool || !pool_count || node_id < 0 || node_id >= index->n_total) {
        return -1;
    }

    *pool_count = 0;
    if (index->active_count <= 1) {
        return 0;
    }

    capacity = diskann_max_i32(index->active_count, index->build_search_list_size);
    visited = (bool *)calloc((size_t)index->n_total, sizeof(bool));
    if (!visited) {
        return -1;
    }

    max_expansions = index->build_search_list_size;
    heap.data = pool;
    heap.capacity = capacity;
    heap.size = 0;

    /* 从 frozen points 作为种子出发 */
    for (i = 0; i < index->storage_params.frozen_point_count; ++i) {
        int32_t seed_id = index->frozen_points ? index->frozen_points[i] : -1;
        if (seed_id >= 0 && seed_id < index->n_total && !index->deleted[seed_id] &&
            seed_id != node_id && !visited[seed_id]) {
            visited[seed_id] = true;
            pool[heap.size].id = seed_id;
            pool[heap.size].distance = diskann_fast_l2_distance(index, node_id, seed_id);
            pool[heap.size].expanded = false;
            heap.size += 1;
        }
    }

    /* 冻结点不可用时回退到 entry_point */
    if (heap.size == 0 && index->entry_point >= 0 && !index->deleted[index->entry_point] &&
        index->entry_point != node_id) {
        visited[index->entry_point] = true;
        pool[heap.size].id = index->entry_point;
        pool[heap.size].distance = diskann_fast_l2_distance(index, node_id, index->entry_point);
        pool[heap.size].expanded = false;
        heap.size += 1;
    }

    if (heap.size == 0) {
        free(visited);
        return 0;
    }

    diskann_heapify(&heap);
    expansions = 0;

    while (expansions < max_expansions) {
        diskann_scored_t current;
        int32_t *row;
        int32_t degree;

        diskann_heap_skip_expanded(&heap);
        if (heap.size <= 0) {
            break;
        }

        current = diskann_heap_pop(&heap);
        current.expanded = true;
        /* 将已展开节点保存到数组末端供后续收集 */
        {
            int32_t slot = capacity - 1 - (*pool_count);
            pool[slot] = current;
        }
        (*pool_count) += 1;
        expansions += 1;

        row = index->neighbors + (size_t)current.id * (size_t)index->index_size;
        degree = index->neighbor_counts[current.id];
        for (i = 0; i < degree; ++i) {
            int32_t neighbor_id = row[i];
            if (neighbor_id < 0 || neighbor_id >= index->n_total ||
                index->deleted[neighbor_id] || visited[neighbor_id]) {
                continue;
            }
            visited[neighbor_id] = true;
            {
                diskann_scored_t nc;
                nc.id = neighbor_id;
                nc.distance = diskann_fast_l2_distance(index, node_id, neighbor_id);
                nc.expanded = false;
                if (heap.size < capacity) {
                    diskann_heap_push(&heap, nc);
                }
            }
        }
    }

    /* 合并堆中剩余节点和已展开节点到 pool 开头 */
    {
        int32_t total = 0;
        int32_t j;
        for (j = 0; j < heap.size; ++j) {
            pool[total++] = pool[j];
        }
        for (j = capacity - (*pool_count); j < capacity; ++j) {
            pool[total++] = pool[j];
        }
        *pool_count = total;
    }

    free(visited);
    return 0;
}

int diskann_inter_insert(diskann_t *index,
                          int32_t node_id,
                          const int32_t *result_ids,
                          int32_t result_count)
{
    int32_t i;

    if (!index || !result_ids || result_count <= 0) {
        return 0;
    }

    for (i = 0; i < result_count; ++i) {
        int32_t to_id = result_ids[i];

        if (to_id < 0 || to_id >= index->n_total || index->deleted[to_id] ||
            to_id == node_id) {
            continue;
        }

        /* 已有反向边则跳过 */
        if (diskann_has_neighbor(index, to_id, node_id)) {
            continue;
        }

        /* 目标邻居未满时直接追加反向边 */
        if (index->neighbor_counts[to_id] < index->index_size) {
            int32_t *row = index->neighbors + (size_t)to_id * (size_t)index->index_size;
            row[index->neighbor_counts[to_id]++] = node_id;
            continue;
        }

        /* 目标邻居已满时重新剪枝 */
        {
            diskann_scored_t *local_pool;
            int32_t *local_result_ids;
            int32_t *row;
            int32_t local_pool_count = 0;
            int32_t local_result_count = 0;
            int32_t j;

            local_pool = (diskann_scored_t *)malloc(
                (size_t)(index->index_size + 1) * sizeof(diskann_scored_t));
            local_result_ids = (int32_t *)malloc(
                (size_t)index->index_size * sizeof(int32_t));
            if (!local_pool || !local_result_ids) {
                free(local_pool);
                free(local_result_ids);
                return -1;
            }

            row = index->neighbors + (size_t)to_id * (size_t)index->index_size;
            for (j = 0; j < index->neighbor_counts[to_id]; ++j) {
                if (row[j] >= 0 && !index->deleted[row[j]]) {
                    diskann_append_unique_candidate(local_pool,
                                                    &local_pool_count,
                                                    index->index_size + 1,
                                                    row[j],
                                                    diskann_fast_l2_distance(
                                                        index, to_id, row[j]));
                }
            }
            diskann_append_unique_candidate(local_pool,
                                            &local_pool_count,
                                            index->index_size + 1,
                                            node_id,
                                            diskann_fast_l2_distance(
                                                index, to_id, node_id));

            if (diskann_robust_prune(index, to_id, local_pool, local_pool_count,
                                     local_result_ids, &local_result_count, 0) != 0) {
                free(local_pool);
                free(local_result_ids);
                return -1;
            }

            diskann_set_neighbors_for_node(index, to_id, local_result_ids,
                                           local_result_count);
            free(local_pool);
            free(local_result_ids);
        }
    }

    return 0;
}

void diskann_remove_id_from_row(diskann_t *index, int32_t row_id, int32_t victim_id)
{
    int32_t *row;
    int32_t degree;
    int32_t write_idx;
    int32_t i;

    if (row_id < 0 || row_id >= index->n_total) {
        return;
    }

    row = index->neighbors + (size_t)row_id * (size_t)index->index_size;
    degree = index->neighbor_counts[row_id];
    write_idx = 0;

    for (i = 0; i < degree; ++i) {
        if (row[i] != victim_id) {
            row[write_idx++] = row[i];
        }
    }
    while (write_idx < index->index_size) {
        row[write_idx++] = -1;
    }
    index->neighbor_counts[row_id] = diskann_min_i32(write_idx, index->index_size);
}
