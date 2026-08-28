#include "db/multimodal_object.h"
#include "db/core/log.h"
#include <stdlib.h>

/* C3-4 T5: cross_modal_search（占位）
 * 真实实现：调用指定 target_modal 的命名向量索引进行搜索。
 * 当前 scaffold：返回 0（标记未实现）。
 */
int mm_cross_modal_search(mm_multimodal_object_t *query_obj,
                         const char *target_modal,
                         int32_t *out_ids, float *out_scores, int k) {
    (void)query_obj; (void)target_modal;
    if (!out_ids || !out_scores) return -1;
    for (int i = 0; i < k; ++i) { out_ids[i] = 0; out_scores[i] = 0.0f; }
    LOG_INFO("cross_modal_search: 骨架（依赖 named vector 索引）");
    return 0;
}

/* C3-4 T4: RRF（Reciprocal Rank Fusion）跨向量归并
 * score = sum(1 / (k_rrf + rank_i)) for each ranked list i
 */
double mm_rrf_score(const int *ranks, size_t n_lists, int k_rrf) {
    if (!ranks || n_lists == 0) return 0.0;
    double score = 0.0;
    for (size_t i = 0; i < n_lists; ++i) {
        if (ranks[i] >= 0) score += 1.0 / (double)(k_rrf + ranks[i]);
    }
    return score;
}
