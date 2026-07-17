/* sort_advanced scaffold — 快排(三数取中)/归并排序/堆排序/计数排序 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static void print_arr(const char *tag, const int a[], int n) {
    printf("%s", tag);
    for (int i = 0; i < n; i++) printf("%d ", a[i]);
    printf("\n");
}

static void swap(int *x, int *y) { int t = *x; *x = *y; *y = t; }

/* ══════════════════════════════════════════════════════════════
 * 1. 快速排序 — 三数取中 + 原地 partition
 * ══════════════════════════════════════════════════════════════ */
static int median3(int a[], int lo, int hi) {
    int mid = lo + (hi - lo) / 2;
    if (a[lo] > a[mid]) swap(&a[lo], &a[mid]);
    if (a[lo] > a[hi])  swap(&a[lo], &a[hi]);
    if (a[mid] > a[hi]) swap(&a[mid], &a[hi]);
    swap(&a[mid], &a[hi]);   /* 中值放到 hi 位置 */
    return a[hi];
}

static int partition(int a[], int lo, int hi, int *cmp) {
    int pivot = median3(a, lo, hi);
    int i = lo - 1;
    for (int j = lo; j < hi; j++) {
        (*cmp)++;
        if (a[j] <= pivot) { i++; swap(&a[i], &a[j]); }
    }
    swap(&a[i + 1], &a[hi]);
    return i + 1;
}

static void quicksort_impl(int a[], int lo, int hi, int depth, int *cmp) {
    if (lo >= hi) return;
    /* 小数组退化为插入排序（门限值 10） */
    if (hi - lo < 10) {
        for (int i = lo + 1; i <= hi; i++) {
            int key = a[i], j = i - 1;
            while (j >= lo && a[j] > key) { a[j+1] = a[j]; j--; (*cmp)++; }
            a[j+1] = key;
        }
        return;
    }
    int pi = partition(a, lo, hi, cmp);
    printf("  [qsort depth=%d] pivot=%d → ", depth, a[pi]);
    for (int k = lo; k <= hi; k++) printf("%d ", a[k]);
    printf("\n");
    quicksort_impl(a, lo, pi - 1, depth + 1, cmp);
    quicksort_impl(a, pi + 1, hi, depth + 1, cmp);
}

static void quicksort(int a[], int n, int *cmp) {
    *cmp = 0;
    quicksort_impl(a, 0, n - 1, 0, cmp);
}

/* ══════════════════════════════════════════════════════════════
 * 2. 归并排序 — 自顶向下
 * ══════════════════════════════════════════════════════════════ */
static void merge(int a[], int tmp[], int lo, int mid, int hi, int *cmp) {
    int i = lo, j = mid + 1, k = lo;
    while (i <= mid && j <= hi) {
        (*cmp)++;
        tmp[k++] = (a[i] <= a[j]) ? a[i++] : a[j++];
    }
    while (i <= mid) tmp[k++] = a[i++];
    while (j <= hi)  tmp[k++] = a[j++];
    for (i = lo; i <= hi; i++) a[i] = tmp[i];
}

static void mergesort_impl(int a[], int tmp[], int lo, int hi, int depth, int *cmp) {
    if (lo >= hi) return;
    int mid = lo + (hi - lo) / 2;
    mergesort_impl(a, tmp, lo, mid, depth + 1, cmp);
    mergesort_impl(a, tmp, mid + 1, hi, depth + 1, cmp);
    merge(a, tmp, lo, mid, hi, cmp);
    printf("  [merge depth=%d] lo=%d hi=%d → ", depth, lo, hi);
    for (int k = lo; k <= hi; k++) printf("%d ", a[k]);
    printf("\n");
}

static void mergesort(int a[], int n, int *cmp) {
    *cmp = 0;
    int *tmp = (int*)malloc((size_t)n * sizeof(int));
    mergesort_impl(a, tmp, 0, n - 1, 0, cmp);
    free(tmp);
}

/* ══════════════════════════════════════════════════════════════
 * 3. 堆排序
 * ══════════════════════════════════════════════════════════════ */
static void heap_sift_down(int a[], int n, int idx, int *cmp) {
    while (1) {
        int largest = idx;
        int left  = 2 * idx + 1;
        int right = 2 * idx + 2;
        if (left  < n) { (*cmp)++; if (a[left]  > a[largest]) largest = left; }
        if (right < n) { (*cmp)++; if (a[right] > a[largest]) largest = right; }
        if (largest == idx) break;
        swap(&a[idx], &a[largest]);
        idx = largest;
    }
}

static void heapsort(int a[], int n, int *cmp) {
    *cmp = 0;
    /* 建堆 — 从最后一个非叶节点开始 sift-down */
    for (int i = n / 2 - 1; i >= 0; i--)
        heap_sift_down(a, n, i, cmp);
    printf("  [heap] 建堆完成: ");
    for (int i = 0; i < n; i++) printf("%d ", a[i]);
    printf("\n");
    /* 取堆顶 → 放末尾 → sift-down */
    for (int i = n - 1; i > 0; i--) {
        swap(&a[0], &a[i]);
        heap_sift_down(a, i, 0, cmp);
        printf("  [heap pass %d] ", n - i);
        for (int k = 0; k < n; k++) printf("%d ", a[k]);
        printf("\n");
    }
}

/* ══════════════════════════════════════════════════════════════
 * 4. 计数排序（仅适用于有限范围整数）
 * ══════════════════════════════════════════════════════════════ */
static void countingsort(int a[], int n, int range) {
    int *count = (int*)calloc((size_t)range + 1, sizeof(int));
    for (int i = 0; i < n; i++) count[a[i]]++;
    printf("  [count] 频次: ");
    for (int i = 0; i <= range; i++)
        if (count[i]) printf("%d:%d ", i, count[i]);
    printf("\n");
    int idx = 0;
    for (int i = 0; i <= range; i++)
        for (int j = 0; j < count[i]; j++)
            a[idx++] = i;
    free(count);
}

static void copy_arr(const int src[], int dst[], int n) {
    for (int i = 0; i < n; i++) dst[i] = src[i];
}

int main(void) {
    int qs_orig[] = {7, 2, 1, 8, 6, 3, 5, 4};
    int n = sizeof(qs_orig) / sizeof(qs_orig[0]);
    int arr[8], cmp;

    /* 快排 */
    printf("=== 快速排序 (Quicksort, 三数取中) ===\n");
    copy_arr(qs_orig, arr, n);
    print_arr("原始: ", arr, n);
    quicksort(arr, n, &cmp);
    print_arr("最终: ", arr, n);
    printf("比较 %d 次 | O(n log n) 平均, O(n^2) 最坏 (三数取中缓解)\n", cmp);

    /* 归并 */
    printf("\n=== 归并排序 (Merge Sort) ===\n");
    copy_arr(qs_orig, arr, n);
    mergesort(arr, n, &cmp);
    print_arr("最终: ", arr, n);
    printf("比较 %d 次 | 稳定 | O(n log n) 严格 | 需 O(n) 辅助空间\n", cmp);

    /* 堆排 */
    printf("\n=== 堆排序 (Heap Sort) ===\n");
    copy_arr(qs_orig, arr, n);
    heapsort(arr, n, &cmp);
    print_arr("最终: ", arr, n);
    printf("比较 %d 次 | 不稳定 | O(n log n) 严格 | 原地排序\n", cmp);

    /* 计数 */
    printf("\n=== 计数排序 (Counting Sort) ===\n");
    int cs_arr[] = {3, 1, 4, 1, 5, 3, 2, 4};
    int cs_n = sizeof(cs_arr) / sizeof(cs_arr[0]);
    print_arr("原始: ", cs_arr, cs_n);
    countingsort(cs_arr, cs_n, 5);
    print_arr("最终: ", cs_arr, cs_n);
    printf("稳定(反向填充时) | O(n+k) | 仅适用于整数范围已知\n");

    printf("\n=== PASS ===\n");
    return 0;
}
