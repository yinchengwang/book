/* diskann index insert */

#include "diskann_private.h"

#include <db/index/heap/heap_vector_store.h>
#include <db/index/vector_ref.h>

/*
 * diskann_load_vector - 从 Heap 取出指定 ID 的向量
 *
 * Heap 模式下，通过 vector_refs[id] 定位页面和偏移，
 * 调用 heap_vector_get 取回完整向量。
 * 兼容模式下，直接从 index->vectors 复制。
 *
 * 返回 0 表示成功，非 0 表示失败。
 */
static int diskann_load_vector(const diskann_t *index,
                               int32_t vec_id,
                               float *out)
{
    if (index == NULL || vec_id < 0 || out == NULL) {
        return -1;
    }

    if (index->heap_store != NULL) {
        if ((uint32_t)vec_id >= index->vector_refs_size) {
            return -1;
        }
        return heap_vector_get(index->heap_store,
                               &index->vector_refs[vec_id],
                               out);
    }

    /* 兼容路径：直接从连续数组读取 */
    if (index->vectors == NULL || vec_id >= index->n_total) {
        return -1;
    }
    memcpy(out, &index->vectors[vec_id * index->dims],
           (size_t)index->dims * sizeof(float));
    return 0;
}

/*
 * diskann_store_vector - 将向量写入 Heap 存储
 *
 * Heap 模式：调用 heap_vector_insert 写入并记录引用
 * 兼容模式：追加到 index->vectors 数组
 *
 * 返回 0 表示成功，非 0 表示失败。
 */
static int diskann_store_vector(diskann_t *index,
                                const float *vector,
                                int32_t new_id)
{
    if (index == NULL || vector == NULL || new_id < 0) {
        return -1;
    }

    if (index->heap_store != NULL) {
        /* 扩展 vector_refs 数组 */
        uint32_t new_size = (uint32_t)(new_id + 1);
        vector_ref_t *new_refs = (vector_ref_t *)realloc(
            index->vector_refs,
            (size_t)new_size * sizeof(vector_ref_t));
        if (new_refs == NULL) {
            return -1;
        }
        index->vector_refs = new_refs;
        index->vector_refs_size = new_size;

        /* 写入 Heap 并记录引用 */
        vector_ref_t ref = heap_vector_insert(index->heap_store, vector);
        if (!vector_ref_is_valid(&ref)) {
            return -1;
        }
        index->vector_refs[new_id] = ref;
        return 0;
    }

    /* 兼容路径：直接存入 vectors 数组（已在 diskann_ensure_vector_capacity 中分配） */
    memcpy(&index->vectors[new_id * index->dims],
           vector,
           (size_t)index->dims * sizeof(float));
    return 0;
}

/* internal insert helper */

int diskann_link_node(diskann_t *index, int32_t node_id)
{
    diskann_scored_t *pool;
    int32_t *result_ids;
    int32_t pool_count;
    int32_t result_count;
    int32_t capacity;

    if (!index || node_id < 0 || node_id >= index->n_total) {
        return -1;
    }

    if (index->active_count <= 1) {
        return 0;
    }

    capacity = diskann_max_i32(index->active_count, index->build_search_list_size);
    pool = (diskann_scored_t *)malloc((size_t)capacity * sizeof(diskann_scored_t));
    result_ids = (int32_t *)malloc((size_t)index->index_size * sizeof(int32_t));
    if (!pool || !result_ids) {
        free(pool);
        free(result_ids);
        return -1;
    }

    /* 三步 Link 流程：图搜索 → 剪枝 → 对称边维护 */
    pool_count = 0;
    if (diskann_iterate_to_fixed_point(index, node_id, pool, &pool_count) != 0) {
        free(pool);
        free(result_ids);
        return -1;
    }

    result_count = 0;
    if (diskann_robust_prune(index, node_id, pool, pool_count,
                             result_ids, &result_count, 0) != 0) {
        free(pool);
        free(result_ids);
        return -1;
    }

    diskann_set_neighbors_for_node(index, node_id, result_ids, result_count);

    if (diskann_inter_insert(index, node_id, result_ids, result_count) != 0) {
        free(pool);
        free(result_ids);
        return -1;
    }

    free(pool);
    free(result_ids);
    return diskann_rebuild_frozen_points(index);
}

int diskann_incremental_insert_node(diskann_t *index, int32_t node_id)
{
    if (!index || !index->built || index->active_count <= 1) {
        return 0;
    }

    return diskann_link_node(index, node_id);
}

/* public insert apis */

int32_t diskann_index_add(diskann_t *index, int32_t n, const float *vectors)
{
    int32_t i;

    if (!index || !vectors || n <= 0) {
        return -1;
    }
    for (i = 0; i < n; ++i) {
        if (diskann_index_insert(index, &vectors[i * index->dims], NULL) < 0) {
            return -1;
        }
    }
    return 0;
}

int32_t diskann_index_insert(diskann_t *index, const float *vector, int32_t *label)
{
    int32_t new_id;

    if (!index || !vector) {
        return -1;
    }
    if (diskann_ensure_vector_capacity(index, index->n_total + 1) != 0) {
        return -1;
    }

    /* 第一步: 把向量追加到存储区，预计算平方范数，并初始化删除标记与邻接行。 */
    new_id = index->n_total;

    /* 存储向量（Heap 或兼容模式） */
    if (diskann_store_vector(index, vector, new_id) != 0) {
        return -1;
    }

    /* 预计算平方范数（Heap 模式下需要先读取） */
    {
        float norm = 0.0f;
        int32_t d;
        if (index->heap_store != NULL) {
            /* Heap 模式：直接从输入向量计算（不重新加载） */
            for (d = 0; d < index->dims; ++d) {
                norm += vector[d] * vector[d];
            }
        } else {
            /* 兼容模式：从已存储位置读取 */
            for (d = 0; d < index->dims; ++d) {
                norm += index->vectors[new_id * index->dims + d] *
                        index->vectors[new_id * index->dims + d];
            }
        }
        index->norms[new_id] = norm;
    }
    index->deleted[new_id] = 0;
    index->neighbor_counts[new_id] = 0;
    memset(index->neighbors + (size_t)new_id * (size_t)index->index_size,
           -1,
           (size_t)index->index_size * sizeof(int32_t));
    index->n_total += 1;
    index->active_count += 1;

    /* 第二步: 若启用了 PQ，则同步生成该向量的编码。 */
    if (diskann_maybe_encode_vector(index, new_id) != 0) {
        return -1;
    }

    /* 第三步: 已建图时走增量接边；未建图时只更新计数，等待统一 build。 */
    if (index->built) {
        if (diskann_rebuild_frozen_points(index) != 0 || diskann_incremental_insert_node(index, new_id) != 0) {
            return -1;
        }
    } else {
        index->entry_point = -1;
    }

    if (label) {
        *label = new_id;
    }
    return new_id;
}

/* public build apis */

int32_t diskann_index_train_pq(diskann_t *index)
{
    float *training_vectors;
    int32_t training_count;

    if (!index || !diskann_pq_enabled(index)) {
        return -1;
    }
    if (index->active_count <= 0) {
        return -1;
    }
    if (diskann_ensure_quantizer(index) != 0) {
        return -1;
    }
    if (diskann_gather_training_vectors(index, &training_vectors, &training_count) != 0) {
        return -1;
    }

    quantizer_reset(index->quantizer);
    if (quantizer_train(index->quantizer, training_count, training_vectors) != 0) {
        free(training_vectors);
        return -1;
    }
    free(training_vectors);

    index->pq_trained_codebook_size = quantizer_distance_table_size(index->quantizer) /
                                      diskann_max_i32(index->quantization_params.pq_m, 1);
    return diskann_encode_all_vectors(index);
}

int32_t diskann_index_build(diskann_t *index)
{
    bool *inserted;
    int32_t *active_ids;
    int32_t *order;
    int32_t *pruned_ids;
    diskann_scored_t *pool;
    int32_t active_count;
    int32_t inserted_count;
    int32_t i;

    if (!index) {
        return -1;
    }
    if (index->active_count <= 0) {
        index->built = true;
        index->entry_point = -1;
        return 0;
    }

    /* 第一步: 如有需要，先训练/刷新 PQ 编码。 */
    if (diskann_pq_enabled(index)) {
        if ((!index->quantizer || !quantizer_is_trained(index->quantizer)) &&
            diskann_index_train_pq(index) != 0) {
            return -1;
        }
        if (diskann_encode_all_vectors(index) != 0) {
            return -1;
        }
    }

    /* 第二步: 重置图结构，收集全部活跃节点并选择入口点。 */
    diskann_reset_graph(index);
    if (diskann_collect_active_ids(index, &active_ids, &active_count) != 0) {
        return -1;
    }

    index->entry_point = diskann_choose_entry_point_from_active(index, active_ids, active_count);
    if (diskann_rebuild_frozen_points(index) != 0) {
        free(active_ids);
        return -1;
    }

    inserted = (bool *)calloc((size_t)index->n_total, sizeof(bool));
    order = (int32_t *)malloc((size_t)active_count * sizeof(int32_t));
    pruned_ids = (int32_t *)malloc((size_t)index->index_size * sizeof(int32_t));
    pool = (diskann_scored_t *)malloc((size_t)active_count * sizeof(diskann_scored_t));
    if (!inserted || !order || !pruned_ids || !pool) {
        free(active_ids);
        free(inserted);
        free(order);
        free(pruned_ids);
        free(pool);
        return -1;
    }

    /* 第三步: 让入口点最先插入，其余节点按顺序逐个接入图。 */
    order[0] = index->entry_point;
    inserted_count = 0;
    /*
     * 第四步: 对每个待插入节点，收集“已插入节点”作为候选池，
     * 经过 robust prune 后得到出边，再为这些出边补上反向边。
     */
    for (i = 0; i < active_count; ++i) {
        if (active_ids[i] != index->entry_point) {
            order[++inserted_count] = active_ids[i];
        }
    }
    inserted_count = 0;

    for (i = 0; i < active_count; ++i) {
        int32_t node_id = order[i];
        int32_t pool_count = 0;
        int32_t result_count = 0;
        int32_t other;

        if (inserted_count == 0) {
            inserted[node_id] = true;
            inserted_count += 1;
            continue;
        }

        for (other = 0; other < index->n_total; ++other) {
            if (!inserted[other] || index->deleted[other]) {
                continue;
            }
            diskann_append_unique_candidate(pool,
                                            &pool_count,
                                            active_count,
                                            other,
                                            diskann_fast_l2_distance(index, node_id, other));
        }

        if (diskann_robust_prune(index, node_id, pool, pool_count, pruned_ids, &result_count, 0) != 0) {
            free(active_ids);
            free(inserted);
            free(order);
            free(pruned_ids);
            free(pool);
            return -1;
        }

        diskann_set_neighbors_for_node(index, node_id, pruned_ids, result_count);
        inserted[node_id] = true;
        inserted_count += 1;
        for (other = 0; other < result_count; ++other) {
            if (diskann_add_reverse_edge(index, node_id, pruned_ids[other]) != 0) {
                free(active_ids);
                free(inserted);
                free(order);
                free(pruned_ids);
                free(pool);
                return -1;
            }
        }
    }

    free(active_ids);
    free(inserted);
    free(order);
    free(pruned_ids);
    free(pool);
    index->built = true;
    return 0;
}