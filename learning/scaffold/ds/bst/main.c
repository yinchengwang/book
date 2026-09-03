/**
 * @file main.c
 * @brief 二叉搜索树（BST）基本操作演示
 *
 * 演示内容：
 * 1. 插入 - 按 BST 规则插入节点
 * 2. 查找 - 递归/迭代查找
 * 3. 删除 - 处理三种情况（叶子/单子/双子）
 * 4. 平衡因子检查 - 检测 AVL 失衡点
 * 5. 中序遍历 - 验证 BST 有序性
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/* ============================================================
 * 数据结构定义
 * ============================================================ */

typedef struct BSTNode {
    int key;                    // 节点键值
    struct BSTNode *left;        // 左子树
    struct BSTNode *right;      // 右子树
} BSTNode;

/* ============================================================
 * 工具函数
 * ============================================================ */

/**
 * 创建新节点
 */
static BSTNode *create_node(int key) {
    BSTNode *node = (BSTNode *)malloc(sizeof(BSTNode));
    if (!node) {
        fprintf(stderr, "内存分配失败\n");
        exit(1);
    }
    node->key = key;
    node->left = NULL;
    node->right = NULL;
    return node;
}

/**
 * 释放整棵树
 */
static void free_tree(BSTNode *root) {
    if (!root) return;
    free_tree(root->left);
    free_tree(root->right);
    free(root);
}

/**
 * 计算树高度
 */
static int height(BSTNode *node) {
    if (!node) return 0;
    int lh = height(node->left);
    int rh = height(node->right);
    return 1 + (lh > rh ? lh : rh);
}

/**
 * 计算平衡因子（左高-右高）
 */
static int balance_factor(BSTNode *node) {
    if (!node) return 0;
    return height(node->left) - height(node->right);
}

/* ============================================================
 * BST 基本操作
 * ============================================================ */

/**
 * 插入节点
 * - 值小于当前节点，往左走
 * - 值大于当前节点，往右走
 * - 值相同，不插入（去重）
 */
static BSTNode *insert(BSTNode *root, int key) {
    if (!root) {
        return create_node(key);
    }

    if (key < root->key) {
        root->left = insert(root->left, key);
    } else if (key > root->key) {
        root->right = insert(root->right, key);
    }
    /* key == root->key 时不做任何操作 */

    return root;
}

/**
 * 查找节点（迭代版本）
 * - 值小，往左找
 * - 值大，往右找
 * - 找到或走到空，返回
 */
static BSTNode *find(BSTNode *root, int key) {
    while (root && key != root->key) {
        root = (key < root->key) ? root->left : root->right;
    }
    return root;
}

/**
 * 查找最小节点（用于删除的后继）
 */
static BSTNode *find_min(BSTNode *node) {
    while (node && node->left) {
        node = node->left;
    }
    return node;
}

/**
 * 删除节点
 *
 * 三种情况：
 * 1. 叶子节点：直接删除
 * 2. 单子树：子树上移
 * 3. 双子树：找后继（左子树最大或右子树最小），替换后删除后继
 */
static BSTNode *delete_node(BSTNode *root, int key) {
    if (!root) return NULL;

    if (key < root->key) {
        root->left = delete_node(root->left, key);
    } else if (key > root->key) {
        root->right = delete_node(root->right, key);
    } else {
        /* 找到要删除的节点 */
        if (!root->left) {
            BSTNode *temp = root->right;
            free(root);
            return temp;
        } else if (!root->right) {
            BSTNode *temp = root->left;
            free(root);
            return temp;
        } else {
            /* 双子树：找后继（右子树最小） */
            BSTNode *successor = find_min(root->right);
            root->key = successor->key;
            root->right = delete_node(root->right, successor->key);
        }
    }

    return root;
}

/* ============================================================
 * 遍历与检查
 * ============================================================ */

/**
 * 中序遍历（验证 BST 有序性）
 */
static void inorder(BSTNode *root) {
    if (!root) return;
    inorder(root->left);
    printf("%d ", root->key);
    inorder(root->right);
}

/**
 * 打印平衡因子（检查是否需要 AVL 旋转）
 */
static void check_balance(BSTNode *root) {
    if (!root) return;

    int bf = balance_factor(root);
    printf("节点 %d: 平衡因子 = %d", root->key, bf);

    if (bf > 1 || bf < -1) {
        printf(" [失衡!]\n");
    } else {
        printf(" [平衡]\n");
    }

    check_balance(root->left);
    check_balance(root->right);
}

/* ============================================================
 * 主函数：演示 BST 操作
 * ============================================================ */

int main(void) {
    printf("=== 二叉搜索树（BST）操作演示 ===\n\n");

    BSTNode *root = NULL;

    /* 1. 插入演示 */
    printf("【1】插入节点: 50, 30, 70, 20, 40, 60, 80\n");
    int keys[] = {50, 30, 70, 20, 40, 60, 80};
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        root = insert(root, keys[i]);
    }

    /* 2. 中序遍历（验证 BST 有序） */
    printf("\n【2】中序遍历（验证有序）:\n  ");
    inorder(root);
    printf("\n");

    /* 3. 平衡因子检查 */
    printf("\n【3】平衡因子检查:\n");
    check_balance(root);

    /* 4. 查找演示 */
    printf("\n【4】查找演示:\n");
    int search_keys[] = {40, 55, 80};
    for (size_t i = 0; i < sizeof(search_keys) / sizeof(search_keys[0]); i++) {
        BSTNode *found = find(root, search_keys[i]);
        printf("  查找 %d: %s\n", search_keys[i],
               found ? "找到" : "未找到");
    }

    /* 5. 删除演示 */
    printf("\n【5】删除演示:\n");

    /* 删除叶子节点 20 */
    printf("  删除 20（叶子节点）: ");
    root = delete_node(root, 20);
    inorder(root);
    printf("\n");

    /* 删除单子树节点 30 */
    printf("  删除 30（单子树）: ");
    root = delete_node(root, 30);
    inorder(root);
    printf("\n");

    /* 删除双子树节点 50（根节点） */
    printf("  删除 50（双子树/根）: ");
    root = delete_node(root, 50);
    inorder(root);
    printf("\n");

    /* 6. 再次检查平衡 */
    printf("\n【6】删除后平衡因子:\n");
    check_balance(root);

    /* 清理 */
    free_tree(root);
    printf("\n演示完成。\n");

    return 0;
}
