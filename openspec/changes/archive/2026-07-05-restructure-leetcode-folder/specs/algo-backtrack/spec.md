# algo-backtrack 规格

## 概述

回溯算法通过枚举所有可能的解来找出满足条件的解。

## 目录结构

```
src/algo/backtrack/
├── README.md           # 详细文档（含 Mermaid 可视化）
└── problems/          # LeetCode 题目
```

## 实现功能

| 功能 | 函数名 | 说明 |
|------|--------|------|
| 全排列 | `permute()` | 生成全排列 |
| 组合 | `combine()` | 生成组合 |
| 子集 | `subsets()` | 生成所有子集 |
| N 皇后 | `n_queens()` | N 皇后问题 |

## README.md 文档要求

1. 回溯原理说明（决策树）
2. 具体示例的决策树可视化
3. 每一步的选择和回溯过程

## 验收标准

- [ ] 回溯算法实现完成
- [ ] README.md 包含决策树可视化
- [ ] CMakeLists.txt 正确配置
