# algo-two-pointers 规格

## 概述

双指针是一种通过两个指针协同遍历来解决问题的技巧。

## 目录结构

```
src/algo/two_pointers/
├── README.md           # 详细文档（含 Mermaid 可视化）
└── problems/          # LeetCode 题目
```

## 实现功能

| 功能 | 函数名 | 说明 |
|------|--------|------|
| 有序数组两数之和 | `two_sum()` | 左右指针 |
| 移除元素 | `remove_element()` | 快慢指针 |
| 反转数组 | `reverse_array()` | 首尾指针 |

## README.md 文档要求

1. 双指针原理说明（快慢指针、对撞指针、滑动窗口）
2. 具体示例的指针移动过程可视化
3. 每一步的 left/right 位置和指向的值

## 验收标准

- [ ] 双指针算法实现完成
- [ ] README.md 包含指针移动可视化
- [ ] CMakeLists.txt 正确配置
