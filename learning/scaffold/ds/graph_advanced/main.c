/**
 * @file main.c
 * @brief 图高级算法：强连通分量 (Tarjan) 与欧拉回路检测
 *
 * 演示内容：
 * 1. Tarjan 算法求强连通分量 (SCC)
 * 2. 欧拉回路/通路检测（Fleury 算法）
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* ══════════════════════════════════════════════════════════════════════
 * 数据结构定义
 * ══════════════════════════════════════════════════════════════════════ */

/** 邻接表边节点 */
typedef struct Edge {
    int to;                 /* 目标顶点 */
    struct Edge *next;      /* 下一条边 */
} Edge;

/** 图结构 */
typedef struct {
    int n;                  /* 顶点数 */
    Edge **adj;             /* 邻接表 */
    int *in_degree;         /* 入度 */
    int *out_degree;        /* 出度 */
    bool is_directed;       /* 是否为有向图 */
} Graph;

/* ══════════════════════════════════════════════════════════════════════
 * 图的创建与销毁
 * ══════════════════════════════════════════════════════════════════════ */

/** 创建图 */
static Graph *graph_create(int n, bool directed)
{
    Graph *g = (Graph *)malloc(sizeof(Graph));
    g->n = n;
    g->is_directed = directed;
    g->adj = (Edge **)calloc((size_t)n, sizeof(Edge *));
    g->in_degree = (int *)calloc((size_t)n, sizeof(int));
    g->out_degree = (int *)calloc((size_t)n, sizeof(int));
    return g;
}

/** 添加边 */
static void graph_add_edge(Graph *g, int from, int to)
{
    Edge *e = (Edge *)malloc(sizeof(Edge));
    e->to = to;
    e->next = g->adj[from];
    g->adj[from] = e;
    g->out_degree[from]++;
    g->in_degree[to]++;

    if (!g->is_directed) {
        Edge *rev = (Edge *)malloc(sizeof(Edge));
        rev->to = from;
        rev->next = g->adj[to];
        g->adj[to] = rev;
        g->out_degree[to]++;
        g->in_degree[from]++;
    }
}

/** 释放图 */
static void graph_free(Graph *g)
{
    for (int i = 0; i < g->n; i++) {
        Edge *e = g->adj[i];
        while (e) {
            Edge *tmp = e;
            e = e->next;
            free(tmp);
        }
    }
    free(g->adj);
    free(g->in_degree);
    free(g->out_degree);
    free(g);
}

/* ══════════════════════════════════════════════════════════════════════
 * Tarjan 算法：求强连通分量
 * ══════════════════════════════════════════════════════════════════════ */

#define MAX_SCC 100
#define MAX_STACK 100

typedef struct {
    int dfn[MAX_STACK];     /* 搜索序号 */
    int low[MAX_STACK];     /* 能追溯到的最小序号 */
    int in_stack[MAX_STACK];/* 是否在栈中 */
    int stack[MAX_STACK];   /* 模拟栈 */
    int top;                /* 栈顶 */
    int timestamp;          /* 时间戳 */
    int scc_id[MAX_STACK];  /* 每个顶点所属的 SCC ID */
    int scc_count;          /* SCC 数量 */
} TarjanCtx;

/**
 * Tarjan DFS
 * @param g 图
 * @param u 当前顶点
 * @param ctx Tarjan 上下文
 */
static void tarjan_dfs(Graph *g, int u, TarjanCtx *ctx)
{
    ctx->timestamp++;
    ctx->dfn[u] = ctx->low[u] = ctx->timestamp;
    ctx->stack[++ctx->top] = u;
    ctx->in_stack[u] = 1;

    for (Edge *e = g->adj[u]; e; e = e->next) {
        int v = e->to;
        if (ctx->dfn[v] == 0) {
            tarjan_dfs(g, v, ctx);
            ctx->low[u] = (ctx->low[u] < ctx->low[v]) ? ctx->low[u] : ctx->low[v];
        } else if (ctx->in_stack[v]) {
            ctx->low[u] = (ctx->low[u] < ctx->dfn[v]) ? ctx->low[u] : ctx->dfn[v];
        }
    }

    /* 找到 SCC 根 */
    if (ctx->low[u] == ctx->dfn[u]) {
        ctx->scc_count++;
        printf("  SCC #%d: ", ctx->scc_count);
        do {
            int v = ctx->stack[ctx->top--];
            ctx->in_stack[v] = 0;
            ctx->scc_id[v] = ctx->scc_count;
            printf("v%d ", v);
        } while (ctx->stack[ctx->top + 1] != u);
        printf("\n");
    }
}

/** Tarjan 算法入口 */
static void tarjan_scc(Graph *g)
{
    TarjanCtx ctx = {0};
    ctx.scc_count = 0;
    ctx.top = -1;
    ctx.timestamp = 0;
    memset(ctx.dfn, 0, sizeof(ctx.dfn));
    memset(ctx.in_stack, 0, sizeof(ctx.in_stack));

    printf("\n  Tarjan 算法求强连通分量:\n");
    for (int i = 0; i < g->n; i++) {
        if (ctx.dfn[i] == 0) {
            tarjan_dfs(g, i, &ctx);
        }
    }
    printf("  共 %d 个强连通分量\n", ctx.scc_count);
}

/* ══════════════════════════════════════════════════════════════════════
 * 欧拉回路检测
 * ══════════════════════════════════════════════════════════════════════ */

/**
 * 检查图是否有欧拉回路或欧拉通路
 * @param g 图
 * @return 0: 无欧拉路 1: 欧拉回路 2: 欧拉通路
 */
static int euler_check(Graph *g)
{
    if (g->is_directed) {
        /* 有向图：检查基图连通性 + 出入度 */
        int start_nonzero = 0, end_nonzero = 0;
        for (int i = 0; i < g->n; i++) {
            int diff = g->out_degree[i] - g->in_degree[i];
            if (diff == 1) start_nonzero++;
            else if (diff == -1) end_nonzero++;
            else if (diff != 0) return 0;
        }
        if (start_nonzero == 0 && end_nonzero == 0) return 1; /* 欧拉回路 */
        if (start_nonzero == 1 && end_nonzero == 1) return 2; /* 欧拉通路 */
        return 0;
    } else {
        /* 无向图：统计奇度顶点 */
        int odd_count = 0;
        for (int i = 0; i < g->n; i++) {
            if ((g->out_degree[i] & 1) == 1) odd_count++;
        }
        if (odd_count == 0) return 1; /* 欧拉回路 */
        if (odd_count == 2) return 2; /* 欧拉通路 */
        return 0;
    }
}

/* ══════════════════════════════════════════════════════════════════════
 * 演示函数
 * ══════════════════════════════════════════════════════════════════════ */

/** 演示强连通分量 */
static void demo_scc(void)
{
    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  示例 1: 有向图的强连通分量 (SCC)\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    /*
     * 有向图:
     *   0 → 1 → 2
     *   ↑       ↓
     *   4 ← 3 ←─┘
     * 顶点 0,3,4 形成 SCC，顶点 1,2 各为独立 SCC
     */
    Graph *g = graph_create(5, true);
    graph_add_edge(g, 0, 1);
    graph_add_edge(g, 1, 2);
    graph_add_edge(g, 2, 3);
    graph_add_edge(g, 3, 4);
    graph_add_edge(g, 4, 0);

    printf("  图结构: 0→1→2→3→4→0 (形成环)\n");
    tarjan_scc(g);
    graph_free(g);
}

/** 演示欧拉回路 */
static void demo_euler(void)
{
    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  示例 2: 欧拉回路与欧拉通路检测\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    /* 示例 2a: 无向完全图 K4 - 有欧拉回路 */
    printf("\n  2a) 无向完全图 K4 (4 顶点完全相连):\n");
    Graph *k4 = graph_create(4, false);
    for (int i = 0; i < 4; i++) {
        for (int j = i + 1; j < 4; j++) {
            graph_add_edge(k4, i, j);
        }
    }
    int result = euler_check(k4);
    printf("    各顶点度: ");
    for (int i = 0; i < 4; i++) printf("v%d=%d ", i, k4->out_degree[i]);
    printf("\n    结果: %s\n", result == 1 ? "有欧拉回路" : (result == 2 ? "有欧拉通路" : "无欧拉路"));
    graph_free(k4);

    /* 示例 2b: 有向图 - 七桥问题 */
    printf("\n  2b) 七桥问题 (4 陆地, 奇度顶点 > 2):\n");
    Graph *bridges = graph_create(4, false);
    graph_add_edge(bridges, 0, 1);
    graph_add_edge(bridges, 0, 2);
    graph_add_edge(bridges, 0, 3);
    graph_add_edge(bridges, 1, 2);
    graph_add_edge(bridges, 1, 2);  /* 第二座桥 */
    graph_add_edge(bridges, 2, 3);
    graph_add_edge(bridges, 2, 3);  /* 第二座桥 */
    result = euler_check(bridges);
    printf("    各顶点度: ");
    for (int i = 0; i < 4; i++) printf("v%d=%d ", i, bridges->out_degree[i]);
    printf("\n    结果: %s\n", result == 1 ? "有欧拉回路" : (result == 2 ? "有欧拉通路" : "无欧拉路"));
    graph_free(bridges);
}

/* ══════════════════════════════════════════════════════════════════════
 * 主函数
 * ══════════════════════════════════════════════════════════════════════ */

int main(void)
{
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║          图高级算法: 强连通分量与欧拉回路                        ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");

    demo_scc();
    demo_euler();

    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  算法复杂度:\n");
    printf("    • Tarjan SCC: O(V+E)\n");
    printf("    • 欧拉检测:   O(V+E)\n");
    printf("  应用场景:\n");
    printf("    • SCC: 2-SAT、缩点、社交网络分析\n");
    printf("    • 欧拉回路: 邮递路线、电路板走线\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    return 0;
}
