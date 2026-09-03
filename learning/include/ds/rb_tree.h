/*
 * rb_tree.h —— 红黑树（Red-Black Tree）
 *
 * ============================================================
 * 概述
 * ============================================================
 * 红黑树是一种自平衡二叉搜索树，每个节点额外存储一个颜色位（红/黑）。
 * 通过颜色约束和旋转操作保证树的高度不超过 2log(n+1)，从而确保
 * 所有操作都在 O(log n) 时间内完成。
 *
 * 红黑树的 5 条性质：
 * 1. 每个节点是红色或黑色
 * 2. 根节点始终是黑色
 * 3. 每个叶子节点（NIL）是黑色
 * 4. 红色节点的两个子节点必须是黑色（不能有两个连续的红色节点）
 * 5. 从任意节点到其所有后代叶子的路径上，黑色节点数量相同（黑高相等）
 *
 * 红黑树 vs AVL 树：
 * - 红黑树牺牲了严格的平衡（最长路径 ≤ 2×最短路径）换取更少的旋转
 * - 插入：红黑树最多 2 次旋转，AVL 最多 2 次旋转
 * - 删除：红黑树最多 3 次旋转，AVL 可能需要 O(log n) 次
 * - 查找：AVL 略快（更严格平衡），红黑树稍慢（树稍高）
 * - 工程实践：红黑树更受欢迎（C++ std::map、Java TreeMap、Linux CFS）
 *
 * ============================================================
 * 核心操作及时间复杂度
 * ============================================================
 * | 操作   | 复杂度  | 说明                    |
 * |-------|--------|-------------------------|
 * | insert| O(log n) | 最多 2 次旋转           |
 * | delete| O(log n) | 最多 3 次旋转           |
 * | search| O(log n) |                        |
 *
 * ============================================================
 * 面试常见问题
 * ============================================================
 * 1. 为什么红黑树比 AVL 树更常用？→ 删除操作红黑树最多 3 次旋转，AVL 可能 O(log n) 次。
 * 2. 红黑树的最长路径和最短路径的关系？→ 最长 ≤ 2×最短（因为不能有连续红节点）。
 * 3. 插入时新节点为什么是红色？→ 红色可能只违反性质4（容易修复）；黑色必定违反性质5（难修）。
 */

#ifndef DS_RB_TREE_H
#define DS_RB_TREE_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 红黑树节点颜色 */
typedef enum { DS_RB_BLACK = 0, DS_RB_RED = 1 } ds_rb_color_t;

typedef struct ds_rb_node {
    void               *data;
    struct ds_rb_node  *left;
    struct ds_rb_node  *right;
    struct ds_rb_node  *parent;  /* 红黑树需要父指针以支持旋转和修复 */
    ds_rb_color_t       color;
} ds_rb_node_t;

typedef int (*ds_rb_compare_fn)(const void *a, const void *b);

/*
 * 红黑树结构：封装根节点 + NIL 哨兵。
 * NIL 哨兵简化了边界处理——所有"空"引用都指向 NIL（黑色叶子）。
 */
typedef struct {
    ds_rb_node_t      *root;
    ds_rb_node_t      *nil;    /* NIL 哨兵节点 */
    ds_rb_compare_fn    compare;
    size_t              size;
} ds_rb_tree_t;

/* 创建/销毁红黑树 */
ds_rb_tree_t *ds_rb_tree_create(ds_rb_compare_fn compare);
void ds_rb_tree_destroy(ds_rb_tree_t *tree, void (*free_data)(void *));

/* 插入/查找/删除 */
int ds_rb_insert(ds_rb_tree_t *tree, const void *data, size_t element_size);
const ds_rb_node_t *ds_rb_search(const ds_rb_tree_t *tree, const void *key);
int ds_rb_delete(ds_rb_tree_t *tree, const void *key, void (*free_data)(void *));

/* 查询 */
size_t ds_rb_size(const ds_rb_tree_t *tree);
bool ds_rb_empty(const ds_rb_tree_t *tree);

/* 演示函数 */
void ds_rb_tree_demo(void);

#ifdef __cplusplus
}
#endif

#endif /* DS_RB_TREE_H */
