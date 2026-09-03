/**
 * @file bm25.c
 * @brief BM25 相关性打分算法实现
 */
#include "db/storage/doc/bm25.h"
#include "db/core/log.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ========================================================================
 * 生命周期 API 实现
 * ======================================================================== */

bm25_scorer_t *bm25_create(double k1, double b) {
    if (k1 <= 0) k1 = BM25_DEFAULT_K1;
    if (b <= 0 || b > 1) b = BM25_DEFAULT_B;

    bm25_scorer_t *scorer = (bm25_scorer_t *)calloc(1, sizeof(bm25_scorer_t));
    if (scorer == NULL) return NULL;

    scorer->k1 = k1;
    scorer->b = b;
    scorer->delta = BM25_DEFAULT_DELTA;
    scorer->avg_doc_len = 100;  /* 默认值 */
    scorer->num_docs = 0;
    scorer->num_terms = 0;

    /* 分配 IDF 缓存 */
    scorer->idf_cache = (double *)calloc(BM25_IDF_CACHE_SIZE, sizeof(double));
    scorer->term_df = (uint64_t *)calloc(BM25_IDF_CACHE_SIZE, sizeof(uint64_t));
    scorer->cache_count = BM25_IDF_CACHE_SIZE;

    /* 初始化 IDF 缓存为 -1（未计算） */
    for (uint32_t i = 0; i < BM25_IDF_CACHE_SIZE; i++) {
        scorer->idf_cache[i] = -1.0;
    }

    return scorer;
}

void bm25_destroy(bm25_scorer_t *scorer) {
    if (scorer == NULL) return;
    free(scorer->idf_cache);
    free(scorer->term_df);
    free(scorer->field_weights);
    free(scorer);
}

/* ========================================================================
 * 索引信息 API 实现
 * ======================================================================== */

void bm25_set_index_info(bm25_scorer_t *scorer, uint64_t num_docs,
                         uint32_t avg_doc_len) {
    if (scorer == NULL) return;
    scorer->num_docs = num_docs;
    scorer->avg_doc_len = avg_doc_len > 0 ? avg_doc_len : 1;
}

void bm25_set_term_df(bm25_scorer_t *scorer, uint32_t term_id,
                      uint64_t doc_freq) {
    if (scorer == NULL) return;
    if (term_id < BM25_IDF_CACHE_SIZE) {
        scorer->term_df[term_id] = doc_freq;
    }
}

void bm25_precompute(bm25_scorer_t *scorer) {
    if (scorer == NULL) return;

    /* 预计算所有 IDF 值 */
    for (uint32_t i = 0; i < scorer->cache_count && i < scorer->num_terms; i++) {
        bm25_idf(scorer, i);
    }

    LOG_DEBUG("BM25 IDF 预计算完成: %u terms", scorer->num_terms);
}

/* ========================================================================
 * IDF 计算实现
 * ======================================================================== */

double bm25_idf(bm25_scorer_t *scorer, uint32_t term_id) {
    if (scorer == NULL) return 0.0;

    if (term_id >= scorer->cache_count) {
        /* 超出缓存范围，使用简化公式 */
        return log((double)(scorer->num_docs + 1) / 2.0);
    }

    /* 检查缓存 */
    if (scorer->idf_cache[term_id] >= 0) {
        return scorer->idf_cache[term_id];
    }

    /* 计算 IDF（改进公式，避免极端情况） */
    uint64_t df = scorer->term_df[term_id];
    double idf;

    if (df == 0) {
        idf = log((double)(scorer->num_docs + 1));
    } else {
        /* Robertson-Sparck Jones 改进公式 */
        idf = log((scorer->num_docs - df + 0.5) / (df + 0.5) + 1.0);
        if (idf < 0) idf = 0;  /* 确保 IDF 非负 */
    }

    scorer->idf_cache[term_id] = idf;
    return idf;
}

void bm25_get_cache_stats(const bm25_scorer_t *scorer,
                          uint32_t *out_hits, uint32_t *out_misses) {
    if (scorer == NULL) return;
    /* 简化实现，未跟踪实际命中率 */
    if (out_hits) *out_hits = 0;
    if (out_misses) *out_misses = scorer->cache_count;
}

/* ========================================================================
 * 打分 API 实现
 * ======================================================================== */

double bm25_score(bm25_scorer_t *scorer,
                  uint32_t doc_len,
                  const uint32_t *term_freqs,
                  const uint32_t *query_terms,
                  uint32_t num_query_terms) {
    if (scorer == NULL || term_freqs == NULL || query_terms == NULL) {
        return 0.0;
    }

    double score = 0.0;
    double doc_len_norm = (double)doc_len / scorer->avg_doc_len;

    for (uint32_t i = 0; i < num_query_terms; i++) {
        uint32_t term_id = query_terms[i];
        uint32_t tf = term_freqs[term_id];

        if (tf == 0) continue;

        double idf = bm25_idf(scorer, term_id);

        /* BM25 公式 */
        double numerator = tf * (scorer->k1 + 1);
        double denominator = tf + scorer->k1 * (1 - scorer->b + scorer->b * doc_len_norm);
        double term_score = idf * (numerator / denominator);

        score += term_score;
    }

    return score;
}

double bm25_plus_score(bm25_scorer_t *scorer,
                       uint32_t doc_len,
                       const uint32_t *term_freqs,
                       const uint32_t *query_terms,
                       uint32_t num_query_terms) {
    if (scorer == NULL) return 0.0;

    double score = 0.0;
    double doc_len_norm = (double)doc_len / scorer->avg_doc_len;

    for (uint32_t i = 0; i < num_query_terms; i++) {
        uint32_t term_id = query_terms[i];
        uint32_t tf = term_freqs[term_id];

        if (tf == 0) continue;

        double idf = bm25_idf(scorer, term_id);

        /* BM25+ 公式：添加 delta 避免零分 */
        double numerator = tf * (scorer->k1 + 1);
        double denominator = tf + scorer->k1 * (1 - scorer->b + scorer->b * doc_len_norm);
        double term_score = idf * (numerator / denominator + scorer->delta);

        score += term_score;
    }

    return score;
}

double bm25f_score(bm25_scorer_t *scorer,
                   const double *field_lens,
                   const uint32_t *field_freqs,
                   const uint32_t *query_terms,
                   uint32_t num_query_terms) {
    if (scorer == NULL || field_lens == NULL || field_freqs == NULL) {
        return 0.0;
    }

    /* BM25F：字段加权的 BM25 变体 */
    double score = 0.0;
    double total_weight = 0.0;

    for (uint32_t i = 0; i < scorer->field_count; i++) {
        total_weight += scorer->field_weights[i].weight;
    }

    if (total_weight == 0) total_weight = 1.0;

    /* 简化的 BM25F 实现 */
    for (uint32_t i = 0; i < num_query_terms; i++) {
        uint32_t term_id = query_terms[i];
        double idf = bm25_idf(scorer, term_id);

        double weighted_tf = 0.0;
        double weighted_len = 0.0;

        for (uint32_t j = 0; j < scorer->field_count; j++) {
            uint32_t freq = field_freqs[j * scorer->num_terms + term_id];
            double weight = scorer->field_weights[j].weight;
            weighted_tf += freq * weight;
            weighted_len += field_lens[j] * weight;
        }

        double avg_field_len = weighted_len / total_weight;
        double doc_len_norm = weighted_len / (avg_field_len > 0 ? avg_field_len : 1);

        double numerator = weighted_tf * (scorer->k1 + 1);
        double denominator = weighted_tf + scorer->k1 * (1 - scorer->b + scorer->b * doc_len_norm);

        score += idf * (numerator / denominator);
    }

    return score;
}

uint32_t bm25_batch_score(bm25_scorer_t *scorer,
                          const bm25_doc_stats_t *docs,
                          const uint32_t *term_freqs_matrix,
                          const uint32_t *query_terms,
                          uint32_t num_query_terms,
                          bm25_batch_result_t *results,
                          uint32_t max_results) {
    if (scorer == NULL || docs == NULL || term_freqs_matrix == NULL ||
        query_terms == NULL || results == NULL || max_results == 0) {
        return 0;
    }

    uint32_t count = 0;

    /* 计算每个文档的分数 */
    for (uint64_t i = 0; i < scorer->num_docs && count < max_results; i++) {
        const uint32_t *term_freqs = &term_freqs_matrix[i * scorer->num_terms];
        double score = bm25_score(scorer, docs[i].doc_len, term_freqs,
                                  query_terms, num_query_terms);

        results[count].doc_id = docs[i].doc_id;
        results[count].score = score;
        count++;
    }

    /* 按分数降序排序 */
    for (uint32_t i = 0; i < count - 1; i++) {
        for (uint32_t j = i + 1; j < count; j++) {
            if (results[j].score > results[i].score) {
                bm25_batch_result_t tmp = results[i];
                results[i] = results[j];
                results[j] = tmp;
            }
        }
    }

    return count;
}

/* ========================================================================
 * 配置 API 实现
 * ======================================================================== */

void bm25_set_delta(bm25_scorer_t *scorer, double delta) {
    if (scorer == NULL) return;
    scorer->delta = delta > 0 ? delta : 0;
}

void bm25_add_field_weight(bm25_scorer_t *scorer,
                           const char *field_name, double weight) {
    if (scorer == NULL || field_name == NULL || weight <= 0) return;

    /* 检查是否已存在 */
    for (uint32_t i = 0; i < scorer->field_count; i++) {
        if (strcmp(scorer->field_weights[i].field_name, field_name) == 0) {
            scorer->field_weights[i].weight = weight;
            return;
        }
    }

    /* 添加新字段权重 */
    uint32_t idx = scorer->field_count++;
    scorer->field_weights = (bm25_field_weight_t *)realloc(
        scorer->field_weights,
        scorer->field_count * sizeof(bm25_field_weight_t));

    scorer->field_weights[idx].field_name = field_name;
    scorer->field_weights[idx].weight = weight;
}
