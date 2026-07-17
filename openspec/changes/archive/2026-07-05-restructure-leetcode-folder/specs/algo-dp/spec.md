# algo-dp 规格

## 概述

动态规划是一种将复杂问题分解为子问题并存储子问题解的算法思想。

## 目录结构

```
src/algo/dp/
├── README.md           # 详细文档（含 Mermaid 可视化）
├── impl/
│   └── dp.c          # 基础 DP 实现
└── problems/          # LeetCode 题目
```

## 实现功能

| 功能 | 函数名 | 说明 |
|------|--------|------|
| 斐波那契 | `fibonacci()` | 基础 DP |
| 爬楼梯 | `climb_stairs()` | 简单 DP |
| 背包问题 | `knapsack()` | 0/1 背包 |
| 最长递增子序列 | `lis()` | LIS |

## README.md 文档要求

每个问题必须包含：
1. DP 状态定义
2. 状态转移方程
3. 具体示例的状态转移过程可视化
4. DP 表/数组的变化过程

## 验收标准

- [ ] 基础 DP 问题实现完成
- [ ] README.md 包含状态转移可视化
- [ ] CMakeLists.txt 正确配置
