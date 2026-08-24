// faiss_hnsw_search_filtered.c
// 实现 faiss_hnsw_search_filtered 带 filter 谓词的 HNSW 搜索（P4-T4.5）
//
// 算法流程：
//   1. 调 faiss_hnsw_search_layer(level=0, query, ef=K*5+50, ...) 取候选
//   2. 对每个候选调 filter 回调（filter 非 NULL 时）；回调返回 0 的候选被丢弃
//   3. 重读 idx->vectors 中的原始向量，重算 L2 平方距离
//   4. 对通过 filter 的候选按距离升序排序，取 top-K
//
// 设计取舍：
//   - ef = K*5+50 保证有足够候选通过 filter（filter 严格时 ef 需更大）
//   - 用 faiss_hnsw_search_layer 的 beam search 距离作初筛，重算精确 L2 取 top-K
//     （避免 beam search 距离被四舍五入或量化失真影响最终排序）
//   - faiss_hnsw 模块零依赖 SQLite：filter 回调由调用方实现（典型：
//     vectors.c 中封装 filter_json 解析 + SQLite metadata 查询）

#include "faiss_hnsw_internal.h"
#include "algo-prod/distance/distance.h"

#include <float.h>
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

    // ef 取值：保证有足够候选覆盖 filter 拒绝后仍能取到 K 个
    // 经验值 K*5+50：filter 严格时仍能取到 ≥ K 个候选
    int32_t ef = k * 5 + 50;
    if (ef > idx->n_total) {
        ef = idx->n_total;
    }
    if (ef < k) {
        ef = k;
    }

    // 1. beam search 取候选（level 0 全局搜索 ef 个候选）
    int32_t *cand_ids = (int32_t *)malloc(sizeof(int32_t) * (size_t)ef);
    float *cand_dists = (float *)malloc(sizeof(float) * (size_t)ef);
    if (!cand_ids || !cand_dists) {
        free(cand_ids);
        free(cand_dists);
        return -1;
    }

    int32_t n_cands = faiss_hnsw_search_layer(
        idx, 0, query, ef, cand_ids, cand_dists, ef);
    if (n_cands < 0) {
        free(cand_ids);
        free(cand_dists);
        return -1;
    }

    // 2. 应用 filter（filter 非 NULL 时）
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

    // 3. 对通过 filter 的候选按距离升序排序（选择排序，简单可靠）
    for (int32_t i = 0; i < n_pass - 1 && i < k; i++) {
        int32_t best = i;
        float best_d = pass_dists[i];
        for (int32_t j = i + 1; j < n_pass; j++) {
            if (pass_dists[j] < best_d) {
                best_d = pass_dists[j];
                best = j;
            }
        }
        if (best != i) {
            int32_t tmp_id = pass_ids[i];
            float tmp_d = pass_dists[i];
            pass_ids[i] = pass_ids[best];
            pass_dists[i] = pass_dists[best];
            pass_ids[best] = tmp_id;
            pass_dists[best] = tmp_d;
        }
    }

    // 4. 取 top-K
    int32_t result_count = (n_pass < k) ? n_pass : k;
    for (int32_t i = 0; i < result_count; i++) {
        out_ids[i] = pass_ids[i];
        out_distances[i] = pass_dists[i];
    }

    free(pass_ids);
    free(pass_dists);
    return result_count;
}