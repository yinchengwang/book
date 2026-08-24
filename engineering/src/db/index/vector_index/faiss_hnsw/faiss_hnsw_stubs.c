// faiss_hnsw_stubs.c
// 实现 faiss_hnsw_index_add / faiss_hnsw_index_size / faiss_hnsw_index_max_level
// 参考 FAISS HNSW.cpp 中的 add_with_ids / search_level / add_links_new_node 逻辑
//
// HNSW 插入算法核心流程（与 FAISS 一致）：
//   1. 分配向量 ID（n_total++），拷贝向量数据
//   2. 用 faiss_hnsw_random_level 随机分配层号
//   3. 从 entry_point 贪婪下降到目标层（上层仅贪婪，无 ef 候选）
//   4. 从目标层逐层向下搜索 ef_construction 个候选
//   5. 在每一层建立双向连接，超出容量时 shrink
//   6. 更新 entry_point 和 max_level

#include "faiss_hnsw_internal.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

// =============================================================================
// 内部辅助：距离计算
// =============================================================================

static float hnsw_vec_dist(const faiss_hnsw_t *idx, const float *a, const float *b) {
    float dist = 0.0f;
    if (idx->metric == DISTANCE_METRIC_L2_SQUARED) {
        for (int32_t i = 0; i < idx->dims; i++) {
            float d = a[i] - b[i];
            dist += d * d;
        }
    } else if (idx->metric == DISTANCE_METRIC_COSINE) {
        float dot = 0.0f, na = 0.0f, nb = 0.0f;
        for (int32_t i = 0; i < idx->dims; i++) {
            dot += a[i] * b[i];
            na  += a[i] * a[i];
            nb  += b[i] * b[i];
        }
        if (na > 0.0f && nb > 0.0f) {
            dist = 1.0f - dot / (sqrtf(na) * sqrtf(nb));
        } else {
            dist = 1.0f;
        }
    } else {
        for (int32_t i = 0; i < idx->dims; i++) {
            float d = a[i] - b[i];
            dist += d * d;
        }
    }
    return dist;
}

static float hnsw_dist_by_id(const faiss_hnsw_t *idx, int32_t a, int32_t b) {
    if (a < 0 || b < 0 || a >= idx->n_total || b >= idx->n_total) return FLT_MAX;
    return hnsw_vec_dist(idx,
                         idx->vectors + (size_t)a * (size_t)idx->dims,
                         idx->vectors + (size_t)b * (size_t)idx->dims);
}

static float hnsw_dist_query(const faiss_hnsw_t *idx, const float *query, int32_t vec_id) {
    if (vec_id < 0 || vec_id >= idx->n_total) return FLT_MAX;
    return hnsw_vec_dist(idx, query, idx->vectors + (size_t)vec_id * (size_t)idx->dims);
}

// =============================================================================
// 内部辅助：邻居数组操作
// =============================================================================

static int32_t neighbor_offset(const faiss_hnsw_t *idx, int32_t vec_id, int32_t level) {
    int32_t level_off = (level > 0) ? idx->cum_nneighbor[level - 1] : 0;
    return idx->offsets[vec_id] + level_off;
}

static int32_t get_nbr(const faiss_hnsw_t *idx, int32_t vec_id, int32_t level, int32_t i) {
    return idx->neighbors[neighbor_offset(idx, vec_id, level) + i];
}

static void set_nbr(faiss_hnsw_t *idx, int32_t vec_id, int32_t level, int32_t i, int32_t val) {
    idx->neighbors[neighbor_offset(idx, vec_id, level) + i] = val;
}

static int32_t max_neighbors(const faiss_hnsw_t *idx, int32_t level) {
    return (level == 0) ? (2 * idx->M) : idx->M;
}

static int32_t count_nbr(const faiss_hnsw_t *idx, int32_t vec_id, int32_t level) {
    int32_t cap = max_neighbors(idx, level);
    int32_t count = 0;
    for (int32_t i = 0; i < cap; i++) {
        if (get_nbr(idx, vec_id, level, i) < 0) break;
        count++;
    }
    return count;
}

static void append_nbr(faiss_hnsw_t *idx, int32_t vec_id, int32_t level, int32_t nbr_id) {
    int32_t cap = max_neighbors(idx, level);
    for (int32_t i = 0; i < cap; i++) {
        if (get_nbr(idx, vec_id, level, i) < 0) {
            set_nbr(idx, vec_id, level, i, nbr_id);
            return;
        }
    }
}

// =============================================================================
// 内部辅助：贪婪下降（高层无 ef，逐级向下找最近邻）
// =============================================================================

static void greedy_down(const faiss_hnsw_t *idx, const float *query,
                        int32_t from_level, int32_t *out_id, float *out_dist) {
    int32_t cur = *out_id;
    float cur_d = hnsw_dist_query(idx, query, cur);

    for (int32_t level = from_level; level >= 1; level--) {
        bool improved = true;
        while (improved) {
            improved = false;
            int32_t ncount = count_nbr(idx, cur, level);
            for (int32_t i = 0; i < ncount; i++) {
                int32_t nbr = get_nbr(idx, cur, level, i);
                if (nbr < 0) break;
                float nd = hnsw_dist_query(idx, query, nbr);
                if (nd < cur_d) {
                    cur_d = nd;
                    cur = nbr;
                    improved = true;
                }
            }
        }
    }
    *out_id = cur;
    *out_dist = cur_d;
}

// =============================================================================
// 内部辅助：选择最优 M 个候选（按距离排序后截取）
// =============================================================================

static int32_t select_best_m(int32_t *ids, float *dists, int32_t count,
                             int32_t max_m, int32_t *out_ids) {
    // 选择排序（候选集通常很小，ef_construction ~128）
    int32_t n = (count < max_m) ? count : max_m;
    for (int32_t i = 0; i < n; i++) {
        int32_t best = i;
        for (int32_t j = i + 1; j < count; j++) {
            if (dists[j] < dists[best]) best = j;
        }
        if (best != i) {
            float td = dists[i]; dists[i] = dists[best]; dists[best] = td;
            int32_t ti = ids[i]; ids[i] = ids[best]; ids[best] = ti;
        }
    }
    for (int32_t i = 0; i < n; i++) {
        out_ids[i] = ids[i];
    }
    return n;
}

// =============================================================================
// faiss_hnsw_index_add
//
// 核心算法（与 FAISS HNSW::add_with_ids 一致）：
//   对每个新向量 x：
//     1. n_total++，拷贝向量数据到 vectors 数组
//     2. 分配随机层号 level = random_level()
//     3. 从 entry_point 贪婪下降到 level+1（上层仅贪婪搜索最近邻）
//     4. 从 level 向下到 0，逐层搜索 ef_construction 个候选
//     5. 在每一层连接双向边，超出容量时 shrink
//     6. 如果新节点层号 > max_level，更新 entry_point
//
// 性能关键：add() 内部使用 capacity 字段避免每向量 realloc。
//   - 首次插入：懒分配（最小粒度 1024）
//   - 容量不足时：几何级数翻倍（O(log N) 次 realloc，而非 N 次）
//   - 建议在 bulk-load 前调用 faiss_hnsw_index_reserve(idx, N) 一次到位
// =============================================================================

// 内部辅助：扩容节点数组（vectors/levels/delete_bitmap/offsets）
// 失败时不修改任何字段（保持原 capacity 不变）
static int hnsw_grow_nodes_to(faiss_hnsw_t *idx, int32_t new_cap) {
    if (new_cap <= idx->capacity) return 0;

    int32_t dims = idx->dims;
    float *nv = (float *)realloc(idx->vectors,
                                  (size_t)new_cap * (size_t)dims * sizeof(float));
    if (!nv) return -1;
    idx->vectors = nv;

    int32_t *nl = (int32_t *)realloc(idx->levels, (size_t)new_cap * sizeof(int32_t));
    if (!nl) return -1;
    idx->levels = nl;

    uint8_t *nb = (uint8_t *)realloc(idx->delete_bitmap, (size_t)new_cap * sizeof(uint8_t));
    if (!nb) return -1;
    idx->delete_bitmap = nb;

    int32_t *no = (int32_t *)realloc(idx->offsets, (size_t)new_cap * sizeof(int32_t));
    if (!no) return -1;
    idx->offsets = no;

    idx->capacity = new_cap;
    return 0;
}

// 内部辅助：扩容 neighbors 数组
// 失败时不修改任何字段（保持原 neighbors_capacity 不变）
static int hnsw_grow_neighbors_to(faiss_hnsw_t *idx, int32_t new_cap) {
    if (new_cap <= idx->neighbors_capacity) return 0;

    int32_t *nn = (int32_t *)realloc(idx->neighbors, (size_t)new_cap * sizeof(int32_t));
    if (!nn) return -1;
    idx->neighbors = nn;
    idx->neighbors_capacity = new_cap;
    return 0;
}

// 计算下一次节点容量（几何级数翻倍，最小 1024）
static int32_t hnsw_next_node_capacity(int32_t current, int32_t needed) {
    int32_t cap = current > 0 ? current : 0;
    if (cap < 1024) cap = 1024;
    while (cap < needed) cap *= 2;
    return cap;
}

// 预分配索引容量（add 前调用，把 realloc 总开销从 O(N) 降到 O(1)）
int32_t faiss_hnsw_index_reserve(faiss_hnsw_t *idx, int32_t n) {
    if (!idx || n <= 0) return -1;

    int32_t old_cap = idx->capacity;
    if (n > idx->capacity) {
        if (hnsw_grow_nodes_to(idx, n) != 0) return -1;
    }

    // neighbors 容量估算：平均每个节点占用 cum_nneighbor[2] 槽
    // （cum_nneighbor[2] = 4M，绝大多数节点层数 <= 2，少数高层节点触发 add 内增量扩容）
    int32_t level_for_est = (idx->cum_nneighbor_size > 2) ? 2 : 0;
    int32_t est_per_node = idx->cum_nneighbor[level_for_est];
    int32_t nb_target = n * est_per_node;
    if (nb_target > idx->neighbors_capacity) {
        // 扩容前保留旧容量，失败则回滚 nodes
        if (hnsw_grow_neighbors_to(idx, nb_target) != 0) {
            if (old_cap > 0) hnsw_grow_nodes_to(idx, old_cap);
            return -1;
        }
    }
    return 0;
}

int32_t faiss_hnsw_index_add(faiss_hnsw_t *idx, int32_t n, const float *vectors) {
    if (!idx || n <= 0 || !vectors) return -1;

    int32_t dims = idx->dims;
    int32_t added = 0;

    for (int32_t xi = 0; xi < n; xi++) {
        const float *vec = vectors + (size_t)xi * (size_t)dims;

        // ---- Step 1: 分配 ID，确保 node 容量充足 ----
        int32_t new_id = idx->n_total;
        idx->n_total++;

        if (idx->n_total > idx->capacity) {
            int32_t new_cap = hnsw_next_node_capacity(idx->capacity, idx->n_total);
            if (hnsw_grow_nodes_to(idx, new_cap) != 0) {
                idx->n_total--;
                break;
            }
        }

        // 拷贝向量 + 初始化 bitmap（无需 realloc，因为容量已预留）
        memcpy(idx->vectors + (size_t)new_id * (size_t)dims,
               vec, (size_t)dims * sizeof(float));
        idx->delete_bitmap[new_id] = 0;

        // 分配层号
        int32_t node_level = faiss_hnsw_random_level(idx, new_id);
        idx->levels[new_id] = node_level;

        // 计算该节点需要的邻居空间
        int32_t needed_capacity = idx->cum_nneighbor[node_level];

        // 设置 offsets
        if (new_id == 0) {
            idx->offsets[0] = 0;
        } else {
            int32_t prev_level = idx->levels[new_id - 1];
            int32_t prev_cap = idx->cum_nneighbor[prev_level];
            idx->offsets[new_id] = idx->offsets[new_id - 1] + prev_cap;
        }

        // 确保 neighbors 容量充足（罕见的高层节点会触发此次扩容）
        int32_t new_total = idx->offsets[new_id] + needed_capacity;
        if (new_total > idx->neighbors_capacity) {
            int32_t new_cap = idx->neighbors_capacity > 0 ? idx->neighbors_capacity : 0;
            if (new_cap < 1024) new_cap = 1024;
            while (new_cap < new_total) new_cap *= 2;
            if (hnsw_grow_neighbors_to(idx, new_cap) != 0) {
                idx->n_total--;
                break;
            }
        }

        // 初始化邻居为 -1
        for (int32_t i = 0; i < needed_capacity; i++) {
            idx->neighbors[idx->offsets[new_id] + i] = -1;
        }

        // ---- Step 2: 空索引（第一个向量） ----
        if (new_id == 0) {
            idx->entry_point = 0;
            idx->max_level = node_level;
            added++;
            continue;
        }

        // ---- Step 3: 从 entry_point 贪婪下降到 node_level ----
        int32_t cur = idx->entry_point;
        float cur_dist = hnsw_dist_query(idx, vec, cur);

        // 贪婪下降
        {
            int32_t s_cur = cur;
            float s_dist = cur_dist;
            greedy_down(idx, vec, idx->max_level, &s_cur, &s_dist);
            cur = s_cur;
            cur_dist = s_dist;
        }

        // ---- Step 4: 从 node_level（或 max_level）向下逐层搜索并连接 ----
        int32_t ef = idx->ef_construction;
        int32_t start_level = (node_level < idx->max_level) ? node_level : idx->max_level;

        for (int32_t level = start_level; level >= 0; level--) {
            // 在当前层搜索 ef 个候选
            int32_t *cand_ids = (int32_t *)malloc(sizeof(int32_t) * (size_t)ef);
            float *cand_dists = (float *)malloc(sizeof(float) * (size_t)ef);
            if (!cand_ids || !cand_dists) {
                free(cand_ids);
                free(cand_dists);
                break;
            }
            for (int32_t i = 0; i < ef; i++) {
                cand_ids[i] = -1;
                cand_dists[i] = FLT_MAX;
            }

            int32_t found = faiss_hnsw_search_layer(idx, level, vec, ef,
                                                     cur, cur_dist,
                                                     cand_ids, cand_dists, ef);

            // 选择最优 M 个
            int32_t target_m = max_neighbors(idx, level);
            int32_t *selected = (int32_t *)malloc(sizeof(int32_t) * (size_t)target_m);
            if (!selected) {
                free(cand_ids);
                free(cand_dists);
                break;
            }
            int32_t sel_count = select_best_m(cand_ids, cand_dists, found, target_m, selected);

            // 保存最近候选（下一层搜索用）
            int32_t best_cand = (found > 0) ? cand_ids[0] : cur;

            // 连接双向边
            for (int32_t i = 0; i < sel_count; i++) {
                int32_t nbr = selected[i];
                if (nbr < 0) continue;

                // 新节点 -> 邻居
                append_nbr(idx, new_id, level, nbr);

                // 邻居 -> 新节点
                int32_t nbr_cap = max_neighbors(idx, level);
                int32_t nbr_cnt = count_nbr(idx, nbr, level);
                if (nbr_cnt < nbr_cap) {
                    append_nbr(idx, nbr, level, new_id);
                } else {
                    // 满了：如果新节点比最远邻居更近则替换
                    int32_t worst_j = -1;
                    float worst_d = -1.0f;
                    for (int32_t j = 0; j < nbr_cap; j++) {
                        int32_t nb = get_nbr(idx, nbr, level, j);
                        if (nb < 0) break;
                        float d = hnsw_dist_by_id(idx, nbr, nb);
                        if (d > worst_d) { worst_d = d; worst_j = j; }
                    }
                    float new_d = hnsw_dist_by_id(idx, nbr, new_id);
                    if (new_d < worst_d && worst_j >= 0) {
                        set_nbr(idx, nbr, level, worst_j, new_id);
                    }
                }
            }

            free(selected);
            free(cand_ids);
            free(cand_dists);

            // 更新 cur 为最近候选（供下一层搜索起点）
            cur = best_cand;
        }

        // ---- Step 5: 更新 entry_point 和 max_level ----
        if (node_level > idx->max_level) {
            idx->entry_point = new_id;
            idx->max_level = node_level;
        }

        added++;
    }

    return added;
}

// =============================================================================
// faiss_hnsw_index_size
// 返回索引当前已插入的向量数量
// =============================================================================
int32_t faiss_hnsw_index_size(const faiss_hnsw_t *index) {
    if (!index) return 0;
    return index->n_total;
}

// =============================================================================
// faiss_hnsw_index_max_level
// 返回索引当前的最高层号（0-indexed，空索引时为 -1）
// =============================================================================
int32_t faiss_hnsw_index_max_level(const faiss_hnsw_t *index) {
    if (!index) return -1;
    return index->max_level;
}
