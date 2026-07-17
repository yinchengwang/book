/*
 * binary_tree.h —— 二叉树
 *
 * ============================================================
 * 概述
 * ============================================================
 * 二叉树是一种树形结构，每个节点最多有两个子节点（左子节点和右子节点）。
 * 二叉树是计算机科学中最重要的数据结构之一，是二叉搜索树、堆、AVL树、
 * 红黑树等的基础。
 *
 * 二叉树的基本概念：
 * - 根节点（Root）：树的顶端节点，没有父节点
 * - 叶子节点（Leaf）：没有子节点的节点
 * - 深度（Depth）：从根到该节点的边数
 * - 高度（Height）：从该节点到最深叶子的边数
 * - 层（Level）：根在第 0 层，深度为 d 的节点在第 d 层
 *
 * 二叉树的遍历（核心考点）：
 * - 前序（Pre-order）：根 → 左 → 右
 * - 中序（In-order）：左 → 根 → 右（BST 中序遍历得到有序序列！）
 * - 后序（Post-order）：左 → 右 → 根（用于自底向上的计算）
 * - 层序（Level-order）：按层从上到下、每层从左到右（BFS）
 *
 * ============================================================
 * 核心操作及时间复杂度
 * ============================================================
 * | 操作            | 复杂度  | 说明                           |
 * |----------------|--------|-------------------------------|
 * | 四种遍历         | O(n)   | 每个节点访问一次                  |
 * | 计算高度/大小    | O(n)   | 需要遍历所有节点                  |
 * | 查找            | O(n)   | 普通二叉树无排序保证，需遍历        |
 *
 * ============================================================
 * 使用场景
 * ============================================================
 * - 表达式树（Expression Tree）：叶子是操作数，内部节点是运算符
 * - 哈夫曼树（Huffman Tree）：用于数据压缩
 * - 决策树：机器学习中的分类模型
 * - 语法树（AST）：编译器中的语法分析结果
 *
 * ============================================================
 * 代码示例
 * ============================================================
 *   #include <ds/binary_tree.h>
 *   #include <stdio.h>
 *
 *   int main(void) {
 *       ds_binary_tree_demo();
 *       return 0;
 *   }
 *
 * ============================================================
 * 面试常见问题
 * ============================================================
 * 1. 前中后序遍历的递归和迭代实现？→ 递归简单直接；迭代需用栈模拟，后序最复杂。
 * 2. 如何用前序+中序重建二叉树？→ 前序首元素是根，中序中根左边是左子树，右边是右子树。
 * 3. 二叉树和二叉树的区别？→ 每个节点最多两个子节点 vs 无限制。
 * 4. 完全二叉树 vs 满二叉树？→ 满：每层都满；完全：除最后一层外满，最后一层左对齐。
 */

#ifndef DS_BINARY_TREE_H
#define DS_BINARY_TREE_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 二叉树节点。注意：这里暴露了结构体内部字段，
 * 因为树的递归性质使得不透明封装变得困难且不自然。
 * 调用者通过提供的函数操作节点，不直接修改内部字段。
 */
typedef struct ds_bitree_node {
    void                 *data;  /* 节点数据 */
    struct ds_bitree_node *left;  /* 左子节点 */
    struct ds_bitree_node *right; /* 右子节点 */
} ds_bitree_node_t;

/* 创建/销毁节点 */
ds_bitree_node_t *ds_bitree_node_create(const void *data, size_t element_size);
void ds_bitree_node_destroy(ds_bitree_node_t *node, void (*free_data)(void *));

/*
 * 遍历回调函数。
 * node: 当前节点, level: 当前层数（从 0 开始）, user_data: 用户自定义数据
 */
typedef void (*ds_bitree_visit_fn)(const ds_bitree_node_t *node, int level, void *user_data);

/* 四种遍历方式（递归实现） */
void ds_bitree_preorder(const ds_bitree_node_t *root, ds_bitree_visit_fn visit, void *user_data);
void ds_bitree_inorder(const ds_bitree_node_t *root, ds_bitree_visit_fn visit, void *user_data);
void ds_bitree_postorder(const ds_bitree_node_t *root, ds_bitree_visit_fn visit, void *user_data);
void ds_bitree_levelorder(const ds_bitree_node_t *root, ds_bitree_visit_fn visit, void *user_data);

/* 查询 */
size_t ds_bitree_height(const ds_bitree_node_t *root);
size_t ds_bitree_size(const ds_bitree_node_t *root);
bool ds_bitree_is_leaf(const ds_bitree_node_t *node);

/* 演示函数 */
void ds_binary_tree_demo(void);

#ifdef __cplusplus
}
#endif

#endif /* DS_BINARY_TREE_H */
