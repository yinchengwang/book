/* graph scaffold — 邻接矩阵/邻接表 + DFS/BFS + Dijkstra + Prim MST */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>
#include <string.h>

/* ══════════════════════════════════════════════════════════════
 * Part 1: 邻接矩阵
 * ══════════════════════════════════════════════════════════════ */
#define V 6

static void print_matrix(const int g[V][V], int n) {
    printf("[matrix] %d x %d\n", n, n);
    for (int i = 0; i < n; i++) {
        printf("  v%d: ", i);
        for (int j = 0; j < n; j++)
            printf("%2d ", g[i][j]);
        printf("\n");
    }
}

/* ── DFS (递归) ── */
static void dfs_matrix(const int g[V][V], int n, int v, bool visited[]) {
    visited[v] = true;
    printf("%d ", v);
    for (int w = 0; w < n; w++)
        if (g[v][w] && !visited[w])
            dfs_matrix(g, n, w, visited);
}

/* ── BFS (队列) ── */
static void bfs_matrix(const int g[V][V], int n, int start) {
    bool visited[V] = {false};
    int q[V], head = 0, tail = 0;
    visited[start] = true;
    q[tail++] = start;
    while (head < tail) {
        int v = q[head++];
        printf("%d ", v);
        for (int w = 0; w < n; w++)
            if (g[v][w] && !visited[w]) {
                visited[w] = true;
                q[tail++] = w;
            }
    }
}

/* ══════════════════════════════════════════════════════════════
 * Part 2: 邻接表
 * ══════════════════════════════════════════════════════════════ */
typedef struct Edge {
    int to, weight;
    struct Edge *next;
} Edge;

typedef struct {
    Edge *head;
} AdjList;

static void adj_add_edge(AdjList *adj, int from, int to, int w) {
    Edge *e = (Edge*)malloc(sizeof(Edge));
    e->to = to; e->weight = w;
    e->next = adj[from].head;
    adj[from].head = e;
}

static void adj_print(const AdjList *adj, int n) {
    printf("[adjlist] %d vertices\n", n);
    for (int i = 0; i < n; i++) {
        printf("  v%d → ", i);
        for (Edge *e = adj[i].head; e; e = e->next)
            printf("%d(w=%d) ", e->to, e->weight);
        printf("\n");
    }
}

static void adj_free(AdjList *adj, int n) {
    for (int i = 0; i < n; i++) {
        Edge *e = adj[i].head;
        while (e) { Edge *t = e; e = e->next; free(t); }
    }
}

/* ══════════════════════════════════════════════════════════════
 * Part 3: Dijkstra — 单源最短路径 (邻接表版)
 * ══════════════════════════════════════════════════════════════ */
static void dijkstra(AdjList *adj, int n, int start) {
    int dist[V];
    bool done[V] = {false};
    for (int i = 0; i < n; i++) dist[i] = INT_MAX;
    dist[start] = 0;

    for (int iter = 0; iter < n; iter++) {
        /* 找未处理节点中 dist 最小的 */
        int u = -1, min_d = INT_MAX;
        for (int i = 0; i < n; i++)
            if (!done[i] && dist[i] < min_d)
                { min_d = dist[i]; u = i; }
        if (u == -1) break;
        done[u] = true;
        printf("  [dijk iter=%d] pick v%d (dist=%d)\n", iter, u, dist[u]);
        /* 松弛所有邻边 */
        for (Edge *e = adj[u].head; e; e = e->next) {
            int v = e->to;
            if (!done[v] && dist[u] != INT_MAX && dist[u] + e->weight < dist[v]) {
                int old = dist[v];
                dist[v] = dist[u] + e->weight;
                printf("    relax v%d: %d → %d\n", v, old, dist[v]);
            }
        }
    }
    printf("[dijk] 最短距离 from v%d: ", start);
    for (int i = 0; i < n; i++)
        printf("v%d=%d ", i, dist[i] == INT_MAX ? -1 : dist[i]);
    printf("\n");
}

/* ══════════════════════════════════════════════════════════════
 * Part 4: Prim — 最小生成树
 * ══════════════════════════════════════════════════════════════ */
static void prim_matrix(const int g[V][V], int n) {
    int key[V];          /* key[i] = 连接 i 到 MST 的最小边权 */
    bool in_mst[V] = {false};
    int parent[V];
    for (int i = 0; i < n; i++) { key[i] = INT_MAX; parent[i] = -1; }
    key[0] = 0;  /* 从顶点 0 开始 */

    for (int iter = 0; iter < n; iter++) {
        /* 找 key 最小的未加入顶点 */
        int u = -1, min_k = INT_MAX;
        for (int i = 0; i < n; i++)
            if (!in_mst[i] && key[i] < min_k)
                { min_k = key[i]; u = i; }
        if (u == -1) break;
        in_mst[u] = true;
        if (parent[u] != -1)
            printf("  [prim] 边 v%d-v%d (w=%d) 加入 MST\n", parent[u], u, g[u][parent[u]]);
        /* 更新邻接顶点的 key */
        for (int v = 0; v < n; v++)
            if (g[u][v] && !in_mst[v] && g[u][v] < key[v])
                { key[v] = g[u][v]; parent[v] = u; }
    }
}

int main(void) {
    /* 无向图邻接矩阵:
     *   0--1--2
     *   |  |\ |
     *   3--4  5
     *   边权用于 Dijkstra/Prim
     */
    int g[V][V] = {
        {0,4,0,2,0,0},
        {4,0,3,0,5,6},
        {0,3,0,0,0,7},
        {2,0,0,0,1,0},
        {0,5,0,1,0,0},
        {0,6,7,0,0,0}
    };

    printf("=== 邻接矩阵 ===\n");
    print_matrix(g, V);

    printf("\n=== DFS (from v0) ===\n");
    bool visited_dfs[V] = {false};
    printf("  [dfs] ");
    dfs_matrix(g, V, 0, visited_dfs);
    printf("\n");

    printf("\n=== BFS (from v0) ===\n");
    printf("  [bfs] ");
    bfs_matrix(g, V, 0);
    printf("\n");

    /* 邻接表 */
    printf("\n=== 邻接表 ===\n");
    AdjList *adj = (AdjList*)calloc((size_t)V, sizeof(AdjList));
    for (int i = 0; i < V; i++) adj[i].head = NULL;
    for (int i = 0; i < V; i++)
        for (int j = i+1; j < V; j++)
            if (g[i][j]) {
                adj_add_edge(adj, i, j, g[i][j]);
                adj_add_edge(adj, j, i, g[i][j]);  /* 无向图双加 */
            }
    adj_print(adj, V);

    /* Dijkstra */
    printf("\n=== Dijkstra 最短路径 (from v0) ===\n");
    dijkstra(adj, V, 0);

    /* Prim */
    printf("\n=== Prim 最小生成树 ===\n");
    prim_matrix(g, V);

    adj_free(adj, V);
    free(adj);

    printf("\n=== PASS ===\n");
    return 0;
}
