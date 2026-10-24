/**
 * @file precise_reranker.h
 * @brief 精确距离精排器
 *
 * 使用原始向量计算精确距离进行重排。
 */

#ifndef DB_PRECISE_RERANKER_H
#define DB_PRECISE_RERANKER_H

#include <stdbool.h>
#include <stdint.h>

#include <algo-prod/distance/distance.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 精确精排器（不透明类型）
 * ======================================================================== */

typedef struct PreciseReranker_s PreciseReranker;

/**
 * @brief 创建精确精排器
 *
 * @param config ReRanker 配置
 * @return 精排器实例
 */
PreciseReranker *precise_reranker_create(const void *config);

/**
 * @brief 销毁精确精排器
 */
void precise_reranker_destroy(PreciseReranker *reranker);

/**
 * @brief 执行精确距离精排
 *
 * @param reranker 精排器
 * @param query 查询向量
 * @param candidates 候选向量数组
 * @param n_candidates 候选数量
 * @param k 返回数量
 * @param[out] scores 输出分数
 * @param[out] reranked_ids 输出 ID
 * @return 实际返回数量
 */
int32_t precise_reranker_rerank(PreciseReranker *reranker,
                                 const float *query,
                                 const float *candidates,
                                 int32_t n_candidates,
                                 int32_t k,
                                 float *scores,
                                 int32_t *reranked_ids);

#ifdef __cplusplus
}
#endif

#endif /* DB_PRECISE_RERANKER_H */
