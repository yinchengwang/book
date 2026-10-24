#ifndef TEXT_RETRIEVER_H
#define TEXT_RETRIEVER_H

#include <algo-prod/dict/dict.h>
#include <db/index/vector_index/BM25/bm25.h>

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct text_retriever text_retriever_t;

typedef struct text_retriever_config {
    bm25_params_t bm25_params;
    dict_t *tokenizer;
    bool take_tokenizer_ownership;
} text_retriever_config_t;

typedef struct text_retriever_document {
    const char *external_id;
    const char *stored_text;
    bm25_document_input_t content;
} text_retriever_document_t;

typedef struct text_retriever_hit {
    int32_t internal_id;
    const char *external_id;
    const char *stored_text;
    float score;
    char *highlighted_snippet;
    char **matched_terms;
    int32_t matched_term_count;
} text_retriever_hit_t;

void text_retriever_set_default_config(text_retriever_config_t *config);
text_retriever_t *text_retriever_create(void);
text_retriever_t *text_retriever_create_with_config(const text_retriever_config_t *config);
int32_t text_retriever_add_document(text_retriever_t *retriever, const text_retriever_document_t *document);
int32_t text_retriever_add_documents(text_retriever_t *retriever,
                                     int32_t document_count,
                                     const text_retriever_document_t *documents,
                                     int32_t *internal_ids);
int32_t text_retriever_update_document(text_retriever_t *retriever,
                                       const char *external_id,
                                       const text_retriever_document_t *document);
int32_t text_retriever_delete_document(text_retriever_t *retriever, const char *external_id);
int32_t text_retriever_search(const text_retriever_t *retriever,
                              const bm25_query_input_t *query,
                              int32_t k,
                              text_retriever_hit_t *hits,
                              int32_t *hit_count);
int32_t text_retriever_search_batch(const text_retriever_t *retriever,
                                    int32_t query_count,
                                    const bm25_query_input_t *queries,
                                    int32_t k,
                                    text_retriever_hit_t *hits,
                                    int32_t *hit_counts);
void text_retriever_free_hits(text_retriever_hit_t *hits, int32_t hit_capacity);
int32_t text_retriever_size(const text_retriever_t *retriever);
int32_t text_retriever_get_params(const text_retriever_t *retriever, bm25_params_t *params);
dict_t *text_retriever_get_tokenizer(const text_retriever_t *retriever);
const bm25_t *text_retriever_get_bm25(const text_retriever_t *retriever);
void text_retriever_drop(text_retriever_t *retriever);

#ifdef __cplusplus
}
#endif

#endif