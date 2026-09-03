/* sort_basic scaffold — 冒泡/选择/插入排序 + 每轮中间状态 */

#include <stdio.h>
#include <stdbool.h>

static void print_arr(const char *tag, const int a[], int n) {
    printf("%s", tag);
    for (int i = 0; i < n; i++) printf("%d ", a[i]);
    printf("\n");
}

static void swap(int *x, int *y) { int t = *x; *x = *y; *y = t; }

/* ── 1. 冒泡排序 ── */
static void bubble_sort(int a[], int n, int *cmp_cnt, int *swap_cnt) {
    *cmp_cnt = *swap_cnt = 0;
    for (int i = 0; i < n - 1; i++) {
        bool swapped = false;
        for (int j = 0; j < n - 1 - i; j++) {
            (*cmp_cnt)++;
            if (a[j] > a[j + 1]) {
                swap(&a[j], &a[j + 1]);
                (*swap_cnt)++;
                swapped = true;
            }
        }
        printf("  [bubble pass %d] ", i + 1);
        for (int k = 0; k < n; k++) printf("%d ", a[k]);
        printf("\n");
        if (!swapped) break;      /* 提前终止优化 */
    }
}

/* ── 2. 选择排序 ── */
static void selection_sort(int a[], int n, int *cmp_cnt, int *swap_cnt) {
    *cmp_cnt = *swap_cnt = 0;
    for (int i = 0; i < n - 1; i++) {
        int min_idx = i;
        for (int j = i + 1; j < n; j++) {
            (*cmp_cnt)++;
            if (a[j] < a[min_idx]) min_idx = j;
        }
        if (min_idx != i) {
            swap(&a[i], &a[min_idx]);
            (*swap_cnt)++;
        }
        printf("  [select pass %d] ", i + 1);
        for (int k = 0; k < n; k++) printf("%d ", a[k]);
        printf("\n");
    }
}

/* ── 3. 插入排序 ── */
static void insertion_sort(int a[], int n, int *cmp_cnt, int *move_cnt) {
    *cmp_cnt = *move_cnt = 0;
    for (int i = 1; i < n; i++) {
        int key = a[i];
        int j = i - 1;
        while (j >= 0) {
            (*cmp_cnt)++;
            if (a[j] > key) {
                a[j + 1] = a[j];
                (*move_cnt)++;
                j--;
            } else break;
        }
        a[j + 1] = key;
        printf("  [insert pass %d] ", i);
        for (int k = 0; k < n; k++) printf("%d ", a[k]);
        printf("\n");
    }
}

static void copy_arr(const int src[], int dst[], int n) {
    for (int i = 0; i < n; i++) dst[i] = src[i];
}

int main(void) {
    int orig[] = {5, 3, 8, 4, 2, 7, 1, 6};
    int n = sizeof(orig) / sizeof(orig[0]);
    int arr[8];
    int cmp, swp, mov;

    print_arr("原始数组:       ", orig, n);

    /* 冒泡 */
    printf("\n=== 冒泡排序 (Bubble Sort) ===\n");
    copy_arr(orig, arr, n);
    bubble_sort(arr, n, &cmp, &swp);
    print_arr("最终:           ", arr, n);
    printf("比较 %d 次, 交换 %d 次 | 稳定: 是 | O(n^2)\n", cmp, swp);

    /* 选择 */
    printf("\n=== 选择排序 (Selection Sort) ===\n");
    copy_arr(orig, arr, n);
    selection_sort(arr, n, &cmp, &swp);
    print_arr("最终:           ", arr, n);
    printf("比较 %d 次, 交换 %d 次 | 稳定: 否 | O(n^2)\n", cmp, swp);

    /* 插入 */
    printf("\n=== 插入排序 (Insertion Sort) ===\n");
    copy_arr(orig, arr, n);
    insertion_sort(arr, n, &cmp, &mov);
    print_arr("最终:           ", arr, n);
    printf("比较 %d 次, 移动 %d 次 | 稳定: 是 | O(n^2), 近乎有序时 O(n)\n", cmp, mov);

    printf("\n=== PASS ===\n");
    return 0;
}
