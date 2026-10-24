/**
 * @file mmr_reranker.h
 * @brief MMR 去重精排器
 *
 * 最大边际相关性（Maximal Marginal Relevance）重排，
 * 在相关性和多样性之间取得平衡。
 */

#ifndef DB_MMR_RERANKER_H
#define DB_MMR_RERANKER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * MMR 精排器（不透明类型）
 * ======================================================================== */

typedef struct MmrReranker_s MmrReranker;

/**
 * @brief 创建 MMR 精排器
 *
 * @param config ReRanker 配置
 * @return MMR 精排器实例
 */
MmrReranker *mmr_reranker_create(const void *config);

/**
 * @brief 销毁 MMR 精排器
 */
void mmr_reranker_destroy(MmrReranker *reranker);

/**
 * @brief 执行 MMR 重排
 *
 * @param reranker MMR 精排器
 * @param query 查询向量
 * @param candidates 候选向量数组
 * @param n_candidates 候选数量
 * @param k 返回数量
 * @param[out] scores 输出分数
 * @param[out] reranked_ids 输出 ID
 * @return 实际返回数量
 */
int32_t mmr_reranker_rerank(MmrReranker *reranker,
                            const float *query,
                            const float *candidates,
                            int32_t n_candidates,
                            int32_t k,
                            float *scores,
                            int32_t *reranked_ids);

#ifdef __cplusplus
}
#endif

#endif /* DB_MMR_RERANKER_H */
