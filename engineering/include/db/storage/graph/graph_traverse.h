/**
 * @file graph_traverse.h
 * @brief 图遍历算法包装层
 *
 * 封装 db/executor/graph/traverse/ 中的遍历算法，
 * 为图引擎提供高级遍历接口。
 */
#ifndef DB_STORAGE_GRAPH_TRAVERSE_H
#define DB_STORAGE_GRAPH_TRAVERSE_H

#include "graph_engine.h"
#include "db/executor/graph/traverse/traverse.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 遍历方向定义
 * ======================================================================== */

/** 遍历方向（复用 traverse.h 中的定义） */
/* 使用 traverse.h 中定义的 graph_traverse_direction_t */

/* ========================================================================
 * 邻居查询结果
 * ======================================================================== */

/** 邻居查询结果 */
typedef struct graph_neighbors_s {
    graph_vertex_id_t *vertices;  /**< 邻居顶点数组 */
    size_t count;                 /**< 邻居数量 */
    size_t capacity;              /**< 容量 */
} graph_neighbors_t;

/* ========================================================================
 * BFS 遍历结果
 * ======================================================================== */

/** BFS 遍历结果 */
typedef struct graph_engine_bfs_result_s {
    graph_vertex_id_t *vertices;  /**< 遍历到的顶点数组 */
    int *depths;                  /**< 对应深度 */
    size_t count;                 /**< 顶点数 */
    size_t capacity;              /**< 容量（内部使用） */
} graph_engine_bfs_result_t;

/* ========================================================================
 * 路径查询结果
 * ======================================================================== */

/** 路径查询结果 */
typedef struct graph_path_result_s {
    graph_vertex_id_t *vertices;  /**< 路径顶点数组 */
    size_t *edge_ids;             /**< 路径边数组 */
    size_t length;                /**< 路径长度（跳数） */
    double weight;                /**< 路径权重（Dijkstra 时有效） */
} graph_path_result_t;

/* ========================================================================
 * API 声明
 * ======================================================================== */

/**
 * @brief 获取图的邻居顶点
 * @param rel 图引擎句柄
 * @param vid 顶点 ID
 * @param direction 遍历方向
 * @param out 结果输出
 * @return 0 成功，-1 失败
 */
int graph_get_neighbors(void *rel, graph_vertex_id_t vid,
                        graph_traverse_direction_t direction,
                        graph_neighbors_t *out);

/**
 * @brief 释放邻居查询结果
 * @param result 邻居查询结果
 */
void graph_neighbors_free(graph_neighbors_t *result);

/**
 * @brief BFS 遍历
 * @param rel 图引擎句柄
 * @param start 起始顶点
 * @param max_depth 最大深度（-1 表示无限制）
 * @param out 输出结果（调用者负责释放）
 * @return 0 成功，-1 失败
 */
int graph_engine_bfs(void *rel, graph_vertex_id_t start, int max_depth,
                     graph_engine_bfs_result_t *out);

/**
 * @brief 释放 BFS 结果
 * @param result BFS 结果
 */
void graph_engine_bfs_free(graph_engine_bfs_result_t *result);

/**
 * @brief 计算无权最短路径（BFS）
 * @param rel 图引擎句柄
 * @param src 源顶点
 * @param dst 目标顶点
 * @param out 输出路径（调用者负责释放）
 * @return 路径长度，-1 表示无路径
 */
int graph_engine_shortest_path(void *rel, graph_vertex_id_t src,
                               graph_vertex_id_t dst,
                               graph_path_result_t *out);

/**
 * @brief 计算加权最短路径（Dijkstra）
 * @param rel 图引擎句柄
 * @param src 源顶点
 * @param dst 目标顶点
 * @param weight_fn 边权重函数（可为 NULL，默认权重 1.0）
 * @param ctx 权重函数上下文
 * @param out 输出路径（调用者负责释放）
 * @return 路径权重，-1.0 表示无路径
 */
double graph_dijkstra_path(void *rel, graph_vertex_id_t src,
                           graph_vertex_id_t dst,
                           double (*weight_fn)(graph_edge_id_t, void *),
                           void *ctx,
                           graph_path_result_t *out);

/**
 * @brief 释放路径结果
 * @param result 路径结果
 */
void graph_path_result_free(graph_path_result_t *result);

/**
 * @brief PageRank 计算结果（引擎层）
 */
typedef struct graph_engine_pagerank_result_s {
    graph_vertex_id_t *vertices;  /**< 顶点数组 */
    double *scores;               /**< PageRank 分数 */
    size_t count;                 /**< 顶点数 */
} graph_engine_pagerank_result_t;

/**
 * @brief 计算 PageRank
 * @param rel 图引擎句柄
 * @param damping 阻尼因子（默认 0.85）
 * @param max_iter 最大迭代次数（默认 100）
 * @param tolerance 收敛阈值（默认 1e-6）
 * @param out 输出结果（调用者负责释放）
 * @return 0 成功，-1 失败
 */
int graph_engine_pagerank(void *rel, double damping, int max_iter,
                          double tolerance,
                          graph_engine_pagerank_result_t *out);

/**
 * @brief 释放 PageRank 结果
 * @param result PageRank 结果
 */
void graph_engine_pagerank_free(graph_engine_pagerank_result_t *result);

/**
 * @brief 获取顶点的出边数量
 * @param rel 图引擎句柄
 * @param vid 顶点 ID
 * @return 出边数量，-1 表示顶点不存在
 */
int64_t graph_get_out_degree(void *rel, graph_vertex_id_t vid);

/**
 * @brief 获取顶点的入边数量
 * @param rel 图引擎句柄
 * @param vid 顶点 ID
 * @return 入边数量，-1 表示顶点不存在
 */
int64_t graph_get_in_degree(void *rel, graph_vertex_id_t vid);

/* ========================================================================
 * 迭代器式遍历（用于大数据集）
 * ======================================================================== */

/** 遍历迭代器 */
typedef struct graph_traverse_iter_s graph_traverse_iter_t;

/**
 * @brief 创建遍历迭代器
 * @param rel 图引擎句柄
 * @param start 起始顶点
 * @param direction 遍历方向
 * @param max_depth 最大深度（-1 表示无限制）
 * @return 迭代器，失败返回 NULL
 */
graph_traverse_iter_t *graph_traverse_iter_create(void *rel,
                                                  graph_vertex_id_t start,
                                                  graph_traverse_direction_t direction,
                                                  int max_depth);

/**
 * @brief 获取下一个顶点
 * @param iter 迭代器
 * @param out_vid 输出顶点 ID
 * @param out_depth 输出深度
 * @return true 有更多顶点，false 遍历完成
 */
bool graph_traverse_iter_next(graph_traverse_iter_t *iter,
                              graph_vertex_id_t *out_vid,
                              int *out_depth);

/**
 * @brief 销毁遍历迭代器
 * @param iter 迭代器
 */
void graph_traverse_iter_destroy(graph_traverse_iter_t *iter);

#ifdef __cplusplus
}
#endif

#endif /* DB_STORAGE_GRAPH_TRAVERSE_H */
