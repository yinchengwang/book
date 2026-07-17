# T-Tree 内存索引

## 概述

T-Tree（Tree with Balance on Search）是一种专为**内存数据库**设计的有序索引结构，平衡了二叉搜索树的简洁性和 B-Tree 的高效性。

## 设计背景

- **TimesTen** 内存数据库使用
- **MySQL NDB Cluster** 内部索引
- 适用于**内存优先**的数据访问模式

## 数据结构

```
节点结构（类似 B-Tree 的多路节点 + 二叉树指针）：

┌─────────────────────────────────────────────┐
│  T-Tree 节点                                │
├─────────────────────────────────────────────┤
│  is_leaf    : 是否叶子节点                  │
│  nkeys      : 当前 key 数量                 │
│  records[]  : 有序 key-value 数组           │
│  left       : 左子树指针（内部节点用）       │
│  right      : 右子树指针（内部节点用）       │
│  prev       : 前驱叶子节点（叶子节点用）     │
│  next       : 后继叶子节点（叶子节点用）     │
└─────────────────────────────────────────────┘
```

## 关键特性

| 特性 | 描述 |
|------|------|
| 多路节点 | 每个节点存储 `[min_keys, 2*min_keys-1]` 个 key |
| 二叉导航 | 内部节点用 left/right 二叉指针，而非 children 数组 |
| 叶子链表 | 叶子节点通过 prev/next 链表连接，支持高效范围查询 |
| 内存友好 | 节点紧凑，无磁盘页概念 |

## 操作复杂度

| 操作 | 时间复杂度 |
|------|------------|
| 查找 | O(log n) |
| 插入 | O(log n) + 可能旋转 |
| 删除 | O(log n) + 可能借位/合并 |
| 范围查询 | O(log n + k)，k 为结果数 |

## 与 B+Tree 的对比

| 维度 | T-Tree | B+Tree |
|------|--------|--------|
| 适用场景 | 内存 | 磁盘 |
| 节点结构 | 紧凑，多 key | 固定页大小 |
| 导航方式 | 二叉（left/right） | 多叉（children[]） |
| 范围查询 | 叶子链表 O(1) 跳转 | 叶子链表 O(1) 跳转 |

## 代码实现

项目路径：`src/index/tree/ttree/`

核心模块：
- `ttree_core.c` - 创建、销毁、比较
- `ttree_insert.c` - 插入（含分裂）
- `ttree_delete.c` - 删除（含借位/合并）
- `ttree_lookup.c` - 查找、范围查询

## 面试要点

1. **T-Tree 的设计目标是什么？** - 内存优化的多路搜索树
2. **为什么 T-Tree 适合内存数据库？** - 节点紧凑，减少指针跳转
3. **T-Tree 与 B+Tree 的核心区别？** - 导航方式（二叉 vs 多叉）
4. **T-Tree 如何支持范围查询？** - 叶子节点通过 prev/next 链表连接

## 参考资料

- Original paper: "The T-Tree: A New Dynamic Data Structure for In-Memory Databases"
- TimesTen In-Memory Database documentation