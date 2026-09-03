/**
 * @file main.c
 * @brief 并查集（Union-Find）：路径压缩 + 按秩合并
 */

#include <stdio.h>
#include <stdlib.h>

typedef struct { int parent, rank; } uf_node_t;
typedef struct { uf_node_t *nodes; int size; } uf_t;

/* 创建：每个元素独立成集合 */
uf_t *uf_create(int n)
{
    if (n <= 0) return NULL;
    uf_t *uf = malloc(sizeof(uf_t));
    uf->nodes = malloc(n * sizeof(uf_node_t));
    uf->size = n;
    for (int i = 0; i < n; i++) { uf->nodes[i].parent = i; uf->nodes[i].rank = 0; }
    return uf;
}

void uf_destroy(uf_t *uf) { free(uf->nodes); free(uf); }

/* 查找（路径压缩）：递归将路径上节点直连根 */
int uf_find(uf_t *uf, int x)
{
    if (!uf || x < 0 || x >= uf->size) return -1;
    if (uf->nodes[x].parent != x)
        uf->nodes[x].parent = uf_find(uf, uf->nodes[x].parent);
    return uf->nodes[x].parent;
}

/* 合并（按秩）：小树合入大树，保持平衡 */
void uf_union(uf_t *uf, int x, int y)
{
    if (!uf) return;
    int rx = uf_find(uf, x), ry = uf_find(uf, y);
    if (rx == -1 || ry == -1 || rx == ry) return;
    if (uf->nodes[rx].rank < uf->nodes[ry].rank) uf->nodes[rx].parent = ry;
    else if (uf->nodes[rx].rank > uf->nodes[ry].rank) uf->nodes[ry].parent = rx;
    else { uf->nodes[ry].parent = rx; uf->nodes[rx].rank++; }
}

/* 判断连通性 */
int uf_connected(uf_t *uf, int x, int y)
{
    if (!uf || x < 0 || y < 0 || x >= uf->size || y >= uf->size) return -1;
    return uf_find(uf, x) == uf_find(uf, y);
}

/*==============================
 * 演示
 *==============================*/
#define N 10

int main(void)
{
    printf("=== 并查集演示：路径压缩 + 按秩合并 ===\n\n");

    uf_t *uf = uf_create(N);
    if (!uf) return perror("创建失败"), EXIT_FAILURE;

    printf("[初始状态]\n");
    for (int i = 0; i < N; i++)
        printf("  %d: parent=%d, rank=%d\n", i, uf->nodes[i].parent, uf->nodes[i].rank);

    printf("\n[合并序列]\n");
    uf_union(uf, 0, 1); printf("union(0,1)\n");
    uf_union(uf, 2, 3); printf("union(2,3)\n");
    uf_union(uf, 0, 2); printf("union(0,2) -> {0,1,2,3}\n");
    uf_union(uf, 4, 5); printf("union(4,5)\n");
    uf_union(uf, 6, 7); printf("union(6,7)\n");
    uf_union(uf, 4, 6); printf("union(4,6) -> {4,5,6,7}\n");
    uf_union(uf, 0, 4); printf("union(0,4) -> {0..7}\n");

    printf("\n[连通性查询]\n");
    printf("  0-3: %s\n", uf_connected(uf, 0, 3) ? "连通" : "不连通");
    printf("  0-7: %s\n", uf_connected(uf, 0, 7) ? "连通" : "不连通");
    printf("  0-8: %s\n", uf_connected(uf, 0, 8) ? "连通" : "不连通");
    printf("  5-7: %s\n", uf_connected(uf, 5, 7) ? "连通" : "不连通");
    printf("  8-9: %s\n", uf_connected(uf, 8, 9) ? "连通" : "不连通");

    printf("\n[连通性矩阵]\n   ");
    for (int j = 0; j < N; j++) printf(" %d", j);
    printf("\n");
    for (int i = 0; i < N; i++) {
        printf(" %d:", i);
        for (int j = 0; j < N; j++) printf(" %d", uf_connected(uf, i, j));
        printf("\n");
    }

    printf("\n[路径压缩效果]\n");
    uf_find(uf, 1); uf_find(uf, 3);
    printf("find(1), find(3) 后节点状态:\n");
    for (int i = 0; i < N; i++)
        printf("  %d: parent=%d, rank=%d\n", i, uf->nodes[i].parent, uf->nodes[i].rank);

    printf("\n=== 完成 ===\n");
    uf_destroy(uf);
    return EXIT_SUCCESS;
}
