/**
 * @file bm25_index.c
 * @brief BM25 全文检索索引实现
 *
 * 实现 Okapi BM25 算法：
 *   score(D, Q) = IDF(q) * (tf * (k1 + 1)) / (tf + k1 * (1 - b + b * |D| / avg_dl))
 *
 * 其中 IDF(q) = log((N - n(q) + 0.5) / (n(q) + 0.5))
 */
#include "db/bm25_index.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <pthread.h>
#include <uthash/uthash.h>

/* ========================================================================
 * 内部结构
 * ======================================================================== */

/** 文档词频映射条目 */
typedef struct doc_tf_entry_s {
    uint32_t term_idx;      /**< 词条在 terms[] 中的索引 */
    uint32_t tf;            /**< 该词在此文档中的出现次数 */
} doc_tf_entry_t;

/** 带词频的文档 */
typedef struct doc_with_tf_s {
    uint64_t doc_id;
    uint32_t length;
    doc_tf_entry_t *tfs;    /**< 词频数组 */
    uint32_t tf_count;      /**< 词频数组长度 */
    uint32_t tf_capacity;
} doc_with_tf_t;

/** 词条哈希表条目 */
typedef struct term_hash_entry_s {
    char term[64];          /**< 词项文本 */
    uint32_t term_idx;      /**< 词条在 terms[] 中的索引 */
    UT_hash_handle hh;      /**< 哈希句柄 */
} term_hash_entry_t;

/** 全局状态（线程安全） */
typedef struct bm25_global_state_s {
    pthread_mutex_t mutex;
    doc_with_tf_t *docs_with_tf;
    uint32_t docs_with_tf_count;
    uint32_t docs_with_tf_capacity;
    term_hash_entry_t *term_hash;  /**< 词条哈希表，O(1) 查找 */
} bm25_global_state_t;

static bm25_global_state_t g_bm25_state = {
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .docs_with_tf = NULL,
    .docs_with_tf_count = 0,
    .docs_with_tf_capacity = 0,
    .term_hash = NULL
};

/* ========================================================================
 * 内部辅助函数
 * ======================================================================== */

/**
 * @brief 简单分词器（空格/标点分隔，转小写）
 * @param text 输入文本
 * @param tokens 输出 token 数组（动态分配）
 * @param max_tokens 数组容量
 * @return 实际 token 数量，tokens 由调用者释放
 */
static uint32_t simple_tokenize(const char *text, char **tokens, uint32_t max_tokens) {
    if (!text) return 0;

    uint32_t count = 0;
    const char *p = text;
    char buf[64];
    uint32_t buf_len = 0;

    while (*p && count < max_tokens) {
        if (isspace((unsigned char)*p) || ispunct((unsigned char)*p)) {
            if (buf_len > 0) {
                buf[buf_len] = '\0';
                /* 转小写 */
                for (uint32_t i = 0; buf[i]; i++) {
                    buf[i] = (char)tolower((unsigned char)buf[i]);
                }
                tokens[count] = strndup(buf, 63);
                count++;
                buf_len = 0;
            }
        } else {
            if (buf_len < 63) {
                buf[buf_len++] = *p;
            }
        }
        p++;
    }
    if (buf_len > 0 && count < max_tokens) {
        buf[buf_len] = '\0';
        for (uint32_t i = 0; buf[i]; i++) {
            buf[i] = (char)tolower((unsigned char)buf[i]);
        }
        tokens[count] = strndup(buf, 63);
        count++;
    }
    return count;
}

/**
 * @brief 在词条哈希表中查找词条（O(1) 平均复杂度）
 * @return 找到返回索引，未找到返回 -1
 */
static int32_t bm25_find_term(const bm25_index_t *index, const char *term) {
    (void)index;  /* index 参数保留以备扩展 */
    term_hash_entry_t *entry = NULL;
    HASH_FIND_STR(g_bm25_state.term_hash, term, entry);
    if (entry) {
        return (int32_t)entry->term_idx;
    }
    return -1;
}

/**
 * @brief 添加新词条（同步更新哈希表）
 */
static int bm25_add_term(bm25_index_t *index, const char *term) {
    /* 扩容检查 */
    if (index->term_count >= index->term_capacity) {
        uint32_t new_cap = index->term_capacity ? index->term_capacity * 2 : 16;
        bm25_term_t *new_terms = realloc(index->terms, new_cap * sizeof(bm25_term_t));
        if (!new_terms) {
            LOG_ERROR("bm25_index: 词条数组扩容失败");
            return -1;
        }
        index->terms = new_terms;
        index->term_capacity = new_cap;
    }

    uint32_t idx = index->term_count++;
    index->terms[idx].term = strdup(term);
    index->terms[idx].doc_freq = 0;
    index->terms[idx].total_tf = 0;

    /* 添加到哈希表 */
    term_hash_entry_t *entry = malloc(sizeof(term_hash_entry_t));
    if (!entry) {
        LOG_ERROR("bm25_index: 哈希表条目分配失败");
        return -1;
    }
    strncpy(entry->term, term, sizeof(entry->term) - 1);
    entry->term[sizeof(entry->term) - 1] = '\0';
    entry->term_idx = idx;
    HASH_ADD_STR(g_bm25_state.term_hash, term, entry);

    return (int)idx;
}

/* ========================================================================
 * 公共 API 实现
 * ======================================================================== */

bm25_index_t* bm25_index_create(bm25_config_t config) {
    bm25_index_t *index = calloc(1, sizeof(bm25_index_t));
    if (!index) {
        LOG_ERROR("bm25_index: 内存分配失败");
        return NULL;
    }
    index->config = config;
    /* 默认参数 */
    if (config.k1 <= 0) index->config.k1 = 1.2f;
    if (config.b <= 0 || config.b > 1) index->config.b = 0.75f;
    return index;
}

void bm25_index_free(bm25_index_t *index) {
    if (!index) return;

    pthread_mutex_lock(&g_bm25_state.mutex);

    /* 释放词条文本 */
    for (uint32_t i = 0; i < index->term_count; i++) {
        free(index->terms[i].term);
    }
    free(index->terms);
    free(index->docs);

    /* 释放词条哈希表 */
    term_hash_entry_t *entry, *tmp;
    HASH_ITER(hh, g_bm25_state.term_hash, entry, tmp) {
        HASH_DEL(g_bm25_state.term_hash, entry);
        free(entry);
    }

    /* 释放文档词频数组 */
    for (uint32_t i = 0; i < g_bm25_state.docs_with_tf_count; i++) {
        free(g_bm25_state.docs_with_tf[i].tfs);
    }
    free(g_bm25_state.docs_with_tf);
    g_bm25_state.docs_with_tf = NULL;
    g_bm25_state.docs_with_tf_count = 0;
    g_bm25_state.docs_with_tf_capacity = 0;

    pthread_mutex_unlock(&g_bm25_state.mutex);

    free(index);
}

int bm25_index_add_document(bm25_index_t *index, uint64_t doc_id, const char *text) {
    if (!index || !text) {
        LOG_ERROR("bm25_index_add_document: 参数错误");
        return -1;
    }

    pthread_mutex_lock(&g_bm25_state.mutex);

    /* 检查文档是否已存在 */
    for (uint32_t i = 0; i < index->doc_count; i++) {
        if (index->docs[i].doc_id == doc_id) {
            LOG_ERROR("bm25_index_add_document: 文档 %lu 已存在", (unsigned long)doc_id);
            pthread_mutex_unlock(&g_bm25_state.mutex);
            return -1;
        }
    }

    /* 动态分配分词结果 */
    uint32_t max_tokens = 1024;
    char **tokens = calloc(max_tokens, sizeof(char *));
    if (!tokens) {
        LOG_ERROR("bm25_index_add_document: 分词数组分配失败");
        pthread_mutex_unlock(&g_bm25_state.mutex);
        return -1;
    }
    uint32_t num_tokens = simple_tokenize(text, tokens, max_tokens);
    if (num_tokens == 0) {
        LOG_WARN("bm25_index_add_document: 文档 %lu 无有效词条", (unsigned long)doc_id);
        /* 仍然添加空文档 */
    }

    /* 更新词条信息 */
    for (uint32_t i = 0; i < num_tokens; i++) {
        int32_t term_idx = bm25_find_term(index, tokens[i]);
        if (term_idx < 0) {
            term_idx = bm25_add_term(index, tokens[i]);
            if (term_idx < 0) {
                for (uint32_t k = 0; k < num_tokens; k++) free(tokens[k]);
                free(tokens);
                pthread_mutex_unlock(&g_bm25_state.mutex);
                return -1;
            }
        }
        index->terms[term_idx].total_tf++;
    }

    /* 计算词频（去重） - 动态分配 */
    doc_tf_entry_t *tfs = calloc(num_tokens > 0 ? num_tokens : 1, sizeof(doc_tf_entry_t));
    if (!tfs) {
        LOG_ERROR("bm25_index_add_document: 词频数组分配失败");
        for (uint32_t k = 0; k < num_tokens; k++) free(tokens[k]);
        free(tokens);
        pthread_mutex_unlock(&g_bm25_state.mutex);
        return -1;
    }
    uint32_t tf_count = 0;
    for (uint32_t i = 0; i < num_tokens; i++) {
        int32_t term_idx = bm25_find_term(index, tokens[i]);
        if (term_idx < 0) continue;

        bool found = false;
        for (uint32_t j = 0; j < tf_count; j++) {
            if (tfs[j].term_idx == (uint32_t)term_idx) {
                tfs[j].tf++;
                found = true;
                break;
            }
        }
        if (!found) {
            tfs[tf_count].term_idx = (uint32_t)term_idx;
            tfs[tf_count].tf = 1;
            tf_count++;
        }
    }

    for (uint32_t i = 0; i < tf_count; i++) {
        index->terms[tfs[i].term_idx].doc_freq++;
    }

    if (index->doc_count >= index->doc_capacity) {
        uint32_t new_cap = index->doc_capacity ? index->doc_capacity * 2 : 16;
        bm25_doc_t *new_docs = realloc(index->docs, new_cap * sizeof(bm25_doc_t));
        if (!new_docs) {
            LOG_ERROR("bm25_index: 文档数组扩容失败");
            for (uint32_t k = 0; k < num_tokens; k++) free(tokens[k]);
            free(tokens);
            free(tfs);
            pthread_mutex_unlock(&g_bm25_state.mutex);
            return -1;
        }
        index->docs = new_docs;
        index->doc_capacity = new_cap;
    }

    uint32_t doc_idx = index->doc_count++;
    index->docs[doc_idx].doc_id = doc_id;
    index->docs[doc_idx].length = num_tokens;
    index->docs[doc_idx].avg_dl = index->avg_dl;
    index->total_terms += num_tokens;
    index->avg_dl = (float)index->total_terms / (float)index->doc_count;

    if (g_bm25_state.docs_with_tf_count >= g_bm25_state.docs_with_tf_capacity) {
        uint32_t new_cap = g_bm25_state.docs_with_tf_capacity ? g_bm25_state.docs_with_tf_capacity * 2 : 16;
        doc_with_tf_t *new_arr = realloc(g_bm25_state.docs_with_tf, new_cap * sizeof(doc_with_tf_t));
        if (!new_arr) {
            LOG_ERROR("bm25_index: 文档词频数组扩容失败");
            for (uint32_t k = 0; k < num_tokens; k++) free(tokens[k]);
            free(tokens);
            free(tfs);
            pthread_mutex_unlock(&g_bm25_state.mutex);
            return -1;
        }
        g_bm25_state.docs_with_tf = new_arr;
        g_bm25_state.docs_with_tf_capacity = new_cap;
    }
    uint32_t tf_idx = g_bm25_state.docs_with_tf_count++;
    g_bm25_state.docs_with_tf[tf_idx].doc_id = doc_id;
    g_bm25_state.docs_with_tf[tf_idx].length = num_tokens;
    g_bm25_state.docs_with_tf[tf_idx].tf_count = tf_count;
    g_bm25_state.docs_with_tf[tf_idx].tf_capacity = tf_count > 0 ? tf_count : 1;
    g_bm25_state.docs_with_tf[tf_idx].tfs = malloc(g_bm25_state.docs_with_tf[tf_idx].tf_capacity * sizeof(doc_tf_entry_t));
    if (!g_bm25_state.docs_with_tf[tf_idx].tfs) {
        LOG_ERROR("bm25_index: 词频条目分配失败");
        for (uint32_t k = 0; k < num_tokens; k++) free(tokens[k]);
        free(tokens);
        free(tfs);
        pthread_mutex_unlock(&g_bm25_state.mutex);
        return -1;
    }
    memcpy(g_bm25_state.docs_with_tf[tf_idx].tfs, tfs, tf_count * sizeof(doc_tf_entry_t));

    for (uint32_t k = 0; k < num_tokens; k++) free(tokens[k]);
    free(tokens);
    free(tfs);

    pthread_mutex_unlock(&g_bm25_state.mutex);

    LOG_DEBUG("bm25_index_add_document: 添加文档 %lu, 词数=%u", (unsigned long)doc_id, num_tokens);
    return 0;
}

float bm25_score(const bm25_index_t *index, uint64_t doc_id, const char *query) {
    if (!index || !query || index->doc_count == 0) {
        return 0.0f;
    }

    pthread_mutex_lock(&g_bm25_state.mutex);

    /* 查找文档 */
    int32_t doc_local_idx = -1;
    uint32_t tf_arr_idx = (uint32_t)-1;
    for (uint32_t i = 0; i < index->doc_count; i++) {
        if (index->docs[i].doc_id == doc_id) {
            doc_local_idx = (int32_t)i;
            break;
        }
    }
    for (uint32_t i = 0; i < g_bm25_state.docs_with_tf_count; i++) {
        if (g_bm25_state.docs_with_tf[i].doc_id == doc_id) {
            tf_arr_idx = i;
            break;
        }
    }
    if (doc_local_idx < 0 || tf_arr_idx == (uint32_t)-1) {
        pthread_mutex_unlock(&g_bm25_state.mutex);
        return 0.0f;
    }

    const bm25_doc_t *doc = &index->docs[doc_local_idx];
    const doc_with_tf_t *doc_tf = &g_bm25_state.docs_with_tf[tf_arr_idx];
    float avg_dl = index->avg_dl > 0 ? index->avg_dl : 1.0f;
    float k1 = index->config.k1;
    float b = index->config.b;
    uint32_t N = index->doc_count;

    /* 分词查询 - 动态分配 */
    uint32_t max_tokens = 256;
    char **tokens = calloc(max_tokens, sizeof(char *));
    if (!tokens) {
        pthread_mutex_unlock(&g_bm25_state.mutex);
        return 0.0f;
    }
    uint32_t num_tokens = simple_tokenize(query, tokens, max_tokens);
    if (num_tokens == 0) {
        free(tokens);
        pthread_mutex_unlock(&g_bm25_state.mutex);
        return 0.0f;
    }

    float score = 0.0f;
    for (uint32_t q = 0; q < num_tokens; q++) {
        int32_t term_idx = bm25_find_term(index, tokens[q]);
        if (term_idx < 0) continue;

        const bm25_term_t *term = &index->terms[term_idx];
        uint32_t n_q = term->doc_freq;

        /* IDF: log((N - n(q) + 0.5) / (n(q) + 0.5)) */
        float idf = logf(((float)N - (float)n_q + 0.5f) / ((float)n_q + 0.5f));
        if (idf < 0) idf = 0;  /* 防止负 IDF */

        /* TF: 在此文档中的词频 */
        uint32_t tf = 0;
        for (uint32_t i = 0; i < doc_tf->tf_count; i++) {
            if (doc_tf->tfs[i].term_idx == (uint32_t)term_idx) {
                tf = doc_tf->tfs[i].tf;
                break;
            }
        }

        /* BM25 公式 */
        float numerator = (float)tf * (k1 + 1.0f);
        float denominator = (float)tf + k1 * (1.0f - b + b * (float)doc->length / avg_dl);
        float tf_component = numerator / (denominator + 1e-6f);

        score += idf * tf_component;
    }

    for (uint32_t i = 0; i < num_tokens; i++) free(tokens[i]);
    free(tokens);

    pthread_mutex_unlock(&g_bm25_state.mutex);
    return score;
}

int bm25_search(const bm25_index_t *index, const char *query, uint32_t top_k,
                uint64_t *results, float *scores) {
    if (!index || !query || !results || top_k == 0) {
        LOG_ERROR("bm25_search: 参数错误");
        return 0;
    }

    if (index->doc_count == 0) {
        return 0;
    }

    /* 计算所有文档的分数 */
    float *all_scores = malloc(index->doc_count * sizeof(float));
    if (!all_scores) {
        LOG_ERROR("bm25_search: 内存分配失败");
        return 0;
    }

    for (uint32_t i = 0; i < index->doc_count; i++) {
        all_scores[i] = bm25_score(index, index->docs[i].doc_id, query);
    }

    /* 选择 top-k（简单选择排序，文档数通常不大） */
    uint32_t returned = 0;
    for (uint32_t r = 0; r < top_k && r < index->doc_count; r++) {
        float max_score = -1.0f;
        uint32_t max_idx = r;
        for (uint32_t i = r + 1; i < index->doc_count; i++) {
            if (all_scores[i] > max_score) {
                max_score = all_scores[i];
                max_idx = i;
            }
        }
        if (max_score > 0 || all_scores[max_idx] == 0) {
            results[r] = index->docs[max_idx].doc_id;
            if (scores) scores[r] = all_scores[max_idx];
            returned++;
        }
        /* 交换 */
        float tmp_s = all_scores[r];
        all_scores[r] = all_scores[max_idx];
        all_scores[max_idx] = tmp_s;
    }

    free(all_scores);
    return (int)returned;
}
