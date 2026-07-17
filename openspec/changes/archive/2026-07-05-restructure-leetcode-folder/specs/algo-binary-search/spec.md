# algo-binary-search 规格

## 概述

二分查找是在有序数组中高效查找目标值的算法。

## 目录结构

```
src/algo/binary_search/
├── README.md           # 详细文档（含 Mermaid 可视化）
├── impl/
│   ├── binary_search.c
│   └── binary_search.hpp
└── problems/          # LeetCode 题目
```

## 实现功能

| 功能 | 函数名 | 时间复杂度 |
|------|--------|-----------|
| 标准二分 | `binary_search()` | O(log n) |
| 左边界二分 | `binary_search_left()` | O(log n) |
| 右边界二分 | `binary_search_right()` | O(log n) |
| 旋转数组二分 | `search_rotated()` | O(log n) |

## README.md 文档要求

每个算法必须包含：
1. 算法原理说明
2. 具体数字示例（如在 `[1, 3, 5, 7, 9]` 中查找 `5`）
3. Mermaid 流程图展示每一步的 left/right/mid 变化
4. 明确标识 mid 位置的元素与目标比较结果

## 验收标准

- [ ] 所有二分查找变体实现完成
- [ ] README.md 包含查找过程可视化
- [ ] 每一步的 left/right/mid 变化清晰展示
- [ ] 包含 LeetCode 2070、3066、3280 题目
- [ ] CMakeLists.txt 正确配置
