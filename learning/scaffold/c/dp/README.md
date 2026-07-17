# dp scaffold

动态规划三范式：斐波那契（记忆化 vs 递推 vs 朴素递归对比）、0/1 背包（完整 DP 表 + 回溯最优解）、最长公共子序列（DP 表 + 回溯路径）。

## 复现命令

```bash
cd learning/scaffold/dp
gcc -Wall -Wextra -O2 -std=c11 -o dp_demo main.c
./dp_demo
```

## 预期输出

### 斐波那契对比
```
[fib naive] fib(10)=55, total calls=177 (O(2^n) exponential!)
[fib memo] computing fib(10)...  ← 仅计算 9 次
[fib memo] result: fib(10) = 55
[fib bottomup] dp[2] = dp[1]+dp[0] = 1+0 = 1
...
[fib bottomup] result: fib(10) = 55
```

### 0/1 背包 DP 表
```
[knapsack] DP table (4 items x capacity 7):
[knapsack]        w=0  w=1  w=2  w=3  w=4  w=5  w=6  w=7
[knapsack] i=0    0    0    0    0    0    0    0    0
[knapsack] i=1    0    1    1    1    1    1    1    1    (wt=1, val=1)
[knapsack] i=2    0    1    1    4    5    5    5    5    (wt=3, val=4)
[knapsack] i=3    0    1    1    4    5    6    6    9    (wt=4, val=5)
[knapsack] i=4    0    1    1    4    5    7    8    9    (wt=5, val=7)
```

### LCS 回溯
```
[lcs] result: "BCBA" (length=4)
```

## 关键点

- **记忆化用空间换时间**：fib(10) 朴素递归 177 次调用，记忆化仅 9 次计算（O(n) vs O(2ⁿ)）
- **背包递推**：`dp[i][w] = max(dp[i-1][w], val[i]+dp[i-1][w-wt[i]])` — 拿或不拿，选价值更大的
- **LCS 三方向**：字符匹配→↖，不匹配取上/左最大值→↑/←。回溯时逆着走
- **DP 表格是核心价值**：一张表让你看到所有子问题的解，不需要重新计算

详见 NOTES.md 工程对照段。
