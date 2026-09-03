/**
 * 图的遍历演示程序
 *
 * 功能：
 * 1. BFS（广度优先搜索）- 使用队列实现
 * 2. DFS（深度优先搜索）- 递归/栈实现
 * 3. 连通分量检测 - 基于 DFS
 * 4. 拓扑排序 - Kahn 算法（BFS 变体）和 DFS 算法
 *
 * 编译：gcc -std=c11 -Wall -O2 main.c -o graph_traversal
 * 运行：./graph_traversal
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_VERTICES 100

/*------------------- 图的数据结构 -------------------*/

typedef struct {
    int n;                      // 顶点数
    int adj[MAX_VERTICES][MAX_VERTICES];  // 邻接矩阵
} Graph;

/**
 * 初始化图
 */
void graph_init(Graph *g, int n) {
    g->n = n;
    memset(g->adj, 0, sizeof(g->adj));
}

/**
 * 添加无向边
 */
void graph_add_edge(Graph *g, int u, int v) {
    if (u >= 0 && u < g->n && v >= 0 && v < g->n) {
        g->adj[u][v] = 1;
        g->adj[v][u] = 1;  // 无向图
    }
}

/**
 * 添加有向边
 */
void graph_add_directed_edge(Graph *g, int u, int v) {
    if (u >= 0 && u < g->n && v >= 0 && v < g->n) {
        g->adj[u][v] = 1;  // 仅单向
    }
}

/*------------------- BFS（广度优先搜索）-------------------*/

typedef struct {
    int data[MAX_VERTICES];
    int front;
    int rear;
} Queue;

void queue_init(Queue *q) {
    q->front = q->rear = 0;
}

int queue_empty(Queue *q) {
    return q->front == q->rear;
}

void queue_push(Queue *q, int v) {
    q->data[q->rear++] = v;
}

int queue_pop(Queue *q) {
    return q->data[q->front++];
}

/**
 * BFS 遍历：从 start 开始
 */
void bfs(Graph *g, int start, int *visited) {
    Queue q;
    queue_init(&q);

    visited[start] = 1;
    queue_push(&q, start);

    printf("BFS: ");
    while (!queue_empty(&q)) {
        int u = queue_pop(&q);
        printf("%d ", u);

        for (int v = 0; v < g->n; v++) {
            if (g->adj[u][v] && !visited[v]) {
                visited[v] = 1;
                queue_push(&q, v);
            }
        }
    }
    printf("\n");
}

/*------------------- DFS（深度优先搜索）-------------------*/

/**
 * 递归 DFS
 */
void dfs_recursive(Graph *g, int u, int *visited) {
    visited[u] = 1;
    printf("%d ", u);

    for (int v = 0; v < g->n; v++) {
        if (g->adj[u][v] && !visited[v]) {
            dfs_recursive(g, v, visited);
        }
    }
}

/**
 * DFS 遍历（包装）
 */
void dfs(Graph *g, int start) {
    int visited[MAX_VERTICES] = {0};
    printf("DFS: ");
    dfs_recursive(g, start, visited);
    printf("\n");
}

/*------------------- 连通分量 -------------------*/

/**
 * 统计无向图的连通分量数
 */
int count_connected_components(Graph *g) {
    int visited[MAX_VERTICES] = {0};
    int count = 0;

    for (int i = 0; i < g->n; i++) {
        if (!visited[i]) {
            count++;
            dfs_recursive(g, i, visited);  // 标记该连通分量所有顶点
        }
    }
    return count;
}

/*------------------- 拓扑排序 -------------------*/

/**
 * Kahn 算法（基于入度）
 * 返回 1 表示成功，0 表示图有环
 */
int topological_sort_kahn(Graph *g) {
    int in_degree[MAX_VERTICES] = {0};
    Queue q;
    queue_init(&q);

    // 计算入度
    for (int u = 0; u < g->n; u++) {
        for (int v = 0; v < g->n; v++) {
            if (g->adj[u][v]) {
                in_degree[v]++;
            }
        }
    }

    // 入度为 0 的顶点入队
    for (int i = 0; i < g->n; i++) {
        if (in_degree[i] == 0) {
            queue_push(&q, i);
        }
    }

    int result[MAX_VERTICES];
    int idx = 0;

    while (!queue_empty(&q)) {
        int u = queue_pop(&q);
        result[idx++] = u;

        for (int v = 0; v < g->n; v++) {
            if (g->adj[u][v]) {
                in_degree[v]--;
                if (in_degree[v] == 0) {
                    queue_push(&q, v);
                }
            }
        }
    }

    if (idx == g->n) {
        printf("拓扑排序 (Kahn): ");
        for (int i = 0; i < idx; i++) {
            printf("%d ", result[i]);
        }
        printf("\n");
        return 1;
    } else {
        printf("图存在环，无法完成拓扑排序\n");
        return 0;
    }
}

/**
 * DFS 版拓扑排序（利用后序逆序）
 */
int topo_order[MAX_VERTICES];
int topo_idx;

void dfs_topological(Graph *g, int u, int *visited, int *on_stack) {
    visited[u] = 1;
    on_stack[u] = 1;

    for (int v = 0; v < g->n; v++) {
        if (g->adj[u][v]) {
            if (on_stack[v]) {
                printf("图存在环，无法完成拓扑排序\n");
                return;
            }
            if (!visited[v]) {
                dfs_topological(g, v, visited, on_stack);
            }
        }
    }

    on_stack[u] = 0;
    topo_order[topo_idx++] = u;  // 后序记录，逆序即为拓扑序
}

void topological_sort_dfs(Graph *g) {
    int visited[MAX_VERTICES] = {0};
    int on_stack[MAX_VERTICES] = {0};
    topo_idx = 0;

    for (int i = 0; i < g->n; i++) {
        if (!visited[i]) {
            dfs_topological(g, i, visited, on_stack);
        }
    }

    printf("拓扑排序 (DFS): ");
    for (int i = topo_idx - 1; i >= 0; i--) {
        printf("%d ", topo_order[i]);
    }
    printf("\n");
}

/*------------------- 主函数 -------------------*/

int main(void) {
    printf("========== 图的遍历演示 ==========\n\n");

    /* 示例 1：无向图 - BFS/DFS/连通分量 */
    printf("--- 示例 1：无向图 ---\n");
    Graph g1;
    graph_init(&g1, 7);
    // 构建一个包含两个连通分量的图
    // 分量 1: 0-1-2, 0-3
    graph_add_edge(&g1, 0, 1);
    graph_add_edge(&g1, 1, 2);
    graph_add_edge(&g1, 0, 3);
    // 分量 2: 4-5-6
    graph_add_edge(&g1, 4, 5);
    graph_add_edge(&g1, 5, 6);

    int visited1[MAX_VERTICES] = {0};
    bfs(&g1, 0, visited1);
    dfs(&g1, 0);
    printf("连通分量数: %d\n\n", count_connected_components(&g1));

    /* 示例 2：有向图 - 拓扑排序 */
    printf("--- 示例 2：有向图（课程先修关系） ---\n");
    Graph g2;
    graph_init(&g2, 5);
    // 0: 数学, 1: 数据结构, 2: 算法, 3: 机器学习, 4: AI
    // 数学是基础，数据结构 -> 算法，算法/数学 -> 机器学习
    graph_add_directed_edge(&g2, 0, 1);  // 数学 -> 数据结构
    graph_add_directed_edge(&g2, 1, 2);  // 数据结构 -> 算法
    graph_add_directed_edge(&g2, 0, 3);  // 数学 -> 机器学习
    graph_add_directed_edge(&g2, 2, 3);  // 算法 -> 机器学习
    graph_add_directed_edge(&g2, 3, 4);  // 机器学习 -> AI

    topological_sort_kahn(&g2);
    topological_sort_dfs(&g2);

    printf("\n========== 演示完成 ==========\n");
    return 0;
}
