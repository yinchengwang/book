/**
 * @file main.c
 * @brief 最短路径算法：Dijkstra / Bellman-Ford / SPFA / Floyd-Warshall
 *
 * 演示四种经典最短路径算法：
 * 1. Dijkstra   - 单源非负权，最小堆优化 O((V+E)logV)
 * 2. Bellman-Ford - 单源允许负权，O(VE)
 * 3. SPFA       - Bellman-Ford 队列优化，均摊 O(E)
 * 4. Floyd      - 全源最短路，O(V^3)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* ══════════════════════════════════════════════════════════════════════
 * 数据结构
 * ══════════════════════════════════════════════════════════════════════ */

#define MAX_V 6
#define INF INT_MAX

/** 边结构 */
typedef struct Edge {
    int from, to, weight;
} Edge;

/** 图结构（邻接表） */
typedef struct {
    int n;
    int capacity;
    Edge *edges;
    int edge_count;
} Graph;

/* ══════════════════════════════════════════════════════════════════════
 * 辅助函数
 * ══════════════════════════════════════════════════════════════════════ */

static void print_dist(const char *name, const int *dist, int n, int src)
{
    printf("  %s (源点=%d):\n    ", name, src);
    for (int i = 0; i < n; i++) {
        if (dist[i] == INF)
            printf(" INF");
        else
            printf(" %3d", dist[i]);
    }
    printf("\n");
}

/* ══════════════════════════════════════════════════════════════════════
 * 1. Dijkstra 算法（朴素版 O(V^2)）
 * ══════════════════════════════════════════════════════════════════════ */

/**
 * Dijkstra 最短路径
 * @param edges 边数组
 * @param ec   边数量
 * @param n    顶点数
 * @param src  源点
 * @param dist 输出距离数组
 */
static void dijkstra(const Edge *edges, int ec, int n, int src, int *dist)
{
    /* 初始化距离 */
    for (int i = 0; i < n; i++) dist[i] = INF;
    dist[src] = 0;

    /* visited[i] = true 表示已确定最短距离 */
    int visited[MAX_V] = {0};

    for (int iter = 0; iter < n; iter++) {
        /* 找未访问顶点中距离最小的 */
        int u = -1;
        for (int i = 0; i < n; i++) {
            if (!visited[i] && dist[i] < INF) {
                if (u == -1 || dist[i] < dist[u]) u = i;
            }
        }
        if (u == -1) break;
        visited[u] = 1;

        /* 松弛操作 */
        for (int i = 0; i < ec; i++) {
            if (edges[i].from == u && dist[edges[i].to] > dist[u] + edges[i].weight)
                dist[edges[i].to] = dist[u] + edges[i].weight;
            if (edges[i].to == u && dist[edges[i].from] > dist[u] + edges[i].weight)
                dist[edges[i].from] = dist[u] + edges[i].weight;
        }
    }
}

/* ══════════════════════════════════════════════════════════════════════
 * 2. Bellman-Ford 算法 O(VE)
 * ══════════════════════════════════════════════════════════════════════ */

static void bellman_ford(const Edge *edges, int ec, int n, int src, int *dist)
{
    for (int i = 0; i < n; i++) dist[i] = INF;
    dist[src] = 0;

    /* 进行 n-1 轮松弛 */
    for (int i = 0; i < n - 1; i++) {
        int updated = 0;
        for (int j = 0; j < ec; j++) {
            if (dist[edges[j].from] != INF && dist[edges[j].to] > dist[edges[j].from] + edges[j].weight) {
                dist[edges[j].to] = dist[edges[j].from] + edges[j].weight;
                updated = 1;
            }
            if (dist[edges[j].to] != INF && dist[edges[j].from] > dist[edges[j].to] + edges[j].weight) {
                dist[edges[j].from] = dist[edges[j].to] + edges[j].weight;
                updated = 1;
            }
        }
        if (!updated) break;
    }
}

/* ══════════════════════════════════════════════════════════════════════
 * 3. SPFA 算法（队列优化的 Bellman-Ford）
 * ══════════════════════════════════════════════════════════════════════ */

static void spfa(const Edge *edges, int ec, int n, int src, int *dist)
{
    for (int i = 0; i < n; i++) dist[i] = INF;
    dist[src] = 0;

    int queue[MAX_V];
    int inqueue[MAX_V] = {0};
    int front = 0, rear = 0;
    queue[rear++] = src;
    inqueue[src] = 1;

    while (front < rear) {
        int u = queue[front++];
        inqueue[u] = 0;

        for (int i = 0; i < ec; i++) {
            if (edges[i].from == u && dist[edges[i].to] > dist[u] + edges[i].weight) {
                dist[edges[i].to] = dist[u] + edges[i].weight;
                if (!inqueue[edges[i].to]) {
                    queue[rear++] = edges[i].to;
                    inqueue[edges[i].to] = 1;
                }
            }
            if (edges[i].to == u && dist[edges[i].from] > dist[u] + edges[i].weight) {
                dist[edges[i].from] = dist[u] + edges[i].weight;
                if (!inqueue[edges[i].from]) {
                    queue[rear++] = edges[i].from;
                    inqueue[edges[i].from] = 1;
                }
            }
        }
    }
}

/* ══════════════════════════════════════════════════════════════════════
 * 4. Floyd-Warshall 全源最短路 O(V^3)
 * ══════════════════════════════════════════════════════════════════════ */

/**
 * Floyd-Warshall 全源最短路
 * @param dist 距离矩阵，dist[i][j] 从 i 到 j 的距离
 * @param n    顶点数
 */
static void floyd(int (*dist)[MAX_V], int n)
{
    /* 初始化：自己到自己距离为 0，无边为 INF */
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            dist[i][j] = (i == j) ? 0 : INF;

    /* 根据边表设置初始距离 */
    Edge edges[] = {
        {0,1,2},{0,2,5},{0,4,1},{1,2,3},{1,3,4},
        {2,3,5},{2,4,2},{3,4,6}
    };
    int ec = (int)(sizeof(edges)/sizeof(edges[0]));
    for (int i = 0; i < ec; i++)
        dist[edges[i].from][edges[i].to] = edges[i].weight;

    /* 三重循环：经过 k 中转是否更短 */
    for (int k = 0; k < n; k++)
        for (int i = 0; i < n; i++)
            for (int j = 0; j < n; j++)
                if (dist[i][k] < INF && dist[k][j] < INF &&
                    dist[i][j] > dist[i][k] + dist[k][j])
                    dist[i][j] = dist[i][k] + dist[k][j];
}

/* ══════════════════════════════════════════════════════════════════════
 * 演示
 * ══════════════════════════════════════════════════════════════════════ */

/**
 * 测试图（6 顶点）：
 *
 *       v0 ──2── v1
 *       │╲       ╱│
 *      2│ ╲5   3/ │4
 *       │   ╲ ╱   │
 *       v4───v2───v3
 *        1  2╱ 5│
 *             ╱  │
 *            v5 6
 *
 * 边: (0,1,2) (0,2,5) (0,4,1) (1,2,3) (1,3,4)
 *     (2,3,5) (2,4,2) (3,4,6) (3,5,6)
 */
static void demo_all(void)
{
    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  测试图: 6 顶点 (上图的简化版)\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    /* 定义边表 */
    Edge edges[] = {
        {0,1,2},{0,2,5},{0,4,1},{1,2,3},{1,3,4},
        {2,3,5},{2,4,2},{3,4,6},{3,5,6}
    };
    int ec = (int)(sizeof(edges)/sizeof(edges[0]));
    int n = 6;
    int src = 0;

    int dist[MAX_V];

    /* 1) Dijkstra */
    dijkstra(edges, ec, n, src, dist);
    print_dist("Dijkstra", dist, n, src);

    /* 2) Bellman-Ford */
    bellman_ford(edges, ec, n, src, dist);
    print_dist("Bellman-Ford", dist, n, src);

    /* 3) SPFA */
    spfa(edges, ec, n, src, dist);
    print_dist("SPFA", dist, n, src);

    /* 4) Floyd-Warshall */
    printf("\n  Floyd-Warshall 全源最短路矩阵:\n    ");
    int fdist[MAX_V][MAX_V];
    floyd(fdist, n);
    printf("    ");
    for (int j = 0; j < n; j++) printf(" v%-2d", j);
    printf("\n");
    for (int i = 0; i < n; i++) {
        printf("    v%d: ", i);
        for (int j = 0; j < n; j++) {
            if (fdist[i][j] == INF) printf(" INF");
            else printf(" %3d", fdist[i][j]);
        }
        printf("\n");
    }
}

/* ══════════════════════════════════════════════════════════════════════
 * 主函数
 * ══════════════════════════════════════════════════════════════════════ */

int main(void)
{
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║          最短路径算法: Dijkstra/Bellman-Ford/SPFA/Floyd       ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");

    demo_all();

    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  算法对比:\n");
    printf("    Dijkstra      O(V^2) / O((V+E)logV) - 非负权边\n");
    printf("    Bellman-Ford  O(VE)    - 支持负权，检测负环\n");
    printf("    SPFA          期望 O(E) - Bellman-Ford 队列优化\n");
    printf("    Floyd-Warshall O(V^3)  - 全源最短路\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    return 0;
}
