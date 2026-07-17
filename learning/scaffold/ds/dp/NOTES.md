# 工程对照说明

## 与 engineering/src/algo-prod/dp/ 的关联

### DP 在工程中的实际应用

`engineering/src/algo-prod/dp/` 目录中的实现涵盖了真实生产环境中的 DP 应用：

#### 1. 路径规划 (path_planning/)

在机器人路径规划和导航系统中，DP 用于计算最短路径：

```c
/* path_dp.c - 栅格地图最短路径 */
int path_dp(int **grid, int rows, int cols) {
    int **dp = malloc(rows * sizeof(int *));
    for (int i = 0; i < rows; i++) {
        dp[i] = malloc(cols * sizeof(int));
        for (int j = 0; j < cols; j++) {
            if (grid[i][j] == OBSTACLE) continue;
            if (i == 0 && j == 0) dp[i][j] = grid[i][j];
            else dp[i][j] = min(dp[i-1][j], dp[i][j-1]) + grid[i][j];
        }
    }
    return dp[rows-1][cols-1];
}
```

与本 scaffold 中斐波那契的递推思想一致：每步最优来自前一步最优。

#### 2. 资源分配 (resource_allocation/)

多阶段资源分配问题（如云计算中的 VM 调度）：

```c
/* resource_dp.c - 多阶段资源分配 */
int resource_dp(int *demands, int n, int total_capacity) {
    int *dp = calloc(total_capacity + 1, sizeof(int));
    for (int i = 0; i < n; i++) {
        for (int c = total_capacity; c >= demands[i]; c--) {
            dp[c] = max(dp[c], dp[c - demands[i]] + profit[i]);
        }
    }
    return dp[total_capacity];
}
```

这与本模块 0-1 背包的「选/不选」决策完全对应。

#### 3. 序列比对 (sequence_alignment/)

生物信息学中的 DNA/蛋白质序列比对：

```c
/* alignment_dp.c - Needleman-Wunsch 算法 */
int sequence_alignment(const char *s1, const char *s2) {
    int m = strlen(s1), n = strlen(s2);
    int **dp = malloc((m+1) * sizeof(int *));
    for (int i = 0; i <= m; i++) dp[i] = calloc(n+1, sizeof(int));
    for (int i = 0; i <= m; i++) dp[i][0] = i;
    for (int j = 0; j <= n; j++) dp[0][j] = j;

    for (int i = 1; i <= m; i++) {
        for (int j = 1; j <= n; j++) {
            int match = dp[i-1][j-1] + (s1[i-1] == s2[j-1] ? 0 : 1);
            int delete = dp[i-1][j] + 1;
            int insert = dp[i][j-1] + 1;
            dp[i][j] = min(match, min(delete, insert));
        }
    }
    return dp[m][n];
}
```

这与本模块的编辑距离实现本质相同，只是权重和场景不同。

### DP 在数据库引擎中的应用

1. **查询优化器**: 动态规划选择最优连接顺序（Join Ordering Problem）
2. **代价估算**: 基于 DP 的代价模型计算候选执行计划的成本
3. **MVCC 版本链**: 链式版本管理中寻找可见版本的最优搜索

### 学习路径

- 本 scaffold (learning/scaffold/ds/dp/) → 理解 DP 核心思想
- 入门实现 (learning/code-solutions/dsa/dynamic_programming/) → 巩固基础
- 工程生产 (engineering/src/algo-prod/dp/) → 理解大规模 DP 的工程权衡
