/**
 * @file graph_sql_functions.h
 * @brief 图模型 SQL 函数扩展头文件
 *
 * 实现 GRAPH_TRAVERSE、GRAPH_SHORTEST_PATH、GRAPH_PAGERANK 等 SQL 函数
 */
#ifndef DB_GRAPH_SQL_FUNCTIONS_H
#define DB_GRAPH_SQL_FUNCTIONS_H

#include "db/graph/graph.h"
#include "db/graph/graph_algo.h"
#include "db/graph/graph_cypher.h"
#include "db/graph/graph_property.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 函数注册表
 * ======================================================================== */

/** 图函数类型 */
typedef enum GraphFunctionType_e {
    GRAPH_FUNC_TRAVERSE = 0,
    GRAPH_FUNC_SHORTEST_PATH,
    GRAPH_FUNC_ALL_SHORTEST_PATHS,
    GRAPH_FUNC_PAGERANK,
    GRAPH_FUNC_DEGREE_CENTRALITY,
    GRAPH_FUNC_COMMUNITY_DETECTION,
    GRAPH_FUNC_SUBGRAPH,
    GRAPH_FUNC_NEIGHBORS,
    GRAPH_FUNC_COMMON_NEIGHBORS
} GraphFunctionType;

/** 图函数信息 */
typedef struct GraphFunctionInfo_s {
    const char *name;           /**< 函数名 */
    GraphFunctionType type;     /**< 函数类型 */
    int min_args;              /**< 最小参数数 */
    int max_args;              /**< 最大参数数 */
    const char *description;   /**< 描述 */
} GraphFunctionInfo;

/**
 * @brief 获取图函数信息
 */
const GraphFunctionInfo *graph_get_function_info(const char *func_name);

/**
 * @brief 列出所有图函数
 */
const GraphFunctionInfo *graph_list_functions(int *out_count);

/* ========================================================================
 * GRAPH_TRAVERSE 函数
 *
 * GRAPH_TRAVERSE(graph_name, start_vid, mode, max_depth, options)
 *
 * 示例:
 *   SELECT * FROM GRAPH_TRAVERSE('social', 1, 'BFS', 3)
 *
 * 参数:
 *   graph_name: 图名称
 *   start_vid: 起始顶点 ID
 *   mode: 'BFS' 或 'DFS'
 *   max_depth: 最大深度，-1 表示不限制
 *   options: 可选的 JSON 选项
 *
 * 返回:
 *   vid, parent_vid, depth, distance
 * ======================================================================== */

/** GRAPH_TRAVERSE 结果项 */
typedef struct GraphTraverseResult_s {
    graph_vertex_id_t vid;
    graph_vertex_id_t parent_vid;
    graph_edge_id_t via_edge;
    int depth;
    double distance;
    graph_label_id_t *labels;
    size_t num_labels;
} GraphTraverseResult;

/** GRAPH_TRAVERSE 上下文 */
typedef struct GraphTraverseCtx_s {
    void *graph;
    GraphTraverseIter *iter;
    GraphTraverseOptions options;
    GraphTraverseResult result;
    bool initialized;
} GraphTraverseCtx;

/**
 * @brief 创建 GRAPH_TRAVERSE 上下文
 */
GraphTraverseCtx *graph_traverse_ctx_create(void *graph,
                                          graph_vertex_id_t start_vid,
                                          const char *mode,
                                          int max_depth,
                                          const char *options_json);

/**
 * @brief 销毁 GRAPH_TRAVERSE 上下文
 */
void graph_traverse_ctx_destroy(GraphTraverseCtx *ctx);

/**
 * @brief 获取下一条记录
 * @return false 表示遍历完成
 */
bool graph_traverse_ctx_next(GraphTraverseCtx *ctx, GraphTraverseResult *out_result);

/* ========================================================================
 * GRAPH_SHORTEST_PATH 函数
 *
 * GRAPH_SHORTEST_PATH(graph_name, src_vid, dst_vid, options)
 *
 * 示例:
 *   SELECT * FROM GRAPH_SHORTEST_PATH('social', 1, 100)
 *
 * 参数:
 *   graph_name: 图名称
 *   src_vid: 源顶点 ID
 *   dst_vid: 目标顶点 ID
 *   options: 可选的 JSON 选项 (edge_weights, directed)
 *
 * 返回:
 *   vertex_id, edge_id, path_order, cum_weight
 * ======================================================================== */

/**
 * @brief 计算最短路径
 */
GraphPath *graph_shortest_path(void *graph,
                              graph_vertex_id_t src,
                              graph_vertex_id_t dst,
                              bool directed,
                              double max_distance);

/* ========================================================================
 * GRAPH_PAGERANK 函数
 *
 * GRAPH_PAGERANK(graph_name, damping, max_iter, tolerance)
 *
 * 示例:
 *   SELECT vid, score FROM GRAPH_PAGERANK('social', 0.85, 100, 0.0001)
 *   ORDER BY score DESC
 *   LIMIT 10
 *
 * 参数:
 *   graph_name: 图名称
 *   damping: 阻尼因子（默认 0.85）
 *   max_iter: 最大迭代次数（默认 100）
 *   tolerance: 收敛容忍度（默认 0.0001）
 *
 * 返回:
 *   vid, score
 * ======================================================================== */

/**
 * @brief 执行 PageRank
 */
GraphPageRankResult *graph_sql_pagerank(void *graph,
                                       double damping,
                                       int max_iter,
                                       double tolerance);

/* ========================================================================
 * GRAPH_NEIGHBORS 函数
 *
 * GRAPH_NEIGHBORS(graph_name, vid, rel_type, direction, depth)
 *
 * 示例:
 *   SELECT neighbor_vid, edge_id, edge_type
 *   FROM GRAPH_NEIGHBORS('social', 1, 'FRIEND', 'OUT', 1)
 *
 * 参数:
 *   graph_name: 图名称
 *   vid: 顶点 ID
 *   rel_type: 关系类型（NULL 表示所有）
 *   direction: 'IN', 'OUT', 'BOTH'（默认 BOTH）
 *   depth: 深度（默认 1）
 *
 * 返回:
 *   neighbor_vid, edge_id, edge_type, depth
 * ======================================================================== */

/** 邻居结果项 */
typedef struct GraphNeighbor_s {
    graph_vertex_id_t vid;
    graph_edge_id_t edge_id;
    graph_label_id_t rel_type;
    int depth;
} GraphNeighbor;

/**
 * @brief 获取邻居
 */
GraphNeighbor *graph_get_neighbors(void *graph,
                                  graph_vertex_id_t vid,
                                  const char *rel_type,
                                  const char *direction,
                                  int depth,
                                  size_t *out_count);

/**
 * @brief 释放邻居数组
 */
void graph_neighbors_free(GraphNeighbor *neighbors, size_t count);

/* ========================================================================
 * GRAPH_SUBGRAPH 函数
 *
 * 从图中提取子图
 *
 * GRAPH_SUBGRAPH(graph_name, vid_list, rel_type_list)
 *
 * 示例:
 *   SELECT * FROM GRAPH_SUBGRAPH('social', ARRAY[1,2,3], ARRAY['KNOWS'])
 * ======================================================================== */

/**
 * @brief 提取子图
 */
void *graph_subgraph_extract(void *graph,
                            const graph_vertex_id_t *vids,
                            size_t num_vids,
                            const char **rel_types,
                            size_t num_rel_types);

/* ========================================================================
 * GRAPH_COMMUNITY 函数
 *
 * GRAPH_COMMUNITY(graph_name, algorithm, options)
 *
 * 示例:
 *   SELECT vid, community_id
 *   FROM GRAPH_COMMUNITY('social', 'LOUVAIN')
 *   ORDER BY community_id
 *
 * 算法:
 *   LOUVAIN, LABEL_PROPAGATION, WEAKLY_CONNECTED
 * ======================================================================== */

/**
 * @brief 检测社区
 */
GraphLouvainResult *graph_community_detection(void *graph,
                                             const char *algorithm,
                                             const char *options_json);

/* ========================================================================
 * GRAPH_CENTRALITY 函数
 *
 * 计算图的中心性指标
 *
 * GRAPH_CENTRALITY(graph_name, type, normalized)
 *
 * 示例:
 *   SELECT vid, score
 *   FROM GRAPH_CENTRALITY('social', 'DEGREE', true)
 *   ORDER BY score DESC
 *
 * 类型:
 *   DEGREE, IN_DEGREE, OUT_DEGREE, BETWEENNESS, CLOSENESS, EIGENVECTOR
 * ======================================================================== */

/**
 * @brief 计算中心性
 */
GraphDegreeCentralityResult *graph_sql_centrality(void *graph,
                                                 const char *type,
                                                 bool normalized);

/* ========================================================================
 * GRAPH_PATTERN_MATCH 函数
 *
 * 图模式匹配
 *
 * GRAPH_PATTERN_MATCH(graph_name, pattern, options)
 *
 * 示例:
 *   SELECT *
 *   FROM GRAPH_PATTERN_MATCH('social',
 *       '(a:Person)-[:KNOWS]->(b:Person)-[:KNOWS]->(c:Person)',
 *       'a.name = "Alice"')
 *
 * 参数:
 *   graph_name: 图名称
 *   pattern: Cypher 模式表达式
 *   where: WHERE 子句（可选）
 *
 * 返回:
 *   绑定变量及其值
 * ======================================================================== */

/** 模式匹配结果 */
typedef struct GraphPatternMatchResult_s {
    char **variable_names;       /**< 变量名数组 */
    void **variable_values;     /**< 变量值数组 */
    size_t num_variables;       /**< 变量数量 */
    graph_vertex_id_t *vertex_ids; /**< 顶点 ID 数组 */
    graph_edge_id_t *edge_ids;  /**< 边 ID 数组 */
    size_t num_vertices;
    size_t num_edges;
} GraphPatternMatchResult;

/**
 * @brief 执行图模式匹配
 */
GraphPatternMatchResult *graph_pattern_match(void *graph,
                                             const char *pattern,
                                             const char *where_clause);

/**
 * @brief 释放模式匹配结果
 */
void graph_pattern_match_free(GraphPatternMatchResult *result);

/* ========================================================================
 * 执行器接口
 * ======================================================================== */

/** 图函数执行器 */
typedef struct GraphFunctionExecutor_s {
    void *db;                  /**< 数据库句柄 */
    void *mem_pool;            /**< 内存池 */
} GraphFunctionExecutor;

/**
 * @brief 创建函数执行器
 */
GraphFunctionExecutor *graph_executor_create(void *db);

/**
 * @brief 销毁函数执行器
 */
void graph_executor_destroy(GraphFunctionExecutor *exec);

/**
 * @brief 注册图函数到 SQL 执行器
 */
int graph_register_functions(void *sql_executor);

#ifdef __cplusplus
}
#endif

#endif /* DB_GRAPH_SQL_FUNCTIONS_H */
