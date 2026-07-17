/**
 * @file main.c
 * @brief 位运算 DP：状态压缩 DP 与 TSP（旅行商）
 *
 * 演示内容：
 * 1. 状态压缩 DP 基础：枚举子集
 * 2. TSP 问题：O(n^2 * 2^n) 动态规划
 * 3. 集合覆盖问题
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* ══════════════════════════════════════════════════════════════════════
 * 辅助函数：位运算工具
 * ══════════════════════════════════════════════════════════════════════ */

/** 统计二进制中 1 的个数 */
static int popcount(int x)
{
    int count = 0;
    while (x) {
        count++;
        x &= x - 1;  /* 消除最低位的 1 */
    }
    return count;
}

/** 枚举子集 */
static void enumerate_subsets(int set)
{
    printf("  子集: {");
    int first = 1;
    for (int subset = set; subset; subset = (subset - 1) & set) {
        if (!first) printf(", ");
        first = 0;
        printf("{");
        int f = 1;
        for (int i = 0; i < 10; i++) {
            if (subset & (1 << i)) {
                if (!f) printf(",");
                printf("%d", i);
                f = 0;
            }
        }
        printf("}");
    }
    printf("}\n");
}

/* ══════════════════════════════════════════════════════════════════════
 * TSP (旅行商问题) 动态规划
 * ══════════════════════════════════════════════════════════════════════ */

#define INF INT_MAX

/**
 * DP 求解 TSP
 * @param dist 距离矩阵 (n x n)
 * @param n 城市数
 * @return 最短路径长度
 */
static int tsp_dp(int **dist, int n)
{
    if (n <= 1) return 0;

    /* dp[mask][i] = 从状态 mask 出发，当前在顶点 i，已访问的最优长度 */
    int size = 1 << n;
    int **dp = (int **)malloc((size_t)size * sizeof(int *));
    for (int i = 0; i < size; i++) {
        dp[i] = (int *)malloc((size_t)n * sizeof(int));
        memset(dp[i], -1, (size_t)n * sizeof(int));
    }

    /* 初始化：从单个顶点出发 */
    for (int i = 0; i < n; i++) {
        dp[1 << i][i] = 0;
    }

    /* 状态转移 */
    for (int mask = 1; mask < size; mask++) {
        for (int u = 0; u < n; u++) {
            if (dp[mask][u] < 0) continue;
            /* 从 u 出发，尝试访问未访问的顶点 v */
            for (int v = 0; v < n; v++) {
                if (mask & (1 << v)) continue;  /* v 已访问 */
                if (dist[u][v] == INF) continue;
                int new_mask = mask | (1 << v);
                int new_dist = dp[mask][u] + dist[u][v];
                if (dp[new_mask][v] < 0 || new_dist < dp[new_mask][v]) {
                    dp[new_mask][v] = new_dist;
                }
            }
        }
    }

    /* 计算结果：从完整集合出发，回到起点 0 */
    int full = size - 1;
    int result = INF;
    for (int i = 1; i < n; i++) {
        if (dp[full][i] >= 0 && dist[i][0] != INF) {
            int total = dp[full][i] + dist[i][0];
            if (total < result) result = total;
        }
    }

    /* 释放内存 */
    for (int i = 0; i < size; i++) {
        free(dp[i]);
    }
    free(dp);

    return result;
}

/* ══════════════════════════════════════════════════════════════════════
 * 集合覆盖问题
 * ══════════════════════════════════════════════════════════════════════ */

/** 集合覆盖：贪心近似算法 */
static int set_cover_greedy(int sets[][4], int set_count, int elements, int *covered)
{
    memset(covered, 0, (size_t)elements * sizeof(int));
    int cover_count = 0;

    while (1) {
        int best_set = -1;
        int best_cover = 0;

        /* 找能覆盖最多未覆盖元素的集合 */
        for (int i = 0; i < set_count; i++) {
            int cover = 0;
            for (int j = 0; j < sets[i][0]; j++) {
                int elem = sets[i][j + 1];
                if (!covered[elem]) cover++;
            }
            if (cover > best_cover) {
                best_cover = cover;
                best_set = i;
            }
        }

        if (best_set < 0 || best_cover == 0) break;

        /* 标记覆盖的元素 */
        for (int j = 0; j < sets[best_set][0]; j++) {
            covered[sets[best_set][j + 1]] = 1;
        }
        cover_count++;
    }

    return cover_count;
}

/* ══════════════════════════════════════════════════════════════════════
 * 演示函数
 * ══════════════════════════════════════════════════════════════════════ */

/** 演示状态压缩基础 */
static void demo_state_compress(void)
{
    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  示例 1: 状态压缩基础操作\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    /* popcount 示例 */
    int numbers[] = {0, 1, 7, 15, 255, 1024};
    printf("  popcount 示例:\n");
    for (size_t i = 0; i < sizeof(numbers) / sizeof(numbers[0]); i++) {
        printf("    popcount(%4d = 0x%04X) = %d\n",
               numbers[i], numbers[i], popcount(numbers[i]));
    }

    /* 枚举子集 */
    printf("\n  枚举集合 {0, 2, 3} 的所有子集:\n");
    enumerate_subsets((1 << 0) | (1 << 2) | (1 << 3));

    /* 位运算技巧 */
    printf("\n  位运算技巧:\n");
    printf("    取最低位的 1: x & (-x) = 0x%X\n", 12 & -12);
    printf("    消除最低位的 1: x & (x-1) = 0x%X\n", 12 & (12 - 1));
    printf("    判断是否为 2 的幂: (x & (x-1)) == 0\n");
    printf("    设置第 i 位: x | (1 << i)\n");
    printf("    清除第 i 位: x & ~(1 << i)\n");
    printf("    切换第 i 位: x ^ (1 << i)\n");
}

/** 演示 TSP */
static void demo_tsp(void)
{
    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  示例 2: 旅行商问题 (TSP) 动态规划\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    /* 4 个城市的距离矩阵 */
    int n = 4;
    int **dist = (int **)malloc((size_t)n * sizeof(int *));
    for (int i = 0; i < n; i++) {
        dist[i] = (int *)malloc((size_t)n * sizeof(int));
    }

    /* 距离矩阵（对称）*/
    int d[4][4] = {
        {0, 10, 15, 20},
        {10, 0, 35, 25},
        {15, 35, 0, 30},
        {20, 25, 30, 0}
    };

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            dist[i][j] = d[i][j];
        }
    }

    printf("  城市距离矩阵:\n");
    printf("       C0   C1   C2   C3\n");
    for (int i = 0; i < n; i++) {
        printf("  C%d:", i);
        for (int j = 0; j < n; j++) {
            printf("  %2d ", dist[i][j]);
        }
        printf("\n");
    }

    int result = tsp_dp(dist, n);
    printf("\n  最短回路长度: %d\n", result);
    printf("  最优路线: 0 → 1 → 3 → 2 → 0 (长度 10+25+30+15=80)\n");

    for (int i = 0; i < n; i++) {
        free(dist[i]);
    }
    free(dist);
}

/** 演示集合覆盖 */
static void demo_set_cover(void)
{
    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  示例 3: 集合覆盖问题（贪心算法）\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    /* 5 个集合，覆盖 6 个元素 {0,1,2,3,4,5} */
    int sets[5][4] = {
        {3, 0, 1, 3},   /* S0 = {0, 1, 3} */
        {2, 1, 2},      /* S1 = {1, 2} */
        {3, 2, 3, 4},   /* S2 = {2, 3, 4} */
        {2, 4, 5},      /* S3 = {4, 5} */
        {2, 0, 5}       /* S4 = {0, 5} */
    };

    int elements = 6;
    int covered[6];

    printf("  集合: S0={0,1,3}, S1={1,2}, S2={2,3,4}, S3={4,5}, S4={0,5}\n");
    int count = set_cover_greedy(sets, 5, elements, covered);

    printf("  贪心选择: S0, S2, S3\n");
    printf("  覆盖的元素: ");
    for (int i = 0; i < elements; i++) {
        if (covered[i]) printf("%d ", i);
    }
    printf("\n  使用集合数: %d\n", count);
}

/* ══════════════════════════════════════════════════════════════════════
 * 主函数
 * ══════════════════════════════════════════════════════════════════════ */

int main(void)
{
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║          位运算 DP: 状态压缩与 TSP                              ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");

    demo_state_compress();
    demo_tsp();
    demo_set_cover();

    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  算法复杂度:\n");
    printf("    • TSP DP:      O(n^2 * 2^n)\n");
    printf("    • 集合覆盖:    O(m * n) 贪心近似\n");
    printf("  应用场景:\n");
    printf("    • 状态机实现：位掩码表示状态组合\n");
    printf("    • 位图索引：数据库页面可用性追踪\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    return 0;
}
