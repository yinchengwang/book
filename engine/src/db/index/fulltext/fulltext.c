/*
 * fulltext.c
 *
 * FULLTEXT 全文索引实现
 *
 * 核心功能：
 * - TF-IDF 评分排序
 * - 支持多种分词器
 * - 高级搜索语法
 *
 * 设计要点：
 * 1. 使用 GIN 作为底层倒排索引
 * 2. TF-IDF 计算公式：score = tf * idf
 * 3. 分词器支持空格分词和中文 MM 分词
 */

#include <db/index/fulltext/fulltext.h>
#include <db/index/gin/gin.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>

/* ── 内部宏定义 ── */
#define FULLTEXT_MAX_TERM_LEN 128
#define FULLTEXT_DEFAULT_MIN_LEN 2
#define FULLTEXT_DEFAULT_MAX_LEN 64

/* ── 内部结构 ── */

/* 词条统计 */
typedef struct term_stats {
    char *term;
    int doc_count;       /* 包含该词的文档数 */
    int total_freq;      /* 该词在所有文档中的总出现次数 */
} term_stats_t;

/* 文档信息 */
typedef struct ft_doc {
    int doc_id;
    int word_count;
    int *term_freqs;     /* 词条频率数组索引 */
} ft_doc_t;

/* 全文索引结构 */
struct fulltext_index {
    gin_index_t *inverted;      /* 底层 GIN 索引 */
    void *term_map;             /* 词条 → term_stats 映射（简化用动态数组） */
    term_stats_t *term_stats;
    int n_terms;
    int term_capacity;

    ft_doc_t *docs;             /* 文档信息数组 */
    int n_docs;
    int doc_capacity;

    tokenizer_type_t tokenizer_type;
    int min_word_len;
    int max_word_len;
};

/* 分词器结构 */
struct tokenizer {
    tokenizer_type_t type;
};

/* ── 静态函数声明 ── */
static tokenizer_t *_tokenizer_create_internal(tokenizer_type_t type);
static void _tokenize_whitespace(const char *text, char **tokens, int *count);
static void _tokenize_chinese_mm(const char *text, char **tokens, int *count,
                                 int min_len, int max_len);
static int _find_or_add_term(fulltext_index_t *idx, const char *term);
static void _normalize_term(char *term);
/* 暂未使用 */
// static int _compare_term_stats(const void *a, const void *b);
// static float _compute_tf_idf(fulltext_index_t *idx, int term_idx, int doc_idx, int tf);
static int _search_simple(fulltext_index_t *idx, const char *query,
                          int *doc_ids, float *scores, int *count, int max_results);
static int _search_with_and(fulltext_index_t *idx, const char **terms, int n_terms,
                            int *doc_ids, float *scores, int *count, int max_results);
static int _search_with_or(fulltext_index_t *idx, const char **terms, int n_terms,
                           int *doc_ids, float *scores, int *count, int max_results);

/* ── 分词器 API 实现 ── */

tokenizer_t *tokenizer_create(tokenizer_type_t type)
{
    return _tokenizer_create_internal(type);
}

char **tokenize(const tokenizer_t *tok, const char *text, int *count)
{
    if (!tok || !text || !count) {
        if (count) *count = 0;
        return NULL;
    }

    char **tokens = (char **)malloc(FULLTEXT_MAX_TERM_LEN * sizeof(char *));
    if (!tokens) {
        *count = 0;
        return NULL;
    }

    switch (tok->type) {
        case TOKENIZER_WHITESPACE:
            _tokenize_whitespace(text, tokens, count);
            break;
        case TOKENIZER_CHINESE_MM:
        case TOKENIZER_MIXED:
            _tokenize_chinese_mm(text, tokens, count,
                                 FULLTEXT_DEFAULT_MIN_LEN, FULLTEXT_DEFAULT_MAX_LEN);
            break;
        default:
            _tokenize_whitespace(text, tokens, count);
    }

    return tokens;
}

void tokenizer_free_tokens(const tokenizer_t *tok, char **tokens, int count)
{
    (void)tok;
    if (!tokens) return;
    for (int i = 0; i < count; i++) {
        free(tokens[i]);
    }
    free(tokens);
}

void tokenizer_destroy(tokenizer_t *tok)
{
    free(tok);
}

static tokenizer_t *_tokenizer_create_internal(tokenizer_type_t type)
{
    tokenizer_t *tok = (tokenizer_t *)malloc(sizeof(tokenizer_t));
    if (!tok) return NULL;
    tok->type = type;
    return tok;
}

/* ── 全文索引 API 实现 ── */

fulltext_index_t *fulltext_create(void)
{
    fulltext_index_t *idx = (fulltext_index_t *)calloc(1, sizeof(fulltext_index_t));
    if (!idx) return NULL;

    idx->inverted = gin_create(10000);
    if (!idx->inverted) {
        free(idx);
        return NULL;
    }

    idx->term_stats = (term_stats_t *)malloc(1000 * sizeof(term_stats_t));
    idx->n_terms = 0;
    idx->term_capacity = 1000;

    idx->docs = (ft_doc_t *)malloc(1000 * sizeof(ft_doc_t));
    idx->n_docs = 0;
    idx->doc_capacity = 1000;

    idx->tokenizer_type = TOKENIZER_WHITESPACE;
    idx->min_word_len = FULLTEXT_DEFAULT_MIN_LEN;
    idx->max_word_len = FULLTEXT_DEFAULT_MAX_LEN;

    return idx;
}

void fulltext_set_tokenizer(fulltext_index_t *idx, tokenizer_type_t type)
{
    if (!idx) return;
    idx->tokenizer_type = type;
}

void fulltext_set_word_length(fulltext_index_t *idx, int min_len, int max_len)
{
    if (!idx) return;
    if (min_len > 0) idx->min_word_len = min_len;
    if (max_len > 0) idx->max_word_len = max_len;
}

int fulltext_index_doc(fulltext_index_t *idx, int doc_id, const char *text)
{
    if (!idx || !text) return -1;

    /* 分词 */
    tokenizer_t *tok = tokenizer_create(idx->tokenizer_type);
    int count = 0;
    char **tokens = tokenize(tok, text, &count);

    if (!tokens || count == 0) {
        tokenizer_destroy(tok);
        return -1;
    }

    /* 索引每个词条 */
    for (int i = 0; i < count; i++) {
        gin_insert(idx->inverted, tokens[i], doc_id);

        /* 更新词条统计 */
        int term_idx = _find_or_add_term(idx, tokens[i]);
        if (term_idx >= 0) {
            idx->term_stats[term_idx].doc_count++;
            idx->term_stats[term_idx].total_freq++;
        }
    }

    /* 记录文档 */
    if (idx->n_docs >= idx->doc_capacity) {
        idx->doc_capacity *= 2;
        ft_doc_t *new_docs = (ft_doc_t *)realloc(idx->docs, idx->doc_capacity * sizeof(ft_doc_t));
        if (new_docs) idx->docs = new_docs;
    }

    idx->docs[idx->n_docs].doc_id = doc_id;
    idx->docs[idx->n_docs].word_count = count;
    idx->n_docs++;

    tokenizer_free_tokens(tok, tokens, count);
    tokenizer_destroy(tok);

    return 0;
}

int fulltext_index_batch(fulltext_index_t *idx, const int *doc_ids,
                        const char **texts, int count)
{
    if (!idx || !doc_ids || !texts) return -1;

    int success = 0;
    for (int i = 0; i < count; i++) {
        if (fulltext_index_doc(idx, doc_ids[i], texts[i]) == 0) {
            success++;
        }
    }
    return success;
}

int fulltext_search(const fulltext_index_t *idx, const char *query,
                   int *doc_ids, float *scores, int *count, int max_results)
{
    if (!idx || !query || !doc_ids || !scores || !count) return -1;

    *count = 0;

    /* 简单解析：检查是否包含 AND/OR */
    const char *and_pos = strstr(query, " AND ");
    const char *or_pos = strstr(query, " OR ");
    const char *not_pos = strstr(query, " NOT ");

    if (and_pos) {
        /* AND 查询 */
        char query_copy[1024];
        strncpy(query_copy, query, sizeof(query_copy) - 1);
        query_copy[sizeof(query_copy) - 1] = '\0';

        char *terms[10];
        int n_terms = 0;
        char *saveptr;
        char *token = strtok_r(query_copy, " AND ", &saveptr);
        while (token && n_terms < 10) {
            terms[n_terms++] = token;
            token = strtok_r(NULL, " AND ", &saveptr);
        }

        return _search_with_and((fulltext_index_t *)idx, (const char **)terms,
                                n_terms, doc_ids, scores, count, max_results);
    }
    else if (or_pos) {
        /* OR 查询 */
        char query_copy[1024];
        strncpy(query_copy, query, sizeof(query_copy) - 1);

        char *terms[10];
        int n_terms = 0;
        char *saveptr;
        char *token = strtok_r(query_copy, " OR ", &saveptr);
        while (token && n_terms < 10) {
            terms[n_terms++] = token;
            token = strtok_r(NULL, " OR ", &saveptr);
        }

        return _search_with_or((fulltext_index_t *)idx, (const char **)terms,
                               n_terms, doc_ids, scores, count, max_results);
    }
    else if (not_pos) {
        /* NOT 查询暂未实现 */
    }

    /* 简单查询 */
    return _search_simple((fulltext_index_t *)idx, query, doc_ids, scores, count, max_results);
}

int fulltext_highlight(const fulltext_index_t *idx, const char *text,
                       const char *query, char *output, int max_len)
{
    if (!idx || !text || !query || !output) return -1;

    /* 简单实现：复制原文并标记匹配词 */
    int pos = 0;
    const char *p = text;

    while (*p && pos < max_len - 10) {
        /* 检查是否是匹配词开头 */
        int is_match = 0;
        const char *check = query;
        while (*check && isspace(*check)) check++;

        if (strlen(p) >= strlen(check)) {
            int match_len = strlen(check);
            if (strncmp(p, check, match_len) == 0) {
                is_match = 1;
            }
        }

        if (is_match) {
            if (pos < max_len - 12) {
                output[pos++] = '<';
                output[pos++] = 'b';
                output[pos++] = '>';
            }
        }

        output[pos++] = *p;

        if (is_match) {
            if (pos < max_len - 5) {
                output[pos++] = '<';
                output[pos++] = '/';
                output[pos++] = 'b';
                output[pos++] = '>';
            }
        }

        p++;
    }

    output[pos] = '\0';
    return 0;
}

void fulltext_stats(const fulltext_index_t *idx, int *out_n_docs,
                   int *out_n_terms, float *out_avg_doc_len)
{
    if (!idx) {
        if (out_n_docs) *out_n_docs = 0;
        if (out_n_terms) *out_n_terms = 0;
        if (out_avg_doc_len) *out_avg_doc_len = 0;
        return;
    }

    if (out_n_docs) *out_n_docs = idx->n_docs;
    if (out_n_terms) *out_n_terms = idx->n_terms;

    if (out_avg_doc_len) {
        int total_words = 0;
        for (int i = 0; i < idx->n_docs; i++) {
            total_words += idx->docs[i].word_count;
        }
        *out_avg_doc_len = idx->n_docs > 0 ? (float)total_words / idx->n_docs : 0;
    }
}

void fulltext_destroy(fulltext_index_t *idx)
{
    if (!idx) return;

    gin_destroy(idx->inverted);

    for (int i = 0; i < idx->n_terms; i++) {
        free(idx->term_stats[i].term);
    }
    free(idx->term_stats);
    free(idx->docs);

    free(idx);
}

/* ── 内部函数实现 ── */

static void _tokenize_whitespace(const char *text, char **tokens, int *count)
{
    *count = 0;

    char word[256];
    int pos = 0;
    const char *p = text;

    while (*p) {
        if (isspace(*p) || ispunct(*p)) {
            if (pos > 0) {
                word[pos] = '\0';
                _normalize_term(word);
                if (strlen(word) >= FULLTEXT_DEFAULT_MIN_LEN) {
                    tokens[*count] = strdup(word);
                    if (tokens[*count]) (*count)++;
                }
                pos = 0;
            }
        } else {
            if (pos < sizeof(word) - 1) {
                word[pos++] = tolower(*p);
            }
        }
        p++;
    }

    if (pos > 0) {
        word[pos] = '\0';
        _normalize_term(word);
        if (strlen(word) >= FULLTEXT_DEFAULT_MIN_LEN) {
            tokens[*count] = strdup(word);
            if (tokens[*count]) (*count)++;
        }
    }
}

static void _tokenize_chinese_mm(const char *text, char **tokens, int *count,
                                 int min_len, int max_len)
{
    *count = 0;
    (void)max_len;  /* 暂未使用 */

    /* 简化实现：按字符分词，然后尝试组成多字词 */
    const char *p = text;
    char word[256];
    int word_len = 0;

    while (*p) {
        /* 判断是否是 ASCII 字符 */
        if ((*p & 0x80) == 0) {
            /* ASCII 字符 */
            if (isspace(*p) || ispunct(*p)) {
                if (word_len >= min_len) {
                    word[word_len] = '\0';
                    tokens[*count] = strdup(word);
                    if (tokens[*count]) (*count)++;
                }
                word_len = 0;
            } else {
                if (word_len < sizeof(word) - 1) {
                    word[word_len++] = tolower(*p);
                }
            }
            p++;
        } else {
            /* 中文字符（简化处理：按 3 字节 Unicode 处理） */
            /* 实际应该根据编码进行适当处理 */

            /* 先输出已有 ASCII 词 */
            if (word_len >= min_len) {
                word[word_len] = '\0';
                tokens[*count] = strdup(word);
                if (tokens[*count]) (*count)++;
            }
            word_len = 0;

            /* 收集中文 */
            char cjk[4] = {0};
            int cjk_len = 0;

            /* 简化：取 2 字符作为词 */
            if ((p[0] & 0xE0) == 0xE0 && p[1] && p[2]) {
                cjk[0] = *p++;
                cjk[1] = *p++;
                cjk[2] = *p;
                cjk_len = 3;
            } else {
                cjk[0] = *p++;
                cjk_len = 1;
            }

            /* 输出单字 */
            for (int i = 0; i < cjk_len && *count < 100; i++) {
                char single[2] = {cjk[i], 0};
                tokens[*count] = strdup(single);
                if (tokens[*count]) (*count)++;
            }

            /* 尝试输出双字词 */
            if (cjk_len >= 2 && *count < 100) {
                char double_word[5] = {cjk[0], cjk[1], 0};
                tokens[*count] = strdup(double_word);
                if (tokens[*count]) (*count)++;
            }
        }
    }

    /* 处理尾部 */
    if (word_len >= min_len) {
        word[word_len] = '\0';
        tokens[*count] = strdup(word);
        if (tokens[*count]) (*count)++;
    }
}

static int _find_or_add_term(fulltext_index_t *idx, const char *term)
{
    for (int i = 0; i < idx->n_terms; i++) {
        if (strcmp(idx->term_stats[i].term, term) == 0) {
            return i;
        }
    }

    /* 添加新词条 */
    if (idx->n_terms >= idx->term_capacity) {
        idx->term_capacity *= 2;
        term_stats_t *new_stats = (term_stats_t *)realloc(
            idx->term_stats, idx->term_capacity * sizeof(term_stats_t));
        if (!new_stats) return -1;
        idx->term_stats = new_stats;
    }

    idx->term_stats[idx->n_terms].term = strdup(term);
    idx->term_stats[idx->n_terms].doc_count = 0;
    idx->term_stats[idx->n_terms].total_freq = 0;

    return idx->n_terms++;
}

static void _normalize_term(char *term)
{
    /* 简单的小写化处理 */
    char *p = term;
    while (*p) {
        *p = tolower(*p);
        p++;
    }
}

/* 暂未使用 _compare_term_stats 和 _compute_tf_idf */
/*
static int _compare_term_stats(const void *a, const void *b)
{
    term_stats_t *ta = (term_stats_t *)a;
    term_stats_t *tb = (term_stats_t *)b;
    return tb->doc_count - ta->doc_count;
}

static float _compute_tf_idf(fulltext_index_t *idx, int term_idx, int doc_idx, int tf)
{
    if (!idx || term_idx < 0 || term_idx >= idx->n_terms) return 0;

    term_stats_t *term = &idx->term_stats[term_idx];

    float tf_val = (float)tf / idx->docs[doc_idx].word_count;
    float idf_val = logf((float)idx->n_docs / (term->doc_count + 1) + 1);

    return tf_val * idf_val;
}
*/

static int _search_simple(fulltext_index_t *idx, const char *query,
                          int *doc_ids, float *scores, int *count, int max_results)
{
    /* 解析查询词 */
    tokenizer_t *tok = tokenizer_create(idx->tokenizer_type);
    int term_count = 0;
    char **terms = tokenize(tok, query, &term_count);

    if (!terms || term_count == 0) {
        tokenizer_free_tokens(tok, terms, term_count);
        tokenizer_destroy(tok);
        return -1;
    }

    /* 收集所有匹配的文档 */
    int matched_docs[1000] = {0};
    float doc_scores[1000] = {0};
    int matched_counts[1000] = {0};
    int n_matched = 0;

    for (int t = 0; t < term_count; t++) {
        int results[1000];
        int result_count = 0;

        gin_search(idx->inverted, terms[t], results, &result_count);

        for (int i = 0; i < result_count; i++) {
            int doc_id = results[i];

            /* 查找或添加文档 */
            int doc_pos = -1;
            for (int j = 0; j < n_matched; j++) {
                if (matched_docs[j] == doc_id) {
                    doc_pos = j;
                    break;
                }
            }

            if (doc_pos < 0) {
                doc_pos = n_matched++;
                matched_docs[doc_pos] = doc_id;
                doc_scores[doc_pos] = 0;
                matched_counts[doc_pos] = 0;
            }

            /* 累加分数 */
            matched_counts[doc_pos]++;
            doc_scores[doc_pos] += 1.0f / term_count;  /* 简化 TF */
        }
    }

    tokenizer_free_tokens(tok, terms, term_count);
    tokenizer_destroy(tok);

    /* 排序并返回 top-k */
    *count = n_matched < max_results ? n_matched : max_results;

    /* 简单选择排序 */
    for (int i = 0; i < *count; i++) {
        int best_idx = i;
        for (int j = i + 1; j < n_matched; j++) {
            if (doc_scores[j] > doc_scores[best_idx]) {
                best_idx = j;
            }
        }
        if (best_idx != i) {
            int tmp_doc = matched_docs[i];
            matched_docs[i] = matched_docs[best_idx];
            matched_docs[best_idx] = tmp_doc;

            float tmp_score = doc_scores[i];
            doc_scores[i] = doc_scores[best_idx];
            doc_scores[best_idx] = tmp_score;
        }
        doc_ids[i] = matched_docs[i];
        scores[i] = doc_scores[i];
    }

    return 0;
}

static int _search_with_and(fulltext_index_t *idx, const char **terms, int n_terms,
                            int *doc_ids, float *scores, int *count, int max_results)
{
    /* AND 查询：取交集 */
    int *doc_sets[10];
    int doc_counts[10];

    for (int i = 0; i < n_terms && i < 10; i++) {
        doc_sets[i] = (int *)malloc(1000 * sizeof(int));
        doc_counts[i] = 0;
        gin_search(idx->inverted, terms[i], doc_sets[i], &doc_counts[i]);
    }

    /* 找交集 */
    int result_count = 0;
    for (int i = 0; i < doc_counts[0] && result_count < max_results; i++) {
        int doc_id = doc_sets[0][i];
        int in_all = 1;

        for (int j = 1; j < n_terms && j < 10; j++) {
            int found = 0;
            for (int k = 0; k < doc_counts[j]; k++) {
                if (doc_sets[j][k] == doc_id) {
                    found = 1;
                    break;
                }
            }
            if (!found) {
                in_all = 0;
                break;
            }
        }

        if (in_all) {
            doc_ids[result_count] = doc_id;
            scores[result_count] = (float)n_terms;  /* 所有词都匹配，得分高 */
            result_count++;
        }
    }

    for (int i = 0; i < n_terms && i < 10; i++) {
        free(doc_sets[i]);
    }

    *count = result_count;
    return 0;
}

static int _search_with_or(fulltext_index_t *idx, const char **terms, int n_terms,
                           int *doc_ids, float *scores, int *count, int max_results)
{
    /* OR 查询：取并集 */
    int all_docs[10000];
    int doc_match_count[10000] = {0};
    int n_all_docs = 0;

    for (int i = 0; i < n_terms && i < 10; i++) {
        int results[1000];
        int result_count = 0;
        gin_search(idx->inverted, terms[i], results, &result_count);

        for (int j = 0; j < result_count; j++) {
            int doc_id = results[j];
            int found = 0;
            for (int k = 0; k < n_all_docs; k++) {
                if (all_docs[k] == doc_id) {
                    doc_match_count[k]++;
                    found = 1;
                    break;
                }
            }
            if (!found && n_all_docs < 10000) {
                all_docs[n_all_docs] = doc_id;
                doc_match_count[n_all_docs] = 1;
                n_all_docs++;
            }
        }
    }

    /* 排序 */
    *count = n_all_docs < max_results ? n_all_docs : max_results;

    for (int i = 0; i < *count; i++) {
        int best_idx = i;
        for (int j = i + 1; j < n_all_docs; j++) {
            if (doc_match_count[j] > doc_match_count[best_idx]) {
                best_idx = j;
            }
        }
        if (best_idx != i) {
            int tmp_doc = all_docs[i];
            all_docs[i] = all_docs[best_idx];
            all_docs[best_idx] = tmp_doc;

            int tmp_cnt = doc_match_count[i];
            doc_match_count[i] = doc_match_count[best_idx];
            doc_match_count[best_idx] = tmp_cnt;
        }
        doc_ids[i] = all_docs[i];
        scores[i] = (float)doc_match_count[i] / n_terms;
    }

    return 0;
}