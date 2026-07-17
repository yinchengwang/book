/**
 * @file doc_fts.c
 * @brief 全文检索增强实现
 */

#include "db/storage/doc/doc_fts.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

/* ========================================================================
 * 工具函数
 * ======================================================================== */

static int token_compare(const void *a, const void *b) {
    return ((const DocToken *)a)->position - ((const DocToken *)b)->position;
}

static bool is_delimiter(char c) {
    return isspace(c) || c == ',' || c == '.' || c == '!' ||
           c == '?' || c == ';' || c == ':' || c == '"' ||
           c == '\'' || c == '(' || c == ')' || c == '[' ||
           c == ']' || c == '{' || c == '}';
}

/* ========================================================================
 * 停用词
 * ======================================================================== */

DocStopwords *doc_stopwords_create(bool case_sensitive) {
    DocStopwords *sw = (DocStopwords *)calloc(1, sizeof(DocStopwords));
    if (sw) {
        sw->case_sensitive = case_sensitive;
        sw->words = (char **)calloc(256, sizeof(char *));
    }
    return sw;
}

int doc_stopwords_load(DocStopwords *sw, const char *filename) {
    if (!sw || !filename) return -1;

    FILE *f = fopen(filename, "r");
    if (!f) return -1;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        /* 去除换行符 */
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[--len] = '\0';
        }
        if (len > 0) {
            doc_stopwords_add(sw, line);
        }
    }

    fclose(f);
    return 0;
}

int doc_stopwords_add(DocStopwords *sw, const char *word) {
    if (!sw || !word) return -1;

    sw->words[sw->num_words++] = sw->case_sensitive ?
        strdup(word) : strdup(word);
    return 0;
}

bool doc_stopwords_contains(DocStopwords *sw, const char *word) {
    if (!sw || !word) return false;

    for (size_t i = 0; i < sw->num_words; i++) {
        if (sw->case_sensitive) {
            if (strcmp(sw->words[i], word) == 0) return true;
        } else {
            if (strcasecmp(sw->words[i], word) == 0) return true;
        }
    }
    return false;
}

void doc_stopwords_destroy(DocStopwords *sw) {
    if (!sw) return;
    for (size_t i = 0; i < sw->num_words; i++) {
        free(sw->words[i]);
    }
    free(sw->words);
    free(sw);
}

/* ========================================================================
 * 同义词
 * ======================================================================== */

DocSynonyms *doc_synonyms_create(void) {
    DocSynonyms *syn = (DocSynonyms *)calloc(1, sizeof(DocSynonyms));
    if (syn) {
        syn->groups = (DocSynonymGroup *)calloc(16, sizeof(DocSynonymGroup));
        syn->num_groups = 0;
    }
    return syn;
}

int doc_synonyms_load(DocSynonyms *syn, const char *filename) {
    if (!syn || !filename) return -1;

    FILE *f = fopen(filename, "r");
    if (!f) return -1;

    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        /* 解析 "word1, word2, word3" 格式 */
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[--len] = '\0';
        }

        char *terms[32];
        size_t num_terms = 0;
        char *tok = strtok(line, ",");
        while (tok && num_terms < 32) {
            /* 去除空格 */
            while (*tok == ' ') tok++;
            char *end = tok + strlen(tok) - 1;
            while (end > tok && *end == ' ') *end-- = '\0';
            terms[num_terms++] = tok;
            tok = strtok(NULL, ",");
        }

        if (num_terms > 0) {
            doc_synonyms_add_group(syn, terms, num_terms);
        }
    }

    fclose(f);
    return 0;
}

int doc_synonyms_add_group(DocSynonyms *syn, char **terms, size_t num_terms) {
    if (!syn || !terms || num_terms == 0) return -1;

    DocSynonymGroup *group = &syn->groups[syn->num_groups++];
    group->terms = (char **)calloc(num_terms, sizeof(char *));
    group->num_terms = num_terms;

    for (size_t i = 0; i < num_terms; i++) {
        group->terms[i] = strdup(terms[i]);
    }

    return 0;
}

size_t doc_synonyms_get(DocSynonyms *syn, const char *term,
                       char ***out_terms) {
    if (!syn || !term || !out_terms) return 0;

    for (size_t g = 0; g < syn->num_groups; g++) {
        DocSynonymGroup *group = &syn->groups[g];
        for (size_t i = 0; i < group->num_terms; i++) {
            if (strcasecmp(group->terms[i], term) == 0) {
                *out_terms = (char **)calloc(group->num_terms, sizeof(char *));
                for (size_t j = 0; j < group->num_terms; j++) {
                    (*out_terms)[j] = strdup(group->terms[j]);
                }
                return group->num_terms;
            }
        }
    }

    return 0;
}

void doc_synonyms_destroy(DocSynonyms *syn) {
    if (!syn) return;
    for (size_t g = 0; g < syn->num_groups; g++) {
        for (size_t i = 0; i < syn->groups[g].num_terms; i++) {
            free(syn->groups[g].terms[i]);
        }
        free(syn->groups[g].terms);
    }
    free(syn->groups);
    free(syn);
}

/* ========================================================================
 * 标准分词器
 * ======================================================================== */

typedef struct DocStandardTokenizer_s {
    DocTokenizerConfig config;
    DocStopwords *stopwords;
} DocStandardTokenizer;

static int standard_tokenizer_init(void *impl, const DocTokenizerConfig *config) {
    DocStandardTokenizer *st = (DocStandardTokenizer *)impl;
    if (config) {
        st->config = *config;
    }
    return 0;
}

static int standard_tokenizer_tokenize(void *impl, const char *text, size_t len,
                                       DocToken **out_tokens, size_t *out_count) {
    DocStandardTokenizer *st = (DocStandardTokenizer *)impl;
    DocToken *tokens = (DocToken *)calloc(len / 2 + 1, sizeof(DocToken));
    size_t num_tokens = 0;

    const char *p = text;
    int position = 0;
    int start = 0;

    while (*p && (size_t)(p - text) < len) {
        /* 跳过非字母数字字符 */
        if (is_delimiter(*p)) {
            p++;
            start++;
            continue;
        }

        /* 提取词 */
        const char *word_start = p;
        int word_start_offset = start;

        while (*p && !is_delimiter(*p) && (size_t)(p - text) < len) {
            char c = st->config.case_sensitive ? *p : tolower(*p);
            p++;
            start++;
        }

        int word_len = p - word_start;
        if (word_len > 0) {
            /* 检查停用词 */
            char *word = (char *)malloc(word_len + 1);
            for (int i = 0; i < word_len; i++) {
                word[i] = st->config.case_sensitive ? word_start[i] : tolower(word_start[i]);
            }
            word[word_len] = '\0';

            /* 检查最小长度和停用词 */
            bool skip = false;
            if (st->config.min_token_length > 0 && word_len < st->config.min_token_length) {
                skip = true;
            }
            if (st->config.remove_stopwords && st->stopwords &&
                doc_stopwords_contains(st->stopwords, word)) {
                skip = true;
            }

            if (!skip) {
                DocToken *t = &tokens[num_tokens++];
                t->text = word;
                t->start_offset = word_start_offset;
                t->end_offset = start;
                t->position = position++;
                t->flags = 0;
            } else {
                free(word);
            }
        }
    }

    *out_tokens = tokens;
    *out_count = num_tokens;
    return 0;
}

static void standard_tokenizer_reset(void *impl) {
    (void)impl;
}

static void standard_tokenizer_destroy(void *impl) {
    free(impl);
}

/* ========================================================================
 * 空白分词器
 * ======================================================================== */

typedef struct DocWhitespaceTokenizer_s {
    DocTokenizerConfig config;
} DocWhitespaceTokenizer;

static int whitespace_tokenizer_init(void *impl, const DocTokenizerConfig *config) {
    DocWhitespaceTokenizer *wt = (DocWhitespaceTokenizer *)impl;
    if (config) wt->config = *config;
    return 0;
}

static int whitespace_tokenizer_tokenize(void *impl, const char *text, size_t len,
                                        DocToken **out_tokens, size_t *out_count) {
    DocWhitespaceTokenizer *wt = (DocWhitespaceTokenizer *)impl;
    DocToken *tokens = (DocToken *)calloc(64, sizeof(DocToken));
    size_t num_tokens = 0;

    const char *p = text;
    int position = 0;
    int start = 0;

    while (*p && (size_t)(p - text) < len) {
        if (isspace(*p)) {
            p++;
            start++;
            continue;
        }

        const char *word_start = p;
        int word_start_offset = start;

        while (*p && !isspace(*p) && (size_t)(p - text) < len) {
            p++;
            start++;
        }

        int word_len = p - word_start;
        if (word_len > 0) {
            DocToken *t = &tokens[num_tokens++];
            t->text = (char *)malloc(word_len + 1);
            memcpy(t->text, word_start, word_len);
            t->text[word_len] = '\0';
            t->start_offset = word_start_offset;
            t->end_offset = start;
            t->position = position++;
            t->flags = 0;
        }
    }

    *out_tokens = tokens;
    *out_count = num_tokens;
    return 0;
}

static void whitespace_tokenizer_destroy(void *impl) {
    free(impl);
}

/* ========================================================================
 * 关键字分词器
 * ======================================================================== */

typedef struct DocKeywordTokenizer_s {
    DocTokenizerConfig config;
} DocKeywordTokenizer;

static int keyword_tokenizer_init(void *impl, const DocTokenizerConfig *config) {
    DocKeywordTokenizer *kt = (DocKeywordTokenizer *)impl;
    if (config) kt->config = *config;
    return 0;
}

static int keyword_tokenizer_tokenize(void *impl, const char *text, size_t len,
                                     DocToken **out_tokens, size_t *out_count) {
    (void)impl;
    DocToken *tokens = (DocToken *)calloc(1, sizeof(DocToken));
    tokens[0].text = (char *)malloc(len + 1);
    memcpy(tokens[0].text, text, len);
    tokens[0].text[len] = '\0';
    tokens[0].start_offset = 0;
    tokens[0].end_offset = (int)len;
    tokens[0].position = 0;
    tokens[0].flags = 0;

    *out_tokens = tokens;
    *out_count = 1;
    return 0;
}

static void keyword_tokenizer_destroy(void *impl) {
    free(impl);
}

/* ========================================================================
 * 分词器注册表
 * ======================================================================== */

static DocTokenizerOps standard_ops = {
    standard_tokenizer_init,
    standard_tokenizer_tokenize,
    NULL,
    standard_tokenizer_reset,
    standard_tokenizer_destroy
};

static DocTokenizerOps whitespace_ops = {
    whitespace_tokenizer_init,
    whitespace_tokenizer_tokenize,
    NULL,
    NULL,
    whitespace_tokenizer_destroy
};

static DocTokenizerOps keyword_ops = {
    keyword_tokenizer_init,
    keyword_tokenizer_tokenize,
    NULL,
    NULL,
    keyword_tokenizer_destroy
};

static DocTokenizerOps *g_registered_ops[8] = {
    &standard_ops,
    &whitespace_ops,
    &standard_ops,  /* simple 复用 standard */
    &keyword_ops,
    &standard_ops,  /* stem 复用 standard */
    &standard_ops,  /* cjk 复用 standard */
    NULL            /* custom */
};

/* ========================================================================
 * 分词器 API
 * ======================================================================== */

DocTokenizer *doc_tokenizer_create(DocTokenizerType type,
                                   const DocTokenizerConfig *config) {
    if (type >= DOC_TOKENIZER_CUSTOM) return NULL;

    DocTokenizerOps *ops = g_registered_ops[type];
    if (!ops) return NULL;

    DocTokenizer *tokenizer = (DocTokenizer *)calloc(1, sizeof(DocTokenizer));
    if (!tokenizer) return NULL;

    tokenizer->type = type;
    tokenizer->ops = ops;

    if (config) {
        tokenizer->config = *config;
    } else {
        tokenizer->config.type = type;
        tokenizer->config.min_token_length = 2;
        tokenizer->config.max_token_length = 256;
        tokenizer->config.case_sensitive = false;
        tokenizer->config.remove_stopwords = false;
    }

    /* 分配实现数据 */
    size_t impl_sizes[] = {
        sizeof(DocStandardTokenizer),
        sizeof(DocWhitespaceTokenizer),
        sizeof(DocStandardTokenizer),
        sizeof(DocKeywordTokenizer),
        sizeof(DocStandardTokenizer),
        sizeof(DocStandardTokenizer),
        0
    };

    tokenizer->impl = calloc(1, impl_sizes[type]);
    if (!tokenizer->impl) {
        free(tokenizer);
        return NULL;
    }

    if (ops->init) {
        ops->init(tokenizer->impl, &tokenizer->config);
    }

    return tokenizer;
}

void doc_tokenizer_destroy(DocTokenizer *tokenizer) {
    if (!tokenizer) return;
    if (tokenizer->ops && tokenizer->ops->destroy) {
        tokenizer->ops->destroy(tokenizer->impl);
    }
    free(tokenizer);
}

int doc_tokenizer_tokenize(DocTokenizer *tokenizer,
                          const char *text, size_t len,
                          DocToken **out_tokens, size_t *out_count) {
    if (!tokenizer || !text || !out_tokens || !out_count) return -1;

    if (tokenizer->ops && tokenizer->ops->tokenize) {
        return tokenizer->ops->tokenize(tokenizer->impl, text, len,
                                        out_tokens, out_count);
    }
    return -1;
}

void doc_tokenizer_free_tokens(DocToken *tokens, size_t count) {
    if (!tokens) return;
    for (size_t i = 0; i < count; i++) {
        free(tokens[i].text);
    }
    free(tokens);
}

int doc_tokenizer_register(DocTokenizerType type, DocTokenizerOps *ops) {
    if (type >= DOC_TOKENIZER_CUSTOM) return -1;
    g_registered_ops[type] = ops;
    return 0;
}

const char *doc_tokenizer_type_name(DocTokenizerType type) {
    const char *names[] = {
        "standard", "whitespace", "simple", "keyword",
        "stem", "cjk", "custom"
    };
    if (type >= DOC_TOKENIZER_CUSTOM) return "unknown";
    return names[type];
}

/* ========================================================================
 * 短语搜索
 * ======================================================================== */

size_t doc_phrase_search(const char **documents,
                        size_t num_docs,
                        const char *phrase,
                        DocTokenizer *tokenizer,
                        DocPhraseResult **results,
                        size_t max_results) {
    if (!documents || !phrase || !tokenizer || !results) return 0;

    *results = (DocPhraseResult *)calloc(max_results, sizeof(DocPhraseResult));
    size_t count = 0;

    /* 分词短语 */
    DocToken *phrase_tokens = NULL;
    size_t num_phrase_tokens = 0;
    doc_tokenizer_tokenize(tokenizer, phrase, strlen(phrase),
                          &phrase_tokens, &num_phrase_tokens);

    if (num_phrase_tokens == 0) {
        return 0;
    }

    /* 搜索每个文档 */
    for (size_t d = 0; d < num_docs && count < max_results; d++) {
        DocToken *doc_tokens = NULL;
        size_t num_doc_tokens = 0;

        doc_tokenizer_tokenize(tokenizer, documents[d], strlen(documents[d]),
                              &doc_tokens, &num_doc_tokens);

        /* 检查短语是否连续匹配 */
        for (size_t i = 0; i + num_phrase_tokens <= num_doc_tokens && count < max_results; i++) {
            bool match = true;
            int phrase_pos = phrase_tokens[0].position;

            for (size_t j = 0; j < num_phrase_tokens; j++) {
                if (doc_tokens[i + j].position != phrase_pos + (int)j) {
                    match = false;
                    break;
                }
                if (strcasecmp(doc_tokens[i + j].text, phrase_tokens[j].text) != 0) {
                    match = false;
                    break;
                }
            }

            if (match) {
                DocPhraseResult *r = &((*results)[count++]);
                r->document_id = (char *)documents[d];
                r->start_offset = doc_tokens[i].start_offset;
                r->end_offset = doc_tokens[i + num_phrase_tokens - 1].end_offset;
                r->match_length = r->end_offset - r->start_offset;
                r->score = 1.0;
                break;  /* 每个文档只返回第一个匹配 */
            }
        }

        doc_tokenizer_free_tokens(doc_tokens, num_doc_tokens);
    }

    doc_tokenizer_free_tokens(phrase_tokens, num_phrase_tokens);
    return count;
}

void doc_phrase_result_free(DocPhraseResult *results, size_t count) {
    (void)count;
    free(results);
}

/* ========================================================================
 * 高亮
 * ======================================================================== */

DocHighlightMarker DOC_HIGHLIGHT_DEFAULT = {"<em>", "</em>"};
DocHighlightMarker DOC_HIGHLIGHT_HTML = {"<mark>", "</mark>"};

size_t doc_highlight(const char *text,
                    const char *query,
                    DocTokenizer *tokenizer,
                    const DocHighlightMarker *marker,
                    size_t max_fragments,
                    size_t fragment_size,
                    DocHighlightFragment **out_fragments) {
    if (!text || !query || !tokenizer || !out_fragments) return 0;

    if (!marker) marker = &DOC_HIGHLIGHT_DEFAULT;

    *out_fragments = (DocHighlightFragment *)calloc(max_fragments,
        sizeof(DocHighlightFragment));
    size_t num_fragments = 0;

    /* 分词查询词 */
    DocToken *query_tokens = NULL;
    size_t num_query_tokens = 0;
    doc_tokenizer_tokenize(tokenizer, query, strlen(query),
                          &query_tokens, &num_query_tokens);

    if (num_query_tokens == 0) {
        return 0;
    }

    /* 分词文本 */
    DocToken *text_tokens = NULL;
    size_t num_text_tokens = 0;
    doc_tokenizer_tokenize(tokenizer, text, strlen(text),
                          &text_tokens, &num_text_tokens);

    /* 找到匹配的 token */
    int *match_positions = (int *)calloc(num_text_tokens, sizeof(int));
    int num_matches = 0;

    for (size_t qi = 0; qi < num_query_tokens; qi++) {
        for (size_t ti = 0; ti < num_text_tokens; ti++) {
            if (strcasecmp(text_tokens[ti].text, query_tokens[qi].text) == 0) {
                match_positions[num_matches++] = (int)ti;
            }
        }
    }

    /* 生成高亮片段 */
    if (num_matches > 0 && num_fragments < max_fragments) {
        DocHighlightFragment *f = &((*out_fragments)[num_fragments++]);
        f->text = (char *)malloc(fragment_size + 256);
        f->start_offset = text_tokens[match_positions[0]].start_offset;
        f->end_offset = text_tokens[match_positions[num_matches - 1]].end_offset;
        f->num_matches = num_matches;

        /* 构建高亮文本 */
        char *p = f->text;
        int pos = 0;

        /* 找到匹配位置周围的文本 */
        int frag_start = match_positions[0] > 3 ? match_positions[0] - 3 : 0;
        int frag_end = match_positions[num_matches - 1] + 4;
        if ((size_t)frag_end > num_text_tokens) frag_end = (int)num_text_tokens;

        for (int i = frag_start; i < frag_end && (size_t)pos < fragment_size; i++) {
            bool is_match = false;
            for (int m = 0; m < num_matches; m++) {
                if (match_positions[m] == i) {
                    is_match = true;
                    break;
                }
            }

            if (is_match) {
                pos += snprintf(p + pos, 256 - pos, "%s%s%s",
                              marker->pre_tag, text_tokens[i].text, marker->post_tag);
            } else {
                pos += snprintf(p + pos, 256 - pos, "%s", text_tokens[i].text);
                if (i < frag_end - 1) {
                    p[pos++] = ' ';
                }
            }
        }
        f->text[pos] = '\0';
    }

    free(match_positions);
    doc_tokenizer_free_tokens(query_tokens, num_query_tokens);
    doc_tokenizer_free_tokens(text_tokens, num_text_tokens);

    return num_fragments;
}

void doc_highlight_free(DocHighlightFragment *fragments, size_t count) {
    if (!fragments) return;
    for (size_t i = 0; i < count; i++) {
        free(fragments[i].text);
    }
    free(fragments);
}

/* ========================================================================
 * 全文检索器
 * ======================================================================== */

DocFtsSearcher *doc_fts_create(const DocFtsConfig *config) {
    DocFtsSearcher *searcher = (DocFtsSearcher *)calloc(1, sizeof(DocFtsSearcher));
    if (!searcher) return NULL;

    if (config) {
        searcher->config = *config;
    } else {
        searcher->config.tokenizer_type = DOC_TOKENIZER_STANDARD;
        searcher->config.enable_synonyms = false;
        searcher->config.enable_stopwords = true;
        searcher->config.min_match = 1;
    }

    searcher->tokenizer = doc_tokenizer_create(searcher->config.tokenizer_type,
                                               &searcher->config.tokenizer_config);
    if (!searcher->tokenizer) {
        free(searcher);
        return NULL;
    }

    if (searcher->config.enable_stopwords) {
        searcher->stopwords = doc_stopwords_create(false);
    }

    if (searcher->config.enable_synonyms) {
        searcher->synonyms = doc_synonyms_create();
    }

    return searcher;
}

void doc_fts_destroy(DocFtsSearcher *searcher) {
    if (!searcher) return;
    doc_tokenizer_destroy(searcher->tokenizer);
    doc_stopwords_destroy(searcher->stopwords);
    doc_synonyms_destroy(searcher->synonyms);
    free(searcher);
}

int doc_fts_set_stopwords(DocFtsSearcher *searcher, DocStopwords *stopwords) {
    if (!searcher) return -1;
    searcher->stopwords = stopwords;
    return 0;
}

int doc_fts_set_synonyms(DocFtsSearcher *searcher, DocSynonyms *synonyms) {
    if (!searcher) return -1;
    searcher->synonyms = synonyms;
    return 0;
}

size_t doc_fts_search(DocFtsSearcher *searcher,
                     const char *query,
                     const char **documents,
                     size_t num_docs,
                     DocPhraseResult **results,
                     size_t max_results) {
    if (!searcher || !query || !documents || !results) return 0;

    /* 简化实现：使用短语搜索 */
    return doc_phrase_search(documents, num_docs, query,
                            searcher->tokenizer, results, max_results);
}

/* ========================================================================
 * SQL 函数
 * ======================================================================== */

bool doc_sql_match(const char *text, const char *query) {
    if (!text || !query) return false;

    DocTokenizer *tokenizer = doc_tokenizer_create(DOC_TOKENIZER_STANDARD, NULL);

    DocToken *query_tokens = NULL;
    size_t num_query = 0;
    doc_tokenizer_tokenize(tokenizer, query, strlen(query), &query_tokens, &num_query);

    DocToken *text_tokens = NULL;
    size_t num_text = 0;
    doc_tokenizer_tokenize(tokenizer, text, strlen(text), &text_tokens, &num_text);

    bool match = false;
    for (size_t qi = 0; qi < num_query && !match; qi++) {
        for (size_t ti = 0; ti < num_text; ti++) {
            if (strcasecmp(text_tokens[ti].text, query_tokens[qi].text) == 0) {
                match = true;
                break;
            }
        }
    }

    doc_tokenizer_free_tokens(query_tokens, num_query);
    doc_tokenizer_free_tokens(text_tokens, num_text);
    doc_tokenizer_destroy(tokenizer);

    return match;
}

char *doc_sql_highlight(const char *text, const char *query) {
    if (!text || !query) return strdup("");

    DocTokenizer *tokenizer = doc_tokenizer_create(DOC_TOKENIZER_STANDARD, NULL);

    DocHighlightFragment *fragments = NULL;
    size_t num_fragments = doc_highlight(text, query, tokenizer,
                                        &DOC_HIGHLIGHT_DEFAULT,
                                        1, 256, &fragments);

    char *result = NULL;
    if (num_fragments > 0 && fragments[0].text) {
        result = strdup(fragments[0].text);
    } else {
        result = strdup(text);
    }

    doc_highlight_free(fragments, num_fragments);
    doc_tokenizer_destroy(tokenizer);

    return result;
}
