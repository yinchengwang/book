/**
 * @file graph_traverse.c
 * @brief 图遍历算法包装层实现
 */
#include "db/storage/graph/graph_traverse.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * 工具函数
 * ======================================================================== */

/**
 * @brief 获取内部图指针
 */
static graph_t *get_graph(void *rel) {
    graph_engine_db_t *db = (graph_engine_db_t *)rel;
    return db ? db->graph : NULL;
}

/**
 * @brief 转换遍历方向
 */
static graph_traverse_direction_t convert_direction(int dir) {
    switch (dir) {
        case 1:  return GRAPH_TRAVERSE_IN;
        case 2:  return GRAPH_TRAVERSE_BOTH;
        default: return GRAPH_TRAVERSE_OUT;
    }
}

/* ========================================================================
 * 邻居查询
 * ======================================================================== */

int graph_get_neighbors(void *rel, graph_vertex_id_t vid,
                        graph_traverse_direction_t direction,
                        graph_neighbors_t *out) {
    if (!rel || !out) return -1;

    graph_t *g = get_graph(rel);
    if (!g) return -1;

    memset(out, 0, sizeof(graph_neighbors_t));
    out->capacity = 64;
    out->vertices = calloc(out->capacity, sizeof(graph_vertex_id_t));
    if (!out->vertices) return -1;

    /* 获取出边 */
    if (direction == 0 || direction == 2) {
        graph_edge_id_t *edges = NULL;
        size_t edge_count = 0;
        graph_vertex_get_out_edges(g, vid, NULL, &edges, &edge_count);

        for (size_t i = 0; i < edge_count; i++) {
            graph_edge_t *edge = NULL;
            if (graph_edge_get(g, edges[i], &edge) == 0) {
                if (out->count >= out->capacity) {
                    out->capacity *= 2;
                    graph_vertex_id_t *new_vertices = realloc(
                        out->vertices, out->capacity * sizeof(graph_vertex_id_t));
                    if (new_vertices) {
                        out->vertices = new_vertices;
                    } else {
                        free(edge);
                        free(edges);
                        return -1;
                    }
                }
                out->vertices[out->count++] = edge->dst_id;
                free(edge);
            }
        }
        free(edges);
    }

    /* 获取入边 */
    if (direction == 1 || direction == 2) {
        graph_edge_id_t *edges = NULL;
        size_t edge_count = 0;
        graph_vertex_get_in_edges(g, vid, NULL, &edges, &edge_count);

        for (size_t i = 0; i < edge_count; i++) {
            graph_edge_t *edge = NULL;
            if (graph_edge_get(g, edges[i], &edge) == 0) {
                /* 避免重复添加（如果是双向） */
                bool found = false;
                for (size_t j = 0; j < out->count; j++) {
                    if (out->vertices[j] == edge->src_id) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    if (out->count >= out->capacity) {
                        out->capacity *= 2;
                        graph_vertex_id_t *new_vertices = realloc(
                            out->vertices, out->capacity * sizeof(graph_vertex_id_t));
                        if (new_vertices) {
                            out->vertices = new_vertices;
                        } else {
                            free(edge);
                            free(edges);
                            return -1;
                        }
                    }
                    out->vertices[out->count++] = edge->src_id;
                }
                free(edge);
            }
        }
        free(edges);
    }

    return 0;
}

void graph_neighbors_free(graph_neighbors_t *result) {
    if (!result) return;
    free(result->vertices);
    memset(result, 0, sizeof(graph_neighbors_t));
}

/* ========================================================================
 * BFS 遍历
 * ======================================================================== */

/* BFS 回调上下文 */
typedef struct {
    graph_engine_bfs_result_t *out;
    size_t capacity;
} bfs_collect_ctx_t;

static bool bfs_collect_callback(graph_vertex_id_t vid, int depth, void *ctx) {
    bfs_collect_ctx_t *c = (bfs_collect_ctx_t *)ctx;
    if (!c || !c->out) return false;

    /* 扩展容量 */
    if (c->out->count >= c->capacity) {
        c->capacity *= 2;
        graph_vertex_id_t *new_vertices = realloc(
            c->out->vertices, c->capacity * sizeof(graph_vertex_id_t));
        int *new_depths = realloc(c->out->depths, c->capacity * sizeof(int));
        if (!new_vertices || !new_depths) {
            return false;
        }
        c->out->vertices = new_vertices;
        c->out->depths = new_depths;
    }

    c->out->vertices[c->out->count] = vid;
    c->out->depths[c->out->count] = depth;
    c->out->count++;
    return true;
}

int graph_engine_bfs(void *rel, graph_vertex_id_t start, int max_depth,
                     graph_engine_bfs_result_t *out) {
    if (!rel || !out) return -1;

    graph_t *g = get_graph(rel);
    if (!g) return -1;

    memset(out, 0, sizeof(graph_engine_bfs_result_t));
    out->capacity = 256;
    out->vertices = calloc(out->capacity, sizeof(graph_vertex_id_t));
    out->depths = calloc(out->capacity, sizeof(int));
    if (!out->vertices || !out->depths) {
        free(out->vertices);
        free(out->depths);
        return -1;
    }

    bfs_collect_ctx_t ctx = { .out = out, .capacity = out->capacity };
    graph_bfs(g, start, max_depth, bfs_collect_callback, &ctx);

    return 0;
}

void graph_engine_bfs_free(graph_engine_bfs_result_t *result) {
    if (!result) return;
    free(result->vertices);
    free(result->depths);
    memset(result, 0, sizeof(graph_engine_bfs_result_t));
}

/* ========================================================================
 * 最短路径
 * ======================================================================== */

int graph_engine_shortest_path(void *rel, graph_vertex_id_t src,
                               graph_vertex_id_t dst,
                               graph_path_result_t *out) {
    if (!rel || !out) return -1;

    graph_t *g = get_graph(rel);
    if (!g) return -1;

    memset(out, 0, sizeof(graph_path_result_t));

    graph_path_t *path = NULL;
    int length = graph_shortest_path(g, src, dst, &path);

    if (length < 0 || !path) {
        return -1;
    }

    out->length = (size_t)length + 1;  /* 顶点数 = 边数 + 1 */
    out->vertices = calloc(out->length, sizeof(graph_vertex_id_t));
    out->edge_ids = calloc(length, sizeof(size_t));
    if (!out->vertices || !out->edge_ids) {
        free(out->vertices);
        free(out->edge_ids);
        graph_path_free(path);
        return -1;
    }

    memcpy(out->vertices, path->vertices, out->length * sizeof(graph_vertex_id_t));
    memcpy(out->edge_ids, path->edges, length * sizeof(graph_edge_id_t));
    out->weight = (double)length;  /* BFS 权重 = 跳数 */

    graph_path_free(path);
    return length;
}

double graph_dijkstra_path(void *rel, graph_vertex_id_t src,
                           graph_vertex_id_t dst,
                           double (*weight_fn)(graph_edge_id_t, void *),
                           void *ctx,
                           graph_path_result_t *out) {
    if (!rel || !out) return -1.0;

    graph_t *g = get_graph(rel);
    if (!g) return -1.0;

    memset(out, 0, sizeof(graph_path_result_t));

    graph_path_t *path = NULL;
    double weight = graph_dijkstra(g, src, dst, weight_fn, ctx, &path);

    if (weight < 0 || !path) {
        return -1.0;
    }

    out->length = (size_t)(weight + 0.5) + 1;  /* 近似 */
    out->vertices = calloc(out->length, sizeof(graph_vertex_id_t));
    out->edge_ids = calloc((size_t)(weight + 0.5), sizeof(size_t));
    if (!out->vertices || !out->edge_ids) {
        free(out->vertices);
        free(out->edge_ids);
        graph_path_free(path);
        return -1.0;
    }

    memcpy(out->vertices, path->vertices, out->length * sizeof(graph_vertex_id_t));
    memcpy(out->edge_ids, path->edges, (size_t)(weight + 0.5) * sizeof(graph_edge_id_t));
    out->weight = weight;

    graph_path_free(path);
    return weight;
}

void graph_path_result_free(graph_path_result_t *result) {
    if (!result) return;
    free(result->vertices);
    free(result->edge_ids);
    memset(result, 0, sizeof(graph_path_result_t));
}

/* ========================================================================
 * PageRank
 * ======================================================================== */

int graph_engine_pagerank(void *rel, double damping, int max_iter,
                          double tolerance,
                          graph_engine_pagerank_result_t *out) {
    if (!rel || !out) return -1;

    graph_t *g = get_graph(rel);
    if (!g) return -1;

    memset(out, 0, sizeof(graph_engine_pagerank_result_t));

    graph_pagerank_config_t config = {
        .damping_factor = damping > 0 ? damping : 0.85,
        .max_iterations = max_iter > 0 ? max_iter : 100,
        .tolerance = tolerance > 0 ? tolerance : 1e-6
    };

    /* 调用底层 graph_pagerank（来自 traverse.h） */
    graph_pagerank_result_t *pr_result = NULL;
    if (graph_pagerank(g, &config, &pr_result) != 0) {
        return -1;
    }

    out->count = pr_result->count;
    out->vertices = pr_result->vertices;
    out->scores = pr_result->scores;

    /* 转移所有权 */
    free(pr_result);
    return 0;
}

void graph_engine_pagerank_free(graph_engine_pagerank_result_t *result) {
    if (!result) return;
    free(result->vertices);
    free(result->scores);
    memset(result, 0, sizeof(graph_engine_pagerank_result_t));
}

/* ========================================================================
 * 度查询
 * ======================================================================== */

int64_t graph_get_out_degree(void *rel, graph_vertex_id_t vid) {
    graph_t *g = get_graph(rel);
    if (!g) return -1;

    graph_edge_id_t *edges = NULL;
    size_t count = 0;
    graph_vertex_get_out_edges(g, vid, NULL, &edges, &count);
    free(edges);
    return (int64_t)count;
}

int64_t graph_get_in_degree(void *rel, graph_vertex_id_t vid) {
    graph_t *g = get_graph(rel);
    if (!g) return -1;

    graph_edge_id_t *edges = NULL;
    size_t count = 0;
    graph_vertex_get_in_edges(g, vid, NULL, &edges, &count);
    free(edges);
    return (int64_t)count;
}

/* ========================================================================
 * 遍历迭代器
 * ======================================================================== */

struct graph_traverse_iter_s {
    graph_traverser_t *traverser;  /**< 底层遍历器 */
    graph_t *graph;                /**< 图指针 */
    int max_depth;                 /**< 最大深度 */
};

/**
 * @brief 内部 BFS 回调（用于迭代器）
 */
static bool iter_callback(graph_vertex_id_t vid, int depth, void *ctx) {
    (void)vid;
    (void)depth;
    (void)ctx;
    return true;  /* 继续遍历 */
}

graph_traverse_iter_t *graph_traverse_iter_create(void *rel,
                                                  graph_vertex_id_t start,
                                                  graph_traverse_direction_t direction,
                                                  int max_depth) {
    if (!rel) return NULL;

    graph_t *g = get_graph(rel);
    if (!g) return NULL;

    graph_traverse_iter_t *iter = calloc(1, sizeof(graph_traverse_iter_t));
    if (!iter) return NULL;

    iter->graph = g;
    iter->max_depth = max_depth;

    graph_traverse_config_t config = {
        .mode = GRAPH_TRAVERSE_BFS,
        .direction = (graph_traverse_direction_t)direction,
        .max_depth = max_depth,
        .visited_set = true
    };

    iter->traverser = graph_traverser_create(g, &config);
    if (!iter->traverser) {
        free(iter);
        return NULL;
    }

    graph_traverser_start(iter->traverser, start);
    return iter;
}

bool graph_traverse_iter_next(graph_traverse_iter_t *iter,
                              graph_vertex_id_t *out_vid,
                              int *out_depth) {
    if (!iter || !out_vid) return false;
    return graph_traverser_next(iter->traverser, out_vid, out_depth);
}

void graph_traverse_iter_destroy(graph_traverse_iter_t *iter) {
    if (!iter) return;
    if (iter->traverser) {
        graph_traverser_destroy(iter->traverser);
    }
    free(iter);
}
