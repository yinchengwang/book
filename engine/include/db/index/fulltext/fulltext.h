/*
 * fulltext.h
 *
 * FULLTEXT 全文索引公共 API
 *
 * 功能：
 * - TF-IDF 评分排序
 * - 支持多种分词器（空格分词、中文 MM 分词）
 * - 高级搜索语法（AND/OR）
 * - 结果高亮
 *
 * 使用方式：
 *   fulltext_index_t *idx = fulltext_create();
 *   fulltext_set_tokenizer(idx, TOKENIZER_WHITESPACE);
 *   fulltext_index_doc(idx, doc_id, "文档内容");
 *   fulltext_search(idx, "查询词", doc_ids, scores, &count, 10);
 *   fulltext_destroy(idx);
 */

#ifndef FULLTEXT_H
#define FULLTEXT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/* ── 类型定义 ── */

/* 分词器类型 */
typedef enum {
    TOKENIZER_WHITESPACE = 0,  /* 空格分词（适用于英文） */
    TOKENIZER_CHINESE_MM,      /* 中文 MM 分词（最大匹配） */
    TOKENIZER_MIXED            /* 混合分词（英文+中文） */
} tokenizer_type_t;

/* 分词器句柄 */
typedef struct tokenizer tokenizer_t;

/* 全文索引句柄 */
typedef struct fulltext_index fulltext_index_t;

/* ── 分词器 API ── */

/**
 * 创建分词器
 * @param type 分词器类型
 * @return 分词器句柄，失败返回 NULL
 */
tokenizer_t *tokenizer_create(tokenizer_type_t type);

/**
 * 对文本进行分词
 * @param tok 分词器句柄
 * @param text 待分词文本
 * @param count 输出：词条数量
 * @return 词条数组（需用 tokenizer_free_tokens 释放），失败返回 NULL
 */
char **tokenize(const tokenizer_t *tok, const char *text, int *count);

/**
 * 释放分词结果
 * @param tok 分词器句柄
 * @param tokens 词条数组
 * @param count 词条数量
 */
void tokenizer_free_tokens(const tokenizer_t *tok, char **tokens, int count);

/**
 * 销毁分词器
 * @param tok 分词器句柄
 */
void tokenizer_destroy(tokenizer_t *tok);

/* ── 全文索引 API ── */

/**
 * 创建全文索引
 * @return 索引句柄，失败返回 NULL
 */
fulltext_index_t *fulltext_create(void);

/**
 * 设置分词器类型
 * @param idx 索引句柄
 * @param type 分词器类型
 */
void fulltext_set_tokenizer(fulltext_index_t *idx, tokenizer_type_t type);

/**
 * 设置词条长度范围
 * @param idx 索引句柄
 * @param min_len 最小长度（<=0 忽略）
 * @param max_len 最大长度（<=0 忽略）
 */
void fulltext_set_word_length(fulltext_index_t *idx, int min_len, int max_len);

/**
 * 索引单个文档
 * @param idx 索引句柄
 * @param doc_id 文档 ID
 * @param text 文档文本
 * @return 0 成功，-1 失败
 */
int fulltext_index_doc(fulltext_index_t *idx, int doc_id, const char *text);

/**
 * 批量索引文档
 * @param idx 索引句柄
 * @param doc_ids 文档 ID 数组
 * @param texts 文档文本数组
 * @param count 文档数量
 * @return 成功索引的文档数
 */
int fulltext_index_batch(fulltext_index_t *idx, const int *doc_ids,
                         const char **texts, int count);

/**
 * 搜索文档
 * @param idx 索引句柄
 * @param query 查询语句（支持 "word1 AND word2"、"word1 OR word2"）
 * @param doc_ids 输出：匹配的文档 ID 数组
 * @param scores 输出：每个文档的相关性得分
 * @param count 输出：匹配数量
 * @param max_results 最大返回数量
 * @return 0 成功，-1 失败
 */
int fulltext_search(const fulltext_index_t *idx, const char *query,
                    int *doc_ids, float *scores, int *count, int max_results);

/**
 * 生成高亮文本
 * @param idx 索引句柄
 * @param text 原文
 * @param query 查询词
 * @param output 输出缓冲区
 * @param max_len 缓冲区最大长度
 * @return 0 成功，-1 失败
 */
int fulltext_highlight(const fulltext_index_t *idx, const char *text,
                       const char *query, char *output, int max_len);

/**
 * 获取索引统计信息
 * @param idx 索引句柄
 * @param out_n_docs 输出：文档数量
 * @param out_n_terms 输出：词条数量
 * @param out_avg_doc_len 输出：平均文档长度
 */
void fulltext_stats(const fulltext_index_t *idx, int *out_n_docs,
                    int *out_n_terms, float *out_avg_doc_len);

/**
 * 销毁全文索引
 * @param idx 索引句柄
 */
void fulltext_destroy(fulltext_index_t *idx);

#ifdef __cplusplus
}
#endif

#endif /* FULLTEXT_H */