/**
 * dynamic_programming/main.c —— 动态规划经典问题
 *
 * 功能：
 *   1. 爬楼梯（基础 DP）
 *   2. 编辑距离（字符串 DP）
 *   3. 股票买卖最佳时机（状态机 DP）
 *
 * 编译：make all
 * 运行：make run
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========== 1. 爬楼梯（基础 DP） ========== */

/**
 * climb_stairs - 爬 n 阶楼梯，每次走 1 或 2 阶，求方法总数
 * @n: 楼梯阶数
 *
 * 递推公式：dp[i] = dp[i-1] + dp[i-2]
 * 优化：只用两个滚动变量，空间 O(1)
 */
int climb_stairs(int n) {
    if (n <= 2)
        return n;

    int prev2 = 1;  /* dp[i-2] */
    int prev1 = 2;  /* dp[i-1] */
    int curr  = 0;

    for (int i = 3; i <= n; i++) {
        curr  = prev1 + prev2;
        prev2 = prev1;
        prev1 = curr;
    }
    return curr;
}

/* ========== 2. 编辑距离（字符串 DP） ========== */

/**
 * min3 - 返回三个整数的最小值（辅助函数）
 */
static int min3(int a, int b, int c) {
    int min = a < b ? a : b;
    return min < c ? min : c;
}

/**
 * edit_distance - 计算将 word1 转换为 word2 的最小编辑步数
 * @word1: 源字符串
 * @word2: 目标字符串
 *
 * 操作：插入、删除、替换各算一步
 * 递推公式：
 *   dp[i][j] = dp[i-1][j-1]              (word1[i]==word2[j])
 *   dp[i][j] = 1 + min(dp[i-1][j],       删除
 *                       dp[i][j-1],       插入
 *                       dp[i-1][j-1])     替换
 */
int edit_distance(const char *word1, const char *word2) {
    int m = (int)strlen(word1);
    int n = (int)strlen(word2);

    /* 二维 DP 表 */
    int **dp = (int **)malloc((m + 1) * sizeof(int *));
    for (int i = 0; i <= m; i++)
        dp[i] = (int *)calloc((n + 1), sizeof(int));

    /* 初始化边界 */
    for (int i = 0; i <= m; i++) dp[i][0] = i;
    for (int j = 0; j <= n; j++) dp[0][j] = j;

    /* 填表 */
    for (int i = 1; i <= m; i++) {
        for (int j = 1; j <= n; j++) {
            if (word1[i - 1] == word2[j - 1])
                dp[i][j] = dp[i - 1][j - 1];
            else
                dp[i][j] = 1 + min3(dp[i - 1][j],     /* 删除 */
                                    dp[i][j - 1],     /* 插入 */
                                    dp[i - 1][j - 1]  /* 替换 */
                );
        }
    }

    int result = dp[m][n];

    for (int i = 0; i <= m; i++)
        free(dp[i]);
    free(dp);

    return result;
}

/* ========== 3. 股票买卖最佳时机（状态机 DP） ========== */

/**
 * max_profit - 股票买卖最佳时机（最多交易 2 笔）
 * @prices: 每日价格数组
 * @size:   数组长度
 *
 * 状态机思路：
 *   - 第一次买入后的最大收益  buy1
 *   - 第一次卖出后的最大收益  sell1
 *   - 第二次买入后的最大收益  buy2
 *   - 第二次卖出后的最大收益  sell2
 */
int max_profit(const int *prices, int size) {
    if (size < 2)
        return 0;

    /* 初始化：第一天只能买入 */
    int buy1  = -prices[0];
    int sell1 = 0;
    int buy2  = -prices[0];
    int sell2 = 0;

    for (int i = 1; i < size; i++) {
        /* 第一次买卖 */
        buy1  = buy1  > -prices[i] ? buy1  : -prices[i];
        sell1 = sell1 > buy1 + prices[i] ? sell1 : buy1 + prices[i];
        /* 第二次买卖（依赖第一次的收益） */
        buy2  = buy2  > sell1 - prices[i] ? buy2 : sell1 - prices[i];
        sell2 = sell2 > buy2 + prices[i] ? sell2 : buy2 + prices[i];
    }

    return sell2 > sell1 ? sell2 : sell1;
}

/* ========== 测试 ========== */

static void test_climb_stairs(void) {
    printf("【爬楼梯】\n");
    for (int n = 1; n <= 10; n++)
        printf("  n=%2d → %2d 种方法\n", n, climb_stairs(n));
}

static void test_edit_distance(void) {
    printf("\n【编辑距离】\n");
    printf("  'kitten' → 'sitting': %d\n", edit_distance("kitten", "sitting"));
    printf("  'abc'    → 'abc':     %d\n", edit_distance("abc", "abc"));
    printf("  ''       → 'abc':     %d\n", edit_distance("", "abc"));
}

static void test_max_profit(void) {
    int prices1[] = {3, 3, 5, 0, 0, 3, 1, 4};
    int prices2[] = {7, 6, 4, 3, 1};
    int prices3[] = {1, 2, 3, 4, 5};

    printf("\n【股票买卖（最多 2 笔）】\n");
    printf("  [3,3,5,0,0,3,1,4] → %d\n", max_profit(prices1, 8));
    printf("  [7,6,4,3,1]       → %d\n", max_profit(prices2, 5));
    printf("  [1,2,3,4,5]       → %d\n", max_profit(prices3, 5));
}

int main(void) {
    printf("========== 动态规划经典问题 ==========\n\n");
    test_climb_stairs();
    test_edit_distance();
    test_max_profit();
    printf("\n========== 演示结束 ==========\n");
    return 0;
}
