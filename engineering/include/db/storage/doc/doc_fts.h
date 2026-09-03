/**
 * @file doc_fts.h
 * @brief 全文检索增强头文件
 *
 * 实现分词器插件接口、默认分词器、短语搜索、高亮、同义词和停用词
 */
#ifndef DB_DOC_FTS_H
#define DB_DOC_FTS_H

#include "db/storage/doc/doc_engine.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 分词器类型
 * ======================================================================== */

/**
 * @brief 分词器类型
 */
typedef enum DocTokenizerType_e {
    DOC_TOKENIZER_STANDARD = 0,  /**< 标准分词器 */
    DOC_TOKENIZER_WHITESPACE,    /**< 空白分词器 */
    DOC_TOKENIZER_SIMPLE,        /**< 简单分词器 */
    DOC_TOKENIZER_KEYWORD,       /**< 关键字分词器 */
    DOC_TOKENIZER_STEM,          /**< 词干分词器 */
    DOC_TOKENIZER_CJK,           /**< CJK 分词器 */
    DOC_TOKENIZER_CUSTOM,        /**< 自定义分词器 */
} DocTokenizerType;

/* ========================================================================
 * 分词器插件接口
 * ======================================================================== */

/** 分词器配置 */
typedef struct DocTokenizerConfig_s {
    DocTokenizerType type;      /**< 分词器类型 */
    char *stopwords_file;       /**< 停用词文件 */
    char *synonyms_file;        /**< 同义词文件 */
    int min_token_length;       /**< 最小词长度 */
    int max_token_length;       /**< 最大词长度 */
    bool case_sensitive;        /**< 大小写敏感 */
    bool remove_stopwords;      /**< 移除停用词 */
    char *ngram_range;          /**< N-gram 范围 */
} DocTokenizerConfig;

/** 词条 */
typedef struct DocToken_s {
    char *text;                 /**< 词条文本 */
    int start_offset;           /**< 起始偏移 */
    int end_offset;             /**< 结束偏移 */
    int position;               /**< 位置 */
    uint32_t flags;             /**< 标志位 */
} DocToken;

/** 分词器接口 */
typedef struct DocTokenizerOps_s {
    /** 初始化分词器 */
    int (*init)(void *tokenizer, const DocTokenizerConfig *config);

    /** 分词 */
    int (*tokenize)(void *tokenizer, const char *text, size_t len,
                   DocToken **out_tokens, size_t *out_count);

    /** 获取词条数量 */
    size_t (*count)(void *tokenizer, const char *text, size_t len);

    /** 重置分词器状态 */
    void (*reset)(void *tokenizer);

    /** 销毁分词器 */
    void (*destroy)(void *tokenizer);
} DocTokenizerOps;

/** 分词器 */
typedef struct DocTokenizer_s {
    DocTokenizerType type;      /**< 类型 */
    DocTokenizerConfig config;  /**< 配置 */
    DocTokenizerOps *ops;       /**< 操作函数表 */
    void *impl;                 /**< 实现数据 */
} DocTokenizer;

/**
 * @brief 创建分词器
 * @param type 分词器类型
 * @param config 配置（可为 NULL）
 * @return 分词器句柄
 */
DocTokenizer *doc_tokenizer_create(DocTokenizerType type,
                                   const DocTokenizerConfig *config);

/**
 * @brief 销毁分词器
 */
void doc_tokenizer_destroy(DocTokenizer *tokenizer);

/**
 * @brief 执行分词
 */
int doc_tokenizer_tokenize(DocTokenizer *tokenizer,
                          const char *text, size_t len,
                          DocToken **out_tokens, size_t *out_count);

/**
 * @brief 释放词条数组
 */
void doc_tokenizer_free_tokens(DocToken *tokens, size_t count);

/**
 * @brief 注册自定义分词器
 */
int doc_tokenizer_register(DocTokenizerType type, DocTokenizerOps *ops);

/**
 * @brief 获取分词器类型名称
 */
const char *doc_tokenizer_type_name(DocTokenizerType type);

/* ========================================================================
 * 停用词
 * ======================================================================== */

/** 停用词表 */
typedef struct DocStopwords_s {
    char **words;               /**< 停用词数组 */
    size_t num_words;           /**< 停用词数量 */
    bool case_sensitive;        /**< 大小写敏感 */
} DocStopwords;

/**
 * @brief 创建停用词表
 */
DocStopwords *doc_stopwords_create(bool case_sensitive);

/**
 * @brief 从文件加载停用词
 */
int doc_stopwords_load(DocStopwords *sw, const char *filename);

/**
 * @brief 添加停用词
 */
int doc_stopwords_add(DocStopwords *sw, const char *word);

/**
 * @brief 检查是否是停用词
 */
bool doc_stopwords_contains(DocStopwords *sw, const char *word);

/**
 * @brief 销毁停用词表
 */
void doc_stopwords_destroy(DocStopwords *sw);

/* ========================================================================
 * 同义词
 * ======================================================================== */

/** 同义词组 */
typedef struct DocSynonymGroup_s {
    char **terms;               /**< 同义词列表 */
    size_t num_terms;           /**< 词数量 */
} DocSynonymGroup;

/** 同义词表 */
typedef struct DocSynonyms_s {
    DocSynonymGroup *groups;    /**< 同义词组数组 */
    size_t num_groups;          /**< 组数量 */
    void *trie;                 /**< Trie 树索引 */
} DocSynonyms;

/**
 * @brief 创建同义词表
 */
DocSynonyms *doc_synonyms_create(void);

/**
 * @brief 从文件加载同义词
 */
int doc_synonyms_load(DocSynonyms *syn, const char *filename);

/**
 * @brief 添加同义词组
 */
int doc_synonyms_add_group(DocSynonyms *syn, char **terms, size_t num_terms);

/**
 * @brief 获取同义词
 * @param syn 同义词表
 * @param term 查询词
 * @param out_terms 输出同义词数组
 * @return 同义词数量
 */
size_t doc_synonyms_get(DocSynonyms *syn, const char *term,
                       char ***out_terms);

/**
 * @brief 销毁同义词表
 */
void doc_synonyms_destroy(DocSynonyms *syn);

/* ========================================================================
 * 短语搜索
 * ======================================================================== */

/** 短语搜索结果 */
typedef struct DocPhraseResult_s {
    char *document_id;          /**< 文档 ID */
    int start_offset;           /**< 起始偏移 */
    int end_offset;             /**< 结束偏移 */
    int match_length;           /**< 匹配长度 */
    double score;               /**< 相关性分数 */
} DocPhraseResult;

/**
 * @brief 短语搜索
 * @param documents 文档数组
 * @param num_docs 文档数量
 * @param phrase 要搜索的短语
 * @param tokenizer 分词器
 * @param results 输出结果
 * @param max_results 最大结果数
 * @return 匹配数量
 */
size_t doc_phrase_search(const char **documents,
                        size_t num_docs,
                        const char *phrase,
                        DocTokenizer *tokenizer,
                        DocPhraseResult **results,
                        size_t max_results);

/**
 * @brief 释放短语搜索结果
 */
void doc_phrase_result_free(DocPhraseResult *results, size_t count);

/* ========================================================================
 * 高亮
 * ======================================================================== */

/** 高亮标记 */
typedef struct DocHighlightMarker_s {
    char pre_tag[32];           /**< 开始标记 */
    char post_tag[32];          /**< 结束标记 */
} DocHighlightMarker;

/** 高亮片段 */
typedef struct DocHighlightFragment_s {
    char *text;                 /**< 高亮文本 */
    int start_offset;           /**< 起始偏移 */
    int end_offset;             /**< 结束偏移 */
    int num_matches;            /**< 匹配数量 */
} DocHighlightFragment;

/**
 * @brief 默认高亮标记
 */
extern DocHighlightMarker DOC_HIGHLIGHT_DEFAULT;

/**
 * @brief HTML 高亮标记
 */
extern DocHighlightMarker DOC_HIGHLIGHT_HTML;

/**
 * @brief 高亮搜索结果
 * @param text 原始文本
 * @param query 查询词
 * @param tokenizer 分词器
 * @param marker 高亮标记
 * @param max_fragments 最大片段数
 * @param fragment_size 片段大小
 * @param out_fragments 输出片段
 * @return 片段数量
 */
size_t doc_highlight(const char *text,
                    const char *query,
                    DocTokenizer *tokenizer,
                    const DocHighlightMarker *marker,
                    size_t max_fragments,
                    size_t fragment_size,
                    DocHighlightFragment **out_fragments);

/**
 * @brief 释放高亮片段
 */
void doc_highlight_free(DocHighlightFragment *fragments, size_t count);

/* ========================================================================
 * 全文检索器
 * ======================================================================== */

/** 全文检索配置 */
typedef struct DocFtsConfig_s {
    DocTokenizerType tokenizer_type; /**< 分词器类型 */
    DocTokenizerConfig tokenizer_config; /**< 分词器配置 */
    bool enable_synonyms;        /**< 启用同义词 */
    bool enable_stopwords;       /**< 启用停用词 */
    int min_match;               /**< 最小匹配词数 */
    double fuzziness;            /**< 模糊度 */
} DocFtsConfig;

/** 全文检索器 */
typedef struct DocFtsSearcher_s {
    DocTokenizer *tokenizer;     /**< 分词器 */
    DocStopwords *stopwords;     /**< 停用词表 */
    DocSynonyms *synonyms;       /**< 同义词表 */
    DocFtsConfig config;         /**< 配置 */
    void *mem_pool;              /**< 内存池 */
} DocFtsSearcher;

/**
 * @brief 创建全文检索器
 * @param config 配置（可为 NULL）
 * @return 检索器句柄
 */
DocFtsSearcher *doc_fts_create(const DocFtsConfig *config);

/**
 * @brief 销毁全文检索器
 */
void doc_fts_destroy(DocFtsSearcher *searcher);

/**
 * @brief 设置停用词表
 */
int doc_fts_set_stopwords(DocFtsSearcher *searcher, DocStopwords *stopwords);

/**
 * @brief 设置同义词表
 */
int doc_fts_set_synonyms(DocFtsSearcher *searcher, DocSynonyms *synonyms);

/**
 * @brief 搜索
 * @param searcher 检索器
 * @param query 查询词
 * @param documents 文档数组
 * @param num_docs 文档数量
 * @param results 输出结果
 * @param max_results 最大结果数
 * @return 匹配数量
 */
size_t doc_fts_search(DocFtsSearcher *searcher,
                     const char *query,
                     const char **documents,
                     size_t num_docs,
                     DocPhraseResult **results,
                     size_t max_results);

/* ========================================================================
 * SQL 函数
 * ======================================================================== */

/**
 * @brief MATCH 函数
 *
 * MATCH(text, query)
 *
 * 示例:
 *   SELECT * FROM documents WHERE MATCH(content, 'database')
 */
bool doc_sql_match(const char *text, const char *query);

/**
 * @brief HIGHLIGHT 函数
 *
 * HIGHLIGHT(text, query)
 *
 * 示例:
 *   SELECT HIGHLIGHT(content, 'search term') FROM documents
 */
char *doc_sql_highlight(const char *text, const char *query);

#ifdef __cplusplus
}
#endif

#endif /* DB_DOC_FTS_H */
