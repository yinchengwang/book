# ds-stack 规格

## 概述

栈是后进先出（LIFO）的数据结构。本规格定义栈的完整实现及可视化文档要求。

## 目录结构

```
src/ds/stack/
├── README.md           # 详细文档（含 Mermaid 可视化）
├── impl/
│   ├── stack_array.c  # 数组实现
│   ├── stack_array.hpp
│   ├── stack_list.c   # 链表实现
│   ├── stack_list.hpp
│   └── monotonic_stack.c  # 单调栈
└── problems/          # LeetCode 题目
```

## 实现功能

### 数组实现

| 功能 | 函数名 | 时间复杂度 |
|------|--------|-----------|
| 创建 | `stack_create()` | O(1) |
| 入栈 | `stack_push()` | O(1) |
| 出栈 | `stack_pop()` | O(1) |
| 获取栈顶 | `stack_top()` | O(1) |
| 判空 | `stack_empty()` | O(1) |
| 获取大小 | `stack_size()` | O(1) |

### 单调栈

| 功能 | 函数名 | 时间复杂度 |
|------|--------|-----------|
| 创建 | `monotonic_stack_create()` | O(1) |
| 入栈 | `monotonic_stack_push()` | O(1) 均摊 |
| 获取下一更大元素 | `next_greater_element()` | O(n) |

## README.md 文档要求

每个操作必须包含：
1. 函数签名
2. 具体数字示例（如栈 `[1, 2, 3]` 入栈 `99` 后变成 `[1, 2, 3, 99]`，`99` 是新的栈顶）
3. Mermaid 流程图展示入栈/出栈过程
4. 用颜色标识栈顶变化

## 数据结构

```c
// 数组实现
typedef struct {
    int* data;
    int top;       // 栈顶索引
    int capacity;
} Stack;

// 单调栈
typedef struct {
    int* data;
    int top;
    int* result;  // 存储结果
} MonotonicStack;
```

## 验收标准

- [ ] 数组实现所有操作完成
- [ ] 单调栈实现完成
- [ ] README.md 包含入栈/出栈的 Mermaid 可视化
- [ ] 包含 LeetCode 0206 有效括号题目
- [ ] CMakeLists.txt 正确配置
