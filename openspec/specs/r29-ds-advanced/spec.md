# R29 DS 图论与高级算法 — 能力规格

## 能力概述

R29 DS 图论与高级算法栈，包含 16 张算法卡片，涵盖图论、动态规划、高级算法设计技巧。

## 16 张卡片规格

| ID | 卡片名 | 核心内容 | 行数 | 编译要求 |
|----|--------|----------|------|----------|
| shortest_path | 最短路径 | Dijkstra/Bellman-Ford/SPFA/Floyd | ~150 | gcc -std=c11 |
| mst | 最小生成树 | Prim/Kruskal/并查集 | ~140 | gcc -std=c11 |
| network_flow | 网络流 | Edmonds-Karp/最大流最小割 | ~150 | gcc -std=c11 |
| dp | 动态规划 | 斐波那契/背包/编辑距离 | ~140 | gcc -std=c11 |
| greedy | 贪心算法 | 区间调度/Huffman/分数背包 | ~130 | gcc -std=c11 |
| backtrack | 回溯算法 | N皇后/全排列/子集生成 | ~130 | gcc -std=c11 |
| divide_conquer | 分治算法 | 归并排序/Karatsuba/最近点对 | ~140 | gcc -std=c11 -lm |
| daemon | 守护进程 | daemon()/信号处理/进程监控 | ~130 | gcc -std=c11 |
| graph_advanced | 图高级 | SCC/Tarjan/欧拉回路 | ~140 | gcc -std=c11 |
| string_algorithm | 字符串算法 | KMP/BM/Trie | ~150 | gcc -std=c11 |
| bit_dp | 位运算DP | 状态压缩/TSP/集合覆盖 | ~140 | gcc -std=c11 |
| tree_dp | 树形DP | 最大独立集/树直径 | ~130 | gcc -std=c11 |
| interval_dp | 区间DP | 矩阵链乘/石子合并 | ~140 | gcc -std=c11 |
| counting | 计数原理 | 排列组合/容斥/卡特兰数 | ~120 | gcc -std=c11 |
| probability | 概率与随机 | LCG/蒙特卡洛/概率DP | ~130 | gcc -std=c11 |
| matrix | 矩阵运算 | 矩阵乘法/快速幂/稀疏矩阵 | ~130 | gcc -std=c11 |

## 共同规格

### 目录结构
```
learning/scaffold/ds/<card_id>/
├── main.c      # 中文注释，~120-150行
├── Makefile    # CC=gcc, CFLAGS=-Wall -Wextra -O2 -std=c11
├── README.md   # 运行说明
└── NOTES.md    # 工程对照（≥100字）
```

### 编译验证
- 所有卡片必须 `gcc -std=c11 -Wall -Wextra -O2 -o demo main.c && ./demo` 通过
- 输出包含算法名称、执行结果、时间复杂度说明

### 工程对照要求
每张卡的 NOTES.md 必须包含：
1. 核心算法原理（3-5行）
2. 工程应用场景（≥100字）
3. 相关知识点引用

## 学习路径

```
图论基础 (R28)
    ↓
R29 图论与高级算法
    ├── 最短路径 → 网络路由、GPS导航
    ├── 最小生成树 → 网络设计、聚类分析
    ├── 网络流 → 流量优化、二分图匹配
    ├── 动态规划 → 优化问题通用解法
    ├── 贪心 → 调度、压缩
    ├── 回溯 → 搜索、约束满足
    ├── 分治 → 并行计算
    ├── 字符串算法 → 文本处理
    └── ...
```

## 验收标准

- [x] 16 张卡片全部创建完成
- [x] 所有 main.c 中文注释完整
- [x] 所有 Makefile 格式统一
- [x] 所有 README.md 包含运行说明
- [x] 所有 NOTES.md 包含工程对照（≥100字）
- [x] statuses.json 更新 done count: 189→215
- [x] r29-progress.md 创建（16行）
- [x] 至少 4 张卡编译运行验证通过
