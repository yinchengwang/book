# algo-sliding-window 规格

## 概述

滑动窗口是一种处理子数组/子字符串问题的技巧。

## 目录结构

```
src/algo/sliding_window/
├── README.md           # 详细文档（含 Mermaid 可视化）
└── problems/          # LeetCode 题目
```

## 实现功能

| 功能 | 函数名 | 说明 |
|------|--------|------|
| 最大子数组和 | `max_subarray_sum()` | 滑动窗口求最大和 |
| 最小子数组 | `min_subarray_len()` | 找到最小长度子数组 |
| 无重复字符最长子串 | `longest_substring()` | 滑动窗口 + 哈希 |

## README.md 文档要求

每个算法必须包含：
1. 算法原理说明
2. 具体数字示例（如 `[1, 2, 3, 4, 5]` 窗口 `[2,3]` 右移一位变成 `[3,4]`）
3. Mermaid 流程图展示窗口滑动过程
4. 明确标识窗口边界变化

## 验收标准

- [ ] 滑动窗口算法实现完成
- [ ] README.md 包含窗口滑动可视化
- [ ] 包含 LeetCode 1984、2270、3297 题目
- [ ] CMakeLists.txt 正确配置
