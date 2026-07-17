# algo-sorting 规格

## 概述

排序算法是基础算法。本规格定义常用排序算法的实现及可视化文档要求。

## 目录结构

```
src/algo/sorting/
├── README.md           # 详细文档（含 Mermaid 可视化）
├── impl/
│   ├── bubble_sort.c
│   ├── selection_sort.c
│   ├── insertion_sort.c
│   ├── merge_sort.c
│   ├── quick_sort.c
│   ├── heap_sort.c
│   └── sort.h
└── problems/          # LeetCode 题目
```

## 实现功能

| 算法 | 函数名 | 时间复杂度 | 空间复杂度 |
|------|--------|-----------|-----------|
| 冒泡排序 | `bubble_sort()` | O(n²) | O(1) |
| 选择排序 | `selection_sort()` | O(n²) | O(1) |
| 插入排序 | `insertion_sort()` | O(n²) | O(1) |
| 归并排序 | `merge_sort()` | O(n log n) | O(n) |
| 快速排序 | `quick_sort()` | O(n log n) 均摊 | O(log n) |
| 堆排序 | `heap_sort()` | O(n log n) | O(1) |

## README.md 文档要求

每个算法必须包含：
1. 算法原理说明
2. 具体数字示例（如 `[5, 2, 8, 1, 9]` 排序过程）
3. Mermaid 流程图展示排序过程
4. 每轮比较/交换的具体元素变化
5. 复杂度分析

## 验收标准

- [ ] 所有排序算法实现完成
- [ ] README.md 包含每个排序算法的可视化
- [ ] 包含排序过程的每轮状态变化
- [ ] CMakeLists.txt 正确配置
