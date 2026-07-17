/**
 * @file main.c
 * @brief 树形 DP：树上最大独立集、树的直径、树形背包
 *
 * 演示内容：
 * 1. 树上最大独立集（树形 DP）
 * 2. 树的直径（两次 BFS/DFS）
 * 3. 树形背包问题
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* ══════════════════════════════════════════════════════════════════════
 * 数据结构定义
 * ══════════════════════════════════════════════════════════════════════ */

/** 树的邻接表边节点 */
typedef struct TreeEdge {
    int to;                 /* 目标顶点 */
    int weight;            /* 边权重 */
    struct TreeEdge *next; /* 下一条边 */
} TreeEdge;

/** 树结构 */
typedef struct {
    int n;                  /* 顶点数 */
    TreeEdge **adj;         /* 邻接表 */
} Tree;

/** 树形 DP 结果 */
typedef struct {
    int include;            /* 选择当前节点的最大值 */
    int exclude;            /* 不选当前节点的最大值 */
} DPResult;

/* ══════════════════════════════════════════════════════════════════════
 * 树的创建与销毁
 * ══════════════════════════════════════════════════════════════════════ */

/** 创建树 */
static Tree *tree_create(int n)
{
    Tree *t = (Tree *)malloc(sizeof(Tree));
    t->n = n;
    t->adj = (TreeEdge **)calloc((size_t)n, sizeof(TreeEdge *));
    return t;
}

/** 添加边 */
static void tree_add_edge(Tree *t, int from, int to, int weight)
{
    TreeEdge *e = (TreeEdge *)malloc(sizeof(TreeEdge));
    e->to = to;
    e->weight = weight;
    e->next = t->adj[from];
    t->adj[from] = e;

    TreeEdge *rev = (TreeEdge *)malloc(sizeof(TreeEdge));
    rev->to = from;
    rev->weight = weight;
    rev->next = t->adj[to];
    t->adj[to] = rev;
}

/** 释放树 */
static void tree_free(Tree *t)
{
    for (int i = 0; i < t->n; i++) {
        TreeEdge *e = t->adj[i];
        while (e) {
            TreeEdge *tmp = e;
            e = e->next;
            free(tmp);
        }
    }
    free(t->adj);
    free(t);
}

/* ══════════════════════════════════════════════════════════════════════
 * 树的直径（迭代 BFS）
 * ══════════════════════════════════════════════════════════════════════ */

/** BFS 找最远节点（迭代，避免栈溢出） */
static int bfs_farthest(Tree *t, int start, int *parent, int *dist)
{
    int max_dist = 0;
    int farthest = start;

    int *queue = (int *)malloc((size_t)t->n * sizeof(int));
    int front = 0, rear = 0;

    /* 初始化 */
    for (int i = 0; i < t->n; i++) {
        parent[i] = -1;
        dist[i] = -1;
    }

    queue[rear++] = start;
    dist[start] = 0;

    while (front < rear) {
        int u = queue[front++];

        for (TreeEdge *e = t->adj[u]; e; e = e->next) {
            if (dist[e->to] < 0) {
                dist[e->to] = dist[u] + e->weight;
                parent[e->to] = u;
                queue[rear++] = e->to;

                if (dist[e->to] > max_dist) {
                    max_dist = dist[e->to];
                    farthest = e->to;
                }
            }
        }
    }

    free(queue);
    return farthest;
}

/**
 * 计算树的直径
 * @param t 树
 * @param end1 直径一端
 * @param end2 直径另一端
 * @return 直径长度
 */
static int tree_diameter(Tree *t, int *end1, int *end2)
{
    int *parent = (int *)malloc((size_t)t->n * sizeof(int));
    int *dist = (int *)malloc((size_t)t->n * sizeof(int));

    /* 第一次 BFS：从节点 0 出发，找最远节点 end1 */
    *end1 = bfs_farthest(t, 0, parent, dist);

    /* 第二次 BFS：从 end1 出发，找最远节点 end2 */
    *end2 = bfs_farthest(t, *end1, parent, dist);
    int diameter = dist[*end2];

    free(parent);
    free(dist);
    return diameter;
}

/* ══════════════════════════════════════════════════════════════════════
 * 树上最大独立集（后序遍历）
 * ══════════════════════════════════════════════════════════════════════ */

/** 树形 DP 求最大独立集（迭代后序遍历） */
static void mis_dp_iterative(Tree *t, DPResult *dp)
{
    int *stack = (int *)malloc((size_t)t->n * sizeof(int));
    int *parent = (int *)malloc((size_t)t->n * sizeof(int));
    int *order = (int *)malloc((size_t)t->n * sizeof(int));  /* 后序遍历顺序 */
    int top = 0;
    int order_idx = 0;

    /* 初始化 */
    for (int i = 0; i < t->n; i++) {
        parent[i] = -1;
        dp[i].include = 1;
        dp[i].exclude = 0;
    }

    /* 根节点入栈 */
    stack[top++] = 0;

    while (top > 0) {
        int u = stack[--top];
        order[order_idx++] = u;

        /* 找所有子节点并记录父子关系 */
        for (TreeEdge *e = t->adj[u]; e; e = e->next) {
            if (parent[e->to] == -1 && e->to != 0) {
                parent[e->to] = u;
                stack[top++] = e->to;
            }
        }
    }

    /* 按后序顺序计算 DP */
    for (int i = order_idx - 1; i >= 0; i--) {
        int u = order[i];
        for (TreeEdge *e = t->adj[u]; e; e = e->next) {
            int v = e->to;
            if (parent[v] == u) {  /* v 是 u 的子节点 */
                dp[u].include += dp[v].exclude;
                dp[u].exclude += (dp[v].include > dp[v].exclude) ?
                                 dp[v].include : dp[v].exclude;
            }
        }
    }

    free(stack);
    free(parent);
    free(order);
}

/* ══════════════════════════════════════════════════════════════════════
 * 树形背包（树上分组背包）
 * ══════════════════════════════════════════════════════════════════════ */

/** 树形背包 DP（迭代后序遍历） */
static void knapsack_dp_iterative(Tree *t, int k, int **dp, int *weights)
{
    int *parent = (int *)malloc((size_t)t->n * sizeof(int));
    int *order = (int *)malloc((size_t)t->n * sizeof(int));
    int *stack = (int *)malloc((size_t)t->n * sizeof(int));
    int top = 0;
    int order_idx = 0;

    /* 初始化 */
    for (int i = 0; i < t->n; i++) {
        parent[i] = -1;
        for (int j = 0; j <= k; j++) {
            dp[i][j] = (j == 0) ? 0 : INT_MIN;
        }
    }

    /* 根节点入栈，收集后序遍历顺序 */
    stack[top++] = 0;

    while (top > 0) {
        int u = stack[--top];
        order[order_idx++] = u;

        for (TreeEdge *e = t->adj[u]; e; e = e->next) {
            if (parent[e->to] == -1 && e->to != 0) {
                parent[e->to] = u;
                stack[top++] = e->to;
            }
        }
    }

    /* 按后序顺序计算 DP */
    for (int i = order_idx - 1; i >= 0; i--) {
        int u = order[i];
        for (TreeEdge *e = t->adj[u]; e; e = e->next) {
            int v = e->to;
            if (parent[v] == u) {  /* v 是 u 的子节点 */
                /* 合并子树的 DP 结果 */
                int *new_dp = (int *)malloc((size_t)(k + 1) * sizeof(int));
                for (int j = 0; j <= k; j++) {
                    new_dp[j] = dp[u][j];
                }

                for (int j = 1; j <= k; j++) {
                    for (int m = 0; m < j; m++) {
                        if (dp[v][m] != INT_MIN) {
                            int val = dp[v][m] + weights[u];
                            if (val > new_dp[j]) {
                                new_dp[j] = val;
                            }
                        }
                    }
                }

                for (int j = 0; j <= k; j++) {
                    dp[u][j] = new_dp[j];
                }
                free(new_dp);
            }
        }
    }

    free(parent);
    free(order);
}

/* ══════════════════════════════════════════════════════════════════════
 * 演示函数
 * ══════════════════════════════════════════════════════════════════════ */

/** 演示树的直径 */
static void demo_diameter(void)
{
    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  示例 1: 树的直径\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    /*
     * 树的形状:
     *       0
     *      /|\
     *     1 2 3
     *    /|   |
     *   4 5   6
     *   |
     *   7
     *
     * 直径: 7 → 4 → 1 → 0 → 3 → 6
     */
    Tree *t = tree_create(8);
    tree_add_edge(t, 0, 1, 1);
    tree_add_edge(t, 0, 2, 2);
    tree_add_edge(t, 0, 3, 3);
    tree_add_edge(t, 1, 4, 4);
    tree_add_edge(t, 1, 5, 5);
    tree_add_edge(t, 3, 6, 6);
    tree_add_edge(t, 4, 7, 7);

    int end1, end2;
    int diameter = tree_diameter(t, &end1, &end2);

    printf("  树结构: 8 个顶点，加权边\n");
    printf("  直径端点: v%d 和 v%d\n", end1, end2);
    printf("  直径长度: %d\n", diameter);

    tree_free(t);
}

/** 演示最大独立集 */
static void demo_mis(void)
{
    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  示例 2: 树上最大独立集\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    /*
     * 树的形状（公司组织结构）:
     *         0 (CEO)
     *        /|\
     *       1 2 3 (部门主管)
     *      /|   |
     *     4 5   6 (员工)
     *
     * 最大独立集：选节点 {0, 2, 4, 5, 6} 或 {1, 2, 4, 5, 6} 等
     */
    Tree *t = tree_create(7);
    tree_add_edge(t, 0, 1, 1);
    tree_add_edge(t, 0, 2, 1);
    tree_add_edge(t, 0, 3, 1);
    tree_add_edge(t, 1, 4, 1);
    tree_add_edge(t, 1, 5, 1);
    tree_add_edge(t, 3, 6, 1);

    DPResult *dp = (DPResult *)malloc((size_t)t->n * sizeof(DPResult));
    mis_dp_iterative(t, dp);

    printf("  组织结构: CEO(0) → 3个部门(1,2,3) → 员工(4,5,6)\n");
    printf("  DP 结果:\n");
    for (int i = 0; i < t->n; i++) {
        printf("    节点 %d: include=%d, exclude=%d\n", i, dp[i].include, dp[i].exclude);
    }
    printf("  最大独立集大小: %d\n",
           (dp[0].include > dp[0].exclude) ? dp[0].include : dp[0].exclude);

    free(dp);
    tree_free(t);
}

/** 演示树形背包 */
static void demo_tree_knapsack(void)
{
    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  示例 3: 树形背包（选人问题）\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    /* 树形背包：每个节点有权重，选择 k 个节点的最大权重和 */
    Tree *t = tree_create(5);
    tree_add_edge(t, 0, 1, 1);
    tree_add_edge(t, 0, 2, 1);
    tree_add_edge(t, 1, 3, 1);
    tree_add_edge(t, 1, 4, 1);

    int weights[] = {5, 4, 3, 2, 1};  /* 节点权重 */
    int k = 3;  /* 选择 3 个节点 */

    /* DP 数组 */
    int **dp = (int **)malloc((size_t)t->n * sizeof(int *));
    for (int i = 0; i < t->n; i++) {
        dp[i] = (int *)malloc((size_t)(k + 1) * sizeof(int));
        for (int j = 0; j <= k; j++) {
            dp[i][j] = INT_MIN;
        }
    }

    knapsack_dp_iterative(t, k, dp, weights);

    printf("  节点权重: ");
    for (int i = 0; i < t->n; i++) printf("v%d=%d ", i, weights[i]);
    printf("\n  选择 %d 个节点的最大权重和: %d\n", k, dp[0][k] >= 0 ? dp[0][k] : 0);

    /* 释放 */
    for (int i = 0; i < t->n; i++) {
        free(dp[i]);
    }
    free(dp);
    tree_free(t);
}

/* ══════════════════════════════════════════════════════════════════════
 * 主函数
 * ══════════════════════════════════════════════════════════════════════ */

int main(void)
{
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║          树形 DP: 最大独立集、树的直径、树形背包                 ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");

    demo_diameter();
    demo_mis();
    demo_tree_knapsack();

    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  算法复杂度:\n");
    printf("    • 树的直径:   O(V)\n");
    printf("    • 最大独立集: O(V)\n");
    printf("    • 树形背包:   O(V * K^2)\n");
    printf("  应用场景:\n");
    printf("    • 决策树构建\n");
    printf("    • XML/JSON 解析树\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    return 0;
}
