/**
 * @file precise_reranker.c
 * @brief 精确距离精排器实现
 */

#include "db/index/vector_index/reranker/precise_reranker.h"
#include "db/index/vector_index/reranker/reranker.h"

#include <stdlib.h>
#include <string.h>

#include <algo-prod/distance/distance.h>

/* ========================================================================
 * 内部结构
 * ======================================================================== */

/** 精确精排器内部结构 */
struct PreciseReranker_s {
    int32_t dims;                /**< 向量维度 */
    distance_metric_t metric;    /**< 距离度量 */
    bool use_simd;               /**< 是否使用 SIMD */
};

/* ========================================================================
 * 创建与销毁
 * ======================================================================== */

PreciseReranker *precise_reranker_create(const void *config) {
    const RerankerConfig *cfg = (const RerankerConfig *)config;

    PreciseReranker *reranker = (PreciseReranker *)calloc(1, sizeof(PreciseReranker));
    if (reranker == NULL) {
        return NULL;
    }

    reranker->dims = cfg->dims;
    reranker->metric = cfg->metric;
    reranker->use_simd = cfg->precise.use_simd;

    return reranker;
}

void precise_reranker_destroy(PreciseReranker *reranker) {
    free(reranker);
}

/* ========================================================================
 * 精确距离计算
 * ======================================================================== */

/**
 * @brief 简单冒泡排序（用于小批量）
 */
static void sort_results(float *scores, int32_t *ids, int32_t n) {
    for (int32_t i = 0; i < n - 1; i++) {
        for (int32_t j = 0; j < n - i - 1; j++) {
            bool need_swap = false;
            if (scores[j] > scores[j + 1]) {
                need_swap = true;
            }
            if (need_swap) {
                float temp_score = scores[j];
                scores[j] = scores[j + 1];
                scores[j + 1] = temp_score;

                int32_t temp_id = ids[j];
                ids[j] = ids[j + 1];
                ids[j + 1] = temp_id;
            }
        }
    }
}

/**
 * @brief 计算精确距离并进行部分排序
 */
static int32_t compute_and_sort(const float *query,
                                const float *candidates,
                                int32_t n_candidates,
                                int32_t dims,
                                distance_metric_t metric,
                                float *scores,
                                int32_t *ids,
                                int32_t k) {
    /* 计算所有候选的距离 */
    for (int32_t i = 0; i < n_candidates; i++) {
        scores[i] = distance_compute(metric, query, candidates + (int64_t)i * dims, dims);
        ids[i] = i;
    }

    /* 部分排序：找出 Top-K */
    if (n_candidates <= k) {
        /* 小批量：简单排序 */
        sort_results(scores, ids, n_candidates);
        return n_candidates;
    }

    /* 大批量：使用选择算法找 Top-K */
    for (int32_t i = 0; i < k; i++) {
        int32_t min_idx = i;
        float min_score = scores[i];

        for (int32_t j = i + 1; j < n_candidates; j++) {
            if (scores[j] < min_score) {
                min_score = scores[j];
                min_idx = j;
            }
        }

        if (min_idx != i) {
            float temp_score = scores[i];
            scores[i] = scores[min_idx];
            scores[min_idx] = temp_score;

            int32_t temp_id = ids[i];
            ids[i] = ids[min_idx];
            ids[min_idx] = temp_id;
        }
    }

    return k;
}

/* ========================================================================
 * 重排操作
 * ======================================================================== */

int32_t precise_reranker_rerank(PreciseReranker *reranker,
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

    /* 使用精确距离计算并排序 */
    return compute_and_sort(query, candidates, n_candidates,
                            reranker->dims, reranker->metric,
                            scores, reranked_ids, k);
}
