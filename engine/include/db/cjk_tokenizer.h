/**
 * @file cjk_tokenizer.h
 * @brief 中文词典分词 + Snowball 词干化（C2-6）
 *
 * 自研实现：FMM/RMM 双向校验 + 词典可插拔 + Snowball English Porter2 移植。
 */
#ifndef DB_CJK_TOKENIZER_H
#define DB_CJK_TOKENIZER_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 词典加载：每行一个词 */
typedef struct cjk_dict_s cjk_dict_t;

cjk_dict_t *cjk_dict_load(const char *path);
void cjk_dict_free(cjk_dict_t *dict);
bool cjk_dict_contains(const cjk_dict_t *dict, const char *word, size_t len);

/* FMM + RMM 双向校验分词 */
typedef struct cjk_token_s {
    char *text;       /* 拷贝到独立 buffer */
    size_t start;     /* 原文偏移 */
    size_t end;       /* 原文偏移+长度 */
} cjk_token_t;

typedef struct cjk_token_list_s {
    cjk_token_t *tokens;
    size_t n_tokens;
} cjk_token_list_t;

cjk_token_list_t cjk_tokenize(const cjk_dict_t *dict, const char *text, size_t len);
void cjk_token_list_free(cjk_token_list_t *list);

/* Snowball English (Porter2) 词干化 */
char *snowball_porter2_stem(const char *word, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* DB_CJK_TOKENIZER_H */