/**
 * @file multi_metric_reranker.c
 * @brief 多度量融合精排器实现
 */

#include "db/index/vector_index/reranker/multi_metric_reranker.h"
#include "db/index/vector_index/reranker/reranker.h"

#include <stdlib.h>
#include <string.h>

#include <algo-prod/distance/distance.h>

/* ========================================================================
 * 内部结构
 * ======================================================================== */

/** 多度量融合精排器内部结构 */
struct MultiMetricReranker_s {
    int32_t dims;                /**< 向量维度 */
    distance_metric_t metrics[3]; /**< 多个度量类型 */
    float weights[3];            /**< 对应权重 */
    int32_t num_metrics;         /**< 度量数量 */
};

/* ========================================================================
 * 创建与销毁
 * ======================================================================== */

MultiMetricReranker *multi_metric_reranker_create(const void *config) {
    const RerankerConfig *cfg = (const RerankerConfig *)config;

    MultiMetricReranker *reranker = (MultiMetricReranker *)calloc(1, sizeof(MultiMetricReranker));
    if (reranker == NULL) {
        return NULL;
    }

    reranker->dims = cfg->dims;
    reranker->num_metrics = cfg->multi_metric.num_metrics;

    for (int32_t i = 0; i < reranker->num_metrics && i < 3; i++) {
        reranker->metrics[i] = cfg->multi_metric.metrics[i];
        reranker->weights[i] = cfg->multi_metric.weights[i];
    }

    return reranker;
}

void multi_metric_reranker_destroy(MultiMetricReranker *reranker) {
    free(reranker);
}

/* ========================================================================
 * 多度量融合重排
 * ======================================================================== */

/**
 * @brief 归一化距离到 [0, 1] 范围
 */
static float normalize_distance(float dist, float min_dist, float max_dist) {
    if (max_dist <= min_dist) {
        return 0.5f;
    }
    float normalized = (dist - min_dist) / (max_dist - min_dist);
    return (normalized < 0.0f) ? 0.0f : (normalized > 1.0f ? 1.0f : normalized);
}

/**
 * @brief 计算融合分数
 */
static void compute_fusion_scores(MultiMetricReranker *reranker,
                                  const float *query,
                                  const float *candidates,
                                  int32_t n_candidates,
                                  float *fusion_scores) {
    /* 先计算每个度量的距离 */
    float *metric_scores[3] = { NULL };

    for (int32_t m = 0; m < reranker->num_metrics; m++) {
        metric_scores[m] = (float *)malloc((size_t)n_candidates * sizeof(float));
        if (metric_scores[m] == NULL) {
            for (int32_t j = 0; j < m; j++) {
                free(metric_scores[j]);
            }
            return;
        }

        distance_metric_t metric = reranker->metrics[m];

        for (int32_t i = 0; i < n_candidates; i++) {
            metric_scores[m][i] = distance_compute(
                metric, query, candidates + (int64_t)i * reranker->dims, reranker->dims);
        }
    }

    /* 归一化并融合 */
    for (int32_t i = 0; i < n_candidates; i++) {
        float fused = 0.0f;

        /* 归一化每个度量的距离 */
        for (int32_t m = 0; m < reranker->num_metrics; m++) {
            /* 找最大最小值用于归一化 */
            float min_d = metric_scores[m][i];
            float max_d = metric_scores[m][i];
            for (int32_t j = 0; j < n_candidates; j++) {
                if (metric_scores[m][j] < min_d) min_d = metric_scores[m][j];
                if (metric_scores[m][j] > max_d) max_d = metric_scores[m][j];
            }

            float normalized = normalize_distance(metric_scores[m][i], min_d, max_d);
            fused += reranker->weights[m] * normalized;
        }

        fusion_scores[i] = fused;
    }

    /* 释放临时内存 */
    for (int32_t m = 0; m < reranker->num_metrics; m++) {
        free(metric_scores[m]);
    }
}

/**
 * @brief 部分排序
 */
static void partial_sort(float *scores, int32_t *ids, int32_t n, int32_t k) {
    for (int32_t i = 0; i < k; i++) {
        int32_t min_idx = i;
        float min_score = scores[i];

        for (int32_t j = i + 1; j < n; j++) {
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
}

/* ========================================================================
 * 重排操作
 * ======================================================================== */

int32_t multi_metric_reranker_rerank(MultiMetricReranker *reranker,
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

    /* 计算融合分数 */
    float *fusion_scores = (float *)malloc((size_t)n_candidates * sizeof(float));
    if (fusion_scores == NULL) {
        return -1;
    }

    compute_fusion_scores(reranker, query, candidates, n_candidates, fusion_scores);

    /* 初始化 IDs */
    for (int32_t i = 0; i < n_candidates; i++) {
        reranked_ids[i] = i;
    }

    /* 部分排序 */
    partial_sort(fusion_scores, reranked_ids, n_candidates, actual_k);

    /* 复制分数输出 */
    for (int32_t i = 0; i < actual_k; i++) {
        scores[i] = fusion_scores[i];
    }

    free(fusion_scores);

    return actual_k;
}
