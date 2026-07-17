# ds-linked-list 规格

## 概述

链表是线性数据结构，通过指针连接节点。本规格定义单链表和双链表的完整实现及可视化文档要求。

## 目录结构

```
src/ds/linked_list/
├── README.md           # 详细文档（含 Mermaid 可视化）
├── impl/
│   ├── singly.c       # 单链表实现
│   ├── singly.hpp     # 单链表 C++ 实现
│   ├── doubly.c       # 双链表实现
│   └── doubly.hpp     # 双链表 C++ 实现
└── problems/          # LeetCode 题目
```

## 实现功能

### 单链表

| 功能 | 函数名 | 时间复杂度 |
|------|--------|-----------|
| 创建 | `singly_create()` | O(1) |
| 头部插入 | `singly_push_front()` | O(1) |
| 尾部插入 | `singly_push_back()` | O(n) |
| 头部删除 | `singly_pop_front()` | O(1) |
| 尾部删除 | `singly_pop_back()` | O(n) |
| 反转 | `singly_reverse()` | O(n) |
| 打印 | `singly_print()` | O(n) |

### 双链表

| 功能 | 函数名 | 时间复杂度 |
|------|--------|-----------|
| 创建 | `doubly_create()` | O(1) |
| 头部插入 | `doubly_push_front()` | O(1) |
| 尾部插入 | `doubly_push_back()` | O(1) |
| 头部删除 | `doubly_pop_front()` | O(1) |
| 尾部删除 | `doubly_pop_back()` | O(1) |
| 反向打印 | `doubly_print_reverse()` | O(n) |

## README.md 文档要求

每个操作必须包含：
1. 函数签名
2. 具体数字示例（如链表 `[1] → [2] → [3]` 头部插入 `99` 后变成 `[99] → [1] → [2] → [3]`）
3. Mermaid 流程图展示指针变化过程
4. 新增节点用绿色标识，被删除节点用红色标识
5. 链表反转需展示每一步的指针变化

## 数据结构

```c
// 单链表节点
typedef struct SinglyNode {
    int data;
    struct SinglyNode* next;
} SinglyNode;

// 双链表节点
typedef struct DoublyNode {
    int data;
    struct DoublyNode* prev;
    struct DoublyNode* next;
} DoublyNode;
```

## 验收标准

- [ ] 单链表所有操作实现完整
- [ ] 双链表所有操作实现完整
- [ ] README.md 包含所有操作的 Mermaid 可视化
- [ ] 链表反转展示完整步骤
- [ ] 单链表 vs 双链表复杂度对比表
- [ ] CMakeLists.txt 正确配置
