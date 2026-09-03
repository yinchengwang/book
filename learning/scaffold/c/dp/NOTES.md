# dp 学习笔记

## 概念地图

动态规划（DP）不是一种算法，而是一类问题求解的**设计范式**——三个核心要素：

- **最优子结构**：大问题的最优解包含子问题的最优解。背包中 `dp[i][w]` 的最优值由 `dp[i-1][w]`（不拿第 i 件）和 `dp[i-1][w-wt[i]]`（拿第 i 件）的最优子解决定
- **重叠子问题**：递归树中同一子问题被反复求解。fib(10) 的朴素递归调用了 177 次，但真正需要算的只有 11 个（fib(0)~fib(10)）。记忆化用缓存消除重复计算
- **两种实现方向**：
  - **自顶向下（记忆化）**：从原问题出发，递归求解子问题，缓存已计算结果。适合"不知道所有子问题"的场景
  - **自底向上（递推）**：从小子问题开始，逐步填充 DP 表，直到原问题。适合"子问题有明确顺序"的场景，且没有递归栈溢出风险
- **状态转移方程**是 DP 的核心契约：`dp[i][w] = max(dp[i-1][w], val[i] + dp[i-1][w-wt[i]])`——一行方程定义了整个问题的解空间

## 踩坑记录

1. **递推顺序错误**：0/1 背包的容量循环**必须大到小**（逆向）才能保证每件物品只用一次——如果 w 从小到大遍历，`dp[w-wt[i]]` 可能已经包含了第 i 件物品（变成完全背包）。本 demo 用的是二维 dp[i][w]，无需担心循环方向
2. **LCS 回溯路径不唯一**：当 `dp[i-1][j] == dp[i][j-1]` 时，两个方向都合法，选不同的方向会产生不同的 LCS（如 "BCBA" vs "BCAB"），但长度相同。本 demo 优先选 ↑
3. **DP 表空间优化**：二维 dp[m+1][n+1] 的 LCS 只需要两行（当前行+上一行）就能递推——`dp[j]` 覆盖 `dp[i-1][j]`，用 `prev` 保存 `dp[i-1][j-1]`。本 demo 保留完整表格是为了打印中间状态
4. **fib 记忆化的边界**：`memo[0]=0, memo[1]=1` 必须在递归前手工填入——如果让递归自己算，当 n=0 或 n=1 时 memo 检查 `memo[n]!=-1` 成立但值是 0，会错误地返回 0（0 ≠ -1 但语义正确，因为 fib(0)=0）
5. **本机 MinGW 无 make**：用 `gcc -Wall -Wextra -O2 -std=c11 -o dp_demo main.c && ./dp_demo` 直接编译运行

## 工程对照（≥100 字硬约束）

动态规划不是算法竞赛的玩具——它在工程中以"最优路径选择"的形式频繁出现：

1. **HMM 分词的 Viterbi 算法**：`engineering/src/algo-prod/dict/dict_hmm.c` 中的中文分词用隐马尔可夫模型——Viterbi 算法本质就是 DP 最短路径：`dp[t][j] = min(dp[t-1][k] + cost[k][j])`，对每个时刻 t 的每个状态 j，选最小的前驱状态转移代价。这和 LCS 的三方向取 max 是同构的
2. **WAL 恢复的脏页最小重放集**：`engineering/src/db/storage/wal/wal_buf.c` 中 WAL 恢复需要选择"最小脏页集合"来重放日志——`dp[i] = min pages to replay for first i log entries`，等价于区间覆盖 DP。checkpoint 的间隔选择同样可建模为 DP（最大化重放效率）
3. **DiskANN beam search 中的路径选择**：`engineering/src/db/index/vector_index/diskann/` 中的 beam search 每步保留 k 个最优候选，选择下一跳邻居的过程等价于 DP 状态转移——`dp[t][neighbor] = min distance from query via beam[t-1]`
4. **Raft 选举超时的退避策略**：`engineering/src/db/consensus/raft.c` 中 candidate 选举超时后重新随机退避——这个"最优退避时间"可以建模为 DP：`dp[failures] = min(backoff 使得选举成功概率最大 + dp[failures+1])`（虽然实际实现用的是随机化，理论分析可用 DP）
5. **预留测试入口**：`engineering/test/algo/dp/` 当前为空目录，后续 OPSX 变更可将 Fibonacci/Knapsack/LCS 的工程化版本落盘，配上 unit test 和性能基准

学完本卡后能动手的事：用 DP 的思路重新分析 `engineering/src/db/storage/wal/wal_buf.c` 中 WAL checkpoint 的选择逻辑——当前的实现是"每隔 N 条日志写一次 checkpoint"，但 DP 可以给出"何时写 checkpoint 使恢复时间最小"的最优策略（trade-off：checkpoint 开销 vs 恢复时的重放开销）。
