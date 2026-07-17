/*
 * dict.h
 *
 * 轻量中文分词词典接口。
 */

#ifndef DICT_DICT_H
#define DICT_DICT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dict dict_t;
typedef struct dict_hmm_model dict_hmm_model_t;

typedef struct token {
    char *text;
    int32_t byte_start;
    int32_t byte_length;
    bool is_ascii;
} token_t;

dict_t *dict_create(void);
dict_t *dict_create_default(void);
int32_t dict_load_default(dict_t *dict);
int32_t dict_load_jieba_file(dict_t *dict, const char *path);
int32_t dict_load_stop_words_file(dict_t *dict, const char *path);
int32_t dict_load_synonyms_file(dict_t *dict, const char *path);
int32_t dict_add_word(dict_t *dict, const char *word, int32_t freq);
int32_t dict_add_stop_word(dict_t *dict, const char *word);
int32_t dict_add_synonym(dict_t *dict, const char *word, const char *canonical_word);
bool dict_is_stop_word(const dict_t *dict, const char *word);
const char *dict_resolve_synonym(const dict_t *dict, const char *word);
int32_t dict_normalize_term(const dict_t *dict, const char *term, char **normalized_term);
int32_t dict_cut(const dict_t *dict, const char *text, token_t **tokens, int32_t *token_count);
int32_t dict_cut_for_search(const dict_t *dict, const char *text, token_t **tokens, int32_t *token_count);
void dict_free_tokens(token_t *tokens, int32_t token_count);
int32_t dict_enable_hmm(dict_t *dict, bool enabled);
int32_t dict_set_hmm_model(dict_t *dict, const dict_hmm_model_t *model);
int32_t dict_size(const dict_t *dict);
void dict_reset(dict_t *dict);
void dict_drop(dict_t *dict);

#ifdef __cplusplus
}
#endif

#endif