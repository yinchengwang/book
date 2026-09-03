/**
 * @file main.c
 * @brief 动态规划（DP）算法演示
 *
 * 演示三种经典 DP 问题：
 * 1. 斐波那契数列 - 递归 vs DP 优化
 * 2. 0-1 背包问题 - 选或不选
 * 3. 编辑距离 - 字符串转换最小代价
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ══════════════════════════════════════════════════════════════════════
 * 斐波那契数列
 * ══════════════════════════════════════════════════════════════════════ */

/** 递归版（指数级，重复计算） */
static long fib_recursive(int n)
{
    if (n <= 1) return (long)n;
    return fib_recursive(n - 1) + fib_recursive(n - 2);
}

/** DP 版（自底向上，O(n)） */
static long fib_dp(int n)
{
    if (n <= 1) return (long)n;
    long prev2 = 0, prev1 = 1, curr = 1;
    for (int i = 2; i <= n; i++) {
        curr = prev1 + prev2;
        prev2 = prev1;
        prev1 = curr;
    }
    return curr;
}

/** 演示斐波那契 */
static void demo_fibonacci(void)
{
    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  示例 1: 斐波那契数列\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    printf("\n  n      递归版          DP版\n");
    printf("  --------------------------------\n");
    for (int n = 0; n <= 15; n++) {
        printf("  %2d    %10ld    %10ld\n", n, fib_recursive(n), fib_dp(n));
    }
    printf("\n  DP 优化: 空间 O(1)，时间 O(n)，避免重复子问题\n");
}

/* ══════════════════════════════════════════════════════════════════════
 * 0-1 背包问题
 * ══════════════════════════════════════════════════════════════════════ */

/**
 * 0-1 背包 DP
 * @param weights 物品重量数组
 * @param values 物品价值数组
 * @param n 物品数量
 * @param capacity 背包容量
 * @return 最大价值
 */
static int knapsack_01(int *weights, int *values, int n, int capacity)
{
    /* dp[i][w] = 前 i 件物品在容量 w 下的最大价值 */
    int **dp = (int **)malloc((size_t)(n + 1) * sizeof(int *));
    for (int i = 0; i <= n; i++) {
        dp[i] = (int *)calloc((size_t)(capacity + 1), sizeof(int));
    }

    for (int i = 1; i <= n; i++) {
        for (int w = 0; w <= capacity; w++) {
            /* 不选第 i 件物品 */
            dp[i][w] = dp[i - 1][w];
            /* 选第 i 件物品（如果能装下） */
            if (w >= weights[i - 1]) {
                int val = dp[i - 1][w - weights[i - 1]] + values[i - 1];
                if (val > dp[i][w]) {
                    dp[i][w] = val;
                }
            }
        }
    }

    int result = dp[n][capacity];

    /* 输出 DP 表 */
    printf("\n  DP 表 (行=物品, 列=容量):\n  ");
    for (int w = 0; w <= capacity; w++) printf("%3d", w);
    printf("\n");
    for (int i = 0; i <= n; i++) {
        printf("  %2d:", i);
        for (int w = 0; w <= capacity; w++) {
            printf("%3d", dp[i][w]);
        }
        printf("\n");
    }

    /* 释放内存 */
    for (int i = 0; i <= n; i++) free(dp[i]);
    free(dp);

    return result;
}

/** 演示 0-1 背包 */
static void demo_knapsack(void)
{
    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  示例 2: 0-1 背包问题\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    /* 示例数据：4 件物品，背包容量 8 */
    int weights[] = {2, 3, 4, 5};
    int values[]  = {3, 4, 5, 6};
    int n = 4;
    int capacity = 8;

    printf("\n  物品:     重量  价值\n");
    for (int i = 0; i < n; i++) {
        printf("  [%d]        %d     %d\n", i + 1, weights[i], values[i]);
    }
    printf("  背包容量: %d\n", capacity);

    int max_value = knapsack_01(weights, values, n, capacity);
    printf("\n  最大价值: %d\n", max_value);
}

/* ══════════════════════════════════════════════════════════════════════
 * 编辑距离（Levenshtein Distance）
 * ══════════════════════════════════════════════════════════════════════ */

/**
 * 计算编辑距离（插入/删除/替换代价均为 1）
 * @param s1 源字符串
 * @param s2 目标字符串
 * @return 最少编辑操作次数
 */
static int edit_distance(const char *s1, const char *s2)
{
    int m = (int)strlen(s1);
    int n = (int)strlen(s2);

    /* dp[i][j] = s1[0..i-1] 到 s2[0..j-1] 的编辑距离 */
    int **dp = (int **)malloc((size_t)(m + 1) * sizeof(int *));
    for (int i = 0; i <= m; i++) {
        dp[i] = (int *)malloc((size_t)(n + 1) * sizeof(int));
        dp[i][0] = i;  /* 删除 i 个字符 */
    }
    for (int j = 0; j <= n; j++) {
        dp[0][j] = j;  /* 插入 j 个字符 */
    }

    for (int i = 1; i <= m; i++) {
        for (int j = 1; j <= n; j++) {
            if (s1[i - 1] == s2[j - 1]) {
                dp[i][j] = dp[i - 1][j - 1];  /* 字符相同，无需操作 */
            } else {
                /* 取插入、删除、替换的最小值 + 1 */
                int replace = dp[i - 1][j - 1] + 1;
                int insert = dp[i][j - 1] + 1;
                int delete = dp[i - 1][j] + 1;
                dp[i][j] = replace < insert ? (replace < delete ? replace : delete)
                                             : (insert < delete ? insert : delete);
            }
        }
    }

    /* 输出 DP 表 */
    printf("\n  DP 表:\n      ");
    for (int j = 0; j < n; j++) printf("   %c", s2[j]);
    printf("\n   ");
    for (int j = 0; j <= n; j++) printf("%3d", j);
    printf("\n");
    for (int i = 0; i <= m; i++) {
        printf("  %c ", i > 0 ? s1[i - 1] : ' ');
        for (int j = 0; j <= n; j++) {
            printf("%3d", dp[i][j]);
        }
        printf("\n");
    }

    int result = dp[m][n];

    for (int i = 0; i <= m; i++) free(dp[i]);
    free(dp);

    return result;
}

/** 演示编辑距离 */
static void demo_edit_distance(void)
{
    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  示例 3: 编辑距离 (Levenshtein Distance)\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    const char *tests[][2] = {
        {"kitten", "sitting"},
        {"flaw",   "lawn"},
        {"algorithm", "altruistic"}
    };

    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        const char *s1 = tests[i][0];
        const char *s2 = tests[i][1];
        printf("\n  \"%s\" → \"%s\"\n", s1, s2);
        int dist = edit_distance(s1, s2);
        printf("  编辑距离: %d\n", dist);
    }
}

/* ══════════════════════════════════════════════════════════════════════
 * 主函数
 * ══════════════════════════════════════════════════════════════════════ */

int main(void)
{
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║              动态规划 (Dynamic Programming)                    ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");

    demo_fibonacci();
    demo_knapsack();
    demo_edit_distance();

    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  DP 核心思想:\n");
    printf("    1. 最优子结构 - 问题最优解由子问题最优解构成\n");
    printf("    2. 重叠子问题 - 子问题空间需足够小（可复用）\n");
    printf("    3. 状态转移   - dp[i] = f(dp[j]) 建立递推关系\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    return 0;
}
