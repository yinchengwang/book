# 工程对照说明

## 与工程模块的关联

### 概率计算与组合优化

在 `engineering/src/algo/combinatorics.c` 中，排列组合是概率论和统计学的基础。本模块的组合数计算 `combination(n,r)` 直接对应组合数学库函数，用于：

- **概率计算**：P(A) = C(n,k) × p^k × (1-p)^(n-k) 二项分布
- **密码学**：组合数用于密钥空间计算
- **组合优化**：选路问题、调度问题的数学基础

### Monte Carlo 概率模拟

Monte Carlo 方法本质上是"重复随机试验 + 计数"：
```c
// 本模块的容斥原理
long long count = count_divisible(n, a, b);

// Monte Carlo 概率估算
long long hits = 0;
for (int i = 0; i < N; i++) {
    if (满足条件) hits++;
}
double probability = (double)hits / N;
```

两者都遵循"大数定律"：重复次数越多，估算越准确。

### 组合数优化策略

本模块采用 `result = result * (n-i) / (i+1)` 策略避免大数溢出：
```c
long long combination(int n, int r) {
    r = (r < n - r) ? r : (n - r);  /* 取较小值 */
    long long result = 1;
    for (int i = 0; i < r; i++) {
        result = result * (n - i) / (i + 1);  /* 逐步约分 */
    }
    return result;
}
```

这与 `engineering/src/algo/bignum.c` 中的大数运算优化思路一致：分步计算 + 防止溢出。

### 卡特兰数的工程应用

| 应用场景 | n | 卡特兰数 C_n | 说明 |
|----------|---|-------------|------|
| 合法括号序列 | 3 | 5 | 3对括号的匹配方式 |
| 二叉树种类 | 4 | 14 | 4个节点的二叉搜索树 |
| 单调路径 | 5 | 42 | n×n 网格左上到右下 |
| 凸多边形三角剖分 | 6 | 132 | 7边形的三角剖分 |

### 数据库查询优化器

查询优化器中"子查询去重"使用容斥原理思想：
```sql
-- 等价转换
SELECT * FROM A WHERE x IN (SELECT FROM B)
           UNION
SELECT * FROM A WHERE y IN (SELECT FROM C)
-- 不可直接 UNION（重复计数）
-- 需要 UNION ALL 再去重，或用 EXCEPT
```

### 工程实现参考

`engineering/src/algo/combinatorics.c` 提供了组合数的查表优化：

```c
/* 预计算组合数表 (帕斯卡三角) */
for (int n = 0; n <= MAX_N; n++) {
    C[n][0] = C[n][n] = 1;
    for (int k = 1; k < n; k++) {
        C[n][k] = C[n-1][k-1] + C[n-1][k];
    }
}
```

本模块的迭代公式是空间优化版，适合 n 较大的场景。
