/**
 * @file main.c
 * @brief 树与图算法练习 - BFS/DFS 遍历、二叉树技巧
 *
 * 包含：
 *   1. 二叉树层序遍历（BFS）
 *   2. 对称树判断（递归）
 *   3. 路径总和（从根到叶）
 *
 * @author Claude
 * @date 2026-07-12
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/*==============================
 * 二叉树节点定义
 *=============================*/

typedef struct TreeNode {
    int val;
    struct TreeNode *left;
    struct TreeNode *right;
} TreeNode;

/**
 * @brief 创建二叉树节点
 */
TreeNode* create_node(int val) {
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
 * @brief 释放二叉树所有节点
 */
void free_tree(TreeNode *root) {
    if (!root) return;
    free_tree(root->left);
    free_tree(root->right);
    free(root);
}

/*==============================
 * 1. 二叉树层序遍历（BFS）
 *=============================*/

/**
 * @brief 二叉树层序遍历 - 使用队列
 *
 * 思路：
 *   - 从根节点开始，将其入队
 *   - 当队列非空时，取出队首节点访问
 *   - 将其左右子节点（如果有）入队
 *   - 重复直到队列为空
 *
 * 时间复杂度: O(n)，空间复杂度: O(w)，w 为最大宽度
 */
void level_order_traversal(TreeNode *root) {
    if (!root) return;

    // 使用数组模拟队列
    TreeNode **queue = (TreeNode **)malloc(sizeof(TreeNode *) * 1000);
    int front = 0, rear = 0;

    queue[rear++] = root;

    printf("层序遍历结果: ");
    while (front < rear) {
        TreeNode *node = queue[front++];
        printf("%d ", node->val);

        if (node->left) queue[rear++] = node->left;
        if (node->right) queue[rear++] = node->right;
    }
    printf("\n");

    free(queue);
}

/*==============================
 * 2. 对称树判断（递归）
 *=============================*/

/**
 * @brief 判断两棵树是否互为镜像
 */
static bool is_mirror(TreeNode *left, TreeNode *right) {
    if (!left && !right) return true;      // 两树都空，镜像
    if (!left || !right) return false;     // 一树空一树非空，非镜像
    if (left->val != right->val) return false; // 根值不同，非镜像

    // 递归判断：左树的左子树与右树的右子树镜像，左树的右子树与右树的左子树镜像
    return is_mirror(left->left, right->right) &&
           is_mirror(left->right, right->left);
}

/**
 * @brief 判断二叉树是否对称
 *
 * 思路：
 *   - 空树是对称的
 *   - 非空树需要判断左右子树是否互为镜像
 *
 * 时间复杂度: O(n)，空间复杂度: O(h)，h 为树高
 */
bool is_symmetric(TreeNode *root) {
    if (!root) return true;
    return is_mirror(root->left, root->right);
}

/*==============================
 * 3. 路径总和（从根到叶）
 *=============================*/

/**
 * @brief 递归判断是否存在从根到叶的路径，其和等于目标值
 *
 * 思路：
 *   - 从根节点开始，将目标值减去当前节点值
 *   - 如果当前节点是叶子节点，检查剩余值是否为 0
 *   - 如果不是叶子节点，递归检查左右子树
 *
 * 时间复杂度: O(n)，空间复杂度: O(h)
 */
bool has_path_sum(TreeNode *root, int target_sum) {
    if (!root) return false;

    // 减去当前节点值
    target_sum -= root->val;

    // 叶子节点：检查是否等于目标值
    if (!root->left && !root->right) {
        return target_sum == 0;
    }

    // 非叶子节点：递归检查子树
    return has_path_sum(root->left, target_sum) ||
           has_path_sum(root->right, target_sum);
}

/*==============================
 * 测试入口
 *=============================*/

int main(void) {
    printf("========== 树与图算法练习 ==========\n\n");

    /* 构建测试树:
           1
          / \
         2   2
        / \ / \
       3  4 4  3   (对称树)
    */
    TreeNode *symmetric = create_node(1);
    symmetric->left = create_node(2);
    symmetric->right = create_node(2);
    symmetric->left->left = create_node(3);
    symmetric->left->right = create_node(4);
    symmetric->right->left = create_node(4);
    symmetric->right->right = create_node(3);

    printf("【测试1】对称树判断\n");
    printf("  树结构: 完全对称的二叉树\n");
    printf("  结果: %s\n\n", is_symmetric(symmetric) ? "对称" : "非对称");

    /* 构建测试树（用于层序遍历和路径总和）:
           5
          / \
         4   8
        /   / \
       11  13  4
      /  \      \
     7    2      1
    */
    TreeNode *root = create_node(5);
    root->left = create_node(4);
    root->right = create_node(8);
    root->left->left = create_node(11);
    root->right->left = create_node(13);
    root->right->right = create_node(4);
    root->left->left->left = create_node(7);
    root->left->left->right = create_node(2);
    root->right->right->right = create_node(1);

    printf("【测试2】层序遍历（BFS）\n");
    level_order_traversal(root);
    printf("\n");

    printf("【测试3】路径总和\n");
    printf("  目标路径和 22 (5->4->11->2): %s\n",
           has_path_sum(root, 22) ? "存在" : "不存在");
    printf("  目标路径和 20 (5->4->11->7): %s\n",
           has_path_sum(root, 20) ? "存在" : "不存在");
    printf("  目标路径和 26 (5->8->4->1): %s\n",
           has_path_sum(root, 26) ? "存在" : "不存在");

    /* 释放内存 */
    free_tree(symmetric);
    free_tree(root);

    printf("\n========== 测试完成 ==========\n");
    return 0;
}
