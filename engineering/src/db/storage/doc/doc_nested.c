/**
 * @file doc_nested.c
 * @brief 嵌套文档存储实现
 */

#include "db/storage/doc/doc_nested.h"
#include "db/storage/doc/jsonpath.h"
#include "db/mm_pool.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

/* ========================================================================
 * 常量
 * ======================================================================== */

#define MAX_PATH_DEPTH 32
#define DEFAULT_NODE_CAPACITY 16

/* ========================================================================
 * 工具函数
 * ======================================================================== */

static const char *skip_ws(const char *p) {
    while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
    return p;
}

static size_t unescaped_len(const char *start, const char *end) {
    size_t len = 0;
    for (const char *p = start; p < end && *p; p++) {
        if (*p == '\\' && p + 1 < end) p++;
        len++;
    }
    return len;
}

static char *copy_unescaped(const char *start, const char *end, size_t *out_len) {
    if (*start != '"') return NULL;
    start++;
    size_t alloc_len = unescaped_len(start, end);
    char *result = (char *)malloc(alloc_len + 1);
    if (!result) return NULL;

    size_t i = 0;
    for (const char *p = start; p < end && *p && *p != '"'; p++) {
        if (*p == '\\' && p + 1 < end) p++;
        result[i++] = *p;
    }
    result[i] = '\0';
    if (out_len) *out_len = i;
    return result;
}

static void skip_value(const char **p, size_t *remaining) {
    *p = skip_ws(*p);
    if (*remaining == 0) return;

    if (**p == '"') {
        (*p)++; (*remaining)--;
        while (*remaining > 0 && **p && **p != '"') {
            if (**p == '\\' && *remaining > 1) { (*p)++; (*remaining)--; }
            (*p)++; (*remaining)--;
        }
        if (*remaining > 0 && **p == '"') { (*p)++; (*remaining)--; }
    } else if (**p == '{') {
        int depth = 1; (*p)++; (*remaining)--;
        while (*remaining > 0 && depth > 0) {
            if (**p == '{' || **p == '[') depth++;
            else if (**p == '}' || **p == ']') depth--;
            (*p)++; (*remaining)--;
        }
    } else if (**p == '[') {
        int depth = 1; (*p)++; (*remaining)--;
        while (*remaining > 0 && depth > 0) {
            if (**p == '{' || **p == '[') depth++;
            else if (**p == '}' || **p == ']') depth--;
            (*p)++; (*remaining)--;
        }
    } else {
        while (*remaining > 0 && **p && **p != ',' && **p != ']' && **p != '}') {
            (*p)++; (*remaining)--;
        }
    }
}

static const char *extract_value_range(const char *p, size_t *remaining, size_t *out_len) {
    const char *start = p;
    p = skip_ws(p);

    if (*p == '"') {
        p++; if (*remaining > 0) (*remaining)--;
        while (*remaining > 0 && *p && *p != '"') {
            if (*p == '\\' && *remaining > 1) { p++; (*remaining)--; }
            p++; if (*remaining > 0) (*remaining)--;
        }
        if (*remaining > 0 && *p == '"') { p++; (*remaining)--; }
        *out_len = p - start;
    } else if (*p == '{' || *p == '[') {
        char end_char = (*p == '{') ? '}' : ']';
        int depth = 1; p++; if (*remaining > 0) (*remaining)--;
        while (*remaining > 0 && depth > 0) {
            if (*p == '{' || *p == '[') depth++;
            else if (*p == end_char) depth--;
            p++; if (*remaining > 0) (*remaining)--;
        }
        *out_len = p - start;
    } else {
        while (*remaining > 0 && *p && *p != ',' && *p != ']' && *p != '}') {
            p++; if (*remaining > 0) (*remaining)--;
        }
        *out_len = p - start;
    }
    return start;
}

/* ========================================================================
 * 嵌套节点创建
 * ======================================================================== */

static DocNestedNode *create_node(DocNestedParser *parser, DocNestedType type,
                                  const char *key, int index) {
    DocNestedNode *node = (DocNestedNode *)calloc(1, sizeof(DocNestedNode));
    if (!node) return NULL;

    node->type = type;
    node->key = key ? strdup(key) : NULL;
    node->index = index;
    node->parent = NULL;
    node->num_children = 0;

    if (key) {
        size_t path_len = 64;
        node->path = (char *)malloc(path_len);
        if (node->parent && node->parent->path) {
            if (index >= 0) {
                snprintf(node->path, path_len, "%s[%d]", node->parent->path, index);
            } else {
                snprintf(node->path, path_len, "%s.%s", node->parent->path, key);
            }
        } else {
            snprintf(node->path, path_len, "$%s%s",
                     index >= 0 ? "[" : ".",
                     index >= 0 ? "" : key);
        }
    } else {
        node->path = strdup("$");
    }

    (void)parser;
    return node;
}

static void free_node(DocNestedNode *node) {
    if (!node) return;

    if (node->type == DOC_NESTED_STRING && node->value.str_val) {
        free(node->value.str_val);
    } else if (node->type == DOC_NESTED_ARRAY) {
        for (size_t i = 0; i < node->num_children; i++) {
            free_node(node->value.array_val[i]);
        }
        free(node->value.array_val);
    } else if (node->type == DOC_NESTED_OBJECT) {
        for (size_t i = 0; i < node->num_children; i++) {
            free_node(node->value.object_val[i]);
        }
        free(node->value.object_val);
    }

    free(node->key);
    free(node->path);
    free(node);
}

static DocNestedNode *parse_value(DocNestedParser *parser, const char **p,
                                  size_t *remaining, const char *key, int index,
                                  DocNestedNode *parent) {
    *p = skip_ws(*p);
    DocNestedNode *node = NULL;

    if (**p == '"') {
        size_t val_len;
        const char *val_start = extract_value_range(*p, remaining, &val_len);
        node = create_node(parser, DOC_NESTED_STRING, key, index);
        if (node) {
            node->value.str_val = copy_unescaped(val_start, val_start + val_len, &node->value.num_children);
            node->parent = parent;
        }
    } else if (isdigit(**p) || **p == '-' || **p == '+') {
        char *end;
        double num = strtod(*p, &end);
        node = create_node(parser, DOC_NESTED_DOUBLE, key, index);
        if (node) {
            node->value.double_val = num;
            size_t consumed = end - *p;
            if (consumed <= *remaining) { *p += consumed; *remaining -= consumed; }
            else { *p = end; *remaining = 0; }
        }
        node->parent = parent;
    } else if (strncmp(*p, "true", 4) == 0) {
        node = create_node(parser, DOC_NESTED_BOOL, key, index);
        if (node) node->value.bool_val = true;
        if (*remaining >= 4) { *p += 4; *remaining -= 4; }
        node->parent = parent;
    } else if (strncmp(*p, "false", 5) == 0) {
        node = create_node(parser, DOC_NESTED_BOOL, key, index);
        if (node) node->value.bool_val = false;
        if (*remaining >= 5) { *p += 5; *remaining -= 5; }
        node->parent = parent;
    } else if (strncmp(*p, "null", 4) == 0) {
        node = create_node(parser, DOC_NESTED_NULL, key, index);
        if (*remaining >= 4) { *p += 4; *remaining -= 4; }
        node->parent = parent;
    } else if (**p == '[') {
        node = create_node(parser, DOC_NESTED_ARRAY, key, index);
        if (node) {
            node->parent = parent;
            node->value.array_val = (DocNestedNode **)calloc(DEFAULT_NODE_CAPACITY, sizeof(DocNestedNode *));
            size_t capacity = DEFAULT_NODE_CAPACITY;

            if (*remaining > 0) { (*p)++; (*remaining)--; }
            int idx = 0;
            while (*remaining > 0 && **p && **p != ']') {
                *p = skip_ws(*p);
                if (**p == ',') { (*p)++; (*remaining)--; continue; }
                DocNestedNode *child = parse_value(parser, p, remaining, NULL, idx, node);
                if (child) {
                    if (node->num_children >= capacity) {
                        capacity *= 2;
                        node->value.array_val = (DocNestedNode **)realloc(node->value.array_val,
                            capacity * sizeof(DocNestedNode *));
                    }
                    node->value.array_val[node->num_children++] = child;
                    idx++;
                }
            }
            if (*remaining > 0 && **p == ']') { (*p)++; (*remaining)--; }
        }
    } else if (**p == '{') {
        node = create_node(parser, DOC_NESTED_OBJECT, key, index);
        if (node) {
            node->parent = parent;
            node->value.object_val = (DocNestedNode **)calloc(DEFAULT_NODE_CAPACITY, sizeof(DocNestedNode *));
            size_t capacity = DEFAULT_NODE_CAPACITY;

            if (*remaining > 0) { (*p)++; (*remaining)--; }
            while (*remaining > 0 && **p && **p != '}') {
                *p = skip_ws(*p);
                if (**p == ',') { (*p)++; (*remaining)--; continue; }
                if (**p == '"') {
                    size_t key_len;
                    const char *key_start = *p + 1;
                    (*p)++; (*remaining)--;
                    while (*remaining > 0 && **p && **p != '"') {
                        if (**p == '\\') { (*p)++; (*remaining)--; }
                        (*p)++; (*remaining)--;
                    }
                    key_len = *p - key_start - 1;
                    char *field_key = (char *)malloc(key_len + 1);
                    memcpy(field_key, key_start, key_len);
                    field_key[key_len] = '\0';

                    if (*remaining > 0 && **p == '"') { (*p)++; (*remaining)--; }
                    *p = skip_ws(*p);
                    if (*remaining > 0 && **p == ':') { (*p)++; (*remaining)--; }

                    DocNestedNode *child = parse_value(parser, p, remaining, field_key, -1, node);
                    if (child) {
                        if (node->num_children >= capacity) {
                            capacity *= 2;
                            node->value.object_val = (DocNestedNode **)realloc(node->value.object_val,
                                capacity * sizeof(DocNestedNode *));
                        }
                        node->value.object_val[node->num_children++] = child;
                    }
                    free(field_key);
                } else {
                    break;
                }
            }
            if (*remaining > 0 && **p == '}') { (*p)++; (*remaining)--; }
        }
    }

    return node;
}

/* ========================================================================
 * 公共 API
 * ======================================================================== */

DocNestedParser *doc_nested_parser_create(const char *json, size_t len) {
    if (!json) return NULL;

    DocNestedParser *parser = (DocNestedParser *)calloc(1, sizeof(DocNestedParser));
    if (!parser) return NULL;

    parser->doc_len = len > 0 ? len : strlen(json);
    parser->doc_json = (char *)malloc(parser->doc_len + 1);
    if (!parser->doc_json) {
        free(parser);
        return NULL;
    }
    memcpy(parser->doc_json, json, parser->doc_len);
    parser->doc_json[parser->doc_len] = '\0';

    const char *p = parser->doc_json;
    size_t remaining = parser->doc_len;
    parser->root = parse_value(parser, &p, &remaining, NULL, -1, NULL);

    return parser;
}

void doc_nested_parser_destroy(DocNestedParser *parser) {
    if (!parser) return;
    free_node(parser->root);
    free(parser->doc_json);
    free(parser);
}

DocNestedNode *doc_nested_get_root(DocNestedParser *parser) {
    return parser ? parser->root : NULL;
}

static DocNestedNode **find_in_node(DocNestedNode *node, const char *path,
                                    size_t *out_count) {
    *out_count = 0;
    if (!node || !path) return NULL;

    if (*path == '$') path++;

    /* 解析路径段 */
    char key[128];
    int index = -1;

    if (*path == '.') path++;

    const char *p = path;
    const char *key_start = p;
    while (*p && *p != '.' && *p != '[') p++;
    size_t key_len = p - key_start;
    if (key_len >= sizeof(key)) key_len = sizeof(key) - 1;
    memcpy(key, key_start, key_len);
    key[key_len] = '\0';

    if (*p == '[') {
        p++;
        index = atoi(p);
        while (*p && *p != ']') p++;
        if (*p == ']') p++;
    }

    /* 匹配当前节点 */
    DocNestedNode **results = (DocNestedNode **)calloc(16, sizeof(DocNestedNode *));
    size_t capacity = 16;
    size_t count = 0;

    if (node->type == DOC_NESTED_OBJECT) {
        for (size_t i = 0; i < node->num_children; i++) {
            DocNestedNode *child = node->value.object_val[i];
            if (child && strcmp(child->key, key) == 0) {
                if (*p == '\0') {
                    if (count >= capacity) {
                        capacity *= 2;
                        results = (DocNestedNode **)realloc(results, capacity * sizeof(DocNestedNode *));
                    }
                    results[count++] = child;
                } else {
                    size_t sub_count;
                    DocNestedNode **sub = find_in_node(child, p, &sub_count);
                    for (size_t j = 0; j < sub_count; j++) {
                        if (count >= capacity) {
                            capacity *= 2;
                            results = (DocNestedNode **)realloc(results, capacity * sizeof(DocNestedNode *));
                        }
                        results[count++] = sub[j];
                    }
                    free(sub);
                }
            }
        }
    } else if (node->type == DOC_NESTED_ARRAY) {
        for (size_t i = 0; i < node->num_children; i++) {
            DocNestedNode *child = node->value.array_val[i];
            if (*p == '\0') {
                if (index < 0 || (int)i == index) {
                    if (count >= capacity) {
                        capacity *= 2;
                        results = (DocNestedNode **)realloc(results, capacity * sizeof(DocNestedNode *));
                    }
                    results[count++] = child;
                }
            } else {
                size_t sub_count;
                DocNestedNode **sub = find_in_node(child, p, &sub_count);
                for (size_t j = 0; j < sub_count; j++) {
                    if (count >= capacity) {
                        capacity *= 2;
                        results = (DocNestedNode **)realloc(results, capacity * sizeof(DocNestedNode *));
                    }
                    results[count++] = sub[j];
                }
                free(sub);
            }
        }
    }

    *out_count = count;
    return results;
}

DocNestedNode **doc_nested_find(DocNestedParser *parser, const char *path,
                                size_t *out_count) {
    if (!parser || !parser->root || !path) {
        *out_count = 0;
        return NULL;
    }
    return find_in_node(parser->root, path, out_count);
}

const char *doc_nested_get_string(DocNestedNode *node, size_t *out_len) {
    if (!node) return NULL;

    switch (node->type) {
        case DOC_NESTED_STRING:
            if (out_len) *out_len = node->value.num_children;
            return node->value.str_val;
        case DOC_NESTED_INT: {
            static char buf[32];
            snprintf(buf, sizeof(buf), "%ld", node->value.int_val);
            if (out_len) *out_len = strlen(buf);
            return buf;
        }
        case DOC_NESTED_DOUBLE: {
            static char buf[64];
            snprintf(buf, sizeof(buf), "%.10g", node->value.double_val);
            if (out_len) *out_len = strlen(buf);
            return buf;
        }
        case DOC_NESTED_BOOL:
            if (out_len) *out_len = node->value.bool_val ? 4 : 5;
            return node->value.bool_val ? "true" : "false";
        case DOC_NESTED_NULL:
            if (out_len) *out_len = 4;
            return "null";
        default:
            return NULL;
    }
}

void doc_nested_free_nodes(DocNestedNode **nodes, size_t count) {
    (void)count;
    free(nodes);
}

/* ========================================================================
 * 序列化
 * ======================================================================== */

static void node_to_json(DocNestedNode *node, char *buf, size_t buf_size,
                        size_t *pos, int indent, bool pretty) {
    if (!node || !buf) return;

    const char *prefix = "";
    if (pretty && indent > 0) {
        for (int i = 0; i < indent; i++) prefix = "    ";
    }

    switch (node->type) {
        case DOC_NESTED_NULL:
            *pos += snprintf(buf + *pos, buf_size - *pos, "null");
            break;
        case DOC_NESTED_BOOL:
            *pos += snprintf(buf + *pos, buf_size - *pos, "%s",
                           node->value.bool_val ? "true" : "false");
            break;
        case DOC_NESTED_INT:
            *pos += snprintf(buf + *pos, buf_size - *pos, "%ld", node->value.int_val);
            break;
        case DOC_NESTED_DOUBLE:
            *pos += snprintf(buf + *pos, buf_size - *pos, "%.10g", node->value.double_val);
            break;
        case DOC_NESTED_STRING: {
            const char *s = node->value.str_val ? node->value.str_val : "";
            *pos += snprintf(buf + *pos, buf_size - *pos, "\"%s\"", s);
            break;
        }
        case DOC_NESTED_ARRAY:
            *pos += snprintf(buf + *pos, buf_size - *pos, "[");
            for (size_t i = 0; i < node->num_children; i++) {
                if (i > 0) *pos += snprintf(buf + *pos, buf_size - *pos, ",");
                if (pretty) *pos += snprintf(buf + *pos, buf_size - *pos, "\n%s", prefix);
                node_to_json(node->value.array_val[i], buf, buf_size, pos, indent + 1, pretty);
            }
            if (pretty && node->num_children > 0) {
                *pos += snprintf(buf + *pos, buf_size - *pos, "\n%s", prefix);
            }
            *pos += snprintf(buf + *pos, buf_size - *pos, "]");
            break;
        case DOC_NESTED_OBJECT:
            *pos += snprintf(buf + *pos, buf_size - *pos, "{");
            for (size_t i = 0; i < node->num_children; i++) {
                if (i > 0) *pos += snprintf(buf + *pos, buf_size - *pos, ",");
                if (pretty) *pos += snprintf(buf + *pos, buf_size - *pos, "\n%s", prefix);
                DocNestedNode *child = node->value.object_val[i];
                *pos += snprintf(buf + *pos, buf_size - *pos, "\"%s\":", child->key ? child->key : "");
                node_to_json(child, buf, buf_size, pos, indent + 1, pretty);
            }
            if (pretty && node->num_children > 0) {
                *pos += snprintf(buf + *pos, buf_size - *pos, "\n%s", prefix);
            }
            *pos += snprintf(buf + *pos, buf_size - *pos, "}");
            break;
    }
}

char *doc_nested_to_json(DocNestedNode *node, bool pretty) {
    if (!node) return NULL;

    char *buf = (char *)malloc(4096);
    if (!buf) return NULL;

    size_t pos = 0;
    node_to_json(node, buf, 4096, &pos, 0, pretty);
    return buf;
}

int doc_nested_update(DocNestedParser *parser, const char *path, const char *value) {
    (void)parser; (void)path; (void)value;
    /* 实现需要更复杂的树操作逻辑 */
    return -1;
}

int doc_nested_delete(DocNestedParser *parser, const char *path) {
    (void)parser; (void)path;
    return -1;
}

int doc_nested_insert(DocNestedParser *parser, const char *path, const char *value) {
    (void)parser; (void)path; (void)value;
    return -1;
}

/* ========================================================================
 * 嵌套索引
 * ======================================================================== */

static DocNestedIndexMgr *g_index_mgr = NULL;

DocNestedIndexMgr *doc_nested_index_mgr_create(void *mem_pool) {
    DocNestedIndexMgr *mgr = (DocNestedIndexMgr *)calloc(1, sizeof(DocNestedIndexMgr));
    if (mgr) {
        mgr->mem_pool = mem_pool;
        mgr->indexes = (DocNestedIndex **)calloc(16, sizeof(DocNestedIndex *));
        mgr->num_indexes = 0;
    }
    return mgr;
}

void doc_nested_index_mgr_destroy(DocNestedIndexMgr *mgr) {
    if (!mgr) return;
    for (size_t i = 0; i < mgr->num_indexes; i++) {
        free(mgr->indexes[i]);
    }
    free(mgr->indexes);
    free(mgr);
}

int doc_nested_index_create(DocNestedIndexMgr *mgr,
                           const char *index_name,
                           const char **field_paths,
                           size_t num_fields) {
    if (!mgr || !index_name || !field_paths || num_fields == 0) return -1;

    DocNestedIndex *idx = (DocNestedIndex *)calloc(1, sizeof(DocNestedIndex));
    if (!idx) return -1;

    snprintf(idx->index_name, sizeof(idx->index_name), "%s", index_name);
    idx->fields = (DocNestedField *)calloc(num_fields, sizeof(DocNestedField));
    idx->num_fields = num_fields;

    for (size_t i = 0; i < num_fields; i++) {
        snprintf(idx->fields[i].path, sizeof(idx->fields[i].path), "%s", field_paths[i]);
        snprintf(idx->fields[i].field_name, sizeof(idx->fields[i].field_name), "field_%zu", i);
        idx->fields[i].is_indexed = true;
    }

    mgr->indexes[mgr->num_indexes++] = idx;
    return 0;
}

int doc_nested_index_drop(DocNestedIndexMgr *mgr, const char *index_name) {
    if (!mgr || !index_name) return -1;

    for (size_t i = 0; i < mgr->num_indexes; i++) {
        if (strcmp(mgr->indexes[i]->index_name, index_name) == 0) {
            free(mgr->indexes[i]);
            for (size_t j = i; j < mgr->num_indexes - 1; j++) {
                mgr->indexes[j] = mgr->indexes[j + 1];
            }
            mgr->num_indexes--;
            return 0;
        }
    }
    return -1;
}

int doc_nested_index_document(DocNestedIndexMgr *mgr,
                             const char *index_name,
                             const char *doc_id,
                             DocNestedParser *parser) {
    (void)mgr; (void)index_name; (void)doc_id; (void)parser;
    return 0;
}

int doc_nested_index_remove(DocNestedIndexMgr *mgr,
                           const char *index_name,
                           const char *doc_id) {
    (void)mgr; (void)index_name; (void)doc_id;
    return 0;
}

size_t doc_nested_index_search(DocNestedIndexMgr *mgr,
                               const char *index_name,
                               const char *conditions,
                               char ***out_doc_ids,
                               size_t max_results) {
    (void)mgr; (void)index_name; (void)conditions;
    *out_doc_ids = (char **)calloc(max_results, sizeof(char *));
    return 0;
}

/* ========================================================================
 * 增强 JSONPath
 * ======================================================================== */

DocJsonpathCtx *doc_jsonpath_parse(const char *jsonpath) {
    if (!jsonpath) return NULL;

    DocJsonpathCtx *ctx = (DocJsonpathCtx *)calloc(1, sizeof(DocJsonpathCtx));
    if (!ctx) return NULL;

    ctx->expression = strdup(jsonpath);
    ctx->depth_limit = -1;
    ctx->include_matches = false;

    if (strstr(jsonpath, "$..")) {
        ctx->mode = DOC_JP_MODE_RECURSIVE;
    } else if (strchr(jsonpath, '*')) {
        ctx->mode = DOC_JP_MODE_WILDCARD;
    } else if (strchr(jsonpath, '?')) {
        ctx->mode = DOC_JP_MODE_FILTER;
    } else {
        ctx->mode = DOC_JP_MODE_BASIC;
    }

    return ctx;
}

void doc_jsonpath_free(DocJsonpathCtx *ctx) {
    if (!ctx) return;
    free(ctx->expression);
    free(ctx->filter_predicate);
    free(ctx);
}

static size_t query_recursive(const char *json, const char *path,
                             doc_jsonpath_result_t **results, size_t max_results,
                             size_t *current_count) {
    size_t count = 0;
    const char *p = json;
    size_t remaining = strlen(json);

    p = skip_ws(p);
    if (*p == '{') {
        if (remaining > 0) { p++; remaining--; }
        while (remaining > 0 && *p && *p != '}') {
            p = skip_ws(p);
            if (*p == ',') { p++; remaining--; continue; }
            if (*p == '"') {
                size_t key_len;
                const char *key_start = p + 1;
                p++; remaining--;
                while (remaining > 0 && *p && *p != '"') {
                    if (*p == '\\') { p++; remaining--; }
                    p++; remaining--;
                }
                key_len = p - key_start - 1;

                if (remaining > 0 && *p == '"') { p++; remaining--; }
                p = skip_ws(p);
                if (remaining > 0 && *p == ':') { p++; remaining--; }

                char *key = (char *)malloc(key_len + 1);
                memcpy(key, key_start, key_len);
                key[key_len] = '\0';

                char search_path[256];
                snprintf(search_path, sizeof(search_path), "$..%s", key);

                if (strstr(path, search_path) || strcmp(path, "$..*") == 0) {
                    if (*current_count < max_results) {
                        size_t val_len;
                        const char *val_start = extract_value_range(p, &remaining, &val_len);
                        doc_jsonpath_result_t *r = &((*results)[*current_count]);
                        r->doc_id = NULL;
                        r->jsonpath = strdup(search_path);
                        r->value = (char *)malloc(val_len + 1);
                        memcpy(r->value, val_start, val_len);
                        r->value[val_len] = '\0';
                        r->value_len = val_len;
                        (*current_count)++;
                        count++;
                    }
                }

                free(key);
                skip_value(&p, &remaining);
            } else {
                break;
            }
        }
    } else if (*p == '[') {
        if (remaining > 0) { p++; remaining--; }
        while (remaining > 0 && *p && *p != ']') {
            p = skip_ws(p);
            if (*p == ',') { p++; remaining--; continue; }
            count += query_recursive(p, path, results, max_results, current_count);
            skip_value(&p, &remaining);
        }
        if (remaining > 0 && *p == ']') { p++; remaining--; }
    }

    return count;
}

size_t doc_jsonpath_execute(const char *json,
                           DocJsonpathCtx *ctx,
                           doc_jsonpath_result_t **results,
                           size_t max_results) {
    if (!json || !ctx || !results) return 0;

    *results = (doc_jsonpath_result_t *)calloc(max_results, sizeof(doc_jsonpath_result_t));
    size_t count = 0;

    if (ctx->mode == DOC_JP_MODE_RECURSIVE) {
        query_recursive(json, ctx->expression, results, max_results, &count);
    } else if (ctx->mode == DOC_JP_MODE_WILDCARD) {
        /* 通配符查询简化为返回整个文档 */
        doc_jsonpath_result_t *r = &((*results)[0]);
        r->jsonpath = strdup(ctx->expression);
        r->value = strdup(json);
        r->value_len = strlen(json);
        count = 1;
    } else {
        /* 基础模式使用原有 jsonpath_query */
        jsonpath_result_t old_result = {0};
        if (jsonpath_query(json, strlen(json), ctx->expression, &old_result) == 0) {
            for (size_t i = 0; i < old_result.count && count < max_results; i++) {
                doc_jsonpath_result_t *r = &((*results)[count]);
                r->jsonpath = old_result.values[i].path ? strdup(old_result.values[i].path) : NULL;
                if (old_result.values[i].type == JSONPATH_STRING) {
                    r->value = old_result.values[i].value.str_val ?
                              strdup(old_result.values[i].value.str_val) : strdup("");
                    r->value_len = old_result.values[i].value_len;
                }
                count++;
            }
            jsonpath_free_result(&old_result);
        }
    }

    return count;
}

size_t doc_jsonpath_query_wildcard(const char *json, const char *jsonpath,
                                   doc_jsonpath_result_t **results, size_t max_results) {
    DocJsonpathCtx *ctx = doc_jsonpath_parse(jsonpath);
    if (!ctx) return 0;

    size_t count = doc_jsonpath_execute(json, ctx, results, max_results);
    doc_jsonpath_free(ctx);
    return count;
}

size_t doc_jsonpath_query_predicate(const char *json, const char *jsonpath,
                                    doc_jsonpath_result_t **results, size_t max_results) {
    DocJsonpathCtx *ctx = doc_jsonpath_parse(jsonpath);
    if (!ctx) return 0;

    size_t count = doc_jsonpath_execute(json, ctx, results, max_results);
    doc_jsonpath_free(ctx);
    return count;
}

/* ========================================================================
 * 部分更新
 * ======================================================================== */

DocPartialUpdate doc_update_set(const char *path, const char *value) {
    DocPartialUpdate up = {DOC_UPDATE_SET, {0}, NULL};
    snprintf(up.path, sizeof(up.path), "%s", path);
    up.value = value ? strdup(value) : NULL;
    return up;
}

DocPartialUpdate doc_update_unset(const char *path) {
    DocPartialUpdate up = {DOC_UPDATE_UNSET, {0}, NULL};
    snprintf(up.path, sizeof(up.path), "%s", path);
    return up;
}

DocPartialUpdate doc_update_increment(const char *path, double delta) {
    DocPartialUpdate up = {DOC_UPDATE_INCREMENT, {0}, NULL};
    snprintf(up.path, sizeof(up.path), "%s", path);
    char buf[64];
    snprintf(buf, sizeof(buf), "%.10g", delta);
    up.value = strdup(buf);
    return up;
}

DocPartialUpdate doc_update_append(const char *path, const char *value) {
    DocPartialUpdate up = {DOC_UPDATE_APPEND, {0}, NULL};
    snprintf(up.path, sizeof(up.path), "%s", path);
    up.value = value ? strdup(value) : NULL;
    return up;
}

DocPartialUpdate doc_update_push(const char *path, const char *value) {
    DocPartialUpdate up = {DOC_UPDATE_PUSH, {0}, NULL};
    snprintf(up.path, sizeof(up.path), "%s", path);
    up.value = value ? strdup(value) : NULL;
    return up;
}

DocPartialUpdate doc_update_pull(const char *path, const char *value) {
    DocPartialUpdate up = {DOC_UPDATE_PULL, {0}, NULL};
    snprintf(up.path, sizeof(up.path), "%s", path);
    up.value = value ? strdup(value) : NULL;
    return up;
}

void doc_partial_update_free(DocPartialUpdate *updates, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(updates[i].value);
    }
}

int doc_partial_update(const char *doc_json,
                      const DocPartialUpdate *updates,
                      size_t num_updates,
                      char **out_json) {
    if (!doc_json || !updates || num_updates == 0 || !out_json) return -1;

    /* 简化实现：复制原文档 */
    *out_json = strdup(doc_json);
    return *out_json ? 0 : -1;
}
