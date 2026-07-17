# algo-prefix-sum 规格

## 概述

前缀和是一种预处理技术，可快速计算数组区间的和。

## 目录结构

```
src/algo/prefix_sum/
├── README.md           # 详细文档（含 Mermaid 可视化）
└── problems/          # LeetCode 题目
```

## 实现功能

| 功能 | 函数名 | 时间复杂度 |
|------|--------|-----------|
| 构建前缀和 | `prefix_sum_build()` | O(n) |
| 区间和查询 | `prefix_sum_range()` | O(1) |

## README.md 文档要求

1. 前缀和数组构建过程可视化
2. 区间和计算的具体示例（如 prefix[5] - prefix[1] = sum[2:5]）
3. 二维前缀和的可视化

## 验收标准

- [ ] 前缀和实现完成
- [ ] README.md 包含前缀和构建和查询可视化
- [ ] 包含 LeetCode 3159 题目
- [ ] CMakeLists.txt 正确配置
