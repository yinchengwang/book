# 二叉堆 Scaffold

## 概述

本目录实现二叉堆（Binary Heap）的教学演示，包含三个核心部分：

1. **基本堆操作** - sift_up / sift_down / heapify
2. **堆排序** - O(n log n) 原地排序算法
3. **优先级队列** - 基于最大堆实现

## 文件结构

```
heap/
├── main.c    # 演示代码
├── Makefile  # 编译脚本
├── README.md # 本文件
└── NOTES.md  # 工程对照说明
```

## 编译运行

```bash
make        # 编译
make run    # 编译并运行
make clean  # 清理
```

## 核心算法

| 操作 | 时间复杂度 | 说明 |
|------|-----------|------|
| sift_up | O(log n) | 上浮：push 后恢复堆序 |
| sift_down | O(log n) | 下沉：pop/heapify 恢复堆序 |
| heapify | O(n) | Floyd 建堆算法 |
| heap_sort | O(n log n) | 原地堆排序 |

## 二叉堆性质

- **结构性质**：完全二叉树，节点 i 的父节点为 (i-1)/2，左子节点为 2*i+1，右子节点为 2*i+2
- **堆序性质**：最大堆中每个节点 ≥ 子节点；最小堆中每个节点 ≤ 子节点

## 优先级队列

基于最大堆实现，支持：
- push：O(log n) 入队
- pop：O(log n) 出队（返回并移除最大元素）
- top：O(1) 查看堆顶
