/**
 * @file hybrid_retrieval.c
 * @brief 混合检索实现
 *
 * 将稠密向量检索、稀疏向量检索、BM25 全文检索的分数进行加权融合。
 * 支持 Min-Max 归一化，确保各路分数处于相同量级。
 */
#include "db/hybrid_retrieval.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ========================================================================
 * 内部辅助函数
 * ======================================================================== */

/**
 * @brief Min-Max 归一化到 [0, 1]
 */
static void normalize_scores(float *scores, uint32_t count) {
    if (count == 0) return;

    float min_val = scores[0];
    float max_val = scores[0];
    for (uint32_t i = 1; i < count; i++) {
        if (scores[i] < min_val) min_val = scores[i];
        if (scores[i] > max_val) max_val = scores[i];
    }

    float range = max_val - min_val;
    if (range < 1e-10f) {
        /* 所有分数相同，归一化为 1 */
        for (uint32_t i = 0; i < count; i++) {
            scores[i] = 1.0f;
        }
    } else {
        for (uint32_t i = 0; i < count; i++) {
            scores[i] = (scores[i] - min_val) / range;
        }
    }
}

/**
 * @brief 比较函数：按 final_score 降序排列
 */
static int compare_results_desc(const void *a, const void *b) {
    float sa = ((const hybrid_result_t*)a)->final_score;
    float sb = ((const hybrid_result_t*)b)->final_score;
    if (sb > sa) return 1;
    if (sb < sa) return -1;
    return 0;
}

/* ========================================================================
 * 公共 API 实现
 * ======================================================================== */

hybrid_config_t hybrid_config_default(void) {
    hybrid_config_t config;
    config.alpha = 0.4f;
    config.beta = 0.3f;
    config.gamma = 0.3f;
    config.normalize_scores = true;
    return config;
}

int hybrid_search(
    const float *query_dense,
    uint32_t dense_dim,
    const sparse_vector_t *query_sparse,
    const char *query_text,
    const void *dense_index,
    uint32_t dense_count,
    const bm25_index_t *bm25_index,
    const hybrid_config_t *config,
    uint32_t top_k,
    hybrid_result_t *results,
    uint32_t *num_results)
{
    (void)query_sparse; /* 稀疏检索待后续引擎接入 */

    if (!results || !num_results || top_k == 0) {
        LOG_ERROR("hybrid_search: 参数错误");
        return -1;
    }

    *num_results = 0;

    /* 权重归一化 */
    hybrid_config_t cfg = config ? *config : hybrid_config_default();
    float w_sum = cfg.alpha + cfg.beta + cfg.gamma;
    if (w_sum < 1e-10f) {
        LOG_ERROR("hybrid_search: 权重之和为 0");
        return -1;
    }
    cfg.alpha /= w_sum;
    cfg.beta /= w_sum;
    cfg.gamma /= w_sum;

    /*
     * 策略：收集所有候选 ID，对每个候选计算三路分数。
     * 实际项目中应使用各检索引擎的 top-k 接口，
     * 此处为简化实现，遍历稠密索引和 BM25 索引的所有文档。
     */

    /* 候选 ID 集合（使用简单数组） */
    uint32_t candidate_cap = top_k * 4;
    uint64_t *candidate_ids = malloc(candidate_cap * sizeof(uint64_t));
    if (!candidate_ids) {
        LOG_ERROR("hybrid_search: 内存分配失败");
        return -1;
    }
    uint32_t candidate_count = 0;

    /* 辅助分数数组 */
    float *dense_scores = calloc(candidate_cap, sizeof(float));
    float *sparse_scores = calloc(candidate_cap, sizeof(float));
    float *bm25_scores_arr = calloc(candidate_cap, sizeof(float));

    if (!dense_scores || !sparse_scores || !bm25_scores_arr) {
        free(candidate_ids);
        free(dense_scores);
        free(sparse_scores);
        free(bm25_scores_arr);
        LOG_ERROR("hybrid_search: 分数数组分配失败");
        return -1;
    }

    /*
     * Step 1: 从稠密索引收集候选
     * dense_index 暂时视为 float* 数组（简化），实际应调用向量检索引擎
     * 这里假设 dense_index 为 NULL 时跳过稠密检索
     */
    if (query_dense && dense_index && dense_count > 0) {
        /* 简化：遍历所有稠密向量计算余弦相似度 */
        const float *vectors = (const float *)dense_index;
        for (uint32_t i = 0; i < dense_count && candidate_count < candidate_cap; i++) {
            uint64_t id = (uint64_t)i;

            /* 计算点积作为相似度（假设向量已归一化） */
            float dot = 0.0f;
            for (uint32_t d = 0; d < dense_dim; d++) {
                dot += query_dense[d] * vectors[i * dense_dim + d];
            }

            candidate_ids[candidate_count] = id;
            dense_scores[candidate_count] = dot;
            candidate_count++;
        }
    }

    /*
     * Step 2: BM25 检索
     */
    if (query_text && bm25_index) {
        /* 对已有候选和 BM25 结果取并集 */
        uint64_t bm25_results[4096];
        float bm25_s[4096];
        uint32_t bm25_count = (uint32_t)bm25_search(bm25_index, query_text,
                                                      (candidate_count > 4096 ? 4096 : candidate_count > top_k * 2 ? top_k * 2 : 4096),
                                                      bm25_results, bm25_s);

        for (uint32_t i = 0; i < (uint32_t)bm25_count && candidate_count < candidate_cap; i++) {
            /* 查找是否已存在 */
            bool found = false;
            for (uint32_t j = 0; j < candidate_count; j++) {
                if (candidate_ids[j] == bm25_results[i]) {
                    bm25_scores_arr[j] = bm25_s[i];
                    found = true;
                    break;
                }
            }
            if (!found) {
                candidate_ids[candidate_count] = bm25_results[i];
                bm25_scores_arr[candidate_count] = bm25_s[i];
                candidate_count++;
            }
        }
    }

    if (candidate_count == 0) {
        *num_results = 0;
        free(candidate_ids);
        free(dense_scores);
        free(sparse_scores);
        free(bm25_scores_arr);
        return 0;
    }

    /*
     * Step 3: 归一化各路分数
     */
    if (cfg.normalize_scores) {
        if (dense_index) normalize_scores(dense_scores, candidate_count);
        if (query_text && bm25_index) normalize_scores(bm25_scores_arr, candidate_count);
        /* sparse_scores 暂未填充，默认 0 */
    }

    /*
     * Step 4: 加权融合
     */
    for (uint32_t i = 0; i < candidate_count; i++) {
        results[i].id = candidate_ids[i];
        results[i].dense_score = dense_scores[i];
        results[i].sparse_score = sparse_scores[i];
        results[i].bm25_score = bm25_scores_arr[i];
        results[i].final_score = cfg.alpha * dense_scores[i]
                               + cfg.beta * sparse_scores[i]
                               + cfg.gamma * bm25_scores_arr[i];
    }

    /*
     * Step 5: 排序并取 top-k
     */
    qsort(results, candidate_count, sizeof(hybrid_result_t), compare_results_desc);

    uint32_t result_count = candidate_count < top_k ? candidate_count : top_k;

    free(candidate_ids);
    free(dense_scores);
    free(sparse_scores);
    free(bm25_scores_arr);

    *num_results = result_count;
    LOG_INFO("hybrid_search: 候选数=%u, 返回=%u", candidate_count, result_count);
    return 0;
}
