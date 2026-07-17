// faiss_hnsw_add.c
// 实现 faiss_hnsw_index_add：向 HNSW 索引批量添加向量
// 参考 FAISS HNSW::add / HNSW::add_with_ids 实现
//
// 数据结构约定：
//   - vectors[vec_id * dims]  → 第 vec_id 个向量的全部坐标
//   - levels[vec_id]          → 第 vec_id 个向量所在的最高层号
//   - offsets[vec_id]         → 第 vec_id 个向量在 neighbors 数组中的起始位置
//   - neighbors[]             → 所有向量的邻居扁平存储
//   每个向量的邻居数 = (vec_id+1 == n_total ? neighbors_size : offsets[vec_id+1]) - offsets[vec_id]
//
// 本版本实现了：
//   1) 向量存储与元数据扩展（levels / offsets / delete_bitmap）
//   2) 层级分配（faiss_hnsw_random_level）
//   3) 暴力最近邻搜索（适用于中小规模数据集）
//   4) 邻居选择（取 M0 个最近邻）

#include "faiss_hnsw_internal.h"

#include <stdio.h>

// L2 平方距离
static inline float l2_sq(const float *a, const float *b, int32_t d) {
    float s = 0.0f;
    for (int32_t i = 0; i < d; i++) {
        float diff = a[i] - b[i];
        s += diff * diff;
    }
    return s;
}

// 暴力找 ef 个最近邻（基于已有向量）
// 返回实际找到的候选数（<= ef）
static int32_t brute_search_ef(const faiss_hnsw_t *idx, const float *query,
                                int32_t ef, int32_t *out_ids, float *out_dists) {
    int32_t n = idx->n_total;
    if (n <= 0) return 0;

    // 计算所有距离
    int32_t take = ef < n ? ef : n;
    float *all_dists = (float *)malloc((size_t)n * sizeof(float));
    int32_t *all_ids = (int32_t *)malloc((size_t)n * sizeof(int32_t));
    if (!all_dists || !all_ids) {
        free(all_dists);
        free(all_ids);
        return 0;
    }

    for (int32_t i = 0; i < n; i++) {
        all_ids[i] = i;
        all_dists[i] = l2_sq(query, idx->vectors + (size_t)i * (size_t)idx->dims, idx->dims);
    }

    // 部分选择：取 take 个最小距离
    int32_t *used = (int32_t *)calloc((size_t)n, sizeof(int32_t));
    if (!used) {
        free(all_dists);
        free(all_ids);
        return 0;
    }

    int32_t found = 0;
    for (int32_t k = 0; k < take; k++) {
        float best = 1e30f;
        int32_t best_idx = -1;
        for (int32_t i = 0; i < n; i++) {
            if (used[i]) continue;
            if (all_dists[i] < best) {
                best = all_dists[i];
                best_idx = i;
            }
        }
        if (best_idx < 0) break;
        used[best_idx] = 1;
        out_ids[found] = all_ids[best_idx];
        out_dists[found] = best;
        found++;
    }

    free(used);
    free(all_dists);
    free(all_ids);
    return found;
}

int32_t faiss_hnsw_index_add(faiss_hnsw_t *index, int32_t n, const float *vectors) {
    // 参数校验
    if (!index || n <= 0 || !vectors) {
        return -1;
    }
    if (index->dims <= 0) {
        return -1;
    }

    const int32_t dims = index->dims;
    const int32_t M = index->M;
    const int32_t M0 = 2 * M;

    int32_t old_total = index->n_total;
    int32_t new_total = old_total + n;

    // ---------------------------------------------------------------------
    // 1. 扩展 vectors 数组（存储所有向量坐标）
    // ---------------------------------------------------------------------
    {
        size_t vec_bytes = (size_t)new_total * (size_t)dims * sizeof(float);
        float *p = (float *)realloc(index->vectors, vec_bytes);
        if (!p) return -1;
        index->vectors = p;
        memcpy(index->vectors + (size_t)old_total * (size_t)dims,
               vectors, (size_t)n * (size_t)dims * sizeof(float));
    }

    // ---------------------------------------------------------------------
    // 2. 扩展 levels 数组（每个向量的最高层号）
    // ---------------------------------------------------------------------
    {
        int32_t *p = (int32_t *)realloc(index->levels,
                                         (size_t)new_total * sizeof(int32_t));
        if (!p) return -1;
        index->levels = p;
    }

    // ---------------------------------------------------------------------
    // 3. 扩展 offsets 数组（每个向量在 neighbors 中的起始位置）
    // ---------------------------------------------------------------------
    {
        int32_t *p = (int32_t *)realloc(index->offsets,
                                         (size_t)new_total * sizeof(int32_t));
        if (!p) return -1;
        index->offsets = p;
        // 新向量的初始 offset 暂时设为 0，将在写入邻居后修正
        for (int32_t i = old_total; i < new_total; i++) {
            index->offsets[i] = 0;
        }
    }

    // ---------------------------------------------------------------------
    // 4. 扩展 delete_bitmap
    // ---------------------------------------------------------------------
    {
        uint8_t *p = (uint8_t *)realloc(index->delete_bitmap,
                                         (size_t)new_total * sizeof(uint8_t));
        if (!p) return -1;
        index->delete_bitmap = p;
        for (int32_t i = old_total; i < new_total; i++) {
            index->delete_bitmap[i] = 0;
        }
    }

    // ---------------------------------------------------------------------
    // 5. 临时缓冲：存储每个新向量的候选邻居
    //    counts[vec_id] = 该向量的邻居数
    // ---------------------------------------------------------------------
    int32_t *counts = (int32_t *)calloc((size_t)new_total, sizeof(int32_t));
    if (!counts) return -1;

    int32_t *candidate_ids = (int32_t *)malloc((size_t)M0 * sizeof(int32_t));
    float *candidate_dists = (float *)malloc((size_t)M0 * sizeof(float));
    if (!candidate_ids || !candidate_dists) {
        free(counts);
        free(candidate_ids);
        free(candidate_dists);
        return -1;
    }

    int32_t ef = index->ef_construction;
    if (ef < M0) ef = M0;

    int32_t *search_buf = (int32_t *)malloc((size_t)ef * sizeof(int32_t));
    float *search_dists = (float *)malloc((size_t)ef * sizeof(float));
    if (!search_buf || !search_dists) {
        free(counts);
        free(candidate_ids);
        free(candidate_dists);
        free(search_buf);
        free(search_dists);
        return -1;
    }

    // ---------------------------------------------------------------------
    // 6. 逐个向量分配层级 + 选邻居
    // ---------------------------------------------------------------------
    int32_t added_count = 0;

    for (int32_t i = 0; i < n; i++) {
        int32_t vec_id = old_total + i;
        const float *vec = vectors + (size_t)i * (size_t)dims;

        // 6.1 层级分配
        int32_t level = faiss_hnsw_random_level(index, vec_id);
        if (level < 0) level = 0;
        index->levels[vec_id] = level;

        // 6.2 更新 max_level / entry_point
        if (level > index->max_level) {
            index->max_level = level;
            index->entry_point = vec_id;
        }

        // 6.3 邻居选择（暴力 ef-近邻 + 取前 M0）
        int32_t n_candidates = 0;
        if (old_total + added_count > 0) {
            // 临时让 n_total 反映"当前可用候选数"（不含自身）
            int32_t saved_total = index->n_total;
            index->n_total = old_total + added_count;
            n_candidates = brute_search_ef(index, vec, ef, search_buf, search_dists);
            index->n_total = saved_total;
        }

        int32_t actual_neighbors = 0;
        if (n_candidates > 0) {
            // 从 n_candidates 个候选中选 M0 个最近
            int32_t take = n_candidates < M0 ? n_candidates : M0;
            int32_t *used = (int32_t *)calloc((size_t)n_candidates, sizeof(int32_t));
            if (!used) {
                free(counts);
                free(candidate_ids);
                free(candidate_dists);
                free(search_buf);
                free(search_dists);
                return added_count > 0 ? added_count : -1;
            }
            for (int32_t k = 0; k < take; k++) {
                float best = 1e30f;
                int32_t best_idx = -1;
                for (int32_t j = 0; j < n_candidates; j++) {
                    if (used[j]) continue;
                    if (search_dists[j] < best) {
                        best = search_dists[j];
                        best_idx = j;
                    }
                }
                if (best_idx < 0) break;
                used[best_idx] = 1;
                candidate_ids[actual_neighbors] = search_buf[best_idx];
                candidate_dists[actual_neighbors] = best;
                actual_neighbors++;
            }
            free(used);
        }

        counts[vec_id] = actual_neighbors;
        added_count++;
    }

    // ---------------------------------------------------------------------
    // 7. 计算每个向量的 offset（neighbors 数组中的起始位置）
    // ---------------------------------------------------------------------
    int32_t total_neighbors = 0;
    for (int32_t i = 0; i < new_total; i++) {
        index->offsets[i] = total_neighbors;
        total_neighbors += counts[i];
    }

    // ---------------------------------------------------------------------
    // 8. 分配并填充 neighbors 数组
    // ---------------------------------------------------------------------
    if (total_neighbors > 0) {
        int32_t *new_neighbors = (int32_t *)realloc(
            index->neighbors, (size_t)total_neighbors * sizeof(int32_t));
        if (!new_neighbors) {
            free(counts);
            free(candidate_ids);
            free(candidate_dists);
            free(search_buf);
            free(search_dists);
            return -1;
        }
        index->neighbors = new_neighbors;
        // 清零
        memset(index->neighbors, 0, (size_t)total_neighbors * sizeof(int32_t));
    }

    // 9. 重新遍历，填充每个向量的邻居
    //    注意：上面已经计算了 candidate_ids/dists 但只针对最后一个 vec
    //    为简化，重新执行一遍邻居搜索与填充
    //    （小数据集性能可接受；大规模应优化为单次扫描）
    int32_t fill_idx = 0;
    for (int32_t i = 0; i < n; i++) {
        int32_t vec_id = old_total + i;
        const float *vec = vectors + (size_t)i * (size_t)dims;

        if (counts[vec_id] == 0) {
            continue;  // 无邻居
        }

        // 暴力搜索 ef 个候选
        int32_t n_candidates = 0;
        if (old_total + fill_idx > 0) {
            int32_t saved_total = index->n_total;
            index->n_total = old_total + fill_idx;
            n_candidates = brute_search_ef(index, vec, ef, search_buf, search_dists);
            index->n_total = saved_total;
        }

        int32_t actual = 0;
        if (n_candidates > 0) {
            int32_t take = n_candidates < counts[vec_id] ? n_candidates : counts[vec_id];
            int32_t *used = (int32_t *)calloc((size_t)n_candidates, sizeof(int32_t));
            if (!used) break;
            for (int32_t k = 0; k < take; k++) {
                float best = 1e30f;
                int32_t best_idx = -1;
                for (int32_t j = 0; j < n_candidates; j++) {
                    if (used[j]) continue;
                    if (search_dists[j] < best) {
                        best = search_dists[j];
                        best_idx = j;
                    }
                }
                if (best_idx < 0) break;
                used[best_idx] = 1;
                index->neighbors[index->offsets[vec_id] + actual] = search_buf[best_idx];
                actual++;
            }
            free(used);
        }
        fill_idx++;
    }

    // ---------------------------------------------------------------------
    // 10. 更新 n_total
    // ---------------------------------------------------------------------
    index->n_total = new_total;

    // 清理
    free(counts);
    free(candidate_ids);
    free(candidate_dists);
    free(search_buf);
    free(search_dists);

    return added_count;
}