/*
 * monotonic_queue.h —— 单调队列
 *
 * ============================================================
 * 概述
 * ============================================================
 * 单调队列是一种特殊用途的队列，队列内元素保持单调性（单调递增或递减）。
 * 与单调栈不同，单调队列在"队首"方向也有移除操作（当元素"过期"时），
 * 因此通常使用双端队列（deque）作为底层容器。
 *
 * 经典应用场景：滑动窗口最值问题。
 *
 * ============================================================
 * 核心操作及时间复杂度
 * ============================================================
 * | 操作    | 复杂度 | 说明                                   |
 * |---------|--------|---------------------------------------|
 * | push    | O(1)*  | 均摊：每个元素入队出队各一次              |
 * | pop     | O(1)*  | 均摊                                   |
 * | get_extreme | O(1) | 获取当前窗口最值（队首元素）              |
 *
 * ============================================================
 * 使用场景
 * ============================================================
 * - 滑动窗口最大值（Sliding Window Maximum）
 * - 滑动窗口中位数
 * - 带限制的最短子数组
 * - 任何需要"维护窗口内最值"的场景
 *
 * ============================================================
 * 代码示例
 * ============================================================
 *   #include <ds/monotonic_queue.h>
 *   #include <stdio.h>
 *
 *   int main(void) {
 *       ds_mono_queue_demo();
 *       return 0;
 *   }
 *
 * ============================================================
 * 面试常见问题
 * ============================================================
 * 1. 单调队列和单调栈的区别？→ 单调队列多了一个"队首过期移除"操作，适合滑动窗口。
 * 2. 滑动窗口最大值为什么用单调队列？→ 维护递减队列，队首始终是窗口最大值。
 *    新元素入队时从队尾弹出比它小的（它们不可能再成为最大值），
 *    窗口滑动时检查队首是否已滑出窗口。
 */

#ifndef DS_MONOTONIC_QUEUE_H
#define DS_MONOTONIC_QUEUE_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 单调队列。
 * 底层使用双端队列，新元素从队尾入队时弹出所有破坏单调性的元素，
 * 窗口滑动时从队首移除过期元素。
 */
typedef struct ds_mono_queue ds_mono_queue_t;

/* 比较函数：返回 a > b ? 正值 : 负值 : 0（类似减法语义） */
typedef int (*ds_mono_queue_compare_fn)(const void *a, const void *b);

/*
 * 创建单调队列。
 * element_size: 元素大小
 * compare: 比较函数
 *   返回 > 0 表示 a > b，用于维护递减队列（队首最大）
 *   返回 < 0 表示 a < b，用于维护递增队列（队首最小）
 */
ds_mono_queue_t *ds_mono_queue_create(size_t element_size, ds_mono_queue_compare_fn compare);
void ds_mono_queue_destroy(ds_mono_queue_t *queue);

/*
 * push：将新元素加入队尾，同时弹出队尾所有破坏单调性的元素。
 * 返回弹出的元素个数。
 */
int ds_mono_queue_push(ds_mono_queue_t *queue, const void *element);

/*
 * pop_front：移除队首元素（当它"过期"时）。
 * 通常由滑动窗口的"左侧滑出"触发。
 */
int ds_mono_queue_pop_front(ds_mono_queue_t *queue, void *out);

/* 获取当前最值（队首元素，不移除） */
int ds_mono_queue_get_extreme(const ds_mono_queue_t *queue, void *out);
const void *ds_mono_queue_get_extreme_ptr(const ds_mono_queue_t *queue);

/* 查询 */
size_t ds_mono_queue_size(const ds_mono_queue_t *queue);
bool ds_mono_queue_empty(const ds_mono_queue_t *queue);
void ds_mono_queue_clear(ds_mono_queue_t *queue);

/* 演示函数 */
void ds_mono_queue_demo(void);

#ifdef __cplusplus
}
#endif

#endif /* DS_MONOTONIC_QUEUE_H */
