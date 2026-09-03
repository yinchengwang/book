/*
 * binarytree.c
 *
 * 二叉树基础演示：构建、前序/中序/后序遍历、节点计数。
 */

#include <stdio.h>
#include <stdlib.h>

/*------------------------------ 数据结构 ------------------------------*/

/* 二叉树节点定义 */
typedef struct TreeNode {
    int val;                     /* 节点值 */
    struct TreeNode *left;        /* 左子树 */
    struct TreeNode *right;       /* 右子树 */
} TreeNode;

/*------------------------------ 工具函数 ------------------------------*/

/**
 * 创建新节点
 * @param val 节点值
 * @return 新节点指针
 */
TreeNode *create_node(int val)
{
    TreeNode *node = (TreeNode *)malloc(sizeof(TreeNode));
    if (!node) {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }
    node->val = val;
    node->left = NULL;
    node->right = NULL;
    return node;
}

/**
 * 释放整棵树（后序遍历释放）
 * @param root 根节点
 */
void free_tree(TreeNode *root)
{
    if (!root) return;
    free_tree(root->left);
    free_tree(root->right);
    free(root);
}

/*------------------------------ 遍历操作 ------------------------------*/

/**
 * 前序遍历：根 -> 左 -> 右
 * @param root 根节点
 * @param visited 访问回调（NULL 则打印到 stdout）
 */
void preorder_traverse(TreeNode *root, void (*visited)(int))
{
    if (!root) return;
    if (visited) {
        visited(root->val);
    } else {
        printf("%d ", root->val);
    }
    preorder_traverse(root->left, visited);
    preorder_traverse(root->right, visited);
}

/**
 * 中序遍历：左 -> 根 -> 右
 * @param root 根节点
 * @param visited 访问回调
 */
void inorder_traverse(TreeNode *root, void (*visited)(int))
{
    if (!root) return;
    inorder_traverse(root->left, visited);
    if (visited) {
        visited(root->val);
    } else {
        printf("%d ", root->val);
    }
    inorder_traverse(root->right, visited);
}

/**
 * 后序遍历：左 -> 右 -> 根
 * @param root 根节点
 * @param visited 访问回调
 */
void postorder_traverse(TreeNode *root, void (*visited)(int))
{
    if (!root) return;
    postorder_traverse(root->left, visited);
    postorder_traverse(root->right, visited);
    if (visited) {
        visited(root->val);
    } else {
        printf("%d ", root->val);
    }
}

/*------------------------------ 统计操作 ------------------------------*/

/**
 * 统计节点总数
 * @param root 根节点
 * @return 节点数量
 */
int count_nodes(TreeNode *root)
{
    if (!root) return 0;
    return 1 + count_nodes(root->left) + count_nodes(root->right);
}

/**
 * 统计叶子节点数
 * @param root 根节点
 * @return 叶子节点数量
 */
int count_leaves(TreeNode *root)
{
    if (!root) return 0;
    if (!root->left && !root->right) return 1;
    return count_leaves(root->left) + count_leaves(root->right);
}

/**
 * 计算树的高度
 * @param root 根节点
 * @return 树的高度（空树为 0）
 */
int tree_height(TreeNode *root)
{
    if (!root) return 0;
    int left_h = tree_height(root->left);
    int right_h = tree_height(root->right);
    return 1 + (left_h > right_h ? left_h : right_h);
}

/*------------------------------ 测试演示 ------------------------------*/

/* 打印节点值 */
void print_val(int val)
{
    printf("%d ", val);
}

int main(void)
{
    /* 构建示例树：
     *        1
     *       / \
     *      2   3
     *     / \   \
     *    4   5   6
     */
    TreeNode *root = create_node(1);
    root->left = create_node(2);
    root->right = create_node(3);
    root->left->left = create_node(4);
    root->left->right = create_node(5);
    root->right->right = create_node(6);

    printf("===== 二叉树遍历演示 =====\n\n");

    printf("前序遍历 (Preorder): ");
    preorder_traverse(root, NULL);
    printf("\n");

    printf("中序遍历 (Inorder):  ");
    inorder_traverse(root, NULL);
    printf("\n");

    printf("后序遍历 (Postorder): ");
    postorder_traverse(root, NULL);
    printf("\n");

    printf("\n===== 统计信息 =====\n");
    printf("节点总数:   %d\n", count_nodes(root));
    printf("叶子节点:   %d\n", count_leaves(root));
    printf("树的高度:   %d\n", tree_height(root));

    /* 释放内存 */
    free_tree(root);

    return 0;
}
