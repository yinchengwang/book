/**
 * @file doc_engine.c
 * @brief 文档存储引擎实现
 */
#include "db/storage/doc/doc_engine.h"
#include "db/storage/doc/jsonpath.h"
#include "db/storage/doc/doc_inverted.h"
#include "db/storage/doc/bm25.h"
#include "db/mm_pool.h"
#include "db/lock.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>

/* Windows/macOS/Linux 跨平台兼容 */
#ifdef _WIN32
    #include <direct.h>
    #include <errno.h>
    #define mkdir(path) _mkdir(path)
    #define rmdir(path) _rmdir(path)
#else
    #include <unistd.h>
#endif

#define DOC_ENGINE_NAME "doc_engine"
#define DOC_DATA_PREFIX "doc_"

typedef struct doc_engine_global_s {
    char data_dir[512];
    bool initialized;
} doc_engine_global_t;

static doc_engine_global_t g_doc_engine = {
    .data_dir = {0},
    .initialized = false
};

typedef struct doc_header_s {
    char name[256];
    uint64_t num_docs;
} doc_header_t;

static int get_dir_path(const char *name, char *path, size_t path_size) {
    snprintf(path, path_size, "%s/%s%s",
             g_doc_engine.data_dir, DOC_DATA_PREFIX, name);
    return 0;
}

static int doc_engine_table_create(const char *name, const storage_schema_t *schema) {
    (void)schema;
    if (name == NULL) return -1;

    char dir_path[512];
    get_dir_path(name, dir_path, sizeof(dir_path));

    /* 创建目录（兼容 Windows/macOS/Linux） */
#ifdef _WIN32
    if (mkdir(dir_path) != 0 && errno != EEXIST) {
#else
    if (mkdir(dir_path, 0755) != 0 && errno != EEXIST) {
#endif
        LOG_ERROR("创建文档目录失败: %s", dir_path);
        return -1;
    }

    char header_path[512];
    snprintf(header_path, sizeof(header_path), "%s/header.bin", dir_path);

    doc_header_t header = { .name = {0}, .num_docs = 0 };
    strncpy(header.name, name, sizeof(header.name) - 1);

    FILE *fp = fopen(header_path, "wb");
    if (fp) {
        fwrite(&header, sizeof(header), 1, fp);
        fclose(fp);
    }
    return 0;
}

static void *doc_engine_table_open(const char *name, AccessMode mode) {
    char dir_path[512];
    get_dir_path(name, dir_path, sizeof(dir_path));

    char header_path[512];
    snprintf(header_path, sizeof(header_path), "%s/header.bin", dir_path);

    FILE *fp = fopen(header_path, "rb");
    if (fp == NULL) return NULL;

    doc_header_t header;
    if (fread(&header, sizeof(header), 1, fp) != 1) {
        fclose(fp);
        return NULL;
    }
    fclose(fp);

    doc_engine_db_t *db = (doc_engine_db_t *)calloc(1, sizeof(doc_engine_db_t));
    if (db == NULL) return NULL;

    strncpy(db->name, name, sizeof(db->name) - 1);
    get_dir_path(name, db->data_dir, sizeof(db->data_dir));
    db->mode = mode;
    db->num_docs = header.num_docs;

    /* 初始化内存池（使用全局文档池） */
    db->mem_pool = g_doc_pool;
    db->use_mem_pool = (g_doc_pool != NULL);

    /* 初始化锁（默认禁用） */
    db->lockmgr = NULL;
    db->rwlock = NULL;
    db->use_lock = false;

    /* 初始化倒排索引 */
    db->use_inverted_index = true;
    db->inverted_index = doc_inverted_open(db->data_dir);
    if (db->inverted_index == NULL) {
        /* 尝试创建新的倒排索引 */
        db->inverted_index = doc_inverted_create(db->data_dir, NULL);
        if (db->inverted_index == NULL) {
            LOG_WARN("倒排索引创建失败");
            db->use_inverted_index = false;
        }
    }

    /* 初始化 BM25 评分器（默认禁用） */
    db->bm25_scorer = NULL;
    db->use_bm25 = false;

    return db;
}

static int doc_engine_table_close(void *rel) {
    if (rel == NULL) return -1;
    doc_engine_db_t *db = (doc_engine_db_t *)rel;

    /* 保存并关闭倒排索引 */
    if (db->use_inverted_index && db->inverted_index != NULL) {
        doc_inverted_close(db->inverted_index);
        db->inverted_index = NULL;
    }

    /* 释放 BM25 评分器 */
    if (db->bm25_scorer != NULL) {
        bm25_destroy(db->bm25_scorer);
        db->bm25_scorer = NULL;
    }

    char header_path[512];
    snprintf(header_path, sizeof(header_path), "%s/header.bin", db->data_dir);

    doc_header_t header = { .num_docs = db->num_docs };
    strncpy(header.name, db->name, sizeof(header.name) - 1);

    FILE *fp = fopen(header_path, "wb");
    if (fp) {
        fwrite(&header, sizeof(header), 1, fp);
        fclose(fp);
    }

    /* 释放读写锁 */
    if (db->rwlock != NULL) {
        free(db->rwlock);
        db->rwlock = NULL;
    }

    free(db);
    return 0;
}

static int doc_engine_table_drop(const char *name) {
    if (name == NULL) return -1;

    char dir_path[512];
    get_dir_path(name, dir_path, sizeof(dir_path));

    /* 删除数据文件 */
    char data_path[512];
    snprintf(data_path, sizeof(data_path), "%s/docs.bin", dir_path);
    remove(data_path);

    /* 删除索引文件（如果存在） */
    char index_path[512];
    snprintf(index_path, sizeof(index_path), "%s/index.bin", dir_path);
    remove(index_path);

    /* 删除元数据文件 */
    char header_path[512];
    snprintf(header_path, sizeof(header_path), "%s/header.bin", dir_path);
    remove(header_path);

    /* 删除目录 */
    return rmdir(dir_path) == 0 ? 0 : -1;
}

static int doc_engine_tuple_insert(void *rel, const void *data, size_t len) {
    if (rel == NULL || data == NULL) return -1;
    doc_engine_db_t *db = (doc_engine_db_t *)rel;
    if (len < sizeof(uint32_t) * 2) return -1;

    const uint8_t *ptr = (const uint8_t *)data;
    uint32_t id_len;
    memcpy(&id_len, ptr, sizeof(uint32_t));
    ptr += sizeof(uint32_t);

    if (id_len > len - sizeof(uint32_t) * 2) return -1;

    /* 提取文档 ID */
    uint64_t doc_id = db->num_docs;

    /* 提取 JSON 内容用于索引 */
    const uint8_t *json_ptr = ptr + id_len;
    uint32_t json_len;
    memcpy(&json_len, json_ptr - sizeof(uint32_t), sizeof(uint32_t));

    /* 写入文档文件 */
    char data_path[512];
    snprintf(data_path, sizeof(data_path), "%s/docs.bin", db->data_dir);

    FILE *fp = fopen(data_path, "ab");
    if (fp == NULL) {
        fp = fopen(data_path, "wb");
        if (fp == NULL) return -1;
    }

    uint32_t size = sizeof(uint32_t) * 2 + id_len + json_len;
    fwrite(&size, sizeof(uint32_t), 1, fp);
    fwrite(data, 1, len, fp);
    fclose(fp);

    /* 同时添加到倒排索引 */
    if (db->use_inverted_index && db->inverted_index != NULL) {
        const char *json_content = (const char *)json_ptr;
        doc_inverted_add(db->inverted_index, doc_id, json_content);
    }

    db->num_docs++;
    return 0;
}

static scan_desc_t *doc_engine_scan_begin(void *rel,
                                          const scan_key_t *keys, int nkeys,
                                          ScanDirection direction) {
    (void)rel;
    (void)keys;
    (void)nkeys;
    (void)direction;
    return NULL;
}

static int doc_engine_scan_next(scan_desc_t *scan, void *out_data, size_t *out_len) {
    (void)scan;
    (void)out_data;
    (void)out_len;
    return 1;
}

static int doc_engine_scan_end(scan_desc_t *scan) {
    if (scan) free(scan);
    return 0;
}

static int doc_engine_get_stats(const char *name, storage_stats_t *stats) {
    if (stats == NULL) return -1;
    memset(stats, 0, sizeof(storage_stats_t));
    if (name == NULL) return 0;

    char dir_path[512];
    get_dir_path(name, dir_path, sizeof(dir_path));

    char header_path[512];
    snprintf(header_path, sizeof(header_path), "%s/header.bin", dir_path);

    FILE *fp = fopen(header_path, "rb");
    if (fp == NULL) return -1;

    doc_header_t header;
    if (fread(&header, sizeof(header), 1, fp) != 1) {
        fclose(fp);
        return -1;
    }
    fclose(fp);

    stats->num_objects = header.num_docs;
    return 0;
}

static const storage_ops_t g_doc_engine_ops = {
    .name = DOC_ENGINE_NAME,
    .model = MODEL_DOCUMENT,
    .init = doc_engine_init,
    .shutdown = doc_engine_shutdown,
    .table_create = doc_engine_table_create,
    .table_open = doc_engine_table_open,
    .table_close = doc_engine_table_close,
    .table_drop = doc_engine_table_drop,
    .tuple_insert = doc_engine_tuple_insert,
    .tuple_update = NULL,
    .tuple_delete = NULL,
    .scan_begin = doc_engine_scan_begin,
    .scan_next = doc_engine_scan_next,
    .scan_end = doc_engine_scan_end,
    .index_create = NULL,
    .index_drop = NULL,
    .get_stats = doc_engine_get_stats,
};

int doc_engine_init(const char *data_dir) {
    if (g_doc_engine.initialized) return 0;
    if (data_dir == NULL) return -1;
    strncpy(g_doc_engine.data_dir, data_dir, sizeof(g_doc_engine.data_dir) - 1);
    g_doc_engine.initialized = true;
    return 0;
}

int doc_engine_shutdown(void) {
    g_doc_engine.initialized = false;
    return 0;
}

const storage_ops_t *doc_engine_get_ops(void) {
    return &g_doc_engine_ops;
}

int doc_engine_create(const char *name, const storage_schema_t *schema) {
    return doc_engine_table_create(name, schema);
}

void *doc_engine_open(const char *name, AccessMode mode) {
    return doc_engine_table_open(name, mode);
}

int doc_engine_close(void *rel) {
    return doc_engine_table_close(rel);
}

int doc_engine_drop(const char *name) {
    return doc_engine_table_drop(name);
}

int doc_engine_insert(void *rel, const void *data, size_t len) {
    return doc_engine_tuple_insert(rel, data, len);
}

int doc_engine_get(void *rel, const void *key, size_t key_len,
                   void **out_data, size_t *out_len) {
    (void)rel;
    (void)key;
    (void)key_len;
    (void)out_data;
    (void)out_len;
    return 1;
}

int doc_engine_stats(const char *name, storage_stats_t *stats) {
    return doc_engine_get_stats(name, stats);
}

/* ========================================================================
 * JSON 查询实现（简化版）
 * ======================================================================== */

/**
 * @brief 简单的 JSON 字段提取
 * @param json JSON 字符串
 * @param json_len JSON 长度
 * @param path JSONPath 路径（如 "$.name" 或 "$.data.items")
 * @param out_value 输出值
 * @param out_len 输出长度
 * @return 0 成功，1 未找到，-1 失败
 */
int doc_engine_query(void *rel,
                     const char *json_path,
                     const void *query_data, size_t query_len,
                     doc_query_result_t *results, uint32_t *num_results) {
    if (rel == NULL || json_path == NULL || results == NULL || num_results == NULL) {
        return -1;
    }

    doc_engine_db_t *db = (doc_engine_db_t *)rel;

    /* 初始化结果 */
    results->docs = calloc(100, sizeof(doc_query_doc_t));
    if (results->docs == NULL) {
        return -1;
    }
    results->count = 0;
    results->capacity = 100;

    char data_path[512];
    snprintf(data_path, sizeof(data_path), "%s/docs.bin", db->data_dir);

    FILE *fp = fopen(data_path, "rb");
    if (fp == NULL) {
        *num_results = 0;
        return 0;
    }

    /* 读取所有文档 */
    while (results->count < results->capacity) {
        uint32_t record_size;
        if (fread(&record_size, sizeof(record_size), 1, fp) != 1) {
            break;
        }

        uint8_t *record = (uint8_t *)malloc(record_size);
        if (record == NULL) break;

        if (fread(record, record_size, 1, fp) != 1) {
            free(record);
            break;
        }

        uint32_t offset = 0;

        /* 解析 ID */
        uint32_t id_len;
        memcpy(&id_len, record + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);

        char *doc_id = (char *)malloc(id_len + 1);
        if (doc_id) {
            memcpy(doc_id, record + offset, id_len);
            doc_id[id_len] = '\0';
        }
        offset += id_len;

        /* 解析 JSON */
        uint32_t json_len;
        memcpy(&json_len, record + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);

        char *json_str = (char *)malloc(json_len + 1);
        if (json_str) {
            memcpy(json_str, record + offset, json_len);
            json_str[json_len] = '\0';

            /* 简单的 JSON 字段匹配 */
            /* 支持格式: "field" 或 "$.field" */
            const char *field = json_path;
            if (field[0] == '$' && field[1] == '.') {
                field += 2;
            }

            /* 构造搜索模式: "\"field\":" */
            char pattern[256];
            snprintf(pattern, sizeof(pattern), "\"%s\"", field);

            /* 在 JSON 中查找字段 */
            char *field_pos = strstr(json_str, pattern);
            if (field_pos != NULL) {
                /* 提取字段值 */
                char *value_start = field_pos + strlen(pattern);
                while (*value_start == ' ' || *value_start == ':') value_start++;

                /* 找到值的结束位置 */
                char *value_end = value_start;
                bool in_string = false;
                int brace_count = 0;

                if (*value_start == '"') {
                    in_string = true;
                    value_start++;
                    value_end = value_start + 1;
                } else if (*value_start == '{') {
                    brace_count = 1;
                    value_end = value_start + 1;
                } else if (*value_start == '[') {
                    brace_count = 1;
                    value_end = value_start + 1;
                }

                while (*value_end && brace_count >= 0) {
                    if (in_string) {
                        if (*value_end == '"' && *(value_end - 1) != '\\') {
                            break;
                        }
                    } else {
                        if (*value_end == '{' || *value_end == '[') brace_count++;
                        if (*value_end == '}' || *value_end == ']') brace_count--;
                        if (brace_count == 0 && (*value_end == ',' || *value_end == '}' || *value_end == ']')) {
                            break;
                        }
                    }
                    value_end++;
                }

                size_t value_len = value_end - value_start;
                char *value = (char *)malloc(value_len + 1);
                if (value) {
                    memcpy(value, value_start, value_len);
                    value[value_len] = '\0';

                    uint32_t idx = results->count++;
                    results->docs[idx].doc_id = doc_id;
                    results->docs[idx].doc_id_len = id_len;
                    results->docs[idx].field_value = value;
                    results->docs[idx].field_value_len = value_len;
                } else {
                    free(doc_id);
                }
            } else {
                free(doc_id);
            }
            free(json_str);
        }

        free(record);
    }

    fclose(fp);
    *num_results = results->count;
    return 0;
}

void doc_engine_free_results(doc_query_result_t *results) {
    if (results && results->docs) {
        for (uint32_t i = 0; i < results->count; i++) {
            free(results->docs[i].doc_id);
            free(results->docs[i].field_value);
        }
        free(results->docs);
        results->docs = NULL;
        results->count = 0;
        results->capacity = 0;
    }
}

/* ========================================================================
 * JSONPath 查询实现
 * ======================================================================== */

int doc_engine_query_jsonpath(void *rel, const char *jsonpath,
                              doc_jsonpath_result_t **results,
                              uint32_t max_results) {
    if (!rel || !jsonpath || !results) return -1;

    doc_engine_db_t *db = (doc_engine_db_t *)rel;

    *results = calloc(max_results, sizeof(doc_jsonpath_result_t));
    if (!*results) return -1;

    uint32_t count = 0;

    char data_path[512];
    snprintf(data_path, sizeof(data_path), "%s/docs.bin", db->data_dir);

    FILE *fp = fopen(data_path, "rb");
    if (fp == NULL) {
        return 0;
    }

    /* 读取所有文档并查询 */
    while (count < max_results) {
        uint32_t record_size;
        if (fread(&record_size, sizeof(record_size), 1, fp) != 1) {
            break;
        }

        uint8_t *record = (uint8_t *)malloc(record_size);
        if (!record) break;

        if (fread(record, record_size, 1, fp) != 1) {
            free(record);
            break;
        }

        uint32_t offset = 0;

        /* 解析 ID */
        uint32_t id_len;
        memcpy(&id_len, record + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);

        char *doc_id = (char *)malloc(id_len + 1);
        if (doc_id) {
            memcpy(doc_id, record + offset, id_len);
            doc_id[id_len] = '\0';
        }
        offset += id_len;

        /* 解析 JSON */
        uint32_t json_len;
        memcpy(&json_len, record + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);

        char *json_str = (char *)malloc(json_len + 1);
        if (json_str) {
            memcpy(json_str, record + offset, json_len);
            json_str[json_len] = '\0';

            /* 使用 JSONPath 查询 */
            jsonpath_result_t jp_result;
            if (jsonpath_query(json_str, json_len, jsonpath, &jp_result) == 0) {
                for (size_t i = 0; i < jp_result.count && count < max_results; i++) {
                    jsonpath_value_t *v = &jp_result.values[i];

                    (*results)[count].doc_id = doc_id ? strdup(doc_id) : NULL;
                    (*results)[count].doc_id_len = doc_id ? id_len : 0;
                    (*results)[count].jsonpath = v->path ? strdup(v->path) : strdup(jsonpath);

                    if (v->type == JSONPATH_STRING) {
                        (*results)[count].value = v->value.str_val ? strdup(v->value.str_val) : strdup("null");
                        (*results)[count].value_len = v->value.str_val ? strlen(v->value.str_val) : 4;
                    } else if (v->type == JSONPATH_NUMBER) {
                        char buf[64];
                        snprintf(buf, sizeof(buf), "%g", v->value.num_val);
                        (*results)[count].value = strdup(buf);
                        (*results)[count].value_len = strlen(buf);
                    } else if (v->type == JSONPATH_BOOL) {
                        (*results)[count].value = strdup(v->value.bool_val ? "true" : "false");
                        (*results)[count].value_len = v->value.bool_val ? 4 : 5;
                    } else if (v->type == JSONPATH_OBJECT || v->type == JSONPATH_ARRAY) {
                        (*results)[count].value = v->value.str_val ? strdup(v->value.str_val) : strdup("null");
                        (*results)[count].value_len = v->value.str_val ? v->value_len : 4;
                    } else {
                        (*results)[count].value = strdup("null");
                        (*results)[count].value_len = 4;
                    }

                    count++;
                }
                jsonpath_free_result(&jp_result);
            }

            free(json_str);
        }

        free(record);
        free(doc_id);
    }

    fclose(fp);
    return (int)count;
}

void doc_engine_free_jsonpath_results(doc_jsonpath_result_t *results, uint32_t count) {
    if (!results) return;

    for (uint32_t i = 0; i < count; i++) {
        free(results[i].doc_id);
        free(results[i].jsonpath);
        free(results[i].value);
    }
    free(results);
}

/* ========================================================================
 * 倒排索引 API 实现
 * ======================================================================== */

int doc_engine_enable_inverted_index(void *rel, const char *tokenizer) {
    if (rel == NULL) return -1;
    doc_engine_db_t *db = (doc_engine_db_t *)rel;

    /* 如果已有索引，先释放 */
    if (db->inverted_index != NULL) {
        doc_inverted_free(db->inverted_index);
    }

    /* 创建新的倒排索引 */
    db->inverted_index = doc_inverted_create(db->data_dir, tokenizer);
    if (db->inverted_index == NULL) {
        LOG_ERROR("创建倒排索引失败");
        return -1;
    }

    db->use_inverted_index = true;
    LOG_INFO("倒排索引已启用");
    return 0;
}

uint32_t doc_engine_inverted_search(void *rel, const char *query,
                                    void *results, uint32_t max_results) {
    if (rel == NULL || query == NULL || results == NULL || max_results == 0) {
        return 0;
    }

    doc_engine_db_t *db = (doc_engine_db_t *)rel;
    if (!db->use_inverted_index || db->inverted_index == NULL) {
        return 0;
    }

    return doc_inverted_search(db->inverted_index, query,
                               (doc_inverted_result_t *)results, max_results);
}

int doc_engine_save_inverted_index(void *rel) {
    if (rel == NULL) return -1;
    doc_engine_db_t *db = (doc_engine_db_t *)rel;
    if (!db->use_inverted_index || db->inverted_index == NULL) {
        return -1;
    }

    return doc_inverted_close(db->inverted_index);
}

void doc_engine_disable_inverted_index(void *rel) {
    if (rel == NULL) return;
    doc_engine_db_t *db = (doc_engine_db_t *)rel;

    if (db->inverted_index != NULL) {
        doc_inverted_free(db->inverted_index);
        db->inverted_index = NULL;
    }

    db->use_inverted_index = false;
    LOG_INFO("倒排索引已禁用");
}

/* ========================================================================
 * 内存池管理 API 实现
 * ======================================================================== */

int doc_engine_enable_mem_pool(void *rel, bool use_pool) {
    if (rel == NULL) return -1;
    doc_engine_db_t *db = (doc_engine_db_t *)rel;

    if (use_pool && g_doc_pool != NULL) {
        db->mem_pool = g_doc_pool;
        db->use_mem_pool = true;
        LOG_INFO("文档引擎内存池已启用");
    } else {
        db->mem_pool = NULL;
        db->use_mem_pool = false;
        LOG_INFO("文档引擎内存池已禁用");
    }
    return 0;
}

int doc_engine_get_mem_pool_stats(void *rel, mm_pool_stats_t *stats) {
    if (rel == NULL || stats == NULL) return -1;
    doc_engine_db_t *db = (doc_engine_db_t *)rel;

    if (db->mem_pool != NULL) {
        return mm_pool_get_stats(db->mem_pool, stats);
    }

    memset(stats, 0, sizeof(mm_pool_stats_t));
    return 0;
}

/* ========================================================================
 * 并发锁控制 API 实现
 * ======================================================================== */

/* 全局锁管理器用于文档引擎 */
static lock_manager_t *g_doc_lockmgr = NULL;

/* 简单的读写锁实现（基于自旋锁） */
typedef struct {
    volatile int readers;
    volatile int writers_waiting;
    volatile int writer_active;
} doc_rwlock_t;

static void doc_rwlock_init(doc_rwlock_t *rwlock) {
    rwlock->readers = 0;
    rwlock->writers_waiting = 0;
    rwlock->writer_active = 0;
}

static void doc_rwlock_read_lock(doc_rwlock_t *rwlock) {
    while (1) {
        while (rwlock->writer_active) { /* 等待写锁 */; }
        __sync_fetch_and_add(&rwlock->readers, 1);
        if (!rwlock->writer_active) return;
        __sync_fetch_and_sub(&rwlock->readers, 1);
    }
}

static void doc_rwlock_read_unlock(doc_rwlock_t *rwlock) {
    __sync_fetch_and_sub(&rwlock->readers, 1);
}

static void doc_rwlock_write_lock(doc_rwlock_t *rwlock) {
    __sync_fetch_and_add(&rwlock->writers_waiting, 1);
    while (1) {
        while (rwlock->readers > 0) { /* 等待读锁 */; }
        if (__sync_bool_compare_and_swap(&rwlock->writer_active, 0, 1)) {
            __sync_fetch_and_sub(&rwlock->writers_waiting, 1);
            return;
        }
    }
}

static void doc_rwlock_write_unlock(doc_rwlock_t *rwlock) {
    __sync_fetch_and_and(&rwlock->writer_active, 0);
}

int doc_engine_enable_lock(void *rel, bool use_lock) {
    if (rel == NULL) return -1;
    doc_engine_db_t *db = (doc_engine_db_t *)rel;

    if (use_lock) {
        if (g_doc_lockmgr == NULL) {
            g_doc_lockmgr = lock_mgr_create();
            if (g_doc_lockmgr == NULL) {
                LOG_ERROR("创建文档引擎锁管理器失败");
                return -1;
            }
        }
        if (db->rwlock == NULL) {
            db->rwlock = calloc(1, sizeof(doc_rwlock_t));
            if (db->rwlock == NULL) return -1;
            doc_rwlock_init((doc_rwlock_t *)db->rwlock);
        }
        db->lockmgr = g_doc_lockmgr;
        db->use_lock = true;
        LOG_INFO("文档引擎并发锁已启用");
    } else {
        db->use_lock = false;
        db->lockmgr = NULL;
        if (db->rwlock != NULL) {
            free(db->rwlock);
            db->rwlock = NULL;
        }
        LOG_INFO("文档引擎并发锁已禁用");
    }
    return 0;
}

int doc_engine_read_lock(void *rel) {
    if (rel == NULL) return -1;
    doc_engine_db_t *db = (doc_engine_db_t *)rel;
    if (db->use_lock && db->rwlock != NULL) {
        doc_rwlock_read_lock((doc_rwlock_t *)db->rwlock);
    }
    return 0;
}

void doc_engine_read_unlock(void *rel) {
    if (rel == NULL) return;
    doc_engine_db_t *db = (doc_engine_db_t *)rel;
    if (db->use_lock && db->rwlock != NULL) {
        doc_rwlock_read_unlock((doc_rwlock_t *)db->rwlock);
    }
}

int doc_engine_write_lock(void *rel, uint32_t timeout_ms) {
    if (rel == NULL) return -1;
    doc_engine_db_t *db = (doc_engine_db_t *)rel;
    if (db->use_lock && db->rwlock != NULL) {
        doc_rwlock_write_lock((doc_rwlock_t *)db->rwlock);
    }
    (void)timeout_ms;
    return 0;
}

void doc_engine_write_unlock(void *rel) {
    if (rel == NULL) return;
    doc_engine_db_t *db = (doc_engine_db_t *)rel;
    if (db->use_lock && db->rwlock != NULL) {
        doc_rwlock_write_unlock((doc_rwlock_t *)db->rwlock);
    }
}

/* ========================================================================
 * BM25 打分 API 实现
 * ======================================================================== */

int doc_engine_enable_bm25(void *rel, double k1, double b) {
    if (rel == NULL) return -1;
    doc_engine_db_t *db = (doc_engine_db_t *)rel;

    /* 如果已有评分器，先释放 */
    if (db->bm25_scorer != NULL) {
        bm25_destroy(db->bm25_scorer);
    }

    /* 创建新的 BM25 评分器 */
    db->bm25_scorer = bm25_create(k1, b);
    if (db->bm25_scorer == NULL) {
        LOG_ERROR("创建 BM25 评分器失败");
        return -1;
    }

    db->use_bm25 = true;
    LOG_INFO("BM25 打分已启用: k1=%.2f, b=%.2f", k1, b);
    return 0;
}

uint32_t doc_engine_bm25_search(void *rel, const char *query,
                                 doc_inverted_result_t *results,
                                 uint32_t max_results) {
    if (rel == NULL || query == NULL || results == NULL || max_results == 0) {
        return 0;
    }

    doc_engine_db_t *db = (doc_engine_db_t *)rel;
    if (!db->use_bm25 || db->bm25_scorer == NULL) {
        /* 回退到普通倒排索引搜索 */
        return doc_engine_inverted_search(rel, query, results, max_results);
    }

    /* 先获取倒排索引结果 */
    uint32_t count = doc_inverted_search(db->inverted_index, query,
                                          results, max_results);

    /* 对结果进行 BM25 重排序 */
    for (uint32_t i = 0; i < count; i++) {
        /* 获取文档长度（简化实现，实际应从索引获取） */
        uint32_t doc_len = results[i].snippet ? strlen(results[i].snippet) : 100;
        uint32_t avg_doc_len = db->num_docs > 0 ?
            (uint32_t)(db->num_docs * 50) : 100;  /* 估算平均长度 */

        /* 设置索引信息 */
        bm25_set_index_info(db->bm25_scorer, db->num_docs, avg_doc_len);

        /* 计算 BM25 分数（简化实现） */
        double score = bm25_score(db->bm25_scorer, doc_len / 5,  /* 词数估算 */
                                  NULL, NULL, 0);
        results[i].score = score > 0 ? score : 0.1;
    }

    /* 按分数降序排序 */
    for (uint32_t i = 0; i < count - 1; i++) {
        for (uint32_t j = i + 1; j < count; j++) {
            if (results[j].score > results[i].score) {
                doc_inverted_result_t tmp = results[i];
                results[i] = results[j];
                results[j] = tmp;
            }
        }
    }

    LOG_DEBUG("BM25 搜索完成: query=%s, count=%u", query, count);
    return count;
}

void doc_engine_set_bm25_index_info(void *rel, uint64_t num_docs,
                                     uint32_t avg_doc_len) {
    if (rel == NULL) return;
    doc_engine_db_t *db = (doc_engine_db_t *)rel;

    if (db->bm25_scorer != NULL) {
        bm25_set_index_info(db->bm25_scorer, num_docs, avg_doc_len);
    }
}

void doc_engine_disable_bm25(void *rel) {
    if (rel == NULL) return;
    doc_engine_db_t *db = (doc_engine_db_t *)rel;

    if (db->bm25_scorer != NULL) {
        bm25_destroy(db->bm25_scorer);
        db->bm25_scorer = NULL;
    }

    db->use_bm25 = false;
    LOG_INFO("BM25 打分已禁用");
}
