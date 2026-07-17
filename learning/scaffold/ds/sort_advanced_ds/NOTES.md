# 工程对照笔记

## 概述

本学习卡片演示的三种高级排序算法（快排、归并、堆排）在工程中均有实际应用。以下对照工程源码中的排序实现，帮助理解理论算法与生产代码的关联。

---

## 对照一：快速排序 — `sort_quick()` 迭代实现

**源文件**：`engineering/src/algo-prod/sort/sort.c`

工程代码使用**迭代版本**而非递归，通过显式栈避免函数调用开销：

```c
int sort_quick(void *base, size_t count, size_t element_size, sort_compare_fn compare)
{
    quick_sort_range_t *stack;
    size_t top = 0u;

    // 分配栈空间，大小为元素个数
    stack = (quick_sort_range_t *)malloc(count * sizeof(quick_sort_range_t));

    stack[top].left = 0u;
    stack[top].right = count - 1;
    top += 1u;

    while (top > 0u) {
        quick_sort_range_t range = stack[--top];
        if (range.left >= range.right) continue;

        // 分区获取 pivot 位置
        size_t pivot = sort_quick_partition(base, range.left, range.right, ...);

        // 先压入大区间，再压入小区间（优化栈深度）
        if (pivot + 1u < range.right) {
            stack[top].left = pivot + 1u;
            stack[top].right = range.right;
            top += 1u;
        }
        if (range.left < pivot) {
            stack[top].left = range.left;
            stack[top].right = pivot - 1u;
            top += 1u;
        }
    }
    free(stack);
}
```

**对照学习点**：
- 学习卡片使用递归实现，便于理解分治思想
- 工程代码使用迭代栈，避免递归深度问题和函数调用开销
- 先压大区间后压小区间，控制栈空间在 O(log n)

---

## 对照二：归并排序 — `sort_merge()` 自底向上

**源文件**：`engineering/src/algo-prod/sort/sort.c`

工程代码采用**自顶向下递归**，但在合并阶段有优化：

```c
static int sort_merge_recursive(unsigned char *base, unsigned char *temp,
                                 size_t left, size_t right,
                                 size_t element_size, sort_compare_fn compare)
{
    if (right - left <= 1u) return 0;  // 递归终止条件

    size_t mid = left + (right - left) / 2u;

    // 分治：先排序左右两半
    sort_merge_recursive(base, temp, left, mid, element_size, compare);
    sort_merge_recursive(base, temp, mid, right, element_size, compare);

    // 合并两个有序区间
    size_t left_index = left, right_index = mid, write_index = left;
    while (left_index < mid && right_index < right) {
        if (compare(base + left_index * element_size,
                    base + right_index * element_size) <= 0) {
            sort_copy_bytes(temp + write_index * element_size,
                            base + left_index * element_size, element_size);
            left_index++;
        } else {
            sort_copy_bytes(temp + write_index * element_size,
                            base + right_index * element_size, element_size);
            right_index++;
        }
        write_index++;
    }

    // 处理剩余元素
    while (left_index < mid) { /* ... */ }
    while (right_index < right) { /* ... */ }

    // 复制回原数组
    memcpy(base + left * element_size, temp + left * element_size,
           (right - left) * element_size);
}
```

**对照学习点**：
- 学习卡片的归并排序是教学版本，简化了边界处理
- 工程代码使用 `unsigned char *` 泛型指针，支持任意元素类型
- 比较函数通过 `sort_compare_fn` 回调，支持自定义排序规则
- 复制回原数组使用 `memcpy` 优化批量操作

---

## 对照三：堆排序 — `sort_heap()` 下沉调整

**源文件**：`engineering/src/algo-prod/sort/sort.c`

工程代码的堆排序使用**下沉调整**实现建堆和排序：

```c
static void sort_heap_sift_down(void *base, size_t start, size_t end,
                                 size_t element_size, sort_compare_fn compare)
{
    size_t root = start;

    while (true) {
        size_t child = root * 2u + 1u;
        size_t swap_index = root;

        if (child > end) return;
        if (compare(sort_element_at(base, swap_index, element_size),
                    sort_element_at(base, child, element_size)) < 0) {
            swap_index = child;
        }
        if (child + 1u <= end &&
            compare(sort_element_at(base, swap_index, element_size),
                    sort_element_at(base, child + 1u, element_size)) < 0) {
            swap_index = child + 1u;
        }
        if (swap_index == root) return;

        sort_swap_bytes(sort_element_at(base, root, element_size),
                        sort_element_at(base, swap_index, element_size),
                        element_size);
        root = swap_index;
    }
}

int sort_heap(void *base, size_t count, size_t element_size, sort_compare_fn compare)
{
    // 建堆：从最后一个非叶子节点向上调整
    for (size_t start = count / 2u; start > 0u; --start) {
        sort_heap_sift_down(base, start - 1u, count - 1u, element_size, compare);
    }

    // 逐个取出堆顶，与末尾交换后调整
    for (size_t end = count - 1u; end > 0u; --end) {
        sort_swap_bytes(sort_element_at(base, 0u, element_size),
                        sort_element_at(base, end, element_size),
                        element_size);
        sort_heap_sift_down(base, 0u, end - 1u, element_size, compare);
    }
}
```

**对照学习点**：
- 学习卡片的堆排序使用 int 类型简化代码
- 工程代码使用 `sort_element_at()` 计算元素地址，支持泛型
- 建堆从 `count / 2` 开始（最后一个非叶子节点）
- 每次取堆顶后，堆大小减一（`end - 1`），实现原地排序

---

## 工程代码模式总结

| 算法 | 关键实现 | 工程优化点 |
|------|---------|-----------|
| 快排 | 迭代栈 + 分区 | 避免递归溢出，先压大区间 |
| 归并 | 泛型指针 + 回调 | memcpy 批量复制，支持任意类型 |
| 堆排 | 下沉调整 | 泛型实现，原地排序 O(1) |

---

## 延伸阅读

- `engineering/src/algo-prod/sort/sort.c` - 完整排序算法实现
- `engineering/include/algo-prod/sort/sort.h` - 排序 API 头文件
- `engineering/src/algo-prod/sort/sort_benchmark.c` - 排序性能测试
