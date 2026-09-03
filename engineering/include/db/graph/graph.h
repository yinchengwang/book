/**
 * @file graph.h
 * @brief 图数据库主接口
 *
 * 提供图数据库的创建、打开、关闭以及顶点/边操作接口
 */
#ifndef DB_GRAPH_H
#define DB_GRAPH_H

#include "types.h"
#include "../kv.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct graph_s graph_t;

/* ============================================================
 * 图数据库配置
 * ============================================================ */

/** 图数据库配置 */
typedef struct graph_config_s {
    const char  *db_path;        /**< 数据库路径 */
    int         buffer_size;     /**< Buffer Pool 大小（0=默认） */
    bool        enable_wal;      /**< 是否启用 WAL */
} graph_config_t;

/* ============================================================
 * 图数据库生命周期
 * ============================================================ */

/**
 * @brief 创建图数据库
 * @param path 数据库路径
 * @return 图数据库句柄，失败返回 NULL
 */
graph_t *graph_create(const char *path);

/**
 * @brief 打开已有图数据库
 * @param path 数据库路径
 * @return 图数据库句柄，失败返回 NULL
 */
graph_t *graph_open(const char *path);

/**
 * @brief 关闭图数据库
 * @param g 图数据库
 * @return 0 成功
 */
int graph_close(graph_t *g);

/**
 * @brief 刷脏页到磁盘
 * @param g 图数据库
 * @return 0 成功
 */
int graph_flush(graph_t *g);

/**
 * @brief 获取错误信息
 * @param g 图数据库
 * @return 错误信息
 */
const char *graph_errmsg(const graph_t *g);

/* ============================================================
 * 标签管理
 * ============================================================ */

/**
 * @brief 获取或创建标签
 * @param g 图数据库
 * @param name 标签名
 * @return 标签 ID，不存在则创建
 */
graph_label_id_t graph_get_or_create_label(graph_t *g, const char *name);

/**
 * @brief 获取标签 ID（不创建）
 * @param g 图数据库
 * @param name 标签名
 * @return 标签 ID，不存在返回 0
 */
graph_label_id_t graph_get_label(graph_t *g, const char *name);

/**
 * @brief 获取标签名
 * @param g 图数据库
 * @param label_id 标签 ID
 * @return 标签名，不存在返回 NULL
 */
const char *graph_get_label_name(graph_t *g, graph_label_id_t label_id);

/**
 * @brief 获取关系类型 ID
 * @param g 图数据库
 * @param name 关系类型名
 * @return 类型 ID，不存在则创建
 */
graph_label_id_t graph_get_or_create_rel_type(graph_t *g, const char *name);

/**
 * @brief 获取关系类型名
 * @param g 图数据库
 * @param rel_type_id 类型 ID
 * @return 类型名，不存在返回 NULL
 */
const char *graph_get_rel_type_name(graph_t *g, graph_label_id_t rel_type_id);

/* ============================================================
 * 顶点操作
 * ============================================================ */

/**
 * @brief 创建顶点
 * @param g 图数据库
 * @param label 标签名
 * @param props 属性数组（可为 NULL）
 * @param n_props 属性数量
 * @return 顶点 ID，失败返回 GRAPH_INVALID_ID
 */
graph_vertex_id_t graph_vertex_create(graph_t *g,
                                      const char *label,
                                      const graph_prop_t *props,
                                      size_t n_props);

/**
 * @brief 获取顶点
 * @param g 图数据库
 * @param vid 顶点 ID
 * @param out_vertex 输出顶点（调用者负责释放）
 * @return 0 成功，-1 失败
 */
int graph_vertex_get(graph_t *g, graph_vertex_id_t vid, graph_vertex_t **out_vertex);

/**
 * @brief 更新顶点属性
 * @param g 图数据库
 * @param vid 顶点 ID
 * @param props 新属性
 * @param n_props 属性数量
 * @return 0 成功，-1 失败
 */
int graph_vertex_update(graph_t *g,
                        graph_vertex_id_t vid,
                        const graph_prop_t *props,
                        size_t n_props);

/**
 * @brief 删除顶点（同时删除所有关联边）
 * @param g 图数据库
 * @param vid 顶点 ID
 * @return 0 成功，-1 失败
 */
int graph_vertex_delete(graph_t *g, graph_vertex_id_t vid);

/**
 * @brief 检查顶点是否存在
 * @param g 图数据库
 * @param vid 顶点 ID
 * @return true 存在，false 不存在
 */
bool graph_vertex_exists(graph_t *g, graph_vertex_id_t vid);

/**
 * @brief 为顶点添加标签
 * @param g 图数据库
 * @param vid 顶点 ID
 * @param label 标签名
 * @return 0 成功，-1 失败
 */
int graph_vertex_add_label(graph_t *g, graph_vertex_id_t vid, const char *label);

/**
 * @brief 获取顶点的出边
 * @param g 图数据库
 * @param vid 顶点 ID
 * @param rel_type 关系类型（NULL 表示所有类型）
 * @param out_edges 输出边 ID 数组（调用者负责释放）
 * @param out_count 输出边数量
 * @return 0 成功，-1 失败
 */
int graph_vertex_get_out_edges(graph_t *g,
                               graph_vertex_id_t vid,
                               const char *rel_type,
                               graph_edge_id_t **out_edges,
                               size_t *out_count);

/**
 * @brief 获取顶点的入边
 * @param g 图数据库
 * @param vid 顶点 ID
 * @param rel_type 关系类型（NULL 表示所有类型）
 * @param out_edges 输出边 ID 数组（调用者负责释放）
 * @param out_count 输出边数量
 * @return 0 成功，-1 失败
 */
int graph_vertex_get_in_edges(graph_t *g,
                              graph_vertex_id_t vid,
                              const char *rel_type,
                              graph_edge_id_t **out_edges,
                              size_t *out_count);

/* ============================================================
 * 边操作
 * ============================================================ */

/**
 * @brief 创建边
 * @param g 图数据库
 * @param src 源顶点 ID
 * @param dst 目标顶点 ID
 * @param rel_type 关系类型名
 * @param props 属性（可为 NULL）
 * @param n_props 属性数量
 * @return 边 ID，失败返回 GRAPH_INVALID_ID
 */
graph_edge_id_t graph_edge_create(graph_t *g,
                                  graph_vertex_id_t src,
                                  graph_vertex_id_t dst,
                                  const char *rel_type,
                                  const graph_prop_t *props,
                                  size_t n_props);

/**
 * @brief 获取边
 * @param g 图数据库
 * @param eid 边 ID
 * @param out_edge 输出边（调用者负责释放）
 * @return 0 成功，-1 失败
 */
int graph_edge_get(graph_t *g, graph_edge_id_t eid, graph_edge_t **out_edge);

/**
 * @brief 更新边属性
 * @param g 图数据库
 * @param eid 边 ID
 * @param props 新属性
 * @param n_props 属性数量
 * @return 0 成功，-1 失败
 */
int graph_edge_update(graph_t *g,
                      graph_edge_id_t eid,
                      const graph_prop_t *props,
                      size_t n_props);

/**
 * @brief 删除边
 * @param g 图数据库
 * @param eid 边 ID
 * @return 0 成功，-1 失败
 */
int graph_edge_delete(graph_t *g, graph_edge_id_t eid);

/**
 * @brief 检查边是否存在
 * @param g 图数据库
 * @param eid 边 ID
 * @return true 存在，false 不存在
 */
bool graph_edge_exists(graph_t *g, graph_edge_id_t eid);

/* ============================================================
 * 图遍历（低级 API）
 * ============================================================ */

/**
 * @brief 遍历所有顶点
 * @param g 图数据库
 * @param label 标签（NULL 表示所有标签）
 * @param callback 回调函数
 * @param ctx 回调上下文
 * @return 遍历的顶点数
 */
size_t graph_scan_vertices(graph_t *g,
                           const char *label,
                           int (*callback)(graph_vertex_id_t, void *),
                           void *ctx);

/* ============================================================
 * 统计信息
 * ============================================================ */

/** 图统计信息 */
typedef struct graph_stats_s {
    uint64_t    num_vertices;     /**< 顶点数 */
    uint64_t    num_edges;        /**< 边数 */
    uint64_t    num_labels;       /**< 标签数 */
    uint64_t    num_rel_types;    /**< 关系类型数 */
} graph_stats_t;

/**
 * @brief 获取图统计信息
 * @param g 图数据库
 * @param stats 输出统计
 * @return 0 成功
 */
int graph_get_stats(graph_t *g, graph_stats_t *stats);

/* ============================================================
 * 底层 KV 操作（供内部模块使用）
 * ============================================================ */

/**
 * @brief 获取底层 KV 存储（供索引等模块使用）
 * @param g 图数据库
 * @return KV 存储句柄
 */
void *graph_get_kv(graph_t *g);

#ifdef __cplusplus
}
#endif

#endif /* DB_GRAPH_H */
