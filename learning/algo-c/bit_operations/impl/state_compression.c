/*
 * state_compression.c —— 状态压缩技法演示
 *
 * ============================================================
 * 位运算基础技巧
 * ============================================================
 *
 * 【获取最低位的 1】x & (-x)
 *   x  = 10100  (-x = 01100)  → x & (-x) = 00100
 *
 * 【消去最低位的 1】x & (x - 1)
 *   x  = 10100  (x-1 = 10011)  → x & (x-1) = 10000
 *   应用：统计二进制中 1 的个数（popcount）
 *
 * 【遍历所有子集】
 *   给定集合 mask（如 mask=1011 表示 {0,1,3}），
 *   遍历其所有子集：
 *     for (sub = mask; sub; sub = (sub - 1) & mask) {
 *         // sub 是 mask 的一个子集
 *         // 例如：1011 → 1010 → 1001 → 1000 → 0011 → 0010 → 0001
 *     }
 *
 * 【判断是否为 2 的幂】x > 0 && (x & (x - 1)) == 0
 *
 * ============================================================
 * 旅行商问题（TSP）—— 状态压缩 DP 模板
 * ============================================================
 *
 * 有 n 个城市，dist[i][j] 表示从 i 到 j 的距离。
 * 求从城市 0 出发，经过所有城市恰好一次，回到 0 的最短路径。
 *
 * DP 状态定义：
 *   dp[mask][i] = 已访问集合为 mask（以二进制表示），
 *                 最后访问的城市是 i 时的最短距离。
 *   mask 的范围：[0, 2^n - 1]
 *
 * 状态转移：
 *   dp[mask][i] = min_{j in mask, j != i} {
 *       dp[mask without i][j] + dist[j][i]
 *   }
 *
 *   其中"mask without i" = mask ^ (1 << i)
 *
 * 初始状态：
 *   dp[1 << start][start] = 0（从 start 出发，只访问了 start）
 *
 * 复杂度：O(n² · 2^n)
 */

#include <ds/state_compression.h>

#include <stdio.h>
#include <string.h>

#define TSP_MAX_N 10u

static int tsp_min(int a, int b) { return a < b ? a : b; }

/*
 * TSP 状态压缩 DP 求解。
 * n: 城市数量, dist: 距离矩阵（dist[i][j] = i→j 的距离）
 * 返回最短路径长度。
 */
static int tsp_solve(int n, int dist[TSP_MAX_N][TSP_MAX_N])
{
    int total_masks = 1 << n;
    int dp[1 << TSP_MAX_N][TSP_MAX_N];

    /* 初始化为 "无穷大" */
    for (int mask = 0; mask < total_masks; ++mask) {
        for (int i = 0; i < n; ++i) {
            dp[mask][i] = 999999;
        }
    }

    /* 初始状态：从城市 0 出发 */
    dp[1][0] = 0;

    /* 遍历所有状态（按 mask 递增顺序，确保子问题已算出） */
    for (int mask = 1; mask < total_masks; ++mask) {
        for (int i = 0; i < n; ++i) {
            /* 检查 i 是否在 mask 中 */
            if (!(mask & (1 << i))) continue;
            if (dp[mask][i] == 999999) continue;

            /* 尝试从 i 走向所有未访问的城市 j */
            for (int j = 0; j < n; ++j) {
                if (mask & (1 << j)) continue; /* j 已访问 */

                int new_mask = mask | (1 << j);
                dp[new_mask][j] = tsp_min(dp[new_mask][j],
                                          dp[mask][i] + dist[i][j]);
            }
        }
    }

    /* 回到起点 0 */
    int ans = 999999;
    int full_mask = total_masks - 1;
    for (int i = 1; i < n; ++i) {
        ans = tsp_min(ans, dp[full_mask][i] + dist[i][0]);
    }

    return ans;
}

/* ============================================================
 * 子集枚举演示
 * ============================================================ */

static void enumerate_subsets(int mask)
{
    printf("  集合 %d (二进制 ", mask);
    for (int i = 4; i >= 0; --i) printf("%d", (mask >> i) & 1);
    printf(") 的所有子集: ");

    int count = 0;
    for (int sub = mask; sub; sub = (sub - 1) & mask) {
        printf("%d ", sub);
        ++count;
    }
    printf("(共 %d 个，包括空集则为 %d 个)\n", count, count + 1);
}

/* ============================================================
 * 演示函数
 * ============================================================ */

void ds_state_compression_demo(void)
{
    printf("========== 状态压缩技法演示 ==========\n");

    /* 位运算基础技巧 */
    printf("\n【位运算基础技巧】\n");
    int x = 20; /* 二进制 10100 */
    printf("  x = %d (0b10100)\n", x);
    printf("  最低位的 1: x & (-x) = %d\n", x & (-x));
    printf("  消去最低位的 1: x & (x-1) = %d\n", x & (x - 1));
    printf("  x 中 1 的个数: ");
    int count = 0;
    for (int t = x; t; t &= t - 1) ++count;
    printf("%d\n", count);

    /* 子集枚举 */
    printf("\n【子集枚举】\n");
    enumerate_subsets(5);  /* 二进制 00101 = {0,2} */
    enumerate_subsets(11); /* 二进制 01011 = {0,1,3} */

    /* TSP 状态压缩 DP */
    printf("\n【TSP 旅行商问题（状态压缩 DP）】\n");
    int dist[TSP_MAX_N][TSP_MAX_N] = {
        {0, 10, 15, 20},
        {10, 0, 35, 25},
        {15, 35, 0, 30},
        {20, 25, 30, 0}
    };
    int n = 4;

    printf("城市距离矩阵：\n");
    printf("    0   1   2   3\n");
    for (int i = 0; i < n; ++i) {
        printf("%d: ", i);
        for (int j = 0; j < n; ++j) {
            printf("%3d ", dist[i][j]);
        }
        printf("\n");
    }

    int shortest = tsp_solve(n, dist);
    printf("最短路径长度: %d\n", shortest);
    printf("（理论最优路径: 0→1→3→2→0 = 10+25+30+15 = 80）\n");

    printf("========== 状态压缩演示结束 ==========\n");
}
