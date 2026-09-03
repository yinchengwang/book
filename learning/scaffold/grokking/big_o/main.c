/**
 * @file big_o/main.c
 * @brief Big O 时间空间复杂度分析演示
 *
 * 演示常见时间复杂度: O(1), O(log n), O(n), O(n log n), O(n²), O(2^n)
 * 以及空间复杂度分析
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ============================================================================
// 时间复杂度演示
// ============================================================================

/**
 * O(1) - 常数时间
 * 无论输入规模如何，操作次数不变
 */
int o_1(int *arr, int n) {
    (void)arr; (void)n;  // 未使用参数，消除警告
    return arr[0];       // 只需访问一个元素
}

/**
 * O(log n) - 对数时间
 * 每次操作将问题规模减半，典型如二分查找
 */
int binary_search(int *arr, int n, int target) {
    int left = 0, right = n - 1;
    while (left <= right) {
        int mid = left + (right - left) / 2;
        if (arr[mid] == target) return mid;
        if (arr[mid] < target) left = mid + 1;
        else right = mid - 1;
    }
    return -1;
}

/**
 * O(n) - 线性时间
 * 操作次数与输入规模成正比
 */
int linear_search(int *arr, int n, int target) {
    for (int i = 0; i < n; i++) {
        if (arr[i] == target) return i;
    }
    return -1;
}

/**
 * O(n log n) - 线性对数时间
 * 典型如高效排序算法: 归并排序、快速排序
 */
void merge_sort(int *arr, int left, int right) {
    if (left >= right) return;
    int mid = left + (right - left) / 2;
    merge_sort(arr, left, mid);
    merge_sort(arr, mid + 1, right);
    // 合并操作 O(n) 在这里省略...
}

/**
 * O(n²) - 平方时间
 * 双重循环，典型如冒泡排序、选择排序
 */
void bubble_sort(int *arr, int n) {
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (arr[j] > arr[j + 1]) {
                int tmp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = tmp;
            }
        }
    }
}

/**
 * O(2^n) - 指数时间
 * 递归树呈指数增长，典型如斐波那契数列的朴素递归
 */
int fibonacci_naive(int n) {
    if (n <= 1) return n;
    return fibonacci_naive(n - 1) + fibonacci_naive(n - 2);
}

// ============================================================================
// 空间复杂度演示
// ============================================================================

/**
 * O(1) 空间
 * 只使用常量额外空间
 */
int o_1_space_1(int *arr, int n) {
    int sum = 0;  // 只用一个变量
    for (int i = 0; i < n; i++) sum += arr[i];
    return sum;
}

/**
 * O(n) 空间
 * 创建与输入规模成正比的数组
 */
int *o_n_space(int *arr, int n) {
    int *copy = (int *)malloc(n * sizeof(int));  // O(n) 额外空间
    if (!copy) return NULL;
    memcpy(copy, arr, n * sizeof(int));
    return copy;  // 调用者需要 free()
}

/**
 * O(n²) 空间
 * 创建二维矩阵
 */
int **o_n2_space(int rows, int cols) {
    int **matrix = (int **)malloc(rows * sizeof(int *));
    if (!matrix) return NULL;
    for (int i = 0; i < rows; i++) {
        matrix[i] = (int *)malloc(cols * sizeof(int));
    }
    return matrix;  // 调用者需要释放
}

// ============================================================================
// 复杂度对比表格
// ============================================================================

void print_complexity_table(void) {
    printf("\n=============================================================\n");
    printf("                    常见复杂度对比表\n");
    printf("=============================================================\n");
    printf("%-15s %-15s %-20s %-15s\n", "复杂度", "名称", "典型算法", "10个元素");
    printf("-------------------------------------------------------------\n");
    printf("%-15s %-15s %-20s %-15d\n", "O(1)", "常数时间", "数组访问", 1);
    printf("%-15s %-15s %-20s %-15d\n", "O(log n)", "对数时间", "二分查找", 3);
    printf("%-15s %-15s %-20s %-15d\n", "O(n)", "线性时间", "线性搜索", 10);
    printf("%-15s %-15s %-20s %-15d\n", "O(n log n)", "线性对数", "归并排序", 33);
    printf("%-15s %-15s %-20s %-15d\n", "O(n²)", "平方时间", "冒泡排序", 100);
    printf("%-15s %-15s %-20s %-15d\n", "O(2^n)", "指数时间", "斐波那契", 1024);
    printf("%-15s %-15s %-20s %-15d\n", "O(n!)", "阶乘时间", "全排列", 3628800);
    printf("=============================================================\n\n");
}

// ============================================================================
// 主函数
// ============================================================================

int main(void) {
    printf("=== Big O 时间空间复杂度分析演示 ===\n\n");

    // 打印复杂度对比表
    print_complexity_table();

    // 测试时间复杂度
    int arr[] = {1, 3, 5, 7, 9, 11, 13, 15, 17, 19};
    int n = 10;

    printf("--- 时间复杂度测试 ---\n");
    printf("O(1) 访问 arr[0]: %d\n", o_1(arr, n));
    printf("O(log n) 二分查找 7: 索引 %d\n", binary_search(arr, n, 7));
    printf("O(n) 线性查找 13: 索引 %d\n", linear_search(arr, n, 13));
    printf("O(n log n) 归并排序: 递归演示 (见代码)\n");
    printf("O(n²) 冒泡排序: 双重循环 (见代码)\n");
    printf("O(2^n) 斐波那契(5): %d\n", fibonacci_naive(5));

    printf("\n--- 空间复杂度测试 ---\n");
    printf("O(1) 空间求和: %d\n", o_1_space_1(arr, n));

    // O(n) 空间测试
    int *copy = o_n_space(arr, n);
    if (copy) {
        printf("O(n) 空间数组复制: [");
        for (int i = 0; i < n; i++) printf("%d ", copy[i]);
        printf("]\n");
        free(copy);
    }

    printf("\n=== 演示完成 ===\n");
    return 0;
}
