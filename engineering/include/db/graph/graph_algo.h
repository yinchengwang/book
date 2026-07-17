/**
 * @file graph_algo.h
 * @brief 图算法库头文件
 *
 * 实现 BFS/DFS、Dijkstra、PageRank、Louvain 等图算法
 */
#ifndef DB_GRAPH_ALGO_H
#define DB_GRAPH_ALGO_H

#include "db/graph/graph.h"
#include "db/graph/graph_engine.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 图遍历算法
 * ======================================================================== */

/** 遍历模式 */
typedef enum GraphTraverseMode_e {
    GRAPH_TRAVERSE_BFS,          /**< 广度优先搜索 */
    GRAPH_TRAVERSE_DFS           /**< 深度优先搜索 */
} GraphTraverseMode;

/** 遍历状态 */
typedef enum GraphTraverseState_e {
    GRAPH_TRAVERSE_STATE_INIT,
    GRAPH_TRAVERSE_STATE_RUNNING,
    GRAPH_TRAVERSE_STATE_DONE
} GraphTraverseState;

/** 遍历选项 */
typedef struct GraphTraverseOptions_s {
    GraphTraverseMode mode;       /**< 遍历模式 */
    int max_depth;              /**< 最大深度，-1 表示不限制 */
    size_t max_nodes;           /**< 最大访问节点数 */
    graph_label_id_t *node_labels; /**< 节点标签过滤 */
    size_t num_node_labels;
    graph_label_id_t *rel_types;  /**< 关系类型过滤 */
    size_t num_rel_types;
    bool directed;              /**< 是否按方向遍历 */
    bool include_start;        /**< 是否包含起始节点 */
} GraphTraverseOptions;

/** 遍历结果项 */
typedef struct GraphTraverseResultItem_s {
    graph_vertex_id_t vid;      /**< 顶点 ID */
    graph_vertex_id_t parent_vid; /**< 父节点 ID */
    graph_edge_id_t via_edge;    /**< 经过的边 */
    int depth;                  /**< 深度 */
    double distance;            /**< 距离（用于加权图） */
} GraphTraverseResultItem;

/** 遍历迭代器 */
typedef struct GraphTraverseIter_s {
    void *graph;                /**< 图数据库 */
    GraphTraverseOptions options;
    GraphTraverseState state;

    /* BFS 队列 */
    graph_vertex_id_t *queue;
    size_t queue_front;
    size_t queue_back;
    size_t queue_capacity;

    /* DFS 栈 */
    graph_vertex_id_t *stack;
    size_t stack_top;
    size_t stack_capacity;

    /* 已访问标记 */
    bool *visited;
    size_t visited_capacity;

    /* 结果缓冲 */
    GraphTraverseResultItem *results;
    size_t num_results;
    size_t result_capacity;

    /* 当前迭代状态 */
    graph_vertex_id_t current_vid;
    int current_depth;
} GraphTraverseIter;

/**
 * @brief 创建遍历迭代器
 */
GraphTraverseIter *graph_traverse_create(void *graph,
                                        graph_vertex_id_t start_vid,
                                        const GraphTraverseOptions *options);

/**
 * @brief 销毁遍历迭代器
 */
void graph_traverse_destroy(GraphTraverseIter *iter);

/**
 * @brief 获取下一个遍历结果
 * @return NULL 表示遍历完成
 */
GraphTraverseResultItem *graph_traverse_next(GraphTraverseIter *iter);

/**
 * @brief 重置迭代器
 */
void graph_traverse_reset(GraphTraverseIter *iter, graph_vertex_id_t new_start_vid);

/**
 * @brief 获取所有遍历结果
 */
GraphTraverseResultItem *graph_traverse_collect(GraphTraverseIter *iter,
                                                size_t *out_count);

/* ========================================================================
 * Dijkstra 最短路径
 * ======================================================================== */

/** 路径结果 */
typedef struct GraphPath_s {
    graph_vertex_id_t *vertices;  /**< 路径顶点数组 */
    graph_edge_id_t *edges;      /**< 路径边数组（可选） */
    size_t length;              /**< 路径长度（顶点数） */
    double total_weight;        /**< 总权重 */
} GraphPath;

/** Dijkstra 选项 */
typedef struct GraphDijkstraOptions_s {
    double *edge_weights;       /**< 边权重数组，NULL 表示单位权重 */
    bool directed;              /**< 是否按方向遍历 */
    double max_distance;        /**< 最大距离，-1 表示不限制 */
    size_t max_paths;           /**< 最大路径数 */
    graph_label_id_t *rel_types; /**< 关系类型过滤 */
    size_t num_rel_types;
} GraphDijkstraOptions;

/**
 * @brief 计算单源最短路径
 * @param graph 图数据库
 * @param src 源顶点 ID
 * @param dst 目标顶点 ID（GRAPH_INVALID_ID 表示到所有顶点）
 * @param options 选项
 * @return 最短路径，失败返回 NULL
 */
GraphPath *graph_dijkstra(void *graph,
                         graph_vertex_id_t src,
                         graph_vertex_id_t dst,
                         const GraphDijkstraOptions *options);

/**
 * @brief 计算 K 短路径
 * @param graph 图数据库
 * @param src 源顶点 ID
 * @param dst 目标顶点 ID
 * @param k 返回前 K 条最短路径
 * @param options 选项
 * @return 路径数组，失败返回 NULL
 */
GraphPath **graph_k_shortest_paths(void *graph,
                                  graph_vertex_id_t src,
                                  graph_vertex_id_t dst,
                                  size_t k,
                                  const GraphDijkstraOptions *options,
                                  size_t *out_count);

/**
 * @brief 释放路径
 */
void graph_path_free(GraphPath *path);

/**
 * @brief 释放路径数组
 */
void graph_paths_free(GraphPath **paths, size_t count);

/* ========================================================================
 * PageRank 算法
 * ======================================================================== */

/** PageRank 选项 */
typedef struct GraphPageRankOptions_s {
    double damping;             /**< 阻尼因子，默认 0.85 */
    int max_iterations;         /**< 最大迭代次数，默认 100 */
    double tolerance;           /**< 收敛容忍度，默认 0.0001 */
    bool directed;             /**< 是否考虑方向 */
    double personalization_weight; /**< 个性化权重，0 表示不考虑 */
    graph_vertex_id_t *personalization_vertices; /**< 个性化顶点 */
    size_t num_personalization;
    double *personalization_values;
} GraphPageRankOptions;

/** PageRank 结果 */
typedef struct GraphPageRankResult_s {
    graph_vertex_id_t *vertices; /**< 顶点 ID 数组 */
    double *scores;             /**< PageRank 分数数组 */
    size_t num_vertices;        /**< 结果数量 */
    int iterations;              /**< 实际迭代次数 */
    double convergence;          /**< 最终收敛值 */
} GraphPageRankResult;

/**
 * @brief 计算 PageRank
 * @param graph 图数据库
 * @param options 选项（可为 NULL，使用默认值）
 * @return PageRank 结果
 */
GraphPageRankResult *graph_pagerank(void *graph,
                                    const GraphPageRankOptions *options);

/**
 * @brief 释放 PageRank 结果
 */
void graph_pagerank_free(GraphPageRankResult *result);

/**
 * @brief 获取顶点的 PageRank 分数
 */
double graph_pagerank_get_score(const GraphPageRankResult *result,
                                graph_vertex_id_t vid);

/* ========================================================================
 * 度中心性
 * ======================================================================== */

/** 中心性类型 */
typedef enum GraphCentralityType_e {
    GRAPH_CENTRALITY_DEGREE,     /**< 度中心性 */
    GRAPH_CENTRALITY_IN_DEGREE,  /**< 入度中心性 */
    GRAPH_CENTRALITY_OUT_DEGREE, /**< 出度中心性 */
    GRAPH_CENTRALITY_NORMALIZED  /**< 归一化度中心性 */
} GraphCentralityType;

/** 度中心性结果 */
typedef struct GraphDegreeCentralityResult_s {
    graph_vertex_id_t *vertices; /**< 顶点 ID 数组 */
    double *scores;             /**< 中心性分数数组 */
    size_t num_vertices;
} GraphDegreeCentralityResult;

/**
 * @brief 计算度中心性
 */
GraphDegreeCentralityResult *graph_degree_centrality(void *graph,
                                                     GraphCentralityType type,
                                                     bool normalized);

/**
 * @brief 释放度中心性结果
 */
void graph_degree_centrality_free(GraphDegreeCentralityResult *result);

/* ========================================================================
 * Louvain 社区检测
 * ======================================================================== */

/** Louvain 选项 */
typedef struct GraphLouvainOptions_s {
    int max_iterations;         /**< 每层最大迭代次数 */
    double tolerance;           /**< 模块度改善阈值 */
    bool hierarchical;          /**< 是否返回层次结构 */
    int resolution;             /**< 分辨率参数 */
} GraphLouvainOptions;

/** 社区结果 */
typedef struct GraphCommunity_s {
    int community_id;           /**< 社区 ID */
    graph_vertex_id_t *members; /**< 成员顶点数组 */
    size_t num_members;         /**< 成员数量 */
    double modularity;          /**< 该社区的模块度贡献 */
} GraphCommunity;

/** Louvain 结果 */
typedef struct GraphLouvainResult_s {
    int *vertex_communities;    /**< 每个顶点所属社区 ID */
    graph_vertex_id_t num_vertices;
    GraphCommunity *communities;/**< 社区数组 */
    size_t num_communities;
    double modularity;          /**< 总模块度 */
    int num_levels;             /**< 层次深度 */
} GraphLouvainResult;

/**
 * @brief 执行 Louvain 社区检测
 */
GraphLouvainResult *graph_louvain(void *graph,
                                  const GraphLouvainOptions *options);

/**
 * @brief 释放 Louvain 结果
 */
void graph_louvain_free(GraphLouvainResult *result);

/**
 * @brief 获取顶点所属社区
 */
int graph_louvain_get_community(const GraphLouvainResult *result,
                                graph_vertex_id_t vid);

/* ========================================================================
 * 连通分量
 * ======================================================================== */

/** 连通分量结果 */
typedef struct GraphConnectedComponents_s {
    int *component_ids;         /**< 每个顶点的连通分量 ID */
    graph_vertex_id_t num_vertices;
    int num_components;         /**< 连通分量数量 */
    size_t *component_sizes;    /**< 每个连通分量的大小 */
} GraphConnectedComponents;

/**
 * @brief 计算无向图的连通分量
 */
GraphConnectedComponents *graph_connected_components(void *graph);

/**
 * @brief 计算有向图的强连通分量
 */
GraphConnectedComponents *graph_strongly_connected_components(void *graph);

/**
 * @brief 释放连通分量结果
 */
void graph_connected_components_free(GraphConnectedComponents *result);

/* ========================================================================
 * 图统计
 * ======================================================================== */

/** 图统计信息 */
typedef struct GraphAlgoStats_s {
    uint64_t num_vertices;
    uint64_t num_edges;
    double avg_degree;
    uint64_t max_degree;
    uint64_t min_degree;
    uint64_t num_connected_components;
    double density;
    uint64_t num_triangles;
    double clustering_coefficient;
} GraphAlgoStats;

/**
 * @brief 计算图统计信息
 */
GraphAlgoStats *graph_algo_stats(void *graph);

/**
 * @brief 释放图统计信息
 */
void graph_algo_stats_free(GraphAlgoStats *stats);

#ifdef __cplusplus
}
#endif

#endif /* DB_GRAPH_ALGO_H */
