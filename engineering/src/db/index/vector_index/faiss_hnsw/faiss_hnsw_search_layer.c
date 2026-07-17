// faiss_hnsw_search_layer.c
// 实现 faiss_hnsw_search_layer：HNSW 核心搜索层算法
// 与 FAISS HNSW::search_layer_to_add / search_from_candidates 语义一致
//
// 算法流程（参考 FAISS HNSW.cpp:search_from_candidates）：
//   1. 从 entry_point 开始，将 (ep, dist) 压入 MinimaxHeap
//   2. 循环：
//      - 弹出堆顶最小距离 cur（MinimaxHeap 是大顶堆 + 维护 nvalid，pop_min 找最小）
//      - 获取 cur 在 level 层的所有邻居
//      - 对每个未访问的邻居 nbr：计算距离，push 到堆
//   3. 收集堆中所有候选作为结果
//
// 关键不变量：
//   - 堆已满（nvalid == n）时，新候选 dist 必须 < max 才插入
//   - 邻居数组通过 cum_nneighbor_per_level 偏移定位每层

#include "faiss_hnsw_internal.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

// =============================================================================
// 内部辅助函数
// =============================================================================

// 计算两点距离（L2 平方 / Cosine）
// 与 FAISS 行为一致：L2 时返回平方距离（用于排序无影响）
static float compute_distance(const faiss_hnsw_t *idx, const float *query, int32_t vec_id) {
    if (vec_id < 0 || vec_id >= idx->n_total || !idx->vectors || !query) {
        return FLT_MAX;
    }

    const float *v = idx->vectors + (size_t)vec_id * (size_t)idx->dims;
    float dist = 0.0f;

    if (idx->metric == DISTANCE_METRIC_L2_SQUARED) {
        for (int32_t i = 0; i < idx->dims; i++) {
            float d = query[i] - v[i];
            dist += d * d;
        }
    } else if (idx->metric == DISTANCE_METRIC_COSINE) {
        // cosine 距离：1 - cos(q, v) = 1 - dot(q, v) / (||q|| * ||v||)
        float dot = 0.0f, norm_q = 0.0f, norm_v = 0.0f;
        for (int32_t i = 0; i < idx->dims; i++) {
            dot += query[i] * v[i];
            norm_q += query[i] * query[i];
            norm_v += v[i] * v[i];
        }
        if (norm_q > 0.0f && norm_v > 0.0f) {
            dist = 1.0f - dot / (sqrtf(norm_q) * sqrtf(norm_v));
        } else {
            dist = 1.0f;
        }
    } else {
        // 内积 / 汉明等其他度量：fallback 到 L2 平方
        for (int32_t i = 0; i < idx->dims; i++) {
            float d = query[i] - v[i];
            dist += d * d;
        }
    }
    return dist;
}

// 获取第 level 层第 i 个邻居的 vec_id
// 内部布局：idx->neighbors 是扁平数组，idx->offsets[vec_id] 是 vec_id 邻居的起始位置
//           idx->cum_nneighbor[level-1] 是 level 层的起始偏移（level=0 时偏移=0）
static int32_t get_neighbor(const faiss_hnsw_t *idx, int32_t vec_id, int32_t level, int32_t i) {
    if (!idx || !idx->neighbors || !idx->offsets || !idx->cum_nneighbor) {
        return -1;
    }
    if (vec_id < 0 || vec_id >= idx->n_total) {
        return -1;
    }

    int32_t level_offset = (level > 0) ? idx->cum_nneighbor[level - 1] : 0;
    int32_t vec_offset = idx->offsets[vec_id];
    return idx->neighbors[vec_offset + level_offset + i];
}

// 获取 vec_id 在 level 层的有效邻居数
// 邻居数组以 -1 结尾（与 FAISS HNSW 一致）
static int32_t get_neighbor_count(const faiss_hnsw_t *idx, int32_t vec_id, int32_t level) {
    if (!idx || !idx->levels || vec_id < 0) {
        return 0;
    }

    int32_t vec_level = idx->levels[vec_id];
    if (level > vec_level) {
        return 0;  // 该节点在此层不存在
    }

    // level 0 的最大邻居数为 2*M，其他层为 M
    int32_t max_neighbors = (level == 0) ? (2 * idx->M) : idx->M;

    int32_t count = 0;
    for (int32_t i = 0; i < max_neighbors; i++) {
        int32_t n = get_neighbor(idx, vec_id, level, i);
        if (n >= 0) {
            count++;
        } else {
            break;  // 遇到 -1 即终止
        }
    }
    return count;
}

// =============================================================================
// 公共 API：faiss_hnsw_search_layer
// =============================================================================

int32_t faiss_hnsw_search_layer(const faiss_hnsw_t *idx, int32_t level, const float *query,
                                 int32_t ef, int32_t *result_ids, float *result_dist,
                                 int32_t result_capacity) {
    // 参数校验
    if (!idx || !query || !result_ids || !result_dist) {
        return -1;
    }
    if (ef <= 0 || result_capacity <= 0) {
        return -1;
    }

    // 空索引或无入口点
    if (idx->n_total == 0 || idx->entry_point < 0) {
        for (int32_t i = 0; i < result_capacity; i++) {
            result_ids[i] = -1;
            result_dist[i] = FLT_MAX;
        }
        return 0;
    }

    // 1. 创建 MinimaxHeap（容量 ef）
    faiss_hnsw_minimax_heap_t *heap = NULL;
    if (faiss_hnsw_minimax_heap_create(&heap, ef) != 0 || !heap) {
        return -1;
    }

    // 2. 创建 VisitedTable（大小足够覆盖所有可能的 vec_id）
    faiss_hnsw_visited_table_t *visited = NULL;
    int32_t vt_size = idx->n_total + ef + 100;
    if (faiss_hnsw_visited_table_create(&visited, vt_size) != 0 || !visited) {
        faiss_hnsw_minimax_heap_drop(heap);
        return -1;
    }

    // 3. 从 entry_point 开始
    int32_t ep = idx->entry_point;
    float ep_dist = compute_distance(idx, query, ep);
    faiss_hnsw_minimax_heap_push(heap, ep, ep_dist);
    faiss_hnsw_visited_table_set(visited, ep);

    // 4. 贪婪向下搜索
    // 循环：弹出当前最小距离的候选，扩展其邻居
    while (faiss_hnsw_minimax_heap_size(heap) > 0) {
        float cur_dist = 0.0f;
        int32_t cur_id = faiss_hnsw_minimax_heap_pop_min(heap, &cur_dist);
        if (cur_id < 0) {
            break;
        }

        // 获取当前节点在 level 层的邻居
        int32_t neighbor_count = get_neighbor_count(idx, cur_id, level);
        for (int32_t i = 0; i < neighbor_count; i++) {
            int32_t nbr = get_neighbor(idx, cur_id, level, i);
            if (nbr < 0) {
                continue;
            }
            // 边界检查：避免访问未分配的 vec_id
            if (nbr >= vt_size) {
                continue;
            }
            // 跳过已访问节点
            if (faiss_hnsw_visited_table_get(visited, nbr)) {
                continue;
            }

            faiss_hnsw_visited_table_set(visited, nbr);
            float nbr_dist = compute_distance(idx, query, nbr);

            // push 内部自动处理堆满时的替换：
            //   - 堆未满：直接插入
            //   - 堆已满：新 dist >= max 时丢弃；否则弹出 max 再插入
            faiss_hnsw_minimax_heap_push(heap, nbr, nbr_dist);
        }
    }

    // 5. 收集结果
    int32_t result_count = 0;
    while (result_count < result_capacity && faiss_hnsw_minimax_heap_size(heap) > 0) {
        float dist = 0.0f;
        int32_t id = faiss_hnsw_minimax_heap_pop_min(heap, &dist);
        if (id >= 0) {
            result_ids[result_count] = id;
            result_dist[result_count] = dist;
            result_count++;
        }
    }

    // 填充剩余位置
    for (int32_t i = result_count; i < result_capacity; i++) {
        result_ids[i] = -1;
        result_dist[i] = FLT_MAX;
    }

    // 6. 清理
    faiss_hnsw_minimax_heap_drop(heap);
    faiss_hnsw_visited_table_drop(visited);

    return result_count;
}
