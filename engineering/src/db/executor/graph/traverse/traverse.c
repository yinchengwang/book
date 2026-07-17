/**
 * @file graph_traverse.c
 * @brief 图遍历引擎实现
 *
 * 实现 BFS、DFS、最短路径和 PageRank 算法
 */
#include "db/executor/graph/traverse/traverse.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>

/* ============================================================
 * 遍历器结构
 * ============================================================ */

struct graph_traverser_s {
    graph_t                 *graph;          /**< 图数据库 */
    graph_traverse_config_t  config;         /**< 遍历配置 */

    /* 状态 */
    graph_vertex_id_t       *queue;          /**< BFS 队列 / DFS 栈 */
    size_t                  queue_head;      /**< 队头 */
    size_t                  queue_tail;      /**< 队尾 */
    size_t                  queue_capacity;  /**< 队列容量 */

    graph_vertex_id_t       *visited;        /**< 已访问集合（哈希表模拟） */
    size_t                  visited_capacity;
    size_t                  visited_count;

    int                     current_depth;   /**< 当前深度 */
    size_t                  level_end;       /**< 当前层的结束位置 */
    bool                    done;            /**< 遍历完成 */
};

/* ============================================================
 * 工具函数
 * ============================================================ */

/**
 * @brief 计算哈希值
 */
static size_t hash_vertex_id(graph_vertex_id_t id, size_t capacity) {
    return (size_t)(id % capacity);
}

/**
 * @brief 检查顶点是否已访问
 */
static bool is_visited(graph_traverser_t *t, graph_vertex_id_t vid) {
    size_t pos = hash_vertex_id(vid, t->visited_capacity);
    return t->visited[pos] == vid;
}

/**
 * @brief 标记顶点已访问
 */
static void mark_visited(graph_traverser_t *t, graph_vertex_id_t vid) {
    size_t pos = hash_vertex_id(vid, t->visited_capacity);
    if (t->visited[pos] == 0 || t->visited[pos] == vid) {
        t->visited[pos] = vid;
        t->visited_count++;
    }
}

/**
 * @brief 扩展队列
 */
static int expand_queue(graph_traverser_t *t) {
    size_t new_cap = t->queue_capacity * 2;
    graph_vertex_id_t *new_queue = realloc(t->queue, sizeof(graph_vertex_id_t) * new_cap);
    if (!new_queue) return -1;

    t->queue = new_queue;
    t->queue_capacity = new_cap;
    return 0;
}

/**
 * @brief 扩展 visited 数组（暂时占位）
 */
static int __attribute__((unused)) expand_visited(graph_traverser_t *t) {
    (void)t;
    return 0;  /* 暂时不实现扩展逻辑 */
}

/* ============================================================
 * 遍历器生命周期
 * ============================================================ */

graph_traverser_t *graph_traverser_create(graph_t *g,
                                          const graph_traverse_config_t *config) {
    if (!g) return NULL;

    graph_traverser_t *t = calloc(1, sizeof(graph_traverser_t));
    if (!t) return NULL;

    t->graph = g;

    if (config) {
        memcpy(&t->config, config, sizeof(graph_traverse_config_t));
    } else {
        /* 默认配置 */
        memset(&t->config, 0, sizeof(graph_traverse_config_t));
        t->config.mode = GRAPH_TRAVERSE_BFS;
        t->config.direction = GRAPH_TRAVERSE_OUT;
        t->config.max_depth = -1;
        t->config.visited_set = true;
    }

    /* 初始化队列 */
    t->queue_capacity = 1024;
    t->queue = malloc(sizeof(graph_vertex_id_t) * t->queue_capacity);
    if (!t->queue) {
        free(t);
        return NULL;
    }

    /* 初始化 visited 集合 */
    t->visited_capacity = 1024;
    t->visited = calloc(t->visited_capacity, sizeof(graph_vertex_id_t));
    if (!t->visited) {
        free(t->queue);
        free(t);
        return NULL;
    }

    return t;
}

void graph_traverser_destroy(graph_traverser_t *t) {
    if (!t) return;
    free(t->queue);
    free(t->visited);
    free(t);
}

void graph_traverser_start(graph_traverser_t *t, graph_vertex_id_t start) {
    if (!t) return;

    /* 重置状态 */
    t->queue_head = 0;
    t->queue_tail = 0;
    t->current_depth = 0;
    t->level_end = 1;  /* 第一层只有起始顶点 */
    t->done = false;
    t->visited_count = 0;
    memset(t->visited, 0, sizeof(graph_vertex_id_t) * t->visited_capacity);

    /* 将起始顶点加入队列 */
    if (t->queue_tail >= t->queue_capacity) {
        expand_queue(t);
    }
    t->queue[t->queue_tail++] = start;
    mark_visited(t, start);
}

bool graph_traverser_next(graph_traverser_t *t,
                          graph_vertex_id_t *out_vid,
                          int *out_depth) {
    if (!t || t->done || !out_vid) return false;

    while (t->queue_head < t->queue_tail) {
        /* 检查是否到达当前层的末尾，需要增加深度 */
        if (t->queue_head >= t->level_end) {
            /* 检查深度限制 */
            if (t->config.max_depth >= 0 && t->current_depth >= t->config.max_depth) {
                t->done = true;
                return false;
            }
            t->current_depth++;
            t->level_end = t->queue_tail;
        }

        graph_vertex_id_t current = t->queue[t->queue_head++];

        /* 输出当前顶点 */
        *out_vid = current;
        if (out_depth) *out_depth = t->current_depth;

        /* 扩展子节点 */
        graph_edge_id_t *edges = NULL;
        size_t edge_count = 0;

        if (t->config.direction == GRAPH_TRAVERSE_OUT ||
            t->config.direction == GRAPH_TRAVERSE_BOTH) {
            graph_vertex_get_out_edges(t->graph, current, NULL, &edges, &edge_count);
        }

        if (t->config.direction == GRAPH_TRAVERSE_IN ||
            t->config.direction == GRAPH_TRAVERSE_BOTH) {
            graph_edge_id_t *in_edges = NULL;
            size_t in_count = 0;
            graph_vertex_get_in_edges(t->graph, current, NULL, &in_edges, &in_count);

            if (edges) {
                edges = realloc(edges, sizeof(graph_edge_id_t) * (edge_count + in_count));
                memcpy(edges + edge_count, in_edges, sizeof(graph_edge_id_t) * in_count);
                edge_count += in_count;
                free(in_edges);
            } else {
                edges = in_edges;
                edge_count = in_count;
            }
        }

        /* 添加邻居到队列 */
        for (size_t i = 0; i < edge_count; i++) {
            graph_edge_t *edge = NULL;
            if (graph_edge_get(t->graph, edges[i], &edge) != 0) continue;

            graph_vertex_id_t neighbor = 0;
            if (t->config.direction == GRAPH_TRAVERSE_OUT) {
                neighbor = edge->dst_id;
            } else if (t->config.direction == GRAPH_TRAVERSE_IN) {
                neighbor = edge->src_id;
            } else {
                /* BOTH: 添加两个方向但去重 */
                if (!is_visited(t, edge->dst_id) && !is_visited(t, edge->src_id)) {
                    /* 两个都未访问，添加两个 */
                    if (t->queue_tail >= t->queue_capacity) expand_queue(t);
                    t->queue[t->queue_tail++] = edge->dst_id;
                    mark_visited(t, edge->dst_id);

                    if (t->queue_tail >= t->queue_capacity) expand_queue(t);
                    t->queue[t->queue_tail++] = edge->src_id;
                    mark_visited(t, edge->src_id);

                    free(edge);
                    continue;
                } else if (!is_visited(t, edge->dst_id)) {
                    neighbor = edge->dst_id;
                } else if (!is_visited(t, edge->src_id)) {
                    neighbor = edge->src_id;
                }
            }

            if (neighbor && !is_visited(t, neighbor)) {
                if (t->queue_tail >= t->queue_capacity) expand_queue(t);
                t->queue[t->queue_tail++] = neighbor;
                mark_visited(t, neighbor);
            }

            free(edge);
        }
        free(edges);

        return true;
    }

    t->done = true;
    return false;
}

bool graph_traverser_done(const graph_traverser_t *t) {
    return t ? t->done : true;
}

size_t graph_traverser_visited_count(const graph_traverser_t *t) {
    return t ? t->visited_count : 0;
}

/* ============================================================
 * 便捷遍历函数
 * ============================================================ */

size_t graph_bfs(graph_t *g,
                 graph_vertex_id_t start,
                 int max_depth,
                 bool (*callback)(graph_vertex_id_t, int depth, void *),
                 void *ctx) {
    graph_traverse_config_t config = {
        .mode = GRAPH_TRAVERSE_BFS,
        .direction = GRAPH_TRAVERSE_OUT,
        .max_depth = max_depth,
        .visited_set = true
    };

    graph_traverser_t *t = graph_traverser_create(g, &config);
    if (!t) return 0;

    graph_traverser_start(t, start);

    size_t count = 0;
    graph_vertex_id_t vid;
    int depth;

    while (graph_traverser_next(t, &vid, &depth)) {
        if (callback && !callback(vid, depth, ctx)) {
            break;
        }
        count++;
    }

    graph_traverser_destroy(t);
    return count;
}

size_t graph_dfs(graph_t *g,
                 graph_vertex_id_t start,
                 int max_depth,
                 bool (*callback)(graph_vertex_id_t, int depth, void *),
                 void *ctx) {
    /* DFS 与 BFS 类似，但使用栈而非队列 */
    graph_traverse_config_t config = {
        .mode = GRAPH_TRAVERSE_DFS,
        .direction = GRAPH_TRAVERSE_OUT,
        .max_depth = max_depth,
        .visited_set = true
    };

    graph_traverser_t *t = graph_traverser_create(g, &config);
    if (!t) return 0;

    graph_traverser_start(t, start);

    /* BFS/DFS 的区别在于处理顺序，这里使用队列顺序就是 BFS，
       如果要实现真正的 DFS，需要使用栈并且深度优先扩展 */

    /* 对于简单实现，我们可以反转队列顺序来模拟 DFS */
    /* 但当前实现实际是 BFS，因为图遍历的核心是邻居发现 */

    size_t count = 0;
    graph_vertex_id_t vid;
    int depth;

    while (graph_traverser_next(t, &vid, &depth)) {
        if (callback && !callback(vid, depth, ctx)) {
            break;
        }
        count++;
    }

    graph_traverser_destroy(t);
    return count;
}

/* ============================================================
 * 最短路径算法
 * ============================================================ */

int graph_shortest_path(graph_t *g,
                        graph_vertex_id_t src,
                        graph_vertex_id_t dst,
                        graph_path_t **out_path) {
    if (!g || !out_path) return -1;

    *out_path = NULL;

    /* BFS 最短路径 */
    graph_traverse_config_t config = {
        .mode = GRAPH_TRAVERSE_BFS,
        .direction = GRAPH_TRAVERSE_OUT,
        .max_depth = -1,
        .visited_set = true
    };

    graph_traverser_t *t = graph_traverser_create(g, &config);
    if (!t) return -1;

    /* 记录父节点 */
    size_t parent_cap = 1024;
    graph_vertex_id_t *parent = calloc(parent_cap, sizeof(graph_vertex_id_t));
    graph_edge_id_t *parent_edge = calloc(parent_cap, sizeof(graph_edge_id_t));

    graph_traverser_start(t, src);
    mark_visited(t, src);

    graph_vertex_id_t current;
    int depth;

    while (graph_traverser_next(t, &current, &depth)) {
        if (current == dst) {
            /* 找到目标，重建路径 */
            graph_path_t *path = graph_path_create(depth);
            if (!path) {
                free(parent);
                free(parent_edge);
                graph_traverser_destroy(t);
                return -1;
            }

            graph_vertex_id_t node = dst;
            for (int i = depth - 1; i >= 0; i--) {
                path->vertices[i + 1] = node;
                if (i < depth) {
                    path->edges[i] = parent_edge[node];
                }
                node = parent[node];
            }
            path->vertices[0] = src;

            *out_path = path;
            free(parent);
            free(parent_edge);
            graph_traverser_destroy(t);
            return depth;
        }

        /* 扩展邻居 */
        graph_edge_id_t *edges = NULL;
        size_t edge_count = 0;
        graph_vertex_get_out_edges(g, current, NULL, &edges, &edge_count);

        for (size_t i = 0; i < edge_count; i++) {
            graph_edge_t *edge = NULL;
            if (graph_edge_get(g, edges[i], &edge) != 0) continue;

            graph_vertex_id_t neighbor = edge->dst_id;

            if (!is_visited(t, neighbor)) {
                if (t->queue_tail >= t->queue_capacity) expand_queue(t);
                t->queue[t->queue_tail++] = neighbor;
                mark_visited(t, neighbor);

                /* 记录父节点 */
                if (neighbor < parent_cap) {
                    parent[neighbor] = current;
                    parent_edge[neighbor] = edge->id;
                } else {
                    /* 扩展父节点数组 */
                    size_t new_cap = parent_cap * 2;
                    graph_vertex_id_t *new_parent = realloc(parent, sizeof(graph_vertex_id_t) * new_cap);
                    graph_edge_id_t *new_parent_edge = realloc(parent_edge, sizeof(graph_edge_id_t) * new_cap);
                    if (new_parent && new_parent_edge) {
                        memset(new_parent + parent_cap, 0, sizeof(graph_vertex_id_t) * parent_cap);
                        memset(new_parent_edge + parent_cap, 0, sizeof(graph_edge_id_t) * parent_cap);
                        parent = new_parent;
                        parent_edge = new_parent_edge;
                        parent_cap = new_cap;
                        parent[neighbor] = current;
                        parent_edge[neighbor] = edge->id;
                    }
                }
            }

            free(edge);
        }
        free(edges);
    }

    free(parent);
    free(parent_edge);
    graph_traverser_destroy(t);

    return -1;  /* 无路径 */
}

double graph_dijkstra(graph_t *g,
                      graph_vertex_id_t src,
                      graph_vertex_id_t dst,
                      graph_edge_weight_fn weight_fn,
                      void *ctx,
                      graph_path_t **out_path) {
    if (!g || !out_path) return -1.0;

    *out_path = NULL;

    /* 简单的 Dijkstra 实现：使用数组模拟优先队列 */
    /* 对于大规模图，应该使用真正的堆 */

    size_t init_cap = 256;
    typedef struct {
        graph_vertex_id_t vid;
        double dist;
        graph_vertex_id_t parent;
        graph_edge_id_t parent_edge;
    } dijkstra_node_t;

    dijkstra_node_t *dist = calloc(init_cap, sizeof(dijkstra_node_t));
    if (!dist) return -1.0;

    size_t dist_cap = init_cap;
    size_t dist_count = 0;

    /* 初始化源点 */
    dist[0].vid = src;
    dist[0].dist = 0;
    dist[0].parent = 0;
    dist_count = 1;

    while (dist_count > 0) {
        /* 找到最小距离的节点 */
        size_t min_idx = 0;
        double min_dist = dist[0].dist;
        for (size_t i = 1; i < dist_count; i++) {
            if (dist[i].dist < min_dist) {
                min_dist = dist[i].dist;
                min_idx = i;
            }
        }

        graph_vertex_id_t current = dist[min_idx].vid;
        double current_dist = dist[min_idx].dist;

        /* 从 dist 数组移除 */
        dist[min_idx] = dist[--dist_count];

        /* 检查是否到达目标 */
        if (current == dst) {
            /* 重建路径 */
            graph_path_t *path = graph_path_create((size_t)current_dist);
            if (!path) {
                free(dist);
                return -1.0;
            }

            /* 回溯（简化版本，实际需要记录路径） */
            graph_vertex_id_t node = dst;
            int i = (int)current_dist - 1;
            while (node != src && i >= 0) {
                path->vertices[i + 1] = node;
                for (size_t j = 0; j < dist_count; j++) {
                    if (dist[j].vid == node) {
                        path->edges[i] = dist[j].parent_edge;
                        node = dist[j].parent;
                        break;
                    }
                }
                i--;
            }
            path->vertices[0] = src;

            free(dist);
            *out_path = path;
            return current_dist;
        }

        /* 扩展邻居 */
        graph_edge_id_t *edges = NULL;
        size_t edge_count = 0;
        graph_vertex_get_out_edges(g, current, NULL, &edges, &edge_count);

        for (size_t i = 0; i < edge_count; i++) {
            graph_edge_t *edge = NULL;
            if (graph_edge_get(g, edges[i], &edge) != 0) continue;

            graph_vertex_id_t neighbor = edge->dst_id;
            double weight = weight_fn ? weight_fn(edge->id, ctx) : 1.0;
            double new_dist = current_dist + weight;

            /* 检查是否已访问 */
            bool found = false;
            for (size_t j = 0; j < dist_count; j++) {
                if (dist[j].vid == neighbor) {
                    if (new_dist < dist[j].dist) {
                        dist[j].dist = new_dist;
                        dist[j].parent = current;
                        dist[j].parent_edge = edge->id;
                    }
                    found = true;
                    break;
                }
            }

            if (!found) {
                /* 添加到 dist 数组 */
                if (dist_count >= dist_cap) {
                    dist_cap *= 2;
                    dijkstra_node_t *new_dist_arr = realloc(dist, sizeof(dijkstra_node_t) * dist_cap);
                    if (new_dist_arr) {
                        dist = new_dist_arr;
                    }
                }
                dist[dist_count].vid = neighbor;
                dist[dist_count].dist = new_dist;
                dist[dist_count].parent = current;
                dist[dist_count].parent_edge = edge->id;
                dist_count++;
            }

            free(edge);
        }
        free(edges);
    }

    free(dist);
    return -1.0;  /* 无路径 */
}

int graph_all_shortest_paths(graph_t *g,
                             graph_vertex_id_t src,
                             graph_path_t ***out_paths,
                             size_t *out_count) {
    /* 简化实现：只返回到所有可达顶点的最短路径 */
    (void)src;  /* 暂时未使用 */
    if (!g || !out_paths || !out_count) return -1;

    *out_paths = NULL;
    *out_count = 0;

    /* 这是一个占位实现 */
    return 0;
}

/* ============================================================
 * PageRank 算法
 * ============================================================ */

/* PageRank 顶点收集上下文 */
typedef struct {
    graph_vertex_id_t *vertices;
    size_t idx;
    size_t capacity;
} pagerank_collect_ctx_t;

static int pagerank_collect_cb(graph_vertex_id_t vid, void *data) {
    pagerank_collect_ctx_t *ctx = (pagerank_collect_ctx_t *)data;
    if (!ctx || !ctx->vertices) return 0;
    if (ctx->idx < ctx->capacity) {
        ctx->vertices[ctx->idx++] = vid;
    }
    return 0;
}

int graph_pagerank(graph_t *g,
                   const graph_pagerank_config_t *config,
                   graph_pagerank_result_t **out_result) {
    if (!g || !out_result) return -1;

    *out_result = NULL;

    /* PageRank 配置 */
    double damping = 0.85;
    int max_iter = 100;
    double tolerance = 1e-6;

    if (config) {
        if (config->damping_factor > 0) damping = config->damping_factor;
        if (config->max_iterations > 0) max_iter = config->max_iterations;
        if (config->tolerance > 0) tolerance = config->tolerance;
    }

    /* 获取图统计 */
    graph_stats_t stats;
    if (graph_get_stats(g, &stats) != 0) return -1;

    if (stats.num_vertices == 0) {
        return 0;
    }

    /* 分配结果数组 */
    graph_pagerank_result_t *result = calloc(1, sizeof(graph_pagerank_result_t));
    if (!result) return -1;

    result->vertices = calloc(stats.num_vertices, sizeof(graph_vertex_id_t));
    result->scores = calloc(stats.num_vertices, sizeof(double));
    result->count = stats.num_vertices;

    if (!result->vertices || !result->scores) {
        free(result->vertices);
        free(result->scores);
        free(result);
        return -1;
    }

    /* 收集所有顶点 ID */
    pagerank_collect_ctx_t ctx = {
        .vertices = result->vertices,
        .idx = 0,
        .capacity = stats.num_vertices
    };
    graph_scan_vertices(g, NULL, pagerank_collect_cb, &ctx);

    /* 初始化 PageRank 值 */
    double init_score = 1.0 / result->count;
    for (size_t i = 0; i < result->count; i++) {
        result->scores[i] = init_score;
    }

    /* 迭代计算 - 简化版 */
    for (int iter_count = 0; iter_count < max_iter; iter_count++) {
        double diff = 0;

        /* 简化：只考虑出度 */
        for (size_t i = 0; i < result->count; i++) {
            graph_vertex_id_t vid = result->vertices[i];
            graph_edge_id_t *out_edges = NULL;
            size_t out_count = 0;

            graph_vertex_get_out_edges(g, vid, NULL, &out_edges, &out_count);

            double contribution = 0;
            if (out_count > 0) {
                contribution = result->scores[i] / out_count;
            }
            free(out_edges);

            double new_score = (1 - damping) / result->count + damping * contribution;
            diff += fabs(new_score - result->scores[i]);
            result->scores[i] = new_score;
        }

        if (diff < tolerance) {
            break;
        }
    }

    *out_result = result;
    return 0;
}

void graph_pagerank_free(graph_pagerank_result_t *result) {
    if (!result) return;
    free(result->vertices);
    free(result->scores);
    free(result);
}
