#include "bm25_private.h"

bm25_t *bm25_index_create(void)
{
    bm25_params_t params;

    /* 默认构造走统一入口，避免多套初始化逻辑分叉。 */
    bm25_set_default_params(&params);
    return bm25_index_create_with_tokenizer(&params, NULL);
}

bm25_t *bm25_index_create_with_params(const bm25_params_t *params)
{
    return bm25_index_create_with_tokenizer(params, NULL);
}

bm25_t *bm25_index_create_with_tokenizer(const bm25_params_t *params, dict_t *tokenizer)
{
    bm25_t *index;
    bm25_params_t normalized_params;

    if (!bm25_params_are_valid(params)) {
        return NULL;
    }

    normalized_params = *params;
    bm25_normalize_params(&normalized_params);

    /* 第一步: 创建索引主结构并写入 BM25 超参数。 */
    index = (bm25_t *)calloc(1, sizeof(bm25_t));
    if (!index) {
        return NULL;
    }

    index->params = normalized_params;
    bm25_reset_search_stats(&index->last_search_stats, BM25_SEARCH_ALGORITHM_AUTO);
    /* 第二步: 绑定外部分词器，或按需创建默认分词器。 */
    if (tokenizer) {
        index->tokenizer = tokenizer;
        index->owns_tokenizer = false;
    } else if (bm25_ensure_default_tokenizer(index) != 0) {
        free(index);
        return NULL;
    }
    return index;
}

void bm25_index_reset(bm25_t *index)
{
    if (!index) {
        return;
    }

    /* 只清空倒排数据，不销毁 tokenizer 和参数配置。 */
    bm25_free_terms(index);
    free(index->doc_lengths);
    index->doc_lengths = NULL;
    index->doc_count = 0;
    index->doc_capacity = 0;
    index->total_terms = 0;
    free(index->free_doc_ids);
    index->free_doc_ids = NULL;
    index->free_doc_count = 0;
    index->free_doc_capacity = 0;
    bm25_reset_search_stats(&index->last_search_stats, BM25_SEARCH_ALGORITHM_AUTO);
}

int32_t bm25_index_size(const bm25_t *index)
{
    if (!index) {
        return -1;
    }
    return index->doc_count - index->free_doc_count;
}

double bm25_index_average_doc_length(const bm25_t *index)
{
    if (!index || index->doc_count <= 0) {
        return 0.0;
    }

    return (double)index->total_terms / (double)index->doc_count;
}

int32_t bm25_index_doc_length(const bm25_t *index, int32_t doc_id)
{
    if (!index || doc_id < 0 || doc_id >= index->doc_count) {
        return -1;
    }

    return index->doc_lengths[doc_id];
}

int32_t bm25_index_set_params(bm25_t *index, const bm25_params_t *params)
{
    bm25_params_t normalized_params;

    if (!index || !bm25_params_are_valid(params)) {
        return -1;
    }

    normalized_params = *params;
    bm25_normalize_params(&normalized_params);
    index->params = normalized_params;
    return 0;
}

int32_t bm25_index_get_params(const bm25_t *index, bm25_params_t *params)
{
    if (!index || !params) {
        return -1;
    }

    *params = index->params;
    return 0;
}

int32_t bm25_index_get_last_search_stats(const bm25_t *index, bm25_search_stats_t *stats)
{
    if (!index || !stats) {
        return -1;
    }

    *stats = index->last_search_stats;
    return 0;
}

int32_t bm25_index_set_tokenizer(bm25_t *index, dict_t *tokenizer)
{
    if (!index) {
        return -1;
    }

    /* 切换 tokenizer 前，先释放旧的内部 tokenizer。 */
    if (index->owns_tokenizer && index->tokenizer) {
        dict_drop(index->tokenizer);
    }

    index->tokenizer = tokenizer;
    index->owns_tokenizer = false;

    if (!index->tokenizer) {
        return bm25_ensure_default_tokenizer(index);
    }

    return 0;
}

dict_t *bm25_index_get_tokenizer(const bm25_t *index)
{
    if (!index) {
        return NULL;
    }

    return index->tokenizer;
}

void bm25_index_drop(bm25_t *index)
{
    if (!index) {
        return;
    }

    /* drop = reset 倒排数据 + 视 ownership 释放 tokenizer + 释放主结构。 */
    bm25_index_reset(index);
    if (index->owns_tokenizer && index->tokenizer) {
        dict_drop(index->tokenizer);
    }
    free(index);
}