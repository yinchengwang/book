/**
 * @file bm25.h
 * @brief BM25 相关性打分算法
 *
 * 实现 Okapi BM25 算法用于文档相关性排序。
 * 支持标准 BM25、BM25F（字段加权）和 BM25+ 变体。
 */
#ifndef DB_DOC_BM25_H
#define DB_DOC_BM25_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 常量定义
 * ======================================================================== */

/** BM25 默认 k1 参数 */
#define BM25_DEFAULT_K1 1.2

/** BM25 默认 b 参数 */
#define BM25_DEFAULT_B 0.75

/** BM25+ 默认 delta 参数 */
#define BM25_DEFAULT_DELTA 1.0

/** IDF 缓存最大条目数 */
#define BM25_IDF_CACHE_SIZE 10000

/* ========================================================================
 * 类型定义
 * ======================================================================== */

/**
 * @brief 字段权重配置
 */
typedef struct bm25_field_weight_s {
    const char *field_name;   /**< 字段名 */
    double weight;            /**< 权重 */
} bm25_field_weight_t;

/**
 * @brief 文档统计信息
 */
typedef struct bm25_doc_stats_s {
    uint64_t doc_id;          /**< 文档 ID */
    uint32_t doc_len;         /**< 文档长度（词数） */
    double *field_lens;       /**< 各字段长度 */
} bm25_doc_stats_t;

/**
 * @brief BM25 评分器
 */
typedef struct bm25_scorer_s {
    /* 参数 */
    double k1;                /**< 词频饱和参数 */
    double b;                 /**< 文档长度归一化参数 */
    double delta;             /**< BM25+ 偏移参数 */
    uint32_t avg_doc_len;     /**< 平均文档长度 */

    /* 索引信息 */
    uint64_t num_docs;        /**< 文档总数 */
    uint32_t num_terms;       /**< 术语数 */

    /* IDF 缓存 */
    double *idf_cache;        /**< IDF 缓存 */
    uint64_t *term_df;        /**< 术语文档频率 */
    uint32_t cache_count;     /**< 缓存条目数 */

    /* 字段权重（用于 BM25F） */
    bm25_field_weight_t *field_weights;
    uint32_t field_count;
} bm25_scorer_t;

/**
 * @brief 批量打分结果
 */
typedef struct bm25_batch_result_s {
    uint64_t doc_id;          /**< 文档 ID */
    double score;             /**< BM25 分数 */
} bm25_batch_result_t;

/* ========================================================================
 * 生命周期 API
 * ======================================================================== */

/**
 * @brief 创建 BM25 评分器
 *
 * @param k1 词频饱和参数
 * @param b 文档长度归一化参数
 * @return 评分器句柄，失败返回 NULL
 */
bm25_scorer_t *bm25_create(double k1, double b);

/**
 * @brief 释放 BM25 评分器
 *
 * @param scorer 评分器
 */
void bm25_destroy(bm25_scorer_t *scorer);

/* ========================================================================
 * 索引信息 API
 * ======================================================================== */

/**
 * @brief 设置索引信息
 *
 * @param scorer 评分器
 * @param num_docs 文档总数
 * @param avg_doc_len 平均文档长度
 */
void bm25_set_index_info(bm25_scorer_t *scorer, uint64_t num_docs,
                         uint32_t avg_doc_len);

/**
 * @brief 设置术语文档频率
 *
 * @param scorer 评分器
 * @param term_id 术语 ID
 * @param doc_freq 文档频率
 */
void bm25_set_term_df(bm25_scorer_t *scorer, uint32_t term_id,
                      uint64_t doc_freq);

/**
 * @brief 预计算所有 IDF 值
 *
 * @param scorer 评分器
 */
void bm25_precompute(bm25_scorer_t *scorer);

/* ========================================================================
 * 打分 API
 * ======================================================================== */

/**
 * @brief 计算单个文档的 BM25 分数
 *
 * @param scorer 评分器
 * @param doc_len 文档长度
 * @param term_freqs 词频数组 [num_terms]
 * @param query_terms 查询术语 ID 数组
 * @param num_query_terms 查询词数
 * @return BM25 分数
 */
double bm25_score(bm25_scorer_t *scorer,
                  uint32_t doc_len,
                  const uint32_t *term_freqs,
                  const uint32_t *query_terms,
                  uint32_t num_query_terms);

/**
 * @brief 计算 BM25+ 分数
 *
 * @param scorer 评分器
 * @param doc_len 文档长度
 * @param term_freqs 词频数组
 * @param query_terms 查询术语 ID 数组
 * @param num_query_terms 查询词数
 * @return BM25+ 分数
 */
double bm25_plus_score(bm25_scorer_t *scorer,
                       uint32_t doc_len,
                       const uint32_t *term_freqs,
                       const uint32_t *query_terms,
                       uint32_t num_query_terms);

/**
 * @brief 计算 BM25F 分数（字段加权）
 *
 * @param scorer 评分器
 * @param field_lens 各字段长度
 * @param field_freqs 各字段词频
 * @param query_terms 查询术语
 * @param num_query_terms 查询词数
 * @return BM25F 分数
 */
double bm25f_score(bm25_scorer_t *scorer,
                   const double *field_lens,
                   const uint32_t *field_freqs,
                   const uint32_t *query_terms,
                   uint32_t num_query_terms);

/**
 * @brief 批量计算 BM25 分数
 *
 * @param scorer 评分器
 * @param docs 文档统计数组
 * @param term_freqs_matrix 词频矩阵 [num_docs][num_terms]
 * @param query_terms 查询术语
 * @param num_query_terms 查询词数
 * @param results 结果数组（调用者分配，按分数降序）
 * @param max_results 最大结果数
 * @return 实际结果数
 */
uint32_t bm25_batch_score(bm25_scorer_t *scorer,
                          const bm25_doc_stats_t *docs,
                          const uint32_t *term_freqs_matrix,
                          const uint32_t *query_terms,
                          uint32_t num_query_terms,
                          bm25_batch_result_t *results,
                          uint32_t max_results);

/* ========================================================================
 * IDF 计算 API
 * ======================================================================== */

/**
 * @brief 计算 IDF（改进公式）
 *
 * @param scorer 评分器
 * @param term_id 术语 ID
 * @return IDF 值
 */
double bm25_idf(bm25_scorer_t *scorer, uint32_t term_id);

/**
 * @brief 获取 IDF 缓存的命中率
 *
 * @param scorer 评分器
 * @param out_hits 输出命中次数
 * @param out_misses 输出未命中次数
 */
void bm25_get_cache_stats(const bm25_scorer_t *scorer,
                          uint32_t *out_hits, uint32_t *out_misses);

/* ========================================================================
 * 配置 API
 * ======================================================================== */

/**
 * @brief 设置 BM25+ delta 参数
 *
 * @param scorer 评分器
 * @param delta delta 值
 */
void bm25_set_delta(bm25_scorer_t *scorer, double delta);

/**
 * @brief 添加字段权重
 *
 * @param scorer 评分器
 * @param field_name 字段名
 * @param weight 权重
 */
void bm25_add_field_weight(bm25_scorer_t *scorer,
                           const char *field_name, double weight);

#ifdef __cplusplus
}
#endif

#endif /* DB_DOC_BM25_H */
