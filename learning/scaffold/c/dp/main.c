/*
 * dp scaffold — 动态规划三范式
 *
 * 演示三种经典 DP 范式，每种打印 DP 表格的填充过程：
 *   1. 斐波那契 — 记忆化 vs 递推 vs 朴素递归（计数器对比）
 *   2. 0/1 背包 — 完整 DP 表 + 最优解回溯
 *   3. LCS（最长公共子序列）— DP 表 + 回溯路径
 *
 * 复现：
 *   gcc -Wall -Wextra -O2 -std=c11 -o dp_demo main.c
 *   ./dp_demo
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================== 1. Fibonacci — 三种方案对比 ==================== */

int fib_naive_count = 0;  /* 朴素递归调用计数器 */

int fib_naive(int n) {
    fib_naive_count++;
    if (n <= 1) return n;
    return fib_naive(n - 1) + fib_naive(n - 2);
}

/* 记忆化（自顶向下） */
int fib_memo_inner(int n, int *memo, int *depth) {
    if (memo[n] != -1) return memo[n];
    printf("[fib memo] computing fib(%d)...\n", n);
    memo[n] = fib_memo_inner(n - 1, memo, depth) +
              fib_memo_inner(n - 2, memo, depth);
    printf("[fib memo] fib(%d) = %d\n", n, memo[n]);
    return memo[n];
}

void fib_memo_demo(int n) {
    int *memo = (int *)malloc((size_t)(n + 1) * sizeof(int));
    for (int i = 0; i <= n; i++) memo[i] = -1;
    memo[0] = 0; memo[1] = 1;
    printf("[fib memo] cache hit: fib(0)=0, fib(1)=1\n");
    int result = fib_memo_inner(n, memo, NULL);
    printf("[fib memo] result: fib(%d) = %d\n", n, result);
    free(memo);
}

/* 递推（自底向上） */
void fib_bottomup(int n) {
    if (n <= 1) {
        printf("[fib bottomup] fib(%d) = %d\n", n, n);
        return;
    }
    int prev2 = 0, prev1 = 1, curr;
    printf("[fib bottomup] dp[0]=0, dp[1]=1\n");
    for (int i = 2; i <= n; i++) {
        curr = prev1 + prev2;
        printf("[fib bottomup] dp[%d] = dp[%d]+dp[%d] = %d+%d = %d\n",
               i, i - 1, i - 2, prev1, prev2, curr);
        prev2 = prev1;
        prev1 = curr;
    }
    printf("[fib bottomup] result: fib(%d) = %d\n", n, curr);
}

/* ==================== 2. 0/1 背包 ==================== */

void print_dp_table(const char *tag, int **dp, int rows, int cols,
                    const int *wt, const int *val) {
    (void)wt; (void)val;  /* wt/val 仅在 knapsack 调用时非 NULL */
    printf("[%s] DP table (%d items x capacity %d):\n", tag, rows - 1, cols - 1);
    /* 表头 */
    printf("[%s]        ", tag);
    for (int w = 0; w < cols; w++) printf("w=%-3d", w);
    printf("\n");
    for (int i = 0; i < rows; i++) {
        if (i == 0)
            printf("[%s] i=0  ", tag);
        else
            printf("[%s] i=%-2d ", tag, i);
        for (int w = 0; w < cols; w++)
            printf("%-5d", dp[i][w]);
        if (i > 0 && wt && val)
            printf(" (wt=%d, val=%d)", wt[i - 1], val[i - 1]);
        printf("\n");
    }
}

void knapsack_01(const int *wt, const int *val, int n, int W) {
    int **dp = (int **)malloc((size_t)(n + 1) * sizeof(int *));
    for (int i = 0; i <= n; i++) {
        dp[i] = (int *)calloc((size_t)(W + 1), sizeof(int));
    }

    printf("[knapsack] items: ");
    for (int i = 0; i < n; i++)
        printf("(#%d wt=%d val=%d) ", i + 1, wt[i], val[i]);
    printf("\n[knapsack] capacity: %d\n", W);

    for (int i = 1; i <= n; i++) {
        for (int w = 0; w <= W; w++) {
            if (wt[i - 1] <= w) {
                int take = val[i - 1] + dp[i - 1][w - wt[i - 1]];
                int skip = dp[i - 1][w];
                dp[i][w] = (take > skip) ? take : skip;
                if (take > skip)
                    printf("[knapsack] dp[%d][%d] = max(dp[%d][%d]=%d, "
                           "val[%d]+dp[%d][%d-wt[%d]]=%d+%d) = %d ← TAKE\n",
                           i, w, i - 1, w, skip, i, i - 1, w, i - 1,
                           val[i - 1], dp[i - 1][w - wt[i - 1]], dp[i][w]);
            } else {
                dp[i][w] = dp[i - 1][w];
                printf("[knapsack] dp[%d][%d] = dp[%d][%d] = %d (item %d too heavy)\n",
                       i, w, i - 1, w, dp[i][w], i);
            }
        }
    }

    print_dp_table("knapsack", dp, n + 1, W + 1, wt, val);

    /* 回溯最优解 */
    printf("[knapsack] optimal selection (backtrack): ");
    int w = W;
    for (int i = n; i > 0; i--) {
        if (dp[i][w] != dp[i - 1][w]) {
            printf("#%d ", i);
            w -= wt[i - 1];
        }
    }
    printf("\n[knapsack] max value = %d\n", dp[n][W]);

    for (int i = 0; i <= n; i++) free(dp[i]);
    free(dp);
}

/* ==================== 3. LCS（最长公共子序列） ==================== */

void lcs(const char *x, const char *y) {
    int m = (int)strlen(x), n = (int)strlen(y);
    int **dp = (int **)malloc((size_t)(m + 1) * sizeof(int *));
    for (int i = 0; i <= m; i++)
        dp[i] = (int *)calloc((size_t)(n + 1), sizeof(int));

    printf("[lcs] X=\"%s\" (len=%d), Y=\"%s\" (len=%d)\n", x, m, y, n);

    for (int i = 1; i <= m; i++) {
        for (int j = 1; j <= n; j++) {
            if (x[i - 1] == y[j - 1]) {
                dp[i][j] = dp[i - 1][j - 1] + 1;
                printf("[lcs] dp[%d][%d] = dp[%d][%d]+1 = %d "
                       "(match X[%d]='%c'==Y[%d]='%c') ↖\n",
                       i, j, i - 1, j - 1, dp[i][j],
                       i - 1, x[i - 1], j - 1, y[j - 1]);
            } else {
                int up = dp[i - 1][j], left = dp[i][j - 1];
                dp[i][j] = (up > left) ? up : left;
                printf("[lcs] dp[%d][%d] = max(dp[%d][%d]=%d, dp[%d][%d]=%d) = %d %s\n",
                       i, j, i - 1, j, up, i, j - 1, left, dp[i][j],
                       (up > left) ? "↑" : (left > up) ? "←" : "↑/←");
            }
        }
    }

    print_dp_table("lcs", dp, m + 1, n + 1, NULL, NULL);

    /* 回溯 LCS */
    int len = dp[m][n];
    char *result = (char *)malloc((size_t)(len + 1));
    result[len] = '\0';
    int i = m, j = n, idx = len - 1;
    printf("[lcs] backtrack: ");
    while (i > 0 && j > 0) {
        if (x[i - 1] == y[j - 1]) {
            result[idx--] = x[i - 1];
            printf("%c ← ", x[i - 1]);
            i--; j--;
        } else if (dp[i - 1][j] > dp[i][j - 1]) {
            printf("↑ ");
            i--;
        } else {
            printf("← ");
            j--;
        }
    }
    printf("\n[lcs] result: \"%s\" (length=%d)\n", result, len);

    free(result);
    for (int k = 0; k <= m; k++) free(dp[k]);
    free(dp);
}

/* ==================== main ==================== */

int main(void) {
    printf("=== 动态规划三范式演示 ===\n\n");

    /* —— Fibonacci —— */
    printf("--- 1. Fibonacci(10) 三种方案对比 ---\n");
    fib_naive_count = 0;
    int fn = fib_naive(10);
    printf("[fib naive] fib(10)=%d, total calls=%d (O(2^n) exponential!)\n\n",
           fn, fib_naive_count);

    fib_memo_demo(10);
    printf("\n");

    fib_bottomup(10);
    printf("\n");

    /* —— 0/1 Knapsack —— */
    printf("--- 2. 0/1 背包 (4 items, capacity=7) ---\n");
    int wt[]  = {1, 3, 4, 5};
    int val[] = {1, 4, 5, 7};
    knapsack_01(wt, val, 4, 7);
    printf("\n");

    /* —— LCS —— */
    printf("--- 3. LCS 最长公共子序列 ---\n");
    lcs("ABCBDAB", "BDCABA");
    printf("\n");

    printf("=== PASS ===\n");
    return 0;
}
