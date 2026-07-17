/*
 * union_find.c —— 并查集实现
 *
 * ============================================================
 * 数据结构内部定义
 * ============================================================
 *
 *   struct ds_union_find {
 *       size_t *parent;  // parent[i] = i 的父节点（根节点的 parent = 自身）
 *       size_t *rank;    // rank[i] = 以 i 为根的树的"秩"（上界高度）
 *       size_t  count;   // 当前连通分量数量
 *       size_t  n;       // 元素总数
 *   };
 *
 * ============================================================
 * 核心算法
 * ============================================================
 *
 * 【路径压缩 —— Find】
 *
 *   普通 Find：沿着 parent 一直向上直到根。
 *
 *   路径压缩：在查找过程中，将经过的节点直接连接到根。
 *   这样后续查找同一条路径上的节点时只需 O(1)。
 *
 *   示例：Find(4) 前的树：
 *       0 ← 1 ← 2 ← 3 ← 4
 *
 *   Find(4) 后（路径压缩）：
 *       0 ← 1
 *        ↑  ↙
 *       2 ← 3 ← 4
 *
 *   更彻底的压缩（递归版本）：
 *       0 ← 1,2,3,4（全部直接连到根）
 *
 *   实现：
 *     if parent[x] != x:
 *         parent[x] = find(parent[x])  // 递归压缩
 *     return parent[x]
 *
 * 【按秩合并 —— Union】
 *
 *   普通 Union：将一棵树的根直接指向另一棵树的根。
 *   随意连接可能导致树退化为链表（查找 O(n)）。
 *
 *   按秩合并：比较两棵树的高度（秩），总是将矮树接在高树下。
 *   这样树高最多增长 1，维持在 O(log n) 以下。
 *
 *   步骤：
 *     1. rootX = find(x), rootY = find(y)
 *     2. if rootX == rootY: 已经同集合，返回
 *     3. if rank[rootX] < rank[rootY]:
 *            parent[rootX] = rootY
 *        else if rank[rootX] > rank[rootY]:
 *            parent[rootY] = rootX
 *        else:  // 秩相等，任选一个为根，新根秩+1
 *            parent[rootY] = rootX
 *            rank[rootX]++
 *     4. count--
 *
 * 为什么"秩"不是精确高度？
 * 路径压缩会改变树的结构，维护精确高度代价太大。
 * 秩是"上界"估计，按秩合并即便在路径压缩下仍然有效。
 *
 * ============================================================
 * 应用：Kruskal 最小生成树
 * ============================================================
 *
 * 算法：
 *   1. 将所有边按权重升序排列
 *   2. 遍历每条边 (u, v, w):
 *      if !connected(u, v):  // 并查集判断
 *          union(u, v)        // 合并
 *          mst_cost += w
 *   3. 如果选了 n-1 条边，则 MST 完成
 *
 * 并查集的作用：判断 u 和 v 是否已经连通，避免在 MST 中形成环路。
 */

#include <ds/union_find.h>

#include <stdio.h>
#include <stdlib.h>

struct ds_union_find {
    size_t *parent;
    size_t *rank;
    size_t  count;
    size_t  n;
};

ds_uf_t *ds_uf_create(size_t n)
{
    ds_uf_t *uf;

    if (n == 0u) {
        return NULL;
    }

    uf = (ds_uf_t *)calloc(1u, sizeof(ds_uf_t));
    if (!uf) {
        return NULL;
    }

    uf->parent = (size_t *)malloc(sizeof(size_t) * n);
    uf->rank = (size_t *)calloc(n, sizeof(size_t));
    if (!uf->parent || !uf->rank) {
        free(uf->parent);
        free(uf->rank);
        free(uf);
        return NULL;
    }

    /* 初始化：每个元素的父节点是它自己，每个集合大小为 1 */
    for (size_t i = 0u; i < n; ++i) {
        uf->parent[i] = i;
    }

    uf->count = n;
    uf->n = n;
    return uf;
}

void ds_uf_destroy(ds_uf_t *uf)
{
    if (!uf) return;
    free(uf->parent);
    free(uf->rank);
    free(uf);
}

/*
 * Find 操作（带路径压缩）
 *
 * 递归版本：在查找过程中将每个节点的 parent 直接指向根。
 *
 * 执行路径：
 *   find(x):
 *     if parent[x] != x:
 *         parent[x] = find(parent[x])  ← 路径压缩的关键！
 *     return parent[x]
 *
 * 非递归版本（避免递归栈溢出，适用于极深的树）：
 *   先找到根，再遍历第二遍将路径上所有节点直接指向根。
 */
size_t ds_uf_find(ds_uf_t *uf, size_t x)
{
    size_t root;

    if (!uf || x >= uf->n) {
        return (size_t)-1;
    }

    /* 找到根 */
    root = x;
    while (uf->parent[root] != root) {
        root = uf->parent[root];
    }

    /* 路径压缩：将路径上的所有节点直接指向根 */
    while (x != root) {
        size_t next = uf->parent[x];
        uf->parent[x] = root;
        x = next;
    }

    return root;
}

/*
 * Union 操作（按秩合并）
 *
 * 秩不是精确高度，而是"上界估计"。
 * 只有在两个 rank 相等时才增加秩。
 */
int ds_uf_union(ds_uf_t *uf, size_t x, size_t y)
{
    size_t root_x;
    size_t root_y;

    if (!uf || x >= uf->n || y >= uf->n) {
        return -1;
    }

    root_x = ds_uf_find(uf, x);
    root_y = ds_uf_find(uf, y);

    if (root_x == root_y) {
        return 0; /* 已在同一集合中，无需合并 */
    }

    /* 按秩合并：矮树接在高树下 */
    if (uf->rank[root_x] < uf->rank[root_y]) {
        uf->parent[root_x] = root_y;
    } else if (uf->rank[root_x] > uf->rank[root_y]) {
        uf->parent[root_y] = root_x;
    } else {
        /* 秩相等，任选一个为根，新根秩+1 */
        uf->parent[root_y] = root_x;
        uf->rank[root_x] += 1u;
    }

    uf->count -= 1u;
    return 0;
}

bool ds_uf_is_connected(const ds_uf_t *uf, size_t x, size_t y)
{
    if (!uf || x >= uf->n || y >= uf->n) {
        return false;
    }

    return ds_uf_find((ds_uf_t *)uf, x) == ds_uf_find((ds_uf_t *)uf, y);
}

size_t ds_uf_count(const ds_uf_t *uf)
{
    return uf ? uf->count : 0u;
}

/* ============================================================
 * 演示函数
 * ============================================================ */

void ds_union_find_demo(void)
{
    printf("========== 并查集演示 ==========\n");

    ds_uf_t *uf = ds_uf_create(10);
    if (!uf) { printf("创建失败\n"); return; }

    printf("\n【初始状态】10 个独立元素，连通分量数: %zu\n", ds_uf_count(uf));

    /* 模拟朋友圈合并 */
    printf("\n【合并操作】\n");
    printf("Union(0,1): 朋友 0 和 1 在同一圈\n");
    ds_uf_union(uf, 0, 1);

    printf("Union(1,2): 朋友 1 和 2 在同一圈（通过传递性，0-1-2 连通）\n");
    ds_uf_union(uf, 1, 2);

    printf("Union(3,4)\n");
    ds_uf_union(uf, 3, 4);

    printf("Union(5,6)\n");
    ds_uf_union(uf, 5, 6);

    printf("Union(6,7)\n");
    ds_uf_union(uf, 6, 7);

    printf("Union(7,8)\n");
    ds_uf_union(uf, 7, 8);

    printf("\n当前连通分量数: %zu\n", ds_uf_count(uf));

    printf("\n【连通性查询】\n");
    printf("0 和 2 连通？%s\n", ds_uf_is_connected(uf, 0, 2) ? "是" : "否");
    printf("0 和 3 连通？%s\n", ds_uf_is_connected(uf, 0, 3) ? "是" : "否");
    printf("5 和 8 连通？%s\n", ds_uf_is_connected(uf, 5, 8) ? "是" : "否");
    printf("0 和 9 连通？%s\n", ds_uf_is_connected(uf, 0, 9) ? "是" : "否");
    printf("（9 还没有任何朋友）\n");

    printf("\n【合并两个大集合：Union(2, 5)】\n");
    ds_uf_union(uf, 2, 5);
    printf("0 和 8 现在连通？%s\n", ds_uf_is_connected(uf, 0, 8) ? "是" : "否");
    printf("连通分量数: %zu\n", ds_uf_count(uf));

    ds_uf_destroy(uf);
    printf("========== 并查集演示结束 ==========\n");
}
