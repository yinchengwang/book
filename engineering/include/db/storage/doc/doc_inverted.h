/**
 * @file doc_inverted.h
 * @brief 文档倒排索引
 *
 * 实现高效的文档全文搜索索引，支持：
 * - FST 术语字典
 * - 倒排列表存储
 * - 增量编码压缩
 * - BM25 打分
 */
#ifndef DB_DOC_INVERTED_H
#define DB_DOC_INVERTED_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 常量定义
 * ======================================================================== */

/** 倒排索引魔数 */
#define DOC_INVERTED_MAGIC 0x44495658  /* "DIVX" */

/** 倒排索引版本 */
#define DOC_INVERTED_VERSION 1

/** 默认分词器类型 */
#define DOC_INVERTED_DEFAULT_TOKENIZER "simple"

/** 术语最大长度 */
#define DOC_INVERTED_MAX_TERM_LEN 128

/** 单个倒排列表初始容量 */
#define DOC_INVERTED_LIST_INITIAL_CAP 32

/* ========================================================================
 * 类型定义
 * ======================================================================== */

/**
 * @brief 倒排列表条目
 */
typedef struct doc_inverted_entry_s {
    uint64_t doc_id;          /**< 文档 ID */
    uint32_t freq;            /**< 词频 */
    uint32_t *positions;      /**< 位置数组 */
    uint32_t pos_count;       /**< 位置数 */
} doc_inverted_entry_t;

/**
 * @brief 倒排列表
 */
typedef struct doc_inverted_list_s {
    doc_inverted_entry_t *entries;    /**< 条目数组 */
    uint32_t count;                   /**< 条目数 */
    uint32_t capacity;                /**< 容量 */
} doc_inverted_list_t;

/**
 * @brief 术语信息
 */
typedef struct doc_term_info_s {
    uint32_t term_id;         /**< 术语 ID */
    uint32_t doc_freq;        /**< 文档频率 */
    uint64_t posting_offset;  /**< 倒排列表偏移 */
} doc_term_info_t;

/**
 * @brief 倒排索引
 */
typedef struct doc_inverted_index_s {
    char data_dir[512];               /**< 数据目录 */
    char tokenizer[64];               /**< 分词器类型 */

    /* 术语字典 */
    void *fst_dict;                  /**< FST 字典（简化实现） */
    doc_term_info_t *terms;          /**< 术语信息数组 */
    uint32_t term_count;             /**< 术语数 */
    uint32_t term_capacity;          /**< 术语容量 */

    /* 倒排列表 */
    doc_inverted_list_t *postings;   /**< 倒排列表数组 */
    FILE *posting_file;              /**< 倒排列表文件 */

    /* 文档存储 */
    FILE *doc_file;                  /**< 文档文件 */
    uint64_t doc_count;              /**< 文档数 */
    uint64_t doc_store_offset;       /**< 文档存储偏移 */

    /* 统计 */
    uint64_t index_size_bytes;       /**< 索引大小 */
} doc_inverted_index_t;

/**
 * @brief 查询结果
 */
typedef struct doc_inverted_result_s {
    uint64_t doc_id;         /**< 文档 ID */
    double score;            /**< 相关性分数 */
    const char *snippet;     /**< 片段（可选） */
} doc_inverted_result_t;

/* ========================================================================
 * 生命周期 API
 * ======================================================================== */

/**
 * @brief 创建倒排索引
 *
 * @param data_dir 数据目录
 * @param tokenizer 分词器类型
 * @return 倒排索引句柄，失败返回 NULL
 */
doc_inverted_index_t *doc_inverted_create(const char *data_dir,
                                           const char *tokenizer);

/**
 * @brief 打开已存在的倒排索引
 *
 * @param data_dir 数据目录
 * @return 倒排索引句柄，失败返回 NULL
 */
doc_inverted_index_t *doc_inverted_open(const char *data_dir);

/**
 * @brief 保存并关闭倒排索引
 *
 * @param index 倒排索引句柄
 * @return 0 成功，-1 失败
 */
int doc_inverted_close(doc_inverted_index_t *index);

/**
 * @brief 释放倒排索引（不保存）
 *
 * @param index 倒排索引句柄
 */
void doc_inverted_free(doc_inverted_index_t *index);

/* ========================================================================
 * 索引操作 API
 * ======================================================================== */

/**
 * @brief 索引文档
 *
 * @param index 倒排索引句柄
 * @param doc_id 文档 ID
 * @param doc_content 文档内容
 * @return 0 成功，-1 失败
 */
int doc_inverted_add(doc_inverted_index_t *index, uint64_t doc_id,
                     const char *doc_content);

/**
 * @brief 移除文档
 *
 * @param index 倒排索引句柄
 * @param doc_id 文档 ID
 * @return 0 成功，-1 失败
 */
int doc_inverted_remove(doc_inverted_index_t *index, uint64_t doc_id);

/**
 * @brief 查询
 *
 * @param index 倒排索引句柄
 * @param query 查询字符串
 * @param results 结果数组（调用者分配）
 * @param max_results 最大结果数
 * @return 实际结果数
 */
uint32_t doc_inverted_search(const doc_inverted_index_t *index,
                              const char *query,
                              doc_inverted_result_t *results,
                              uint32_t max_results);

/**
 * @brief 获取文档
 *
 * @param index 倒排索引句柄
 * @param doc_id 文档 ID
 * @param out_content 输出内容（调用者负责释放）
 * @return 0 成功，-1 失败
 */
int doc_inverted_get_doc(const doc_inverted_index_t *index, uint64_t doc_id,
                         char **out_content);

/* ========================================================================
 * 工具函数
 * ======================================================================== */

/**
 * @brief 简单分词器
 *
 * @param text 输入文本
 * @param callback 回调函数
 * @param ctx 回调上下文
 */
void doc_simple_tokenize(const char *text,
                         void (*callback)(const char *term, uint32_t pos, void *ctx),
                         void *ctx);

/**
 * @brief 计算字符串的哈希值（用于简化字典）
 *
 * @param str 字符串
 * @return 哈希值
 */
uint64_t doc_term_hash(const char *str);

#ifdef __cplusplus
}
#endif

#endif /* DB_DOC_INVERTED_H */
