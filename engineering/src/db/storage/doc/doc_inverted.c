/**
 * @file doc_inverted.c
 * @brief 文档倒排索引实现
 */
#define _POSIX_C_SOURCE 200809L
#include "db/storage/doc/doc_inverted.h"
#include "db/core/log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir(path) _mkdir(path)
#endif

/* strndup polyfill for MinGW */
#if defined(_WIN32) || !defined(_GNU_SOURCE)
static char *doc_strndup(const char *s, size_t n) {
    const char *end = memchr(s, '\0', n);
    size_t len = end ? (size_t)(end - s) : n;
    char *dup = (char *)malloc(len + 1);
    if (dup) {
        memcpy(dup, s, len);
        dup[len] = '\0';
    }
    return dup;
}
#ifndef strndup
#define strndup doc_strndup
#endif
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
    char postings_path[512];
    char docs_path[512];

    snprintf(meta_path, sizeof(meta_path), "%s/meta.bin", data_dir);
    snprintf(postings_path, sizeof(postings_path), "%s/postings.bin", data_dir);
    snprintf(docs_path, sizeof(docs_path), "%s/docs.bin", data_dir);

    /* 检查 meta.bin 是否存在 */
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

    /* 读取版本 */
    uint32_t version;
    if (fread(&version, sizeof(version), 1, fp) != 1 || version != DOC_INVERTED_VERSION) {
        fclose(fp);
        return doc_inverted_create(data_dir, NULL);
    }

    /* 分配索引结构 */
    doc_inverted_index_t *index = (doc_inverted_index_t *)calloc(
        1, sizeof(doc_inverted_index_t));
    if (index == NULL) {
        fclose(fp);
        return NULL;
    }

    strncpy(index->data_dir, data_dir, sizeof(index->data_dir) - 1);
    strncpy(index->tokenizer, DOC_INVERTED_DEFAULT_TOKENIZER,
            sizeof(index->tokenizer) - 1);

    /* 读取术语数量 */
    if (fread(&index->term_count, sizeof(index->term_count), 1, fp) != 1) {
        fclose(fp);
        free(index);
        return doc_inverted_create(data_dir, NULL);
    }

    /* 读取文档数量 */
    if (fread(&index->doc_count, sizeof(index->doc_count), 1, fp) != 1) {
        fclose(fp);
        free(index);
        return doc_inverted_create(data_dir, NULL);
    }

    /* 读取术语数组 */
    index->term_capacity = index->term_count > 0 ? index->term_count * 2 : 1024;
    index->terms = (doc_term_info_t *)calloc(index->term_capacity,
                                             sizeof(doc_term_info_t));
    if (index->terms == NULL) {
        fclose(fp);
        free(index);
        return NULL;
    }

    if (index->term_count > 0) {
        if (fread(index->terms, sizeof(doc_term_info_t), index->term_count, fp)
                != index->term_count) {
            fclose(fp);
            free(index->terms);
            free(index);
            return doc_inverted_create(data_dir, NULL);
        }
    }

    /* 读取 doc_id -> offset 映射数量（暂存用于构建映射） */
    uint64_t doc_map_count;
    if (fread(&doc_map_count, sizeof(doc_map_count), 1, fp) != 1) {
        fclose(fp);
        free(index->terms);
        free(index);
        return doc_inverted_create(data_dir, NULL);
    }

    fclose(fp);

    /* 打开 postings.bin 和 docs.bin */
    index->posting_file = fopen(postings_path, "r+b");
    index->doc_file = fopen(docs_path, "r+b");

    if (index->posting_file == NULL || index->doc_file == NULL) {
        if (index->posting_file) fclose(index->posting_file);
        if (index->doc_file) fclose(index->doc_file);
        free(index->terms);
        free(index);
        return NULL;
    }

    /* 初始化倒排列表数组（lazy - 在搜索时按需加载） */
    index->postings = (doc_inverted_list_t *)calloc(index->term_capacity,
                                                     sizeof(doc_inverted_list_t));
    if (index->postings == NULL) {
        fclose(index->posting_file);
        fclose(index->doc_file);
        free(index->terms);
        free(index);
        return NULL;
    }

    LOG_INFO("倒排索引加载成功: %s, terms=%u, docs=%lu",
             data_dir, index->term_count, index->doc_count);
    return index;
}

int doc_inverted_close(doc_inverted_index_t *index) {
    if (index == NULL) return -1;

    /* 保存元数据到 meta.bin */
    char meta_path[512];
    snprintf(meta_path, sizeof(meta_path), "%s/meta.bin", index->data_dir);

    FILE *fp = fopen(meta_path, "wb");
    if (fp != NULL) {
        /* 写入魔数 */
        uint32_t magic = DOC_INVERTED_MAGIC;
        uint32_t version = DOC_INVERTED_VERSION;
        fwrite(&magic, sizeof(magic), 1, fp);
        fwrite(&version, sizeof(version), 1, fp);

        /* 写入术语数量 */
        fwrite(&index->term_count, sizeof(index->term_count), 1, fp);

        /* 写入文档数量 */
        fwrite(&index->doc_count, sizeof(index->doc_count), 1, fp);

        /* 写入术语数组 */
        if (index->terms != NULL && index->term_count > 0) {
            fwrite(index->terms, sizeof(doc_term_info_t), index->term_count, fp);
        }

        /* 写入 doc_id -> offset 映射数量占位（后续扩展用） */
        uint64_t doc_map_count = index->doc_count;
        fwrite(&doc_map_count, sizeof(doc_map_count), 1, fp);

        fclose(fp);
    }

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
            if (index->postings[i].entries[j].doc_id_str) {
                free(index->postings[i].entries[j].doc_id_str);
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
    const char *doc_id_str;
    size_t doc_id_str_len;
    uint64_t doc_offset;
} tokenize_ctx_t;

static void tokenize_callback(const char *term, uint32_t pos, void *ctx) {
    tokenize_ctx_t *tctx = (tokenize_ctx_t *)ctx;
    doc_inverted_index_t *index = tctx->index;

    /* 查找或创建术语（使用精确字符串比较） */
    uint32_t term_id = UINT32_MAX;
    for (uint32_t i = 0; i < index->term_count; i++) {
        /* 使用哈希值匹配 */
        uint64_t term_hash = doc_term_hash(term);
        if (index->terms[i].term_id == (uint32_t)(term_hash % index->term_capacity)) {
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
        index->terms[term_id].term_id = (uint32_t)(doc_term_hash(term) % index->term_capacity);
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
    list->entries[idx].doc_id_str = tctx->doc_id_str != NULL ?
        strndup(tctx->doc_id_str, tctx->doc_id_str_len) : NULL;
    list->entries[idx].doc_offset = tctx->doc_offset;
    list->entries[idx].freq = 1;
    list->entries[idx].pos_count = 1;
    list->entries[idx].positions = (uint32_t *)malloc(sizeof(uint32_t));
    list->entries[idx].positions[0] = pos;

    /* 更新文档频率 */
    index->terms[term_id].doc_freq++;
}

int doc_inverted_add(doc_inverted_index_t *index, uint64_t doc_id,
                     const char *doc_id_str, size_t doc_id_str_len,
                     uint64_t doc_offset, const char *doc_content) {
    (void)doc_content;  /* doc_engine already wrote to docs.bin */
    if (index == NULL) return -1;

    tokenize_ctx_t ctx = {
        .index = index,
        .doc_id = doc_id,
        .term_id = 0,
        .doc_id_str = doc_id_str,
        .doc_id_str_len = doc_id_str_len,
        .doc_offset = doc_offset
    };

    /* 分词并索引（不重复写文件，doc_engine_tuple_insert 已写入 docs.bin） */
    /* 需要重新分词，所以需要 doc_content，但不再写文件 */
    char data_path[512];
    snprintf(data_path, sizeof(data_path), "%s/docs.bin", index->data_dir);
    FILE *fp = fopen(data_path, "rb");
    if (fp == NULL) return -1;

    /* 找到文档内容位置 */
    fseek(fp, (long)doc_offset + sizeof(uint32_t), SEEK_SET);  /* skip size */
    uint32_t id_len;
    fread(&id_len, sizeof(id_len), 1, fp);
    fseek(fp, id_len, SEEK_CUR);  /* skip doc_id */
    uint32_t json_len;
    fread(&json_len, sizeof(json_len), 1, fp);
    char *json_content = (char *)malloc(json_len + 1);
    if (json_content == NULL) {
        fclose(fp);
        return -1;
    }
    fread(json_content, 1, json_len, fp);
    json_content[json_len] = '\0';
    fclose(fp);

    /* 分词并索引 */
    doc_simple_tokenize(json_content, tokenize_callback, &ctx);
    free(json_content);

    index->doc_count++;
    LOG_DEBUG("文档索引完成: doc_id=%lu, content_len=%zu", doc_id, json_len);
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

    /* 解析查询词 */
    char *query_copy = strdup(query);
    if (query_copy == NULL) return 0;

    /* 收集查询词信息 */
    char *terms[64];
    uint32_t term_count = 0;
    char *saveptr;
    char *term = strtok_r(query_copy, " \t\n", &saveptr);
    while (term && term_count < 64) {
        terms[term_count++] = term;
        term = strtok_r(NULL, " \t\n", &saveptr);
    }
    if (term_count == 0) {
        free(query_copy);
        return 0;
    }

    /* 统计每个文档的词频和，并计算 IDF 缓存 */
    typedef struct {
        uint64_t doc_id;
        uint32_t total_tf;     /* 累计词频（用于长度归一化） */
        double bm25_score;     /* 累计 BM25 分数 */
    } doc_score_t;

    doc_score_t *doc_scores = (doc_score_t *)calloc(max_results, sizeof(doc_score_t));
    if (doc_scores == NULL) {
        free(query_copy);
        return 0;
    }

    uint32_t doc_count = 0;
    double N = (double)index->doc_count;
    if (N < 1) N = 1;

    /* BM25 参数 */
    double k1 = 1.2;
    double b = 0.75;

    /* 遍历每个查询词 */
    for (uint32_t ti = 0; ti < term_count; ti++) {
        const char *t = terms[ti];

        /* 查找术语 */
        uint32_t term_hash = (uint32_t)(doc_term_hash(t) % index->term_capacity);
        int32_t term_idx = -1;
        for (uint32_t i = 0; i < index->term_count; i++) {
            if (index->terms[i].term_id == term_hash) {
                term_idx = (int32_t)i;
                break;
            }
        }
        if (term_idx < 0) continue;

        uint32_t df = index->terms[term_idx].doc_freq;
        if (df == 0) df = 1;

        /* IDF: log(N / df) */
        double idf = log(N / (double)df);
        if (idf < 0) idf = 0;

        /* 遍历倒排列表中的文档 */
        doc_inverted_list_t *list = &index->postings[term_idx];
        for (uint32_t j = 0; j < list->count; j++) {
            doc_inverted_entry_t *entry = &list->entries[j];

            /* 查找或创建文档分数条目 */
            uint32_t di = 0;
            while (di < doc_count) {
                if (doc_scores[di].doc_id == entry->doc_id) break;
                di++;
            }
            if (di == doc_count && doc_count < max_results) {
                doc_scores[di].doc_id = entry->doc_id;
                doc_scores[di].total_tf = 0;
                doc_scores[di].bm25_score = 0;
                doc_count++;
            }
            if (di >= max_results) break;

            /* 累加词频 */
            doc_scores[di].total_tf += entry->freq;
        }
    }

    free(query_copy);

    /* 估算平均文档长度 */
    uint32_t total_tf_all = 0;
    for (uint32_t i = 0; i < doc_count; i++) {
        total_tf_all += doc_scores[i].total_tf;
    }
    double avgdl = (doc_count > 0) ? ((double)total_tf_all / doc_count) : 1;
    if (avgdl < 1) avgdl = 1;

    /* 对每个查询词重新遍历，计算每个文档的 BM25 分数并累加 */
    for (uint32_t ti = 0; ti < term_count; ti++) {
        const char *t = terms[ti];

        uint32_t term_hash = (uint32_t)(doc_term_hash(t) % index->term_capacity);
        int32_t term_idx = -1;
        for (uint32_t i = 0; i < index->term_count; i++) {
            if (index->terms[i].term_id == term_hash) {
                term_idx = (int32_t)i;
                break;
            }
        }
        if (term_idx < 0) continue;

        uint32_t df = index->terms[term_idx].doc_freq;
        if (df == 0) df = 1;

        double idf = log(N / (double)df);
        if (idf < 0) idf = 0;

        doc_inverted_list_t *list = &index->postings[term_idx];
        for (uint32_t j = 0; j < list->count; j++) {
            doc_inverted_entry_t *entry = &list->entries[j];

            /* 找到文档 */
            uint32_t di = 0;
            while (di < doc_count && doc_scores[di].doc_id != entry->doc_id) {
                di++;
            }
            if (di >= doc_count) continue;

            uint32_t tf = entry->freq;
            double doc_len = (double)doc_scores[di].total_tf;
            double len_norm = k1 * (1 - b + b * doc_len / avgdl);

            /* BM25: TF * IDF / (TF + len_norm) */
            double bm25_term = ((double)tf * idf) / ((double)tf + len_norm);
            doc_scores[di].bm25_score += bm25_term;
        }
    }

    /* 输出结果 */
    for (uint32_t i = 0; i < doc_count; i++) {
        results[i].doc_id = doc_scores[i].doc_id;
        results[i].score = doc_scores[i].bm25_score > 0 ? doc_scores[i].bm25_score : 0.1;
    }

    /* 按分数降序排序 */
    for (uint32_t i = 0; i < doc_count - 1; i++) {
        for (uint32_t j = i + 1; j < doc_count; j++) {
            if (results[j].score > results[i].score) {
                doc_inverted_result_t tmp = results[i];
                results[i] = results[j];
                results[j] = tmp;
            }
        }
    }

    free(doc_scores);
    return doc_count;
}

uint64_t doc_inverted_find(const doc_inverted_index_t *index,
                           const char *doc_id, size_t id_len) {
    if (index == NULL || doc_id == NULL || id_len == 0) return 0;

    /* 遍历所有术语的倒排列表，查找匹配的 doc_id_str */
    for (uint32_t i = 0; i < index->term_count; i++) {
        doc_inverted_list_t *list = &index->postings[i];
        for (uint32_t j = 0; j < list->count; j++) {
            doc_inverted_entry_t *entry = &list->entries[j];
            if (entry->doc_id_str != NULL &&
                strlen(entry->doc_id_str) == id_len &&
                memcmp(entry->doc_id_str, doc_id, id_len) == 0) {
                return entry->doc_offset;
            }
        }
    }
    return 0;
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
