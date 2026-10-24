/*
 * bm25.h
 *
 * BM25 文本倒排索引接口。
 * 支持两种输入形式：调用方已切好的 token 序列，或者待内部词典分词的原始文本。
 */

#ifndef BM25_INDEX_H
#define BM25_INDEX_H

#include <algo-prod/dict/dict.h>

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bm25 bm25_t;

typedef enum bm25_search_algorithm {
    BM25_SEARCH_ALGORITHM_AUTO = 0,
    BM25_SEARCH_ALGORITHM_TAAT = 1,
    BM25_SEARCH_ALGORITHM_DAAT = 2,
} bm25_search_algorithm_t;

typedef struct bm25_search_stats {
    bm25_search_algorithm_t algorithm;
    int32_t block_skip_count;
    int32_t skipped_postings_count;
    int32_t scored_doc_count;
    int32_t candidate_count;         /* 进入 Top-K 堆的候选文档数。 */
    int32_t heap_compare_count;      /* 堆比较次数。 */
    int64_t time_cost_us;            /* 搜索耗时（微秒）。 */
} bm25_search_stats_t;

typedef struct bm25_params {
    float k1;
    float b;
    int32_t candidate_queue_size;
    int32_t posting_filter_window;
    int32_t daat_threshold;
    bm25_search_algorithm_t search_algorithm;
} bm25_params_t;

typedef enum bm25_input_kind {
    BM25_INPUT_KIND_TOKENS = 0,
    BM25_INPUT_KIND_TEXT = 1,
} bm25_input_kind_t;

typedef struct bm25_token_input {
    int32_t term_count;
    const char *const *terms;
} bm25_token_input_t;

typedef struct bm25_document_input {
    bm25_input_kind_t kind;
    union {
        bm25_token_input_t tokens;
        const char *text;
    } value;
} bm25_document_input_t;

typedef bm25_document_input_t bm25_query_input_t;

bm25_t *bm25_index_create(void);
bm25_t *bm25_index_create_with_params(const bm25_params_t *params);
bm25_t *bm25_index_create_with_tokenizer(const bm25_params_t *params, dict_t *tokenizer);
int32_t bm25_index_add(bm25_t *index, const bm25_document_input_t *document);
int32_t bm25_index_add_document(bm25_t *index, int32_t term_count, const char *const *terms);
int32_t bm25_index_add_text(bm25_t *index, const char *text);
int32_t bm25_index_add_documents(bm25_t *index, int32_t document_count,
                                  const bm25_document_input_t *documents, int32_t *doc_ids);
int32_t bm25_delete_document(bm25_t *index, int32_t doc_id);
int32_t bm25_index_search(const bm25_t *index,
                          int32_t query_term_count,
                          const char *const *query_terms,
                          int32_t k,
                          float *scores,
                          int32_t *doc_ids);
int32_t bm25_index_search_query(const bm25_t *index,
                                const bm25_query_input_t *query,
                                int32_t k,
                                float *scores,
                                int32_t *doc_ids);
int32_t bm25_index_search_text(const bm25_t *index,
                               const char *query,
                               int32_t k,
                               float *scores,
                               int32_t *doc_ids);
int32_t bm25_index_search_with_count(const bm25_t *index,
                                     int32_t query_term_count,
                                     const char *const *query_terms,
                                     int32_t k,
                                     float *scores,
                                     int32_t *doc_ids,
                                     int32_t *hit_count);
int32_t bm25_index_search_query_with_count(const bm25_t *index,
                                           const bm25_query_input_t *query,
                                           int32_t k,
                                           float *scores,
                                           int32_t *doc_ids,
                                           int32_t *hit_count);
int32_t bm25_index_search_text_with_count(const bm25_t *index,
                                          const char *query,
                                          int32_t k,
                                          float *scores,
                                          int32_t *doc_ids,
                                          int32_t *hit_count);
void bm25_index_reset(bm25_t *index);
int32_t bm25_index_size(const bm25_t *index);
double bm25_index_average_doc_length(const bm25_t *index);
int32_t bm25_index_doc_length(const bm25_t *index, int32_t doc_id);
int32_t bm25_index_set_params(bm25_t *index, const bm25_params_t *params);
int32_t bm25_index_get_params(const bm25_t *index, bm25_params_t *params);
int32_t bm25_index_get_last_search_stats(const bm25_t *index, bm25_search_stats_t *stats);
int32_t bm25_index_set_tokenizer(bm25_t *index, dict_t *tokenizer);
dict_t *bm25_index_get_tokenizer(const bm25_t *index);
int32_t bm25_save(const bm25_t *index, const char *path);
bm25_t *bm25_index_load(const char *path);
void bm25_index_drop(bm25_t *index);

#ifdef __cplusplus
}
#endif

#endif