/*
 * fenwick_tree.h —— 树状数组（Fenwick Tree / Binary Indexed Tree）
 *
 * ============================================================
 * 概述
 * ============================================================
 * 树状数组（BIT）是一种用于高效处理前缀和查询和单点更新的数据结构。
 * 它比线段树更简洁（通常 10-20 行代码），但功能也受限（主要支持
 * 前缀查询和单点更新，不支持任意区间更新）。
 *
 * 树状数组的核心是 lowbit 操作：lowbit(x) = x & (-x)
 * 它提取出 x 的二进制表示中最低位的 1 所代表的值。
 *
 * 树状数组不是一棵真正的"树"，而是利用二进制规律将数组组织起来。
 * tree[i] 存储的区间范围由 lowbit(i) 决定。
 *
 * ============================================================
 * lowbit 原理
 * ============================================================
 * lowbit(x) = x & (-x)
 *
 * 补码表示下：-x = ~x + 1
 * 因此 x & (-x) 提取了 x 二进制最低位的 1。
 *
 * 示例：
 *   x = 6 → 二进制 0110
 *              -x = 1010
 *        0110 & 1010 = 0010 = 2 → tree[6] 负责范围 [5,6]，长度为 2
 *
 *   x = 8 → 二进制 1000
 *              -x = 1000
 *        1000 & 1000 = 1000 = 8 → tree[8] 负责范围 [1,8]，长度为 8
 *
 * 树状数组结构（n=8）：
 *
 *   tree[1] = arr[1]                     (lowbit=1: [1,1])
 *   tree[2] = arr[1]+arr[2]              (lowbit=2: [1,2])
 *   tree[3] = arr[3]                     (lowbit=1: [3,3])
 *   tree[4] = arr[1]+arr[2]+arr[3]+arr[4] (lowbit=4: [1,4])
 *   tree[5] = arr[5]
 *   tree[6] = arr[5]+arr[6]
 *   tree[7] = arr[7]
 *   tree[8] = arr[1]+...+arr[8]
 *
 * ============================================================
 * 核心操作及时间复杂度
 * ============================================================
 * | 操作            | 复杂度  | 说明                           |
 * |----------------|--------|-------------------------------|
 * | update(idx, d) | O(log n) | 单点更新：arr[idx] += d        |
 * | prefix_sum(idx)| O(log n) | 前缀查询：sum(arr[1..idx])     |
 * | range_sum(L,R) | O(log n) | 区间查询：prefix(R) - prefix(L-1) |
 *
 * ============================================================
 * 使用场景
 * ============================================================
 * - 频繁的单点修改 + 前缀/区间求和
 * - 求逆序对数量（离散化 + BIT）
 * - 求数组中比当前元素小的元素个数
 * - 带修改的区间第 K 小
 *
 * ============================================================
 * 代码示例
 * ============================================================
 *   #include <ds/fenwick_tree.h>
 *   #include <stdio.h>
 *
 *   int main(void) {
 *       ds_fenwick_tree_demo();
 *       return 0;
 *   }
 *
 * ============================================================
 * 面试常见问题
 * ============================================================
 * 1. 树状数组 vs 线段树？→ BIT 代码更短、常数更小，但只能做前缀操作。
 * 2. lowbit 为什么有效？→ 利用了补码的位运算特性，详见 lowbit 原理部分。
 * 3. BIT 能做区间更新吗？→ 可以！使用差分技巧（两次 BIT）。
 */

#ifndef DS_FENWICK_TREE_H
#define DS_FENWICK_TREE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ds_fenwick ds_fenwick_t;

/* 创建大小为 n 的树状数组（下标 0 不使用，实际大小 n+1） */
ds_fenwick_t *ds_fenwick_create(const int64_t *arr, size_t n);
void ds_fenwick_destroy(ds_fenwick_t *bit);

/* 单点更新：arr[idx] += delta */
int ds_fenwick_update(ds_fenwick_t *bit, size_t idx, int64_t delta);

/* 前缀查询：sum(arr[1..idx]) */
int64_t ds_fenwick_prefix_sum(const ds_fenwick_t *bit, size_t idx);

/* 区间查询：sum(arr[L..R]) */
int64_t ds_fenwick_range_sum(const ds_fenwick_t *bit, size_t L, size_t R);

/* 演示函数 */
void ds_fenwick_tree_demo(void);

#ifdef __cplusplus
}
#endif

#endif /* DS_FENWICK_TREE_H */
