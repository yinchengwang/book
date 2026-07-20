# Milvus 与项目关联

## 学习目标

- 分析 Milvus 设计对项目存储引擎的启发性
- 找出项目中可借鉴的技术点

## 架构对比

```mermaid
graph TB
    subgraph "Milvus"
        M1[Proxy 代理层]
        M2[QueryNode 查询节点]
        M3[DataNode 数据节点]
        M4[ObjectStore 对象存储]
        M5[IndexNode 索引节点]
    end

    subgraph "项目"
        P1[SQL 执行器]
        P2[Access Method 层]
        P3[Buffer Pool 管理]
        P4[磁盘文件存储]
        P5[索引构建线程]
    end

    M2 -..->|借鉴: 堆叠执行| P2
    M3 -..->|借鉴: Segment 管理| P3
    M4 -..->|借鉴: 分层存储| P4
```

## 可借鉴设计

### 1. 插件化引擎架构

Milvus 内部集成多个索引引擎，通过统一接口切换：

| Milvus 设计 | 项目对应 | 可借鉴程度 |
|------------|---------|-----------|
| 插件化索引引擎 (Faiss/HNSW/DiskANN) | storage_engine_t 接口 | ⭐⭐⭐ |
| Segment 统一管理单元 | Buffer Pool + 页面管理 | ⭐⭐⭐ |
| 混合过滤下推 | 项目中待实现的过滤逻辑 | ⭐⭐ |
| 计算存储分离 | 项目单机架构，未来可演进 | ⭐ |

### 2. 索引调度

Milvus 的索引构建由 IndexNode 异步执行：

```c
// 项目现有: 同步索引构建
btree_build_index(btree, tuples);

// 借鉴 Milvus 异步模式: 索引任务队列
// index_task_t task = {.type = BTREE, .data = tuples};
// index_node_enqueue(task);  // 异步构建
```

### 3. Segment 管理

Milvus 的 Growing → Sealed → Indexed 状态机对项目中 Buffer Pool 的脏页管理有启发。

## 项目提升计划

```mermaid
graph LR
    P1[Phase 1: 引擎接口统一<br/>借鉴插件化设计] --> P2[Phase 2: 异步索引<br/>IndexNode 任务队列]
    P2 --> P3[Phase 3: 混合过滤<br/>标量+向量协同]
```

## 要点总结

- Milvus 的插件化引擎架构对项目的多模态引擎设计有直接参考价值
- Segment 状态机可借鉴到 Buffer Pool 的页面管理
- 异步索引构建模式可提升项目索引吞吐
- 混合过滤是向量检索的必备能力，值得重点借鉴

## 思考题

1. 项目中 storage_engine_t 接口能否借鉴 Milvus 的插件化模式，支持动态加载索引算法？
2. 我们现有的 Buffer Pool 管理能否实现类似 Milvus Segment 的多级状态？
3. 异步索引构建有哪些实现难点（一致性、调度策略）？