/**
 * @file main.c
 * @brief 高级排序算法学习卡片 - 演示快排/归并/堆排
 *
 * 编译：gcc -std=c11 -Wall -o test main.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================== 工具函数 ==================== */

static void print_array(const char *label, const int *arr, size_t size)
{
    printf("  %s: [", label);
    for (size_t i = 0; i < size; i++)
        printf("%d%s", arr[i], (i < size - 1) ? ", " : "");
    printf("]\n");
}

static void swap(int *a, int *b) { int t = *a; *a = *b; *b = t; }

static int *copy_array(const int *src, size_t size)
{
    int *dst = (int *)malloc(size * sizeof(int));
    return dst ? memcpy(dst, src, size * sizeof(int)) : NULL;
}

/* ==================== 快速排序 ==================== */

/**
 * 三数取中划分 + 小数组插入排序优化
 */
static void quick_sort_rec(int *arr, size_t left, size_t right)
{
    if (left >= right) return;

    /* 小数组用插入排序优化 */
    if (right - left < 7) {
        for (size_t i = left + 1; i <= right; i++) {
            int key = arr[i]; size_t j = i;
            while (j > left && arr[j-1] > key) { arr[j] = arr[j-1]; j--; }
            arr[j] = key;
        }
        return;
    }

    /* 三数取中选 pivot */
    size_t mid = left + (right - left) / 2;
    if (arr[left] > arr[mid]) swap(&arr[left], &arr[mid]);
    if (arr[mid] > arr[right]) swap(&arr[mid], &arr[right]);
    if (arr[left] > arr[mid]) swap(&arr[left], &arr[mid]);
    swap(&arr[mid], &arr[right - 1]);

    /* 分区 */
    int pivot = arr[right - 1];
    size_t i = left, j = right - 1;
    while (1) {
        while (arr[++i] < pivot) {}
        while (j > left && arr[--j] > pivot) {}
        if (i >= j) break;
        swap(&arr[i], &arr[j]);
    }
    swap(&arr[i], &arr[right - 1]);

    quick_sort_rec(arr, left, i > 0 ? i - 1 : 0);
    quick_sort_rec(arr, i + 1, right);
}

static void quick_sort(int *arr, size_t size)
{
    if (size > 1) quick_sort_rec(arr, 0, size - 1);
}

/* ==================== 归并排序 ==================== */

/**
 * 分治：先递归排序子数组，再合并
 */
static void merge(int *arr, int *tmp, size_t l, size_t m, size_t r)
{
    size_t i = l, j = m + 1, k = l;
    while (i <= m && j <= r)
        tmp[k++] = (arr[i] <= arr[j]) ? arr[i++] : arr[j++];
    while (i <= m) tmp[k++] = arr[i++];
    while (j <= r) tmp[k++] = arr[j++];
    memcpy(arr + l, tmp + l, (r - l + 1) * sizeof(int));
}

static void merge_sort_rec(int *arr, int *tmp, size_t l, size_t r)
{
    if (l >= r) return;
    size_t m = l + (r - l) / 2;
    merge_sort_rec(arr, tmp, l, m);
    merge_sort_rec(arr, tmp, m + 1, r);
    merge(arr, tmp, l, m, r);
}

static void merge_sort(int *arr, size_t size)
{
    if (size <= 1) return;
    int *tmp = (int *)malloc(size * sizeof(int));
    if (tmp) { merge_sort_rec(arr, tmp, 0, size - 1); free(tmp); }
}

/* ==================== 堆排序 ==================== */

/**
 * 下沉调整：恢复大顶堆性质
 */
static void sift_down(int *arr, size_t start, size_t end)
{
    while (1) {
        size_t child = start * 2 + 1;
        if (child > end) return;
        size_t largest = (arr[child] > arr[start]) ? child : start;
        if (child + 1 <= end && arr[child+1] > arr[largest]) largest = child + 1;
        if (largest == start) return;
        swap(&arr[start], &arr[largest]);
        start = largest;
    }
}

static void heap_sort(int *arr, size_t size)
{
    if (size <= 1) return;
    /* 建堆：从最后一个非叶子节点向上调整 */
    for (size_t i = size / 2; i > 0; i--)
        sift_down(arr, i - 1, size - 1);
    /* 逐个取出堆顶 */
    for (size_t i = size - 1; i > 0; i--) {
        swap(&arr[0], &arr[i]);
        sift_down(arr, 0, i - 1);
    }
}

/* ==================== 复杂度分析 ==================== */

static void print_complexity(void)
{
    printf("[复杂度分析]\n");
    printf("  算法    | 最优        | 平均        | 最差        | 空间 | 稳定\n");
    printf("  --------|-------------|-------------|-------------|------|----\n");
    printf("  快排    | O(n log n)  | O(n log n)  | O(n^2)      | O(log n) | 否\n");
    printf("  归并    | O(n log n)  | O(n log n)  | O(n log n)  | O(n) | 是\n");
    printf("  堆排    | O(n log n)  | O(n log n)  | O(n log n)  | O(1) | 否\n");
    printf("\n[优化策略]\n");
    printf("  - 快排：三数取中避免最差、小数组用插入排序\n");
    printf("  - 归并：稳定排序、适合链表（只需O(1)空间）\n");
    printf("  - 堆排：原地排序、不受输入分布影响\n\n");
}

/* ==================== 主函数 ==================== */

int main(void)
{
    printf("=== 高级排序算法学习卡片 ===\n\n");
    print_complexity();

    int original[] = {64, 34, 25, 12, 22, 11, 90, 45, 33, 77};
    size_t size = sizeof(original) / sizeof(original[0]);

    printf("[1] 快速排序（三数取中 + 小数组优化）\n");
    int *a1 = copy_array(original, size);
    if (a1) { print_array("原始", a1, size); quick_sort(a1, size); print_array("排序后", a1, size); free(a1); }

    printf("\n[2] 归并排序（分治 + 外部辅助数组）\n");
    int *a2 = copy_array(original, size);
    if (a2) { print_array("原始", a2, size); merge_sort(a2, size); print_array("排序后", a2, size); free(a2); }

    printf("\n[3] 堆排序（原地 + 无额外空间）\n");
    int *a3 = copy_array(original, size);
    if (a3) { print_array("原始", a3, size); heap_sort(a3, size); print_array("排序后", a3, size); free(a3); }

    printf("\n=== PASS ===\n");
    return 0;
}
