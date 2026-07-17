# ds-fenwick-tree 规格

## 概述

树状数组（Fenwick Tree）是一种支持 O(log n) 前缀和查询和单点更新的数据结构。

## 目录结构

```
src/ds/fenwick_tree/
├── README.md           # 详细文档（含 Mermaid 可视化）
├── impl/
│   └── fenwick_tree.c
└── problems/          # LeetCode 题目
```

## 实现功能

| 功能 | 函数名 | 时间复杂度 |
|------|--------|-----------|
| 创建 | `fenwick_create()` | O(n) |
| 单点更新 | `fenwick_update()` | O(log n) |
| 前缀和查询 | `fenwick_prefix_sum()` | O(log n) |
| 区间和查询 | `fenwick_range_sum()` | O(log n) |

## README.md 文档要求

1. 树状数组结构可视化（如数组 `[1,2,3,4,5]` 对应的树状数组结构）
2. lowbit 操作原理可视化
3. 更新和查询过程可视化

## 验收标准

- [ ] 树状数组基本操作实现完成
- [ ] README.md 包含 lowbit 和查询更新可视化
- [ ] CMakeLists.txt 正确配置
