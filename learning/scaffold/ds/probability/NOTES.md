# 工程对照说明

## 与工程模块的关联

### Monte Carlo 模拟

在 `engineering/src/algo/monte_carlo.c` 中，实现了金融期权的蒙特卡洛定价：

```c
/* 期权定价核心：生成多条标的资产路径 */
for (int path = 0; path < NUM_PATHS; path++) {
    double S = S0;  /* 初始价格 */
    for (int t = 0; t < T; t++) {
        /* GBM: S_{t+1} = S_t * exp((r - σ²/2)Δt + σ√Δt * Z) */
        double Z = gaussian_rand(rng);  /* 标准正态随机数 */
        S *= exp((r - sigma*sigma/2) * dt + sigma * sqrt(dt) * Z);
    }
    payoff += fmax(S - K, 0);  /* 看涨期权 payoff */
}
price = exp(-r * T) * payoff / NUM_PATHS;
```

本模块的 PI 估算与此完全对应：
```c
/* 本模块的蒙特卡洛 PI 估算 */
inside = 0;
for (i = 0; i < N; i++) {
    double x = uniform_rand(), y = uniform_rand();
    if (x*x + y*y <= 1) inside++;  /* 圆内判定 */
}
pi_estimate = 4.0 * inside / N;
```

### 随机化算法

`engineering/src/algo/randomized_algo.c` 包含：
- **快速排序随机化**：随机选择 pivot，避免最坏情况 O(n²)
- **跳过列表**：随机层数决定搜索复杂度
- **布隆过滤器**：使用多个哈希函数的随机映射

### 线性同余生成器的工程实现

glibc 的 `random()` 函数使用 LCG：
```c
/* glibc 内部实现 */
state[0] = (state[0] * 1103515245 + 12345) & 0x7fffffff;
return state[0];
```

本模块的 LCG 实现与之完全兼容，可直接替换 glibc 的 `rand()`。

### 高质量随机数生成

现代工程推荐使用 Mersenne Twister (MT19937)：
```c
/* 工程中替代 LCG 的推荐方案 */
#include <random.h>
mt19937 rng(seed);
double x = uniform_real_distribution(rng);
```

但在嵌入式/游戏等场景，LCG 因简单高效仍被广泛使用。

### 概率 DP 在数据库优化器中的应用

查询优化器的"基数估计"使用概率 DP：
```c
/* 估算 Filter 结果行数 */
double selectivity = 1.0;
for (int i = 0; i < num_conditions; i++) {
    selectivity *= estimate_clause_selectivity(clause[i]);
}
estimated_rows = base_table_rows * selectivity;
```

### 随机采样在 OLAP 引擎中的应用

大规模数据分析使用随机采样加速：
- `TABLESAMPLE SYSTEM (10)` — 系统页级采样
- `TABLESAMPLE BERNOULLI (10)` — 行级随机采样
- 采样精度：`误差 ∝ 1/√n`

### 工程实现参考

`engineering/src/algo/statistics.c` 提供了随机数的统计分析：
```c
/* 验证随机数均匀性的卡方检验 */
double chi_square_test(int *observed, double *expected, int n) {
    double chi2 = 0;
    for (int i = 0; i < n; i++) {
        chi2 += (observed[i] - expected[i]) * (observed[i] - expected[i]) / expected[i];
    }
    return chi2;  /* 与临界值比较判断均匀性 */
}
```
