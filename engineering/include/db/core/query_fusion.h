/**
 * @file query_fusion.h
 * @brief 多模态查询结果融合策略
 *
 * 支持的融合策略：
 *   - RRF (Reciprocal Rank Fusion): 基于排名的融合
 *   - 加权融合: 基于分数的加权求和
 *   - 自定义融合: 通过回调函数实现自定义策略
 */

#ifndef DB_QUERY_FUSION_H
#define DB_QUERY_FUSION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 融合类型
 * ======================================================================== */

/** 融合策略类型 */
#ifndef FUSION_STRATEGY_DEFINED
#define FUSION_STRATEGY_DEFINED
typedef enum FusionStrategy_e {
    FUSION_STRATEGY_RRF = 0,         /**< Reciprocal Rank Fusion */
    FUSION_STRATEGY_WEIGHTED,        /**< 加权融合 */
    FUSION_STRATEGY_CUSTOM           /**< 自定义融合 */
} FusionStrategy;
#endif

/* ========================================================================
 * 单个检索结果
 * ======================================================================== */

/** 检索结果来源 */
typedef enum ResultSource_e {
    SOURCE_VECTOR = 0,      /**< 向量检索 */
    SOURCE_BM25,            /**< BM25 全文检索 */
    SOURCE_HYBRID           /**< 混合来源 */
} ResultSource;

/** 单个检索结果 */
typedef struct QueryResultItem_s {
    int64_t id;             /**< 记录 ID */
    double vector_score;    /**< 向量相似度分数 (0-1, 越大越好) */
    double bm25_score;      /**< BM25 分数 */
    double combined_score;  /**< 融合后的综合分数 */
    int vector_rank;        /**< 向量检索中的排名 (0 = 未命中) */
    int bm25_rank;          /**< BM25 检索中的排名 (0 = 未命中) */
    ResultSource source;    /**< 结果来源 */
} QueryResultItem;

/* ========================================================================
 * 结果列表
 * ======================================================================== */

/** 检索结果列表 */
typedef struct QueryResultList_s {
    QueryResultItem *items;     /**< 结果项数组 */
    int count;                  /**< 结果数量 */
    int capacity;               /**< 容量 */
} QueryResultList;

/* ========================================================================
 * 融合配置
 * ======================================================================== */

/** 融合配置 */
typedef struct FusionConfig_s {
    FusionStrategy strategy;    /**< 融合策略 */
    double vector_weight;       /**< 向量权重 (0-1) */
    double bm25_weight;         /**< BM25 权重 (0-1) */
    int rrf_k;                  /**< RRF k 参数 (通常 60) */
    int top_k;                  /**< 返回的 top-k 结果 */
    bool normalize_scores;      /**< 是否归一化分数 */
} FusionConfig;

/* ========================================================================
 * 自定义融合回调
 * ======================================================================== */

/**
 * @brief 自定义融合函数类型
 * @param items 输入结果项数组
 * @param num_items 结果数量
 * @param config 融合配置
 * @param out_scores 输出分数数组 (调用方分配)
 * @param ctx 用户上下文
 */
typedef void (*FusionCallback)(const QueryResultItem *items, int num_items,
                                const FusionConfig *config,
                                double *out_scores, void *ctx);

/* ========================================================================
 * 结果融合 API
 * ======================================================================== */

/**
 * @brief 创建融合配置（默认 RRF）
 * @param strategy 融合策略
 * @return 融合配置
 */
FusionConfig *fusion_config_create(FusionStrategy strategy);

/**
 * @brief 创建 RRF 融合配置
 * @param rrf_k RRF k 参数
 * @param top_k 返回的 top-k 结果
 * @return 融合配置
 */
FusionConfig *fusion_config_create_rrf(int rrf_k, int top_k);

/**
 * @brief 创建加权融合配置
 * @param vector_weight 向量权重
 * @param bm25_weight BM25 权重
 * @param top_k 返回的 top-k 结果
 * @return 融合配置
 */
FusionConfig *fusion_config_create_weighted(double vector_weight, double bm25_weight, int top_k);

/**
 * @brief 释放融合配置
 * @param config 融合配置
 */
void fusion_config_free(FusionConfig *config);

/**
 * @brief 执行 RRF 融合
 * @param vector_results 向量检索结果
 * @param num_vector 结果数量
 * @param bm25_results BM25 检索结果
 * @param num_bm25 结果数量
 * @param config 融合配置
 * @return 融合后的结果列表
 */
QueryResultList *fusion_rrf(const QueryResultItem *vector_results, int num_vector,
                            const QueryResultItem *bm25_results, int num_bm25,
                            const FusionConfig *config);

/**
 * @brief 执行加权融合
 * @param vector_results 向量检索结果
 * @param num_vector 结果数量
 * @param bm25_results BM25 检索结果
 * @param num_bm25 结果数量
 * @param config 融合配置
 * @return 融合后的结果列表
 */
QueryResultList *fusion_weighted(const QueryResultItem *vector_results, int num_vector,
                                  const QueryResultItem *bm25_results, int num_bm25,
                                  const FusionConfig *config);

/**
 * @brief 执行自定义融合
 * @param items 输入结果项数组
 * @param num_items 结果数量
 * @param config 融合配置
 * @param callback 自定义融合回调
 * @param ctx 用户上下文
 * @return 融合后的结果列表
 */
QueryResultList *fusion_custom(const QueryResultItem *items, int num_items,
                                const FusionConfig *config,
                                FusionCallback callback, void *ctx);

/**
 * @brief 执行融合（根据配置选择策略）
 * @param vector_results 向量检索结果
 * @param num_vector 结果数量
 * @param bm25_results BM25 检索结果
 * @param num_bm25 结果数量
 * @param config 融合配置
 * @return 融合后的结果列表
 */
QueryResultList *fusion_execute(const QueryResultItem *vector_results, int num_vector,
                                 const QueryResultItem *bm25_results, int num_bm25,
                                 const FusionConfig *config);

/* ========================================================================
 * 结果列表管理
 * ======================================================================== */

/**
 * @brief 创建结果列表
 * @param capacity 初始容量
 * @return 结果列表
 */
QueryResultList *query_result_list_create(int capacity);

/**
 * @brief 添加结果到列表
 * @param list 结果列表
 * @param item 结果项
 * @return 0 成功，-1 失败
 */
int query_result_list_add(QueryResultList *list, const QueryResultItem *item);

/**
 * @brief 释放结果列表
 * @param list 结果列表
 */
void query_result_list_free(QueryResultList *list);

/**
 * @brief 复制结果列表
 * @param list 原列表
 * @return 复制后的列表
 */
QueryResultList *query_result_list_copy(const QueryResultList *list);

/**
 * @brief 获取第 i 个结果
 * @param list 结果列表
 * @param i 索引
 * @return 结果项
 */
const QueryResultItem *query_result_list_get(const QueryResultList *list, int i);

/* ========================================================================
 * 便捷宏
 * ======================================================================== */

/** 获取结果数量 */
#define QUERY_RESULT_COUNT(list) ((list) ? (list)->count : 0)

/** 结果列表是否为空 */
#define QUERY_RESULT_IS_EMPTY(list) (!(list) || (list)->count == 0)

/** 获取综合分数 */
#define QUERY_RESULT_SCORE(item) ((item)->combined_score)

/** 获取向量分数 */
#define QUERY_RESULT_VECTOR_SCORE(item) ((item)->vector_score)

/** 获取 BM25 分数 */
#define QUERY_RESULT_BM25_SCORE(item) ((item)->bm25_score)

#ifdef __cplusplus
}
#endif

#endif /* DB_QUERY_FUSION_H */
