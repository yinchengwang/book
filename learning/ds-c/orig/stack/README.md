# 栈 (Stack)

## 概述

栈是一种后进先出 (LIFO) 的数据结构，只允许在栈顶进行插入和删除操作。

## 基本操作

| 操作 | 时间复杂度 | 说明 |
|------|-----------|------|
| push (入栈) | O(1) | 在栈顶添加元素 |
| pop (出栈) | O(1) | 移除栈顶元素 |
| top (查看栈顶) | O(1) | 返回栈顶元素 |
| isEmpty | O(1) | 判断是否为空 |

## 可视化示例

### 栈结构

```
栈顶 (Top)
┌───────┐
│   5   │  ← pop()
├───────┤
│   4   │
├───────┤
│   3   │
├───────┤
│   2   │
├───────┤
│   1   │  ← 栈底
└───────┘
```

### 入栈出栈示例

对栈依次执行：push(1), push(2), push(3), pop(), push(4)

```mermaid
graph TD
    A["push(1)"] --> B["push(2)"]
    B --> C["push(3)"]
    C --> D["pop() = 3"]
    D --> E["push(4)"]
    E --> F["栈状态: [1, 2, 4]"]
    style F fill:#e8f5e9
```

### 括号匹配示例

验证 `([{}])` 是否合法：

```mermaid
sequenceDiagram
    participant S as 栈
    participant I as 输入
    
    I->>S: '(' 入栈
    I->>S: '[' 入栈
    I->>S: '{' 入栈
    I->>S: '}' 匹配到 '{'，出栈
    I->>S: ']' 匹配到 '['，出栈
    I->>S: ')' 匹配到 '('，出栈
    Note over S: 栈空，匹配成功 ✓
```

## 实现文件

| 文件 | 说明 |
|------|------|
| [impl/stack_by_array.c](impl/stack_by_array.c) | 顺序栈实现 |
| [impl/stack_by_list.c](impl/stack_by_list.c) | 链式栈实现 |
| [impl/monotonic_stack.c](impl/monotonic_stack.c) | 单调栈实现 |

## LeetCode 题目

| 题号 | 题目 | 难度 |
|------|------|------|
| 0020 | [有效的括号](../0020_valid_parentheses/) | 简单 |
