/**
 * @file main.c
 * @brief AVL 树自平衡演示
 *
 * 演示内容：
 * 1. AVL 树插入 - 保持平衡
 * 2. 四种旋转操作 - LL/RR/LR/RL
 * 3. 平衡因子维护 - 左高-右高
 * 4. 高度维护 - 每次插入后更新高度
 * 5. 与 BST 对比 - 验证 AVL 的平衡优势
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/* ============================================================
 * 数据结构定义
 * ============================================================ */

typedef struct AVLNode {
    int key;                    // 节点键值
    int height;                 // 当前节点高度
    struct AVLNode *left;       // 左子树
    struct AVLNode *right;      // 右子树
} AVLNode;

/* ============================================================
 * 工具函数
 * ============================================================ */

/**
 * 创建新节点
 */
static AVLNode *create_node(int key) {
    AVLNode *node = (AVLNode *)malloc(sizeof(AVLNode));
    if (!node) {
        fprintf(stderr, "内存分配失败\n");
        exit(1);
    }
    node->key = key;
    node->height = 1;           // 新节点高度为 1（叶子节点）
    node->left = NULL;
    node->right = NULL;
    return node;
}

/**
 * 获取节点高度（空节点高度为 0）
 */
static int get_height(AVLNode *node) {
    return node ? node->height : 0;
}

/**
 * 计算平衡因子（左子树高度 - 右子树高度）
 * - 平衡因子 = 0: 左右子树等高
 * - 平衡因子 = 1: 左子树高 1
 * - 平衡因子 = -1: 右子树高 1
 * - |平衡因子| > 1: 失衡，需要旋转
 */
static int get_balance(AVLNode *node) {
    return node ? get_height(node->left) - get_height(node->right) : 0;
}

/**
 * 更新节点高度
 * 高度 = 1 + max(左子树高度, 右子树高度)
 */
static void update_height(AVLNode *node) {
    if (node) {
        int lh = get_height(node->left);
        int rh = get_height(node->right);
        node->height = 1 + (lh > rh ? lh : rh);
    }
}

/**
 * 释放整棵树
 */
static void free_tree(AVLNode *root) {
    if (!root) return;
    free_tree(root->left);
    free_tree(root->right);
    free(root);
}

/* ============================================================
 * 旋转操作（四种情况）
 * ============================================================ */

/**
 * LL 旋转（右旋）
 *
 *  失衡情况：          旋转后：
 *     z                  y
 *    / \               /   \
 *   y   T4    -->     x     z
 *  / \               / \   / \
 * x   T3           T1 T2 T3 T4
 */
static AVLNode *rotate_right(AVLNode *z) {
    AVLNode *y = z->left;
    AVLNode *T3 = y->right;

    // 执行旋转
    y->right = z;
    z->left = T3;

    // 更新高度（先更新 z，再更新 y，因为 y 在上层）
    update_height(z);
    update_height(y);

    return y;  // 返回新的根节点
}

/**
 * RR 旋转（左旋）
 *
 *  失衡情况：          旋转后：
 *    z                  y
 *   / \               /   \
 *  T1  y     -->     z     x
 *     / \           / \   / \
 *    T2  x         T1 T2 T3 T4
 */
static AVLNode *rotate_left(AVLNode *z) {
    AVLNode *y = z->right;
    AVLNode *T2 = y->left;

    // 执行旋转
    y->left = z;
    z->right = T2;

    // 更新高度
    update_height(z);
    update_height(y);

    return y;
}

/**
 * LR 旋转（左-右双旋）
 *
 *  失衡情况：            第一次旋转后           第二次旋转后：
 *     z                    z                       y
 *    / \                  / \                    /   \
 *   y   T4      -->      y   T4        -->      x     z
 *  / \                  / \                    / \   / \
 * T1  x                x   T3                 T1 T2 T3 T4
 *    / \              / \
 *   T2  T3           T2  T3
 */
static AVLNode *rotate_lr(AVLNode *z) {
    z->left = rotate_left(z->left);  // 先对左子树左旋
    return rotate_right(z);          // 再整体右旋
}

/**
 * RL 旋转（右-左双旋）
 *
 *  失衡情况：            第一次旋转后           第二次旋转后：
 *    z                    z                       y
 *   / \                  / \                    /   \
 *  T1  y        -->     T1  y          -->     z     x
 *     / \                   / \                / \   / \
 *    x   T4                T2  x              T1 T2 T3 T4
 *   / \                       / \
 *  T2  T3                    T3  T4
 */
static AVLNode *rotate_rl(AVLNode *z) {
    z->right = rotate_right(z->right);  // 先对右子树右旋
    return rotate_left(z);              // 再整体左旋
}

/* ============================================================
 * AVL 树操作
 * ============================================================ */

/**
 * 插入节点并保持平衡
 *
 * 插入后从插入点向根回溯，检查并修复失衡：
 * - 平衡因子 > 1 且新节点在左子树：LL 或 LR
 * - 平衡因子 < -1 且新节点在右子树：RR 或 RL
 */
static AVLNode *insert(AVLNode *node, int key) {
    // 1. 标准 BST 插入
    if (!node) {
        return create_node(key);
    }

    if (key < node->key) {
        node->left = insert(node->left, key);
    } else if (key > node->key) {
        node->right = insert(node->right, key);
    } else {
        // 重复键不插入
        return node;
    }

    // 2. 更新当前节点高度
    update_height(node);

    // 3. 获取平衡因子，检查是否失衡
    int balance = get_balance(node);

    // 4. 根据失衡类型选择旋转
    // LL: 左子树的左孩子
    if (balance > 1 && key < node->left->key) {
        printf("  [LL 旋转] 节点 %d 右旋\n", node->key);
        return rotate_right(node);
    }
    // RR: 右子树的右孩子
    if (balance < -1 && key > node->right->key) {
        printf("  [RR 旋转] 节点 %d 左旋\n", node->key);
        return rotate_left(node);
    }
    // LR: 左子树的右孩子
    if (balance > 1 && key > node->left->key) {
        printf("  [LR 旋转] 节点 %d 先左旋后右旋\n", node->key);
        return rotate_lr(node);
    }
    // RL: 右子树的左孩子
    if (balance < -1 && key < node->right->key) {
        printf("  [RL 旋转] 节点 %d 先右旋后左旋\n", node->key);
        return rotate_rl(node);
    }

    return node;
}

/* ============================================================
 * 遍历与检查
 * ============================================================ */

/**
 * 中序遍历（验证 BST 有序性）
 */
static void inorder(AVLNode *root) {
    if (!root) return;
    inorder(root->left);
    printf("%d ", root->key);
    inorder(root->right);
}

/**
 * 先序遍历（显示树结构）
 */
static void preorder(AVLNode *root) {
    if (!root) return;
    int balance = get_balance(root);
    printf("%d(bf=%d,h=%d) ", root->key, balance, root->height);
    preorder(root->left);
    preorder(root->right);
}

/**
 * 打印树的可视化结构
 */
static void print_tree(AVLNode *root, int level, char prefix) {
    if (!root) return;
    for (int i = 0; i < level; i++) printf("  ");
    printf("%c-%d (bf=%d)\n", prefix, root->key, get_balance(root));
    print_tree(root->left, level + 1, 'L');
    print_tree(root->right, level + 1, 'R');
}

/* ============================================================
 * 主函数：演示 AVL 树操作
 * ============================================================ */

int main(void) {
    printf("=== AVL 树自平衡演示 ===\n\n");

    AVLNode *root = NULL;

    /* 1. 演示 LL 旋转 */
    printf("【1】LL 旋转演示（依次插入: 30, 20, 10）\n");
    root = NULL;
    int ll_keys[] = {30, 20, 10};
    for (size_t i = 0; i < 3; i++) {
        printf("  插入 %d:\n", ll_keys[i]);
        root = insert(root, ll_keys[i]);
    }
    printf("  最终树结构:\n");
    print_tree(root, 0, 'R');
    printf("  中序遍历: ");
    inorder(root);
    printf("\n\n");
    free_tree(root);

    /* 2. 演示 RR 旋转 */
    printf("【2】RR 旋转演示（依次插入: 10, 20, 30）\n");
    root = NULL;
    int rr_keys[] = {10, 20, 30};
    for (size_t i = 0; i < 3; i++) {
        printf("  插入 %d:\n", rr_keys[i]);
        root = insert(root, rr_keys[i]);
    }
    printf("  最终树结构:\n");
    print_tree(root, 0, 'R');
    printf("  中序遍历: ");
    inorder(root);
    printf("\n\n");
    free_tree(root);

    /* 3. 演示 LR 旋转 */
    printf("【3】LR 旋转演示（依次插入: 30, 10, 20）\n");
    root = NULL;
    int lr_keys[] = {30, 10, 20};
    for (size_t i = 0; i < 3; i++) {
        printf("  插入 %d:\n", lr_keys[i]);
        root = insert(root, lr_keys[i]);
    }
    printf("  最终树结构:\n");
    print_tree(root, 0, 'R');
    printf("  中序遍历: ");
    inorder(root);
    printf("\n\n");
    free_tree(root);

    /* 4. 演示 RL 旋转 */
    printf("【4】RL 旋转演示（依次插入: 10, 30, 20）\n");
    root = NULL;
    int rl_keys[] = {10, 30, 20};
    for (size_t i = 0; i < 3; i++) {
        printf("  插入 %d:\n", rl_keys[i]);
        root = insert(root, rl_keys[i]);
    }
    printf("  最终树结构:\n");
    print_tree(root, 0, 'R');
    printf("  中序遍历: ");
    inorder(root);
    printf("\n\n");
    free_tree(root);

    /* 5. 综合演示：插入多个节点 */
    printf("【5】综合演示（插入序列: 10, 20, 30, 40, 50, 25）\n");
    root = NULL;
    int seq[] = {10, 20, 30, 40, 50, 25};
    for (size_t i = 0; i < 6; i++) {
        printf("  插入 %d:\n", seq[i]);
        root = insert(root, seq[i]);
    }
    printf("  最终树结构:\n");
    print_tree(root, 0, 'R');
    printf("  中序遍历: ");
    inorder(root);
    printf("\n  先序遍历: ");
    preorder(root);
    printf("\n\n");

    /* 6. 与 BST 对比 */
    printf("【6】AVL vs BST 高度对比\n");
    printf("  相同数据 [10, 20, 30, 40, 50, 25]:\n");

    // 模拟 BST 插入（不做平衡）
    // BST 最坏情况：按顺序插入时退化成链表
    int bst_height_seq[] = {10, 20, 30, 40, 50, 25};
    size_t bst_count = sizeof(bst_height_seq) / sizeof(bst_height_seq[0]);
    printf("  BST 最坏情况高度: %zu（退化成链表）\n", bst_count);
    printf("  AVL 树高度: %d（平衡）\n", get_height(root));

    free_tree(root);
    printf("\n演示完成。\n");

    return 0;
}
