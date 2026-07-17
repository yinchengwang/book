/**
 * @file main.c
 * @brief 图的表示方法：邻接矩阵与邻接表
 *
 * 演示两种基本图表示法：
 * 1. 邻接矩阵 - O(1) 边查询，适合稠密图
 * 2. 邻接表   - O(deg) 边遍历，适合稀疏图
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* ══════════════════════════════════════════════════════════════════════
 * 数据结构定义
 * ══════════════════════════════════════════════════════════════════════ */

/** 邻接表边节点 */
typedef struct AdjEdge {
    int vertex;              /* 邻接顶点编号 */
    int weight;             /* 边权重（可选） */
    struct AdjEdge *next;   /* 下一条边 */
} AdjEdge;

/** 邻接表头节点 */
typedef struct AdjListNode {
    int vertex;             /* 顶点编号 */
    AdjEdge *head;          /* 出边链表头 */
} AdjListNode;

/** 图结构 */
typedef struct {
    int n;                  /* 顶点数 */
    int **matrix;           /* 邻接矩阵 */
    AdjListNode *adj_list;  /* 邻接表 */
    bool is_directed;       /* 是否为有向图 */
} Graph;

/* ══════════════════════════════════════════════════════════════════════
 * 邻接矩阵操作
 * ══════════════════════════════════════════════════════════════════════ */

/**
 * 创建空邻接矩阵
 * @param n 矩阵维度（顶点数）
 * @return 分配好的矩阵（二维数组）
 */
static int **matrix_create(int n)
{
    int **m = (int **)malloc((size_t)n * sizeof(int *));
    for (int i = 0; i < n; i++) {
        m[i] = (int *)calloc((size_t)n, sizeof(int));
    }
    return m;
}

/** 释放邻接矩阵 */
static void matrix_free(int **m, int n)
{
    for (int i = 0; i < n; i++) {
        free(m[i]);
    }
    free(m);
}

/** 添加边（矩阵版） */
static void matrix_add_edge(int **m, int n, int from, int to, int weight, bool directed)
{
    m[from][to] = weight;
    if (!directed) {
        m[to][from] = weight;  /* 无向图需对称设置 */
    }
}

/** 打印邻接矩阵 */
static void matrix_print(const int **m, int n)
{
    printf("\n  [邻接矩阵] %d x %d:\n     ", n, n);
    for (int j = 0; j < n; j++) {
        printf(" v%-3d", j);
    }
    printf("\n");
    for (int i = 0; i < n; i++) {
        printf("  v%d: ", i);
        for (int j = 0; j < n; j++) {
            printf(" %-4d ", m[i][j]);
        }
        printf("\n");
    }
}

/* ══════════════════════════════════════════════════════════════════════
 * 邻接表操作
 * ══════════════════════════════════════════════════════════════════════ */

/**
 * 创建邻接表
 * @param n 顶点数
 * @return 邻接表头指针
 */
static AdjListNode *adjlist_create(int n)
{
    AdjListNode *adj = (AdjListNode *)calloc((size_t)n, sizeof(AdjListNode));
    for (int i = 0; i < n; i++) {
        adj[i].vertex = i;
        adj[i].head = NULL;
    }
    return adj;
}

/**
 * 添加边（邻接表版）
 * @param adj 邻接表
 * @param from 起始顶点
 * @param to 目标顶点
 * @param weight 权重
 * @param directed 是否为有向图
 */
static void adjlist_add_edge(AdjListNode *adj, int from, int to, int weight, bool directed)
{
    /* 头插法：O(1) 插入 */
    AdjEdge *edge = (AdjEdge *)malloc(sizeof(AdjEdge));
    edge->vertex = to;
    edge->weight = weight;
    edge->next = adj[from].head;
    adj[from].head = edge;

    /* 无向图需双向添加 */
    if (!directed) {
        AdjEdge *rev = (AdjEdge *)malloc(sizeof(AdjEdge));
        rev->vertex = from;
        rev->weight = weight;
        rev->next = adj[to].head;
        adj[to].head = rev;
    }
}

/** 打印邻接表 */
static void adjlist_print(const AdjListNode *adj, int n)
{
    printf("\n  [邻接表] %d 个顶点:\n", n);
    for (int i = 0; i < n; i++) {
        printf("  v%d → ", adj[i].vertex);
        AdjEdge *e = adj[i].head;
        if (!e) {
            printf("(空)");
        } else {
            while (e) {
                printf("v%d(w=%d)", e->vertex, e->weight);
                if (e->next) printf(" → ");
                e = e->next;
            }
        }
        printf("\n");
    }
}

/** 释放邻接表 */
static void adjlist_free(AdjListNode *adj, int n)
{
    for (int i = 0; i < n; i++) {
        AdjEdge *e = adj[i].head;
        while (e) {
            AdjEdge *tmp = e;
            e = e->next;
            free(tmp);
        }
    }
    free(adj);
}

/* ══════════════════════════════════════════════════════════════════════
 * 演示函数
 * ══════════════════════════════════════════════════════════════════════ */

/**
 * 创建示例图（5 个顶点的社交网络）
 *
 *        v0 ─── v1
 *        │╲    ╱│
 *       2│ ╲  ╱ │3
 *        │  ╲╱  │
 *        v4 ─── v2 ─── v3
 *          1      5
 *
 * 边定义: (from, to, weight)
 * v0-v1(2), v0-v2(5), v0-v4(1)
 * v1-v2(3), v1-v3(4)
 * v2-v3(5), v2-v4(2)
 * v4-v3(6)
 */
static void demo_undirected_graph(void)
{
    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  示例 1: 无向加权图 (5 顶点)\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    const int N = 5;

    /* 邻接矩阵方式 */
    int **matrix = matrix_create(N);
    matrix_add_edge(matrix, N, 0, 1, 2, false);
    matrix_add_edge(matrix, N, 0, 2, 5, false);
    matrix_add_edge(matrix, N, 0, 4, 1, false);
    matrix_add_edge(matrix, N, 1, 2, 3, false);
    matrix_add_edge(matrix, N, 1, 3, 4, false);
    matrix_add_edge(matrix, N, 2, 3, 5, false);
    matrix_add_edge(matrix, N, 2, 4, 2, false);
    matrix_add_edge(matrix, N, 3, 4, 6, false);
    matrix_print((const int **)matrix, N);
    matrix_free(matrix, N);

    /* 邻接表方式 */
    AdjListNode *adj = adjlist_create(N);
    adjlist_add_edge(adj, 0, 1, 2, false);
    adjlist_add_edge(adj, 0, 2, 5, false);
    adjlist_add_edge(adj, 0, 4, 1, false);
    adjlist_add_edge(adj, 1, 2, 3, false);
    adjlist_add_edge(adj, 1, 3, 4, false);
    adjlist_add_edge(adj, 2, 3, 5, false);
    adjlist_add_edge(adj, 2, 4, 2, false);
    adjlist_add_edge(adj, 3, 4, 6, false);
    adjlist_print(adj, N);
    adjlist_free(adj, N);
}

/** 演示有向图 */
static void demo_directed_graph(void)
{
    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  示例 2: 有向无环图 DAG (4 顶点, 拓扑排序候选)\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    const int N = 4;

    /* 邻接矩阵 */
    int **matrix = matrix_create(N);
    matrix_add_edge(matrix, N, 0, 1, 1, true);  /* v0 → v1 */
    matrix_add_edge(matrix, N, 0, 2, 1, true);  /* v0 → v2 */
    matrix_add_edge(matrix, N, 1, 3, 1, true);  /* v1 → v3 */
    matrix_add_edge(matrix, N, 2, 3, 1, true);  /* v2 → v3 */
    matrix_print((const int **)matrix, N);
    matrix_free(matrix, N);

    /* 邻接表 */
    AdjListNode *adj = adjlist_create(N);
    adjlist_add_edge(adj, 0, 1, 1, true);
    adjlist_add_edge(adj, 0, 2, 1, true);
    adjlist_add_edge(adj, 1, 3, 1, true);
    adjlist_add_edge(adj, 2, 3, 1, true);
    adjlist_print(adj, N);
    adjlist_free(adj, N);
}

/* ══════════════════════════════════════════════════════════════════════
 * 主函数
 * ══════════════════════════════════════════════════════════════════════ */

int main(void)
{
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║          图的表示方法: 邻接矩阵 vs 邻接表                        ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");

    demo_undirected_graph();
    demo_directed_graph();

    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  空间复杂度对比:\n");
    printf("    • 邻接矩阵: O(V^2) — 固定二维数组\n");
    printf("    • 邻接表:   O(V+E) — 顶点 + 边节点\n");
    printf("  选择建议:\n");
    printf("    • 稠密图 (E ≈ V^2): 邻接矩阵\n");
    printf("    • 稀疏图 (E << V^2): 邻接表\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    return 0;
}
