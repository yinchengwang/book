/*
 * array.h —— 动态数组（Vector）
 *
 * ============================================================
 * 概述
 * ============================================================
 * 动态数组是最基础的数据结构之一。它在底层使用一片连续内存存储元素，
 * 当容量不足时自动扩容（通常翻倍），从而在保留数组 O(1) 随机访问优势
 * 的同时，解除了编译期固定大小的限制。
 *
 * 核心思想：用空间换时间——通过预留额外容量来摊销插入操作的成本。
 * 单次 push_back 可能是 O(n)（触发扩容时），但均摊复杂度为 O(1)。
 *
 * ============================================================
 * 核心操作及时间复杂度
 * ============================================================
 * | 操作          | 平均复杂度 | 最坏复杂度 | 说明                 |
 * |--------------|-----------|-----------|---------------------|
 * | create       | O(1)      | O(1)      | 初始化               |
 * | destroy      | O(n)      | O(n)      | 释放所有元素和数组自身  |
 * | get / set    | O(1)      | O(1)      | 随机访问              |
 * | push_back    | O(1)*     | O(n)      | 均摊 O(1)，扩容时 O(n) |
 * | pop_back     | O(1)      | O(1)      | 删除末尾元素          |
 * | insert       | O(n)      | O(n)      | 需要移动后续元素       |
 * | remove_at    | O(n)      | O(n)      | 需要移动后续元素       |
 * | size / empty | O(1)      | O(1)      |                      |
 *
 * ============================================================
 * 使用场景
 * ============================================================
 * - 需要频繁随机访问且主要在尾部增删元素（如日志收集、栈）
 * - 已知大致元素数量，可以提前 reserve 避免扩容开销
 * - 几乎所有需要"列表"概念的场景
 *
 * ============================================================
 * 代码示例
 * ============================================================
 *   #include <ds/array.h>
 *   #include <stdio.h>
 *
 *   int main(void) {
 *       ds_array_t *arr = ds_array_create(sizeof(int), 4);
 *       int val = 42;
 *       ds_array_push_back(arr, &val);
 *       val = 100;
 *       ds_array_push_back(arr, &val);
 *
 *       int out;
 *       ds_array_get(arr, 0, &out);
 *       printf("arr[0] = %d, size = %zu\n", out, ds_array_size(arr));
 *
 *       ds_array_destroy(arr);
 *       return 0;
 *   }
 *
 * ============================================================
 * 面试常见问题
 * ============================================================
 * 1. 动态数组和链表的区别？→ 数组 O(1) 随机访问但插入/删除 O(n)；链表反之。
 * 2. 扩容策略为什么通常是翻倍？→ 均摊分析证明翻倍策略下 push_back 均摊 O(1)。
 * 3. 如何实现二维动态数组？→ 数组的数组，或展平为一维配合索引计算。
 */

#ifndef DS_ARRAY_H
#define DS_ARRAY_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ds_array ds_array_t;

/* 创建动态数组，initial_capacity 为 0 时使用默认值 */
ds_array_t *ds_array_create(size_t element_size, size_t initial_capacity);
void ds_array_destroy(ds_array_t *array);

/* 随机访问：get 将元素拷贝到 out，set 将 element 拷贝到指定位置 */
int ds_array_get(const ds_array_t *array, size_t index, void *out);
int ds_array_set(ds_array_t *array, size_t index, const void *element);
const void *ds_array_get_ptr(const ds_array_t *array, size_t index);

/* 尾部操作 */
int ds_array_push_back(ds_array_t *array, const void *element);
int ds_array_pop_back(ds_array_t *array, void *out);

/* 中间插入/删除：index 必须在 [0, size] 范围内 */
int ds_array_insert(ds_array_t *array, size_t index, const void *element);
int ds_array_remove_at(ds_array_t *array, size_t index, void *out);

/* 容量管理 */
int ds_array_reserve(ds_array_t *array, size_t capacity);
int ds_array_shrink_to_fit(ds_array_t *array);

/* 查询 */
size_t ds_array_size(const ds_array_t *array);
size_t ds_array_capacity(const ds_array_t *array);
bool ds_array_empty(const ds_array_t *array);
void ds_array_clear(ds_array_t *array);

#ifdef __cplusplus
}
#endif

#endif /* DS_ARRAY_H */
