/**
 * @file graph_store.c
 * @brief 图存储引擎实现
 *
 * 基于 KV 存储实现图的顶点和边管理
 */
#include "db/graph/graph.h"
#include "db/storage/kv/kv.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* ============================================================
 * 存储键前缀
 * ============================================================ */

#define GRAPH_KEY_VERTEX_PREFIX   "gv:"        /**< 顶点: gv:{id} */
#define GRAPH_KEY_EDGE_PREFIX     "ge:"        /**< 边: ge:{id} */
#define GRAPH_KEY_OUT_EDGES       "gvo:"       /**< 出边索引: gvo:{vid} */
#define GRAPH_KEY_IN_EDGES        "gvi:"       /**< 入边索引: gvi:{vid} */
#define GRAPH_KEY_LABEL           "gl:"        /**< 标签: gl:{name} */
#define GRAPH_KEY_REL_TYPE        "gr:"        /**< 关系类型: gr:{name} */
#define GRAPH_KEY_LABEL_ID        "glid:"      /**< 标签ID映射: glid:{id} */
#define GRAPH_KEY_REL_TYPE_ID     "grtid:"     /**< 关系类型ID映射: grtid:{id} */
#define GRAPH_KEY_SEQ_VERTEX      "gseq:v"     /**< 顶点序列号 */
#define GRAPH_KEY_SEQ_EDGE        "gseq:e"     /**< 边序列号 */

/* ============================================================
 * 图数据库结构
 * ============================================================ */

struct graph_s {
    kv_t        *kv;                /**< KV 存储 */
    char        *db_path;           /**< 数据库路径 */
    char        error_msg[256];     /**< 错误信息 */

    /* 序列号缓存 */
    graph_vertex_id_t next_vertex_id;
    graph_edge_id_t   next_edge_id;
    graph_label_id_t  next_label_id;
    graph_label_id_t  next_rel_type_id;
};

/* ============================================================
 * 工具函数
 * ============================================================ */

/**
 * @brief 设置错误信息
 */
static void graph_set_error(graph_t *g, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(g->error_msg, sizeof(g->error_msg), fmt, args);
    va_end(args);
}

/**
 * @brief 分配顶点 ID
 */
static graph_vertex_id_t graph_alloc_vertex_id(graph_t *g) {
    char key[64];
    char value[16];

    snprintf(key, sizeof(key), GRAPH_KEY_SEQ_VERTEX);
    kv_result_t r = kv_get(g->kv, key, strlen(key), (void**)&value, NULL);

    graph_vertex_id_t id;
    if (r == KV_NOT_FOUND) {
        id = 1;
    } else {
        id = g->next_vertex_id;
    }

    snprintf(value, sizeof(value), "%lu", (unsigned long)(id + 1));
    kv_put(g->kv, key, strlen(key), value, strlen(value));
    g->next_vertex_id = id + 1;

    return id;
}

/**
 * @brief 分配边 ID
 */
static graph_edge_id_t graph_alloc_edge_id(graph_t *g) {
    char key[64];
    char value[16];

    snprintf(key, sizeof(key), GRAPH_KEY_SEQ_EDGE);
    kv_result_t r = kv_get(g->kv, key, strlen(key), (void**)&value, NULL);

    graph_edge_id_t id;
    if (r == KV_NOT_FOUND) {
        id = 1;
    } else {
        id = g->next_edge_id;
    }

    snprintf(value, sizeof(value), "%lu", (unsigned long)(id + 1));
    kv_put(g->kv, key, strlen(key), value, strlen(value));
    g->next_edge_id = id + 1;

    return id;
}

/**
 * @brief 分配标签 ID
 */
static graph_label_id_t graph_alloc_label_id(graph_t *g) {
    char key[64];
    char value[16];

    snprintf(key, sizeof(key), "gseq:label");
    kv_result_t r = kv_get(g->kv, key, strlen(key), (void**)&value, NULL);

    graph_label_id_t id;
    if (r == KV_NOT_FOUND) {
        id = 1;
    } else {
        id = g->next_label_id;
    }

    snprintf(value, sizeof(value), "%u", id + 1);
    kv_put(g->kv, key, strlen(key), value, strlen(value));
    g->next_label_id = id + 1;

    return id;
}

/**
 * @brief 分配关系类型 ID
 */
static graph_label_id_t graph_alloc_rel_type_id(graph_t *g) {
    char key[64];
    char value[16];

    snprintf(key, sizeof(key), "gseq:reltype");
    kv_result_t r = kv_get(g->kv, key, strlen(key), (void**)&value, NULL);

    graph_label_id_t id;
    if (r == KV_NOT_FOUND) {
        id = 1;
    } else {
        id = g->next_rel_type_id;
    }

    snprintf(value, sizeof(value), "%u", id + 1);
    kv_put(g->kv, key, strlen(key), value, strlen(value));
    g->next_rel_type_id = id + 1;

    return id;
}

/**
 * @brief 构建顶点键
 */
static int graph_make_vertex_key(graph_vertex_id_t vid, char *out_key, size_t *out_len) {
    *out_len = snprintf(out_key, 64, GRAPH_KEY_VERTEX_PREFIX "%lu", (unsigned long)vid);
    return 0;
}

/**
 * @brief 构建边键
 */
static int graph_make_edge_key(graph_edge_id_t eid, char *out_key, size_t *out_len) {
    *out_len = snprintf(out_key, 64, GRAPH_KEY_EDGE_PREFIX "%lu", (unsigned long)eid);
    return 0;
}

/**
 * @brief 构建出边索引键
 */
static int graph_make_out_edges_key(graph_vertex_id_t vid, char *out_key, size_t *out_len) {
    *out_len = snprintf(out_key, 64, GRAPH_KEY_OUT_EDGES "%lu", (unsigned long)vid);
    return 0;
}

/**
 * @brief 构建入边索引键
 */
static int graph_make_in_edges_key(graph_vertex_id_t vid, char *out_key, size_t *out_len) {
    *out_len = snprintf(out_key, 64, GRAPH_KEY_IN_EDGES "%lu", (unsigned long)vid);
    return 0;
}

/**
 * @brief 添加边到邻接表
 */
static int graph_add_edge_to_adj_list(graph_t *g,
                                     graph_vertex_id_t src,
                                     graph_vertex_id_t dst,
                                     graph_edge_id_t eid,
                                     bool is_out) {
    char key[64];
    size_t key_len;
    char *old_value = NULL;
    size_t old_len = 0;

    if (is_out) {
        graph_make_out_edges_key(src, key, &key_len);
    } else {
        graph_make_in_edges_key(dst, key, &key_len);
    }

    kv_result_t r = kv_get(g->kv, key, key_len, (void**)&old_value, &old_len);

    /* 计算新值大小 */
    size_t new_len = old_len + sizeof(graph_edge_id_t);
    char *new_value = malloc(new_len);

    if (r == KV_NOT_FOUND) {
        /* 新建邻接表 */
        memcpy(new_value, &eid, sizeof(graph_edge_id_t));
    } else {
        /* 追加到现有邻接表 */
        memcpy(new_value, old_value, old_len);
        memcpy(new_value + old_len, &eid, sizeof(graph_edge_id_t));
        free(old_value);
    }

    kv_put(g->kv, key, key_len, new_value, new_len);
    free(new_value);

    return 0;
}

/**
 * @brief 从邻接表移除边
 */
static int graph_remove_edge_from_adj_list(graph_t *g,
                                           graph_vertex_id_t src,
                                           graph_vertex_id_t dst,
                                           graph_edge_id_t eid,
                                           bool is_out) {
    char key[64];
    size_t key_len;
    char *old_value = NULL;
    size_t old_len = 0;

    if (is_out) {
        graph_make_out_edges_key(src, key, &key_len);
    } else {
        graph_make_in_edges_key(dst, key, &key_len);
    }

    kv_result_t r = kv_get(g->kv, key, key_len, (void**)&old_value, &old_len);
    if (r == KV_NOT_FOUND) {
        return 0;
    }

    size_t edge_count = old_len / sizeof(graph_edge_id_t);
    size_t new_len = 0;
    char *new_value = NULL;

    for (size_t i = 0; i < edge_count; i++) {
        graph_edge_id_t curr;
        memcpy(&curr, old_value + i * sizeof(graph_edge_id_t), sizeof(graph_edge_id_t));
        if (curr == eid) {
            continue;  /* 跳过要删除的边 */
        }

        if (new_value == NULL) {
            new_len = old_len - sizeof(graph_edge_id_t);
            new_value = malloc(new_len > 0 ? new_len : 1);
        }
        memcpy(new_value + new_len, &curr, sizeof(graph_edge_id_t));
        new_len += sizeof(graph_edge_id_t);
    }

    free(old_value);

    if (new_len == 0) {
        kv_delete(g->kv, key, key_len);
    } else {
        kv_put(g->kv, key, key_len, new_value, new_len);
    }
    free(new_value);

    return 0;
}

/* ============================================================
 * 图数据库生命周期
 * ============================================================ */

graph_t *graph_create(const char *path) {
    graph_t *g = calloc(1, sizeof(graph_t));
    if (!g) return NULL;

    g->kv = kv_open(path);
    if (!g->kv) {
        free(g);
        return NULL;
    }

    g->db_path = strdup(path);
    g->next_vertex_id = 1;
    g->next_edge_id = 1;
    g->next_label_id = 1;
    g->next_rel_type_id = 1;

    return g;
}

graph_t *graph_open(const char *path) {
    return graph_create(path);  /* KV 存储自动处理创建/打开 */
}

int graph_close(graph_t *g) {
    if (!g) return -1;

    kv_close(g->kv);
    free(g->db_path);
    free(g);

    return 0;
}

int graph_flush(graph_t *g) {
    if (!g) return -1;
    return kv_flush(g->kv);
}

const char *graph_errmsg(const graph_t *g) {
    return g ? g->error_msg : "NULL graph";
}

/* ============================================================
 * 标签管理
 * ============================================================ */

graph_label_id_t graph_get_or_create_label(graph_t *g, const char *name) {
    char key[128];
    char *value = NULL;
    size_t value_len;

    snprintf(key, sizeof(key), GRAPH_KEY_LABEL "%s", name);
    kv_result_t r = kv_get(g->kv, key, strlen(key), (void**)&value, &value_len);

    if (r == KV_OK) {
        graph_label_id_t id = (graph_label_id_t)atol(value);
        free(value);
        return id;
    }

    /* 创建新标签 */
    graph_label_id_t id = graph_alloc_label_id(g);
    char id_str[16];
    snprintf(id_str, sizeof(id_str), "%u", id);

    kv_put(g->kv, key, strlen(key), id_str, strlen(id_str));

    /* 缓存标签名 */
    char name_key[128];
    snprintf(name_key, sizeof(name_key), GRAPH_KEY_LABEL_ID "%u", id);
    kv_put(g->kv, name_key, strlen(name_key), name, strlen(name));

    return id;
}

/**
 * @brief 获取标签 ID（不创建）
 */
graph_label_id_t graph_get_label(graph_t *g, const char *name) {
    char key[128];
    char *value = NULL;
    size_t value_len;

    snprintf(key, sizeof(key), GRAPH_KEY_LABEL "%s", name);
    kv_result_t r = kv_get(g->kv, key, strlen(key), (void**)&value, &value_len);

    if (r == KV_OK) {
        graph_label_id_t id = (graph_label_id_t)atol(value);
        free(value);
        return id;
    }

    return 0;  /* 不存在 */
}

const char *graph_get_label_name(graph_t *g, graph_label_id_t label_id) {
    static char name[GRAPH_MAX_LABEL_NAME];
    memset(name, 0, sizeof(name));

    char key[128];
    snprintf(key, sizeof(key), GRAPH_KEY_LABEL_ID "%u", label_id);

    char *value = NULL;
    size_t value_len;
    kv_result_t r = kv_get(g->kv, key, strlen(key), (void**)&value, &value_len);

    if (r == KV_OK && value && value_len > 0) {
        size_t copy_len = value_len < sizeof(name) - 1 ? value_len : sizeof(name) - 1;
        memcpy(name, value, copy_len);
        name[copy_len] = '\0';
        free(value);
        return name;
    }

    return NULL;
}

graph_label_id_t graph_get_or_create_rel_type(graph_t *g, const char *name) {
    char key[128];
    char *value = NULL;
    size_t value_len;

    snprintf(key, sizeof(key), GRAPH_KEY_REL_TYPE "%s", name);
    kv_result_t r = kv_get(g->kv, key, strlen(key), (void**)&value, &value_len);

    if (r == KV_OK) {
        graph_label_id_t id = (graph_label_id_t)atol(value);
        free(value);
        return id;
    }

    /* 创建新关系类型 */
    graph_label_id_t id = graph_alloc_rel_type_id(g);
    char id_str[16];
    snprintf(id_str, sizeof(id_str), "%u", id);

    kv_put(g->kv, key, strlen(key), id_str, strlen(id_str));

    /* 缓存类型名 */
    char name_key[128];
    snprintf(name_key, sizeof(name_key), GRAPH_KEY_REL_TYPE_ID "%u", id);
    kv_put(g->kv, name_key, strlen(name_key), name, strlen(name));

    return id;
}

const char *graph_get_rel_type_name(graph_t *g, graph_label_id_t rel_type_id) {
    static char name[GRAPH_MAX_REL_TYPE_NAME];

    char key[128];
    snprintf(key, sizeof(key), GRAPH_KEY_REL_TYPE_ID "%u", rel_type_id);

    char *value = NULL;
    size_t value_len;
    kv_result_t r = kv_get(g->kv, key, strlen(key), (void**)&value, &value_len);

    if (r == KV_OK && value && value_len > 0) {
        size_t copy_len = value_len < sizeof(name) - 1 ? value_len : sizeof(name) - 1;
        memcpy(name, value, copy_len);
        name[copy_len] = '\0';
        free(value);
        return name;
    }

    return NULL;
}

/* ============================================================
 * 顶点操作
 * ============================================================ */

graph_vertex_id_t graph_vertex_create(graph_t *g,
                                      const char *label,
                                      const graph_prop_t *props,
                                      size_t n_props) {
    if (!g) return GRAPH_INVALID_ID;

    /* 分配顶点 ID */
    graph_vertex_id_t vid = graph_alloc_vertex_id(g);
    if (vid == GRAPH_INVALID_ID) return vid;

    /* 创建顶点结构 */
    graph_vertex_t vertex;
    memset(&vertex, 0, sizeof(vertex));
    vertex.id = vid;
    vertex.state = GRAPH_VERTEX_LIVE;
    vertex.n_labels = 0;
    vertex.n_props = n_props > GRAPH_MAX_PROPS ? GRAPH_MAX_PROPS : (uint16_t)n_props;

    /* 添加标签 */
    if (label) {
        graph_label_id_t label_id = graph_get_or_create_label(g, label);
        if (vertex.n_labels < GRAPH_MAX_LABELS) {
            vertex.label_ids[vertex.n_labels++] = label_id;
        }
    }

    /* 复制属性 */
    for (size_t i = 0; i < vertex.n_props; i++) {
        memcpy(&vertex.props[i], &props[i], sizeof(graph_prop_t));
    }

    /* 存储顶点 */
    char key[64];
    size_t key_len;
    graph_make_vertex_key(vid, key, &key_len);

    if (kv_put(g->kv, key, key_len, &vertex, sizeof(vertex)) != KV_OK) {
        graph_set_error(g, "Failed to store vertex");
        return GRAPH_INVALID_ID;
    }

    return vid;
}

int graph_vertex_get(graph_t *g, graph_vertex_id_t vid, graph_vertex_t **out_vertex) {
    if (!g || !out_vertex) return -1;

    char key[64];
    size_t key_len;
    graph_make_vertex_key(vid, key, &key_len);

    char *value = NULL;
    size_t value_len;
    kv_result_t r = kv_get(g->kv, key, key_len, (void**)&value, &value_len);

    if (r == KV_NOT_FOUND) {
        *out_vertex = NULL;
        return -1;
    }

    *out_vertex = malloc(sizeof(graph_vertex_t));
    memcpy(*out_vertex, value, sizeof(graph_vertex_t));
    free(value);

    return 0;
}

int graph_vertex_update(graph_t *g,
                        graph_vertex_id_t vid,
                        const graph_prop_t *props,
                        size_t n_props) {
    if (!g) return -1;

    /* 获取现有顶点 */
    graph_vertex_t *vertex = NULL;
    if (graph_vertex_get(g, vid, &vertex) != 0) {
        return -1;
    }

    /* 更新属性 */
    vertex->n_props = n_props > GRAPH_MAX_PROPS ? GRAPH_MAX_PROPS : (uint16_t)n_props;
    for (size_t i = 0; i < vertex->n_props; i++) {
        memcpy(&vertex->props[i], &props[i], sizeof(graph_prop_t));
    }

    /* 写回 */
    char key[64];
    size_t key_len;
    graph_make_vertex_key(vid, key, &key_len);

    kv_put(g->kv, key, key_len, vertex, sizeof(graph_vertex_t));
    free(vertex);

    return 0;
}

int graph_vertex_delete(graph_t *g, graph_vertex_id_t vid) {
    if (!g) return -1;

    /* 获取顶点 */
    graph_vertex_t *vertex = NULL;
    if (graph_vertex_get(g, vid, &vertex) != 0) {
        return -1;
    }

    /* 标记为已删除 */
    vertex->state = GRAPH_VERTEX_DELETED;

    char key[64];
    size_t key_len;
    graph_make_vertex_key(vid, key, &key_len);
    kv_put(g->kv, key, key_len, vertex, sizeof(graph_vertex_t));

    /* 删除所有关联边 */
    graph_edge_id_t *out_edges = NULL;
    size_t out_count = 0;
    graph_vertex_get_out_edges(g, vid, NULL, &out_edges, &out_count);

    for (size_t i = 0; i < out_count; i++) {
        graph_edge_delete(g, out_edges[i]);
    }
    free(out_edges);

    graph_edge_id_t *in_edges = NULL;
    size_t in_count = 0;
    graph_vertex_get_in_edges(g, vid, NULL, &in_edges, &in_count);

    for (size_t i = 0; i < in_count; i++) {
        graph_edge_delete(g, in_edges[i]);
    }
    free(in_edges);

    free(vertex);
    return 0;
}

bool graph_vertex_exists(graph_t *g, graph_vertex_id_t vid) {
    char key[64];
    size_t key_len;
    graph_make_vertex_key(vid, key, &key_len);

    char *value = NULL;
    size_t value_len;
    kv_result_t r = kv_get(g->kv, key, key_len, (void**)&value, &value_len);

    if (r != KV_OK) {
        return false;
    }

    /* 检查状态是否为 LIVE */
    graph_vertex_t *vertex = (graph_vertex_t *)value;
    bool exists = (vertex->state == GRAPH_VERTEX_LIVE);
    free(value);
    return exists;
}

int graph_vertex_add_label(graph_t *g, graph_vertex_id_t vid, const char *label) {
    if (!g) return -1;

    graph_vertex_t *vertex = NULL;
    if (graph_vertex_get(g, vid, &vertex) != 0) {
        return -1;
    }

    if (vertex->n_labels >= GRAPH_MAX_LABELS) {
        free(vertex);
        graph_set_error(g, "Too many labels");
        return -1;
    }

    graph_label_id_t label_id = graph_get_or_create_label(g, label);
    vertex->label_ids[vertex->n_labels++] = label_id;

    char key[64];
    size_t key_len;
    graph_make_vertex_key(vid, key, &key_len);
    kv_put(g->kv, key, key_len, vertex, sizeof(graph_vertex_t));

    free(vertex);
    return 0;
}

int graph_vertex_get_out_edges(graph_t *g,
                               graph_vertex_id_t vid,
                               const char *rel_type,
                               graph_edge_id_t **out_edges,
                               size_t *out_count) {
    if (!g || !out_edges || !out_count) return -1;

    char key[64];
    size_t key_len;
    graph_make_out_edges_key(vid, key, &key_len);

    char *value = NULL;
    size_t value_len;
    kv_result_t r = kv_get(g->kv, key, key_len, (void**)&value, &value_len);

    if (r == KV_NOT_FOUND) {
        *out_edges = NULL;
        *out_count = 0;
        return 0;
    }

    size_t edge_count = value_len / sizeof(graph_edge_id_t);
    graph_edge_id_t *edges = malloc(sizeof(graph_edge_id_t) * edge_count);

    size_t filtered_count = 0;
    for (size_t i = 0; i < edge_count; i++) {
        graph_edge_id_t eid;
        memcpy(&eid, value + i * sizeof(graph_edge_id_t), sizeof(graph_edge_id_t));

        /* 可选：按关系类型过滤 */
        if (rel_type) {
            graph_edge_t *edge = NULL;
            if (graph_edge_get(g, eid, &edge) == 0) {
                const char *edge_type = graph_get_rel_type_name(g, edge->rel_type_id);
                if (edge_type && strcmp(edge_type, rel_type) == 0) {
                    edges[filtered_count++] = eid;
                }
                free(edge);
            }
        } else {
            edges[filtered_count++] = eid;
        }
    }

    free(value);

    *out_edges = realloc(edges, sizeof(graph_edge_id_t) * filtered_count);
    *out_count = filtered_count;

    return 0;
}

int graph_vertex_get_in_edges(graph_t *g,
                              graph_vertex_id_t vid,
                              const char *rel_type,
                              graph_edge_id_t **out_edges,
                              size_t *out_count) {
    if (!g || !out_edges || !out_count) return -1;

    char key[64];
    size_t key_len;
    graph_make_in_edges_key(vid, key, &key_len);

    char *value = NULL;
    size_t value_len;
    kv_result_t r = kv_get(g->kv, key, key_len, (void**)&value, &value_len);

    if (r == KV_NOT_FOUND) {
        *out_edges = NULL;
        *out_count = 0;
        return 0;
    }

    size_t edge_count = value_len / sizeof(graph_edge_id_t);
    graph_edge_id_t *edges = malloc(sizeof(graph_edge_id_t) * edge_count);

    size_t filtered_count = 0;
    for (size_t i = 0; i < edge_count; i++) {
        graph_edge_id_t eid;
        memcpy(&eid, value + i * sizeof(graph_edge_id_t), sizeof(graph_edge_id_t));

        if (rel_type) {
            graph_edge_t *edge = NULL;
            if (graph_edge_get(g, eid, &edge) == 0) {
                const char *edge_type = graph_get_rel_type_name(g, edge->rel_type_id);
                if (edge_type && strcmp(edge_type, rel_type) == 0) {
                    edges[filtered_count++] = eid;
                }
                free(edge);
            }
        } else {
            edges[filtered_count++] = eid;
        }
    }

    free(value);

    *out_edges = realloc(edges, sizeof(graph_edge_id_t) * filtered_count);
    *out_count = filtered_count;

    return 0;
}

/* ============================================================
 * 边操作
 * ============================================================ */

graph_edge_id_t graph_edge_create(graph_t *g,
                                  graph_vertex_id_t src,
                                  graph_vertex_id_t dst,
                                  const char *rel_type,
                                  const graph_prop_t *props,
                                  size_t n_props) {
    if (!g) return GRAPH_INVALID_ID;

    /* 验证顶点存在 */
    if (!graph_vertex_exists(g, src) || !graph_vertex_exists(g, dst)) {
        graph_set_error(g, "Vertex not found");
        return GRAPH_INVALID_ID;
    }

    /* 分配边 ID */
    graph_edge_id_t eid = graph_alloc_edge_id(g);
    if (eid == GRAPH_INVALID_ID) return eid;

    /* 创建边结构 */
    graph_edge_t edge;
    memset(&edge, 0, sizeof(edge));
    edge.id = eid;
    edge.src_id = src;
    edge.dst_id = dst;
    edge.state = GRAPH_EDGE_LIVE;
    edge.direction = 1;  /* 正向 */

    /* 设置关系类型 */
    if (rel_type) {
        edge.rel_type_id = graph_get_or_create_rel_type(g, rel_type);
    }

    /* 设置属性 */
    edge.n_props = n_props > GRAPH_MAX_PROPS ? GRAPH_MAX_PROPS : (uint16_t)n_props;
    for (size_t i = 0; i < edge.n_props; i++) {
        memcpy(&edge.props[i], &props[i], sizeof(graph_prop_t));
    }

    /* 存储边 */
    char key[64];
    size_t key_len;
    graph_make_edge_key(eid, key, &key_len);

    if (kv_put(g->kv, key, key_len, &edge, sizeof(edge)) != KV_OK) {
        graph_set_error(g, "Failed to store edge");
        return GRAPH_INVALID_ID;
    }

    /* 更新邻接表 */
    if (graph_add_edge_to_adj_list(g, src, dst, eid, true) != 0 ||
        graph_add_edge_to_adj_list(g, src, dst, eid, false) != 0) {
        graph_set_error(g, "Failed to update adjacency list");
        /* 边已存储，邻接表失败不是致命错误 */
    }

    /* 更新顶点度数（可选优化，出错不返回失败） */
    graph_vertex_t *vertex = NULL;
    if (graph_vertex_get(g, src, &vertex) == 0) {
        vertex->out_degree++;
        char vkey[64];
        size_t vkey_len;
        graph_make_vertex_key(src, vkey, &vkey_len);
        kv_put(g->kv, vkey, vkey_len, vertex, sizeof(graph_vertex_t));
        free(vertex);
    }

    if (graph_vertex_get(g, dst, &vertex) == 0) {
        vertex->in_degree++;
        char vkey[64];
        size_t vkey_len;
        graph_make_vertex_key(dst, vkey, &vkey_len);
        kv_put(g->kv, vkey, vkey_len, vertex, sizeof(graph_vertex_t));
        free(vertex);
    }

    return eid;
}

int graph_edge_get(graph_t *g, graph_edge_id_t eid, graph_edge_t **out_edge) {
    if (!g || !out_edge) return -1;

    char key[64];
    size_t key_len;
    graph_make_edge_key(eid, key, &key_len);

    char *value = NULL;
    size_t value_len;
    kv_result_t r = kv_get(g->kv, key, key_len, (void**)&value, &value_len);

    if (r == KV_NOT_FOUND) {
        *out_edge = NULL;
        return -1;
    }

    *out_edge = malloc(sizeof(graph_edge_t));
    memcpy(*out_edge, value, sizeof(graph_edge_t));
    free(value);

    return 0;
}

int graph_edge_update(graph_t *g,
                      graph_edge_id_t eid,
                      const graph_prop_t *props,
                      size_t n_props) {
    if (!g) return -1;

    graph_edge_t *edge = NULL;
    if (graph_edge_get(g, eid, &edge) != 0) {
        return -1;
    }

    edge->n_props = n_props > GRAPH_MAX_PROPS ? GRAPH_MAX_PROPS : (uint16_t)n_props;
    for (size_t i = 0; i < edge->n_props; i++) {
        memcpy(&edge->props[i], &props[i], sizeof(graph_prop_t));
    }

    char key[64];
    size_t key_len;
    graph_make_edge_key(eid, key, &key_len);
    kv_put(g->kv, key, key_len, edge, sizeof(graph_edge_t));
    free(edge);

    return 0;
}

int graph_edge_delete(graph_t *g, graph_edge_id_t eid) {
    if (!g) return -1;

    graph_edge_t *edge = NULL;
    if (graph_edge_get(g, eid, &edge) != 0) {
        return -1;
    }

    /* 从邻接表移除 */
    graph_remove_edge_from_adj_list(g, edge->src_id, edge->dst_id, eid, true);
    graph_remove_edge_from_adj_list(g, edge->src_id, edge->dst_id, eid, false);

    /* 更新顶点度数 */
    graph_vertex_t *vertex = NULL;
    if (graph_vertex_get(g, edge->src_id, &vertex) == 0) {
        if (vertex->out_degree > 0) vertex->out_degree--;
        char vkey[64];
        size_t vkey_len;
        graph_make_vertex_key(edge->src_id, vkey, &vkey_len);
        kv_put(g->kv, vkey, vkey_len, vertex, sizeof(graph_vertex_t));
        free(vertex);
    }

    if (graph_vertex_get(g, edge->dst_id, &vertex) == 0) {
        if (vertex->in_degree > 0) vertex->in_degree--;
        char vkey[64];
        size_t vkey_len;
        graph_make_vertex_key(edge->dst_id, vkey, &vkey_len);
        kv_put(g->kv, vkey, vkey_len, vertex, sizeof(graph_vertex_t));
        free(vertex);
    }

    /* 标记为已删除 */
    edge->state = GRAPH_EDGE_DELETED;
    char key[64];
    size_t key_len;
    graph_make_edge_key(eid, key, &key_len);
    kv_put(g->kv, key, key_len, edge, sizeof(graph_edge_t));

    free(edge);
    return 0;
}

bool graph_edge_exists(graph_t *g, graph_edge_id_t eid) {
    char key[64];
    size_t key_len;
    graph_make_edge_key(eid, key, &key_len);

    char *value = NULL;
    size_t value_len;
    kv_result_t r = kv_get(g->kv, key, key_len, (void**)&value, &value_len);

    if (r != KV_OK) {
        return false;
    }

    /* 检查状态是否为 LIVE */
    graph_edge_t *edge = (graph_edge_t *)value;
    bool exists = (edge->state == GRAPH_EDGE_LIVE);
    free(value);
    return exists;
}

/* ============================================================
 * 图遍历
 * ============================================================ */

size_t graph_scan_vertices(graph_t *g,
                           const char *label,
                           int (*callback)(graph_vertex_id_t, void *),
                           void *ctx) {
    if (!g || !callback) return 0;

    /* 简单实现：扫描所有 gv: 前缀的键 */
    kv_iter_t *iter = kv_scan(g->kv, GRAPH_KEY_VERTEX_PREFIX, strlen(GRAPH_KEY_VERTEX_PREFIX),
                              GRAPH_KEY_VERTEX_PREFIX "~", strlen(GRAPH_KEY_VERTEX_PREFIX "~"));

    if (!iter) return 0;

    size_t count = 0;
    graph_label_id_t label_filter = 0;

    if (label) {
        /* 使用 graph_get_label 只查询，不创建新标签 */
        label_filter = graph_get_label(g, label);
    }

    while (kv_iter_next(iter) == KV_OK) {
        const void *key = kv_iter_key(iter);
        (void)kv_iter_key_len(iter);  /* 暂时未使用 */

        /* 解析顶点 ID */
        graph_vertex_id_t vid = 0;
        sscanf((const char*)key + strlen(GRAPH_KEY_VERTEX_PREFIX), "%lu", (unsigned long*)&vid);

        /* 获取顶点检查标签和状态 */
        graph_vertex_t *vertex = NULL;
        if (graph_vertex_get(g, vid, &vertex) == 0) {
            /* 只处理 LIVE 状态的顶点 */
            if (vertex->state == GRAPH_VERTEX_LIVE) {
                if (label_filter == 0) {
                    /* 没有指定标签，或标签不存在时，返回所有顶点 */
                    if (callback(vid, ctx) == 0) count++;
                } else {
                    /* 检查标签 */
                    bool has_label = false;
                    for (int i = 0; i < vertex->n_labels; i++) {
                        if (vertex->label_ids[i] == label_filter) {
                            has_label = true;
                            break;
                        }
                    }
                    if (has_label && callback(vid, ctx) == 0) {
                        count++;
                    }
                }
            }
            free(vertex);
        }
    }

    kv_iter_free(iter);
    return count;
}

/* ============================================================
 * 统计信息
 * ============================================================ */

int graph_get_stats(graph_t *g, graph_stats_t *stats) {
    if (!g || !stats) return -1;

    memset(stats, 0, sizeof(graph_stats_t));

    /* 统计顶点数（只统计 LIVE 状态） */
    kv_iter_t *v_iter = kv_scan(g->kv, GRAPH_KEY_VERTEX_PREFIX, strlen(GRAPH_KEY_VERTEX_PREFIX),
                                GRAPH_KEY_VERTEX_PREFIX "~", strlen(GRAPH_KEY_VERTEX_PREFIX "~"));
    if (v_iter) {
        while (kv_iter_next(v_iter) == KV_OK) {
            const void *key = kv_iter_key(v_iter);
            (void)kv_iter_key_len(v_iter);

            /* 解析顶点 ID */
            graph_vertex_id_t vid = 0;
            sscanf((const char*)key + strlen(GRAPH_KEY_VERTEX_PREFIX), "%lu", (unsigned long*)&vid);

            /* 检查顶点状态 */
            graph_vertex_t *vertex = NULL;
            if (graph_vertex_get(g, vid, &vertex) == 0) {
                if (vertex->state == GRAPH_VERTEX_LIVE) {
                    stats->num_vertices++;
                }
                free(vertex);
            }
        }
        kv_iter_free(v_iter);
    }

    /* 统计边数（只统计 LIVE 状态） */
    kv_iter_t *e_iter = kv_scan(g->kv, GRAPH_KEY_EDGE_PREFIX, strlen(GRAPH_KEY_EDGE_PREFIX),
                                GRAPH_KEY_EDGE_PREFIX "~", strlen(GRAPH_KEY_EDGE_PREFIX "~"));
    if (e_iter) {
        while (kv_iter_next(e_iter) == KV_OK) {
            const void *key = kv_iter_key(e_iter);
            (void)kv_iter_key_len(e_iter);

            /* 解析边 ID */
            graph_edge_id_t eid = 0;
            sscanf((const char*)key + strlen(GRAPH_KEY_EDGE_PREFIX), "%lu", (unsigned long*)&eid);

            /* 检查边状态 */
            graph_edge_t *edge = NULL;
            if (graph_edge_get(g, eid, &edge) == 0) {
                if (edge->state == GRAPH_EDGE_LIVE) {
                    stats->num_edges++;
                }
                free(edge);
            }
        }
        kv_iter_free(e_iter);
    }

    /* 统计标签数 */
    kv_iter_t *l_iter = kv_scan(g->kv, GRAPH_KEY_LABEL, strlen(GRAPH_KEY_LABEL),
                                GRAPH_KEY_LABEL "~", strlen(GRAPH_KEY_LABEL "~"));
    if (l_iter) {
        while (kv_iter_next(l_iter) == KV_OK) {
            stats->num_labels++;
        }
        kv_iter_free(l_iter);
    }

    /* 统计关系类型数 */
    kv_iter_t *rt_iter = kv_scan(g->kv, GRAPH_KEY_REL_TYPE, strlen(GRAPH_KEY_REL_TYPE),
                                 GRAPH_KEY_REL_TYPE "~", strlen(GRAPH_KEY_REL_TYPE "~"));
    if (rt_iter) {
        while (kv_iter_next(rt_iter) == KV_OK) {
            stats->num_rel_types++;
        }
        kv_iter_free(rt_iter);
    }

    return 0;
}

void *graph_get_kv(graph_t *g) {
    return g ? g->kv : NULL;
}

/* ============================================================
 * 路径结构实现
 * ============================================================ */

graph_path_t *graph_path_create(size_t length) {
    graph_path_t *path = calloc(1, sizeof(graph_path_t));
    if (!path) return NULL;

    path->vertices = calloc(length + 1, sizeof(graph_vertex_id_t));
    path->edges = calloc(length, sizeof(graph_edge_id_t));
    path->length = length;

    if (!path->vertices || !path->edges) {
        free(path->vertices);
        free(path->edges);
        free(path);
        return NULL;
    }

    return path;
}

void graph_path_free(graph_path_t *path) {
    if (!path) return;
    free(path->vertices);
    free(path->edges);
    free(path);
}
