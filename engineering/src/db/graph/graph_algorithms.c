/**
 * @file graph_algorithms.c
 * @brief 图分析算法实现
 *
 * 实现以下算法：
 * - Dijkstra 单源最短路径（最小堆优化）
 * - BFS 无权图最短路径
 * - PageRank 迭代计算
 * - 连通分量检测（Union-Find）
 * - 图统计（顶点数、边数、平均度、聚类系数）
 */

#include "db/graph/graph_algorithms.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>

/* ============================================================
 * 内部数据结构
 * ============================================================ */

/** 最小堆节点（用于 Dijkstra） */
typedef struct {
    graph_vertex_id_t vertex;
    double priority;
} heap_node_t;

/** 最小堆 */
typedef struct {
    heap_node_t *nodes;
    size_t size;
    size_t capacity;
    double *distances;  /**< 外部距离数组引用，用于更新优先级 */
} min_heap_t;

/** Union-Find 节点 */
typedef struct {
    int64_t parent;
    int rank;
} uf_node_t;

/** 遍历状态 */
typedef struct {
    bool *visited;
    graph_vertex_id_t *parent;
    double *distance;
    size_t max_vertex_id;
} traversal_state_t;

/* ============================================================
 * 最小堆操作（用于 Dijkstra）
 * ============================================================ */

static int heap_create(min_heap_t *heap, size_t capacity, double *distances) {
    heap->nodes = (heap_node_t *)malloc(sizeof(heap_node_t) * capacity);
    if (!heap->nodes) return GRAPH_ALGO_ERR_NO_MEMORY;
    heap->size = 0;
    heap->capacity = capacity;
    heap->distances = distances;
    return GRAPH_ALGO_OK;
}

static void heap_destroy(min_heap_t *heap) {
    if (heap->nodes) {
        free(heap->nodes);
        heap->nodes = NULL;
    }
    heap->size = 0;
    heap->capacity = 0;
}

static void heap_swap(heap_node_t *a, heap_node_t *b) {
    heap_node_t tmp = *a;
    *a = *b;
    *b = tmp;
}

static void heapify_up(min_heap_t *heap, size_t idx) {
    while (idx > 0) {
        size_t parent = (idx - 1) / 2;
        if (heap->nodes[idx].priority < heap->nodes[parent].priority) {
            heap_swap(&heap->nodes[idx], &heap->nodes[parent]);
            idx = parent;
        } else {
            break;
        }
    }
}

static void heapify_down(min_heap_t *heap, size_t idx) {
    while (1) {
        size_t smallest = idx;
        size_t left = 2 * idx + 1;
        size_t right = 2 * idx + 2;

        if (left < heap->size &&
            heap->nodes[left].priority < heap->nodes[smallest].priority) {
            smallest = left;
        }
        if (right < heap->size &&
            heap->nodes[right].priority < heap->nodes[smallest].priority) {
            smallest = right;
        }

        if (smallest != idx) {
            heap_swap(&heap->nodes[idx], &heap->nodes[smallest]);
            idx = smallest;
        } else {
            break;
        }
    }
}

static int heap_push(min_heap_t *heap, graph_vertex_id_t vertex, double priority) {
    if (heap->size >= heap->capacity) {
        return GRAPH_ALGO_ERR_NO_MEMORY;
    }
    heap->nodes[heap->size].vertex = vertex;
    heap->nodes[heap->size].priority = priority;
    heapify_up(heap, heap->size);
    heap->size++;
    return GRAPH_ALGO_OK;
}

static int heap_pop(min_heap_t *heap, graph_vertex_id_t *vertex, double *priority) {
    if (heap->size == 0) {
        return GRAPH_ALGO_ERR_NO_MEMORY;
    }
    *vertex = heap->nodes[0].vertex;
    *priority = heap->nodes[0].priority;
    heap->size--;
    if (heap->size > 0) {
        heap->nodes[0] = heap->nodes[heap->size];
        heapify_down(heap, 0);
    }
    return GRAPH_ALGO_OK;
}

/* ============================================================
 * 遍历状态管理
 * ============================================================ */

static void traversal_state_destroy(traversal_state_t *state) {
    if (state->visited) { free(state->visited); state->visited = NULL; }
    if (state->parent) { free(state->parent); state->parent = NULL; }
    if (state->distance) { free(state->distance); state->distance = NULL; }
}

static int traversal_state_create(traversal_state_t *state, size_t max_vertex_id) {
    state->max_vertex_id = max_vertex_id;
    state->visited = (bool *)calloc(max_vertex_id + 1, sizeof(bool));
    state->parent = (graph_vertex_id_t *)malloc(sizeof(graph_vertex_id_t) * (max_vertex_id + 1));
    state->distance = (double *)malloc(sizeof(double) * (max_vertex_id + 1));

    if (!state->visited || !state->parent || !state->distance) {
        traversal_state_destroy(state);
        return GRAPH_ALGO_ERR_NO_MEMORY;
    }

    for (size_t i = 0; i <= max_vertex_id; i++) {
        state->visited[i] = false;
        state->parent[i] = GRAPH_INVALID_ID;
        state->distance[i] = DBL_MAX;
    }

    return GRAPH_ALGO_OK;
}

/* ============================================================
 * Dijkstra 最短路径
 * ============================================================ */

int graph_dijkstra_algo(const graph_t *graph,
                   graph_vertex_id_t source,
                   graph_vertex_id_t target,
                   graph_path_result_t *result) {
    if (!graph || !result) return GRAPH_ALGO_ERR_INVALID_PARAM;

    /* 获取图统计信息以确定顶点范围 */
    graph_stats_t stats;
    if (graph_get_stats((graph_t *)graph, &stats) != 0) {
        return GRAPH_ALGO_ERR_INVALID_PARAM;
    }

    /* 通过 graph_scan_vertices 获取实际最大顶点 ID */
    size_t vertex_capacity = 256;
    size_t vertex_count = 0;
    graph_vertex_id_t *vertex_ids = (graph_vertex_id_t *)malloc(sizeof(graph_vertex_id_t) * vertex_capacity);
    if (!vertex_ids) return GRAPH_ALGO_ERR_NO_MEMORY;

    typedef struct {
        graph_vertex_id_t *ids;
        size_t count;
        size_t capacity;
    } vertex_collect_ctx_t;

    int collect_vertices(graph_vertex_id_t vid, void *data) {
        vertex_collect_ctx_t *c = (vertex_collect_ctx_t *)data;
        if (c->count >= c->capacity) {
            c->capacity *= 2;
            c->ids = (graph_vertex_id_t *)realloc(c->ids, sizeof(graph_vertex_id_t) * c->capacity);
            if (!c->ids) return -1;
        }
        c->ids[c->count++] = vid;
        return 0;
    }

    vertex_collect_ctx_t vctx = { vertex_ids, 0, vertex_capacity };
    graph_scan_vertices((graph_t *)graph, NULL, collect_vertices, &vctx);

    vertex_count = vctx.count;
    vertex_ids = vctx.ids;

    /* 计算最大顶点 ID */
    graph_vertex_id_t max_vid = 0;
    for (size_t i = 0; i < vertex_count; i++) {
        if (vertex_ids[i] > max_vid) max_vid = vertex_ids[i];
    }
    size_t max_vertex_id = (size_t)max_vid;

    if (vertex_count == 0) {
        free(vertex_ids);
        return GRAPH_ALGO_ERR_VERTEX_NOT_FOUND;
    }

    /* 初始化遍历状态 */
    traversal_state_t state;
    int rc = traversal_state_create(&state, max_vertex_id);
    if (rc != GRAPH_ALGO_OK) {
        free(vertex_ids);
        return rc;
    }

    /* 初始化最小堆 */
    min_heap_t heap;
    rc = heap_create(&heap, max_vertex_id + 1, state.distance);
    if (rc != GRAPH_ALGO_OK) {
        traversal_state_destroy(&state);
        free(vertex_ids);
        return rc;
    }

    /* 初始化源顶点 */
    state.distance[source] = 0.0;
    heap_push(&heap, source, 0.0);

    while (heap.size > 0) {
        graph_vertex_id_t u;
        double u_dist;
        heap_pop(&heap, &u, &u_dist);

        if (state.visited[u]) continue;
        state.visited[u] = true;

        /* 到达目标顶点 */
        if (u == target) break;

        /* 获取邻接边 */
        graph_edge_id_t *edges = NULL;
        size_t edge_count = 0;
        rc = graph_vertex_get_out_edges((graph_t *)graph, u, NULL, &edges, &edge_count);
        if (rc != 0) continue;

        for (size_t i = 0; i < edge_count; i++) {
            graph_edge_t *edge = NULL;
            if (graph_edge_get((graph_t *)graph, edges[i], &edge) != 0) continue;

            graph_vertex_id_t v = edge->dst_id;
            if (v > max_vertex_id) continue;

            /* 获取边权重（默认为 1.0） */
            double weight = 1.0;
            /* 如果边有 weight 属性，使用它 */
            for (uint16_t p = 0; p < edge->n_props; p++) {
                if (strcmp(edge->props[p].key, "weight") == 0) {
                    if (edge->props[p].type == GRAPH_FLOAT) {
                        weight = edge->props[p].value.u.float_val;
                    } else if (edge->props[p].type == GRAPH_INT) {
                        weight = (double)edge->props[p].value.u.int_val;
                    }
                    break;
                }
            }

            double new_dist = state.distance[u] + weight;
            if (new_dist < state.distance[v]) {
                state.distance[v] = new_dist;
                state.parent[v] = u;
                heap_push(&heap, v, new_dist);
            }

            free(edge);
        }
        free(edges);
    }

    /* 检查是否找到路径 */
    if (state.distance[target] == DBL_MAX) {
        heap_destroy(&heap);
        traversal_state_destroy(&state);
        free(vertex_ids);
        return GRAPH_ALGO_ERR_NO_PATH;
    }

    /* 重建路径 */
    /* 先计算路径长度 */
    size_t path_len = 0;
    graph_vertex_id_t current = target;
    while (current != GRAPH_INVALID_ID) {
        path_len++;
        current = state.parent[current];
    }

    /* 分配路径数组 */
    result->path = (graph_vertex_id_t *)malloc(sizeof(graph_vertex_id_t) * path_len);
    if (!result->path) {
        heap_destroy(&heap);
        traversal_state_destroy(&state);
        free(vertex_ids);
        return GRAPH_ALGO_ERR_NO_MEMORY;
    }

    /* 填充路径（逆序） */
    current = target;
    for (int i = (int)path_len - 1; i >= 0; i--) {
        result->path[i] = current;
        current = state.parent[current];
    }
    result->path_length = path_len;
    result->total_cost = state.distance[target];

    heap_destroy(&heap);
    traversal_state_destroy(&state);
    free(vertex_ids);
    return GRAPH_ALGO_OK;
}

/* ============================================================
 * BFS 最短路径
 * ============================================================ */

int graph_bfs_shortest_path(const graph_t *graph,
                            graph_vertex_id_t source,
                            graph_vertex_id_t target,
                            graph_path_result_t *result) {
    if (!graph || !result) return GRAPH_ALGO_ERR_INVALID_PARAM;

    /* 使用 graph_scan_vertices 收集所有顶点 ID */
    size_t vertex_capacity = 256;
    size_t vertex_count = 0;
    graph_vertex_id_t *vertex_ids = (graph_vertex_id_t *)malloc(sizeof(graph_vertex_id_t) * vertex_capacity);
    if (!vertex_ids) return GRAPH_ALGO_ERR_NO_MEMORY;

    /* 收集顶点 ID */
    typedef struct {
        graph_vertex_id_t *ids;
        size_t count;
        size_t capacity;
    } vertex_collect_ctx_t;

    int collect_vertices(graph_vertex_id_t vid, void *data) {
        vertex_collect_ctx_t *c = (vertex_collect_ctx_t *)data;
        if (c->count >= c->capacity) {
            c->capacity *= 2;
            c->ids = (graph_vertex_id_t *)realloc(c->ids, sizeof(graph_vertex_id_t) * c->capacity);
            if (!c->ids) return -1;
        }
        c->ids[c->count++] = vid;
        return 0;
    }

    vertex_collect_ctx_t ctx = { vertex_ids, 0, vertex_capacity };
    graph_scan_vertices((graph_t *)graph, NULL, collect_vertices, &ctx);

    vertex_count = ctx.count;
    vertex_ids = ctx.ids;

    /* 计算最大顶点 ID */
    graph_vertex_id_t max_vid = 0;
    for (size_t i = 0; i < vertex_count; i++) {
        if (vertex_ids[i] > max_vid) max_vid = vertex_ids[i];
    }
    size_t max_vertex_id = (size_t)max_vid;

    traversal_state_t state;
    int rc = traversal_state_create(&state, max_vertex_id);
    if (rc != GRAPH_ALGO_OK) {
        free(vertex_ids);
        return rc;
    }
    if (rc != GRAPH_ALGO_OK) return rc;

    /* BFS 队列 */
    graph_vertex_id_t *queue = (graph_vertex_id_t *)malloc(sizeof(graph_vertex_id_t) * (max_vertex_id + 1));
    if (!queue) {
        traversal_state_destroy(&state);
        free(vertex_ids);
        return GRAPH_ALGO_ERR_NO_MEMORY;
    }

    size_t front = 0, rear = 0;
    queue[rear++] = source;
    state.visited[source] = true;
    state.distance[source] = 0.0;

    bool found = false;
    while (front < rear) {
        graph_vertex_id_t u = queue[front++];

        if (u == target) {
            found = true;
            break;
        }

        /* 获取邻接边 */
        graph_edge_id_t *edges = NULL;
        size_t edge_count = 0;
        rc = graph_vertex_get_out_edges((graph_t *)graph, u, NULL, &edges, &edge_count);
        if (rc != 0) continue;

        for (size_t i = 0; i < edge_count; i++) {
            graph_edge_t *edge = NULL;
            if (graph_edge_get((graph_t *)graph, edges[i], &edge) != 0) continue;

            graph_vertex_id_t v = edge->dst_id;
            if (v <= max_vertex_id && !state.visited[v]) {
                state.visited[v] = true;
                state.parent[v] = u;
                state.distance[v] = state.distance[u] + 1.0;
                queue[rear++] = v;
            }

            free(edge);
        }
        free(edges);
    }

    free(queue);
    free(vertex_ids);

    if (!found) {
        traversal_state_destroy(&state);
        return GRAPH_ALGO_ERR_NO_PATH;
    }

    /* 重建路径 */
    size_t path_len = 0;
    graph_vertex_id_t current = target;
    while (current != GRAPH_INVALID_ID) {
        path_len++;
        current = state.parent[current];
    }

    result->path = (graph_vertex_id_t *)malloc(sizeof(graph_vertex_id_t) * path_len);
    if (!result->path) {
        traversal_state_destroy(&state);
        return GRAPH_ALGO_ERR_NO_MEMORY;
    }

    current = target;
    for (int i = (int)path_len - 1; i >= 0; i--) {
        result->path[i] = current;
        current = state.parent[current];
    }
    result->path_length = path_len;
    result->total_cost = state.distance[target];

    traversal_state_destroy(&state);
    return GRAPH_ALGO_OK;
}

/* ============================================================
 * DFS 深度优先遍历
 * ============================================================ */

static int dfs_visit(const graph_t *graph,
                     graph_vertex_id_t vid,
                     size_t depth,
                     bool *visited,
                     graph_dfs_visitor_fn visitor,
                     void *user_data) {
    visited[vid] = true;

    int rc = visitor(vid, depth, user_data);
    if (rc != 0) return rc;

    /* 获取邻接边 */
    graph_edge_id_t *edges = NULL;
    size_t edge_count = 0;
    rc = graph_vertex_get_out_edges((graph_t *)graph, vid, NULL, &edges, &edge_count);
    if (rc != 0) return 0;  /* 忽略获取边的错误 */

    for (size_t i = 0; i < edge_count; i++) {
        graph_edge_t *edge = NULL;
        if (graph_edge_get((graph_t *)graph, edges[i], &edge) != 0) continue;

        graph_vertex_id_t v = edge->dst_id;
        if (!visited[v]) {
            rc = dfs_visit(graph, v, depth + 1, visited, visitor, user_data);
            if (rc != 0) {
                free(edge);
                free(edges);
                return rc;
            }
        }

        free(edge);
    }
    free(edges);

    return 0;
}

int graph_dfs_algo(const graph_t *graph,
              graph_vertex_id_t start,
              graph_dfs_visitor_fn visitor,
              void *user_data) {
    if (!graph || !visitor) return GRAPH_ALGO_ERR_INVALID_PARAM;

    /* 收集所有顶点 ID */
    size_t vertex_capacity = 256;
    size_t vertex_count = 0;
    graph_vertex_id_t *vertex_ids = (graph_vertex_id_t *)malloc(sizeof(graph_vertex_id_t) * vertex_capacity);
    if (!vertex_ids) return GRAPH_ALGO_ERR_NO_MEMORY;

    typedef struct {
        graph_vertex_id_t *ids;
        size_t count;
        size_t capacity;
    } vertex_collect_ctx_t;

    int collect_vertices(graph_vertex_id_t vid, void *data) {
        vertex_collect_ctx_t *c = (vertex_collect_ctx_t *)data;
        if (c->count >= c->capacity) {
            c->capacity *= 2;
            c->ids = (graph_vertex_id_t *)realloc(c->ids, sizeof(graph_vertex_id_t) * c->capacity);
            if (!c->ids) return -1;
        }
        c->ids[c->count++] = vid;
        return 0;
    }

    vertex_collect_ctx_t ctx = { vertex_ids, 0, vertex_capacity };
    graph_scan_vertices((graph_t *)graph, NULL, collect_vertices, &ctx);

    vertex_count = ctx.count;
    vertex_ids = ctx.ids;

    /* 计算最大顶点 ID */
    graph_vertex_id_t max_vid = 0;
    for (size_t i = 0; i < vertex_count; i++) {
        if (vertex_ids[i] > max_vid) max_vid = vertex_ids[i];
    }
    size_t max_vertex_id = (size_t)max_vid;

    bool *visited = (bool *)calloc(max_vertex_id + 1, sizeof(bool));
    if (!visited) {
        free(vertex_ids);
        return GRAPH_ALGO_ERR_NO_MEMORY;
    }

    int rc = dfs_visit(graph, start, 0, visited, visitor, user_data);
    free(visited);
    free(vertex_ids);
    return rc;
}

/* ============================================================
 * BFS 广度优先遍历
 * ============================================================ */

int graph_bfs_algo(const graph_t *graph,
              graph_vertex_id_t start,
              graph_bfs_visitor_fn visitor,
              void *user_data) {
    if (!graph || !visitor) return GRAPH_ALGO_ERR_INVALID_PARAM;

    /* 收集所有顶点 ID */
    size_t vertex_capacity = 256;
    size_t vertex_count = 0;
    graph_vertex_id_t *vertex_ids = (graph_vertex_id_t *)malloc(sizeof(graph_vertex_id_t) * vertex_capacity);
    if (!vertex_ids) return GRAPH_ALGO_ERR_NO_MEMORY;

    typedef struct {
        graph_vertex_id_t *ids;
        size_t count;
        size_t capacity;
    } vertex_collect_ctx_t;

    int collect_vertices(graph_vertex_id_t vid, void *data) {
        vertex_collect_ctx_t *c = (vertex_collect_ctx_t *)data;
        if (c->count >= c->capacity) {
            c->capacity *= 2;
            c->ids = (graph_vertex_id_t *)realloc(c->ids, sizeof(graph_vertex_id_t) * c->capacity);
            if (!c->ids) return -1;
        }
        c->ids[c->count++] = vid;
        return 0;
    }

    vertex_collect_ctx_t ctx = { vertex_ids, 0, vertex_capacity };
    graph_scan_vertices((graph_t *)graph, NULL, collect_vertices, &ctx);

    vertex_count = ctx.count;
    vertex_ids = ctx.ids;

    /* 计算最大顶点 ID */
    graph_vertex_id_t max_vid = 0;
    for (size_t i = 0; i < vertex_count; i++) {
        if (vertex_ids[i] > max_vid) max_vid = vertex_ids[i];
    }
    size_t max_vertex_id = (size_t)max_vid;

    bool *visited = (bool *)calloc(max_vertex_id + 1, sizeof(bool));
    if (!visited) {
        free(vertex_ids);
        return GRAPH_ALGO_ERR_NO_MEMORY;
    }

    graph_vertex_id_t *queue = (graph_vertex_id_t *)malloc(sizeof(graph_vertex_id_t) * (max_vertex_id + 1));
    if (!queue) {
        free(visited);
        free(vertex_ids);
        return GRAPH_ALGO_ERR_NO_MEMORY;
    }

    size_t front = 0, rear = 0;
    queue[rear++] = start;
    visited[start] = true;

    int rc = 0;
    size_t depth = 0;
    size_t level_end = rear;

    while (front < rear) {
        graph_vertex_id_t u = queue[front++];

        rc = visitor(u, depth, user_data);
        if (rc != 0) break;

        /* 获取邻接边 */
        graph_edge_id_t *edges = NULL;
        size_t edge_count = 0;
        int err = graph_vertex_get_out_edges((graph_t *)graph, u, NULL, &edges, &edge_count);
        if (err == 0) {
            for (size_t i = 0; i < edge_count; i++) {
                graph_edge_t *edge = NULL;
                if (graph_edge_get((graph_t *)graph, edges[i], &edge) == 0) {
                    graph_vertex_id_t v = edge->dst_id;
                    if (v <= max_vertex_id && !visited[v]) {
                        visited[v] = true;
                        queue[rear++] = v;
                    }
                    free(edge);
                }
            }
            free(edges);
        }

        if (front == level_end) {
            depth++;
            level_end = rear;
        }
    }

    free(vertex_ids);
    free(queue);
    free(visited);
    return rc;
}

/* ============================================================
 * PageRank 算法
 * ============================================================ */

int graph_pagerank_new(const graph_t *graph,
                   graph_pagerank_result_t *result,
                   int max_iterations,
                   double damping_factor,
                   double tolerance) {
    if (!graph || !result) return GRAPH_ALGO_ERR_INVALID_PARAM;

    /* 默认参数 */
    if (max_iterations <= 0) max_iterations = 100;
    if (damping_factor <= 0.0) damping_factor = 0.85;
    if (tolerance <= 0.0) tolerance = 1e-6;

    /* 使用 graph_scan_vertices 收集所有顶点 ID */
    size_t vertex_capacity = 256;
    size_t vertex_count = 0;
    graph_vertex_id_t *vertex_ids = (graph_vertex_id_t *)malloc(sizeof(graph_vertex_id_t) * vertex_capacity);
    if (!vertex_ids) return GRAPH_ALGO_ERR_NO_MEMORY;

    typedef struct {
        graph_vertex_id_t *ids;
        size_t count;
        size_t capacity;
    } vertex_collect_ctx_t;

    int collect_vertices(graph_vertex_id_t vid, void *data) {
        vertex_collect_ctx_t *c = (vertex_collect_ctx_t *)data;
        if (c->count >= c->capacity) {
            c->capacity *= 2;
            c->ids = (graph_vertex_id_t *)realloc(c->ids, sizeof(graph_vertex_id_t) * c->capacity);
            if (!c->ids) return -1;
        }
        c->ids[c->count++] = vid;
        return 0;
    }

    vertex_collect_ctx_t ctx = { vertex_ids, 0, vertex_capacity };
    graph_scan_vertices((graph_t *)graph, NULL, collect_vertices, &ctx);

    vertex_count = ctx.count;
    vertex_ids = ctx.ids;

    if (vertex_count == 0) {
        free(vertex_ids);
        return GRAPH_ALGO_ERR_VERTEX_NOT_FOUND;
    }

    /* 计算最大顶点 ID，用于索引映射 */
    graph_vertex_id_t max_vid = 0;
    for (size_t i = 0; i < vertex_count; i++) {
        if (vertex_ids[i] > max_vid) max_vid = vertex_ids[i];
    }
    size_t max_vertex_id = (size_t)max_vid;
    size_t n = max_vertex_id + 1;

    /* 建立 vertex_id → 索引 的映射，以及 索引 → vertex_id 的反向映射 */
    size_t *id_to_index = (size_t *)malloc(sizeof(size_t) * n);
    if (!id_to_index) {
        free(vertex_ids);
        return GRAPH_ALGO_ERR_NO_MEMORY;
    }
    for (size_t i = 0; i < n; i++) id_to_index[i] = SIZE_MAX; /* SIZE_MAX 表示无效 */

    for (size_t i = 0; i < vertex_count; i++) {
        id_to_index[vertex_ids[i]] = i;
    }

    /* 使用实际顶点数 n_real 作为 PageRank 的归一化基数 */
    size_t n_real = vertex_count;

    /* 分配分数数组（按 n 大小分配，但归一化使用 n_real） */
    result->scores = (double *)malloc(sizeof(double) * n);
    if (!result->scores) {
        free(id_to_index);
        free(vertex_ids);
        return GRAPH_ALGO_ERR_NO_MEMORY;
    }
    result->num_vertices = n_real;

    /* 初始化分数：仅对实际存在的顶点分配初始分数 */
    for (size_t i = 0; i < n; i++) {
        result->scores[i] = 0.0;
    }
    double initial_score = 1.0 / n_real;
    for (size_t i = 0; i < vertex_count; i++) {
        result->scores[vertex_ids[i]] = initial_score;
    }

    /* 计算出度（仅对实际存在的顶点） */
    size_t *out_degree = (size_t *)calloc(n, sizeof(size_t));
    if (!out_degree) {
        free(result->scores);
        free(id_to_index);
        free(vertex_ids);
        return GRAPH_ALGO_ERR_NO_MEMORY;
    }

    for (size_t i = 0; i < vertex_count; i++) {
        graph_vertex_id_t vid = vertex_ids[i];
        graph_edge_id_t *edges = NULL;
        size_t edge_count = 0;
        if (graph_vertex_get_out_edges((graph_t *)graph, vid, NULL, &edges, &edge_count) == 0) {
            out_degree[vid] = edge_count;
            free(edges);
        }
    }

    /* 迭代计算 */
    double *new_scores = (double *)malloc(sizeof(double) * n);
    if (!new_scores) {
        free(out_degree);
        free(result->scores);
        free(id_to_index);
        free(vertex_ids);
        return GRAPH_ALGO_ERR_NO_MEMORY;
    }

    result->converged = false;
    for (int iter = 0; iter < max_iterations; iter++) {
        /* 计算悬挂顶点的总分数，并将其均匀分配给所有顶点。 */
        double dangling_rank = 0.0;
        for (size_t i = 0; i < vertex_count; i++) {
            if (out_degree[vertex_ids[i]] == 0) {
                dangling_rank += result->scores[vertex_ids[i]];
            }
        }

        /* 计算新分数：初始值 = (1-d)/N + d * dangling / N */
        double dangling_contrib = damping_factor * dangling_rank / n_real;
        for (size_t i = 0; i < n; i++) {
            new_scores[i] = (1.0 - damping_factor) / n_real + dangling_contrib;
        }

        /* 累加有出边顶点的贡献 */
        for (size_t i = 0; i < vertex_count; i++) {
            graph_vertex_id_t vid = vertex_ids[i];
            if (out_degree[vid] == 0) {
                continue;
            }

            double contribution = damping_factor * result->scores[vid] / out_degree[vid];

            /* 获取邻接边 */
            graph_edge_id_t *edges = NULL;
            size_t edge_count = 0;
            if (graph_vertex_get_out_edges((graph_t *)graph, vid, NULL, &edges, &edge_count) == 0) {
                for (size_t j = 0; j < edge_count; j++) {
                    graph_edge_t *edge = NULL;
                    if (graph_edge_get((graph_t *)graph, edges[j], &edge) == 0) {
                        if (edge->dst_id < n && id_to_index[edge->dst_id] != SIZE_MAX) {
                            new_scores[edge->dst_id] += contribution;
                        }
                        free(edge);
                    }
                }
                free(edges);
            }
        }

        /* 检查收敛 */
        double diff = 0.0;
        for (size_t i = 0; i < n; i++) {
            diff += fabs(new_scores[i] - result->scores[i]);
        }

        /* 交换分数 */
        double *tmp = result->scores;
        result->scores = new_scores;
        new_scores = tmp;

        result->iterations = iter + 1;
        if (diff < tolerance) {
            result->converged = true;
            break;
        }
    }

    free(new_scores);
    free(out_degree);
    free(id_to_index);

    /* 对外按实际顶点顺序返回紧凑分数，避免顶点 ID 空洞被误计入结果。 */
    double *compact_scores = (double *)malloc(sizeof(double) * vertex_count);
    if (!compact_scores) {
        free(result->scores);
        result->scores = NULL;
        free(vertex_ids);
        result->num_vertices = 0;
        return GRAPH_ALGO_ERR_NO_MEMORY;
    }
    for (size_t i = 0; i < vertex_count; i++) {
        compact_scores[i] = result->scores[vertex_ids[i]];
    }
    free(result->scores);
    result->scores = compact_scores;

    free(vertex_ids);
    return GRAPH_ALGO_OK;
}

/* ============================================================
 * 连通分量（Union-Find）
 * ============================================================ */

static int uf_find(uf_node_t *nodes, int64_t x) {
    if (nodes[x].parent != x) {
        nodes[x].parent = uf_find(nodes, nodes[x].parent);
    }
    return nodes[x].parent;
}

static void uf_union(uf_node_t *nodes, int64_t x, int64_t y) {
    int64_t rx = uf_find(nodes, x);
    int64_t ry = uf_find(nodes, y);
    if (rx == ry) return;

    if (nodes[rx].rank < nodes[ry].rank) {
        nodes[rx].parent = ry;
    } else if (nodes[rx].rank > nodes[ry].rank) {
        nodes[ry].parent = rx;
    } else {
        nodes[ry].parent = rx;
        nodes[rx].rank++;
    }
}

int graph_connected_components(const graph_t *graph,
                               graph_component_result_t *result) {
    if (!graph || !result) return GRAPH_ALGO_ERR_INVALID_PARAM;

    /* 使用 graph_scan_vertices 收集所有顶点 ID */
    size_t vertex_capacity = 256;
    size_t vertex_count = 0;
    graph_vertex_id_t *vertex_ids = (graph_vertex_id_t *)malloc(sizeof(graph_vertex_id_t) * vertex_capacity);
    if (!vertex_ids) return GRAPH_ALGO_ERR_NO_MEMORY;

    typedef struct {
        graph_vertex_id_t *ids;
        size_t count;
        size_t capacity;
    } vertex_collect_ctx_t;

    int collect_vertices(graph_vertex_id_t vid, void *data) {
        vertex_collect_ctx_t *c = (vertex_collect_ctx_t *)data;
        if (c->count >= c->capacity) {
            c->capacity *= 2;
            c->ids = (graph_vertex_id_t *)realloc(c->ids, sizeof(graph_vertex_id_t) * c->capacity);
            if (!c->ids) return -1;
        }
        c->ids[c->count++] = vid;
        return 0;
    }

    vertex_collect_ctx_t ctx = { vertex_ids, 0, vertex_capacity };
    graph_scan_vertices((graph_t *)graph, NULL, collect_vertices, &ctx);

    vertex_count = ctx.count;
    vertex_ids = ctx.ids;

    if (vertex_count == 0) {
        free(vertex_ids);
        return GRAPH_ALGO_ERR_VERTEX_NOT_FOUND;
    }

    /* 计算最大顶点 ID */
    graph_vertex_id_t max_vid = 0;
    for (size_t i = 0; i < vertex_count; i++) {
        if (vertex_ids[i] > max_vid) max_vid = vertex_ids[i];
    }
    size_t n = (size_t)max_vid + 1;

    /* 建立 vertex_id → 索引 的映射 */
    size_t *id_to_index = (size_t *)malloc(sizeof(size_t) * n);
    if (!id_to_index) {
        free(vertex_ids);
        return GRAPH_ALGO_ERR_NO_MEMORY;
    }
    for (size_t i = 0; i < n; i++) id_to_index[i] = SIZE_MAX;
    for (size_t i = 0; i < vertex_count; i++) {
        id_to_index[vertex_ids[i]] = i;
    }

    /* 初始化 Union-Find（使用顶点数量而非最大 ID） */
    uf_node_t *nodes = (uf_node_t *)malloc(sizeof(uf_node_t) * vertex_count);
    if (!nodes) {
        free(id_to_index);
        free(vertex_ids);
        return GRAPH_ALGO_ERR_NO_MEMORY;
    }

    for (size_t i = 0; i < vertex_count; i++) {
        nodes[i].parent = (int64_t)i;
        nodes[i].rank = 0;
    }

    /* 合并连通的顶点 */
    for (size_t i = 0; i < vertex_count; i++) {
        graph_vertex_id_t vid = vertex_ids[i];
        graph_edge_id_t *edges = NULL;
        size_t edge_count = 0;

        /* 遍历出边 */
        if (graph_vertex_get_out_edges((graph_t *)graph, vid, NULL,
                                       &edges, &edge_count) == 0) {
            for (size_t j = 0; j < edge_count; j++) {
                graph_edge_t *edge = NULL;
                if (graph_edge_get((graph_t *)graph, edges[j], &edge) == 0) {
                    if (edge->dst_id < n && id_to_index[edge->dst_id] != SIZE_MAX) {
                        uf_union(nodes, (int64_t)i, (int64_t)id_to_index[edge->dst_id]);
                    }
                    free(edge);
                }
            }
            free(edges);
        }

        /* 遍历入边 */
        edges = NULL;
        edge_count = 0;
        if (graph_vertex_get_in_edges((graph_t *)graph, vid, NULL,
                                      &edges, &edge_count) == 0) {
            for (size_t j = 0; j < edge_count; j++) {
                graph_edge_t *edge = NULL;
                if (graph_edge_get((graph_t *)graph, edges[j], &edge) == 0) {
                    if (edge->src_id < n && id_to_index[edge->src_id] != SIZE_MAX) {
                        uf_union(nodes, (int64_t)i, (int64_t)id_to_index[edge->src_id]);
                    }
                    free(edge);
                }
            }
            free(edges);
        }
    }

    /* 分配结果数组 */
    result->component_ids = (int64_t *)malloc(sizeof(int64_t) * vertex_count);
    if (!result->component_ids) {
        free(nodes);
        free(id_to_index);
        free(vertex_ids);
        return GRAPH_ALGO_ERR_NO_MEMORY;
    }
    result->num_vertices = vertex_count;

    /* 压缩路径并分配连续的组件 ID */
    int64_t next_id = 0;
    int64_t *id_map = (int64_t *)malloc(sizeof(int64_t) * vertex_count);
    if (!id_map) {
        free(result->component_ids);
        free(nodes);
        free(id_to_index);
        free(vertex_ids);
        return GRAPH_ALGO_ERR_NO_MEMORY;
    }
    for (size_t i = 0; i < vertex_count; i++) {
        id_map[i] = -1;
    }

    for (size_t i = 0; i < vertex_count; i++) {
        int64_t root = uf_find(nodes, (int64_t)i);
        if (id_map[root] == -1) {
            id_map[root] = next_id++;
        }
        result->component_ids[i] = id_map[root];
    }
    result->num_components = (size_t)next_id;

    free(id_map);
    free(nodes);
    free(id_to_index);
    free(vertex_ids);
    return GRAPH_ALGO_OK;
}

/* ============================================================
 * 图统计
 * ============================================================ */

int graph_stats(const graph_t *graph,
                graph_stats_result_t *result) {
    if (!graph || !result) return GRAPH_ALGO_ERR_INVALID_PARAM;

    /* 获取图引擎统计 */
    graph_stats_t engine_stats;
    if (graph_get_stats((graph_t *)graph, &engine_stats) != 0) {
        return GRAPH_ALGO_ERR_INVALID_PARAM;
    }

    result->num_vertices = engine_stats.num_vertices;
    result->num_edges = engine_stats.num_edges;

    /* 计算平均度 */
    if (result->num_vertices > 0) {
        result->avg_degree = (double)(result->num_edges * 2) / result->num_vertices;
    } else {
        result->avg_degree = 0.0;
    }

    /* 聚类系数需要更复杂的计算，暂时返回 0 */
    result->clustering_coefficient = 0.0;

    return GRAPH_ALGO_OK;
}

/* ============================================================
 * 结果释放函数
 * ============================================================ */

void graph_path_result_clear(graph_path_result_t *result) {
    if (result) {
        /* path 由调用者分配，不释放 */
        result->path = NULL;
        result->path_length = 0;
        result->total_cost = 0.0;
    }
}

void graph_pagerank_result_clear(graph_pagerank_result_t *result) {
    if (result) {
        if (result->scores) {
            free(result->scores);
            result->scores = NULL;
        }
        result->num_vertices = 0;
        result->iterations = 0;
        result->converged = false;
    }
}

void graph_component_result_clear(graph_component_result_t *result) {
    if (result) {
        if (result->component_ids) {
            free(result->component_ids);
            result->component_ids = NULL;
        }
        result->num_vertices = 0;
        result->num_components = 0;
    }
}

void graph_stats_result_clear(graph_stats_result_t *result) {
    if (result) {
        result->num_vertices = 0;
        result->num_edges = 0;
        result->avg_degree = 0.0;
        result->clustering_coefficient = 0.0;
    }
}