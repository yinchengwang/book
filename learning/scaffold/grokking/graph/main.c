/**
 * graph/main.c — 图算法典型应用（C11）
 *
 * 1. 克隆图（BFS）
 * 2. 课程表（拓扑排序）
 * 3. 岛屿数量（DFS 网格）
 *
 * 编译：make          # 生成 graph
 * 运行：./graph       # 运行所有示例
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* ========== 图节点（邻接表）========== */
typedef struct GNode {
    int val;
    struct GNode **neighbors;
    int neighbor_cnt;
} GNode;

/* 创建图节点 */
static GNode *node_new(int val)
{
    GNode *n = (GNode *)calloc(1, sizeof(GNode));
    n->val = val;
    n->neighbors = NULL;
    n->neighbor_cnt = 0;
    return n;
}

/* 添加邻居 */
static void node_add_neighbor(GNode *n, GNode *nb)
{
    n->neighbor_cnt++;
    n->neighbors = (GNode **)realloc(n->neighbors,
                                     (size_t)n->neighbor_cnt * sizeof(GNode *));
    n->neighbors[n->neighbor_cnt - 1] = nb;
}

/* ========== 队列（BFS 辅助）========== */
typedef struct {
    void **data;
    int head, tail, cap;
} Queue;

static Queue *queue_new(int cap)
{
    Queue *q = (Queue *)malloc(sizeof(Queue));
    q->data = (void **)calloc((size_t)cap, sizeof(void *));
    q->head = q->tail = 0;
    q->cap = cap;
    return q;
}
static void queue_push(Queue *q, void *v) { q->data[q->tail++] = v; }
static void *queue_pop(Queue *q) { return q->data[q->head++]; }
static bool queue_empty(Queue *q) { return q->head >= q->tail; }
static void queue_free(Queue *q) { free(q->data); free(q); }

/* ========== 1. 克隆图（BFS）========== */
/* 用 BFS 遍历原图，建立原节点→克隆节点的映射 */
#define MAX_NODES 128

static GNode *clone_graph(GNode *node)
{
    if (!node) return NULL;
    GNode *map[MAX_NODES] = {NULL};
    map[node->val] = node_new(node->val);

    Queue *q = queue_new(MAX_NODES);
    queue_push(q, node);
    while (!queue_empty(q)) {
        GNode *cur = (GNode *)queue_pop(q);
        GNode *clone = map[cur->val];
        for (int i = 0; i < cur->neighbor_cnt; i++) {
            GNode *nb = cur->neighbors[i];
            if (!map[nb->val]) {
                map[nb->val] = node_new(nb->val);
                queue_push(q, nb);
            }
            node_add_neighbor(clone, map[nb->val]);
        }
    }
    queue_free(q);
    return map[node->val];
}

/* 释放图（DFS 递归） */
static void graph_free(GNode *node, char visited[])
{
    if (!node || visited[node->val]) return;
    visited[node->val] = 1;
    for (int i = 0; i < node->neighbor_cnt; i++)
        graph_free(node->neighbors[i], visited);
    free(node->neighbors);
    free(node);
}

/* 打印图（BFS） */
static void graph_print(GNode *node)
{
    if (!node) return;
    char visited[MAX_NODES] = {0};
    Queue *q = queue_new(MAX_NODES);
    queue_push(q, node);
    visited[node->val] = 1;
    while (!queue_empty(q)) {
        GNode *cur = (GNode *)queue_pop(q);
        printf("  %d -> [", cur->val);
        for (int i = 0; i < cur->neighbor_cnt; i++)
            printf("%s%d", i > 0 ? "," : "", cur->neighbors[i]->val);
        printf("]\n");
        for (int i = 0; i < cur->neighbor_cnt; i++) {
            GNode *nb = cur->neighbors[i];
            if (!visited[nb->val]) {
                visited[nb->val] = 1;
                queue_push(q, nb);
            }
        }
    }
    queue_free(q);
}

/* ========== 2. 课程表（拓扑排序）========== */
/* n 门课，prerequisites[i] = [先修, 课程]，判断能否完成 */
#define MAX_COURSES 16

static bool can_finish(int n, int prereq[][2], int m)
{
    /* 建图 + 入度 */
    int indeg[MAX_COURSES] = {0};
    int adj[MAX_COURSES][MAX_COURSES] = {{0}};
    int deg[MAX_COURSES] = {0};
    for (int i = 0; i < m; i++) {
        int a = prereq[i][0], b = prereq[i][1];
        adj[a][deg[a]++] = b;
        indeg[b]++;
    }

    /* Kahn 算法 */
    int q[MAX_COURSES], head = 0, tail = 0, cnt = 0;
    for (int i = 0; i < n; i++)
        if (indeg[i] == 0) q[tail++] = i;
    while (head < tail) {
        int u = q[head++];
        cnt++;
        for (int i = 0; i < deg[u]; i++)
            if (--indeg[adj[u][i]] == 0)
                q[tail++] = adj[u][i];
    }
    return cnt == n;
}

/* ========== 3. 岛屿数量（DFS 网格）========== */
/* 0=水, 1=陆地，四连通的岛屿算一个 */
static void dfs_sink(char grid[][8], int rows, int cols, int r, int c)
{
    if (r < 0 || r >= rows || c < 0 || c >= cols || grid[r][c] == '0')
        return;
    grid[r][c] = '0';
    dfs_sink(grid, rows, cols, r - 1, c);
    dfs_sink(grid, rows, cols, r + 1, c);
    dfs_sink(grid, rows, cols, r, c - 1);
    dfs_sink(grid, rows, cols, r, c + 1);
}

static int num_islands(char grid[][8], int rows, int cols)
{
    int cnt = 0;
    for (int r = 0; r < rows; r++)
        for (int c = 0; c < cols; c++)
            if (grid[r][c] == '1') {
                cnt++;
                dfs_sink(grid, rows, cols, r, c);
            }
    return cnt;
}

/* ========== main: 运行所有示例 ========== */
int main(void)
{
    /* --- 1. 克隆图 --- */
    printf("=== 克隆图（BFS）===\n");
    /* 构建有向图: 1->2, 1->3, 2->4, 3->5 */
    GNode *n1 = node_new(1);
    GNode *n2 = node_new(2);
    GNode *n3 = node_new(3);
    GNode *n4 = node_new(4);
    GNode *n5 = node_new(5);
    node_add_neighbor(n1, n2); node_add_neighbor(n1, n3);
    node_add_neighbor(n2, n4);
    node_add_neighbor(n3, n5);
    printf("原图:\n");
    graph_print(n1);
    GNode *cloned = clone_graph(n1);
    printf("克隆图:\n");
    graph_print(cloned);
    char vis1[MAX_NODES] = {0};
    graph_free(n1, vis1);
    char vis2[MAX_NODES] = {0};
    graph_free(cloned, vis2);

    /* --- 2. 课程表 --- */
    printf("\n=== 课程表（拓扑排序）===\n");
    /* 示例: 2 门课, [[1,0]] => 可完成 */
    int prereq1[][2] = {{1, 0}};
    printf("  课程数=2, 先修[1->0]: %s\n",
           can_finish(2, prereq1, 1) ? "可完成" : "有环");
    /* 示例: 2 门课, [[1,0],[0,1]] => 有环 */
    int prereq2[][2] = {{1, 0}, {0, 1}};
    printf("  课程数=2, 先修[1->0, 0->1]: %s\n",
           can_finish(2, prereq2, 2) ? "可完成" : "有环");

    /* --- 3. 岛屿数量 --- */
    printf("\n=== 岛屿数量（DFS 网格）===\n");
    char grid[4][8] = {
        "11000",
        "11000",
        "00100",
        "00011"
    };
    int islands = num_islands(grid, 4, 5);
    printf("  岛屿数量: %d\n", islands);

    return 0;
}
