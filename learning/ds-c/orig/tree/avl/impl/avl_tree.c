/*
 * avl_tree.c —— AVL 树实现
 *
 * ============================================================
 * 核心辅助函数
 * ============================================================ */

#include <ds/avl_tree.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 获取节点高度（NULL 节点高度为 0） */
static int height(const ds_avl_node_t *node)
{
    return node ? node->height : 0;
}

/* 计算平衡因子：BF = height(left) - height(right) */
static int balance_factor(const ds_avl_node_t *node)
{
    return node ? height(node->left) - height(node->right) : 0;
}

/* 更新节点高度 */
static void update_height(ds_avl_node_t *node)
{
    if (!node) {
        return;
    }
    int hl = height(node->left);
    int hr = height(node->right);
    node->height = 1 + (hl > hr ? hl : hr);
}

/* 创建新节点 */
static ds_avl_node_t *avl_node_create(const void *data, size_t element_size)
{
    ds_avl_node_t *node;

    if (element_size == 0u) {
        return NULL;
    }

    node = (ds_avl_node_t *)calloc(1u, sizeof(ds_avl_node_t));
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
    node->height = 1; /* 新节点高度初始为 1 */
    return node;
}

/*
 * ============================================================
 * 旋转操作
 * ============================================================
 *
 * 【右旋（针对 LL 情况）】
 *
 *       A (+2)            B ( 0)
 *      /                /   \
 *     B (+1)    →     newR    A
 *    / \                    / \
 *  newL newR             newR  AR
 *       / \              / \
 *     BL  BR            BL BR
 *     (可能为空)
 *
 *   步骤：
 *     1. B = A->left
 *     2. A->left = B->right    （newR 成为 A 的左子）
 *     3. B->right = A          （A 成为 B 的右子）
 *     4. 更新 A 和 B 的高度
 *     5. 返回 B（新根）
 */
static ds_avl_node_t *rotate_right(ds_avl_node_t *A)
{
    ds_avl_node_t *B = A->left;
    ds_avl_node_t *newR = B->right; /* 原 B 的右子，将成为新 A 的左子 */

    /* 执行旋转 */
    B->right = A;
    A->left = newR;

    /* 更新高度（先更新子树 A，再更新根 B） */
    update_height(A);
    update_height(B);

    return B; /* B 成为新的子树根 */
}

/*
 * 【左旋（针对 RR 情况）】
 *
 *     A (-2)              B ( 0)
 *      \                /   \
 *       B (-1)   →     A     newR
 *      / \            / \
 *   newL newR       AL  newL
 *   / \
 * BL BR
 *
 *   步骤：
 *     1. B = A->right
 *     2. A->right = B->left    （newL 成为 A 的右子）
 *     3. B->left = A           （A 成为 B 的左子）
 *     4. 更新 A 和 B 的高度
 *     5. 返回 B（新根）
 */
static ds_avl_node_t *rotate_left(ds_avl_node_t *A)
{
    ds_avl_node_t *B = A->right;
    ds_avl_node_t *newL = B->left;

    B->left = A;
    A->right = newL;

    update_height(A);
    update_height(B);

    return B;
}

/*
 * ============================================================
 * 平衡函数：检查并修复 AVL 性质
 *
 * 返回平衡后的子树根节点。
 *
 * 四种情况判断：
 *   BF = +2（左子树偏高）：
 *     if BF(left) >= 0 → LL 情况 → 一次右旋
 *     if BF(left) < 0  → LR 情况 → 先左旋 left，再右旋 root
 *
 *   BF = -2（右子树偏高）：
 *     if BF(right) <= 0 → RR 情况 → 一次左旋
 *     if BF(right) > 0  → RL 情况 → 先右旋 right，再左旋 root
 *
 * 注意：BF=0 的边界情况归入 LL/RR（只需要一次旋转）。
 * ============================================================ */
static ds_avl_node_t *avl_balance(ds_avl_node_t *node)
{
    int bf;

    if (!node) {
        return NULL;
    }

    update_height(node);
    bf = balance_factor(node);

    /* LL 情况：左子树偏高，且插入在左子树的左侧 */
    if (bf > 1 && balance_factor(node->left) >= 0) {
        return rotate_right(node);
    }

    /* LR 情况：左子树偏高，但插入在左子树的右侧 */
    if (bf > 1 && balance_factor(node->left) < 0) {
        node->left = rotate_left(node->left); /* 先左旋左子 */
        return rotate_right(node);            /* 再右旋根 */
    }

    /* RR 情况：右子树偏高，且插入在右子树的右侧 */
    if (bf < -1 && balance_factor(node->right) <= 0) {
        return rotate_left(node);
    }

    /* RL 情况：右子树偏高，但插入在右子树的左侧 */
    if (bf < -1 && balance_factor(node->right) > 0) {
        node->right = rotate_right(node->right); /* 先右旋右子 */
        return rotate_left(node);                 /* 再左旋根 */
    }

    return node;
}

/* ============================================================
 * 插入操作
 *
 * 1. 标准 BST 插入（递归找到位置）
 * 2. 逐层返回时更新高度
 * 3. 检查平衡因子，必要时旋转
 *
 * 关键：插入后最多 2 次旋转就能恢复平衡。
 * 因为旋转后子树高度恢复到插入前的值，不会影响更上层的祖先。
 * ============================================================ */
ds_avl_node_t *ds_avl_insert(ds_avl_node_t *root, const void *data,
                              size_t element_size, ds_avl_compare_fn compare)
{
    if (!compare) {
        return root;
    }

    /* 步骤1：标准 BST 插入 */
    if (!root) {
        return avl_node_create(data, element_size);
    }

    if (compare(data, root->data) < 0) {
        root->left = ds_avl_insert(root->left, data, element_size, compare);
    } else if (compare(data, root->data) > 0) {
        root->right = ds_avl_insert(root->right, data, element_size, compare);
    } else {
        /* 重复值：不插入（或可选择更新数据） */
        return root;
    }

    /* 步骤2+3：更新高度并平衡 */
    return avl_balance(root);
}

/* ============================================================
 * 删除操作
 *
 * 1. 标准 BST 删除（3 种情况）
 * 2. 逐层返回时更新高度
 * 3. 检查平衡因子，必要时旋转
 *
 * 注意：删除可能需要 O(log n) 次旋转（沿路径向上传播）。
 * 因为删除后子树高度可能减少，即使旋转后高度仍可能比原来低。
 * ============================================================ */

/* 找最小节点 */
static ds_avl_node_t *avl_find_min_node(ds_avl_node_t *node)
{
    while (node && node->left) {
        node = node->left;
    }
    return node;
}

ds_avl_node_t *ds_avl_delete(ds_avl_node_t *root, const void *key,
                              ds_avl_compare_fn compare, void (*free_data)(void *))
{
    int cmp;

    if (!root || !compare) {
        return NULL;
    }

    cmp = compare(key, root->data);
    if (cmp < 0) {
        root->left = ds_avl_delete(root->left, key, compare, free_data);
    } else if (cmp > 0) {
        root->right = ds_avl_delete(root->right, key, compare, free_data);
    } else {
        /* 找到要删除的节点 */
        ds_avl_node_t *to_delete = root;

        /* 情况1+2：0 或 1 个子节点 */
        if (!root->left || !root->right) {
            root = root->left ? root->left : root->right;

            if (free_data && to_delete->data) {
                free_data(to_delete->data);
            }
            free(to_delete->data);
            free(to_delete);
        } else {
            /* 情况3：两个子节点 —— 找后继替换 */
            ds_avl_node_t *successor = avl_find_min_node(root->right);

            /* 简单方式：交换数据，然后删除后继 */
            void *tmp_data = root->data;
            root->data = successor->data;
            successor->data = tmp_data;

            root->right = ds_avl_delete(root->right, key, compare, free_data);
        }
    }

    return avl_balance(root);
}

const ds_avl_node_t *ds_avl_search(const ds_avl_node_t *root, const void *key,
                                    ds_avl_compare_fn compare)
{
    int cmp;

    if (!root || !compare) {
        return NULL;
    }

    cmp = compare(key, root->data);
    if (cmp == 0) {
        return root;
    } else if (cmp < 0) {
        return ds_avl_search(root->left, key, compare);
    } else {
        return ds_avl_search(root->right, key, compare);
    }
}

/* 验证 AVL：中序遍历 + BF 检查 */
static bool avl_is_valid_impl(const ds_avl_node_t *root, ds_avl_compare_fn compare,
                               const void **prev)
{
    if (!root) {
        return true;
    }

    /* 检查平衡因子 */
    int bf = balance_factor(root);
    if (bf < -1 || bf > 1) {
        return false;
    }

    /* 检查高度一致性 */
    int expected_h = 1 + (height(root->left) > height(root->right)
                          ? height(root->left) : height(root->right));
    if (root->height != expected_h) {
        return false;
    }

    /* 中序遍历检查有序性 */
    if (!avl_is_valid_impl(root->left, compare, prev)) {
        return false;
    }
    if (*prev && compare(root->data, *prev) <= 0) {
        return false;
    }
    *prev = root->data;
    return avl_is_valid_impl(root->right, compare, prev);
}

bool ds_avl_is_valid(const ds_avl_node_t *root, ds_avl_compare_fn compare)
{
    const void *prev = NULL;
    return avl_is_valid_impl(root, compare, &prev);
}

void ds_avl_destroy(ds_avl_node_t *root, void (*free_data)(void *))
{
    if (!root) return;
    ds_avl_destroy(root->left, free_data);
    ds_avl_destroy(root->right, free_data);
    if (free_data && root->data) free_data(root->data);
    free(root->data);
    free(root);
}

size_t ds_avl_height(const ds_avl_node_t *root)
{
    return (size_t)height(root);
}

size_t ds_avl_size(const ds_avl_node_t *root)
{
    if (!root) return 0u;
    return 1u + ds_avl_size(root->left) + ds_avl_size(root->right);
}

/* ============================================================
 * 演示函数
 * ============================================================ */

static int avl_int_compare(const void *a, const void *b)
{
    return *(const int *)a - *(const int *)b;
}

static void avl_print_inorder(const ds_avl_node_t *root)
{
    if (!root) return;
    avl_print_inorder(root->left);
    printf("%d ", *(const int *)root->data);
    avl_print_inorder(root->right);
}

void ds_avl_tree_demo(void)
{
    printf("========== AVL 树演示 ==========\n");

    ds_avl_node_t *root = NULL;

    /* 有序插入（会触发多次旋转） */
    printf("\n【有序插入 1..7——触发旋转保持平衡】\n");
    printf("插入顺序: 1, 2, 3, 4, 5, 6, 7\n");
    for (int i = 1; i <= 7; ++i) {
        root = ds_avl_insert(root, &i, sizeof(int), avl_int_compare);
        printf("  插入 %d 后: 高度=%zu, 大小=%zu\n",
               i, ds_avl_height(root), ds_avl_size(root));
    }

    printf("中序遍历: ");
    avl_print_inorder(root);
    printf("\n");

    printf("树的高度: %zu（BST 退化时为 7，AVL 保持 O(log n)）\n",
           ds_avl_height(root));
    printf("是否为合法 AVL: %s\n", ds_avl_is_valid(root, avl_int_compare) ? "是" : "否");

    /* 查找演示 */
    printf("\n【查找】\n");
    for (int key = 1; key <= 8; ++key) {
        const ds_avl_node_t *found = ds_avl_search(root, &key, avl_int_compare);
        printf("  查找 %d: %s\n", key, found ? "找到" : "未找到");
    }

    /* 删除演示 */
    printf("\n【删除 4（两个子节点场景）】\n");
    int del_key = 4;
    root = ds_avl_delete(root, &del_key, avl_int_compare, NULL);
    printf("删除后中序遍历: ");
    avl_print_inorder(root);
    printf("\n高度: %zu, 合法: %s\n", ds_avl_height(root),
           ds_avl_is_valid(root, avl_int_compare) ? "是" : "否");

    ds_avl_destroy(root, NULL);
    printf("========== AVL 树演示结束 ==========\n");
}
