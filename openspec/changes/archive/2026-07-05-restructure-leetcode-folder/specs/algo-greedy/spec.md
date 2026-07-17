# algo-greedy 规格

## 概述

贪心算法在每一步选择局部最优解，期望得到全局最优解。

## 目录结构

```
src/algo/greedy/
├── README.md           # 详细文档（含 Mermaid 可视化）
└── problems/          # LeetCode 题目
```

## 实现功能

| 功能 | 函数名 | 说明 |
|------|--------|------|
| 活动选择 | `activity_selection()` | 选择最多不重叠活动 |
| 钱币找零 | `coin_change()` | 最少硬币数 |
| 区间调度 | `interval_scheduling()` | 最多兼容区间 |

## README.md 文档要求

1. 贪心选择原理说明
2. 具体示例的每步选择过程
3. 可选：与 DP 方案对比

## 验收标准

- [ ] 贪心算法实现完成
- [ ] README.md 包含贪心选择过程可视化
- [ ] CMakeLists.txt 正确配置
