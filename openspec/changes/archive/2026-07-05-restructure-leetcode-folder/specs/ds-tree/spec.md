# ds-tree 规格

## 概述

树是非线性层次数据结构。本规格定义二叉树、二叉搜索树、AVL树、红黑树、B树、堆、Trie 的完整实现及可视化文档要求。

## 目录结构

```
src/ds/tree/
├── binary_tree/
│   ├── README.md
│   ├── impl/
│   │   ├── binary_tree.c
│   │   └── binary_tree.hpp
│   └── problems/
├── bst/
│   ├── README.md
│   └── impl/
├── avl/
│   ├── README.md
│   └── impl/
├── rb_tree/
│   ├── README.md
│   └── impl/
├── b_tree/
│   ├── README.md
│   └── impl/
├── trie/
│   ├── README.md
│   └── impl/
├── heap/
│   ├── README.md
│   ├── impl/
│   │   ├── heap.c
│   │   ├── max_heap.c
│   │   ├── min_heap.c
│   │   └── priority_queue.c
│   └── problems/
└── interval_tree/
    ├── README.md
    └── impl/
```

## 实现功能

### 二叉树

| 功能 | 函数名 | 时间复杂度 |
|------|--------|-----------|
| 创建节点 | `bt_create_node()` | O(1) |
| 前序遍历 | `bt_preorder()` | O(n) |
| 中序遍历 | `bt_inorder()` | O(n) |
| 后序遍历 | `bt_postorder()` | O(n) |
| 层序遍历 | `bt_levelorder()` | O(n) |
| 计算节点数 | `bt_count()` | O(n) |
| 计算高度 | `bt_height()` | O(n) |

### 堆

| 功能 | 函数名 | 时间复杂度 |
|------|--------|-----------|
| 创建 | `heap_create()` | O(1) |
| 插入 | `heap_push()` | O(log n) |
| 弹出堆顶 | `heap_pop()` | O(log n) |
| 获取堆顶 | `heap_top()` | O(1) |
| 堆化 | `heapify()` | O(n) |

## README.md 文档要求

每个操作必须包含：
1. 函数签名
2. 具体树结构示例（如输入 `[1,2,3,4,5]` 的二叉树）
3. Mermaid 图形展示树结构
4. 遍历过程可视化
5. 堆的插入/删除需展示上浮/下沉过程

## 数据结构

```c
// 二叉树节点
typedef struct BTNode {
    int val;
    struct BTNode* left;
    struct BTNode* right;
} BTNode;

// 堆
typedef struct {
    int* data;
    int size;
    int capacity;
    int (*compare)(int, int);  // 比较函数
} Heap;
```

## 验收标准

- [ ] 二叉树所有遍历实现完成
- [ ] 堆的插入/删除实现完成（含上浮/下沉可视化）
- [ ] README.md 包含遍历的 Mermaid 可视化
- [ ] 包含 LeetCode 0100、0222 题目
- [ ] 包含 LeetCode 1705、3066 堆相关题目
- [ ] CMakeLists.txt 正确配置
