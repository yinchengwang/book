/*
 * bst.h —— 二叉搜索树（Binary Search Tree）
 *
 * ============================================================
 * 概述
 * ============================================================
 * 二叉搜索树（BST）是一种特殊的二叉树，满足以下性质：
 * - 左子树中所有节点的值 < 根节点的值
 * - 右子树中所有节点的值 > 根节点的值
 * - 左右子树本身也是 BST
 *
 * BST 的核心优势：二分查找式的高效搜索，平均 O(log n)。
 *
 * 重要警告：BST 在最坏情况下（有序插入序列）会退化为链表，
 * 时间复杂度退化为 O(n)。这就是为什么需要 AVL 树和红黑树等
 * 自平衡变种（见 avl_tree 和 rb_tree 模块）。
 *
 * ============================================================
 * 核心操作及时间复杂度
 * ============================================================
 * | 操作     | 平均复杂度 | 最坏复杂度 | 说明                           |
 * |---------|-----------|-----------|-------------------------------|
 * | insert  | O(log n)  | O(n)      | 退化为链表时为 O(n)             |
 * | search  | O(log n)  | O(n)      |                               |
 * | delete  | O(log n)  | O(n)      |                               |
 * | min/max | O(log n)  | O(n)      | 最左/最右节点                   |
 *
 * ============================================================
 * 删除操作的三种情况
 * ============================================================
 * 删除节点 X 分三种情况：
 *   情况1（X 是叶子）：直接删除，父节点的指针置 NULL
 *   情况2（X 只有一个子节点）：用子节点替代 X
 *   情况3（X 有两个子节点）：找到右子树的最小节点（后继），
 *           将其值复制到 X，然后删除后继（后继最多只有右子节点，转为情况1或2）
 *
 * ============================================================
 * 使用场景
 * ============================================================
 * - C++ std::set / std::map 的内部实现（通常是红黑树）
 * - 需要有序遍历的键值存储
 * - 范围查询（find all keys in [L, R])
 *
 * ============================================================
 * 代码示例
 * ============================================================
 *   #include <ds/bst.h>
 *   #include <stdio.h>
 *
 *   int main(void) {
 *       ds_bst_demo();
 *       return 0;
 *   }
 *
 * ============================================================
 * 面试常见问题
 * ============================================================
 * 1. BST 如何验证？→ 中序遍历检查是否严格递增。
 * 2. BST 的删除为什么选后继而不是前驱？→ 都可以，选后继是惯例。
 *    某些自平衡树会根据高度选择。
 * 3. 如何找 BST 的第 k 小元素？→ 维护每个节点的子树大小，O(log n)。
 * 4. BST vs HashTable？→ BST O(log n) 有序遍历，HashTable O(1) 无序。
 */

#ifndef DS_BST_H
#define DS_BST_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ds_bst_node {
    void               *data;
    struct ds_bst_node *left;
    struct ds_bst_node *right;
} ds_bst_node_t;

typedef int (*ds_bst_compare_fn)(const void *a, const void *b);

/* 在 BST 中插入元素，返回新的根节点（根可能因 rebalance demo 变化） */
ds_bst_node_t *ds_bst_insert(ds_bst_node_t *root, const void *data,
                              size_t element_size, ds_bst_compare_fn compare);

/* 在 BST 中搜索元素，返回节点指针或 NULL */
const ds_bst_node_t *ds_bst_search(const ds_bst_node_t *root, const void *key,
                                    ds_bst_compare_fn compare);

/* 删除节点，返回新的根节点 */
ds_bst_node_t *ds_bst_delete(ds_bst_node_t *root, const void *key,
                              ds_bst_compare_fn compare, void (*free_data)(void *));

/* 获取最小值/最大值节点 */
const ds_bst_node_t *ds_bst_min(const ds_bst_node_t *root);
const ds_bst_node_t *ds_bst_max(const ds_bst_node_t *root);

/* 验证是否为合法的 BST（中序遍历检查递增） */
bool ds_bst_is_valid(const ds_bst_node_t *root, ds_bst_compare_fn compare);

/* 释放整棵树 */
void ds_bst_destroy(ds_bst_node_t *root, void (*free_data)(void *));

/* 演示函数 */
void ds_bst_demo(void);

#ifdef __cplusplus
}
#endif

#endif /* DS_BST_H */
