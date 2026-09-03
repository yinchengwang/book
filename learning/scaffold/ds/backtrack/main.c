/**
 * @file main.c
 * @brief 回溯算法：N 皇后 + 全排列 + 子集生成
 *
 * 演示三种经典回溯问题：
 * 1. N 皇后 - 约束满足问题
 * 2. 全排列 - 字典序生成
 * 3. 子集生成 - 二进制位枚举
 */

#include <stdio.h>
#include <stdlib.h>

#define N 4  /* 默认 4 皇后 */

/* ══════════════════════════════════════════════════════════════════════
 * 1. N 皇后问题
 * ══════════════════════════════════════════════════════════════════════ */

static int queens[N];  /* queens[i] = j 表示第 i 行皇后在第 j 列 */

/** 检查 (row, col) 能否放皇后 */
static int can_place(int row, int col)
{
    for (int i = 0; i < row; i++) {
        int j = queens[i];
        if (j == col) return 0;           /* 同列 */
        if (i - j == row - col) return 0; /* 主对角线 */
        if (i + j == row + col) return 0; /* 副对角线 */
    }
    return 1;
}

/** 递归放置皇后 */
static void place_queen(int row, int *count)
{
    if (row == N) {
        (*count)++;
        printf("  解 %2d: ", *count);
        for (int i = 0; i < N; i++) printf("%d ", queens[i]);
        printf("\n");
        return;
    }
    for (int col = 0; col < N; col++) {
        if (can_place(row, col)) {
            queens[row] = col;
            place_queen(row + 1, count);
        }
    }
}

static void demo_n_queen(void)
{
    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  [N 皇后] N=%d\n", N);
    printf("═══════════════════════════════════════════════════════════════\n");
    int count = 0;
    place_queen(0, &count);
    printf("  共 %d 个解\n", count);
}

/* ══════════════════════════════════════════════════════════════════════
 * 2. 全排列（字典序）
 * ══════════════════════════════════════════════════════════════════════ */

#define MAX_A 4

static int arr[MAX_A] = {1, 2, 3, 4};

/** 交换两个元素 */
static void swap(int *a, int *b)
{
    int t = *a; *a = *b; *b = t;
}

/** 生成下一个排列（标准库思路） */
static int next_perm(int *a, int n)
{
    /* 1. 从右向左找第一个降序对 */
    int i = n - 2;
    while (i >= 0 && a[i] >= a[i + 1]) i--;
    if (i < 0) return 0;  /* 已是最末排列 */
    /* 2. 从右向左找第一个大于 a[i] 的 */
    int j = n - 1;
    while (a[j] <= a[i]) j--;
    /* 3. 交换并反转后缀 */
    swap(&a[i], &a[j]);
    for (int l = i + 1, r = n - 1; l < r; l++, r--) swap(&a[l], &a[r]);
    return 1;
}

static void demo_permutation(void)
{
    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  [全排列] 数组: 1 2 3 4\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    int n = MAX_A;
    int count = 0;
    do {
        printf("  %2d: ", ++count);
        for (int i = 0; i < n; i++) printf("%d ", arr[i]);
        printf("\n");
    } while (next_perm(arr, n));
}

/* ══════════════════════════════════════════════════════════════════════
 * 3. 子集生成（二进制枚举）
 * ══════════════════════════════════════════════════════════════════════ */

/** 打印子集（对应掩码 mask） */
static void print_subset(const int *arr, int n, int mask)
{
    printf("  { ");
    for (int i = 0; i < n; i++)
        if (mask & (1 << i)) printf("%d ", arr[i]);
    printf("}\n");
}

static void demo_subset(void)
{
    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  [子集生成] 数组: 1 2 3\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    int n = 3;
    int arr[3] = {1, 2, 3};
    int total = 1 << n;  /* 2^n 个子集 */
    for (int mask = 0; mask < total; mask++) {
        print_subset(arr, n, mask);
    }
}

/* ══════════════════════════════════════════════════════════════════════
 * 主函数
 * ══════════════════════════════════════════════════════════════════════ */

int main(void)
{
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║          回溯算法: N皇后 / 全排列 / 子集生成                ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");

    demo_n_queen();
    demo_permutation();
    demo_subset();

    printf("\n═══════════════════════════════════════════════════════════════\n");
    printf("  回溯三要素:\n");
    printf("    1. 选择 (Choose): 做出选择\n");
    printf("    2. 探索 (Explore): 递归搜索\n");
    printf("    3. 撤销 (Unchoose): 回溯恢复\n");
    printf("  复杂度: O(N!) / O(2^N)\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    return 0;
}
