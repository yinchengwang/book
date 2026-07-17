#include "bm25_private.h"

static int bm25_collect_document_terms(const bm25_t *index,
                                       int32_t term_count,
                                       const char *const *terms,
                                       bm25_term_freq_map_t *doc_terms,
                                       int32_t *valid_term_count)
{
    int32_t i;

    if (!doc_terms) {
        return -1;
    }
    memset(doc_terms, 0, sizeof(*doc_terms));
    if (valid_term_count) {
        *valid_term_count = 0;
    }
    if (!index || term_count < 0 || (term_count > 0 && !terms)) {
        return -1;
    }
    if (bm25_term_freq_map_init(doc_terms, term_count) != 0) {
        return -1;
    }

    /* 先对文档内 term 做归一化与词频聚合，生成“一篇文档的局部词频表”。 */
    for (i = 0; i < term_count; ++i) {
        char *normalized_term;
        uint64_t hash_value;

        if (!terms[i] || terms[i][0] == '\0') {
            continue;
        }

        normalized_term = NULL;
        if (dict_normalize_term(index->tokenizer, terms[i], &normalized_term) != 0) {
            bm25_term_freq_map_destroy(doc_terms);
            return -1;
        }
        if (!normalized_term) {
            continue;
        }

        hash_value = bm25_hash_term(normalized_term);
        if (bm25_term_freq_map_increment(doc_terms, normalized_term, hash_value) != 0) {
            bm25_term_freq_map_destroy(doc_terms);
            return -1;
        }

        if (valid_term_count) {
            *valid_term_count += 1;
        }
    }

    return 0;
}

int32_t bm25_index_add_document(bm25_t *index, int32_t term_count, const char *const *terms)
{
    bm25_term_freq_map_t doc_terms;
    bm25_term_slot_t **prepared_slots;
    bool *created_flags;
    int32_t valid_term_count;
    int32_t doc_id;
    int32_t unique_term_count;
    int32_t block_size;
    int32_t prepared_count = 0;
    int32_t i;

    if (!index || term_count < 0 || (term_count > 0 && !terms)) {
        return -1;
    }

    /* 第一步: 对输入文档做规范化并统计文档内词频。 */
    if (bm25_collect_document_terms(index, term_count, terms, &doc_terms, &valid_term_count) != 0) {
        return -1;
    }

    if (bm25_ensure_doc_capacity(index, index->doc_count + 1) != 0) {
        bm25_term_freq_map_destroy(&doc_terms);
        return -1;
    }

    unique_term_count = doc_terms.item_count;
    prepared_slots = unique_term_count > 0 ? (bm25_term_slot_t **)calloc((size_t)unique_term_count, sizeof(bm25_term_slot_t *)) : NULL;
    created_flags = unique_term_count > 0 ? (bool *)calloc((size_t)unique_term_count, sizeof(bool)) : NULL;
    if (unique_term_count > 0 && (!prepared_slots || !created_flags)) {
        free(prepared_slots);
        free(created_flags);
        bm25_term_freq_map_destroy(&doc_terms);
        return -1;
    }

    block_size = index->params.posting_filter_window > 0 ? index->params.posting_filter_window : 16;

    /*
     * 第二步: 为每个词项准备倒排项。
     * 这里先把扩容/新建词项都做好，确保后面写入 posting 时不会半途失败。
     */
    i = 0;
    for (i = 0; i < unique_term_count; ++i) {
        bm25_term_freq_entry_t *current = doc_terms.items[i];
        bm25_term_slot_t *term_slot;
        int32_t term_slot_index;
        int32_t target_filter_count;

        term_slot_index = bm25_find_term_slot_index(index, current->term, current->hash_value);
        if (term_slot_index < 0) {
            term_slot_index = bm25_insert_term_slot(index, current->term, current->hash_value);
            if (term_slot_index < 0) {
                goto rollback;
            }
            created_flags[i] = true;
        }

        term_slot = bm25_get_term_slot(index, term_slot_index);
        if (!term_slot || bm25_reserve_postings(term_slot, term_slot->posting_count + 1) != 0) {
            goto rollback;
        }
        target_filter_count = (term_slot->posting_count + 1 + block_size - 1) / block_size;
        if (bm25_reserve_posting_filters(term_slot, target_filter_count) != 0) {
            goto rollback;
        }
        prepared_slots[i] = term_slot;
        prepared_count += 1;
    }

    /* 第三步: 正式分配 doc_id，并更新全局长度统计。 */
    doc_id = bm25_allocate_doc_id(index);
    index->doc_lengths[doc_id] = valid_term_count;
    if (doc_id >= index->doc_count) {
        index->doc_count = doc_id + 1;
    }
    index->total_terms += valid_term_count;

    /* 第四步: 有序插入 posting 并同步维护正排索引。 */
    bm25_doc_forward_init(index, doc_id);
    for (i = 0; i < unique_term_count; ++i) {
        bm25_term_freq_entry_t *current = doc_terms.items[i];
        bm25_term_slot_t *term_slot = prepared_slots[i];

        if (bm25_posting_insert_ordered(term_slot, doc_id, current->freq) != 0) {
            goto rollback;
        }
        term_slot->doc_freq += 1;
        term_slot->total_term_freq += current->freq;
        bm25_doc_forward_add_token(index, doc_id,
                                    bm25_find_term_slot_index(index, current->term, current->hash_value),
                                    current->hash_value);
        bm25_rebuild_posting_filters(index, term_slot);
    }

    free(prepared_slots);
    free(created_flags);
    bm25_term_freq_map_destroy(&doc_terms);
    return doc_id;

rollback:
    /* 如果中途失败，需要撤销本次新建但尚未稳定写入的 term entry。 */
    while (prepared_count > 0) {
        prepared_count -= 1;
        if (created_flags[prepared_count]) {
            bm25_remove_term_slot(index, index->term_slot_count - 1);
        }
    }

    free(prepared_slots);
    free(created_flags);
    bm25_term_freq_map_destroy(&doc_terms);
    return -1;
}

int32_t bm25_index_add(bm25_t *index, const bm25_document_input_t *document)
{
    if (!index || !document) {
        return -1;
    }

    if (document->kind == BM25_INPUT_KIND_TOKENS) {
        return bm25_index_add_document(index, document->value.tokens.term_count, document->value.tokens.terms);
    }
    if (document->kind == BM25_INPUT_KIND_TEXT) {
        return bm25_index_add_text(index, document->value.text);
    }

    return -1;
}

int32_t bm25_index_add_text(bm25_t *index, const char *text)
{
    token_t *tokens;
    const char **terms;
    int32_t token_count;
    int32_t doc_id;
    int32_t i;

    if (!index || !text) {
        return -1;
    }

    tokens = NULL;
    token_count = 0;
    /* 原始文本路径先分词，再复用 token 版本的建库逻辑。 */
    if (bm25_tokenize_text(index, text, &tokens, &token_count) != 0) {
        return -1;
    }

    if (token_count <= 0) {
        dict_free_tokens(tokens, token_count);
        return bm25_index_add_document(index, 0, NULL);
    }

    terms = (const char **)malloc((size_t)token_count * sizeof(const char *));
    if (!terms) {
        dict_free_tokens(tokens, token_count);
        return -1;
    }

    for (i = 0; i < token_count; ++i) {
        terms[i] = tokens[i].text;
    }

    doc_id = bm25_index_add_document(index, token_count, terms);
    free(terms);
    dict_free_tokens(tokens, token_count);
    return doc_id;
}