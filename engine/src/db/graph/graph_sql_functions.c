/**
 * @file graph_sql_functions.c
 * @brief 图模型 SQL 函数扩展实现
 */

#include "db/graph/graph_sql_functions.h"
#include "db/graph/graph_engine.h"
#include "db/graph/graph_algo.h"
#include "db/graph/graph_cypher.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ========================================================================
 * 函数注册表
 * ======================================================================== */

static const GraphFunctionInfo graph_functions[] = {
    {"GRAPH_TRAVERSE", GRAPH_FUNC_TRAVERSE, 2, 5,
     "图遍历查询，支持 BFS 和 DFS"},
    {"GRAPH_SHORTEST_PATH", GRAPH_FUNC_SHORTEST_PATH, 3, 5,
     "计算两点间最短路径"},
    {"GRAPH_ALL_SHORTEST_PATHS", GRAPH_FUNC_ALL_SHORTEST_PATHS, 3, 5,
     "计算 K 条最短路径"},
    {"GRAPH_PAGERANK", GRAPH_FUNC_PAGERANK, 1, 4,
     "计算 PageRank 分数"},
    {"GRAPH_DEGREE_CENTRALITY", GRAPH_FUNC_DEGREE_CENTRALITY, 1, 3,
     "计算度中心性"},
    {"GRAPH_COMMUNITY", GRAPH_FUNC_COMMUNITY_DETECTION, 1, 3,
     "社区检测算法"},
    {"GRAPH_SUBGRAPH", GRAPH_FUNC_SUBGRAPH, 2, 4,
     "提取子图"},
    {"GRAPH_NEIGHBORS", GRAPH_FUNC_NEIGHBORS, 2, 5,
     "获取邻居节点"},
    {"GRAPH_COMMON_NEIGHBORS", GRAPH_FUNC_COMMON_NEIGHBORS, 3, 4,
     "计算共同邻居"}
};

const GraphFunctionInfo *graph_get_function_info(const char *func_name)
{
    for (size_t i = 0; i < sizeof(graph_functions) / sizeof(graph_functions[0]); i++) {
        if (strcasecmp(func_name, graph_functions[i].name) == 0) {
            return &graph_functions[i];
        }
    }
    return NULL;
}

const GraphFunctionInfo *graph_list_functions(int *out_count)
{
    *out_count = sizeof(graph_functions) / sizeof(graph_functions[0]);
    return graph_functions;
}

/* ========================================================================
 * GRAPH_TRAVERSE 实现
 * ======================================================================== */

GraphTraverseCtx *graph_traverse_ctx_create(void *graph,
                                         graph_vertex_id_t start_vid,
                                         const char *mode,
                                         int max_depth,
                                         const char *options_json)
{
    GraphTraverseCtx *ctx = (GraphTraverseCtx *)calloc(1, sizeof(GraphTraverseCtx));
    if (!ctx) return NULL;

    ctx->graph = graph;
    ctx->iter = NULL;
    ctx->initialized = false;

    /* 解析遍历模式 */
    ctx->options.mode = GRAPH_TRAVERSE_BFS;
    if (mode) {
        if (strcasecmp(mode, "DFS") == 0) {
            ctx->options.mode = GRAPH_TRAVERSE_DFS;
        }
    }

    /* 解析最大深度 */
    ctx->options.max_depth = max_depth >= 0 ? max_depth : -1;
    ctx->options.max_nodes = 0;
    ctx->options.node_labels = NULL;
    ctx->options.num_node_labels = 0;
    ctx->options.rel_types = NULL;
    ctx->options.num_rel_types = 0;
    ctx->options.directed = false;
    ctx->options.include_start = true;

    /* 创建迭代器 */
    ctx->iter = graph_traverse_create(graph, start_vid, &ctx->options);
    if (!ctx->iter) {
        free(ctx);
        return NULL;
    }

    return ctx;
}

void graph_traverse_ctx_destroy(GraphTraverseCtx *ctx)
{
    if (!ctx) return;

    if (ctx->iter) {
        graph_traverse_destroy(ctx->iter);
    }
    free(ctx);
}

bool graph_traverse_ctx_next(GraphTraverseCtx *ctx, GraphTraverseResult *out_result)
{
    if (!ctx || !ctx->iter || !out_result) return false;

    GraphTraverseResultItem *item = graph_traverse_next(ctx->iter);
    if (!item) return false;

    out_result->vid = item->vid;
    out_result->parent_vid = item->parent_vid;
    out_result->via_edge = item->via_edge;
    out_result->depth = item->depth;
    out_result->distance = item->distance;

    /* 获取顶点标签 */
    graph_vertex_t *vertex = NULL;
    if (graph_vertex_get(ctx->graph, item->vid, &vertex) == 0 && vertex) {
        out_result->labels = vertex->labels;
        out_result->num_labels = vertex->num_labels;
        graph_vertex_free(vertex);
    } else {
        out_result->labels = NULL;
        out_result->num_labels = 0;
    }

    return true;
}

/* ========================================================================
 * 最短路径实现
 * ======================================================================== */

GraphPath *graph_shortest_path(void *graph,
                             graph_vertex_id_t src,
                             graph_vertex_id_t dst,
                             bool directed,
                             double max_distance)
{
    GraphDijkstraOptions options = {0};
    options.directed = directed;
    options.max_distance = max_distance;

    return graph_dijkstra(graph, src, dst, &options);
}

/* ========================================================================
 * PageRank 实现
 * ======================================================================== */

GraphPageRankResult *graph_sql_pagerank(void *graph,
                                       double damping,
                                       int max_iter,
                                       double tolerance)
{
    GraphPageRankOptions options = {0};
    options.damping = damping > 0 ? damping : 0.85;
    options.max_iterations = max_iter > 0 ? max_iter : 100;
    options.tolerance = tolerance > 0 ? tolerance : 0.0001;

    return graph_pagerank(graph, &options);
}

/* ========================================================================
 * 邻居查询实现
 * ======================================================================== */

GraphNeighbor *graph_get_neighbors(void *graph,
                                  graph_vertex_id_t vid,
                                  const char *rel_type,
                                  const char *direction,
                                  int depth,
                                  size_t *out_count)
{
    *out_count = 0;
    size_t capacity = 64;
    GraphNeighbor *neighbors = (GraphNeighbor *)malloc(sizeof(GraphNeighbor) * capacity);

    /* 解析方向 */
    bool include_out = true;
    bool include_in = true;

    if (direction) {
        if (strcasecmp(direction, "OUT") == 0) {
            include_in = false;
        } else if (strcasecmp(direction, "IN") == 0) {
            include_out = false;
        }
    }

    /* 递归获取邻居 */
    typedef struct {
        graph_vertex_id_t vid;
        int depth;
    } QueueItem;

    QueueItem *queue = (QueueItem *)malloc(sizeof(QueueItem) * 64);
    size_t queue_front = 0, queue_back = 0;

    queue[queue_back++] = (QueueItem){vid, 0};

    while (queue_front < queue_back) {
        QueueItem item = queue[queue_front++];

        if (depth >= 0 && item.depth > depth) continue;

        /* 获取出边 */
        if (include_out) {
            graph_edge_id_t *out_edges = NULL;
            size_t out_count = 0;
            graph_vertex_get_out_edges(graph, item.vid, rel_type, &out_edges, &out_count);

            for (size_t i = 0; i < out_count; i++) {
                graph_edge_t *edge = NULL;
                if (graph_edge_get(graph, out_edges[i], &edge) != 0 || !edge) continue;

                if (*out_count >= capacity) {
                    capacity *= 2;
                    neighbors = (GraphNeighbor *)realloc(neighbors,
                        sizeof(GraphNeighbor) * capacity);
                }

                neighbors[*out_count].vid = edge->dst;
                neighbors[*out_count].edge_id = out_edges[i];
                neighbors[*out_count].rel_type = edge->rel_type;
                neighbors[*out_count].depth = item.depth + 1;
                (*out_count)++;

                /* 添加到队列 */
                queue[queue_back++] = (QueueItem){edge->dst, item.depth + 1};

                graph_edge_free(edge);
            }
            free(out_edges);
        }

        /* 获取入边 */
        if (include_in) {
            graph_edge_id_t *in_edges = NULL;
            size_t in_count = 0;
            graph_vertex_get_in_edges(graph, item.vid, rel_type, &in_edges, &in_count);

            for (size_t i = 0; i < in_count; i++) {
                graph_edge_t *edge = NULL;
                if (graph_edge_get(graph, in_edges[i], &edge) != 0 || !edge) continue;

                if (*out_count >= capacity) {
                    capacity *= 2;
                    neighbors = (GraphNeighbor *)realloc(neighbors,
                        sizeof(GraphNeighbor) * capacity);
                }

                neighbors[*out_count].vid = edge->src;
                neighbors[*out_count].edge_id = in_edges[i];
                neighbors[*out_count].rel_type = edge->rel_type;
                neighbors[*out_count].depth = item.depth + 1;
                (*out_count)++;

                /* 添加到队列 */
                queue[queue_back++] = (QueueItem){edge->src, item.depth + 1};

                graph_edge_free(edge);
            }
            free(in_edges);
        }
    }

    free(queue);
    return neighbors;
}

void graph_neighbors_free(GraphNeighbor *neighbors, size_t count)
{
    (void)count;
    free(neighbors);
}

/* ========================================================================
 * 子图提取实现
 * ======================================================================== */

void *graph_subgraph_extract(void *graph,
                            const graph_vertex_id_t *vids,
                            size_t num_vids,
                            const char **rel_types,
                            size_t num_rel_types)
{
    (void)graph; (void)vids; (void)num_vids;
    (void)rel_types; (void)num_rel_types;
    /* 简化实现：返回 NULL 表示子图提取需要更复杂的实现 */
    return NULL;
}

/* ========================================================================
 * 社区检测实现
 * ======================================================================== */

GraphLouvainResult *graph_community_detection(void *graph,
                                             const char *algorithm,
                                             const char *options_json)
{
    (void)options_json;

    GraphLouvainOptions options = {0};
    options.max_iterations = 100;
    options.tolerance = 0.0001;
    options.resolution = 1;

    /* 目前只实现 Louvain */
    if (algorithm && strcasecmp(algorithm, "LOUVAIN") != 0) {
        /* 可以扩展支持其他算法 */
    }

    return graph_louvain(graph, &options);
}

/* ========================================================================
 * 中心性计算实现
 * ======================================================================== */

GraphDegreeCentralityResult *graph_sql_centrality(void *graph,
                                                 const char *type,
                                                 bool normalized)
{
    GraphCentralityType ctype = GRAPH_CENTRALITY_DEGREE;

    if (type) {
        if (strcasecmp(type, "IN_DEGREE") == 0) {
            ctype = GRAPH_CENTRALITY_IN_DEGREE;
        } else if (strcasecmp(type, "OUT_DEGREE") == 0) {
            ctype = GRAPH_CENTRALITY_OUT_DEGREE;
        } else if (strcasecmp(type, "DEGREE") == 0 ||
                   strcasecmp(type, "DEGREE_CENTRALITY") == 0) {
            ctype = GRAPH_CENTRALITY_DEGREE;
        }
    }

    return graph_degree_centrality(graph, ctype, normalized);
}

/* ========================================================================
 * 模式匹配实现
 * ======================================================================== */

GraphPatternMatchResult *graph_pattern_match(void *graph,
                                            const char *pattern,
                                            const char *where_clause)
{
    /* 解析 Cypher 模式 */
    CypherQuery *query = cypher_parse_query(pattern);
    if (!query) {
        return NULL;
    }

    /* 简化实现：返回空结果 */
    GraphPatternMatchResult *result = (GraphPatternMatchResult *)calloc(1,
        sizeof(GraphPatternMatchResult));
    result->variable_names = NULL;
    result->variable_values = NULL;
    result->num_variables = 0;
    result->vertex_ids = NULL;
    result->edge_ids = NULL;
    result->num_vertices = 0;
    result->num_edges = 0;

    (void)graph;
    (void)where_clause;

    cypher_query_free(query);
    return result;
}

void graph_pattern_match_free(GraphPatternMatchResult *result)
{
    if (!result) return;

    for (size_t i = 0; i < result->num_variables; i++) {
        free(result->variable_names[i]);
        free(result->variable_values[i]);
    }
    free(result->variable_names);
    free(result->variable_values);
    free(result->vertex_ids);
    free(result->edge_ids);
    free(result);
}

/* ========================================================================
 * 执行器接口
 * ======================================================================== */

GraphFunctionExecutor *graph_executor_create(void *db)
{
    GraphFunctionExecutor *exec = (GraphFunctionExecutor *)calloc(1,
        sizeof(GraphFunctionExecutor));
    if (!exec) return NULL;

    exec->db = db;
    exec->mem_pool = NULL;

    return exec;
}

void graph_executor_destroy(GraphFunctionExecutor *exec)
{
    free(exec);
}

int graph_register_functions(void *sql_executor)
{
    (void)sql_executor;
    /* 注册图函数到 SQL 执行器
     * 需要与 SQL 执行器集成
     * 目前只是占位实现
     */
    return 0;
}
