/*
 * segment_tree.h —— 线段树
 *
 * ============================================================
 * 概述
 * ============================================================
 * 线段树是一种用于区间查询和区间更新（Range Query, Range Update）的
 * 二叉树结构。每个节点代表一个区间 [L, R]，叶子节点代表单个元素，
 * 内部节点存储其子区间合并后的结果（如区间和、区间最值等）。
 *
 * 线段树的作用是将区间操作的时间复杂度从 O(n) 优化到 O(log n)。
 *
 * 线段树的结构（n=8 的满线段树）：
 *
 *                      [0,7] (sum=36)
 *                     /        \
 *               [0,3]           [4,7]
 *               sum=10          sum=26
 *              /     \         /     \
 *         [0,1]    [2,3]   [4,5]   [6,7]
 *         sum=3    sum=7   sum=11  sum=15
 *         /   \    /   \   /   \   /   \
 *       [0] [1] [2] [3] [4] [5] [6] [7]
 *       1    2   3   4   5   6   7   8
 *
 * 底层数组：4 × n（确保有足够空间存储线段树节点）
 *
 * ============================================================
 * 核心操作及时间复杂度
 * ============================================================
 * | 操作         | 复杂度  | 说明                           |
 * |-------------|--------|-------------------------------|
 * | build       | O(n)   | 自底向上建树                    |
 * | query       | O(log n) | 区间查询                       |
 * | update_point| O(log n) | 单点更新                       |
 * | update_range| O(log n) | 区间更新（需 lazy propagation） |
 *
 * ============================================================
 * 使用场景
 * ============================================================
 * - 数组区间求和/最大最小值查询（频繁修改+查询）
 * - 区间染色问题（线段覆盖）
 * - 统计区间内满足某条件的元素个数
 * - 矩形面积并（扫描线算法）
 *
 * ============================================================
 * 代码示例
 * ============================================================
 *   #include <ds/segment_tree.h>
 *   #include <stdio.h>
 *
 *   int main(void) {
 *       ds_segment_tree_demo();
 *       return 0;
 *   }
 *
 * ============================================================
 * 面试常见问题
 * ============================================================
 * 1. 线段树 vs 树状数组？→ 线段树功能更强大（区间更新），但实现更复杂、常数更大。
 * 2. lazy propagation 的原理？→ 将更新操作"暂存"在节点上，查询时再向下推。
 * 3. 为什么数组大小是 4×n？→ 线段树是二叉树，最坏情况下需要 4n 的空间。
 */

#ifndef DS_SEGMENT_TREE_H
#define DS_SEGMENT_TREE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ds_segment_tree ds_segtree_t;

/*
 * 创建线段树（支持区间求和）。
 * arr: 原始数组, n: 数组大小
 */
ds_segtree_t *ds_segtree_create(const int64_t *arr, size_t n);
void ds_segtree_destroy(ds_segtree_t *tree);

/* 单点更新：arr[idx] += delta */
int ds_segtree_update_point(ds_segtree_t *tree, size_t idx, int64_t delta);

/* 区间更新：[L, R] 每个元素 += delta（带 lazy propagation） */
int ds_segtree_update_range(ds_segtree_t *tree, size_t L, size_t R, int64_t delta);

/* 区间查询：求 [L, R] 的区间和 */
int64_t ds_segtree_query(const ds_segtree_t *tree, size_t L, size_t R);

/* 演示函数 */
void ds_segment_tree_demo(void);

#ifdef __cplusplus
}
#endif

#endif /* DS_SEGMENT_TREE_H */
