/**
 * @file bm25_index.h
 * @brief BM25 全文检索索引
 *
 * 实现 Okapi BM25 算法，用于文本检索评分。
 * 支持文档索引、TF-IDF 评分、BM25 公式计算。
 */
#ifndef DB_BM25_INDEX_H
#define DB_BM25_INDEX_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * BM25 配置结构
 * ======================================================================== */

/**
 * @brief BM25 参数配置
 */
typedef struct bm25_config_s {
    float k1;               /**< 词频饱和参数 (default 1.2) */
    float b;                /**< 文档长度归一化参数 (default 0.75) */
} bm25_config_t;

/* ========================================================================
 * BM25 词条结构
 * ======================================================================== */

/**
 * @brief 词条信息
 */
typedef struct bm25_term_s {
    char *term;             /**< 词项文本（堆分配） */
    uint32_t doc_freq;      /**< 包含此词的文档数 */
    uint64_t total_tf;      /**< 此词在所有文档中的总出现次数 */
} bm25_term_t;

/* ========================================================================
 * BM25 文档结构
 * ======================================================================== */

/**
 * @brief 文档信息
 */
typedef struct bm25_doc_s {
    uint64_t doc_id;        /**< 文档标识 */
    uint32_t length;        /**< 文档长度（词数） */
    float avg_dl;           /**< 平均文档长度（由索引维护） */
} bm25_doc_t;

/* ========================================================================
 * BM25 索引结构
 * ======================================================================== */

/**
 * @brief BM25 索引
 *
 * 包含词典（terms）和文档信息（docs），支持增量添加文档。
 */
typedef struct bm25_index_s {
    bm25_term_t *terms;     /**< 词条数组 */
    uint32_t term_count;    /**< 当前词条数量 */
    uint32_t term_capacity; /**< 词条数组容量 */

    bm25_doc_t *docs;       /**< 文档数组 */
    uint32_t doc_count;     /**< 当前文档数量 */
    uint32_t doc_capacity;  /**< 文档数组容量 */

    bm25_config_t config;   /**< BM25 参数 */

    /* 统计 */
    uint64_t total_terms;   /**< 所有文档的总词数 */
    float avg_dl;           /**< 平均文档长度 */
} bm25_index_t;

/* ========================================================================
 * BM25 API
 * ======================================================================== */

/**
 * @brief 创建 BM25 索引
 * @param config BM25 参数配置
 * @return 成功返回索引指针，失败返回 NULL
 */
bm25_index_t* bm25_index_create(bm25_config_t config);

/**
 * @brief 释放 BM25 索引
 * @param index 索引指针（可为 NULL）
 */
void bm25_index_free(bm25_index_t *index);

/**
 * @brief 向索引添加文档
 * @param index BM25 索引
 * @param doc_id 文档标识
 * @param text 文档文本内容
 * @return 0 成功，-1 参数错误或内存不足
 */
int bm25_index_add_document(bm25_index_t *index, uint64_t doc_id, const char *text);

/**
 * @brief 计算文档对查询的 BM25 分数
 * @param index BM25 索引
 * @param doc_id 文档标识
 * @param query 查询文本（空格分隔的词项）
 * @return BM25 分数，文档不存在或查询为空返回 0
 */
float bm25_score(const bm25_index_t *index, uint64_t doc_id, const char *query);

/**
 * @brief 搜索并返回 top-k 结果
 * @param index BM25 索引
 * @param query 查询文本
 * @param top_k 返回结果数量上限
 * @param results 输出文档 ID 数组（调用者分配，长度 >= top_k）
 * @param scores 输出分数数组（调用者分配，长度 >= top_k，可为 NULL）
 * @return 实际返回的结果数量
 */
int bm25_search(const bm25_index_t *index, const char *query, uint32_t top_k,
                uint64_t *results, float *scores);

#ifdef __cplusplus
}
#endif

#endif /* DB_BM25_INDEX_H */
