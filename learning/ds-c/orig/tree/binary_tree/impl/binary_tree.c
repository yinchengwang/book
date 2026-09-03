/*
 * binary_tree.c —— 二叉树实现
 *
 * ============================================================
 * 数据结构内部定义
 * ============================================================
 *
 * 二叉树节点结构（已在头文件中暴露）：
 *
 *   ds_bitree_node_t
 *   ┌─────────────────────┐
 *   │ data  ──────────> 用户数据区                  │
 *   │ left  ──────────> 左子节点指针（可为 NULL）     │
 *   │ right ──────────> 右子节点指针（可为 NULL）     │
 *   └─────────────────────┘
 *
 * 示例二叉树：
 *
 *           1          ← 根节点（root）
 *         /   \
 *        2     3       ← 内部节点
 *       / \   /
 *      4   5 6         ← 叶子节点
 *
 * ============================================================
 * 遍历算法详解
 * ============================================================
 *
 * 【前序遍历（Pre-order）：根 → 左 → 右】
 *
 *   递归公式：
 *     preorder(node):
 *       if node == NULL: return
 *       visit(node)                ← 先访问根
 *       preorder(node->left)       ← 再遍历左子树
 *       preorder(node->right)      ← 最后遍历右子树
 *
 *   示例树上：1 → 2 → 4 → 5 → 3 → 6
 *
 *   应用场景：复制树结构（先创建根，再递归创建左右子树）
 *
 * 【中序遍历（In-order）：左 → 根 → 右】
 *
 *   递归公式：
 *     inorder(node):
 *       if node == NULL: return
 *       inorder(node->left)        ← 先遍历左子树
 *       visit(node)                ← 再访问根
 *       inorder(node->right)       ← 最后遍历右子树
 *
 *   示例树上：4 → 2 → 5 → 1 → 6 → 3
 *
 *   重点：对 BST 进行中序遍历，结果是升序序列！
 *
 * 【后序遍历（Post-order）：左 → 右 → 根】
 *
 *   递归公式：
 *     postorder(node):
 *       if node == NULL: return
 *       postorder(node->left)      ← 先遍历左子树
 *       postorder(node->right)     ← 再遍历右子树
 *       visit(node)                ← 最后访问根
 *
 *   示例树上：4 → 5 → 2 → 6 → 3 → 1
 *
 *   应用场景：计算树的高度（先知道左右子树高度才能算当前节点高度）、
 *            释放树的内存（先释放左右子树才能释放根）
 *
 * 【层序遍历（Level-order）：逐层、每层从左到右】
 *
 *   算法（BFS + 队列）：
 *     queue.push(root)
 *     while !queue.empty():
 *       node = queue.pop()
 *       visit(node)
 *       if node->left:  queue.push(node->left)
 *       if node->right: queue.push(node->right)
 *
 *   示例树上：1 → 2 → 3 → 4 → 5 → 6
 *
 * ============================================================
 * 迭代实现（非递归）
 * ============================================================
 *
 * 前序遍历的迭代版本（用栈模拟递归）：
 *
 *   stack.push(root)
 *   while !stack.empty():
 *     node = stack.pop()
 *     visit(node)
 *     // 注意：先压右子再压左子，保证左子先出栈
 *     if node->right: stack.push(node->right)
 *     if node->left:  stack.push(node->left)
 *
 * 中序遍历的迭代版本（一路向左 + 栈回溯）：
 *
 *   curr = root
 *   while curr != NULL || !stack.empty():
 *     // 一路向左走到尽头
 *     while curr != NULL:
 *       stack.push(curr)
 *       curr = curr->left
 *     // 回溯：弹出并访问，然后转向右子树
 *     curr = stack.pop()
 *     visit(curr)
 *     curr = curr->right
 *
 * 后序遍历的迭代版本（最复杂，需要额外标记）：
 *   常用方法：双栈法 或 记录上次访问节点法。
 */

#include <ds/binary_tree.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * 为简单起见，本实现将数据直接存储在节点中（固定大小）。
 * data 指向 malloc 的空间。
 */

ds_bitree_node_t *ds_bitree_node_create(const void *data, size_t element_size)
{
    ds_bitree_node_t *node;

    if (element_size == 0u) {
        return NULL;
    }

    node = (ds_bitree_node_t *)calloc(1u, sizeof(ds_bitree_node_t));
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

void ds_bitree_node_destroy(ds_bitree_node_t *node, void (*free_data)(void *))
{
    if (!node) {
        return;
    }

    /* 后序遍历顺序释放：先释放左右子树，再释放根 */
    ds_bitree_node_destroy(node->left, free_data);
    ds_bitree_node_destroy(node->right, free_data);

    if (free_data && node->data) {
        free_data(node->data);
    }
    free(node->data);
    free(node);
}

/* ============================================================
 * 遍历实现
 * ============================================================ */

/* 递归前序 */
void ds_bitree_preorder(const ds_bitree_node_t *root, ds_bitree_visit_fn visit, void *user_data)
{
    if (!root || !visit) {
        return;
    }

    visit(root, 0, user_data);
    ds_bitree_preorder(root->left, visit, user_data);
    ds_bitree_preorder(root->right, visit, user_data);
}

/* 递归中序 */
void ds_bitree_inorder(const ds_bitree_node_t *root, ds_bitree_visit_fn visit, void *user_data)
{
    if (!root || !visit) {
        return;
    }

    ds_bitree_inorder(root->left, visit, user_data);
    visit(root, 0, user_data);
    ds_bitree_inorder(root->right, visit, user_data);
}

/* 递归后序 */
void ds_bitree_postorder(const ds_bitree_node_t *root, ds_bitree_visit_fn visit, void *user_data)
{
    if (!root || !visit) {
        return;
    }

    ds_bitree_postorder(root->left, visit, user_data);
    ds_bitree_postorder(root->right, visit, user_data);
    visit(root, 0, user_data);
}

/*
 * 层序遍历（迭代 + 简单数组队列模拟）
 *
 * 使用固定大小的数组作为队列（为演示简洁，假设树节点数不超过 1000）
 */
#define LEVELORDER_MAX_NODES 1000u

void ds_bitree_levelorder(const ds_bitree_node_t *root, ds_bitree_visit_fn visit, void *user_data)
{
    const ds_bitree_node_t *queue[LEVELORDER_MAX_NODES];
    size_t                   front;
    size_t                   rear;
    int                      level;

    if (!root || !visit) {
        return;
    }

    front = 0u;
    rear = 0u;
    queue[rear++] = root;
    level = 0;

    while (front < rear) {
        size_t level_size = rear - front;

        /* 处理当前层的所有节点 */
        for (size_t i = 0u; i < level_size; ++i) {
            const ds_bitree_node_t *node = queue[front++];

            visit(node, level, user_data);

            if (node->left && rear < LEVELORDER_MAX_NODES) {
                queue[rear++] = node->left;
            }
            if (node->right && rear < LEVELORDER_MAX_NODES) {
                queue[rear++] = node->right;
            }
        }
        ++level;
    }
}

/* ============================================================
 * 查询函数
 * ============================================================ */

/* 最大深度递归计算 */
static size_t max_size_t(size_t a, size_t b)
{
    return a > b ? a : b;
}

size_t ds_bitree_height(const ds_bitree_node_t *root)
{
    if (!root) {
        return 0u;
    }

    return 1u + max_size_t(ds_bitree_height(root->left),
                           ds_bitree_height(root->right));
}

size_t ds_bitree_size(const ds_bitree_node_t *root)
{
    if (!root) {
        return 0u;
    }

    return 1u + ds_bitree_size(root->left) + ds_bitree_size(root->right);
}

bool ds_bitree_is_leaf(const ds_bitree_node_t *node)
{
    return node && !node->left && !node->right;
}

/* ============================================================
 * 演示函数
 * ============================================================ */

/* 访问回调：打印节点数据 */
static void print_node(const ds_bitree_node_t *node, int level, void *user_data)
{
    (void)user_data;
    (void)level;
    printf("%d ", *(const int *)node->data);
}

/* 构建一棵示例树：
 *       1
 *     /   \
 *    2     3
 *   / \   /
 *  4   5 6
 */
static ds_bitree_node_t *build_demo_tree(void)
{
    int val;

    val = 1;
    ds_bitree_node_t *root = ds_bitree_node_create(&val, sizeof(int));

    val = 2;
    root->left = ds_bitree_node_create(&val, sizeof(int));
    val = 3;
    root->right = ds_bitree_node_create(&val, sizeof(int));

    val = 4;
    root->left->left = ds_bitree_node_create(&val, sizeof(int));
    val = 5;
    root->left->right = ds_bitree_node_create(&val, sizeof(int));

    val = 6;
    root->right->left = ds_bitree_node_create(&val, sizeof(int));

    return root;
}

void ds_binary_tree_demo(void)
{
    printf("========== 二叉树演示 ==========\n");

    ds_bitree_node_t *root = build_demo_tree();

    printf("\n前序遍历（根→左→右）: ");
    ds_bitree_preorder(root, print_node, NULL);
    printf("\n");

    printf("中序遍历（左→根→右）: ");
    ds_bitree_inorder(root, print_node, NULL);
    printf("\n");

    printf("后序遍历（左→右→根）: ");
    ds_bitree_postorder(root, print_node, NULL);
    printf("\n");

    printf("层序遍历（逐层）: ");
    ds_bitree_levelorder(root, print_node, NULL);
    printf("\n");

    printf("\n树的高度: %zu\n", ds_bitree_height(root));
    printf("树的节点数: %zu\n", ds_bitree_size(root));
    printf("叶子节点: ");
    /* 简单列举叶子——在实际项目中会提供专门的遍历判断 */
    if (root->left->left && ds_bitree_is_leaf(root->left->left))
        printf("4 ");
    if (root->left->right && ds_bitree_is_leaf(root->left->right))
        printf("5 ");
    if (root->right->left && ds_bitree_is_leaf(root->right->left))
        printf("6 ");
    printf("\n");

    ds_bitree_node_destroy(root, NULL);
    printf("========== 二叉树演示结束 ==========\n");
}
