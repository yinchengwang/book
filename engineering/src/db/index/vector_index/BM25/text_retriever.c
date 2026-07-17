#include <db/index/vector_index/BM25/text_retriever.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct text_retriever {
    bm25_t *index;
    dict_t *tokenizer;
    bool owns_tokenizer;
    bm25_params_t params;
    char **external_ids;
    char **stored_texts;
    bm25_input_kind_t *content_kinds;
    char **indexed_texts;
    char ***indexed_token_terms;
    int32_t *indexed_token_counts;
    int32_t doc_count;
    int32_t doc_capacity;
};

typedef struct text_retriever_term_list {
    char **items;
    int32_t count;
    int32_t capacity;
} text_retriever_term_list_t;

static void text_retriever_set_default_bm25_params(bm25_params_t *params)
{
    if (!params) {
        return;
    }

    params->k1 = 1.2f;
    params->b = 0.75f;
    params->candidate_queue_size = 64;
    params->posting_filter_window = 16;
    params->daat_threshold = 64;
    params->search_algorithm = BM25_SEARCH_ALGORITHM_AUTO;
}

static char *text_retriever_strdup(const char *value)
{
    size_t length;
    char *copy;

    if (!value) {
        return NULL;
    }

    length = strlen(value);
    copy = (char *)malloc(length + 1);
    if (!copy) {
        return NULL;
    }

    memcpy(copy, value, length + 1);
    return copy;
}

static char *text_retriever_dup_generated_id(int32_t doc_id)
{
    char buffer[32];
    int written;

    written = snprintf(buffer, sizeof(buffer), "%d", doc_id);
    if (written < 0 || written >= (int)sizeof(buffer)) {
        return NULL;
    }

    return text_retriever_strdup(buffer);
}

static void text_retriever_free_token_terms(char **terms, int32_t term_count)
{
    int32_t i;

    if (!terms || term_count < 0) {
        return;
    }

    for (i = 0; i < term_count; ++i) {
        free(terms[i]);
    }
    free(terms);
}

static void text_retriever_clear_slot(text_retriever_t *retriever, int32_t index)
{
    if (!retriever || index < 0 || index >= retriever->doc_capacity) {
        return;
    }

    free(retriever->external_ids[index]);
    free(retriever->stored_texts[index]);
    free(retriever->indexed_texts[index]);
    text_retriever_free_token_terms(retriever->indexed_token_terms[index], retriever->indexed_token_counts[index]);

    retriever->external_ids[index] = NULL;
    retriever->stored_texts[index] = NULL;
    retriever->content_kinds[index] = BM25_INPUT_KIND_TEXT;
    retriever->indexed_texts[index] = NULL;
    retriever->indexed_token_terms[index] = NULL;
    retriever->indexed_token_counts[index] = 0;
}

static void text_retriever_move_slot(text_retriever_t *retriever, int32_t destination, int32_t source)
{
    if (!retriever || destination < 0 || source < 0 || destination >= retriever->doc_capacity ||
        source >= retriever->doc_capacity || destination == source) {
        return;
    }

    retriever->external_ids[destination] = retriever->external_ids[source];
    retriever->stored_texts[destination] = retriever->stored_texts[source];
    retriever->content_kinds[destination] = retriever->content_kinds[source];
    retriever->indexed_texts[destination] = retriever->indexed_texts[source];
    retriever->indexed_token_terms[destination] = retriever->indexed_token_terms[source];
    retriever->indexed_token_counts[destination] = retriever->indexed_token_counts[source];

    retriever->external_ids[source] = NULL;
    retriever->stored_texts[source] = NULL;
    retriever->content_kinds[source] = BM25_INPUT_KIND_TEXT;
    retriever->indexed_texts[source] = NULL;
    retriever->indexed_token_terms[source] = NULL;
    retriever->indexed_token_counts[source] = 0;
}

static int32_t text_retriever_find_document_index(const text_retriever_t *retriever, const char *external_id)
{
    int32_t i;

    if (!retriever || !external_id) {
        return -1;
    }

    for (i = 0; i < retriever->doc_count; ++i) {
        if (retriever->external_ids[i] && strcmp(retriever->external_ids[i], external_id) == 0) {
            return i;
        }
    }

    return -1;
}

static bool text_retriever_external_id_exists(const text_retriever_t *retriever,
                                              const char *external_id,
                                              int32_t skip_index)
{
    int32_t i;

    if (!retriever || !external_id) {
        return false;
    }

    for (i = 0; i < retriever->doc_count; ++i) {
        if (i == skip_index) {
            continue;
        }
        if (retriever->external_ids[i] && strcmp(retriever->external_ids[i], external_id) == 0) {
            return true;
        }
    }

    return false;
}

static char *text_retriever_dup_unique_generated_id(const text_retriever_t *retriever)
{
    int32_t candidate;

    if (!retriever) {
        return NULL;
    }

    candidate = retriever->doc_count;
    while (candidate >= 0) {
        char *generated_id = text_retriever_dup_generated_id(candidate);

        if (!generated_id) {
            return NULL;
        }
        if (!text_retriever_external_id_exists(retriever, generated_id, -1)) {
            return generated_id;
        }
        free(generated_id);
        candidate += 1;
    }

    return NULL;
}

static int text_retriever_clone_token_terms(int32_t term_count,
                                            const char *const *terms,
                                            char ***terms_out)
{
    char **copies;
    int32_t i;

    if (!terms_out || term_count < 0 || (term_count > 0 && !terms)) {
        return -1;
    }

    *terms_out = NULL;
    if (term_count == 0) {
        return 0;
    }

    copies = (char **)calloc((size_t)term_count, sizeof(char *));
    if (!copies) {
        return -1;
    }

    for (i = 0; i < term_count; ++i) {
        if (!terms[i]) {
            continue;
        }
        copies[i] = text_retriever_strdup(terms[i]);
        if (!copies[i]) {
            text_retriever_free_token_terms(copies, term_count);
            return -1;
        }
    }

    *terms_out = copies;
    return 0;
}

static int text_retriever_prepare_document_metadata(const text_retriever_t *retriever,
                                                    const text_retriever_document_t *document,
                                                    int32_t skip_index,
                                                    char **external_id_out,
                                                    char **stored_text_out,
                                                    bm25_input_kind_t *content_kind_out,
                                                    char **indexed_text_out,
                                                    char ***indexed_token_terms_out,
                                                    int32_t *indexed_token_count_out)
{
    char *external_id_copy;
    char *stored_text_copy;
    char *indexed_text_copy;
    char **indexed_token_terms_copy;
    const char *stored_text_source;

    if (!retriever || !document || !external_id_out || !stored_text_out || !content_kind_out || !indexed_text_out ||
        !indexed_token_terms_out || !indexed_token_count_out) {
        return -1;
    }

    *external_id_out = NULL;
    *stored_text_out = NULL;
    *content_kind_out = BM25_INPUT_KIND_TEXT;
    *indexed_text_out = NULL;
    *indexed_token_terms_out = NULL;
    *indexed_token_count_out = 0;

    if (document->external_id) {
        if (text_retriever_external_id_exists(retriever, document->external_id, skip_index)) {
            return -1;
        }
        external_id_copy = text_retriever_strdup(document->external_id);
    } else {
        external_id_copy = text_retriever_dup_unique_generated_id(retriever);
    }
    if (!external_id_copy) {
        return -1;
    }

    stored_text_source = document->stored_text;
    if (!stored_text_source && document->content.kind == BM25_INPUT_KIND_TEXT) {
        stored_text_source = document->content.value.text;
    }

    stored_text_copy = stored_text_source ? text_retriever_strdup(stored_text_source) : NULL;
    if (stored_text_source && !stored_text_copy) {
        free(external_id_copy);
        return -1;
    }

    indexed_text_copy = NULL;
    indexed_token_terms_copy = NULL;
    if (document->content.kind == BM25_INPUT_KIND_TEXT) {
        if (!document->content.value.text) {
            free(external_id_copy);
            free(stored_text_copy);
            return -1;
        }
        indexed_text_copy = text_retriever_strdup(document->content.value.text);
        if (!indexed_text_copy) {
            free(external_id_copy);
            free(stored_text_copy);
            return -1;
        }
    } else if (document->content.kind == BM25_INPUT_KIND_TOKENS) {
        if (text_retriever_clone_token_terms(document->content.value.tokens.term_count,
                                             document->content.value.tokens.terms,
                                             &indexed_token_terms_copy) != 0) {
            free(external_id_copy);
            free(stored_text_copy);
            return -1;
        }
        *indexed_token_count_out = document->content.value.tokens.term_count;
    } else {
        free(external_id_copy);
        free(stored_text_copy);
        return -1;
    }

    *external_id_out = external_id_copy;
    *stored_text_out = stored_text_copy;
    *content_kind_out = document->content.kind;
    *indexed_text_out = indexed_text_copy;
    *indexed_token_terms_out = indexed_token_terms_copy;
    return 0;
}

static int text_retriever_rebuild_index(text_retriever_t *retriever)
{
    bm25_t *new_index;
    int32_t i;

    if (!retriever) {
        return -1;
    }

    new_index = bm25_index_create_with_tokenizer(&retriever->params, retriever->tokenizer);
    if (!new_index) {
        return -1;
    }

    for (i = 0; i < retriever->doc_count; ++i) {
        bm25_document_input_t document;
        int32_t internal_id;

        memset(&document, 0, sizeof(document));

        document.kind = retriever->content_kinds[i];
        if (document.kind == BM25_INPUT_KIND_TEXT) {
            document.value.text = retriever->indexed_texts[i];
        } else {
            document.value.tokens.term_count = retriever->indexed_token_counts[i];
            document.value.tokens.terms = (const char *const *)retriever->indexed_token_terms[i];
        }

        internal_id = bm25_index_add(new_index, &document);
        if (internal_id != i) {
            bm25_index_drop(new_index);
            return -1;
        }
    }

    bm25_index_drop(retriever->index);
    retriever->index = new_index;
    return 0;
}

static void text_retriever_term_list_free(text_retriever_term_list_t *terms)
{
    int32_t i;

    if (!terms) {
        return;
    }
    for (i = 0; i < terms->count; ++i) {
        free(terms->items[i]);
    }
    free(terms->items);
    terms->items = NULL;
    terms->count = 0;
    terms->capacity = 0;
}

static int text_retriever_term_list_append_unique(text_retriever_term_list_t *terms, const char *value)
{
    char **new_items;
    char *copy;
    int32_t i;
    int32_t new_capacity;

    if (!terms || !value || value[0] == '\0') {
        return -1;
    }

    for (i = 0; i < terms->count; ++i) {
        if (strcmp(terms->items[i], value) == 0) {
            return 0;
        }
    }

    if (terms->count >= terms->capacity) {
        new_capacity = terms->capacity > 0 ? terms->capacity * 2 : 4;
        new_items = (char **)realloc(terms->items, (size_t)new_capacity * sizeof(char *));
        if (!new_items) {
            return -1;
        }
        terms->items = new_items;
        terms->capacity = new_capacity;
    }

    copy = text_retriever_strdup(value);
    if (!copy) {
        return -1;
    }

    terms->items[terms->count] = copy;
    terms->count += 1;
    return 0;
}

static int text_retriever_collect_query_terms(const text_retriever_t *retriever,
                                              const bm25_query_input_t *query,
                                              text_retriever_term_list_t *terms)
{
    token_t *tokens;
    int32_t token_count;
    int32_t i;

    if (!retriever || !query || !terms) {
        return -1;
    }

    if (query->kind == BM25_INPUT_KIND_TOKENS) {
        for (i = 0; i < query->value.tokens.term_count; ++i) {
            const char *term = query->value.tokens.terms ? query->value.tokens.terms[i] : NULL;

            if (term && text_retriever_term_list_append_unique(terms, term) != 0) {
                return -1;
            }
        }
        return 0;
    }

    if (query->kind != BM25_INPUT_KIND_TEXT || !query->value.text) {
        return -1;
    }

    tokens = NULL;
    token_count = 0;
    if (dict_cut(retriever->tokenizer, query->value.text, &tokens, &token_count) != 0) {
        return -1;
    }

    for (i = 0; i < token_count; ++i) {
        if (text_retriever_term_list_append_unique(terms, tokens[i].text) != 0) {
            dict_free_tokens(tokens, token_count);
            return -1;
        }
    }

    dict_free_tokens(tokens, token_count);
    return 0;
}

static int text_retriever_build_highlighted_snippet(const char *stored_text,
                                                    const text_retriever_term_list_t *matched_terms,
                                                    char **snippet_out)
{
    const char *cursor;
    size_t total_length;
    size_t highlight_count;
    char *snippet;
    int32_t i;

    if (!snippet_out) {
        return -1;
    }
    *snippet_out = NULL;

    if (!stored_text) {
        return 0;
    }
    if (!matched_terms || matched_terms->count == 0) {
        *snippet_out = text_retriever_strdup(stored_text);
        return *snippet_out ? 0 : -1;
    }

    total_length = 0;
    highlight_count = 0;
    cursor = stored_text;
    while (*cursor) {
        size_t best_length = 0;

        for (i = 0; i < matched_terms->count; ++i) {
            const char *term = matched_terms->items[i];
            size_t term_length;

            if (!term || term[0] == '\0') {
                continue;
            }
            term_length = strlen(term);
            if (term_length > best_length && strncmp(cursor, term, term_length) == 0) {
                best_length = term_length;
            }
        }

        if (best_length > 0) {
            total_length += best_length + 4;
            highlight_count += 1;
            cursor += best_length;
        } else {
            total_length += 1;
            cursor += 1;
        }
    }

    if (highlight_count == 0) {
        *snippet_out = text_retriever_strdup(stored_text);
        return *snippet_out ? 0 : -1;
    }

    snippet = (char *)malloc(total_length + 1);
    if (!snippet) {
        return -1;
    }

    cursor = stored_text;
    total_length = 0;
    while (*cursor) {
        size_t best_length = 0;

        for (i = 0; i < matched_terms->count; ++i) {
            const char *term = matched_terms->items[i];
            size_t term_length;

            if (!term || term[0] == '\0') {
                continue;
            }
            term_length = strlen(term);
            if (term_length > best_length && strncmp(cursor, term, term_length) == 0) {
                best_length = term_length;
            }
        }

        if (best_length > 0) {
            memcpy(snippet + total_length, "[[", 2);
            total_length += 2;
            memcpy(snippet + total_length, cursor, best_length);
            total_length += best_length;
            memcpy(snippet + total_length, "]]", 2);
            total_length += 2;
            cursor += best_length;
        } else {
            snippet[total_length] = *cursor;
            total_length += 1;
            cursor += 1;
        }
    }
    snippet[total_length] = '\0';
    *snippet_out = snippet;
    return 0;
}

static int text_retriever_ensure_capacity(text_retriever_t *retriever, int32_t target)
{
    int32_t new_capacity;
    char **new_external_ids;
    char **new_stored_texts;
    bm25_input_kind_t *new_content_kinds;
    char **new_indexed_texts;
    char ***new_indexed_token_terms;
    int32_t *new_indexed_token_counts;

    if (!retriever || target < 0) {
        return -1;
    }
    if (target <= retriever->doc_capacity) {
        return 0;
    }

    new_capacity = retriever->doc_capacity > 0 ? retriever->doc_capacity : 8;
    while (new_capacity < target) {
        new_capacity *= 2;
    }

    new_external_ids = (char **)calloc((size_t)new_capacity, sizeof(char *));
    new_stored_texts = (char **)calloc((size_t)new_capacity, sizeof(char *));
    new_content_kinds = (bm25_input_kind_t *)calloc((size_t)new_capacity, sizeof(bm25_input_kind_t));
    new_indexed_texts = (char **)calloc((size_t)new_capacity, sizeof(char *));
    new_indexed_token_terms = (char ***)calloc((size_t)new_capacity, sizeof(char **));
    new_indexed_token_counts = (int32_t *)calloc((size_t)new_capacity, sizeof(int32_t));
    if (!new_external_ids || !new_stored_texts || !new_content_kinds || !new_indexed_texts || !new_indexed_token_terms ||
        !new_indexed_token_counts) {
        free(new_external_ids);
        free(new_stored_texts);
        free(new_content_kinds);
        free(new_indexed_texts);
        free(new_indexed_token_terms);
        free(new_indexed_token_counts);
        return -1;
    }

    if (retriever->doc_capacity > 0) {
        memcpy(new_external_ids, retriever->external_ids, (size_t)retriever->doc_capacity * sizeof(char *));
        memcpy(new_stored_texts, retriever->stored_texts, (size_t)retriever->doc_capacity * sizeof(char *));
        memcpy(new_content_kinds, retriever->content_kinds, (size_t)retriever->doc_capacity * sizeof(bm25_input_kind_t));
        memcpy(new_indexed_texts, retriever->indexed_texts, (size_t)retriever->doc_capacity * sizeof(char *));
        memcpy(new_indexed_token_terms,
               retriever->indexed_token_terms,
               (size_t)retriever->doc_capacity * sizeof(char **));
        memcpy(new_indexed_token_counts,
               retriever->indexed_token_counts,
               (size_t)retriever->doc_capacity * sizeof(int32_t));
    }

    free(retriever->external_ids);
    free(retriever->stored_texts);
    free(retriever->content_kinds);
    free(retriever->indexed_texts);
    free(retriever->indexed_token_terms);
    free(retriever->indexed_token_counts);

    retriever->external_ids = new_external_ids;
    retriever->stored_texts = new_stored_texts;
    retriever->content_kinds = new_content_kinds;
    retriever->indexed_texts = new_indexed_texts;
    retriever->indexed_token_terms = new_indexed_token_terms;
    retriever->indexed_token_counts = new_indexed_token_counts;
    retriever->doc_capacity = new_capacity;
    return 0;
}

void text_retriever_set_default_config(text_retriever_config_t *config)
{
    if (!config) {
        return;
    }

    memset(config, 0, sizeof(*config));
    text_retriever_set_default_bm25_params(&config->bm25_params);
}

text_retriever_t *text_retriever_create(void)
{
    text_retriever_config_t config;

    text_retriever_set_default_config(&config);
    return text_retriever_create_with_config(&config);
}

text_retriever_t *text_retriever_create_with_config(const text_retriever_config_t *config)
{
    text_retriever_t *retriever;
    text_retriever_config_t default_config;
    const text_retriever_config_t *effective;
    dict_t *tokenizer;

    if (config) {
        effective = config;
    } else {
        text_retriever_set_default_config(&default_config);
        effective = &default_config;
    }

    tokenizer = effective->tokenizer;
    retriever = (text_retriever_t *)calloc(1, sizeof(text_retriever_t));
    if (!retriever) {
        return NULL;
    }

    if (!tokenizer) {
        tokenizer = dict_create_default();
        if (!tokenizer) {
            free(retriever);
            return NULL;
        }
        retriever->owns_tokenizer = true;
    } else {
        retriever->owns_tokenizer = effective->take_tokenizer_ownership;
    }

    retriever->tokenizer = tokenizer;
    retriever->params = effective->bm25_params;
    retriever->index = bm25_index_create_with_tokenizer(&effective->bm25_params, tokenizer);
    if (!retriever->index) {
        if (retriever->owns_tokenizer) {
            dict_drop(tokenizer);
        }
        free(retriever);
        return NULL;
    }

    return retriever;
}

int32_t text_retriever_add_document(text_retriever_t *retriever, const text_retriever_document_t *document)
{
    int32_t next_doc_id;
    char *external_id_copy;
    char *stored_text_copy;
    bm25_input_kind_t content_kind;
    char *indexed_text_copy;
    char **indexed_token_terms_copy;
    int32_t indexed_token_count;
    int32_t internal_id;

    if (!retriever || !document) {
        return -1;
    }

    next_doc_id = retriever->doc_count;
    if (text_retriever_ensure_capacity(retriever, next_doc_id + 1) != 0) {
        return -1;
    }

    if (text_retriever_prepare_document_metadata(retriever,
                                                 document,
                                                 -1,
                                                 &external_id_copy,
                                                 &stored_text_copy,
                                                 &content_kind,
                                                 &indexed_text_copy,
                                                 &indexed_token_terms_copy,
                                                 &indexed_token_count) != 0) {
        return -1;
    }

    internal_id = bm25_index_add(retriever->index, &document->content);
    if (internal_id != next_doc_id) {
        free(external_id_copy);
        free(stored_text_copy);
        free(indexed_text_copy);
        text_retriever_free_token_terms(indexed_token_terms_copy, indexed_token_count);
        return -1;
    }

    retriever->external_ids[internal_id] = external_id_copy;
    retriever->stored_texts[internal_id] = stored_text_copy;
    retriever->content_kinds[internal_id] = content_kind;
    retriever->indexed_texts[internal_id] = indexed_text_copy;
    retriever->indexed_token_terms[internal_id] = indexed_token_terms_copy;
    retriever->indexed_token_counts[internal_id] = indexed_token_count;
    retriever->doc_count += 1;
    return internal_id;
}

int32_t text_retriever_add_documents(text_retriever_t *retriever,
                                     int32_t document_count,
                                     const text_retriever_document_t *documents,
                                     int32_t *internal_ids)
{
    int32_t i;

    if (!retriever || document_count < 0 || (document_count > 0 && !documents)) {
        return -1;
    }

    for (i = 0; i < document_count; ++i) {
        int32_t internal_id = text_retriever_add_document(retriever, &documents[i]);

        if (internal_id < 0) {
            return -1;
        }
        if (internal_ids) {
            internal_ids[i] = internal_id;
        }
    }

    return 0;
}

int32_t text_retriever_update_document(text_retriever_t *retriever,
                                       const char *external_id,
                                       const text_retriever_document_t *document)
{
    int32_t document_index;
    char *new_external_id;
    char *new_stored_text;
    bm25_input_kind_t new_content_kind;
    char *new_indexed_text;
    char **new_indexed_token_terms;
    int32_t new_indexed_token_count;
    char *old_external_id;
    char *old_stored_text;
    bm25_input_kind_t old_content_kind;
    char *old_indexed_text;
    char **old_indexed_token_terms;
    int32_t old_indexed_token_count;

    if (!retriever || !external_id || !document) {
        return -1;
    }

    document_index = text_retriever_find_document_index(retriever, external_id);
    if (document_index < 0) {
        return -1;
    }

    if (text_retriever_prepare_document_metadata(retriever,
                                                 document,
                                                 document_index,
                                                 &new_external_id,
                                                 &new_stored_text,
                                                 &new_content_kind,
                                                 &new_indexed_text,
                                                 &new_indexed_token_terms,
                                                 &new_indexed_token_count) != 0) {
        return -1;
    }

    old_external_id = retriever->external_ids[document_index];
    old_stored_text = retriever->stored_texts[document_index];
    old_content_kind = retriever->content_kinds[document_index];
    old_indexed_text = retriever->indexed_texts[document_index];
    old_indexed_token_terms = retriever->indexed_token_terms[document_index];
    old_indexed_token_count = retriever->indexed_token_counts[document_index];

    retriever->external_ids[document_index] = new_external_id;
    retriever->stored_texts[document_index] = new_stored_text;
    retriever->content_kinds[document_index] = new_content_kind;
    retriever->indexed_texts[document_index] = new_indexed_text;
    retriever->indexed_token_terms[document_index] = new_indexed_token_terms;
    retriever->indexed_token_counts[document_index] = new_indexed_token_count;

    if (text_retriever_rebuild_index(retriever) != 0) {
        retriever->external_ids[document_index] = old_external_id;
        retriever->stored_texts[document_index] = old_stored_text;
        retriever->content_kinds[document_index] = old_content_kind;
        retriever->indexed_texts[document_index] = old_indexed_text;
        retriever->indexed_token_terms[document_index] = old_indexed_token_terms;
        retriever->indexed_token_counts[document_index] = old_indexed_token_count;
        free(new_external_id);
        free(new_stored_text);
        free(new_indexed_text);
        text_retriever_free_token_terms(new_indexed_token_terms, new_indexed_token_count);
        return -1;
    }

    free(old_external_id);
    free(old_stored_text);
    free(old_indexed_text);
    text_retriever_free_token_terms(old_indexed_token_terms, old_indexed_token_count);
    return document_index;
}

int32_t text_retriever_delete_document(text_retriever_t *retriever, const char *external_id)
{
    int32_t document_index;
    char *removed_external_id;
    char *removed_stored_text;
    bm25_input_kind_t removed_content_kind;
    char *removed_indexed_text;
    char **removed_indexed_token_terms;
    int32_t removed_indexed_token_count;
    int32_t i;

    if (!retriever || !external_id) {
        return -1;
    }

    document_index = text_retriever_find_document_index(retriever, external_id);
    if (document_index < 0) {
        return -1;
    }

    removed_external_id = retriever->external_ids[document_index];
    removed_stored_text = retriever->stored_texts[document_index];
    removed_content_kind = retriever->content_kinds[document_index];
    removed_indexed_text = retriever->indexed_texts[document_index];
    removed_indexed_token_terms = retriever->indexed_token_terms[document_index];
    removed_indexed_token_count = retriever->indexed_token_counts[document_index];

    retriever->external_ids[document_index] = NULL;
    retriever->stored_texts[document_index] = NULL;
    retriever->content_kinds[document_index] = BM25_INPUT_KIND_TEXT;
    retriever->indexed_texts[document_index] = NULL;
    retriever->indexed_token_terms[document_index] = NULL;
    retriever->indexed_token_counts[document_index] = 0;

    for (i = document_index + 1; i < retriever->doc_count; ++i) {
        text_retriever_move_slot(retriever, i - 1, i);
    }
    retriever->doc_count -= 1;

    if (text_retriever_rebuild_index(retriever) != 0) {
        for (i = retriever->doc_count; i > document_index; --i) {
            text_retriever_move_slot(retriever, i, i - 1);
        }
        retriever->external_ids[document_index] = removed_external_id;
        retriever->stored_texts[document_index] = removed_stored_text;
        retriever->content_kinds[document_index] = removed_content_kind;
        retriever->indexed_texts[document_index] = removed_indexed_text;
        retriever->indexed_token_terms[document_index] = removed_indexed_token_terms;
        retriever->indexed_token_counts[document_index] = removed_indexed_token_count;
        retriever->doc_count += 1;
        return -1;
    }

    free(removed_external_id);
    free(removed_stored_text);
    free(removed_indexed_text);
    text_retriever_free_token_terms(removed_indexed_token_terms, removed_indexed_token_count);
    return 0;
}

int32_t text_retriever_search(const text_retriever_t *retriever,
                              const bm25_query_input_t *query,
                              int32_t k,
                              text_retriever_hit_t *hits,
                              int32_t *hit_count)
{
    text_retriever_term_list_t query_terms;
    float *scores;
    int32_t *doc_ids;
    int32_t result;
    int32_t topk;
    int32_t i;

    if (!retriever || !query || !hit_count || k < 0 || (k > 0 && !hits)) {
        return -1;
    }

    if (k == 0) {
        *hit_count = 0;
        return 0;
    }

    memset(&query_terms, 0, sizeof(query_terms));
    if (text_retriever_collect_query_terms(retriever, query, &query_terms) != 0) {
        return -1;
    }

    scores = (float *)malloc((size_t)k * sizeof(float));
    doc_ids = (int32_t *)malloc((size_t)k * sizeof(int32_t));
    if (!scores || !doc_ids) {
        text_retriever_term_list_free(&query_terms);
        free(scores);
        free(doc_ids);
        return -1;
    }

    result = bm25_index_search_query_with_count(retriever->index, query, k, scores, doc_ids, hit_count);
    if (result != 0) {
        text_retriever_term_list_free(&query_terms);
        free(scores);
        free(doc_ids);
        return result;
    }

    topk = *hit_count < k ? *hit_count : k;
    for (i = 0; i < k; ++i) {
        hits[i].internal_id = -1;
        hits[i].external_id = NULL;
        hits[i].stored_text = NULL;
        hits[i].score = 0.0f;
        hits[i].highlighted_snippet = NULL;
        hits[i].matched_terms = NULL;
        hits[i].matched_term_count = 0;
    }
    for (i = 0; i < topk; ++i) {
        text_retriever_term_list_t matched_terms;
        text_retriever_term_list_t snippet_terms;

        memset(&matched_terms, 0, sizeof(matched_terms));
        memset(&snippet_terms, 0, sizeof(snippet_terms));
        hits[i].internal_id = doc_ids[i];
        hits[i].external_id = retriever->external_ids[doc_ids[i]];
        hits[i].stored_text = retriever->stored_texts[doc_ids[i]];
        hits[i].score = scores[i];
        hits[i].highlighted_snippet = NULL;
        hits[i].matched_terms = NULL;
        hits[i].matched_term_count = 0;

        if (hits[i].stored_text) {
            int32_t term_index;

            for (term_index = 0; term_index < query_terms.count; ++term_index) {
                if (strstr(hits[i].stored_text, query_terms.items[term_index]) &&
                    text_retriever_term_list_append_unique(&matched_terms, query_terms.items[term_index]) != 0) {
                    text_retriever_term_list_free(&matched_terms);
                    text_retriever_term_list_free(&query_terms);
                    free(scores);
                    free(doc_ids);
                    return -1;
                }
            }
        }

        if (matched_terms.count > 0) {
            hits[i].matched_terms = matched_terms.items;
            hits[i].matched_term_count = matched_terms.count;
            snippet_terms.items = hits[i].matched_terms;
            snippet_terms.count = hits[i].matched_term_count;
            snippet_terms.capacity = hits[i].matched_term_count;
            matched_terms.items = NULL;
            matched_terms.count = 0;
            matched_terms.capacity = 0;
        }

        if (text_retriever_build_highlighted_snippet(hits[i].stored_text,
                                                     hits[i].matched_term_count > 0 ? &snippet_terms : NULL,
                                                     &hits[i].highlighted_snippet) != 0) {
            text_retriever_term_list_free(&matched_terms);
            text_retriever_term_list_free(&query_terms);
            free(scores);
            free(doc_ids);
            return -1;
        }
        text_retriever_term_list_free(&matched_terms);
    }

    text_retriever_term_list_free(&query_terms);
    free(scores);
    free(doc_ids);
    return 0;
}

int32_t text_retriever_search_batch(const text_retriever_t *retriever,
                                    int32_t query_count,
                                    const bm25_query_input_t *queries,
                                    int32_t k,
                                    text_retriever_hit_t *hits,
                                    int32_t *hit_counts)
{
    int32_t i;

    if (!retriever || query_count < 0 || (query_count > 0 && (!queries || !hits || !hit_counts)) || k < 0) {
        return -1;
    }

    for (i = 0; i < query_count; ++i) {
        if (text_retriever_search(retriever,
                                  &queries[i],
                                  k,
                                  hits + ((size_t)i * (size_t)k),
                                  &hit_counts[i]) != 0) {
            return -1;
        }
    }

    return 0;
}

void text_retriever_free_hits(text_retriever_hit_t *hits, int32_t hit_capacity)
{
    int32_t i;
    int32_t term_index;

    if (!hits || hit_capacity < 0) {
        return;
    }

    for (i = 0; i < hit_capacity; ++i) {
        for (term_index = 0; term_index < hits[i].matched_term_count; ++term_index) {
            free(hits[i].matched_terms[term_index]);
        }
        free(hits[i].matched_terms);
        free(hits[i].highlighted_snippet);
        hits[i].matched_terms = NULL;
        hits[i].matched_term_count = 0;
        hits[i].highlighted_snippet = NULL;
    }
}

int32_t text_retriever_size(const text_retriever_t *retriever)
{
    if (!retriever) {
        return -1;
    }

    return retriever->doc_count;
}

int32_t text_retriever_get_params(const text_retriever_t *retriever, bm25_params_t *params)
{
    if (!retriever || !params) {
        return -1;
    }

    *params = retriever->params;
    return 0;
}

dict_t *text_retriever_get_tokenizer(const text_retriever_t *retriever)
{
    if (!retriever) {
        return NULL;
    }

    return retriever->tokenizer;
}

const bm25_t *text_retriever_get_bm25(const text_retriever_t *retriever)
{
    if (!retriever) {
        return NULL;
    }

    return retriever->index;
}

void text_retriever_drop(text_retriever_t *retriever)
{
    int32_t i;

    if (!retriever) {
        return;
    }

    for (i = 0; i < retriever->doc_capacity; ++i) {
        text_retriever_clear_slot(retriever, i);
    }
    free(retriever->external_ids);
    free(retriever->stored_texts);
    free(retriever->content_kinds);
    free(retriever->indexed_texts);
    free(retriever->indexed_token_terms);
    free(retriever->indexed_token_counts);
    bm25_index_drop(retriever->index);
    if (retriever->owns_tokenizer) {
        dict_drop(retriever->tokenizer);
    }
    free(retriever);
}