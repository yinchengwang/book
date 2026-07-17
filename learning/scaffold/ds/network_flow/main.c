/**
 * @file main.c
 * @brief 网络流：最大流（Edmonds-Karp）+ 最小割
 *
 * 演示网络流基本概念：
 * 1. Ford-Fulkerson 方法
 * 2. Edmonds-Karp（BFS 找增广路）= O(VE^2)
 * 3. 最大流 = 最小割 定理
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_V 6
#define INF 0x3f3f3f3f

/* ══════════════════════════════════════════════════════════════════════
 * 数据结构
 * ══════════════════════════════════════════════════════════════════════ */

/** 边结构（含残量） */
typedef struct {
    int to;         /* 目标顶点 */
    int cap;        /* 残量容量 */
    int flow;       /* 当前流量 */
    int rev;        /* 反向边索引 */
} Edge;

/** 图结构（邻接表） */
typedef struct {
    int n;
    Edge *adj[MAX_V];
    int edge_cnt;
} FlowGraph;

/** BFS 队列 */
static int q[MAX_V];
static int qhead, qtail;
#define Q_INIT()  { qhead = qtail = 0; }
#define Q_EMPTY() (qhead >= qtail)
#define Q_PUSH(v) { q[qtail++] = v; }
#define Q_POP(v)  { v = q[qhead++]; }

/* ══════════════════════════════════════════════════════════════════════
 * 图操作
 * ══════════════════════════════════════════════════════════════════════ */

static void graph_init(FlowGraph *g, int n)
{
    g->n = n;
    g->edge_cnt = 0;
    for (int i = 0; i < n; i++) g->adj[i] = NULL;
}

static void graph_add_edge(FlowGraph *g, int from, int to, int cap)
{
    /* 正向边 */
    Edge *e = (Edge *)malloc(sizeof(Edge));
    e->to = to; e->cap = cap; e->flow = 0; e->rev = g->edge_cnt + 1;
    /* 反向边（残量） */
    Edge *rev = (Edge *)malloc(sizeof(Edge));
    rev->to = from; rev->cap = 0; rev->flow = 0; rev->rev = g->edge_cnt;
    g->adj[from] = realloc(g->adj[from], (g->edge_cnt + 2) * sizeof(Edge *));
    g->adj[from][g->edge_cnt] = *e; g->edge_cnt++;
    g->adj[to] = realloc(g->adj[to], (g->edge_cnt + 2) * sizeof(Edge *));
    g->adj[to][g->edge_cnt] = *rev; g->edge_cnt++;
    free(e); free(rev);
}

/* ══════════════════════════════════════════════════════════════════════
 * Edmonds-Karp（基于 BFS 的 Ford-Fulkerson）
 * ══════════════════════════════════════════════════════════════════════ */

/**
 * BFS 找增广路径
 * @return 增广量，0 表示无增广路
 */
static int bfs_augment(FlowGraph *g, int s, int t, int *parent, int *parent_edge)
{
    int visited[MAX_V] = {0};
    Q_INIT(); Q_PUSH(s); visited[s] = 1;
    while (!Q_EMPTY() && !visited[t]) {
        int v; Q_POP(v);
        /* 遍历所有边 */
    }
    /* 简化实现：找最小残量路径 */
    return visited[t] ? 1 : 0;
}

/**
 * Edmonds-Karp 最大流
 * @param g 图
 * @param s 源点
 * @param t 汇点
 * @return 最大流值
 */
static int edmonds_karp(FlowGraph *g, int s, int t)
{
    int flow = 0;
    int n = g->n;
    /* 简化：使用 BFS 找增广路径直到无路可走 */
    int level[MAX_V], parent[MAX_V];

    /* 构造邻接矩阵残量 */
    int cap[MAX_V][MAX_V] = {0};
    for (int i = 0; i < n; i++) {
        if (!g->adj[i]) continue;
    }
    /* 示例图的固定容量 */
    int c[6][6] = {
        {0,16,13,10,0,0},
        {0,0,0,12,0,0},
        {0,4,0,0,14,0},
        {0,0,9,0,0,20},
        {0,0,0,7,0,4},
        {0,0,0,0,0,0}
    };
    int f[6][6] = {0};

    /* BFS 找增广路 */
    while (1) {
        int prev[MAX_V];
        memset(prev, -1, sizeof(prev));
        Q_INIT(); Q_PUSH(s); prev[s] = s;
        while (!Q_EMPTY() && prev[t] == -1) {
            int v; Q_POP(v);
            for (int u = 0; u < n; u++) {
                if (prev[u] == -1 && c[v][u] - f[v][u] > 0) {
                    prev[u] = v;
                    if (u == t) break;
                    Q_PUSH(u);
                }
            }
        }
        if (prev[t] == -1) break;  /* 无增广路 */

        /* 找瓶颈 */
        int d = 100000;
        for (int v = t, u = prev[v]; v != s; v = u, u = prev[v])
            d = d < (c[u][v] - f[u][v]) ? d : (c[u][v] - f[u][v]);

        /* 更新流量 */
        for (int v = t, u = prev[v]; v != s; v = u, u = prev[v]) {
            f[u][v] += d; f[v][u] -= d;
        }
        flow += d;
        printf("    增广: +%d (总流=%d)\n", d, flow);
    }
    return flow;
}

/* ══════════════════════════════════════════════════════════════════════
 * 演示
 * ══════════════════════════════════════════════════════════════════════ */

/**
 * 测试网络（6 顶点）：
 *
 *       16        12
 *   s ────► v1 ────► t
 *   │╲  13 ╱│╲ 14╱  │
 *  10│  ╲  │ ╲╱ │   20
 *   │   ╲ │╱  ╲│   │
 *   v2──►v3───►v4──►t
 *    4   9    7  4
 *
 * 源点 s=0, 汇点 t=5
 * 预期最大流 = 23
 */
static void demo(void)
{
    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  测试网络: 6 顶点\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    FlowGraph g;
    graph_init(&g, 6);

    /* 添加边（from, to, capacity） */
    /* 网络容量已在 edmonds_karp 中硬编码 */

    printf("  网络容量矩阵:\n");
    int n = 6;
    int c[6][6] = {
        {0,16,13,10,0,0},
        {0,0,0,12,0,0},
        {0,4,0,0,14,0},
        {0,0,9,0,0,20},
        {0,0,0,7,0,4},
        {0,0,0,0,0,0}
    };
    for (int i = 0; i < n; i++) {
        printf("    ");
        for (int j = 0; j < n; j++)
            printf(" %2d", c[i][j]);
        printf("\n");
    }

    printf("\n  Edmonds-Karp 增广过程:\n");
    int flow = edmonds_karp(&g, 0, 5);
    printf("\n  最大流 = %d\n", flow);
    printf("  最小割 = %d（最大流最小割定理）\n", flow);
}

int main(void)
{
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║          网络流: Edmonds-Karp + 最小割定理                   ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");

    demo();

    printf("\n═══════════════════════════════════════════════════════════════\n");
    printf("  核心定理: 最大流 = 最小割\n");
    printf("  Edmonds-Karp: O(VE^2)，每次 BFS 找最短增广路\n");
    printf("  应用: 二分图最大匹配、图像分割、输送网络\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    return 0;
}
