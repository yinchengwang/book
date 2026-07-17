/* diskann index search */

#include "diskann_private.h"

#include "diskann_spann.h"
#include "diskann_fresh.h"

#include <db/index/heap/heap_vector_store.h>
#include <db/index/vector_ref.h>
#include <algo-prod/distance/distance.h>

/* ============================================================================
 * SPANN 增强搜索：支持分区选择
 * ============================================================================ */

/**
 * @brief 在选中分区中进行搜索
 * @param[in] index 索引
 * @param[in] query 查询向量
 * @param[in] selected_partitions 选中的分区 ID
 * @param[in] selected_count 选中分区数量
 * @param[in] partition_ids 每个向量所属分区 ID
 * @param[in] k 结果数量
 * @param[in] search_list_size 搜索候选大小
 * @param[out] distances 距离数组
 * @param[out] labels 标签数组
 * @return 成功返回结果数量，失败返回 -1
 */
int32_t diskann_search_in_partitions(diskann_t *index,
                                     const float *query,
                                     const int32_t *selected_partitions,
                                     int32_t selected_count,
                                     const int32_t *partition_ids,
                                     int32_t k,
                                     int32_t search_list_size,
                                     float *distances,
                                     int32_t *labels)
{
    diskann_scored_t *results;
    diskann_scored_t *filtered;
    int32_t result_count;
    int32_t filtered_count;
    int32_t i, j;
    int32_t effective_k;
    int32_t total_vectors;

    if (!index || !query || !selected_partitions || !partition_ids || k <= 0) {
        return -1;
    }

    total_vectors = diskann_index_size(index);
    effective_k = diskann_min_i32(k, index->active_count);

    /* 分配临时数组 */
    results = (diskann_scored_t *)malloc((size_t)search_list_size * sizeof(diskann_scored_t));
    filtered = (diskann_scored_t *)malloc((size_t)total_vectors * sizeof(diskann_scored_t));

    if (!results || !filtered) {
        free(results);
        free(filtered);
        return -1;
    }

    /* 先在全局搜索获取候选 */
    if (diskann_search_candidates(index, query, search_list_size, results, &result_count) != 0) {
        free(results);
        free(filtered);
        return -1;
    }

    /* 按分区过滤 */
    filtered_count = 0;
    for (i = 0; i < result_count && filtered_count < total_vectors; ++i) {
        int32_t id = results[i].id;
        if (id >= 0 && id < total_vectors) {
            int32_t pid = partition_ids[id];
            /* 检查是否在选中的分区中 */
            for (j = 0; j < selected_count; ++j) {
                if (pid == selected_partitions[j]) {
                    filtered[filtered_count++] = results[i];
                    break;
                }
            }
        }
    }

    /* 如果过滤后结果不足，返回所有候选 */
    if (filtered_count < effective_k && filtered_count < result_count) {
        for (i = 0; i < result_count && filtered_count < total_vectors; ++i) {
            filtered[filtered_count++] = results[i];
        }
    }

    /* 排序并返回 top-k */
    qsort(filtered, (size_t)filtered_count, sizeof(diskann_scored_t), diskann_compare_scored);

    for (i = 0; i < effective_k && i < filtered_count; ++i) {
        distances[i] = filtered[i].distance;
        labels[i] = filtered[i].id;
    }
    for (; i < k; ++i) {
        distances[i] = FLT_MAX;
        labels[i] = -1;
    }

    free(results);
    free(filtered);
    return effective_k;
}

/* ============================================================================
 * FreshDiskANN 增强搜索：双区搜索
 * ============================================================================ */

/**
 * @brief 在动态区和静态区同时搜索
 * @param[in] index 索引
 * @param[in] query 查询向量
 * @param[in] k 结果数量
 * @param[in] search_list_size 搜索候选大小
 * @param[out] distances 距离数组
 * @param[out] labels 标签数组
 * @return 成功返回结果数量，失败返回 -1
 */
int32_t diskann_search_dual_zone(diskann_t *index,
                                 const float *query,
                                 int32_t k,
                                 int32_t search_list_size,
                                 float *distances,
                                 int32_t *labels)
{
    float *frozen_distances;
    float *fresh_distances;
    int32_t *frozen_labels;
    int32_t *fresh_labels;
    diskann_scored_t *merged;
    int32_t i;
    int32_t effective_k;
    int32_t frozen_count = 0;
    int32_t fresh_count = 0;
    int32_t merged_count = 0;

    if (!index || !query || !distances || !labels || k <= 0) {
        return -1;
    }

    effective_k = diskann_min_i32(k, diskann_index_active_size(index));

    /* 分配临时数组 */
    frozen_distances = (float *)malloc((size_t)k * sizeof(float));
    fresh_distances = (float *)malloc((size_t)k * sizeof(float));
    frozen_labels = (int32_t *)malloc((size_t)k * sizeof(int32_t));
    fresh_labels = (int32_t *)malloc((size_t)k * sizeof(int32_t));
    merged = (diskann_scored_t *)malloc((size_t)(k * 2) * sizeof(diskann_scored_t));

    if (!frozen_distances || !fresh_distances || !frozen_labels || !fresh_labels || !merged) {
        free(frozen_distances);
        free(fresh_distances);
        free(frozen_labels);
        free(fresh_labels);
        free(merged);
        return -1;
    }

    /* 初始化为无效值 */
    for (i = 0; i < k; ++i) {
        frozen_distances[i] = FLT_MAX;
        fresh_distances[i] = FLT_MAX;
        frozen_labels[i] = -1;
        fresh_labels[i] = -1;
    }

    /* 在静态区搜索 */
    frozen_count = diskann_index_search(index, query, k, search_list_size, 1, frozen_distances, frozen_labels);

    /* 在动态区搜索（如果启用） */
    if (index->fresh_ctx && diskann_fresh_count(index->fresh_ctx) > 0) {
        /* 动态区标签需要偏移以区分 */
        int32_t offset = diskann_index_size(index);
        fresh_count = diskann_fresh_search(index->fresh_ctx, query, k, search_list_size,
                                          fresh_distances, fresh_labels);

        /* 调整动态区标签（加偏移量） */
        for (i = 0; i < fresh_count; ++i) {
            if (fresh_labels[i] >= 0) {
                fresh_labels[i] += offset;
            }
        }
    }

    /* 合并结果 */
    for (i = 0; i < frozen_count && i < k; ++i) {
        if (frozen_labels[i] >= 0) {
            merged[merged_count].id = frozen_labels[i];
            merged[merged_count].distance = frozen_distances[i];
            merged[merged_count].expanded = false;
            merged_count++;
        }
    }

    for (i = 0; i < fresh_count && i < k; ++i) {
        if (fresh_labels[i] >= 0) {
            merged[merged_count].id = fresh_labels[i];
            merged[merged_count].distance = fresh_distances[i];
            merged[merged_count].expanded = false;
            merged_count++;
        }
    }

    /* 排序并返回 top-k */
    qsort(merged, (size_t)merged_count, sizeof(diskann_scored_t), diskann_compare_scored);

    for (i = 0; i < effective_k && i < merged_count; ++i) {
        distances[i] = merged[i].distance;
        labels[i] = merged[i].id;
    }
    for (; i < k; ++i) {
        distances[i] = FLT_MAX;
        labels[i] = -1;
    }

    free(frozen_distances);
    free(fresh_distances);
    free(frozen_labels);
    free(fresh_labels);
    free(merged);

    return effective_k;
}

/**
 * @brief 获取动态区搜索结果并调整标签
 * @param[in] ctx Fresh 上下文
 * @param[in] index 静态区索引
 * @param[in] query 查询向量
 * @param[in] k 结果数量
 * @param[in] search_list_size 搜索候选大小
 * @param[out] distances 距离数组
 * @param[out] labels 标签数组
 * @return 成功返回结果数量，失败返回 -1
 */
int32_t diskann_fresh_search_with_labels(diskann_fresh_context_t *ctx,
                                        diskann_t *index,
                                        const float *query,
                                        int32_t k,
                                        int32_t search_list_size,
                                        float *distances,
                                        int32_t *labels)
{
    float *raw_distances;
    int32_t *raw_labels;
    int32_t *offset_labels;
    int32_t i;
    int32_t count;
    int32_t result_count;
    int32_t frozen_size;

    if (!ctx || !index || !query || !distances || !labels) {
        return -1;
    }

    frozen_size = diskann_index_size(index);

    raw_distances = (float *)malloc((size_t)k * sizeof(float));
    raw_labels = (int32_t *)malloc((size_t)k * sizeof(int32_t));
    offset_labels = (int32_t *)malloc((size_t)k * sizeof(int32_t));

    if (!raw_distances || !raw_labels || !offset_labels) {
        free(raw_distances);
        free(raw_labels);
        free(offset_labels);
        return -1;
    }

    /* 在动态区搜索 */
    count = diskann_fresh_search(ctx, query, k, search_list_size, raw_distances, raw_labels);

    /* 调整标签：动态区标签 + 静态区大小 = 全局标签 */
    result_count = 0;
    for (i = 0; i < count && i < k; ++i) {
        if (raw_labels[i] >= 0) {
            distances[result_count] = raw_distances[i];
            labels[result_count] = raw_labels[i] + frozen_size;
            result_count++;
        }
    }

    free(raw_distances);
    free(raw_labels);
    free(offset_labels);

    return result_count;
}

/* internal search helper */

int diskann_search_candidates(const diskann_t *index,
                                    const float *query,
                                    int32_t search_list_size,
                                    diskann_scored_t *results,
                                    int32_t *result_count)
{
    bool *visited;
    diskann_scored_t *candidates;
    float *distance_table;
    int32_t distance_table_size;
    int32_t capacity;
    int32_t count;
    int32_t expansions;
    int32_t max_expansions;
    int32_t i;

    if (!index || !query || !results || !result_count || index->active_count <= 0) {
        return -1;
    }

    /* 第一步: 初始化 visited、候选池，以及可选的 PQ 距离表。 */
    capacity = index->n_total;
    visited = (bool *)calloc((size_t)index->n_total, sizeof(bool));
    candidates = (diskann_scored_t *)malloc((size_t)capacity * sizeof(diskann_scored_t));
    if (!visited || !candidates) {
        free(visited);
        free(candidates);
        return -1;
    }

    distance_table = NULL;
    if (diskann_pq_ready(index)) {
        distance_table_size = quantizer_distance_table_size(index->quantizer);
        distance_table = (float *)malloc((size_t)distance_table_size * sizeof(float));
        if (!distance_table) {
            free(visited);
            free(candidates);
            return -1;
        }
        if (quantizer_compute_distance_table(index->quantizer,
                                                  index->metric,
                                                  query,
                                                  distance_table) != 0) {
            free(distance_table);
            free(visited);
            free(candidates);
            return -1;
        }
    }

    count = 0;
    expansions = 0;
    max_expansions = diskann_max_i32(search_list_size, 1);

    /* 第二步: 优先从 frozen points 作为种子出发；它们相当于图上的稳定入口。 */
    for (i = 0; i < index->storage_params.frozen_point_count; ++i) {
        int32_t seed_id = index->frozen_points ? index->frozen_points[i] : -1;
        if (seed_id >= 0 && seed_id < index->n_total && !index->deleted[seed_id] && !visited[seed_id]) {
            visited[seed_id] = true;
            candidates[count].id = seed_id;
            candidates[count].distance = diskann_candidate_distance_from_query(index, query, distance_table, seed_id);
            candidates[count].expanded = false;
            count += 1;
        }
    }

    if (count == 0) {
        for (i = 0; i < index->n_total; ++i) {
            if (!index->deleted[i]) {
                visited[i] = true;
                candidates[count].id = i;
                candidates[count].distance = diskann_candidate_distance_from_query(index, query, distance_table, i);
                candidates[count].expanded = false;
                count += 1;
                break;
            }
        }
    }

    /*
     * 第三步: 使用二叉最小堆选择最近未展开节点，替代线性扫描。
     * 堆直接包装 candidates 数组，无额外内存分配。
     */
    {
        diskann_heap_t heap;
        int32_t expanded_slot;

        heap.data = candidates;
        heap.capacity = capacity;
        heap.size = count;
        diskann_heapify(&heap);

        /* 从数组末端回存已展开节点，避免分配额外数组 */
        expanded_slot = capacity;

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
            /* 将已展开节点保存到数组末端，供最终排序使用 */
            expanded_slot -= 1;
            candidates[expanded_slot] = current;
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
                    nc.distance = diskann_candidate_distance_from_query(
                        index, query, distance_table, neighbor_id);
                    nc.expanded = false;
                    diskann_heap_push(&heap, nc);
                }
            }
        }

        /*
         * 将堆中剩余未展开节点 + 已展开节点合并到 candidates[0..total-1]，
         * 然后排序截断。
         */
        {
            int32_t total = 0;
            int32_t j;
            for (j = 0; j < heap.size; ++j) {
                candidates[total++] = heap.data[j];
            }
            for (j = expanded_slot; j < capacity; ++j) {
                candidates[total++] = candidates[j];
            }
            count = total;
        }
    }

    /* 第四步: 对已见候选按距离排序，并截断到 search_list_size。 */
    qsort(candidates, (size_t)count, sizeof(diskann_scored_t), diskann_compare_scored);
    *result_count = diskann_min_i32(count, search_list_size);
    memcpy(results, candidates, (size_t)(*result_count) * sizeof(diskann_scored_t));

    free(distance_table);
    free(visited);
    free(candidates);
    return 0;
}

/* public search api */

int32_t diskann_index_search(diskann_t *index,
                                   const float *query,
                                   int32_t k,
                                   int32_t search_list_size,
                                   int32_t max_iterations,
                                   float *distances,
                                   int32_t *labels)
{
    diskann_scored_t *results;
    diskann_scored_t *merged;
    int32_t result_count;
    int32_t effective_k;
    int32_t effective_l;
    int32_t iterations;
    int32_t i;

    if (!index || !query || !distances || !labels || k <= 0) {
        return -1;
    }

    if (index->active_count <= 0) {
        for (i = 0; i < k; ++i) {
            distances[i] = FLT_MAX;
            labels[i] = -1;
        }
        return 0;
    }

    /* 如果图尚未构建，则先补建。 */
    if (!index->built && diskann_index_build(index) != 0) {
        return -1;
    }

    effective_k = diskann_min_i32(k, index->active_count);
    effective_l = diskann_max_i32(search_list_size, diskann_max_i32(index->build_search_list_size, effective_k));
    effective_l = diskann_min_i32(effective_l, index->active_count);

    results = (diskann_scored_t *)malloc((size_t)effective_l * sizeof(diskann_scored_t));
    merged = (diskann_scored_t *)malloc((size_t)effective_l * sizeof(diskann_scored_t));
    if (!results || !merged) {
        free(results);
        free(merged);
        return -1;
    }

    iterations = 0;
    result_count = 0;
    if (max_iterations <= 0) {
        max_iterations = 1;
    }

    /*
     * 渐进搜索扩宽：当首轮返回不足时，翻倍 search_list_size 后重新搜索。
     */
    while (iterations < max_iterations && result_count < effective_k) {
        int32_t current_l = effective_l;

        /* 翻倍搜索深度（首轮不翻倍） */
        if (iterations > 0) {
            current_l = diskann_min_i32(effective_l * 2, index->active_count);
            effective_l = current_l;
        }

        {
            int32_t round_count = 0;

            /* 重新分配更大的 results 数组 */
            if (iterations > 0) {
                diskann_scored_t *new_results = (diskann_scored_t *)realloc(
                    results, (size_t)current_l * sizeof(diskann_scored_t));
                if (!new_results) {
                    free(results);
                    free(merged);
                    return -1;
                }
                results = new_results;
            }

            if (diskann_search_candidates(index, query, current_l, results, &round_count) != 0) {
                free(results);
                free(merged);
                return -1;
            }

            /* 先前轮次结果已存在 merged 中，合并本轮结果 */
            if (iterations == 0) {
                for (i = 0; i < round_count; ++i) {
                    merged[i] = results[i];
                }
                result_count = round_count;
            } else {
                /* 合并去重：按 id 保留距离更小的版本 */
                for (i = 0; i < round_count; ++i) {
                    int32_t j;
                    bool found = false;
                    for (j = 0; j < result_count; ++j) {
                        if (merged[j].id == results[i].id) {
                            if (results[i].distance < merged[j].distance) {
                                merged[j].distance = results[i].distance;
                            }
                            found = true;
                            break;
                        }
                    }
                    if (!found && result_count < current_l) {
                        merged[result_count++] = results[i];
                    }
                }
            }
        }

        iterations += 1;

        /* 已达最大候选集则不再翻倍 */
        if (effective_l >= index->active_count) {
            break;
        }
    }

    /*
     * 第五步（Heap 模式专属重排序）:
     *   对阶段 1 收集的 result_count 个候选，从 Heap 取出完整向量，
     *   用精确 L2 距离重排序，再取 top-k。
     *
     * 这保证即使量化启用，最终结果也基于精确距离。
     * Heap 模式下候选池已自动放大（见 Step 2），弥补近似距离的误差。
     */
    if (index->heap_store != NULL && result_count > 0) {
        float *candidate_vectors = (float *)malloc((size_t)result_count * index->dims * sizeof(float));
        if (candidate_vectors == NULL) {
            free(results);
            free(merged);
            return -1;
        }

        /* 从 Heap 批量加载候选向量 */
        for (i = 0; i < result_count; i++) {
            int32_t vec_id = merged[i].id;
            if (vec_id < 0 || (uint32_t)vec_id >= index->vector_refs_size) {
                continue;
            }
            if (heap_vector_get(index->heap_store,
                                &index->vector_refs[vec_id],
                                &candidate_vectors[(size_t)i * index->dims]) != 0) {
                /* 加载失败，保留原距离作为兜底 */
                continue;
            }
            /* 用精确距离覆盖阶段 1 的近似距离 */
            merged[i].distance = distance_compute(
                index->metric,
                query,
                &candidate_vectors[(size_t)i * index->dims],
                index->dims);
        }
        free(candidate_vectors);
    } else {
        /* 兼容模式：直接从 index->vectors 计算精确距离 */
        for (i = 0; i < result_count; ++i) {
            merged[i].distance = diskann_exact_distance_from_query(index, query, merged[i].id);
        }
    }

    qsort(merged, (size_t)result_count, sizeof(diskann_scored_t), diskann_compare_scored);

    for (i = 0; i < effective_k && i < result_count; ++i) {
        distances[i] = merged[i].distance;
        labels[i] = merged[i].id;
    }
    for (; i < k; ++i) {
        distances[i] = FLT_MAX;
        labels[i] = -1;
    }

    free(results);
    free(merged);
    return effective_k;
}
