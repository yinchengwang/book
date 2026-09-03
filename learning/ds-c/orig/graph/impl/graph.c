/*
 * graph.c —— 图实现（邻接表）
 *
 * ============================================================
 * 数据结构内部定义
 * ============================================================
 *
 * 邻接表的结构：
 *
 *   struct ds_graph {
 *       ds_graph_adj_node_t **adj_list;  // 邻接表数组
 *       size_t   vertices;               // 顶点数量
 *       bool     directed;               // 有向/无向
 *   };
 *
 * 邻接表节点（链式存储）：
 *
 *   struct ds_graph_adj_node {
 *       size_t                  to;       // 目标顶点
 *       double                  weight;   // 边权重
 *       ds_graph_adj_node_t    *next;     // 下一条边
 *   };
 *
 * 示例图（有向邻接表表示）：
 *
 *   顶点 0 ──→ [to=1, w=3.0] → [to=2, w=1.0] → NULL
 *   顶点 1 ──→ [to=3, w=2.0] → NULL
 *   顶点 2 ──→ [to=3, w=5.0] → NULL
 *   顶点 3 ──→ NULL
 *
 * ============================================================
 * DFS 算法
 * ============================================================
 *
 * 深度优先搜索（递归 + visited 标记）：
 *
 *   dfs(v):
 *     visited[v] = true
 *     visit(v)
 *     for each neighbor u of v:
 *       if !visited[u]:
 *         dfs(u)
 *
 * 时间复杂度：O(V + E)
 * 空间复杂度：O(V)（递归栈深度 + visited 数组）
 *
 * ============================================================
 * BFS 算法
 * ============================================================
 *
 * 广度优先搜索（队列 + visited 标记）：
 *
 *   queue.push(start)
 *   visited[start] = true
 *   while !queue.empty():
 *     v = queue.pop()
 *     visit(v)
 *     for each neighbor u of v:
 *       if !visited[u]:
 *         visited[u] = true
 *         queue.push(u)
 *
 * 时间复杂度：O(V + E)
 * 空间复杂度：O(V)（队列 + visited 数组）
 *
 * ============================================================
 * 拓扑排序（Kahn 算法 —— BFS 变种）
 * ============================================================
 *
 * 只适用于 DAG（有向无环图）。
 *
 * 算法：
 *   1. 计算每个顶点的入度（indegree）
 *   2. 将所有入度为 0 的顶点入队
 *   3. while !queue.empty():
 *        v = queue.pop()
 *        将 v 加入结果
 *        for each neighbor u of v:
 *          indegree[u]--
 *          if indegree[u] == 0: queue.push(u)
 *   4. 如果结果长度 < V，说明图中有环（无法完成拓扑排序）
 *
 * 应用场景：
 *   - 课程安排（先修课关系）
 *   - 构建系统中的依赖顺序
 *   - 任务调度
 *
 * ============================================================
 * Dijkstra 最短路径
 * ============================================================
 *
 * 本实现中为简洁仅演示 DFS/BFS/拓扑排序。
 * Dijkstra 实现见 src/algo/ 或 index/ 模块中的图算法。
 */

#include <ds/graph.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 邻接表节点 */
typedef struct ds_graph_adj_node {
    size_t                   to;
    double                   weight;
    struct ds_graph_adj_node *next;
} ds_graph_adj_node_t;

/* 图结构 */
struct ds_graph {
    ds_graph_adj_node_t **adj_list;
    size_t                vertices;
    bool                  directed;
};

/* 创建邻接表节点 */
static ds_graph_adj_node_t *adj_node_create(size_t to, double weight)
{
    ds_graph_adj_node_t *node;

    node = (ds_graph_adj_node_t *)malloc(sizeof(ds_graph_adj_node_t));
    if (!node) {
        return NULL;
    }
    node->to = to;
    node->weight = weight;
    node->next = NULL;
    return node;
}

ds_graph_t *ds_graph_create(size_t vertices, bool directed)
{
    ds_graph_t *graph;

    if (vertices == 0u) {
        return NULL;
    }

    graph = (ds_graph_t *)calloc(1u, sizeof(ds_graph_t));
    if (!graph) {
        return NULL;
    }

    graph->adj_list = (ds_graph_adj_node_t **)calloc(vertices, sizeof(ds_graph_adj_node_t *));
    if (!graph->adj_list) {
        free(graph);
        return NULL;
    }

    graph->vertices = vertices;
    graph->directed = directed;
    return graph;
}

void ds_graph_destroy(ds_graph_t *graph)
{
    if (!graph) return;

    for (size_t i = 0u; i < graph->vertices; ++i) {
        ds_graph_adj_node_t *curr = graph->adj_list[i];
        while (curr) {
            ds_graph_adj_node_t *next = curr->next;
            free(curr);
            curr = next;
        }
    }
    free(graph->adj_list);
    free(graph);
}

/* 添加边（头插法） */
int ds_graph_add_edge(ds_graph_t *graph, size_t from, size_t to, double weight)
{
    ds_graph_adj_node_t *node;

    if (!graph || from >= graph->vertices || to >= graph->vertices) {
        return -1;
    }

    node = adj_node_create(to, weight);
    if (!node) {
        return -1;
    }

    /* 头插到邻接表 */
    node->next = graph->adj_list[from];
    graph->adj_list[from] = node;

    /* 无向图：还需要添加反向边 */
    if (!graph->directed) {
        node = adj_node_create(from, weight);
        if (!node) {
            return -1;
        }
        node->next = graph->adj_list[to];
        graph->adj_list[to] = node;
    }

    return 0;
}

/* ============================================================
 * DFS（递归实现）
 * ============================================================ */

static void graph_dfs_impl(const ds_graph_t *graph, size_t v, bool *visited,
                            ds_graph_visit_fn visit, void *user_data)
{
    visited[v] = true;
    if (visit) {
        visit(v, user_data);
    }

    for (ds_graph_adj_node_t *edge = graph->adj_list[v]; edge; edge = edge->next) {
        if (!visited[edge->to]) {
            graph_dfs_impl(graph, edge->to, visited, visit, user_data);
        }
    }
}

void ds_graph_dfs(const ds_graph_t *graph, size_t start, ds_graph_visit_fn visit, void *user_data)
{
    bool *visited;

    if (!graph || start >= graph->vertices) {
        return;
    }

    visited = (bool *)calloc(graph->vertices, sizeof(bool));
    if (!visited) {
        return;
    }

    graph_dfs_impl(graph, start, visited, visit, user_data);
    free(visited);
}

/* ============================================================
 * BFS（队列实现——用简单数组模拟）
 * ============================================================ */

void ds_graph_bfs(const ds_graph_t *graph, size_t start, ds_graph_visit_fn visit, void *user_data)
{
    bool  *visited;
    size_t *queue;
    size_t  front;
    size_t  rear;

    if (!graph || start >= graph->vertices) {
        return;
    }

    visited = (bool *)calloc(graph->vertices, sizeof(bool));
    queue = (size_t *)malloc(sizeof(size_t) * graph->vertices);
    if (!visited || !queue) {
        free(visited);
        free(queue);
        return;
    }

    front = 0u;
    rear = 0u;

    /* 入队起始顶点 */
    queue[rear++] = start;
    visited[start] = true;

    while (front < rear) {
        size_t v = queue[front++];

        if (visit) {
            visit(v, user_data);
        }

        /* 遍历邻居 */
        for (ds_graph_adj_node_t *edge = graph->adj_list[v]; edge; edge = edge->next) {
            if (!visited[edge->to]) {
                visited[edge->to] = true;
                queue[rear++] = edge->to;
            }
        }
    }

    free(queue);
    free(visited);
}

/* ============================================================
 * 拓扑排序（Kahn 算法）
 * ============================================================ */

size_t *ds_graph_topological_sort(const ds_graph_t *graph, size_t *out_len)
{
    size_t *indegree;
    size_t *result;
    size_t *queue;
    size_t  front;
    size_t  rear;
    size_t  result_idx;

    if (!graph || !out_len) {
        return NULL;
    }

    *out_len = 0u;

    /* 只有有向无环图（DAG）才有拓扑排序 */
    if (!graph->directed) {
        return NULL;
    }

    indegree = (size_t *)calloc(graph->vertices, sizeof(size_t));
    result = (size_t *)malloc(sizeof(size_t) * graph->vertices);
    queue = (size_t *)malloc(sizeof(size_t) * graph->vertices);
    if (!indegree || !result || !queue) {
        free(indegree);
        free(result);
        free(queue);
        return NULL;
    }

    /* 步骤1：计算入度 */
    for (size_t i = 0u; i < graph->vertices; ++i) {
        for (ds_graph_adj_node_t *edge = graph->adj_list[i]; edge; edge = edge->next) {
            indegree[edge->to] += 1u;
        }
    }

    /* 步骤2：入度为 0 的顶点入队 */
    front = 0u;
    rear = 0u;
    for (size_t i = 0u; i < graph->vertices; ++i) {
        if (indegree[i] == 0u) {
            queue[rear++] = i;
        }
    }

    /* 步骤3：BFS 处理 */
    result_idx = 0u;
    while (front < rear) {
        size_t v = queue[front++];
        result[result_idx++] = v;

        for (ds_graph_adj_node_t *edge = graph->adj_list[v]; edge; edge = edge->next) {
            indegree[edge->to] -= 1u;
            if (indegree[edge->to] == 0u) {
                queue[rear++] = edge->to;
            }
        }
    }

    free(indegree);
    free(queue);

    if (result_idx != graph->vertices) {
        /* 有环，无法完成拓扑排序 */
        free(result);
        return NULL;
    }

    *out_len = result_idx;
    return result;
}

/* ============================================================
 * 演示函数
 * ============================================================ */

static void graph_print_vertex(size_t vertex, void *user_data)
{
    (void)user_data;
    printf("%zu ", vertex);
}

void ds_graph_demo(void)
{
    printf("========== 图演示 ==========\n");

    /* 创建有向图：
     *   0 → 1 → 3
     *   0 → 2 → 3
     *   1 → 4
     *   2 → 4
     */
    ds_graph_t *graph = ds_graph_create(5, true);
    if (!graph) { printf("创建失败\n"); return; }

    ds_graph_add_edge(graph, 0, 1, 1.0);
    ds_graph_add_edge(graph, 0, 2, 1.0);
    ds_graph_add_edge(graph, 1, 3, 1.0);
    ds_graph_add_edge(graph, 2, 3, 1.0);
    ds_graph_add_edge(graph, 1, 4, 1.0);
    ds_graph_add_edge(graph, 2, 4, 1.0);

    printf("\n【DFS 从顶点 0 开始】\n");
    ds_graph_dfs(graph, 0, graph_print_vertex, NULL);
    printf("\n");

    printf("\n【BFS 从顶点 0 开始】\n");
    ds_graph_bfs(graph, 0, graph_print_vertex, NULL);
    printf("\n");

    printf("\n【拓扑排序】\n");
    size_t topo_len = 0u;
    size_t *topo = ds_graph_topological_sort(graph, &topo_len);
    if (topo) {
        printf("拓扑序: ");
        for (size_t i = 0u; i < topo_len; ++i) {
            printf("%zu%s", topo[i], i < topo_len - 1u ? " → " : "");
        }
        printf("\n");
        free(topo);
    } else {
        printf("图中存在环路，无法完成拓扑排序\n");
    }

    ds_graph_destroy(graph);
    printf("========== 图演示结束 ==========\n");
}
