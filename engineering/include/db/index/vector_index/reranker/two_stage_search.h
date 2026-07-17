/**
 * @file two_stage_search.h
 * @brief 两阶段检索接口
 *
 * Stage 1: HNSW/DiskANN 粗排（返回 Top-K 候选）
 * Stage 2: ReRank 精排（精确距离 + 多度量融合 + MMR）
 */

#ifndef DB_TWO_STAGE_SEARCH_H
#define DB_TWO_STAGE_SEARCH_H

#include <stdbool.h>
#include <stdint.h>

#include <algo-prod/distance/distance.h>
#include "reranker.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 两阶段检索配置
 * ======================================================================== */

/** 两阶段检索配置 */
typedef struct TwoStageConfig_s {
    /* Stage 1 配置 */
    struct {
        int32_t k;                    /**< 粗排返回候选数 */
        int32_t ef_search;            /**< HNSW 搜索参数 */
        bool use_pq;                  /**< 是否使用 PQ 加速 */
    } stage1;

    /* Stage 2 配置 */
    struct {
        RerankerType reranker_type;   /**< 精排类型 */
        RerankerConfig reranker_config; /**< 精排配置 */
        bool enable_mmr;              /**< 是否启用 MMR */
        float mmr_lambda;             /**< MMR lambda */
    } stage2;

    /* 性能监控 */
    struct {
        bool enable_timing;           /**< 启用计时 */
        bool enable_stats;            /**< 启用统计 */
    } monitor;
} TwoStageConfig;

/**
 * @brief 创建默认两阶段检索配置
 */
TwoStageConfig two_stage_config_default(int32_t dims);

/* ========================================================================
 * 两阶段检索结果
 * ======================================================================== */

/** 两阶段检索结果 */
typedef struct TwoStageResult_s {
    int32_t id;               /**< 向量 ID */
    float score;              /**< 最终分数 */
    float stage1_score;       /**< Stage 1 分数（近似距离） */
    float stage2_score;       /**< Stage 2 分数（精确距离） */
} TwoStageResult;

/** 两阶段检索统计 */
typedef struct TwoStageStats_s {
    int64_t stage1_time_us;       /**< Stage 1 耗时（微秒） */
    int64_t stage2_time_us;       /**< Stage 2 耗时（微秒） */
    int64_t total_time_us;        /**< 总耗时 */
    int32_t candidates;           /**< 候选数量 */
    int32_t returned;             /**< 返回数量 */
} TwoStageStats;

/* ========================================================================
 * 两阶段检索器（不透明类型）
 * ======================================================================== */

typedef struct TwoStageSearcher_s TwoStageSearcher;

/**
 * @brief 创建两阶段检索器
 *
 * @param stage1_index Stage 1 索引（通常是 HNSW）
 * @param config 配置
 * @return 检索器实例
 */
TwoStageSearcher *two_stage_searcher_create(void *stage1_index,
                                           const TwoStageConfig *config);

/**
 * @brief 销毁两阶段检索器
 */
void two_stage_searcher_destroy(TwoStageSearcher *searcher);

/**
 * @brief 执行两阶段检索
 *
 * @param searcher 检索器
 * @param query 查询向量
 * @param k 返回结果数
 * @param[out] results 输出结果
 * @param[out] stats 统计信息（可为 NULL）
 * @return 实际返回数量，失败返回 -1
 */
int32_t two_stage_search(TwoStageSearcher *searcher,
                         const float *query,
                         int32_t k,
                         TwoStageResult *results,
                         TwoStageStats *stats);

/**
 * @brief 仅执行 Stage 1（粗排）
 */
int32_t two_stage_search_stage1(TwoStageSearcher *searcher,
                                const float *query,
                                int32_t k,
                                int32_t *ids,
                                float *distances);

/**
 * @brief 仅执行 Stage 2（精排）
 */
int32_t two_stage_search_stage2(TwoStageSearcher *searcher,
                                const float *query,
                                const float *candidates,
                                int32_t n_candidates,
                                int32_t k,
                                int32_t *reranked_ids,
                                float *scores);

/**
 * @brief 获取统计信息
 */
void two_stage_searcher_get_stats(const TwoStageSearcher *searcher,
                                  TwoStageStats *stats);

/**
 * @brief 重置统计信息
 */
void two_stage_searcher_reset_stats(TwoStageSearcher *searcher);

/* ========================================================================
 * 便捷函数
 * ======================================================================== */

/**
 * @brief 独立使用 MMR 重排
 *
 * @param candidates 候选向量
 * @param ids 候选 ID
 * @param n 候选数量
 * @param k 返回数量
 * @param lambda MMR lambda 参数
 * @param metric 距离度量
 * @param[out] reranked_ids 重排后的 ID
 * @return 实际返回数量
 */
int32_t mmr_rerank_standalone(const float *candidates,
                              int32_t *ids,
                              int32_t n,
                              int32_t k,
                              float lambda,
                              distance_metric_t metric,
                              int32_t *reranked_ids);

#ifdef __cplusplus
}
#endif

#endif /* DB_TWO_STAGE_SEARCH_H */
