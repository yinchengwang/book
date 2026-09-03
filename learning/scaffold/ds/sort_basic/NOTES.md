# 工程对照笔记

## 概述

本学习卡片演示的三种基础排序算法直接对应工程代码中的实际实现。工程中的排序实现更通用，支持任意类型元素。以下对照 `sort.c` 中的实际实现。

---

## 对照一：冒泡排序工程实现

**源文件**：`engineering/src/algo-prod/sort/sort.c`

工程中的冒泡排序是通用实现，支持任意元素类型和比较函数：

```c
int sort_bubble(void *base, size_t count, size_t element_size,
                sort_compare_fn compare)
{
    size_t pass;
    size_t index;

    if (sort_validate_generic(base, count, element_size, compare) != 0) {
        return -1;
    }

    for (pass = count; pass > 1u; --pass) {
        bool swapped = false;

        for (index = 1u; index < pass; ++index) {
            /* 通用比较：通过函数指针 */
            if (compare(sort_element_at(base, index - 1u, element_size),
                        sort_element_at(base, index, element_size)) > 0) {
                /* 通用交换：字节级交换 */
                sort_swap_bytes(sort_element_at(base, index - 1u, element_size),
                                sort_element_at(base, index, element_size),
                                element_size);
                swapped = true;
            }
        }
        if (!swapped) {
            break;  /* 提前终止优化 */
        }
    }

    return 0;
}
```

**对照学习点**：
- 与本卡片 `bubble_sort` 逻辑完全一致
- 关键区别：工程实现支持任意类型（通过 `element_size` 和比较函数指针）
- `sort_element_at()` 根据元素大小计算任意元素的内存地址
- `sort_swap_bytes()` 逐字节交换，支持任意大小元素

---

## 对照二：选择排序工程实现

**源文件**：`engineering/src/algo-prod/sort/sort.c`

```c
int sort_selection(void *base, size_t count, size_t element_size,
                   sort_compare_fn compare)
{
    size_t index;

    if (sort_validate_generic(base, count, element_size, compare) != 0) {
        return -1;
    }

    for (index = 0u; index + 1u < count; ++index) {
        size_t min_index = index;
        size_t cursor;

        /* 在未排序部分找最小值 */
        for (cursor = index + 1u; cursor < count; ++cursor) {
            if (compare(sort_element_at(base, cursor, element_size),
                        sort_element_at(base, min_index, element_size)) < 0) {
                min_index = cursor;
            }
        }

        /* 交换到已排序部分 */
        if (min_index != index) {
            sort_swap_bytes(sort_element_at(base, index, element_size),
                            sort_element_at(base, min_index, element_size),
                            element_size);
        }
    }

    return 0;
}
```

**对照学习点**：
- 与本卡片 `selection_sort` 逻辑一致
- 注意 `index + 1u < count` 而不是 `<=`：最后一个元素不需要比较
- 选择排序的不稳定性来自这里：相同值的元素可能被交换位置

---

## 对照三：插入排序工程实现

**源文件**：`engineering/src/algo-prod/sort/sort.c`

```c
int sort_insertion(void *base, size_t count, size_t element_size,
                   sort_compare_fn compare)
{
    unsigned char *temp;
    size_t index;

    if (sort_validate_generic(base, count, element_size, compare) != 0) {
        return -1;
    }
    if (count < 2u) {
        return 0;
    }

    /* 临时缓冲区存储当前要插入的元素 */
    temp = (unsigned char *)malloc(element_size);
    if (!temp) {
        return -1;
    }

    for (index = 1u; index < count; ++index) {
        size_t position = index;

        /* 保存当前元素 */
        sort_copy_bytes(temp, sort_element_at(base, index, element_size),
                        element_size);

        /* 元素后移，寻找插入位置 */
        while (position > 0u &&
               compare(sort_element_at(base, position - 1u, element_size), temp) > 0) {
            sort_copy_bytes(sort_element_at(base, position, element_size),
                            sort_element_at(base, position - 1u, element_size),
                            element_size);
            position -= 1u;
        }

        /* 插入元素 */
        sort_copy_bytes(sort_element_at(base, position, element_size), temp,
                        element_size);
    }

    free(temp);
    return 0;
}
```

**对照学习点**：
- 与本卡片 `insertion_sort` 逻辑一致
- 关键区别：使用 `temp` 临时缓冲区保存要插入的元素（通用类型）
- `sort_copy_bytes()` 使用 `memcpy` 实现任意类型复制
- 稳定排序的关键：元素后移时从后向前，不跳过相同值

---

## 对照四：通用工具函数

**源文件**：`engineering/src/algo-prod/sort/sort.c`

工程排序实现依赖的通用工具函数：

```c
/* 计算任意索引元素的内存地址 */
static unsigned char *sort_element_at(void *base, size_t index, size_t element_size)
{
    return (unsigned char *)base + index * element_size;
}

/* 字节级交换 - 支持任意大小元素 */
static void sort_swap_bytes(void *lhs, void *rhs, size_t element_size)
{
    unsigned char *left = (unsigned char *)lhs;
    unsigned char *right = (unsigned char *)rhs;
    size_t index;

    if (lhs == rhs) {
        return;
    }

    for (index = 0u; index < element_size; ++index) {
        unsigned char temp = left[index];
        left[index] = right[index];
        right[index] = temp;
    }
}

/* 字节级复制 - 支持任意大小元素 */
static void sort_copy_bytes(void *dst, const void *src, size_t element_size)
{
    memcpy(dst, src, element_size);
}
```

**对照学习点**：
- 这三个函数是通用排序的基石
- `sort_element_at`：用指针算术计算元素地址
- `sort_swap_bytes`：逐字节交换，避免类型依赖
- `sort_copy_bytes`：直接用 `memcpy`，高效

---

## 工程代码模式总结

| 函数 | 作用 | 对应卡片 |
|------|------|---------|
| `sort_bubble()` | 冒泡排序 | `bubble_sort()` |
| `sort_selection()` | 选择排序 | `selection_sort()` |
| `sort_insertion()` | 插入排序 | `insertion_sort()` |
| `sort_element_at()` | 计算元素地址 | 数组下标计算 |
| `sort_swap_bytes()` | 通用交换 | `swap()` |
| `sort_copy_bytes()` | 通用复制 | 元素赋值 |

---

## 延伸阅读

- `engineering/src/algo-prod/sort/sort.c` - 完整排序实现
- `engineering/src/algo-prod/sort/sort.h` - 排序头文件和算法枚举
- `sort.h` 中定义了 `sort_algorithm_t` 枚举，支持所有排序算法
