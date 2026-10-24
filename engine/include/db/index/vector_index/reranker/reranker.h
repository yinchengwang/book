/**
 * @file reranker.h
 * @brief ReRanker 抽象接口
 *
 * 定义两阶段检索中的精排（ReRank）能力：
 * 1. 精确距离精排（基于原始向量计算真实距离）
 * 2. 多度量融合（结合多种距离度量）
 * 3. MMR 去重（最大边际相关性）
 */

#ifndef DB_RERANKER_H
#define DB_RERANKER_H

#include <stdbool.h>
#include <stdint.h>

#include <algo-prod/distance/distance.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * ReRanker 类型
 * ======================================================================== */

/** ReRanker 算法类型 */
typedef enum RerankerType_e {
    RERANKER_PRECISE = 0,        /**< 精确距离精排 */
    RERANKER_MULTI_METRIC = 1,   /**< 多度量融合 */
    RERANKER_MMR = 2,            /**< MMR 去重 */
    RERANKER_CUSTOM = 99         /**< 自定义（通过注册工厂） */
} RerankerType;

/* ========================================================================
 * ReRanker 配置
 * ======================================================================== */

/** ReRanker 配置 */
typedef struct RerankerConfig_s {
    RerankerType type;           /**< 重排类型 */
    int32_t dims;                /**< 向量维度 */
    distance_metric_t metric;    /**< 主距离度量 */

    /* 精确精排配置 */
    struct {
        bool use_simd;           /**< 是否使用 SIMD 加速 */
    } precise;

    /* 多度量融合配置 */
    struct {
        distance_metric_t metrics[3];   /**< 多个度量类型 */
        float weights[3];               /**< 对应权重 */
        int32_t num_metrics;            /**< 度量数量 */
    } multi_metric;

    /* MMR 配置 */
    struct {
        float lambda;            /**< MMR lambda 参数 [0,1] */
        int32_t diversity_dim;   /**< 多样性维度索引 */
    } mmr;
} RerankerConfig;

/**
 * @brief 创建默认 ReRanker 配置
 */
RerankerConfig reranker_config_default(RerankerType type, int32_t dims);

/* ========================================================================
 * ReRanker 接口（不透明类型）
 * ======================================================================== */

typedef struct Reranker_s Reranker;

/**
 * @brief 创建 ReRanker
 *
 * @param type ReRanker 类型
 * @param config 配置
 * @return ReRanker 实例，失败返回 NULL
 */
Reranker *reranker_create(RerankerType type, const RerankerConfig *config);

/**
 * @brief 销毁 ReRanker
 */
void reranker_destroy(Reranker *reranker);

/**
 * @brief 执行重排
 *
 * @param reranker ReRanker 实例
 * @param query 查询向量
 * @param candidates 候选向量数组（dims * n_candidates）
 * @param n_candidates 候选数量
 * @param k 返回结果数
 * @param[out] scores 输出分数数组
 * @param[out] reranked_ids 输出重排后的 ID 数组
 * @return 实际返回结果数，失败返回 -1
 */
int32_t reranker_rerank(Reranker *reranker,
                        const float *query,
                        const float *candidates,
                        int32_t n_candidates,
                        int32_t k,
                        float *scores,
                        int32_t *reranked_ids);

/**
 * @brief 获取 ReRanker 类型
 */
RerankerType reranker_get_type(const Reranker *reranker);

/**
 * @brief 获取 ReRanker 名称
 */
const char *reranker_get_name(const Reranker *reranker);

/* ========================================================================
 * ReRanker 工厂注册
 * ======================================================================== */

/** 重排函数类型 */
typedef int32_t (*rerank_func_t)(const float *query,
                                 const float *candidates,
                                 int32_t n_candidates,
                                 int32_t k,
                                 float *scores,
                                 int32_t *reranked_ids,
                                 void *user_data);

/**
 * @brief 注册自定义 ReRanker
 *
 * @param name 注册名称
 * @param func 重排函数
 * @param user_data 用户数据
 * @return 0 成功，-1 失败
 */
int32_t reranker_register(const char *name,
                          rerank_func_t func,
                          void *user_data);

/**
 * @brief 应用已注册的 ReRanker
 *
 * @param name 注册名称
 * @return 0 成功，-1 失败（未找到或执行失败）
 */
int32_t reranker_apply(const char *name,
                       const float *query,
                       const float *candidates,
                       int32_t n_candidates,
                       int32_t k,
                       float *scores,
                       int32_t *reranked_ids);

/**
 * @brief 列出已注册的 ReRanker
 *
 * @param names 输出名称数组（至少 32 个）
 * @param max_names 最大数量
 * @return 实际数量
 */
int32_t reranker_list_registered(char names[][64], int32_t max_names);

#ifdef __cplusplus
}
#endif

#endif /* DB_RERANKER_H */
