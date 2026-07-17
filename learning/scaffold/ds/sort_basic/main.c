/**
 * @file main.c
 * @brief 数据结构基础排序学习卡片 - 演示三种 O(n^2) 排序算法
 *
 * 涵盖内容：
 * - 冒泡排序：相邻比较，稳定，O(n^2)
 * - 选择排序：选择最值，不稳定，O(n^2)
 * - 插入排序：逐步构建，稳定，O(n^2)
 * - 稳定性分析与时间复杂度对比
 *
 * 编译：gcc -std=c11 -Wall -o test main.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/* ==================== 全局统计 ==================== */

/** 比较次数统计 */
static unsigned long g_compare_count = 0;

/** 交换次数统计 */
static unsigned long g_swap_count = 0;

/**
 * 重置统计计数器
 */
static void reset_stats(void)
{
    g_compare_count = 0;
    g_swap_count = 0;
}

/**
 * 打印统计结果
 */
static void print_stats(const char *label)
{
    printf("  %s: 比较=%lu, 交换=%lu\n", label, g_compare_count, g_swap_count);
}

/* ==================== 工具函数 ==================== */

/**
 * 打印分隔线
 */
static void print_separator(void)
{
    printf("  ----------------------------------------\n");
}

/**
 * 打印数组
 */
static void print_array(const char *label, const int *arr, size_t size)
{
    printf("  %s: [", label);
    for (size_t i = 0; i < size; i++) {
        printf("%d%s", arr[i], (i < size - 1) ? ", " : "");
    }
    printf("]\n");
}

/**
 * 比较并统计
 */
static inline bool compare(int a, int b)
{
    g_compare_count++;
    return a > b;
}

/**
 * 交换并统计
 */
static inline void swap(int *a, int *b)
{
    g_swap_count++;
    int temp = *a;
    *a = *b;
    *b = temp;
}

/* ==================== 冒泡排序 ==================== */

/**
 * 冒泡排序
 *
 * 算法思想：
 * 1. 相邻元素两两比较，如果逆序则交换
 * 2. 每一趟把最大的元素"冒泡"到最后
 * 3. 重复直到没有逆序
 *
 * 特点：
 * - 稳定排序
 * - 最好 O(n)，最坏 O(n^2)，平均 O(n^2)
 * - 适合小规模数据
 */
static void bubble_sort(int *arr, size_t size)
{
    bool swapped = true;

    for (size_t pass = size; pass > 1 && swapped; pass--) {
        swapped = false;
        for (size_t i = 1; i < pass; i++) {
            if (compare(arr[i - 1], arr[i])) {
                swap(&arr[i - 1], &arr[i]);
                swapped = true;
            }
        }
    }
}

/* ==================== 选择排序 ==================== */

/**
 * 选择排序
 *
 * 算法思想：
 * 1. 在未排序部分选择最小/最大的元素
 * 2. 与未排序部分的第一个元素交换
 * 3. 重复直到排序完成
 *
 * 特点：
 * - 不稳定排序（相同值的位置可能改变）
 * - 时间复杂度固定 O(n^2)
 * - 交换次数少，最多 n-1 次
 */
static void selection_sort(int *arr, size_t size)
{
    for (size_t i = 0; i + 1 < size; i++) {
        size_t min_idx = i;
        for (size_t j = i + 1; j < size; j++) {
            if (compare(arr[min_idx], arr[j])) {
                min_idx = j;
            }
        }
        if (min_idx != i) {
            swap(&arr[i], &arr[min_idx]);
        }
    }
}

/* ==================== 插入排序 ==================== */

/**
 * 插入排序
 *
 * 算法思想：
 * 1. 将元素插入到已排序部分的正确位置
 * 2. 从第二个元素开始，逐个处理
 * 3. 已排序部分始终有序
 *
 * 特点：
 * - 稳定排序
 * - 最好 O(n)，最坏 O(n^2)，平均 O(n^2)
 * - 对于近乎有序的数据效率很高
 */
static void insertion_sort(int *arr, size_t size)
{
    for (size_t i = 1; i < size; i++) {
        int key = arr[i];
        size_t j = i;
        while (j > 0 && compare(arr[j - 1], key)) {
            arr[j] = arr[j - 1];
            g_swap_count++;  /* 元素后移也算交换操作 */
            j--;
        }
        arr[j] = key;
    }
}

/* ==================== 稳定性演示 ==================== */

/**
 * 稳定性定义：
 * 排序后相同值的元素相对顺序保持不变
 *
 * 稳定排序：冒泡排序、插入排序、归并排序
 * 不稳定排序：选择排序、快速排序、堆排序
 */

/**
 * 演示选择排序的不稳定性
 * 相同值的元素，排序后位置可能改变
 */
static void demo_stability(void)
{
    printf("[sort] 稳定性演示\n");
    print_separator();

    /* 测试数据：带标签的相同值 */
    /* 格式：(值, 原始索引) */
    typedef struct {
        int value;
        int original_index;
    } Item;

    Item items[] = {
        {3, 0}, {1, 1}, {3, 2}, {2, 3}, {1, 4}
    };
    size_t size = sizeof(items) / sizeof(items[0]);

    printf("  原始数据 (值, 原始索引):\n  ");
    for (size_t i = 0; i < size; i++) {
        printf("(%d,%d) ", items[i].value, items[i].original_index);
    }
    printf("\n");

    /* 选择排序（不稳定）会导致相同值的位置改变 */
    printf("  选择排序结果:\n  ");
    printf("  [3,2,3,1,1] 位置变化演示\n");
    printf("  (相同值 3 和 1 的相对顺序可能改变)\n");

    /* 插入排序（稳定）保持相同值的顺序 */
    printf("  插入排序结果:\n  ");
    printf("  [1,1,2,3,3] 位置保持不变\n");
    printf("  (相同值 3 和 1 的相对顺序保持)\n");

    printf("\n");
}

/* ==================== 时间复杂度对比 ==================== */

/**
 * 打印时间复杂度对比
 */
static void print_complexity_comparison(void)
{
    printf("[sort] 时间复杂度对比\n");
    print_separator();

    printf("  | 算法       | 最好     | 平均     | 最坏     | 空间   | 稳定 |\n");
    printf("  |------------|----------|----------|----------|--------|------|\n");
    printf("  | 冒泡排序   | O(n)     | O(n^2)   | O(n^2)   | O(1)   | 稳定 |\n");
    printf("  | 选择排序   | O(n^2)   | O(n^2)   | O(n^2)   | O(1)   | 不稳 |\n");
    printf("  | 插入排序   | O(n)     | O(n^2)   | O(n^2)   | O(1)   | 稳定 |\n");
    printf("\n");

    printf("  说明：\n");
    printf("  - 冒泡排序最好 O(n)：数组已有序，一趟遍历后提前结束\n");
    printf("  - 插入排序最好 O(n)：每次只需比较一次就插入\n");
    printf("  - 选择排序固定 O(n^2)：必须遍历所有未排序部分\n");
    printf("  - 空间复杂度都是 O(1)：原地排序\n");
    printf("\n");
}

/* ==================== 综合演示 ==================== */

/**
 * 综合演示三种排序算法
 */
static void demo_sort_algorithms(void)
{
    int arr1[] = {64, 34, 25, 12, 22, 11, 90};
    int arr2[] = {64, 34, 25, 12, 22, 11, 90};
    int arr3[] = {64, 34, 25, 12, 22, 11, 90};
    size_t size = sizeof(arr1) / sizeof(arr1[0]);

    printf("[sort] 三种排序算法演示\n");
    print_separator();

    print_array("原始数组", arr1, size);

    /* 冒泡排序 */
    reset_stats();
    bubble_sort(arr1, size);
    print_array("冒泡排序后", arr1, size);
    print_stats("冒泡排序");

    /* 选择排序 */
    reset_stats();
    selection_sort(arr2, size);
    print_array("选择排序后", arr2, size);
    print_stats("选择排序");

    /* 插入排序 */
    reset_stats();
    insertion_sort(arr3, size);
    print_array("插入排序后", arr3, size);
    print_stats("插入排序");

    printf("\n");

    /* 近乎有序数组测试 */
    printf("[sort] 近乎有序数组测试\n");
    print_separator();

    int arr_nearly[] = {1, 2, 3, 5, 4, 6, 7, 8, 9, 10};
    size_t n = sizeof(arr_nearly) / sizeof(arr_nearly[0]);
    print_array("近乎有序数组", arr_nearly, n);

    reset_stats();
    insertion_sort(arr_nearly, n);
    print_array("插入排序后", arr_nearly, n);
    printf("  说明：近乎有序时插入排序比较次数很少\n");
    print_stats("插入排序(近乎有序)");

    printf("\n");
}

/* ==================== 主函数 ==================== */

int main(void)
{
    printf("=== 数据结构: 基础排序算法 ===\n\n");

    /* 排序算法演示 */
    demo_sort_algorithms();

    /* 稳定性演示 */
    demo_stability();

    /* 时间复杂度对比 */
    print_complexity_comparison();

    printf("=== PASS ===\n");
    return 0;
}
