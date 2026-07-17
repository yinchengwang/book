# ds-bitmap 规格

## 概述

位图是一种用位来表示集合的数据结构，适用于大规模数据去重和统计。

## 目录结构

```
src/ds/bitmap/
├── README.md           # 详细文档（含 Mermaid 可视化）
├── impl/
│   └── bitmap.c
└── problems/          # LeetCode 题目
```

## 实现功能

| 功能 | 函数名 | 说明 |
|------|--------|------|
| 创建 | `bitmap_create()` | 创建位图 |
| 设置位 | `bitmap_set()` | 将某位设为 1 |
| 清除位 | `bitmap_clear()` | 将某位设为 0 |
| 获取位 | `bitmap_test()` | 获取某位的值 |
| 批量设置 | `bitmap_set_range()` | 设置范围内所有位 |

## README.md 文档要求

1. 位图内存布局可视化
2. 设置/清除位的具体示例
3. 位图应用场景（去重、统计）

## 验收标准

- [ ] 位图基本操作实现完成
- [ ] README.md 包含位操作可视化
- [ ] CMakeLists.txt 正确配置
