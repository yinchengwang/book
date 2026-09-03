/**
 * @file traverse.h
 * @brief 图遍历引擎
 *
 * 支持 BFS、DFS 遍历，以及图算法（最短路径、PageRank）
 */
#ifndef DB_GRAPH_TRAVERSE_H
#define DB_GRAPH_TRAVERSE_H

#include "../../../graph/types.h"
#include "../../../graph/graph.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 遍历模式
 * ============================================================ */

/** 遍历模式 */
typedef enum graph_traverse_mode_e {
    GRAPH_TRAVERSE_BFS = 0,       /**< 广度优先搜索 */
    GRAPH_TRAVERSE_DFS = 1        /**< 深度优先搜索 */
} graph_traverse_mode_t;

/** 遍历方向 */
typedef enum graph_traverse_direction_e {
    GRAPH_TRAVERSE_OUT = 0,       /**< 出边 */
    GRAPH_TRAVERSE_IN = 1,        /**< 入边 */
    GRAPH_TRAVERSE_BOTH = 2       /**< 双向 */
} graph_traverse_direction_t;

/* ============================================================
 * 遍历配置
 * ============================================================ */

/** 遍历配置 */
typedef struct graph_traverse_config_s {
    graph_traverse_mode_t     mode;           /**< 遍历模式 */
    graph_traverse_direction_t direction;     /**< 遍历方向 */
    graph_label_id_t          label_filter;   /**< 顶点标签过滤（0=不过滤） */
    graph_label_id_t          rel_type_filter; /**< 边类型过滤（0=不过滤） */
    int                       max_depth;      /**< 最大深度（-1=无限制） */
    int                       min_depth;      /**< 最小深度 */
    bool                      visited_set;    /**< 是否使用 visited 集合 */
} graph_traverse_config_t;

/* ============================================================
 * 遍历器
 * ============================================================ */

/** 遍历器 */
typedef struct graph_traverser_s graph_traverser_t;

/**
 * @brief 创建遍历器
 * @param g 图数据库
 * @param config 遍历配置
 * @return 遍历器，失败返回 NULL
 */
graph_traverser_t *graph_traverser_create(graph_t *g,
                                          const graph_traverse_config_t *config);

/**
 * @brief 销毁遍历器
 * @param t 遍历器
 */
void graph_traverser_destroy(graph_traverser_t *t);

/**
 * @brief 从起始顶点开始遍历
 * @param t 遍历器
 * @param start 起始顶点 ID
 */
void graph_traverser_start(graph_traverser_t *t, graph_vertex_id_t start);

/**
 * @brief 获取下一个顶点
 * @param t 遍历器
 * @param out_vid 输出顶点 ID
 * @param out_depth 输出深度
 * @return true 有更多顶点，false 遍历完成
 */
bool graph_traverser_next(graph_traverser_t *t,
                          graph_vertex_id_t *out_vid,
                          int *out_depth);

/**
 * @brief 检查遍历是否完成
 * @param t 遍历器
 * @return true 完成，false 继续
 */
bool graph_traverser_done(const graph_traverser_t *t);

/**
 * @brief 获取已访问顶点数
 * @param t 遍历器
 * @return 已访问顶点数
 */
size_t graph_traverser_visited_count(const graph_traverser_t *t);

/* ============================================================
 * 便捷遍历函数
 * ============================================================ */

/**
 * @brief BFS 遍历
 * @param g 图数据库
 * @param start 起始顶点
 * @param max_depth 最大深度
 * @param callback 回调（返回 true 继续，false 停止）
 * @param ctx 回调上下文
 * @return 访问的顶点数
 */
size_t graph_bfs(graph_t *g,
                 graph_vertex_id_t start,
                 int max_depth,
                 bool (*callback)(graph_vertex_id_t, int depth, void *),
                 void *ctx);

/**
 * @brief DFS 遍历
 * @param g 图数据库
 * @param start 起始顶点
 * @param max_depth 最大深度
 * @param callback 回调
 * @param ctx 回调上下文
 * @return 访问的顶点数
 */
size_t graph_dfs(graph_t *g,
                 graph_vertex_id_t start,
                 int max_depth,
                 bool (*callback)(graph_vertex_id_t, int depth, void *),
                 void *ctx);

/* ============================================================
 * 最短路径算法
 * ============================================================ */

/**
 * @brief 最短路径算法类型
 */
typedef enum graph_ssp_algo_e {
    GRAPH_SSP_BFS = 0,            /**< BFS（无权图） */
    GRAPH_SSP_DIJKSTRA = 1        /**< Dijkstra（加权图） */
} graph_ssp_algo_t;

/**
 * @brief 边权重回调
 * @param edge 边 ID
 * @param ctx 上下文
 * @return 边权重
 */
typedef double (*graph_edge_weight_fn)(graph_edge_id_t edge, void *ctx);

/**
 * @brief 计算最短路径（无权图，BFS）
 * @param g 图数据库
 * @param src 源顶点
 * @param dst 目标顶点
 * @param out_path 输出路径（调用者负责释放）
 * @return 路径长度（跳数），-1 表示无路径
 */
int graph_shortest_path(graph_t *g,
                        graph_vertex_id_t src,
                        graph_vertex_id_t dst,
                        graph_path_t **out_path);

/**
 * @brief 计算加权最短路径（Dijkstra）
 * @param g 图数据库
 * @param src 源顶点
 * @param dst 目标顶点
 * @param weight_fn 边权重函数（可为 NULL，默认权重 1.0）
 * @param ctx 上下文
 * @param out_path 输出路径（调用者负责释放）
 * @return 路径权重，-1.0 表示无路径
 */
double graph_dijkstra(graph_t *g,
                      graph_vertex_id_t src,
                      graph_vertex_id_t dst,
                      graph_edge_weight_fn weight_fn,
                      void *ctx,
                      graph_path_t **out_path);

/**
 * @brief 计算所有最短路径
 * @param g 图数据库
 * @param src 源顶点
 * @param out_paths 输出路径数组（调用者负责释放）
 * @param out_count 输出路径数量
 * @return 0 成功
 */
int graph_all_shortest_paths(graph_t *g,
                             graph_vertex_id_t src,
                             graph_path_t ***out_paths,
                             size_t *out_count);

/* ============================================================
 * PageRank 算法
 * ============================================================ */

/** PageRank 配置 */
typedef struct graph_pagerank_config_s {
    double  damping_factor;   /**< 阻尼因子（默认 0.85） */
    int     max_iterations;   /**< 最大迭代次数（默认 100） */
    double  tolerance;        /**< 收敛阈值（默认 1e-6） */
} graph_pagerank_config_t;

/** PageRank 结果 */
typedef struct graph_pagerank_result_s {
    graph_vertex_id_t *vertices;    /**< 顶点数组 */
    double            *scores;      /**< PageRank 分数数组 */
    size_t            count;        /**< 顶点数 */
} graph_pagerank_result_t;

/**
 * @brief 计算 PageRank
 * @param g 图数据库
 * @param config 配置（可为 NULL，使用默认值）
 * @param out_result 输出结果（调用者负责释放）
 * @return 0 成功，-1 失败
 */
int graph_pagerank(graph_t *g,
                   const graph_pagerank_config_t *config,
                   graph_pagerank_result_t **out_result);

/**
 * @brief 释放 PageRank 结果
 * @param result PageRank 结果
 */
void graph_pagerank_free(graph_pagerank_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* DB_GRAPH_TRAVERSE_H */
