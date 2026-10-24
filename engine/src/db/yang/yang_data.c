/**
 * @file yang_data.c
 * @brief YANG 数据节点与 datastore 实现
 *
 * 提供数据节点的创建、查找、路径解析、克隆、值转换等基础功能。
 * 数据树结构是多叉树：parent <-> first_child <-> next_sibling。
 */
#include "db/yang/yang_data.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ============================================================
 * 辅助宏
 * ============================================================ */

/** 安全写入字符串（限长） */
#define SET_STR(dst, src)                                   \
    do {                                                    \
        if ((src) != NULL) {                                \
            strncpy((dst), (src), sizeof(dst) - 1);         \
            (dst)[sizeof(dst) - 1] = '\0';                 \
        } else {                                            \
            (dst)[0] = '\0';                                \
        }                                                   \
    } while (0)

/* ============================================================
 * 节点生命周期
 * ============================================================ */

yang_data_node_t *yang_data_node_create(const char *name,
                                        yang_node_kind_t kind,
                                        yang_type_value_t value_type) {
    yang_data_node_t *node = (yang_data_node_t *)calloc(1, sizeof(yang_data_node_t));
    if (!node) return NULL;

    SET_STR(node->name, name);
    node->kind = kind;
    node->value_type = value_type;
    node->value[0] = '\0';

    for (int i = 0; i < 8; i++) {
        node->keys[i][0] = '\0';
    }

    node->parent = NULL;
    node->first_child = NULL;
    node->next_sibling = NULL;
    node->prev_sibling = NULL;
    return node;
}

void yang_data_node_free(yang_data_node_t *node) {
    if (!node) return;
    yang_data_node_t *child = node->first_child;
    while (child) {
        yang_data_node_t *next = child->next_sibling;
        yang_data_node_free(child);
        child = next;
    }
    free(node);
}

/* ============================================================
 * 父子 / 兄弟关系
 * ============================================================ */

int yang_data_add_child(yang_data_node_t *parent, yang_data_node_t *child) {
    if (!parent || !child) return -1;
    if (child->parent || child->next_sibling || child->prev_sibling) {
        return -1;  /* 已挂载到别处 */
    }
    child->parent = parent;
    child->prev_sibling = NULL;
    child->next_sibling = NULL;

    if (!parent->first_child) {
        parent->first_child = child;
    } else {
        yang_data_node_t *tail = parent->first_child;
        while (tail->next_sibling) tail = tail->next_sibling;
        tail->next_sibling = child;
        child->prev_sibling = tail;
    }
    return 0;
}

yang_data_node_t *yang_data_find_child(yang_data_node_t *parent,
                                       const char *name) {
    if (!parent || !name) return NULL;
    for (yang_data_node_t *c = parent->first_child; c; c = c->next_sibling) {
        if (strcmp(c->name, name) == 0) return c;
    }
    return NULL;
}

/* ============================================================
 * 路径解析
 * ============================================================
 *
 * 路径形式：
 *   /a/b/c                  仅节点名
 *   /interfaces[name='eth0']/mtu   list 项 + key
 *
 * 支持 list 的谓词 [key='value']，仅匹配第一个。
 */

static const char *skip_token(const char *p, char *out, size_t out_len) {
    if (!p || !out || out_len == 0) return NULL;
    size_t i = 0;
    while (*p && *p != '/' && *p != '[') {
        if (i + 1 < out_len) out[i++] = *p;
        p++;
    }
    out[i] = '\0';
    return p;
}

/* 解析列表谓词 [k1='v1'][k2='v2']... 写入 keys 与 vals。返回剩余字符串 */
static const char *parse_predicates(const char *p,
                                    char keys[][YANG_MAX_NAME_LEN],
                                    char vals[][YANG_MAX_VALUE_LEN],
                                    int *count) {
    *count = 0;
    while (*p == '[') {
        if (*count >= 8) return NULL;
        p++;  /* 跳过 '[' */
        /* 键名 */
        size_t i = 0;
        while (*p && *p != '=' && i + 1 < YANG_MAX_NAME_LEN) {
            keys[*count][i++] = *p++;
        }
        keys[*count][i] = '\0';
        if (*p != '=') return NULL;
        p++;  /* 跳过 '=' */
        if (*p != '\'') return NULL;
        p++;  /* 跳过 '\'' */
        i = 0;
        while (*p && *p != '\'' && i + 1 < YANG_MAX_VALUE_LEN) {
            vals[*count][i++] = *p++;
        }
        vals[*count][i] = '\0';
        if (*p != '\'') return NULL;
        p++;  /* 跳过 '\'' */
        if (*p != ']') return NULL;
        p++;  /* 跳过 ']' */
        (*count)++;
    }
    return p;
}

/** 比较 list 节点的 key 集合是否与谓词匹配 */
static bool match_keys(const yang_data_node_t *node,
                       char keys[][YANG_MAX_NAME_LEN],
                       char vals[][YANG_MAX_VALUE_LEN],
                       int count) {
    for (int i = 0; i < count; i++) {
        /* 在节点的 keys 中找，匹配后直接读节点子节点的 value */
        bool found = false;
        for (int j = 0; j < 8 && node->keys[j][0]; j++) {
            if (strcmp(node->keys[j], keys[i]) == 0) {
                /* key 的值作为 list 项的子节点存在 */
                yang_data_node_t *kv = yang_data_find_child(
                    (yang_data_node_t *)node, node->keys[j]);
                if (!kv) return false;
                if (strcmp(kv->value, vals[i]) != 0) return false;
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    return true;
}

yang_data_node_t *yang_data_get_node(yang_data_node_t *root, const char *path) {
    if (!root || !path) return NULL;

    /* 跳过前导 '/' */
    while (*path == '/') path++;

    if (*path == '\0') return root;

    yang_data_node_t *current = root;
    char name[YANG_MAX_NAME_LEN];
    char pred_keys[8][YANG_MAX_NAME_LEN];
    char pred_vals[8][YANG_MAX_VALUE_LEN];
    int pred_count = 0;

    while (*path) {
        const char *after = skip_token(path, name, sizeof(name));
        if (!name[0]) return NULL;

        if (*after == '[') {
            after = parse_predicates(after, pred_keys, pred_vals, &pred_count);
            if (!after) return NULL;
        } else {
            pred_count = 0;
        }

        yang_data_node_t *next = NULL;
        for (yang_data_node_t *c = current->first_child; c; c = c->next_sibling) {
            if (strcmp(c->name, name) != 0) continue;
            if (pred_count > 0) {
                if (c->kind != YANG_KIND_LIST) continue;
                if (match_keys(c, pred_keys, pred_vals, pred_count)) {
                    next = c;
                    break;
                }
            } else {
                next = c;
                break;
            }
        }
        if (!next) return NULL;

        current = next;
        path = after;
        while (*path == '/') path++;
    }
    return current;
}

yang_data_node_t *yang_data_create_node(yang_data_node_t *root,
                                        const char *path,
                                        yang_node_kind_t kind) {
    if (!root || !path) return NULL;
    while (*path == '/') path++;
    if (*path == '\0') return root;

    yang_data_node_t *current = root;
    char name[YANG_MAX_NAME_LEN];
    char pred_keys[8][YANG_MAX_NAME_LEN];
    char pred_vals[8][YANG_MAX_VALUE_LEN];
    int pred_count = 0;

    while (*path) {
        const char *after = skip_token(path, name, sizeof(name));
        if (!name[0]) return NULL;

        if (*after == '[') {
            after = parse_predicates(after, pred_keys, pred_vals, &pred_count);
            if (!after) return NULL;
        } else {
            pred_count = 0;
        }

        yang_data_node_t *next = NULL;
        if (pred_count > 0) {
            /* 含谓词：在同名 list 子节点中按 key 匹配 */
            for (yang_data_node_t *c = current->first_child; c; c = c->next_sibling) {
                if (strcmp(c->name, name) != 0) continue;
                if (c->kind != YANG_KIND_LIST) continue;
                if (match_keys(c, pred_keys, pred_vals, pred_count)) {
                    next = c;
                    break;
                }
            }
        } else {
            next = yang_data_find_child(current, name);
        }
        if (!next) {
            next = yang_data_node_create(name, kind, YANG_TYPE_EMPTY);
            if (!next) return NULL;
            if (kind == YANG_KIND_LIST && pred_count > 0) {
                int k = 0;
                for (int i = 0; i < pred_count && k < 8; i++, k++) {
                    SET_STR(next->keys[k], pred_keys[i]);
                    yang_data_node_t *kv = yang_data_node_create(pred_keys[i],
                                                                 YANG_KIND_LEAF,
                                                                 YANG_TYPE_STRING);
                    SET_STR(kv->value, pred_vals[i]);
                    yang_data_add_child(next, kv);
                }
            }
            yang_data_add_child(current, next);
        }
        current = next;
        path = after;
        while (*path == '/') path++;
    }
    return current;
}

int yang_data_remove_child(yang_data_node_t *parent, const char *name) {
    if (!parent || !name) return -1;
    yang_data_node_t *prev = NULL;
    for (yang_data_node_t *c = parent->first_child; c; c = c->next_sibling) {
        if (strcmp(c->name, name) == 0) {
            if (prev) prev->next_sibling = c->next_sibling;
            else parent->first_child = c->next_sibling;
            if (c->next_sibling) c->next_sibling->prev_sibling = prev;
            c->parent = NULL;
            c->next_sibling = NULL;
            c->prev_sibling = NULL;
            yang_data_node_free(c);
            return 0;
        }
        prev = c;
    }
    return -1;
}

/* ============================================================
 * 节点克隆
 * ============================================================ */

yang_data_node_t *yang_data_clone(const yang_data_node_t *src) {
    if (!src) return NULL;
    yang_data_node_t *node = yang_data_node_create(src->name, src->kind,
                                                   src->value_type);
    SET_STR(node->value, src->value);
    for (int i = 0; i < 8; i++) {
        SET_STR(node->keys[i], src->keys[i]);
    }
    for (const yang_data_node_t *c = src->first_child; c; c = c->next_sibling) {
        yang_data_node_t *cc = yang_data_clone(c);
        if (!cc) {
            yang_data_node_free(node);
            return NULL;
        }
        yang_data_add_child(node, cc);
    }
    return node;
}

/* ============================================================
 * 值类型转换
 * ============================================================ */

yang_type_value_t yang_data_type_from_string(const char *s) {
    if (!s) return YANG_TYPE_EMPTY;
    if (strcmp(s, "int8") == 0) return YANG_TYPE_INT8;
    if (strcmp(s, "int16") == 0) return YANG_TYPE_INT16;
    if (strcmp(s, "int32") == 0) return YANG_TYPE_INT32;
    if (strcmp(s, "int64") == 0) return YANG_TYPE_INT64;
    if (strcmp(s, "uint8") == 0) return YANG_TYPE_UINT8;
    if (strcmp(s, "uint16") == 0) return YANG_TYPE_UINT16;
    if (strcmp(s, "uint32") == 0) return YANG_TYPE_UINT32;
    if (strcmp(s, "uint64") == 0) return YANG_TYPE_UINT64;
    if (strcmp(s, "string") == 0) return YANG_TYPE_STRING;
    if (strcmp(s, "boolean") == 0) return YANG_TYPE_BOOLEAN;
    return YANG_TYPE_EMPTY;
}

const char *yang_data_type_to_string(yang_type_value_t t) {
    switch (t) {
        case YANG_TYPE_INT8: return "int8";
        case YANG_TYPE_INT16: return "int16";
        case YANG_TYPE_INT32: return "int32";
        case YANG_TYPE_INT64: return "int64";
        case YANG_TYPE_UINT8: return "uint8";
        case YANG_TYPE_UINT16: return "uint16";
        case YANG_TYPE_UINT32: return "uint32";
        case YANG_TYPE_UINT64: return "uint64";
        case YANG_TYPE_STRING: return "string";
        case YANG_TYPE_BOOLEAN: return "boolean";
        default: return "empty";
    }
}

/** 将字符串解析为目标类型并写入临时缓冲区，返回 0 成功 */
static int parse_value(yang_type_value_t type, const char *text,
                       char *out, size_t out_len) {
    if (!text || !out || out_len == 0) return -1;
    switch (type) {
        case YANG_TYPE_STRING:
            SET_STR(out, text);
            return 0;
        case YANG_TYPE_BOOLEAN:
            if (strcmp(text, "true") == 0 || strcmp(text, "1") == 0) {
                SET_STR(out, "true");
                return 0;
            }
            if (strcmp(text, "false") == 0 || strcmp(text, "0") == 0) {
                SET_STR(out, "false");
                return 0;
            }
            return -1;
        case YANG_TYPE_INT8:  { long long v = strtoll(text, NULL, 0); if (v < -128 || v > 127) return -1; snprintf(out, out_len, "%lld", v); return 0; }
        case YANG_TYPE_INT16: { long long v = strtoll(text, NULL, 0); if (v < -32768 || v > 32767) return -1; snprintf(out, out_len, "%lld", v); return 0; }
        case YANG_TYPE_INT32: { long long v = strtoll(text, NULL, 0); if (v < -2147483648LL || v > 2147483647LL) return -1; snprintf(out, out_len, "%lld", v); return 0; }
        case YANG_TYPE_INT64: { long long v = strtoll(text, NULL, 0); snprintf(out, out_len, "%lld", v); return 0; }
        case YANG_TYPE_UINT8:  { unsigned long long v = strtoull(text, NULL, 0); if (v > 255) return -1; snprintf(out, out_len, "%llu", v); return 0; }
        case YANG_TYPE_UINT16: { unsigned long long v = strtoull(text, NULL, 0); if (v > 65535) return -1; snprintf(out, out_len, "%llu", v); return 0; }
        case YANG_TYPE_UINT32: { unsigned long long v = strtoull(text, NULL, 0); if (v > 4294967295ULL) return -1; snprintf(out, out_len, "%llu", v); return 0; }
        case YANG_TYPE_UINT64: { unsigned long long v = strtoull(text, NULL, 0); snprintf(out, out_len, "%llu", v); return 0; }
        default:
            SET_STR(out, text);
            return 0;
    }
}

int yang_data_set_value_from_string(yang_data_node_t *node, const char *text) {
    if (!node) return -1;
    if (!text) text = "";
    char buf[YANG_MAX_VALUE_LEN];
    if (parse_value(node->value_type, text, buf, sizeof(buf)) != 0) {
        return -1;
    }
    SET_STR(node->value, buf);
    return 0;
}

int yang_data_format_value(const yang_data_node_t *node,
                           char *out, size_t out_len) {
    if (!node || !out || out_len == 0) return -1;
    if (node->kind == YANG_KIND_CONTAINER || node->kind == YANG_KIND_LIST) {
        out[0] = '\0';
        return 0;
    }
    SET_STR(out, node->value);
    return (int)strlen(out);
}

/* ============================================================
 * Datastore
 * ============================================================ */

yang_datastore_t *yang_datastore_create(const char *name) {
    yang_datastore_t *ds = (yang_datastore_t *)calloc(1, sizeof(yang_datastore_t));
    if (!ds) return NULL;
    SET_STR(ds->name, name);
    ds->root = yang_data_node_create(name, YANG_KIND_CONTAINER, YANG_TYPE_EMPTY);
    if (!ds->root) {
        free(ds);
        return NULL;
    }
    /* 根节点 name 已设置为 datastore 名（与 data node name 不同源，但只是标识） */
    ds->node_count = 1;
    return ds;
}

void yang_datastore_free(yang_datastore_t *ds) {
    if (!ds) return;
    yang_data_node_free(ds->root);
    free(ds);
}

int yang_datastore_clear(yang_datastore_t *ds) {
    if (!ds) return -1;
    if (ds->root) {
        yang_data_node_free(ds->root);
    }
    ds->root = yang_data_node_create(ds->name, YANG_KIND_CONTAINER, YANG_TYPE_EMPTY);
    if (!ds->root) return -1;
    ds->node_count = 1;
    return 0;
}

yang_data_node_t *yang_datastore_root(yang_datastore_t *ds) {
    if (!ds) return NULL;
    return ds->root;
}