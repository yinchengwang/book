/*
 * deque.h —— 双端队列（Double-Ended Queue）
 *
 * ============================================================
 * 概述
 * ============================================================
 * 双端队列（deque，发音 "deck"）是一种允许在两端进行插入和删除的线性结构。
 * 它融合了栈（一端操作）和队列（FIFO）的能力，可以视为两者的超集。
 *
 * 本实现基于环形缓冲区（ring buffer），使用 front/rear 双指针在数组上
 * 实现 O(1) 的头尾插入删除。当容量不足时，自动扩容并重新排列元素。
 *
 * ============================================================
 * 核心操作及时间复杂度
 * ============================================================
 * | 操作                 | 复杂度 | 说明                   |
 * |---------------------|--------|----------------------|
 * | push_front / push_back | O(1)* | 均摊 O(1)             |
 * | pop_front / pop_back   | O(1)  |                        |
 * | front / back          | O(1)  |                        |
 * | size / empty          | O(1)  |                        |
 *
 * ============================================================
 * 使用场景
 * ============================================================
 * - 滑动窗口算法（作为单调队列的底层容器）
 * - 实现"撤销/重做"功能（前进后退分别在两端操作）
 * - BFS 的最短路径（0-1 BFS 中 push_front push_back 混合使用）
 * - 回文检查（从两端向中间收缩）
 *
 * ============================================================
 * 代码示例
 * ============================================================
 *   #include <ds/deque.h>
 *   #include <stdio.h>
 *
 *   int main(void) {
 *       ds_deque_t *dq = ds_deque_create(sizeof(int), 8);
 *       int val = 1;
 *       ds_deque_push_back(dq, &val);   // [1]
 *       val = 2;
 *       ds_deque_push_front(dq, &val);  // [2, 1]
 *       int out;
 *       ds_deque_pop_front(dq, &out);   // out=2, [1]
 *       ds_deque_destroy(dq);
 *       return 0;
 *   }
 *
 * ============================================================
 * 面试常见问题
 * ============================================================
 * 1. deque 和 vector 的区别？→ deque 支持 O(1) 头插，vector 只有 O(1) 尾插。
 * 2. 如何用 deque 实现滑动窗口最大值？→ 维护一个单调递减的 deque（见 monotonic_queue）。
 * 3. 环形缓冲区如何判断空/满？→ 使用 size 字段最简单直接。
 */

#ifndef DS_DEQUE_H
#define DS_DEQUE_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ds_deque ds_deque_t;

ds_deque_t *ds_deque_create(size_t element_size, size_t initial_capacity);
void ds_deque_destroy(ds_deque_t *deque);

/* 前端操作 */
int ds_deque_push_front(ds_deque_t *deque, const void *element);
int ds_deque_pop_front(ds_deque_t *deque, void *out);

/* 后端操作 */
int ds_deque_push_back(ds_deque_t *deque, const void *element);
int ds_deque_pop_back(ds_deque_t *deque, void *out);

/* 窥视（不移除） */
int ds_deque_front(const ds_deque_t *deque, void *out);
int ds_deque_back(const ds_deque_t *deque, void *out);
const void *ds_deque_front_ptr(const ds_deque_t *deque);
const void *ds_deque_back_ptr(const ds_deque_t *deque);

/* 查询 */
size_t ds_deque_size(const ds_deque_t *deque);
bool ds_deque_empty(const ds_deque_t *deque);
void ds_deque_clear(ds_deque_t *deque);

#ifdef __cplusplus
}
#endif

#endif /* DS_DEQUE_H */
