# 线段树 (Segment Tree)

## 概述

线段树是一种二叉树结构，用于高效支持区间查询与区间更新操作。本 scaffold 实现了一个支持**动态开点**的线段树，包含懒标记机制。

## 核心功能

| 操作 | API | 说明 |
|------|-----|------|
| 构建 | `segtree_build()` | 从数组初始化树 |
| 单点赋值 | `segtree_point_set()` | O(log n) |
| 区间加法 | `segtree_range_add()` | 懒标记下发 |
| 区间赋值 | `segtree_range_assign()` | 懒标记下发 |
| 区间求和 | `segtree_query_sum()` | O(log n) |
| 区间最大值 | `segtree_query_max()` | O(log n) |

## 编译运行

```bash
make        # 编译
make run    # 编译并运行
make clean  # 清理
```

## 算法复杂度

- **时间复杂度**：
  - 单点更新：O(log n)
  - 区间查询：O(log n)
  - 区间更新：O(log n)（均摊）
- **空间复杂度**：O(n)（动态开点实际为 O(k)，k 为访问过的结点数）

## 懒标记机制

懒标记（Lazy Propagation）用于延迟向下传播区间更新：
- 区间操作直接作用于当前结点，记录标记
- 真正需要访问子结点时，才将标记下发
- 支持**加法**与**赋值**两种操作类型

## 应用场景

- **RMQ（Range Maximum Query）**：区间最值查询
- **区间求和/统计**：如股票价格区间统计
- **差分数组还原**：将差分操作转化为区间更新
- **动态区间修改**：支持大范围批量修改

## 文件结构

```
segmenttree/
├── main.c     # 演示代码（约 140 行核心逻辑）
├── Makefile   # 编译配置
├── README.md  # 本文件
└── NOTES.md   # 工程对照说明
```
