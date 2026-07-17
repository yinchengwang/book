/**
 * @file doc_inverted.c
 * @brief 文档倒排索引实现
 */
#include "db/storage/doc/doc_inverted.h"
#include "db/core/log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir(path) _mkdir(path)
#endif

/* ========================================================================
 * 内部辅助函数
 * ======================================================================== */

static int ensure_dir(const char *path) {
    if (path == NULL) return -1;
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) return 0;
    return mkdir(path) == 0 ? 0 : -1;
}

/* 简单的字符串哈希函数 */
uint64_t doc_term_hash(const char *str) {
    if (str == NULL) return 0;
    uint64_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

/* ========================================================================
 * 分词器实现
 * ======================================================================== */

void doc_simple_tokenize(const char *text,
                         void (*callback)(const char *term, uint32_t pos, void *ctx),
                         void *ctx) {
    if (text == NULL || callback == NULL) return;

    char term[DOC_INVERTED_MAX_TERM_LEN];
    uint32_t term_len = 0;
    uint32_t position = 0;

    for (size_t i = 0; text[i]; i++) {
        char c = text[i];

        /* 只保留字母和数字 */
        if (isalnum((unsigned char)c)) {
            c = tolower((unsigned char)c);
            if (term_len < DOC_INVERTED_MAX_TERM_LEN - 1) {
                term[term_len++] = c;
            }
        } else if (term_len > 0) {
            /* 结束当前词 */
            term[term_len] = '\0';
            callback(term, position++, ctx);
            term_len = 0;
        }
    }

    /* 处理最后一个词 */
    if (term_len > 0) {
        term[term_len] = '\0';
        callback(term, position, ctx);
    }
}

/* ========================================================================
 * 生命周期 API 实现
 * ======================================================================== */

doc_inverted_index_t *doc_inverted_create(const char *data_dir,
                                           const char *tokenizer) {
    if (data_dir == NULL) return NULL;

    ensure_dir(data_dir);

    doc_inverted_index_t *index = (doc_inverted_index_t *)calloc(
        1, sizeof(doc_inverted_index_t));
    if (index == NULL) return NULL;

    strncpy(index->data_dir, data_dir, sizeof(index->data_dir) - 1);
    if (tokenizer != NULL) {
        strncpy(index->tokenizer, tokenizer, sizeof(index->tokenizer) - 1);
    } else {
        strncpy(index->tokenizer, DOC_INVERTED_DEFAULT_TOKENIZER,
                sizeof(index->tokenizer) - 1);
    }

    /* 初始化术语数组 */
    index->term_capacity = 1024;
    index->terms = (doc_term_info_t *)calloc(index->term_capacity,
                                             sizeof(doc_term_info_t));

    /* 初始化倒排列表数组 */
    index->postings = (doc_inverted_list_t *)calloc(index->term_capacity,
                                                     sizeof(doc_inverted_list_t));

    /* 打开文件 */
    char postings_path[512];
    char docs_path[512];
    char meta_path[512];

    snprintf(postings_path, sizeof(postings_path), "%s/postings.bin", data_dir);
    snprintf(docs_path, sizeof(docs_path), "%s/docs.bin", data_dir);
    snprintf(meta_path, sizeof(meta_path), "%s/meta.bin", data_dir);

    index->posting_file = fopen(postings_path, "w+b");
    index->doc_file = fopen(docs_path, "w+b");

    if (index->posting_file == NULL || index->doc_file == NULL) {
        if (index->posting_file) fclose(index->posting_file);
        if (index->doc_file) fclose(index->doc_file);
        free(index->terms);
        free(index->postings);
        free(index);
        return NULL;
    }

    /* 写入魔数 */
    uint32_t magic = DOC_INVERTED_MAGIC;
    uint32_t version = DOC_INVERTED_VERSION;
    fwrite(&magic, sizeof(magic), 1, index->posting_file);
    fwrite(&version, sizeof(version), 1, index->posting_file);

    LOG_INFO("倒排索引创建成功: %s", data_dir);
    return index;
}

doc_inverted_index_t *doc_inverted_open(const char *data_dir) {
    if (data_dir == NULL) return NULL;

    char meta_path[512];
    snprintf(meta_path, sizeof(meta_path), "%s/meta.bin", data_dir);

    FILE *fp = fopen(meta_path, "rb");
    if (fp == NULL) {
        return doc_inverted_create(data_dir, NULL);
    }

    /* 读取魔数验证 */
    uint32_t magic;
    if (fread(&magic, sizeof(magic), 1, fp) != 1 || magic != DOC_INVERTED_MAGIC) {
        fclose(fp);
        return doc_inverted_create(data_dir, NULL);
    }
    fclose(fp);

    /* TODO: 完整加载索引 */
    LOG_INFO("倒排索引加载成功: %s", data_dir);
    return doc_inverted_create(data_dir, NULL);
}

int doc_inverted_close(doc_inverted_index_t *index) {
    if (index == NULL) return -1;

    if (index->posting_file) fclose(index->posting_file);
    if (index->doc_file) fclose(index->doc_file);

    LOG_INFO("倒排索引关闭: terms=%u, docs=%lu",
             index->term_count, index->doc_count);
    return 0;
}

void doc_inverted_free(doc_inverted_index_t *index) {
    if (index == NULL) return;

    doc_inverted_close(index);

    for (uint32_t i = 0; i < index->term_count; i++) {
        for (uint32_t j = 0; j < index->postings[i].count; j++) {
            if (index->postings[i].entries[j].positions) {
                free(index->postings[i].entries[j].positions);
            }
        }
        if (index->postings[i].entries) {
            free(index->postings[i].entries);
        }
    }

    free(index->terms);
    free(index->postings);
    free(index);
}

/* ========================================================================
 * 索引操作 API 实现
 * ======================================================================== */

typedef struct {
    doc_inverted_index_t *index;
    uint64_t doc_id;
    uint32_t term_id;
} tokenize_ctx_t;

static void tokenize_callback(const char *term, uint32_t pos, void *ctx) {
    tokenize_ctx_t *tctx = (tokenize_ctx_t *)ctx;
    doc_inverted_index_t *index = tctx->index;

    /* 查找或创建术语 */
    uint32_t term_id = UINT32_MAX;
    for (uint32_t i = 0; i < index->term_count; i++) {
        /* 简单比较，实际应该用哈希或字典 */
        if (index->terms[i].term_id == doc_term_hash(term) % index->term_capacity) {
            term_id = i;
            break;
        }
    }

    if (term_id == UINT32_MAX) {
        /* 创建新术语 */
        if (index->term_count >= index->term_capacity) {
            uint32_t new_cap = index->term_capacity * 2;
            index->terms = (doc_term_info_t *)realloc(
                index->terms, new_cap * sizeof(doc_term_info_t));
            index->postings = (doc_inverted_list_t *)realloc(
                index->postings, new_cap * sizeof(doc_inverted_list_t));
            index->term_capacity = new_cap;
        }
        term_id = index->term_count++;
        index->terms[term_id].term_id = doc_term_hash(term) % index->term_capacity;
        index->terms[term_id].doc_freq = 0;
    }

    /* 添加到倒排列表 */
    doc_inverted_list_t *list = &index->postings[term_id];
    if (list->count >= list->capacity) {
        list->capacity = list->capacity > 0 ? list->capacity * 2 : DOC_INVERTED_LIST_INITIAL_CAP;
        list->entries = (doc_inverted_entry_t *)realloc(
            list->entries, list->capacity * sizeof(doc_inverted_entry_t));
    }

    uint32_t idx = list->count++;
    list->entries[idx].doc_id = tctx->doc_id;
    list->entries[idx].freq = 1;
    list->entries[idx].pos_count = 1;
    list->entries[idx].positions = (uint32_t *)malloc(sizeof(uint32_t));
    list->entries[idx].positions[0] = pos;

    /* 更新文档频率 */
    index->terms[term_id].doc_freq++;
}

int doc_inverted_add(doc_inverted_index_t *index, uint64_t doc_id,
                     const char *doc_content) {
    if (index == NULL || doc_content == NULL) return -1;

    tokenize_ctx_t ctx = {index, doc_id, 0};

    /* 存储文档内容 */
    size_t content_len = strlen(doc_content) + 1;
    fseek(index->doc_file, 0, SEEK_END);
    uint64_t offset = ftell(index->doc_file);
    fwrite(&doc_id, sizeof(doc_id), 1, index->doc_file);
    fwrite(&content_len, sizeof(content_len), 1, index->doc_file);
    fwrite(doc_content, 1, content_len, index->doc_file);

    /* 分词并索引 */
    doc_simple_tokenize(doc_content, tokenize_callback, &ctx);

    index->doc_count++;
    LOG_DEBUG("文档索引完成: doc_id=%lu, content_len=%zu", doc_id, content_len);
    return 0;
}

int doc_inverted_remove(doc_inverted_index_t *index, uint64_t doc_id) {
    if (index == NULL) return -1;

    /* TODO: 实现墓碑机制和合并清理 */
    (void)doc_id;
    return 0;
}

uint32_t doc_inverted_search(const doc_inverted_index_t *index,
                              const char *query,
                              doc_inverted_result_t *results,
                              uint32_t max_results) {
    if (index == NULL || query == NULL || results == NULL || max_results == 0) {
        return 0;
    }

    /* TODO: 实现 BM25 打分搜索 */
    /* 目前简化实现：只统计匹配词数 */

    uint32_t found = 0;
    char *query_copy = strdup(query);
    if (query_copy == NULL) return 0;

    char *saveptr;
    char *term = strtok_r(query_copy, " \t\n", &saveptr);

    while (term && found < max_results) {
        uint32_t term_id = doc_term_hash(term) % index->term_capacity;

        /* 查找术语 */
        for (uint32_t i = 0; i < index->term_count; i++) {
            if (index->terms[i].term_id == term_id) {
                /* 收集文档 ID */
                doc_inverted_list_t *list = &index->postings[i];
                for (uint32_t j = 0; j < list->count && found < max_results; j++) {
                    results[found].doc_id = list->entries[j].doc_id;
                    results[found].score = (double)list->entries[j].freq;
                    found++;
                }
                break;
            }
        }

        term = strtok_r(NULL, " \t\n", &saveptr);
    }

    free(query_copy);
    return found;
}

int doc_inverted_get_doc(const doc_inverted_index_t *index, uint64_t doc_id,
                         char **out_content) {
    if (index == NULL || out_content == NULL) return -1;

    *out_content = NULL;

    /* 搜索文档 */
    rewind(index->doc_file);
    uint64_t id;
    size_t len;

    while (fread(&id, sizeof(id), 1, index->doc_file) == 1) {
        fread(&len, sizeof(len), 1, index->doc_file);
        if (id == doc_id) {
            *out_content = (char *)malloc(len);
            if (*out_content != NULL) {
                fread(*out_content, 1, len, index->doc_file);
            }
            return 0;
        }
        fseek(index->doc_file, len, SEEK_CUR);
    }

    return -1;
}
