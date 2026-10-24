/*
 * persistent_ds.c —— 可持久化数据结构演示
 *
 * ============================================================
 * 主席树（可持久化线段树）—— 区间第 K 小问题
 * ============================================================
 *
 * 问题：给定数组 arr，多次查询 [L, R] 区间内第 K 小的数。
 *
 * 如果只查询一次：排序后取第 K 个 → O(n log n)
 * 如果查询 Q 次：可持久化线段树 → O((n+Q) log n)
 *
 * 核心思想：
 * 1. 离散化：将数组值映射到 [1, m] 的排名
 * 2. 构建可持久化线段树：
 *    - 线段树维护"值域"而非"下标范围"
 *    - tree[node] = 该值域内出现的元素个数
 *    - 按照 arr[0], arr[1], ..., arr[n-1] 的顺序依次插入，
 *      每次插入创建新版本 root[i+1]（基于 root[i] 修改路径）
 * 3. 查询 [L, R] 区间第 K 小：
 *    - 版本 root[R] 和 root[L-1] 的每个节点作差，
 *      得到 [L, R] 区间内在该值域内出现的元素个数
 *    - 根据个数决定走左子树还是右子树（类似 BST 的查找）
 *
 * ============================================================
 * 简化演示：路径复制思想
 * ============================================================
 *
 * 本文件用简化版演示可持久化的核心思想——路径复制。
 * 完整的主席树实现请参考 src/self_made/tree/ 中的相关代码。
 *
 * 以下演示：对一个小数组创建多个版本的"可持久化数组"。
 * 每次修改一个位置，通过路径复制创建新版本。
 */

#include <ds/persistent_ds.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * 简化的可持久化线段树节点。
 * 每个节点存储：值域区间元素个数 + 左右子节点指针。
 */
typedef struct ps_node {
    int             count;    /* 该值域内的元素个数 */
    struct ps_node *left;
    struct ps_node *right;
} ps_node_t;

#define PS_VALUE_MAX 100

/* 创建节点 */
static ps_node_t *ps_node_create(void)
{
    ps_node_t *node = (ps_node_t *)calloc(1u, sizeof(ps_node_t));
    return node;
}

/*
 * 可持久化更新：在已有版本 root 的基础上，在位置 pos 处 +1。
 * 返回新版本的根节点。
 *
 * 关键：对路径上的节点创建副本，未修改的子树复用旧节点。
 *
 * 参数：
 *   root: 旧版本的根节点
 *   L, R: 当前节点代表的值域范围 [L, R]
 *   pos:  要更新的位置（离散化后的排名）
 *
 * 返回：新版本的根节点
 */
static ps_node_t *ps_update(ps_node_t *root, int L, int R, int pos)
{
    ps_node_t *new_node = ps_node_create();
    if (!new_node) return NULL;

    if (root) {
        /* 复制旧节点的数据 */
        new_node->count = root->count + 1;
        new_node->left = root->left;
        new_node->right = root->right;
    } else {
        new_node->count = 1;
    }

    if (L == R) {
        /* 叶子节点：只更新 count，不需要子节点 */
        return new_node;
    }

    int mid = (L + R) / 2;
    if (pos <= mid) {
        /* 在左子树更新 —— 左子需要复制，右子复用旧版本 */
        new_node->left = ps_update(root ? root->left : NULL, L, mid, pos);
    } else {
        /* 在右子树更新 —— 右子需要复制，左子复用旧版本 */
        new_node->right = ps_update(root ? root->right : NULL, mid + 1, R, pos);
    }

    /* 更新 count（左右子节点可能有变化） */
    new_node->count = 1; /* 每个版本只插入一个新元素 */
    if (new_node->left) new_node->count += new_node->left->count;
    if (new_node->right) new_node->count += new_node->right->count;

    return new_node;
}

/*
 * 查询：在版本 vL 和 vR 的"差分"线段树中查找第 K 小。
 *
 * 原理：count(R版本) - count(L-1版本) = [L, R] 区间内该值域的元素个数。
 */
static int ps_query_kth(ps_node_t *vL, ps_node_t *vR, int L, int R, int k)
{
    if (L == R) return L; /* 找到第 K 小的值 */

    int mid = (L + R) / 2;
    int left_count = 0;

    if (vR && vR->left) left_count += vR->left->count;
    if (vL && vL->left) left_count -= vL->left->count;
    /* left_count = [L, R] 区间内在值域 [L, mid] 中的元素个数 */

    if (k <= left_count) {
        /* 第 K 小在左子树中 */
        return ps_query_kth(vL ? vL->left : NULL, vR ? vR->left : NULL, L, mid, k);
    } else {
        /* 第 K 小在右子树中 */
        return ps_query_kth(vL ? vL->right : NULL, vR ? vR->right : NULL, mid + 1, R, k - left_count);
    }
}

/* 递归释放 */
static void ps_free(ps_node_t *node)
{
    if (!node) return;
    ps_free(node->left);
    ps_free(node->right);
    free(node);
}

/* ============================================================
 * 演示函数
 * ============================================================ */

void ds_persistent_ds_demo(void)
{
    printf("========== 可持久化数据结构演示 ==========\n");

    printf("\n【路径复制思想】\n");
    printf("普通线段树更新：原地修改路径上的节点，旧版本丢失\n");
    printf("可持久化线段树：对修改的节点创建副本，未修改的子树共享\n");
    printf("每次更新只需 O(log n) 个新节点\n");

    /* 简化的可持久化线段表演示 */
    printf("\n【主席树：区间第 K 小】\n");
    int arr[] = {5, 2, 6, 3, 1, 4};
    int n = 6;

    printf("数组: [");
    for (int i = 0; i < n; ++i) printf("%d%s", arr[i], i < n - 1 ? ", " : "");
    printf("]\n");

    /* 构建可持久化线段树（每个元素插入一个新版本） */
    ps_node_t **roots = (ps_node_t **)calloc((size_t)(n + 1), sizeof(ps_node_t *));
    if (!roots) { printf("内存分配失败\n"); return; }

    roots[0] = NULL; /* 空版本 */
    for (int i = 0; i < n; ++i) {
        roots[i + 1] = ps_update(roots[i], 1, PS_VALUE_MAX, arr[i]);
    }

    /* 查询 [L, R] 区间第 K 小 */
    printf("\n查询：\n");
    printf("  [1, 4] 第 2 小: %d（预期：3）\n",
           ps_query_kth(roots[0], roots[4], 1, PS_VALUE_MAX, 2));
    printf("  [0, 5] 第 1 小: %d（预期：1）\n",
           ps_query_kth(roots[0], roots[6], 1, PS_VALUE_MAX, 1));
    printf("  [0, 5] 第 4 小: %d（预期：4）\n",
           ps_query_kth(roots[0], roots[6], 1, PS_VALUE_MAX, 4));
    printf("  [2, 4] 第 2 小: %d（预期：3）\n",
           ps_query_kth(roots[1], roots[4], 1, PS_VALUE_MAX, 2));

    /* 释放 */
    for (int i = 0; i <= n; ++i) ps_free(roots[i]);
    free(roots);

    printf("\n【其他可持久化数据结构】\n");
    printf("  - 可持久化数组：修改某个位置时复制路径\n");
    printf("  - 可持久化 Trie：用于最大异或和等问题\n");
    printf("  - 可持久化并查集：Git 风格的版本控制\n");
    printf("  - 可持久化平衡树：函数式编程中的不可变集合\n");

    printf("========== 可持久化数据结构演示结束 ==========\n");
}
