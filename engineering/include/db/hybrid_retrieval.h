/**
 * @file hybrid_retrieval.h
 * @brief 混合检索接口
 *
 * 将稠密向量检索、稀疏向量检索、BM25 全文检索的结果融合，提供统一的混合检索 API。
 * 支持加权分数融合和分数归一化。
 */
#ifndef DB_HYBRID_RETRIEVAL_H
#define DB_HYBRID_RETRIEVAL_H

#include "sparse_vector.h"
#include "bm25_index.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 混合检索配置
 * ======================================================================== */

/**
 * @brief 混合检索配置
 */
typedef struct hybrid_config_s {
    float alpha;            /**< 稠密向量权重 (0-1) */
    float beta;             /**< 稀疏向量权重 (0-1) */
    float gamma;            /**< BM25 权重 (0-1) */
    bool normalize_scores;  /**< 是否对各路分数归一化到 [0,1] */
} hybrid_config_t;

/**
 * @brief 混合检索结果
 */
typedef struct hybrid_result_s {
    uint64_t id;            /**< 文档/向量 ID */
    float dense_score;      /**< 稠密向量分数 */
    float sparse_score;     /**< 稀疏向量分数 */
    float bm25_score;       /**< BM25 分数 */
    float final_score;      /**< 加权融合后的最终分数 */
} hybrid_result_t;

/* ========================================================================
 * 混合检索 API
 * ======================================================================== */

/**
 * @brief 获取默认混合检索配置
 * @return 默认配置（alpha=0.4, beta=0.3, gamma=0.3, normalize=true）
 */
hybrid_config_t hybrid_config_default(void);

/**
 * @brief 执行混合检索
 *
 * @param query_dense 查询稠密向量（可为 NULL，跳过稠密检索）
 * @param dense_dim 稠密向量维度
 * @param query_sparse 查询稀疏向量（可为 NULL，跳过稀疏检索）
 * @param query_text 查询文本（可为 NULL，跳过 BM25 检索）
 * @param dense_index 稠密索引句柄（传给稠密检索引擎）
 * @param dense_count 稠密索引中的向量数量
 * @param bm25_index BM25 索引（可为 NULL，跳过 BM25 检索）
 * @param config 混合配置（NULL 使用默认配置）
 * @param top_k 返回结果数量上限
 * @param results 输出结果数组（调用者分配，长度 >= top_k）
 * @param num_results 实际返回的结果数量（输出参数）
 * @return 0 成功，-1 参数错误
 */
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
    uint32_t *num_results
);

#ifdef __cplusplus
}
#endif

#endif /* DB_HYBRID_RETRIEVAL_H */
