/**
 * @file tokenizer.h
 * @brief 多语言分词器接口
 *
 * Phase12 - 实现多语言分词器，追赶 Elasticsearch/Jieba 水平。
 */
#ifndef DB_INDEX_FULLTEXT_TOKENIZER_H
#define DB_INDEX_FULLTEXT_TOKENIZER_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 分词器类型 */
typedef enum {
    TOKENIZER_WHITESPACE = 0,
    TOKENIZER_SIMPLE = 1,
    TOKENIZER_CHINESE_MM = 2,
    TOKENIZER_CHINESE_JIEBA = 3,
    TOKENIZER_ICU = 4,
    TOKENIZER_PORTER = 5
} tokenizer_type_t;

/** 分词器配置 */
typedef struct {
    tokenizer_type_t type;
    bool lowercase;           /**< 转小写 */
    bool remove_stop_words;  /**< 移除停用词 */
    size_t min_token_length;  /**< 最小词长 */
    size_t max_token_length;  /**< 最大词长 */
} tokenizer_config_t;

/** 分词器不透明类型 */
typedef struct tokenizer tokenizer_t;

/** 分词结果 */
typedef struct {
    char **tokens;
    size_t num_tokens;
    size_t capacity;
} token_list_t;

/** 停用词集合 */
typedef struct stop_words stop_words_t;

/**
 * @brief 创建分词器
 */
tokenizer_t *tokenizer_create(tokenizer_type_t type, const tokenizer_config_t *config);

/**
 * @brief 创建中文分词器
 */
tokenizer_t *tokenizer_create_chinese(const char *dict_path);

/**
 * @brief 销毁分词器
 */
void tokenizer_destroy(tokenizer_t *tokenizer);

/**
 * @brief 分词
 */
token_list_t *tokenize(tokenizer_t *tokenizer, const char *text, size_t text_len);

/**
 * @brief 释放分词结果
 */
void token_list_free(token_list_t *list);

/**
 * @brief 创建停用词集合
 */
stop_words_t *stop_words_create(void);

/**
 * @brief 添加停用词
 */
void stop_words_add(stop_words_t *stop_words, const char *word);

/**
 * @brief 检查是否为停用词
 */
bool is_stop_word(const stop_words_t *stop_words, const char *word);

#ifdef __cplusplus
}
#endif

#endif /* DB_INDEX_FULLTEXT_TOKENIZER_H */
