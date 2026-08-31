/**
 * @file rdf_engine.c
 * @brief RDF 知识图谱引擎实现
 *
 * 实现 Triple Store 和基本 SPARQL 查询能力。
 */
#include "db/rdf_engine.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <direct.h>
#include <errno.h>
#include <windows.h>
#include <fileapi.h>
#define mkdir(path) _mkdir(path)
#else
#include <sys/stat.h>
#include <unistd.h>
#include <ftw.h>
#endif

#define RDF_ENGINE_NAME "rdf_engine"
#define RDF_DATA_PREFIX "rdf_"
#define MAX_TRIPLES_PER_PAGE 1024

/* ========================================================================
 * 内部数据结构
 * ======================================================================== */

/** 三元组索引键 */
typedef struct triple_key_s {
    uint64_t subject_hash;
    uint64_t predicate_hash;
    uint64_t object_hash;
} triple_key_t;

/** 三元组存储节点 */
typedef struct triple_node_s {
    rdf_triple_t triple;
    struct triple_node_s *next;
} triple_node_t;

/** RDF 图全局状态 */
typedef struct rdf_engine_global_s {
    char data_dir[512];
    bool initialized;
} rdf_engine_global_t;

static rdf_engine_global_t g_rdf_engine = {
    .data_dir = {0},
    .initialized = false
};

/** RDF 图头 */
typedef struct rdf_header_s {
    char name[256];
    uint64_t num_triples;
    uint64_t num_subjects;
    uint64_t num_predicates;
    uint64_t num_objects;
} rdf_header_t;

/** RDF 数据库句柄 */
typedef struct rdf_db_s {
    char name[256];
    char data_dir[512];
    AccessMode mode;
    uint64_t num_triples;

    /* 内存索引 */
    triple_node_t *triple_list;     /**< 三元组链表 */
    void *subject_index;            /**< 主语索引（哈希表） */
    void *predicate_index;          /**< 谓语索引 */
    void *object_index;             /**< 宾语索引 */
} rdf_db_t;

/* ========================================================================
 * 哈希函数
 * ======================================================================== */

static uint64_t hash_string(const char *str) {
    uint64_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

static uint64_t hash_term(const rdf_term_t *term) {
    uint64_t h = hash_string((char *)(uintptr_t)(unsigned long)term->type);
    h = h * 31 + hash_string(term->value);
    h = h * 31 + hash_string(term->lang);
    h = h * 31 + hash_string(term->datatype);
    return h;
}

/* ========================================================================
 * 工具函数
 * ======================================================================== */

static int get_dir_path(const char *name, char *path, size_t path_size) {
    snprintf(path, path_size, "%s/%s%s",
             g_rdf_engine.data_dir, RDF_DATA_PREFIX, name);
    return 0;
}

rdf_term_t rdf_term_uri(const char *uri) {
    rdf_term_t term = {RDF_URI, {0}, {0}, {0}};
    strncpy(term.value, uri, sizeof(term.value) - 1);
    return term;
}

rdf_term_t rdf_term_blank(const char *id) {
    rdf_term_t term = {RDF_BLANK, {0}, {0}, {0}};
    strncpy(term.value, id, sizeof(term.value) - 1);
    return term;
}

rdf_term_t rdf_term_literal(const char *value, const char *lang,
                             const char *datatype) {
    rdf_term_t term = {RDF_LITERAL, {0}, {0}, {0}};
    strncpy(term.value, value, sizeof(term.value) - 1);
    if (lang) strncpy(term.lang, lang, sizeof(term.lang) - 1);
    if (datatype) strncpy(term.datatype, datatype, sizeof(term.datatype) - 1);
    return term;
}

bool rdf_term_equals(const rdf_term_t *a, const rdf_term_t *b) {
    if (a->type != b->type) return false;
    if (strcmp(a->value, b->value) != 0) return false;
    if (a->type == RDF_LITERAL) {
        if (strcmp(a->lang, b->lang) != 0) return false;
        if (strcmp(a->datatype, b->datatype) != 0) return false;
    }
    return true;
}

bool rdf_term_matches(const rdf_term_t *term, const rdf_term_t *pattern) {
    /* 空值表示通配符 */
    if (!pattern || !pattern->value[0]) return true;
    return rdf_term_equals(term, pattern);
}

/* ========================================================================
 * 生命周期
 * ======================================================================== */

int rdf_engine_init(const char *data_dir) {
    if (g_rdf_engine.initialized) return 0;

    if (data_dir) {
        strncpy(g_rdf_engine.data_dir, data_dir, sizeof(g_rdf_engine.data_dir) - 1);
    } else {
        strcpy(g_rdf_engine.data_dir, "./data/rdf");
    }

#ifdef _WIN32
    mkdir(g_rdf_engine.data_dir);
#else
    mkdir(g_rdf_engine.data_dir, 0755);
#endif

    g_rdf_engine.initialized = true;
    LOG_INFO("RDF engine initialized: %s", g_rdf_engine.data_dir);
    return 0;
}

int rdf_engine_shutdown(void) {
    g_rdf_engine.initialized = false;
    LOG_INFO("RDF engine shutdown");
    return 0;
}

/* ========================================================================
 * 表操作
 * ======================================================================== */

static int rdf_engine_table_create(const char *name, const storage_schema_t *schema) {
    (void)schema;
    if (!g_rdf_engine.initialized || !name) return -1;

    char dir_path[512];
    get_dir_path(name, dir_path, sizeof(dir_path));

#ifdef _WIN32
    if (mkdir(dir_path) != 0 && errno != EEXIST) {
#else
    if (mkdir(dir_path, 0755) != 0 && errno != EEXIST) {
#endif
        LOG_ERROR("创建 RDF 图目录失败: %s", dir_path);
        return -1;
    }

    char meta_path[512];
    snprintf(meta_path, sizeof(meta_path), "%s/header.bin", dir_path);

    rdf_header_t header = { .name = {0}, .num_triples = 0,
                            .num_subjects = 0, .num_predicates = 0, .num_objects = 0 };
    strncpy(header.name, name, sizeof(header.name) - 1);

    FILE *fp = fopen(meta_path, "wb");
    if (fp) {
        fwrite(&header, sizeof(header), 1, fp);
        fclose(fp);
    }

    LOG_INFO("RDF graph created: %s", name);
    return 0;
}

static void *rdf_engine_table_open(const char *name, AccessMode mode) {
    char dir_path[512];
    get_dir_path(name, dir_path, sizeof(dir_path));

    char meta_path[512];
    snprintf(meta_path, sizeof(meta_path), "%s/header.bin", dir_path);

    FILE *fp = fopen(meta_path, "rb");
    if (!fp) return NULL;

    rdf_header_t header;
    if (fread(&header, sizeof(header), 1, fp) != 1) {
        fclose(fp);
        return NULL;
    }
    fclose(fp);

    rdf_db_t *db = (rdf_db_t *)calloc(1, sizeof(rdf_db_t));
    if (!db) return NULL;

    strncpy(db->name, name, sizeof(db->name) - 1);
    get_dir_path(name, db->data_dir, sizeof(db->data_dir));
    db->mode = mode;
    db->num_triples = header.num_triples;
    db->triple_list = NULL;
    db->subject_index = NULL;
    db->predicate_index = NULL;
    db->object_index = NULL;

    /* 加载三元组到内存 */
    char data_path[512];
    snprintf(data_path, sizeof(data_path), "%s/triples.bin", db->data_dir);

    FILE *tfp = fopen(data_path, "rb");
    if (tfp) {
        fseek(tfp, 0, SEEK_END);
        long file_size = ftell(tfp);
        fseek(tfp, 0, SEEK_SET);

        size_t triple_count = file_size / sizeof(rdf_triple_t);
        for (size_t i = 0; i < triple_count; i++) {
            rdf_triple_t triple;
            if (fread(&triple, sizeof(triple), 1, tfp) == 1) {
                triple_node_t *node = (triple_node_t *)malloc(sizeof(triple_node_t));
                if (node) {
                    node->triple = triple;
                    node->next = db->triple_list;
                    db->triple_list = node;
                }
            }
        }
        fclose(tfp);
    }

    return db;
}

static int rdf_engine_table_close(void *rel) {
    if (!rel) return -1;
    rdf_db_t *db = (rdf_db_t *)rel;

    /* 保存头信息 */
    char meta_path[512];
    snprintf(meta_path, sizeof(meta_path), "%s/header.bin", db->data_dir);

    rdf_header_t header = {
        .num_triples = db->num_triples,
        .num_subjects = 0,
        .num_predicates = 0,
        .num_objects = 0
    };
    strncpy(header.name, db->name, sizeof(header.name) - 1);

    FILE *fp = fopen(meta_path, "wb");
    if (fp) {
        fwrite(&header, sizeof(header), 1, fp);
        fclose(fp);
    }

    /* 释放链表 */
    triple_node_t *node = db->triple_list;
    while (node) {
        triple_node_t *next = node->next;
        free(node);
        node = next;
    }

    free(db);
    return 0;
}

static int rdf_engine_table_drop(const char *name) {
    if (!name) return -1;

    char dir_path[512];
    get_dir_path(name, dir_path, sizeof(dir_path));

    /* 递归删除目录内容 */
#ifdef _WIN32
    char pattern[1024];
    snprintf(pattern, sizeof(pattern), "%s\\*", dir_path);

    WIN32_FIND_DATAW ffd;
    HANDLE hFind = FindFirstFileW((LPCWSTR)pattern, &ffd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (wcscmp(ffd.cFileName, L".") != 0 && wcscmp(ffd.cFileName, L"..") != 0) {
                char full_path[1024];
                snprintf(full_path, sizeof(full_path), "%s\\%ls", dir_path, ffd.cFileName);
                if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    /* 递归删除子目录 */
                    char sub_pattern[1024];
                    snprintf(sub_pattern, sizeof(sub_pattern), "%s\\*", full_path);
                    WIN32_FIND_DATAW sub_ffd;
                    HANDLE sub_hFind = FindFirstFileW((LPCWSTR)sub_pattern, &sub_ffd);
                    if (sub_hFind != INVALID_HANDLE_VALUE) {
                        do {
                            if (wcscmp(sub_ffd.cFileName, L".") != 0 && wcscmp(sub_ffd.cFileName, L"..") != 0) {
                                char sub_file[1024];
                                snprintf(sub_file, sizeof(sub_file), "%s\\%ls", full_path, sub_ffd.cFileName);
                                _unlink(sub_file);
                            }
                        } while (FindNextFileW(sub_hFind, &sub_ffd));
                        FindClose(sub_hFind);
                    }
                    _rmdir(full_path);
                } else {
                    _unlink(full_path);
                }
            }
        } while (FindNextFileW(hFind, &ffd));
        FindClose(hFind);
    }
    _rmdir(dir_path);
#else
    /* POSIX: use nftw for recursive delete */
    int (*unlink_cb)(const char *, const struct stat *, int, struct FTW *) = [](const char *path, const struct stat *sb, int typeflag, struct FTW *ftw) -> int {
        (void)sb; (void)typeflag; (void)ftw;
        return remove(path);
    };
    nftw(dir_path, unlink_cb, 16, FTW_DEPTH | FTW_PHYS);
#endif

    LOG_INFO("RDF graph dropped: %s", name);
    return 0;
}

static int rdf_engine_tuple_insert(void *rel, const void *data, size_t len) {
    if (!rel || !data) return -1;
    if (len < sizeof(rdf_triple_t)) return -1;

    rdf_db_t *db = (rdf_db_t *)rel;
    const rdf_triple_t *triple = (const rdf_triple_t *)data;

    /* 追加到文件 */
    char data_path[512];
    snprintf(data_path, sizeof(data_path), "%s/triples.bin", db->data_dir);

    FILE *fp = fopen(data_path, "ab");
    if (!fp) return -1;

    fwrite(triple, sizeof(rdf_triple_t), 1, fp);
    fclose(fp);

    /* 添加到内存链表 */
    triple_node_t *node = (triple_node_t *)malloc(sizeof(triple_node_t));
    if (node) {
        node->triple = *triple;
        node->next = db->triple_list;
        db->triple_list = node;
        db->num_triples++;
    }

    /* 激活索引 */
    rdf_index_add_triple(db->num_triples, triple);

    return 0;
}

static int rdf_engine_delete_triple(void *rel, const rdf_triple_t *triple) {
    if (!rel || !triple) return -1;
    rdf_db_t *db = (rdf_db_t *)rel;

    triple_node_t **prev = &db->triple_list;
    triple_node_t *node = db->triple_list;

    while (node) {
        if (rdf_term_equals(&node->triple.subject, &triple->subject) &&
            rdf_term_equals(&node->triple.predicate, &triple->predicate) &&
            rdf_term_equals(&node->triple.object, &triple->object)) {
            *prev = node->next;
            free(node);
            db->num_triples--;
            return 0;
        }
        prev = &node->next;
        node = node->next;
    }

    return -1;  /* 未找到 */
}

static int rdf_engine_get_stats(const char *name, storage_stats_t *stats) {
    if (!stats || !name) return -1;

    memset(stats, 0, sizeof(storage_stats_t));

    char dir_path[512];
    get_dir_path(name, dir_path, sizeof(dir_path));

    char meta_path[512];
    snprintf(meta_path, sizeof(meta_path), "%s/header.bin", dir_path);

    FILE *fp = fopen(meta_path, "rb");
    if (fp) {
        rdf_header_t header;
        if (fread(&header, sizeof(header), 1, fp) == 1) {
            stats->num_objects = header.num_triples;
        }
        fclose(fp);
    }

    return 0;
}

/* ========================================================================
 * 三元组查询
 * ======================================================================== */

int rdf_engine_match(void *rel,
                     const rdf_term_t *subject,
                     const rdf_term_t *predicate,
                     const rdf_term_t *object,
                     rdf_triple_t *results,
                     int32_t max_results,
                     int32_t *num_results) {
    if (!rel || !results || !num_results) return -1;

    rdf_db_t *db = (rdf_db_t *)rel;
    int32_t count = 0;

    triple_node_t *node = db->triple_list;
    while (node && count < max_results) {
        if (rdf_term_matches(&node->triple.subject, subject) &&
            rdf_term_matches(&node->triple.predicate, predicate) &&
            rdf_term_matches(&node->triple.object, object)) {
            results[count++] = node->triple;
        }
        node = node->next;
    }

    *num_results = count;
    return 0;
}

int rdf_engine_get_outgoing(void *rel, const rdf_term_t *subject,
                             rdf_triple_t *results, int32_t max_results,
                             int32_t *num_results) {
    return rdf_engine_match(rel, subject, NULL, NULL, results, max_results, num_results);
}

int rdf_engine_get_incoming(void *rel, const rdf_term_t *object,
                             rdf_triple_t *results, int32_t max_results,
                             int32_t *num_results) {
    return rdf_engine_match(rel, NULL, NULL, object, results, max_results, num_results);
}

/* ========================================================================
 * SPARQL 查询（简化实现）
 * ======================================================================== */

/** 解析 SPARQL SELECT 子句 */
static int parse_select_vars(const char *query, char vars[][64], int *var_count) {
    *var_count = 0;

    const char *select_pos = strstr(query, "SELECT");
    if (!select_pos) return -1;

    const char *where_pos = strstr(query, "WHERE");
    if (!where_pos) return -1;

    /* 提取变量名 */
    const char *p = select_pos + 6;
    while (p < where_pos && *var_count < 16) {
        while (*p == ' ' || *p == '\t' || *p == '\n') p++;

        if (*p == '?') {
            p++;
            char var[64] = {0};
            int i = 0;
            while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '(' && i < 63) {
                var[i++] = *p++;
            }
            if (var[0]) {
                strncpy(vars[*var_count], var, 63);
                (*var_count)++;
            }
        } else if (*p == '*') {
            /* TODO: 支持 * 表示所有变量 */
            break;
        } else {
            p++;
        }
    }

    return 0;
}

/** 解析三元组模式 */
static int parse_triple_pattern(const char *pattern,
                                 rdf_term_t *s, rdf_term_t *p, rdf_term_t *o) {
    /* 简单实现：解析 "subject predicate object ." 格式 */
    char buf[1024];
    char *tokens[4] = {0};
    int token_count = 0;

    strncpy(buf, pattern, sizeof(buf) - 1);

    char *saveptr = NULL;
    char *token = strtok_r(buf, " \t\n", &saveptr);
    while (token && token_count < 4) {
        /* 移除末尾的 . */
        size_t len = strlen(token);
        if (len > 0 && token[len - 1] == '.') {
            token[len - 1] = '\0';
        }
        if (strlen(token) > 0) {
            tokens[token_count++] = token;
        }
        token = strtok_r(NULL, " \t\n", &saveptr);
    }

    if (token_count < 3) return -1;

    /* 解析各部分 */
    if (tokens[0][0] == '?') {
        /* 变量占位 */
    } else if (tokens[0][0] == '<') {
        *s = rdf_term_uri(tokens[0] + 1);
    } else {
        *s = rdf_term_blank(tokens[0]);
    }

    if (tokens[1][0] == '<') {
        *p = rdf_term_uri(tokens[1] + 1);
    } else {
        *p = rdf_term_uri(tokens[1]);
    }

    if (tokens[2][0] == '?') {
        /* 变量占位 */
    } else if (tokens[2][0] == '"') {
        *o = rdf_term_literal(tokens[2] + 1, NULL, NULL);
    } else if (tokens[2][0] == '<') {
        *o = rdf_term_uri(tokens[2] + 1);
    } else {
        *o = rdf_term_blank(tokens[2]);
    }

    return 0;
}

int rdf_engine_sparql_select(void *rel, const char *sparql_query,
                              sparql_result_t *result) {
    if (!rel || !sparql_query || !result) return -1;

    memset(result, 0, sizeof(sparql_result_t));
    result->capacity = 1024;
    result->bindings = (sparql_binding_t *)calloc(result->capacity, sizeof(sparql_binding_t));

    rdf_db_t *db = (rdf_db_t *)rel;

    /* 解析 SELECT 变量 */
    char vars[16][64];
    int var_count = 0;
    if (parse_select_vars(sparql_query, vars, &var_count) != 0) {
        return -1;
    }

    /* 查找 WHERE 子句 */
    const char *where_pos = strstr(sparql_query, "WHERE");
    if (!where_pos) return -1;

    /* 提取三元组模式（简化实现） */
    const char *brace_pos = strchr(sparql_query, '{');
    const char *brace_end = brace_pos ? strchr(brace_pos, '}') : NULL;

    if (!brace_pos || !brace_end) {
        /* 尝试解析无花括号的模式 */
        const char *pattern_start = where_pos + 5;
        while (*pattern_start == ' ' || *pattern_start == '\t' || *pattern_start == '\n') {
            pattern_start++;
        }

        rdf_term_t s = {0}, pred = {0}, o = {0};
        if (parse_triple_pattern(pattern_start, &s, &pred, &o) == 0) {
            rdf_triple_t results[1024];
            int32_t count = 0;

            if (rdf_engine_match(rel, &s, &pred, &o, results, 1024, &count) == 0) {
                result->binding_count = var_count > 0 ? var_count : 3;
                result->row_count = count;

                for (int32_t i = 0; i < count && i < 1024; i++) {
                    for (int v = 0; v < result->binding_count && v < 3; v++) {
                        switch (v) {
                            case 0: result->bindings[i * result->binding_count + v].value = results[i].subject; break;
                            case 1: result->bindings[i * result->binding_count + v].value = results[i].predicate; break;
                            case 2: result->bindings[i * result->binding_count + v].value = results[i].object; break;
                        }
                        if (var_count > v) {
                            strncpy(result->bindings[i * result->binding_count + v].var_name,
                                   vars[v], 63);
                        }
                    }
                }
            }
        }
    }

    return 0;
}

bool rdf_engine_sparql_ask(void *rel, const char *sparql_query) {
    if (!rel || !sparql_query) return false;

    sparql_result_t result;
    if (rdf_engine_sparql_select(rel, sparql_query, &result) == 0) {
        bool exists = result.row_count > 0;
        rdf_engine_free_result(&result);
        return exists;
    }

    return false;
}

void rdf_engine_free_result(sparql_result_t *result) {
    if (!result) return;
    free(result->bindings);
    memset(result, 0, sizeof(sparql_result_t));
}

/* ========================================================================
 * storage_ops_t 适配层
 * ======================================================================== */

static storage_ops_t g_rdf_storage_ops = {
    .name = RDF_ENGINE_NAME,
    .model = MODEL_GRAPH,  /* 使用图模型 */
    .init = rdf_engine_init,
    .shutdown = rdf_engine_shutdown,
    .table_create = rdf_engine_table_create,
    .table_open = rdf_engine_table_open,
    .table_close = rdf_engine_table_close,
    .table_drop = rdf_engine_table_drop,
    .tuple_insert = rdf_engine_tuple_insert,
    .get_stats = rdf_engine_get_stats,
};

const storage_ops_t *rdf_engine_get_ops(void) {
    return &g_rdf_storage_ops;
}
