/**
 * @file multi_metric_reranker.h
 * @brief 多度量融合精排器
 *
 * 结合多种距离度量进行加权融合。
 */

#ifndef DB_MULTI_METRIC_RERANKER_H
#define DB_MULTI_METRIC_RERANKER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 多度量融合精排器（不透明类型）
 * ======================================================================== */

typedef struct MultiMetricReranker_s MultiMetricReranker;

/**
 * @brief 创建多度量融合精排器
 */
MultiMetricReranker *multi_metric_reranker_create(const void *config);

/**
 * @brief 销毁多度量融合精排器
 */
void multi_metric_reranker_destroy(MultiMetricReranker *reranker);

/**
 * @brief 执行多度量融合精排
 */
int32_t multi_metric_reranker_rerank(MultiMetricReranker *reranker,
                                     const float *query,
                                     const float *candidates,
                                     int32_t n_candidates,
                                     int32_t k,
                                     float *scores,
                                     int32_t *reranked_ids);

#ifdef __cplusplus
}
#endif

#endif /* DB_MULTI_METRIC_RERANKER_H */
