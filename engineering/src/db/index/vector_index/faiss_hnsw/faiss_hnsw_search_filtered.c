// faiss_hnsw_search_filtered.c
// 实现 faiss_hnsw_search_filtered 带 filter 谓词的 HNSW 搜索（P4-T4.5 + P5-3 优化）
//
// 算法流程：
//   1. 从 entry_point 贪婪下降到 level 0 的局部最优节点
//   2. 调 faiss_hnsw_search_layer(level=0, query, ef=caller_k*5+50, ...) 取候选
//   3. 对每个候选调 filter 回调（filter 非 NULL 时）；回调返回 0 的候选被丢弃
//   4. 重读 idx->vectors 中的原始向量，重算 L2 平方距离
//   5. 用 MinimaxHeap 取 top-K（P5-3：替换原有选择排序 O(K·ef) → O(K·log K)）
//
// 设计取舍：
//   - ef 使用调用方指定的 k 值（k = top_k * 50 + 50），确保足够的搜索宽度
//   - 用 faiss_hnsw_search_layer 的 beam search 距离作初筛，重算精确 L2 取 top-K
//     （避免 beam search 距离被四舍五入或量化失真影响最终排序）
//   - faiss_hnsw 模块零依赖 SQLite：filter 回调由调用方实现（典型：
//     vectors.c 中封装 filter_json 解析 + SQLite metadata 查询）

#include "faiss_hnsw_internal.h"
#include "algo-prod/distance/distance.h"

#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int32_t faiss_hnsw_search_filtered(
    faiss_hnsw_t *idx,
    const float *query,
    int32_t k,
    faiss_hnsw_filter_fn filter,
    void *user_data,
    int32_t *out_ids,
    float *out_distances) {
    // 参数校验
    if (!idx || !query || !out_ids || !out_distances) {
        return -1;
    }
    if (k <= 0) {
        return -1;
    }

    // 初始化输出
    for (int32_t i = 0; i < k; i++) {
        out_ids[i] = -1;
        out_distances[i] = FLT_MAX;
    }

    // 空索引保护
    if (idx->n_total == 0 || idx->entry_point < 0) {
        return 0;
    }

    // ef 取值：使用调用方指定的 k（search_k）按公式计算，确保足够的搜索宽度
    // 公式：ef = k * 5 + 50；小数据集 ef=n_total；ef 不得小于 k
    int32_t ef = k * 5 + 50;
    if (ef > idx->n_total) {
        ef = idx->n_total;
    }
    if (ef < k) {
        ef = k;
    }

    // 1. 从 entry_point 贪婪下降到 level 0 的局部最优节点（P5-5）
    //    与 faiss_hnsw_search.c 一致：上层仅做贪婪下降，不启动 beam search
    int32_t cur = idx->entry_point;
    float cur_dist = FLT_MAX;
    {
        // 计算 entry_point 距离（复用 distance_l2sqr_from_query，仅在 level 0 重排时用）
        // 对于非 L2 度量，这里用 L2 作近似贪婪下降（与 faiss_hnsw_search_layer 内部一致）
        const float *ep_vec = idx->vectors + (size_t)cur * (size_t)idx->dims;
        float d = 0.0f;
        for (int32_t i = 0; i < idx->dims; i++) {
            float diff = query[i] - ep_vec[i];
            d += diff * diff;
        }
        cur_dist = d;

        for (int32_t level = idx->max_level; level >= 1; level--) {
            while (1) {
                int32_t best_nbr = -1;
                float best_dist = FLT_MAX;
                int32_t nbr_count = 0;
                if (cur >= 0 && cur < idx->n_total && idx->neighbors && idx->offsets) {
                    int32_t level_offset = (level > 0) ? idx->cum_nneighbor[level - 1] : 0;
                    int32_t vec_offset = idx->offsets[cur];
                    int32_t max_nbrs = (level == 0) ? (2 * idx->M) : idx->M;
                    for (int32_t ni = 0; ni < max_nbrs; ni++) {
                        int32_t nbr = idx->neighbors[vec_offset + level_offset + ni];
                        if (nbr < 0) break;
                        const float *nbr_vec = idx->vectors + (size_t)nbr * (size_t)idx->dims;
                        float nd = 0.0f;
                        for (int32_t di = 0; di < idx->dims; di++) {
                            float diff = query[di] - nbr_vec[di];
                            nd += diff * diff;
                        }
                        if (nd < best_dist) {
                            best_dist = nd;
                            best_nbr = nbr;
                        }
                    }
                }
                if (best_nbr < 0 || best_dist >= cur_dist) break;
                cur = best_nbr;
                cur_dist = best_dist;
            }
        }
    }

    // 2. beam search 取候选（level 0 从 greedy descent 局部最优节点出发）
    int32_t *cand_ids = (int32_t *)malloc(sizeof(int32_t) * (size_t)ef);
    float *cand_dists = (float *)malloc(sizeof(float) * (size_t)ef);
    if (!cand_ids || !cand_dists) {
        free(cand_ids);
        free(cand_dists);
        return -1;
    }

    int32_t n_cands = faiss_hnsw_search_layer(
        idx, 0, query, ef, cur, cur_dist, cand_ids, cand_dists, ef);
    if (n_cands < 0) {
        free(cand_ids);
        free(cand_dists);
        return -1;
    }

    // 3. 应用 filter（filter 非 NULL 时）
    //    收集通过 filter 的候选到 compact 数组，重算 L2 后取 top-K
    int32_t *pass_ids = (int32_t *)malloc(sizeof(int32_t) * (size_t)ef);
    float *pass_dists = (float *)malloc(sizeof(float) * (size_t)ef);
    if (!pass_ids || !pass_dists) {
        free(cand_ids);
        free(cand_dists);
        free(pass_ids);
        free(pass_dists);
        return -1;
    }

    int32_t n_pass = 0;
    for (int32_t i = 0; i < n_cands; i++) {
        int32_t vid = cand_ids[i];
        if (vid < 0) continue;
        // filter 检查（NULL 时全部通过）
        if (filter && filter(vid, user_data) == 0) {
            continue;
        }
        // 重算精确 L2 距离（避免 beam search 距离近似）
        pass_ids[n_pass] = vid;
        pass_dists[n_pass] = distance_l2sqr_from_query(query, idx->vectors, idx->dims, vid);
        n_pass++;
    }

    free(cand_ids);
    free(cand_dists);

    if (n_pass == 0) {
        free(pass_ids);
        free(pass_dists);
        return 0;
    }

    // 4. P5-3 优化：用 MinimaxHeap 取 top-K，替换原有选择排序 O(K·ef) → O(K·log K)
    //    构建容量为 k 的大顶堆（CMax 语义：堆顶为最大距离）
    faiss_hnsw_minimax_heap_t* result_heap = NULL;
    if (faiss_hnsw_minimax_heap_create(&result_heap, k) != 0 || !result_heap) {
        free(pass_ids);
        free(pass_dists);
        return -1;
    }

    for (int32_t i = 0; i < n_pass; i++) {
        faiss_hnsw_minimax_heap_push(result_heap, pass_ids[i], pass_dists[i]);
    }

    // 5. 从堆中按距离升序 pop 出 top-K 结果
    int32_t result_count = (n_pass < k) ? n_pass : k;
    for (int32_t i = result_count - 1; i >= 0; i--) {
        float dist = 0.0f;
        int32_t id = faiss_hnsw_minimax_heap_pop_min(result_heap, &dist);
        out_ids[i] = id;
        out_distances[i] = dist;
    }

    faiss_hnsw_minimax_heap_drop(result_heap);
    free(pass_ids);
    free(pass_dists);
    return result_count;
}