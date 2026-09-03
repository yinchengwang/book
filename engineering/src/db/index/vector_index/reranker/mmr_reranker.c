/**
 * @file mmr_reranker.c
 * @brief MMR 去重精排器实现
 *
 * 最大边际相关性（MMR）算法：
 * MMR = λ * sim(query, doc) - (1-λ) * max_sim(doc, selected_docs)
 *
 * 在选择下一个文档时，同时考虑：
 * 1. 与查询的相关性
 * 2. 与已选文档的多样性
 */

#include "db/index/vector_index/reranker/mmr_reranker.h"
#include "db/index/vector_index/reranker/reranker.h"

#include <stdlib.h>
#include <string.h>
#include <float.h>

#include <algo-prod/distance/distance.h>

/* ========================================================================
 * 内部结构
 * ======================================================================== */

/** MMR 精排器内部结构 */
struct MmrReranker_s {
    int32_t dims;                /**< 向量维度 */
    float lambda;                /**< MMR lambda 参数 [0,1] */
    int32_t diversity_dim;       /**< 多样性维度索引（用于快速距离计算） */
    distance_metric_t metric;    /**< 距离度量 */
};

/* ========================================================================
 * 创建与销毁
 * ======================================================================== */

MmrReranker *mmr_reranker_create(const void *config) {
    const RerankerConfig *cfg = (const RerankerConfig *)config;

    MmrReranker *reranker = (MmrReranker *)calloc(1, sizeof(MmrReranker));
    if (reranker == NULL) {
        return NULL;
    }

    reranker->dims = cfg->dims;
    reranker->lambda = cfg->mmr.lambda;
    reranker->diversity_dim = cfg->mmr.diversity_dim;
    reranker->metric = cfg->metric;

    /* 确保 lambda 在 [0, 1] 范围内 */
    if (reranker->lambda < 0.0f) reranker->lambda = 0.0f;
    if (reranker->lambda > 1.0f) reranker->lambda = 1.0f;

    return reranker;
}

void mmr_reranker_destroy(MmrReranker *reranker) {
    free(reranker);
}

/* ========================================================================
 * MMR 算法
 * ======================================================================== */

/**
 * @brief 计算候选文档与已选文档的最大相似度
 */
static float max_similarity_to_selected(const float *candidates,
                                        int32_t candidate_idx,
                                        int32_t dims,
                                        const int32_t *selected,
                                        int32_t n_selected,
                                        distance_metric_t metric) {
    if (n_selected == 0) {
        return 0.0f;
    }

    float max_sim = 0.0f;
    const float *candidate_vec = candidates + (int64_t)candidate_idx * dims;

    for (int32_t i = 0; i < n_selected; i++) {
        const float *selected_vec = candidates + (int64_t)selected[i] * dims;
        float dist = distance_compute(metric, candidate_vec, selected_vec, dims);
        /* 距离越小越相似 */
        if (dist < max_sim || i == 0) {
            max_sim = dist;
        }
    }

    return max_sim;
}

/**
 * @brief 计算 MMR 分数
 *
 * MMR = λ * relevance - (1-λ) * diversity
 * 即：λ * (1/dist) - (1-λ) * max_sim
 */
static float compute_mmr_score(float relevance,
                               float diversity,
                               float lambda) {
    /* relevance 是距离的倒数（距离越小相关性越高） */
    float relevance_score = (relevance > 0.0f) ? (1.0f / (1.0f + relevance)) : 1.0f;
    /* diversity 是与已选文档的最大距离 */
    return lambda * relevance_score + (1.0f - lambda) * diversity;
}

/* ========================================================================
 * 重排操作
 * ======================================================================== */

int32_t mmr_reranker_rerank(MmrReranker *reranker,
                            const float *query,
                            const float *candidates,
                            int32_t n_candidates,
                            int32_t k,
                            float *scores,
                            int32_t *reranked_ids) {
    if (reranker == NULL || query == NULL || candidates == NULL) {
        return -1;
    }

    if (n_candidates <= 0 || k <= 0) {
        return 0;
    }

    int32_t actual_k = (k > n_candidates) ? n_candidates : k;

    /* 标记是否已选择 */
    bool *selected = (bool *)calloc((size_t)n_candidates, sizeof(bool));
    if (selected == NULL) {
        return -1;
    }

    int32_t n_selected = 0;
    float lambda = reranker->lambda;

    for (int32_t step = 0; step < actual_k; step++) {
        float best_mmr = -FLT_MAX;
        int32_t best_idx = -1;

        /* 遍历所有未选中的候选 */
        for (int32_t i = 0; i < n_candidates; i++) {
            if (selected[i]) {
                continue;
            }

            /* 计算与查询的相关性（距离） */
            float relevance = distance_compute(
                reranker->metric, query, candidates + (int64_t)i * reranker->dims, reranker->dims);

            /* 计算与已选文档的最大相似度 */
            float diversity = max_similarity_to_selected(
                candidates, i, reranker->dims,
                reranked_ids, n_selected, reranker->metric);

            /* 计算 MMR 分数 */
            float mmr = compute_mmr_score(relevance, diversity, lambda);

            if (mmr > best_mmr) {
                best_mmr = mmr;
                best_idx = i;
            }
        }

        if (best_idx < 0) {
            break;  /* 没有更多候选 */
        }

        /* 标记选中 */
        selected[best_idx] = true;
        reranked_ids[n_selected] = best_idx;
        scores[n_selected] = best_mmr;
        n_selected++;
    }

    free(selected);

    return n_selected;
}
