/*
 * segment_tree.c —— 线段树实现（区间求和 + lazy propagation）
 *
 * ============================================================
 * 数据结构内部定义
 * ============================================================
 *
 *   struct ds_segment_tree {
 *       int64_t *tree;   // 线段树数组（存储区间和）
 *       int64_t *lazy;   // 惰性标记数组（存储待下推的增量）
 *       size_t   n;      // 原始数组大小
 *   };
 *
 * 数组表示（下标从 0 开始但逻辑上从 1 开始）：
 *   - root 的下标 = 1，表示区间 [0, n-1]
 *   - 对于下标 i 的节点：
 *       left_child  = i * 2
 *       right_child = i * 2 + 1
 *       mid = (L + R) / 2
 *
 * ============================================================
 * Lazy Propagation（惰性传播）原理
 * ============================================================
 *
 * 问题：如果要对整个区间 [0, n-1] 每个元素 +1，单点更新需要 O(n log n)，
 * 太慢了。如何优化到 O(log n)？
 *
 * 惰性传播的核心思想：延迟更新。
 * - 当更新一个完全覆盖当前节点区间的范围时，直接在当前节点记录增量，
 *   设置 lazy 标记，不继续向下更新子节点。
 * - 当后续查询或更新需要访问子节点时，将 lazy 标记向下"推"给子节点，
 *   并清除当前节点的 lazy 标记。
 *
 * 示例：对 [1, 5] 每个元素 +3
 *
 *   update_range(1, 1, 8, 1, 5, 3):
 *     node [1,8] 不完全在 [1,5] 内 → 递归左右子节点
 *       node [1,4] 完全在 [1,5] 内 → tree[1,4] += 4*3=12, lazy[1,4] += 3
 *       node [5,8] 部分在 [1,5] 内 → 递归
 *         node [5,6] 完全在 [1,5] 内 → tree[5,6] += 2*3=6, lazy[5,6] += 3
 *         node [7,8] 完全不在 → 不处理
 *
 * 后续查询 [3, 4] 经过 node [1,4] 时：
 *   push_down: tree[left] += lazy[1,4]*2, lazy[left] += lazy[1,4]
 *              tree[right] += lazy[1,4]*2, lazy[right] += lazy[1,4]
 *              lazy[1,4] = 0
 *   然后正常查询子节点。
 */

#include <ds/segment_tree.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DS_SEGTREE_MAX(a, b) ((a) > (b) ? (a) : (b))
#define DS_SEGTREE_MIN(a, b) ((a) < (b) ? (a) : (b))

struct ds_segment_tree {
    int64_t *tree;   /* 线段树数组：tree[i] = 节点 i 对应区间的和 */
    int64_t *lazy;   /* 惰性标记数组 */
    size_t   n;      /* 原数组大小 */
    size_t   capacity; /* 线段树数组总大小 */
};

/*
 * 递归建树（自底向上）。
 *
 * 参数：
 *   node: 当前线段树节点下标
 *   L, R: 当前节点表示的区间 [L, R]
 *   arr: 原始数组
 *
 * 终止条件：L == R（叶子节点），tree[node] = arr[L]
 * 递归：建左右子树，tree[node] = tree[left] + tree[right]
 */
static void seqtree_build(ds_segtree_t *tree, size_t node, size_t L, size_t R,
                           const int64_t *arr)
{
    if (L == R) {
        tree->tree[node] = arr[L];
        return;
    }

    size_t mid = (L + R) / 2u;
    seqtree_build(tree, node * 2u, L, mid, arr);
    seqtree_build(tree, node * 2u + 1u, mid + 1u, R, arr);
    tree->tree[node] = tree->tree[node * 2u] + tree->tree[node * 2u + 1u];
}

/* ============================================================
 * push_down：将当前节点的 lazy 标记下传到子节点
 *
 * 在访问子节点之前调用，确保子节点的值是最新的。
 *
 *   tree[left] += lazy[node] * left_range_length
 *   lazy[left] += lazy[node]
 *   tree[right] += lazy[node] * right_range_length
 *   lazy[right] += lazy[node]
 *   lazy[node] = 0  （清除当前标记）
 * ============================================================ */
static void seqtree_push_down(ds_segtree_t *tree, size_t node, size_t L, size_t R)
{
    if (tree->lazy[node] == 0) {
        return;
    }

    size_t mid = (L + R) / 2u;
    size_t left_node = node * 2u;
    size_t right_node = node * 2u + 1u;

    /* 下传到左子 */
    tree->tree[left_node] += tree->lazy[node] * (int64_t)(mid - L + 1u);
    tree->lazy[left_node] += tree->lazy[node];

    /* 下传到右子 */
    tree->tree[right_node] += tree->lazy[node] * (int64_t)(R - mid);
    tree->lazy[right_node] += tree->lazy[node];

    /* 清除当前标记 */
    tree->lazy[node] = 0;
}

/*
 * 单点更新：arr[idx] += delta
 *
 * 递归找到叶子节点，沿路径更新所有祖先的区间和。
 */
static void seqtree_update_point_impl(ds_segtree_t *tree, size_t node,
                                       size_t L, size_t R, size_t idx, int64_t delta)
{
    if (L == R) {
        tree->tree[node] += delta;
        return;
    }

    seqtree_push_down(tree, node, L, R);

    size_t mid = (L + R) / 2u;
    if (idx <= mid) {
        seqtree_update_point_impl(tree, node * 2u, L, mid, idx, delta);
    } else {
        seqtree_update_point_impl(tree, node * 2u + 1u, mid + 1u, R, idx, delta);
    }

    /* 更新祖先的区间和 */
    tree->tree[node] = tree->tree[node * 2u] + tree->tree[node * 2u + 1u];
}

int ds_segtree_update_point(ds_segtree_t *tree, size_t idx, int64_t delta)
{
    if (!tree || idx >= tree->n) {
        return -1;
    }

    seqtree_update_point_impl(tree, 1u, 0u, tree->n - 1u, idx, delta);
    return 0;
}

/*
 * 区间更新：[qL, qR] 每个元素 += delta
 *
 * 三种情况：
 *   1. 完全覆盖 [L,R]：直接更新 tree[node] 和 lazy[node]
 *   2. 完全不重叠：直接返回
 *   3. 部分重叠：push_down 后递归处理左右子节点
 */
static void seqtree_update_range_impl(ds_segtree_t *tree, size_t node,
                                       size_t L, size_t R,
                                       size_t qL, size_t qR, int64_t delta)
{
    if (qL > R || qR < L) {
        /* 完全不重叠 */
        return;
    }

    if (qL <= L && R <= qR) {
        /* 完全覆盖：打上 lazy 标记 */
        tree->tree[node] += delta * (int64_t)(R - L + 1u);
        tree->lazy[node] += delta;
        return;
    }

    /* 部分重叠：先下推标记，再递归 */
    seqtree_push_down(tree, node, L, R);

    size_t mid = (L + R) / 2u;
    seqtree_update_range_impl(tree, node * 2u, L, mid, qL, qR, delta);
    seqtree_update_range_impl(tree, node * 2u + 1u, mid + 1u, R, qL, qR, delta);
    tree->tree[node] = tree->tree[node * 2u] + tree->tree[node * 2u + 1u];
}

int ds_segtree_update_range(ds_segtree_t *tree, size_t L, size_t R, int64_t delta)
{
    if (!tree || L > R || R >= tree->n) {
        return -1;
    }

    seqtree_update_range_impl(tree, 1u, 0u, tree->n - 1u, L, R, delta);
    return 0;
}

/*
 * 区间查询：求 [qL, qR] 的区间和
 *
 * 三种情况同上：
 *   1. 完全覆盖 → 返回 tree[node]
 *   2. 完全不重叠 → 返回 0
 *   3. 部分重叠 → push_down 后递归查询左右子区间
 */
static int64_t seqtree_query_impl(const ds_segtree_t *tree, size_t node,
                                   size_t L, size_t R, size_t qL, size_t qR)
{
    if (qL > R || qR < L) {
        return 0;
    }

    if (qL <= L && R <= qR) {
        return tree->tree[node];
    }

    /* 查询前先下推 lazy 标记（因为需要访问子节点） */
    seqtree_push_down((ds_segtree_t *)tree, node, L, R);

    size_t mid = (L + R) / 2u;
    int64_t left_sum = seqtree_query_impl(tree, node * 2u, L, mid, qL, qR);
    int64_t right_sum = seqtree_query_impl(tree, node * 2u + 1u, mid + 1u, R, qL, qR);
    return left_sum + right_sum;
}

int64_t ds_segtree_query(const ds_segtree_t *tree, size_t L, size_t R)
{
    if (!tree || L > R || R >= tree->n) {
        return 0;
    }

    return seqtree_query_impl(tree, 1u, 0u, tree->n - 1u, L, R);
}

/* 创建线段树 */
ds_segtree_t *ds_segtree_create(const int64_t *arr, size_t n)
{
    ds_segtree_t *tree;

    if (!arr || n == 0u) {
        return NULL;
    }

    tree = (ds_segtree_t *)calloc(1u, sizeof(ds_segtree_t));
    if (!tree) {
        return NULL;
    }

    /* 4×n 确保足够空间 */
    tree->capacity = 4u * n;
    tree->n = n;

    tree->tree = (int64_t *)calloc(tree->capacity, sizeof(int64_t));
    tree->lazy = (int64_t *)calloc(tree->capacity, sizeof(int64_t));
    if (!tree->tree || !tree->lazy) {
        free(tree->tree);
        free(tree->lazy);
        free(tree);
        return NULL;
    }

    seqtree_build(tree, 1u, 0u, n - 1u, arr);
    return tree;
}

void ds_segtree_destroy(ds_segtree_t *tree)
{
    if (!tree) return;
    free(tree->tree);
    free(tree->lazy);
    free(tree);
}

/* ============================================================
 * 演示函数
 * ============================================================ */

void ds_segment_tree_demo(void)
{
    printf("========== 线段树演示（区间求和） ==========\n");

    int64_t arr[] = {1, 3, 5, 7, 9, 11};
    size_t n = 6;

    printf("\n原数组: [");
    for (size_t i = 0; i < n; ++i) printf("%lld%s", (long long)arr[i], i < n-1 ? ", " : "");
    printf("]\n");

    ds_segtree_t *tree = ds_segtree_create(arr, n);
    if (!tree) { printf("创建失败\n"); return; }

    printf("区间和 [0,2]: %lld\n", (long long)ds_segtree_query(tree, 0, 2));
    printf("区间和 [3,5]: %lld\n", (long long)ds_segtree_query(tree, 3, 5));
    printf("区间和 [1,4]: %lld\n", (long long)ds_segtree_query(tree, 1, 4));

    printf("\n【单点更新：arr[2] += 10】\n");
    ds_segtree_update_point(tree, 2, 10);
    printf("区间和 [0,2] 更新后: %lld\n", (long long)ds_segtree_query(tree, 0, 2));

    printf("\n【区间更新：[1, 3] += 5】\n");
    ds_segtree_update_range(tree, 1, 3, 5);
    printf("区间和 [0,2] 更新后: %lld\n", (long long)ds_segtree_query(tree, 0, 2));
    printf("区间和 [1,3] 更新后: %lld\n", (long long)ds_segtree_query(tree, 1, 3));
    printf("区间和 [0,5] 全部: %lld\n", (long long)ds_segtree_query(tree, 0, 5));

    ds_segtree_destroy(tree);
    printf("========== 线段树演示结束 ==========\n");
}
