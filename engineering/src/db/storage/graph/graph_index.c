/**
 * @file graph_index.c
 * @brief 图索引实现
 *
 * 支持 Label 索引和属性索引
 */
#include "db/graph/index.h"
#include "db/graph/graph.h"
#include "db/storage/kv/kv.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

/* 定义 Oid 类型（如果未定义） */
#ifndef Oid
typedef uint32_t Oid;
#endif

/* ============================================================
 * 存储键前缀
 * ============================================================ */

#define GRAPH_KEY_INDEX_META     "gi:meta:"      /**< 索引元数据: gi:meta:{oid} */
#define GRAPH_KEY_INDEX_LIST     "gi:list"       /**< 索引列表 */
#define GRAPH_KEY_LABEL_INDEX    "gli:"          /**< Label 索引: gli:{label}:{vid} */
#define GRAPH_KEY_PROP_INDEX     "gpi:"          /**< 属性索引: gpi:{label}:{prop}:{value}:{vid} */
#define GRAPH_KEY_PROP_SCAN      "gpiscan:"      /**< 属性索引扫描: gpiscan:{label}:{prop} */

/* ============================================================
 * 索引扫描结构
 * ============================================================ */

struct graph_index_scan_s {
    kv_t               *kv;            /**< KV 存储 */
    graph_index_type_t  type;           /**< 索引类型 */
    char                label[64];       /**< 标签 */
    char                property[64];    /**< 属性名 */
    graph_value_t       min_value;       /**< 范围扫描最小值 */
    graph_value_t       max_value;       /**< 范围扫描最大值 */
    bool                has_range;       /**< 是否有范围 */
    char                scan_prefix[128]; /**< 扫描前缀 */
    size_t              scan_prefix_len;  /**< 前缀长度 */
    kv_iter_t           *iter;          /**< KV 迭代器 */
};

/* ============================================================
 * 工具函数
 * ============================================================ */

/**
 * @brief 序列化属性值到字符串
 */
static int serialize_value(const graph_value_t *value, char *out, size_t out_size) {
    if (!value || !out) return -1;

    switch (value->type) {
        case GRAPH_NULL:
            snprintf(out, out_size, "null");
            break;
        case GRAPH_INT:
            snprintf(out, out_size, "%lld", (long long)value->u.int_val);
            break;
        case GRAPH_FLOAT:
            snprintf(out, out_size, "%.6f", value->u.float_val);
            break;
        case GRAPH_BOOL:
            snprintf(out, out_size, "%s", value->u.bool_val ? "true" : "false");
            break;
        case GRAPH_STRING:
            snprintf(out, out_size, "%s", value->u.string_val.str);
            break;
        default:
            return -1;
    }
    return 0;
}

/**
 * @brief 构建属性索引键
 */
static int build_prop_index_key(const char *label, const char *prop,
                                const graph_value_t *value,
                                graph_vertex_id_t vid,
                                char *out_key, size_t *out_len) {
    char value_str[256];
    serialize_value(value, value_str, sizeof(value_str));

    *out_len = snprintf(out_key, 512, GRAPH_KEY_PROP_INDEX "%s:%s:%s:%lu",
                        label ? label : "",
                        prop ? prop : "",
                        value_str,
                        (unsigned long)vid);
    return 0;
}

/* ============================================================
 * 索引操作
 * ============================================================ */

/**
 * @brief 生成唯一 OID
 */
static Oid generate_oid(void) {
    static Oid next_oid = 1;
    return next_oid++;
}

Oid graph_index_create(graph_t *g,
                       const char *name,
                       graph_index_type_t type,
                       const char *label,
                       const char *property,
                       bool is_unique) {
    if (!g) return 0;

    kv_t *kv = (kv_t*)graph_get_kv(g);
    if (!kv) return 0;

    /* 生成 OID */
    Oid oid = generate_oid();

    /* 存储索引元数据 */
    char key[128];
    snprintf(key, sizeof(key), GRAPH_KEY_INDEX_META "%u", oid);

    /* 构建元数据字符串 */
    char meta[512];
    int len = snprintf(meta, sizeof(meta), "%s|%d|%s|%s|%d",
                       name, type,
                       label ? label : "",
                       property ? property : "",
                       is_unique ? 1 : 0);

    kv_put(kv, key, strlen(key), meta, len);

    /* 添加到索引列表 */
    char list_key[64] = GRAPH_KEY_INDEX_LIST;
    char *old_list = NULL;
    size_t old_len = 0;
    kv_result_t r = kv_get(kv, list_key, strlen(list_key), (void**)&old_list, &old_len);

    char new_list[1024];
    size_t new_len;
    if (r == KV_NOT_FOUND) {
        new_len = snprintf(new_list, sizeof(new_list), "%u", oid);
    } else {
        new_len = snprintf(new_list, sizeof(new_list), "%s,%u", old_list, oid);
        free(old_list);
    }

    kv_put(kv, list_key, strlen(list_key), new_list, new_len);

    return oid;
}

int graph_index_drop(graph_t *g, Oid index_oid) {
    if (!g) return -1;

    kv_t *kv = (kv_t*)graph_get_kv(g);
    if (!kv) return -1;

    /* 删除元数据 */
    char key[128];
    snprintf(key, sizeof(key), GRAPH_KEY_INDEX_META "%u", index_oid);
    kv_delete(kv, key, strlen(key));

    /* 从列表中移除 */
    char list_key[64] = GRAPH_KEY_INDEX_LIST;
    char *old_list = NULL;
    size_t old_len = 0;
    kv_result_t r = kv_get(kv, list_key, strlen(list_key), (void**)&old_list, &old_len);

    if (r == KV_OK && old_list) {
        /* 从列表中移除该 OID */
        char new_list[1024] = {0};
        char *token = strtok(old_list, ",");
        bool first = true;
        while (token) {
            if ((Oid)atoi(token) != index_oid) {
                if (!first) strcat(new_list, ",");
                strcat(new_list, token);
                first = false;
            }
            token = strtok(NULL, ",");
        }
        kv_put(kv, list_key, strlen(list_key), new_list, strlen(new_list));
        free(old_list);
    }

    return 0;
}

graph_index_info_t *graph_list_indexes(graph_t *g, size_t *out_count) {
    if (!g || !out_count) return NULL;

    kv_t *kv = (kv_t*)graph_get_kv(g);
    if (!kv) return NULL;

    *out_count = 0;

    /* 获取索引列表 */
    char list_key[64] = GRAPH_KEY_INDEX_LIST;
    char *list = NULL;
    size_t list_len = 0;
    kv_result_t r = kv_get(kv, list_key, strlen(list_key), (void**)&list, &list_len);

    if (r == KV_NOT_FOUND) {
        return NULL;
    }

    /* 计算索引数量 */
    size_t count = 1;
    for (size_t i = 0; i < list_len; i++) {
        if (list[i] == ',') count++;
    }

    graph_index_info_t *indexes = calloc(count, sizeof(graph_index_info_t));
    if (!indexes) {
        free(list);
        return NULL;
    }

    /* 解析每个索引 */
    char *token = strtok(list, ",");
    size_t idx = 0;
    while (token && idx < count) {
        Oid oid = (Oid)atoi(token);

        char key[128];
        snprintf(key, sizeof(key), GRAPH_KEY_INDEX_META "%u", oid);

        char *meta = NULL;
        size_t meta_len = 0;
        if (kv_get(kv, key, strlen(key), (void**)&meta, &meta_len) == KV_OK) {
            indexes[idx].oid = oid;

            /* 解析元数据 */
            char name[64] = {0}, type_str[16] = {0}, label[64] = {0},
                 prop[64] = {0}, unique_str[8] = {0};
            sscanf(meta, "%63[^|]|%15[^|]|%63[^|]|%63[^|]|%7s",
                   name, type_str, label, prop, unique_str);

            strncpy(indexes[idx].name, name, sizeof(indexes[idx].name) - 1);
            indexes[idx].type = (graph_index_type_t)atoi(type_str);
            strncpy(indexes[idx].label, label, sizeof(indexes[idx].label) - 1);
            strncpy(indexes[idx].property, prop, sizeof(indexes[idx].property) - 1);
            indexes[idx].is_unique = (unique_str[0] == '1');
            indexes[idx].is_valid = true;

            free(meta);
            idx++;
        }

        token = strtok(NULL, ",");
    }

    free(list);
    *out_count = idx;

    return indexes;
}

void graph_index_info_free(graph_index_info_t *indexes) {
    free(indexes);
}

/* ============================================================
 * 索引扫描
 * ============================================================ */

graph_index_scan_t *graph_index_scan_eq(graph_t *g,
                                        const char *label,
                                        const char *property,
                                        const graph_value_t *value) {
    if (!g || !value) return NULL;

    kv_t *kv = (kv_t*)graph_get_kv(g);
    if (!kv) return NULL;

    graph_index_scan_t *scan = calloc(1, sizeof(graph_index_scan_t));
    if (!scan) return NULL;

    scan->kv = kv;  /* 保存 KV 句柄 */
    scan->type = GRAPH_INDEX_PROPERTY;
    scan->has_range = true;
    scan->min_value = *value;
    scan->max_value = *value;

    if (label) strncpy(scan->label, label, sizeof(scan->label) - 1);
    if (property) strncpy(scan->property, property, sizeof(scan->property) - 1);

    /* 构建扫描前缀 */
    char value_str[256];
    serialize_value(value, value_str, sizeof(value_str));

    scan->scan_prefix_len = snprintf(scan->scan_prefix, sizeof(scan->scan_prefix),
                                     GRAPH_KEY_PROP_INDEX "%s:%s:%s:",
                                     label ? label : "",
                                     property ? property : "",
                                     value_str);

    /* 创建 KV 迭代器 */
    scan->iter = kv_scan(kv,
                         scan->scan_prefix, scan->scan_prefix_len,
                         scan->scan_prefix, scan->scan_prefix_len);

    return scan;
}

graph_index_scan_t *graph_index_scan_range(graph_t *g,
                                           const char *label,
                                           const char *property,
                                           const graph_value_t *min,
                                           const graph_value_t *max) {
    if (!g) return NULL;

    kv_t *kv = (kv_t*)graph_get_kv(g);
    if (!kv) return NULL;

    graph_index_scan_t *scan = calloc(1, sizeof(graph_index_scan_t));
    if (!scan) return NULL;

    scan->kv = kv;  /* 保存 KV 句柄 */
    scan->type = GRAPH_INDEX_PROPERTY;
    scan->has_range = true;

    if (min) {
        scan->min_value = *min;
    } else {
        scan->min_value.type = GRAPH_NULL;
    }

    if (max) {
        scan->max_value = *max;
    } else {
        scan->max_value.type = GRAPH_NULL;
    }

    if (label) strncpy(scan->label, label, sizeof(scan->label) - 1);
    if (property) strncpy(scan->property, property, sizeof(scan->property) - 1);

    /* 构建扫描前缀 */
    scan->scan_prefix_len = snprintf(scan->scan_prefix, sizeof(scan->scan_prefix),
                                     GRAPH_KEY_PROP_INDEX "%s:%s:",
                                     label ? label : "",
                                     property ? property : "");

    /* 创建 KV 迭代器 */
    char end_prefix[256];
    snprintf(end_prefix, sizeof(end_prefix), GRAPH_KEY_PROP_INDEX "%s:%s~",
             label ? label : "", property ? property : "");

    scan->iter = kv_scan(kv,
                         scan->scan_prefix, scan->scan_prefix_len,
                         end_prefix, strlen(end_prefix));

    return scan;
}

bool graph_index_scan_next(graph_index_scan_t *scan, graph_vertex_id_t *out_vid) {
    if (!scan || !scan->iter || !out_vid) return false;

    while (kv_iter_next(scan->iter) == KV_OK) {
        const void *key = kv_iter_key(scan->iter);
        size_t key_len = kv_iter_key_len(scan->iter);

        /* 跳过非索引键 */
        if (key_len <= scan->scan_prefix_len) continue;

        const char *key_str = (const char*)key;

        /* 解析顶点 ID（从键末尾提取） */
        /* 键格式: gpi:{label}:{prop}:{value}:{vid} */
        const char *vid_str = strrchr(key_str, ':');
        if (!vid_str) continue;
        vid_str++;  /* 跳过 ':' */

        graph_vertex_id_t vid = (graph_vertex_id_t)atoll(vid_str);

        /* 如果有范围限制，检查值是否在范围内 */
        if (scan->has_range && scan->min_value.type != GRAPH_NULL) {
            /* 需要解析值并比较 */
            /* 简化实现：直接返回所有匹配的键 */
        }

        *out_vid = vid;
        return true;
    }

    return false;
}

void graph_index_scan_close(graph_index_scan_t *scan) {
    if (!scan) return;
    if (scan->iter) {
        kv_iter_free(scan->iter);
    }
    free(scan);
}

/* ============================================================
 * 索引维护（供存储层调用）
 * ============================================================ */

/**
 * @brief 为顶点添加 Label 索引
 */
int graph_index_on_vertex_create(graph_t *g, graph_vertex_id_t vid, const char *label) {
    if (!g || !label) return -1;

    kv_t *kv = (kv_t*)graph_get_kv(g);
    if (!kv) return -1;

    /* 构建 Label 索引键 */
    char key[128];
    snprintf(key, sizeof(key), GRAPH_KEY_LABEL_INDEX "%s:%lu", label, (unsigned long)vid);

    /* 存储索引（值为空） */
    char val[1] = {0};
    kv_put(kv, key, strlen(key), val, 0);

    return 0;
}

/**
 * @brief 为顶点移除 Label 索引
 */
int graph_index_on_vertex_delete(graph_t *g, graph_vertex_id_t vid, const char *label) {
    if (!g || !label) return -1;

    kv_t *kv = (kv_t*)graph_get_kv(g);
    if (!kv) return -1;

    char key[128];
    snprintf(key, sizeof(key), GRAPH_KEY_LABEL_INDEX "%s:%lu", label, (unsigned long)vid);
    kv_delete(kv, key, strlen(key));

    return 0;
}

/**
 * @brief 为顶点添加属性索引
 */
int graph_index_on_property_set(graph_t *g, graph_vertex_id_t vid,
                                 const char *label, const char *prop,
                                 const graph_value_t *value) {
    if (!g || !prop || !value) return -1;

    kv_t *kv = (kv_t*)graph_get_kv(g);
    if (!kv) return -1;

    char key[512];
    size_t key_len;
    build_prop_index_key(label, prop, value, vid, key, &key_len);

    char val[1] = {0};
    kv_put(kv, key, key_len, val, 0);

    return 0;
}

/**
 * @brief 为顶点移除属性索引
 */
int graph_index_on_property_remove(graph_t *g, graph_vertex_id_t vid,
                                   const char *label, const char *prop,
                                   const graph_value_t *old_value) {
    if (!g || !prop || !old_value) return -1;

    kv_t *kv = (kv_t*)graph_get_kv(g);
    if (!kv) return -1;

    char key[512];
    size_t key_len;
    build_prop_index_key(label, prop, old_value, vid, key, &key_len);

    kv_delete(kv, key, key_len);

    return 0;
}
