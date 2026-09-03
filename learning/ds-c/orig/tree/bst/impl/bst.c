/*
 * bst.c —— 二叉搜索树实现
 *
 * ============================================================
 * 数据结构内部定义
 * ============================================================
 *
 * BST 节点结构（已在头文件中暴露）：
 *
 *   ┌───────────┐
 *   │ data      │ → 用户数据
 *   │ left      │ → 左子节点（所有值 < 当前节点）
 *   │ right     │ → 右子节点（所有值 > 当前节点）
 *   └───────────┘
 *
 * BST 性质图解：
 *
 *           8              ← 根
 *         /   \
 *        3     10          ← 3 < 8 < 10
 *       / \      \
 *      1   6      14       ← 1 < 3, 6 > 3 且 6 < 8
 *         / \    /
 *        4   7  13          ← 叶子或半叶节点
 *
 * 中序遍历结果：1, 3, 4, 6, 7, 8, 10, 13, 14（有序！）
 *
 * ============================================================
 * 退化风险
 * ============================================================
 *
 * 如果按 [1, 2, 3, 4, 5] 的顺序插入 BST：
 *
 *   1
 *    \
 *     2
 *      \
 *       3
 *        \
 *         4
 *          \
 *           5
 *
 * 退化为链表！查找效率从 O(log n) 变成 O(n)。
 * 这就是后续需要学习 AVL 树和红黑树的原因。
 *
 * ============================================================
 * 删除操作详解
 * ============================================================
 *
 * 删除 3 种情况：
 *
 * 情况1（叶子节点）：直接删除即可。
 *   例：删除 7
 *        8                8
 *       / \              / \
 *      3   10    →     3   10
 *     / \    \        / \    \
 *    1   6   14      1   6   14
 *       / \               \
 *      4   7               4
 *
 * 情况2（只有一个子节点）：用子节点替换。
 *   例：删除 10（只有右子 14）
 *        8                8
 *       / \              / \
 *      3   10    →     3   14
 *     / \    \        / \  /
 *    1   6   14      1   6 13
 *       / \  /           / \
 *      4   7 13          4   7
 *
 * 情况3（有两个子节点）：找后继（右子树最小节点）替换。
 *   例：删除 3（左右子节点都有）
 *      后继 = min(右子树) = 4（右子树中最左的节点）
 *        8                8
 *       / \              / \
 *      3   10    →     4   10
 *     / \    \        / \    \
 *    1   6   14      1   6   14
 *       / \               \
 *      4   7               7
 */

#include <ds/bst.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 内部辅助：创建节点 */
static ds_bst_node_t *bst_node_create(const void *data, size_t element_size)
{
    ds_bst_node_t *node;

    if (element_size == 0u) {
        return NULL;
    }

    node = (ds_bst_node_t *)calloc(1u, sizeof(ds_bst_node_t));
    if (!node) {
        return NULL;
    }

    if (data) {
        node->data = malloc(element_size);
        if (!node->data) {
            free(node);
            return NULL;
        }
        memcpy(node->data, data, element_size);
    }

    return node;
}

/* 内部辅助：找后继 —— 右子树中的最小节点（即右子树最左） */
static ds_bst_node_t *bst_find_min_node(ds_bst_node_t *node)
{
    while (node && node->left) {
        node = node->left;
    }
    return node;
}

/* ============================================================
 * 插入操作
 *
 * 递归公式：
 *   bst_insert(root, data):
 *     if root == NULL: return new_node(data)
 *     if data < root->data: root->left = bst_insert(root->left, data)
 *     else:                 root->right = bst_insert(root->right, data)
 *     return root
 *
 * 注意：允许重复值（插入右子树）。
 * ============================================================ */
ds_bst_node_t *ds_bst_insert(ds_bst_node_t *root, const void *data,
                              size_t element_size, ds_bst_compare_fn compare)
{
    if (!compare) {
        return root;
    }

    if (!root) {
        /* 终止条件：到达空位置，创建新节点 */
        return bst_node_create(data, element_size);
    }

    if (compare(data, root->data) < 0) {
        /* 新数据更小 → 递归插入左子树 */
        root->left = ds_bst_insert(root->left, data, element_size, compare);
    } else {
        /* 新数据更大或相等 → 递归插入右子树（允许重复） */
        root->right = ds_bst_insert(root->right, data, element_size, compare);
    }

    return root;
}

/* ============================================================
 * 搜索操作
 *
 * 递归公式：
 *   bst_search(root, key):
 *     if root == NULL:          return NULL
 *     if key == root->data:     return root
 *     if key < root->data:      return bst_search(root->left, key)
 *     else:                     return bst_search(root->right, key)
 * ============================================================ */
const ds_bst_node_t *ds_bst_search(const ds_bst_node_t *root, const void *key,
                                    ds_bst_compare_fn compare)
{
    int cmp;

    if (!root || !compare) {
        return NULL;
    }

    cmp = compare(key, root->data);
    if (cmp == 0) {
        return root;
    } else if (cmp < 0) {
        return ds_bst_search(root->left, key, compare);
    } else {
        return ds_bst_search(root->right, key, compare);
    }
}

/* ============================================================
 * 删除操作（最复杂）
 * ============================================================ */
ds_bst_node_t *ds_bst_delete(ds_bst_node_t *root, const void *key,
                              ds_bst_compare_fn compare, void (*free_data)(void *))
{
    int cmp;

    if (!root || !compare) {
        return NULL;
    }

    cmp = compare(key, root->data);
    if (cmp < 0) {
        /* key 在左子树中 */
        root->left = ds_bst_delete(root->left, key, compare, free_data);
    } else if (cmp > 0) {
        /* key 在右子树中 */
        root->right = ds_bst_delete(root->right, key, compare, free_data);
    } else {
        /* 找到要删除的节点 */
        ds_bst_node_t *to_delete = root;

        /* 情况1 + 情况2：只有一个子节点或没有子节点 */
        if (!root->left) {
            root = root->right;
        } else if (!root->right) {
            root = root->left;
        } else {
            /*
             * 情况3：有两个子节点
             * 找后继（右子树中的最小节点），将其数据复制上来，
             * 然后递归地从右子树中删除后继节点。
             */
            ds_bst_node_t *successor = bst_find_min_node(root->right);

            /* 复制后继的数据到当前节点 */
            if (free_data && root->data) {
                free_data(root->data);
            }
            root->data = successor->data;

            /* 从右子树删除后继节点 */
            root->right = ds_bst_delete(root->right, successor->data, compare, NULL);

            /* 注意：successor->data 已移到 root，不需要释放 */
            return root;
        }

        /* 释放旧节点 */
        if (free_data && to_delete->data) {
            free_data(to_delete->data);
        }
        free(to_delete);
    }
    }

    return root;
}

/* ============================================================
 * 验证 BST 合法性
 *
 * 方法：中序遍历，记录前一个访问的值，检查是否严格递增。
 * ============================================================ */
static bool bst_is_valid_impl(const ds_bst_node_t *root, ds_bst_compare_fn compare,
                               const void **prev)
{
    if (!root) {
        return true;
    }

    /* 左子树 */
    if (!bst_is_valid_impl(root->left, compare, prev)) {
        return false;
    }

    /* 当前节点：检查是否大于前驱 */
    if (*prev && compare(root->data, *prev) <= 0) {
        return false;
    }
    *prev = root->data;

    /* 右子树 */
    return bst_is_valid_impl(root->right, compare, prev);
}

bool ds_bst_is_valid(const ds_bst_node_t *root, ds_bst_compare_fn compare)
{
    const void *prev = NULL;
    return bst_is_valid_impl(root, compare, &prev);
}

const ds_bst_node_t *ds_bst_min(const ds_bst_node_t *root)
{
    if (!root) {
        return NULL;
    }
    while (root->left) {
        root = root->left;
    }
    return root;
}

const ds_bst_node_t *ds_bst_max(const ds_bst_node_t *root)
{
    if (!root) {
        return NULL;
    }
    while (root->right) {
        root = root->right;
    }
    return root;
}

void ds_bst_destroy(ds_bst_node_t *root, void (*free_data)(void *))
{
    if (!root) {
        return;
    }
    ds_bst_destroy(root->left, free_data);
    ds_bst_destroy(root->right, free_data);
    if (free_data && root->data) {
        free_data(root->data);
    }
    free(root->data);
    free(root);
}

/* ============================================================
 * 演示函数
 * ============================================================ */

static int int_compare(const void *a, const void *b)
{
    return *(const int *)a - *(const int *)b;
}

static void print_bst_inorder(const ds_bst_node_t *root)
{
    if (!root) return;
    print_bst_inorder(root->left);
    printf("%d ", *(const int *)root->data);
    print_bst_inorder(root->right);
}

void ds_bst_demo(void)
{
    printf("========== BST 二叉搜索树演示 ==========\n");

    ds_bst_node_t *root = NULL;

    printf("\n【插入序列: 8, 3, 10, 1, 6, 14, 4, 7, 13】\n");
    int nums[] = {8, 3, 10, 1, 6, 14, 4, 7, 13};
    for (int i = 0; i < 9; ++i) {
        root = ds_bst_insert(root, &nums[i], sizeof(int), int_compare);
    }

    printf("中序遍历: ");
    print_bst_inorder(root);
    printf("\n");

    printf("最小值: %d\n", *(const int *)ds_bst_min(root)->data);
    printf("最大值: %d\n", *(const int *)ds_bst_max(root)->data);

    /* 搜索演示 */
    int key = 6;
    const ds_bst_node_t *found = ds_bst_search(root, &key, int_compare);
    printf("搜索 %d: %s\n", key, found ? "找到" : "未找到");

    key = 99;
    found = ds_bst_search(root, &key, int_compare);
    printf("搜索 %d: %s\n", key, found ? "找到" : "未找到");

    /* 退化演示 */
    printf("\n【退化演示——有序插入 1..5】\n");
    ds_bst_node_t *degenerate = NULL;
    printf("插入顺序: 1, 2, 3, 4, 5\n");
    for (int i = 1; i <= 5; ++i) {
        degenerate = ds_bst_insert(degenerate, &i, sizeof(int), int_compare);
    }
    printf("中序遍历: ");
    print_bst_inorder(degenerate);
    printf("（值仍有序，但树结构已退化为链表！）\n");
    printf("树的高度（应接近 5）—— 查找效率从 O(log n) 退化为 O(n)\n");

    ds_bst_destroy(root, NULL);
    ds_bst_destroy(degenerate, NULL);
    printf("========== BST 演示结束 ==========\n");
}
