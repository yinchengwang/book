/**
 * @file main.c
 * @brief 区间 DP：区间合并 + 矩阵链乘 + 石子合并
 *
 * 演示三种经典区间 DP 问题：
 * 1. 区间合并调度 - 最优合并顺序
 * 2. 矩阵链乘 - 最优括号化
 * 3. 石子合并 - 环形合并游戏
 */

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#define MAX_N 6

/* ══════════════════════════════════════════════════════════════════════
 * 1. 区间合并调度（哈夫曼思想简化版）
 * ══════════════════════════════════════════════════════════════════════ */

/**
 * 最优合并成本 - 贪心近似
 * 合并最小的两堆，直到只剩一堆
 */
static int merge_cost(int *arr, int n)
{
    printf("  合并过程: ");
    int cost = 0;
    for (int i = 0; i < n; i++) printf("%d ", arr[i]);
    printf("\n");

    int temp[MAX_N];
    for (int i = 0; i < n - 1; i++) {
        /* 找最小的两个 */
        int min1 = 0, min2 = 1;
        if (arr[min1] > arr[min2]) { int t = min1; min1 = min2; min2 = t; }
        for (int j = 2; j < n - i; j++) {
            if (arr[j] < arr[min1]) { min2 = min1; min1 = j; }
            else if (arr[j] < arr[min2]) { min2 = j; }
        }
        int merged = arr[min1] + arr[min2];
        cost += merged;
        printf("    合并 %d + %d = %d, 累计: %d\n", arr[min1], arr[min2], merged, cost);
        /* 替换为合并后的值 */
        arr[min1] = merged;
        arr[min2] = arr[n - i - 1];
    }
    return cost;
}

static void demo_merge_intervals(void)
{
    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  [区间合并调度]\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    int arr[] = {4, 3, 2, 6};
    int n = 4;
    int cost = merge_cost(arr, n);
    printf("  最优合并成本: %d\n", cost);
}

/* ══════════════════════════════════════════════════════════════════════
 * 2. 矩阵链乘最优括号化
 * ══════════════════════════════════════════════════════════════════════ */

/**
 * 矩阵链乘最优成本
 * p[i-1] x p[i] 大小的矩阵 i
 */
static int matrix_chain_dp(int *p, int n)
{
    /* dp[i][j] = 矩阵 i 到 j 的最小乘法次数 */
    int dp[MAX_N][MAX_N] = {0};
    int m, j;

    /* length = 链的长度 */
    for (int len = 2; len <= n; len++) {
        for (int i = 1; i <= n - len + 1; i++) {
            j = i + len - 1;
            dp[i][j] = INT_MAX;
            for (m = i; m < j; m++) {
                int q = dp[i][m] + dp[m + 1][j] + p[i - 1] * p[m] * p[j];
                if (q < dp[i][j]) dp[i][j] = q;
            }
        }
    }
    return dp[1][n];
}

static void demo_matrix_chain(void)
{
    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  [矩阵链乘最优括号化]\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    /* 矩阵链: A1(10x30) A2(30x5) A3(5x15) A4(15x10) */
    int p[] = {10, 30, 5, 15, 10};
    int n = 4;
    printf("  矩阵链维度: ");
    for (int i = 0; i < n; i++) printf("(A%d: %dx%d) ", i + 1, p[i], p[i + 1]);
    printf("\n");
    int cost = matrix_chain_dp(p, n);
    printf("  最优乘法次数: %d\n", cost);
}

/* ══════════════════════════════════════════════════════════════════════
 * 3. 石子合并（环形）
 * ══════════════════════════════════════════════════════════════════════ */

#define INF 1e9

/**
 * 线性石子合并 DP
 * 合并相邻两堆石子，成本为石子数量和
 */
static int stone_merge(int *arr, int n)
{
    /* dp[i][j] = 合并 i~j 堆的最小成本 */
    int dp[MAX_N][MAX_N] = {0};
    int sum[MAX_N][MAX_N] = {0};

    /* 前缀和 */
    for (int i = 0; i < n; i++)
        for (int j = i; j < n; j++)
            for (int k = i; k <= j; k++)
                sum[i][j] += arr[k];

    /* len = 区间长度 */
    for (int len = 2; len <= n; len++) {
        for (int i = 0; i <= n - len; i++) {
            int j = i + len - 1;
            dp[i][j] = INF;
            for (int k = i; k < j; k++) {
                int cost = dp[i][k] + dp[k + 1][j] + sum[i][j];
                if (cost < dp[i][j]) dp[i][j] = cost;
            }
        }
    }
    return dp[0][n - 1];
}

static void demo_stone_merge(void)
{
    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  [石子合并]\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    int arr[] = {3, 4, 5, 2, 1};
    int n = 5;
    printf("  石子堆: ");
    for (int i = 0; i < n; i++) printf("%d ", arr[i]);
    printf("\n");
    int cost = stone_merge(arr, n);
    printf("  最小合并成本: %d\n", cost);
}

/* ══════════════════════════════════════════════════════════════════════
 * 主函数
 * ══════════════════════════════════════════════════════════════════════ */

int main(void)
{
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║          区间 DP: 合并调度 / 矩阵链乘 / 石子合并           ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");

    demo_merge_intervals();
    demo_matrix_chain();
    demo_stone_merge();

    printf("\n═══════════════════════════════════════════════════════════════\n");
    printf("  区间 DP 特征:\n");
    printf("    dp[i][j] = min(dp[i][k] + dp[k+1][j] + cost(i,j))\n");
    printf("    时间复杂度: O(N^3)\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    return 0;
}
