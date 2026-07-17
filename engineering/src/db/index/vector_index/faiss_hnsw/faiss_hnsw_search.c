// faiss_hnsw_search.c
// 实现 faiss_hnsw_index_search 搜索接口
// 参考 FAISS HNSW.cpp 中的 HNSW::search 方法
// 完整重写为纯 C 版本
//
// 搜索流程（与 FAISS HNSW::search 一致）：
// 1. 从 entry_point 开始，逐层向下搜索（贪婪下降）
// 2. 底层（level 0）调用 search_layer 进行 beam search
// 3. 对 beam search 结果排序，取 top k

#include "faiss_hnsw_internal.h"

#include <float.h>

// =============================================================================
// 内部辅助函数（自包含，避免依赖其他 .c 的静态函数）
// =============================================================================

// 计算查询向量到索引中指定向量的距离
// 与 faiss_hnsw_search_layer.c 中的实现行为一致
static float compute_distance_internal(faiss_hnsw_t *idx, const float *query, int32_t vec_id) {
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
static int32_t get_neighbor_internal(faiss_hnsw_t *idx, int32_t vec_id, int32_t level, int32_t i) {
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

// 获取 vec_id 在 level 层的有效邻居数（邻居以 -1 结尾）
static int32_t get_neighbor_count_internal(faiss_hnsw_t *idx, int32_t vec_id, int32_t level) {
    if (!idx || !idx->levels || vec_id < 0) {
        return 0;
    }

    int32_t vec_level = idx->levels[vec_id];
    if (level > vec_level) {
        return 0;
    }

    int32_t max_neighbors = (level == 0) ? (2 * idx->M) : idx->M;

    int32_t count = 0;
    for (int32_t i = 0; i < max_neighbors; i++) {
        int32_t n = get_neighbor_internal(idx, vec_id, level, i);
        if (n >= 0) {
            count++;
        } else {
            break;
        }
    }
    return count;
}

// =============================================================================
// faiss_hnsw_index_search
// 主搜索接口
// =============================================================================
int32_t faiss_hnsw_index_search(faiss_hnsw_t *idx, const float *query, int32_t k,
                                int32_t ef_search, float *distances, int32_t *ids) {
    if (!idx || k <= 0 || !query || !distances || !ids) {
        return -1;
    }

    // 空索引保护
    if (idx->n_total == 0 || idx->entry_point < 0) {
        for (int i = 0; i < k; i++) {
            ids[i] = -1;
            distances[i] = FLT_MAX;
        }
        return 0;
    }

    // 初始化结果
    for (int i = 0; i < k; i++) {
        ids[i] = -1;
        distances[i] = FLT_MAX;
    }

    // 使用足够大的 ef（与 FAISS 行为一致）
    int32_t ef = (ef_search > k) ? ef_search : k;
    if (ef < 16) {
        ef = 16;
    }

    // 从 entry_point 开始，逐层向下搜索（贪婪下降）
    int32_t cur = idx->entry_point;
    float cur_dist = compute_distance_internal(idx, query, cur);

    // 从 max_level 下降到 1（上层仅做贪婪下降，不需要 ef 个候选）
    for (int32_t level = idx->max_level; level >= 1; level--) {
        // 贪婪向下搜索：每层找最近的邻居，直到不能再前进
        while (1) {
            int32_t best_nbr = -1;
            float best_dist = FLT_MAX;
            int32_t neighbor_count = get_neighbor_count_internal(idx, cur, level);

            for (int i = 0; i < neighbor_count; i++) {
                int32_t nbr = get_neighbor_internal(idx, cur, level, i);
                if (nbr < 0) {
                    break;
                }
                float d = compute_distance_internal(idx, query, nbr);
                if (d < best_dist) {
                    best_dist = d;
                    best_nbr = nbr;
                }
            }

            if (best_nbr < 0 || best_dist >= cur_dist) {
                break;
            }
            cur = best_nbr;
            cur_dist = best_dist;
        }
    }

    // 在 level 0 调用 faiss_hnsw_search_layer 进行 beam search
    int32_t *layer_result_ids = (int32_t *)malloc(sizeof(int32_t) * (size_t)ef);
    float *layer_result_dists = (float *)malloc(sizeof(float) * (size_t)ef);
    if (!layer_result_ids || !layer_result_dists) {
        free(layer_result_ids);
        free(layer_result_dists);
        return -1;
    }
    for (int i = 0; i < ef; i++) {
        layer_result_ids[i] = -1;
        layer_result_dists[i] = FLT_MAX;
    }

    // 调用公共 search_layer（在最底层 layer 0 全局搜索 ef 个候选）
    int32_t n_results = faiss_hnsw_search_layer(idx, 0, query, ef,
                                                 layer_result_ids, layer_result_dists, ef);

    if (n_results < 0) {
        free(layer_result_ids);
        free(layer_result_dists);
        return -1;
    }

    // 对 layer 结果按距离升序排序，取 top k
    // 简单选择排序（O(n^2)，适合小 ef）
    for (int i = 0; i < n_results - 1 && i < k; i++) {
        int32_t best_idx = i;
        float best_dist = layer_result_dists[i];
        for (int j = i + 1; j < n_results; j++) {
            if (layer_result_dists[j] < best_dist) {
                best_dist = layer_result_dists[j];
                best_idx = j;
            }
        }
        if (best_idx != i) {
            int32_t tmp_id = layer_result_ids[i];
            float tmp_dist = layer_result_dists[i];
            layer_result_ids[i] = layer_result_ids[best_idx];
            layer_result_dists[i] = layer_result_dists[best_idx];
            layer_result_ids[best_idx] = tmp_id;
            layer_result_dists[best_idx] = tmp_dist;
        }
    }

    // 取前 k 个结果
    int32_t result_count = (n_results < k) ? n_results : k;
    for (int i = 0; i < result_count; i++) {
        ids[i] = layer_result_ids[i];
        distances[i] = layer_result_dists[i];
    }
    for (int i = result_count; i < k; i++) {
        ids[i] = -1;
        distances[i] = FLT_MAX;
    }

    free(layer_result_ids);
    free(layer_result_dists);

    return result_count;
}
