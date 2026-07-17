/*
 * monotonic_stack.h —— 单调栈
 *
 * ============================================================
 * 概述
 * ============================================================
 * 单调栈是一种特殊用途的栈，栈内元素保持单调性（单调递增或单调递减）。
 * 当新元素即将破坏单调性时，持续弹出栈顶直到恢复单调性为止。
 *
 * 核心思想：每个元素入栈一次、出栈一次，总时间复杂度 O(n)。
 * 虽然内层有 while 循环，但均摊分析保证每个元素至多被 pop 一次。
 *
 * 单调性选择策略（记忆口诀）：
 * - 找"下一个更大元素" → 单调递减栈（遇到更大的就 pop 小的）
 * - 找"下一个更小元素" → 单调递增栈（遇到更小的就 pop 大的）
 * - 求"以某元素为最值的区间" → 单调栈可以同时得到左右边界
 *
 * ============================================================
 * 核心操作及时间复杂度
 * ============================================================
 * | 操作   | 复杂度 | 说明                              |
 * |--------|--------|----------------------------------|
 * | push   | O(1)*  | 均摊 O(1)：每个元素入栈出栈各一次   |
 * | pop    | O(1)*  | 内部被 push 触发                   |
 * | top    | O(1)   |                                   |
 *
 * ============================================================
 * 使用场景
 * ============================================================
 * - 下一个更大/更小元素（Next Greater/Smaller Element）
 * - 柱状图中最大矩形（Largest Rectangle in Histogram）
 * - 接雨水（Trapping Rain Water）
 * - 每日温度（Daily Temperatures）
 * - 子数组最小值之和 / 最大值之和
 *
 * ============================================================
 * 代码示例
 * ============================================================
 *   #include <ds/monotonic_stack.h>
 *   #include <stdio.h>
 *
 *   int main(void) {
 *       ds_mono_stack_demo();
 *       return 0;
 *   }
 *
 * ============================================================
 * 面试常见问题
 * ============================================================
 * 1. 单调栈和普通栈的区别？→ 单调栈在 push 时会主动 pop 破坏单调性的元素。
 * 2. 为什么单调栈是 O(n)？→ 均摊分析：每个元素入栈一次出栈一次，总共 2n 次操作。
 * 3. "接雨水"为什么用单调栈？→ 当遇到更高柱子时，栈顶柱子形成一个"凹槽"可以接水。
 */

#ifndef DS_MONOTONIC_STACK_H
#define DS_MONOTONIC_STACK_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 单调栈类型。
 * compare 函数指针决定单调方向：
 *   compare(a,b) <= 0 → 单调递增（栈顶最小）
 *   compare(a,b) >= 0 → 单调递减（栈顶最大）
 */
typedef struct ds_mono_stack ds_mono_stack_t;

/*
 * compare 返回 -1/0/1（类似 strcmp 的约定）：
 *   返回 < 0：a < b
 *   返回   0：a == b
 *   返回 > 0：a > b
 */
typedef int (*ds_mono_stack_compare_fn)(const void *a, const void *b);

/*
 * 创建单调栈。
 * compare 决定栈的单调方向：
 *   - 找"下一个更大元素"：compare 应使栈递减（栈底大栈顶小），
 *     当新元素大于栈顶时弹出。（返回负值表示 a 比 b 小，即升序比较）
 *   - 找"下一个更小元素"：compare 应使栈递增，同理。
 */
ds_mono_stack_t *ds_mono_stack_create(size_t element_size,
                                       ds_mono_stack_compare_fn compare);
void ds_mono_stack_destroy(ds_mono_stack_t *stack);

/*
 * 入栈：会自动弹出破坏单调性的栈顶元素。
 * 返回弹出的元素个数（可用于计算"跨度"）。
 */
int ds_mono_stack_push(ds_mono_stack_t *stack, const void *element);

/* 出栈（手动） */
int ds_mono_stack_pop(ds_mono_stack_t *stack, void *out);

/* 查看栈顶 */
int ds_mono_stack_top(const ds_mono_stack_t *stack, void *out);
const void *ds_mono_stack_top_ptr(const ds_mono_stack_t *stack);

/* 查询 */
size_t ds_mono_stack_size(const ds_mono_stack_t *stack);
bool ds_mono_stack_empty(const ds_mono_stack_t *stack);
void ds_mono_stack_clear(ds_mono_stack_t *stack);

/* 演示函数 */
void ds_mono_stack_demo(void);

#ifdef __cplusplus
}
#endif

#endif /* DS_MONOTONIC_STACK_H */
