# Buffer Pool 实现

## 学习目标
- 理解 Buffer Pool 在数据库中的角色
- 掌握缓存淘汰算法和并发控制

## 核心概念

- **Buffer Pool**：内存中的页面缓存区，减少磁盘 I/O
- **Page**：最小的数据单元，通常 4KB-16KB
- **Dirty Page**：被修改后尚未写盘的页面
- **淘汰策略**：LRU/Clock/ARC 等

## 架构设计

```mermaid
graph TD
    subgraph Buffer Pool
        FP[空闲页链表]
        HP[哈希表]
        LP[LRU 链表]
        DP[脏页链表]
    end

    Request[页面请求] --> HP
    HP -->|命中| Return[直接返回]
    HP -->|未命中| LP
    LP -->|淘汰| FP
    FP -->|读盘| Load[从磁盘加载]
    Load --> Return
```

## 淘汰算法

```mermaid
flowchart TD
    A[请求页 P] --> B{P 在 Buffer Pool?}
    B -->|是| C[增加引用计数]
    B -->|否| D[选择淘汰页 V]
    D --> E{V 是脏页?}
    E -->|是| F[写回磁盘]
    E -->|否| G[加载页 P 到 V 的位置]
    F --> G
    G --> H[返回页 V]
```

## 要点总结

- Buffer Pool 的核心组件：哈希表、链表、脏页管理
- 淘汰策略的选择影响 I/O 性能

## 思考题

1. LRU 和 Clock 算法的优劣对比？
2. 如何处理 Buffer Pool 的并发访问？