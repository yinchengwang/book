/* diskann index delete */

#include "diskann_private.h"

int32_t diskann_index_delete(diskann_t *index, int32_t label)
{
    bool *affected;
    int32_t *victim_neighbors;
    int32_t victim_neighbor_count;
    int32_t affected_count;
    int32_t status;
    int32_t i;

    if (!index || label < 0 || label >= index->n_total || index->deleted[label]) {
        return -1;
    }

    /* 第一步: 记录受影响节点集合，以及被删节点原有的邻居列表。 */
    affected = (bool *)calloc((size_t)index->n_total, sizeof(bool));
    victim_neighbors = (int32_t *)malloc((size_t)index->index_size * sizeof(int32_t));
    if (!affected || !victim_neighbors) {
        free(affected);
        free(victim_neighbors);
        return -1;
    }

    /* 第二步: 保存被删节点的直接邻居，这些节点最可能需要重连。 */
    victim_neighbor_count = 0;
    for (i = 0; i < index->neighbor_counts[label]; ++i) {
        int32_t neighbor = index->neighbors[(size_t)label * (size_t)index->index_size + i];
        if (neighbor >= 0 && !index->deleted[neighbor]) {
            victim_neighbors[victim_neighbor_count++] = neighbor;
            affected[neighbor] = true;
        }
    }

    /* 第三步: 将节点标记删除，并清空自己的邻接行。 */
    index->deleted[label] = 1;
    index->active_count -= 1;
    index->neighbor_counts[label] = 0;
    memset(index->neighbors + (size_t)label * (size_t)index->index_size,
           -1,
           (size_t)index->index_size * sizeof(int32_t));

    /* 第四步: 从其他节点的邻居列表里移除该 id，并继续扩展受影响集合。 */
    for (i = 0; i < index->n_total; ++i) {
        if (!index->deleted[i]) {
            if (diskann_has_neighbor(index, i, label)) {
                affected[i] = true;
            }
            diskann_remove_id_from_row(index, i, label);
        }
    }

    /* 第五步: 特殊情况，删除后图为空，直接重置入口与 frozen points。 */
    if (index->active_count <= 0) {
        index->entry_point = -1;
        index->built = true;
        for (i = 0; i < index->storage_params.frozen_point_count; ++i) {
            index->frozen_points[i] = -1;
        }
        free(affected);
        free(victim_neighbors);
        return 0;
    }

    affected_count = 0;
    for (i = 0; i < index->n_total; ++i) {
        if (affected[i] && !index->deleted[i]) {
            affected_count += 1;
        }
    }

    /* 第六步: 对受影响节点做邻接修复；必要时会退化为整图重建。 */
    status = diskann_repair_neighbors_after_delete(index,
                                                   affected,
                                                   affected_count,
                                                   victim_neighbors,
                                                   victim_neighbor_count);
    free(affected);
    free(victim_neighbors);
    return status;
}