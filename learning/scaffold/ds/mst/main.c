/**
 * @file main.c
 * @brief 最小生成树：Prim 算法与 Kruskal 算法 + 并查集
 *
 * 演示两种经典 MST 算法：
 * 1. Prim   - 切分法，从点出发，O(V^2) 或 O(E log V)
 * 2. Kruskal - 贪心边排序 + 并查集，O(E log E)
 */

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#define MAX_V 6
#define MAX_E 10
#define INF INT_MAX

/* ══════════════════════════════════════════════════════════════════════
 * 数据结构
 * ══════════════════════════════════════════════════════════════════════ */

/** 边结构 */
typedef struct {
    int from, to, weight;
} Edge;

/** 并查集 */
typedef struct {
    int parent[MAX_V];
    int rank[MAX_V];
} UnionFind;

/* ══════════════════════════════════════════════════════════════════════
 * 并查集操作
 * ══════════════════════════════════════════════════════════════════════ */

static void uf_init(UnionFind *uf, int n)
{
    for (int i = 0; i < n; i++) {
        uf->parent[i] = i;
        uf->rank[i] = 0;
    }
}

static int uf_find(UnionFind *uf, int x)
{
    if (uf->parent[x] != x)
        uf->parent[x] = uf_find(uf, uf->parent[x]);  /* 路径压缩 */
    return uf->parent[x];
}

static int uf_union(UnionFind *uf, int x, int y)
{
    int px = uf_find(uf, x);
    int py = uf_find(uf, y);
    if (px == py) return 0;  /* 已连通 */
    /* 按秩合并 */
    if (uf->rank[px] < uf->rank[py]) uf->parent[px] = py;
    else if (uf->rank[px] > uf->rank[py]) uf->parent[py] = px;
    else { uf->parent[py] = px; uf->rank[px]++; }
    return 1;
}

/* ══════════════════════════════════════════════════════════════════════
 * 1. Prim 算法
 * ══════════════════════════════════════════════════════════════════════ */

/**
 * Prim 最小生成树
 * @param edges 边数组
 * @param ec    边数量
 * @param n     顶点数
 * @param mst   输出 MST 边数组
 * @return MST 边数
 */
static int prim(const Edge *edges, int ec, int n, Edge *mst)
{
    int visited[MAX_V] = {0};
    int key[MAX_V];
    int parent[MAX_V];
    for (int i = 0; i < n; i++) key[i] = INF;

    key[0] = 0;  /* 从顶点 0 开始 */
    for (int iter = 0; iter < n - 1; iter++) {
        /* 选最小 key 的未访问顶点 */
        int u = -1;
        for (int i = 0; i < n; i++)
            if (!visited[i] && (u == -1 || key[i] < key[u])) u = i;

        visited[u] = 1;
        if (u != 0) {  /* 记录 MST 边 */
            int cnt = iter;
            mst[cnt].from = parent[u];
            mst[cnt].to = u;
            mst[cnt].weight = key[u];
        }
        /* 松弛 */
        for (int i = 0; i < ec; i++) {
            if (edges[i].from == u && !visited[edges[i].to] && edges[i].weight < key[edges[i].to]) {
                key[edges[i].to] = edges[i].weight;
                parent[edges[i].to] = u;
            }
            if (edges[i].to == u && !visited[edges[i].from] && edges[i].weight < key[edges[i].from]) {
                key[edges[i].from] = edges[i].weight;
                parent[edges[i].from] = u;
            }
        }
    }
    return n - 1;
}

/* ══════════════════════════════════════════════════════════════════════
 * 2. Kruskal 算法
 * ══════════════════════════════════════════════════════════════════════ */

/** 边比较函数 */
static int edge_cmp(const void *a, const void *b)
{
    return ((Edge *)a)->weight - ((Edge *)b)->weight;
}

/**
 * Kruskal 最小生成树
 * @param edges 边数组
 * @param ec    边数量
 * @param n     顶点数
 * @param mst   输出 MST 边数组
 * @return MST 边数
 */
static int kruskal(Edge *edges, int ec, int n, Edge *mst)
{
    qsort(edges, (size_t)ec, sizeof(Edge), edge_cmp);
    UnionFind uf;
    uf_init(&uf, n);
    int cnt = 0;
    for (int i = 0; i < ec && cnt < n - 1; i++) {
        if (uf_union(&uf, edges[i].from, edges[i].to)) {
            mst[cnt++] = edges[i];
        }
    }
    return cnt;
}

/* ══════════════════════════════════════════════════════════════════════
 * 演示
 * ══════════════════════════════════════════════════════════════════════ */

/**
 * 测试图（6 顶点）：
 *
 *      1     4
 *  v0 ─── v1 ─── v2
 *  │╲     │     ╱│
 * 3│  ╲5 │   2/ │3
 *  │    ╲│╱    │
 *  v4 ─── v3 ─── v5
 *      2     6
 */
static void demo(void)
{
    Edge edges[] = {
        {0,1,1},{0,2,5},{0,4,3},{1,2,4},{1,3,2},
        {2,3,2},{2,5,3},{3,4,2},{3,5,6},{4,5,8}
    };
    int ec = (int)(sizeof(edges)/sizeof(edges[0]));
    int n = 6;
    Edge mst[MAX_E];
    int total;

    printf("\n  [Prim 算法]\n");
    total = prim(edges, ec, n, mst);
    int w1 = 0;
    for (int i = 0; i < total; i++) {
        printf("    v%d ─%2d─ v%d\n", mst[i].from, mst[i].weight, mst[i].to);
        w1 += mst[i].weight;
    }
    printf("    总权重: %d\n", w1);

    printf("\n  [Kruskal 算法]\n");
    total = kruskal(edges, ec, n, mst);
    int w2 = 0;
    for (int i = 0; i < total; i++) {
        printf("    v%d ─%2d─ v%d\n", mst[i].from, mst[i].weight, mst[i].to);
        w2 += mst[i].weight;
    }
    printf("    总权重: %d\n", w2);

    printf("\n  [并查集追踪]\n");
    printf("    find(0)=%d, find(5)=%d, 初始不相连\n", 0, 5);
}

int main(void)
{
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║          最小生成树: Prim / Kruskal + 并查集                  ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");

    demo();

    printf("\n═══════════════════════════════════════════════════════════════\n");
    printf("  算法对比:\n");
    printf("    Prim    O(V^2) / O(E log V) - 适合稠密图\n");
    printf("    Kruskal O(E log E)          - 适合稀疏图\n");
    printf("  核心思想: 切分定理 / 贪心边选择 + 并查集判环\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    return 0;
}
