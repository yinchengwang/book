/**
 * @file graph_algorithms.h
 * @brief 图分析算法公共 API
 *
 * 提供常用图分析算法：
 * - Dijkstra 单源最短路径
 * - BFS 无权图最短路径
 * - PageRank 迭代计算
 * - 连通分量检测
 * - 图统计（顶点数、边数、平均度、聚类系数）
 */
#ifndef DB_GRAPH_ALGORITHMS_H
#define DB_GRAPH_ALGORITHMS_H

#include "graph.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 错误码定义
 * ============================================================ */

#define GRAPH_ALGO_OK                0   /**< 成功 */
#define GRAPH_ALGO_ERR_INVALID_PARAM -1  /**< 参数无效 */
#define GRAPH_ALGO_ERR_VERTEX_NOT_FOUND -2 /**< 顶点不存在 */
#define GRAPH_ALGO_ERR_NO_PATH      -3  /**< 路径不存在（不可达） */
#define GRAPH_ALGO_ERR_NO_MEMORY    -4  /**< 内存不足 */

/* ============================================================
 * 结果数据结构
 * ============================================================ */

/**
 * @brief 最短路径结果
 *
 * 存储从源顶点到目标顶点的路径信息。
 * 调用者需要确保 path 数组足够大（至少为目标距离+1）。
 */
typedef struct {
    graph_vertex_id_t *path;    /**< 路径顶点 ID 数组（含首尾） */
    size_t path_length;         /**< 路径长度（顶点数） */
    double total_cost;          /**< 总代价（加权图）或跳数（无权图） */
} graph_path_result_t;

/**
 * @brief PageRank 结果
 *
 * 每个顶点的 PageRank 分数存储在 scores 数组中，
 * 索引与顶点 ID 对应（需要顶点 ID 连续或使用映射）。
 */
typedef struct {
    double *scores;             /**< PageRank 分数数组 */
    size_t num_vertices;        /**< 顶点数量 */
    int iterations;             /**< 实际迭代次数 */
    bool converged;             /**< 是否收敛 */
} graph_pagerank_result_t;

/**
 * @brief 连通分量结果
 */
typedef struct {
    int64_t *component_ids;     /**< 每个顶点所属连通分量 ID */
    size_t num_vertices;        /**< 顶点数量 */
    size_t num_components;      /**< 连通分量数量 */
} graph_component_result_t;

/**
 * @brief 图统计结果
 */
typedef struct {
    size_t num_vertices;        /**< 顶点数 */
    size_t num_edges;           /**< 边数 */
    double avg_degree;          /**< 平均度 */
    double clustering_coefficient; /**< 全局聚类系数 */
} graph_stats_result_t;

/* ============================================================
 * 最短路径算法
 * ============================================================ */

/**
 * @brief Dijkstra 单源最短路径
 *
 * 计算从源顶点到目标顶点的最短路径（带权图）。
 * 时间复杂度：O((V+E) log V)（使用最小堆优化）
 *
 * @param graph 图数据库
 * @param source 源顶点 ID
 * @param target 目标顶点 ID
 * @param result 结果（调用者分配，path 需要足够大小）
 * @return GRAPH_ALGO_OK 成功，其他表示错误
 *
 * @note 要求边权重 ≥ 0，否则行为未定义
 * @note path 数组大小应至少为 max_path_length + 1
 */
int graph_dijkstra_algo(const graph_t *graph,
                   graph_vertex_id_t source,
                   graph_vertex_id_t target,
                   graph_path_result_t *result);

/**
 * @brief BFS 无权图最短路径
 *
 * 计算无权图（或等权图）中从源顶点到目标顶点的最短路径。
 * 时间复杂度：O(V+E)
 *
 * @param graph 图数据库
 * @param source 源顶点 ID
 * @param target 目标顶点 ID
 * @param result 结果（调用者分配，path 需要足够大小）
 * @return GRAPH_ALGO_OK 成功，其他表示错误
 */
int graph_bfs_shortest_path(const graph_t *graph,
                            graph_vertex_id_t source,
                            graph_vertex_id_t target,
                            graph_path_result_t *result);

/* ============================================================
 * 遍历算法
 * ============================================================ */

/**
 * @brief DFS 深度优先遍历回调
 *
 * @param vid 当前顶点 ID
 * @param depth 当前深度
 * @param user_data 用户数据
 * @return 0 继续遍历，非 0 停止遍历
 */
typedef int (*graph_dfs_visitor_fn)(graph_vertex_id_t vid,
                                    size_t depth,
                                    void *user_data);

/**
 * @brief DFS 深度优先遍历
 *
 * @param graph 图数据库
 * @param start 起始顶点 ID
 * @param visitor 访问回调函数
 * @param user_data 传递给回调的用户数据
 * @return GRAPH_ALGO_OK 成功，其他表示错误
 */
int graph_dfs_algo(const graph_t *graph,
              graph_vertex_id_t start,
              graph_dfs_visitor_fn visitor,
              void *user_data);

/**
 * @brief BFS 广度优先遍历回调
 *
 * @param vid 当前顶点 ID
 * @param depth 当前深度（距离起始顶点的跳数）
 * @param user_data 用户数据
 * @return 0 继续遍历，非 0 停止遍历
 */
typedef int (*graph_bfs_visitor_fn)(graph_vertex_id_t vid,
                                    size_t depth,
                                    void *user_data);

/**
 * @brief BFS 广度优先遍历
 *
 * @param graph 图数据库
 * @param start 起始顶点 ID
 * @param visitor 访问回调函数
 * @param user_data 传递给回调的用户数据
 * @return GRAPH_ALGO_OK 成功，其他表示错误
 */
int graph_bfs_algo(const graph_t *graph,
              graph_vertex_id_t start,
              graph_bfs_visitor_fn visitor,
              void *user_data);

/* ============================================================
 * PageRank 算法
 * ============================================================ */

/**
 * @brief PageRank 迭代计算
 *
 * 计算图中每个顶点的 PageRank 分数。
 * 公式：PR(v) = (1-d)/N + d * Σ(PR(u)/deg(u))
 *
 * @param graph 图数据库
 * @param result 结果（调用者分配，scores 数组需足够大）
 * @param max_iterations 最大迭代次数（0 表示默认 100）
 * @param damping_factor 阻尼因子（0 表示默认 0.85）
 * @param tolerance 收敛阈值（0 表示默认 1e-6）
 * @return GRAPH_ALGO_OK 成功，其他表示错误
 *
 * @note scores 数组大小应至少为最大顶点 ID + 1
 * @note 顶点 ID 需连续（0, 1, 2, ...），否则需要外部映射
 */
int graph_pagerank_new(const graph_t *graph,
                   graph_pagerank_result_t *result,
                   int max_iterations,
                   double damping_factor,
                   double tolerance);

/* ============================================================
 * 连通分量
 * ============================================================ */

/**
 * @brief 连通分量检测
 *
 * 检测图中所有连通分量（无向图）。
 *
 * @param graph 图数据库
 * @param result 结果（调用者分配）
 * @return GRAPH_ALGO_OK 成功，其他表示错误
 *
 * @note component_ids 数组大小应至少为最大顶点 ID + 1
 * @note 仅适用于无向图，有向图请使用强连通分量算法
 */
int graph_connected_components(const graph_t *graph,
                               graph_component_result_t *result);

/* ============================================================
 * 图统计
 * ============================================================ */

/**
 * @brief 图统计信息
 *
 * 计算图的基本统计信息。
 *
 * @param graph 图数据库
 * @param result 结果（调用者分配）
 * @return GRAPH_ALGO_OK 成功，其他表示错误
 */
int graph_stats(const graph_t *graph,
                graph_stats_result_t *result);

/* ============================================================
 * 结果释放函数
 * ============================================================ */

/**
 * @brief 释放路径结果
 *
 * 由于路径结果的 path 数组由调用者分配，此函数不释放 path。
 * 仅重置结构体字段。
 */
void graph_path_result_clear(graph_path_result_t *result);

/**
 * @brief 释放 PageRank 结果
 *
 * 释放 scores 数组内存。
 */
void graph_pagerank_result_clear(graph_pagerank_result_t *result);

/**
 * @brief 释放连通分量结果
 *
 * 释放 component_ids 数组内存。
 */
void graph_component_result_clear(graph_component_result_t *result);

/**
 * @brief 释放图统计结果
 *
 * 统计结果不包含动态分配的内存，此函数仅重置字段。
 */
void graph_stats_result_clear(graph_stats_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* DB_GRAPH_ALGORITHMS_H */