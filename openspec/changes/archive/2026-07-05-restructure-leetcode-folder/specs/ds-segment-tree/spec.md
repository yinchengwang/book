# ds-segment-tree 规格

## 概述

线段树是一种用于区间查询的数据结构，支持 O(log n) 的区间更新和查询。

## 目录结构

```
src/ds/segment_tree/
├── README.md           # 详细文档（含 Mermaid 可视化）
├── impl/
│   └── segment_tree.c
└── problems/          # LeetCode 题目
```

## 实现功能

| 功能 | 函数名 | 时间复杂度 |
|------|--------|-----------|
| 创建 | `seg_create()` | O(n) |
| 区间查询 | `seg_query()` | O(log n) |
| 单点更新 | `seg_update()` | O(log n) |
| 区间更新 | `seg_update_range()` | O(log n) |

## README.md 文档要求

1. 线段树结构可视化（如数组 `[1,2,3,4,5]` 对应的线段树）
2. 区间查询过程可视化
3. 更新操作过程可视化

## 验收标准

- [ ] 线段树基本操作实现完成
- [ ] README.md 包含查询和更新可视化
- [ ] CMakeLists.txt 正确配置
