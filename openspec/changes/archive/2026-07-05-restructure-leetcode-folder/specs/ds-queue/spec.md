# ds-queue 规格

## 概述

队列是先进先出（FIFO）的数据结构。本规格定义队列的完整实现及可视化文档要求。

## 目录结构

```
src/ds/queue/
├── README.md           # 详细文档（含 Mermaid 可视化）
├── impl/
│   ├── queue.c        # 普通队列
│   ├── queue.hpp
│   ├── circle_queue.c # 循环队列
│   ├── circle_queue.hpp
│   ├── deque.c        # 双端队列
│   ├── deque.hpp
│   └── priority_queue.c # 优先队列
└── problems/          # LeetCode 题目
```

## 实现功能

### 普通队列

| 功能 | 函数名 | 时间复杂度 |
|------|--------|-----------|
| 创建 | `queue_create()` | O(1) |
| 入队 | `queue_push()` | O(1) |
| 出队 | `queue_pop()` | O(1) |
| 获取队首 | `queue_front()` | O(1) |
| 判空 | `queue_empty()` | O(1) |

### 循环队列

| 功能 | 函数名 | 时间复杂度 |
|------|--------|-----------|
| 创建 | `circle_queue_create()` | O(1) |
| 入队 | `circle_queue_push()` | O(1) |
| 出队 | `circle_queue_pop()` | O(1) |
| 判满/空 | `circle_queue_is_full/is_empty()` | O(1) |

## README.md 文档要求

每个操作必须包含：
1. 函数签名
2. 具体数字示例
3. Mermaid 流程图展示入队/出队过程
4. 循环队列需展示环绕逻辑

## 数据结构

```c
// 普通队列
typedef struct {
    int* data;
    int front;  // 队首
    int rear;   // 队尾
    int size;
    int capacity;
} Queue;
```

## 验收标准

- [ ] 普通队列实现完成
- [ ] 循环队列实现完成
- [ ] README.md 包含入队/出队的 Mermaid 可视化
- [ ] CMakeLists.txt 正确配置
