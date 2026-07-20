# 与我项目的关联

## 学习目标

- 分析 StoneDB 的设计对项目存储引擎的启发性
- 找出项目中可借鉴的技术点

## 项目 vs StoneDB 架构对比

```mermaid
graph TB
    subgraph "项目（我们的存储引擎）"
        P1[KV 存储层]
        P2[Buffer Pool<br/>Clock-Sweep]
        P3[Access Method<br/>Relation 抽象]
        P4[Heap AM / BTree AM]
        P5[WAL 日志]
    end

    subgraph "StoneDB"
        S1[MySQL 兼容层]
        S2[Tianmu 列存引擎]
        S3[知识网格<br/>DPN + Knowledge Node]
        S4[Data Pack 压缩/解压]
        S5[列式执行器]
    end

    P3 -..-> S2
    P4 -..-> S3
    P5 -..-> S5
```

## 可从 StoneDB 借鉴的设计

### 1. 多引擎共存模式

StoneDB 的双引擎（InnoDB + Tianmu）设计启发了项目中"多模态存储引擎"的架构：

```mermaid
graph TD
    subgraph "项目多模态引擎"
        M[多模态引擎层<br/>mm_storage.h] --> KV[KV 引擎]
        M --> REL[行存引擎<br/>Heap/BTree]
        M --> VEC[向量引擎]
        M --> TS[时序引擎]
        M --> DOC[文档引擎]
        M --> SPATIAL[空间引擎]
        M --> GRAPH[图引擎]
        M --> YANG[Yang 引擎]

        REL --> NODE1[HINT: 像 InnoDB 负责 OLTP]
        VEC --> NODE2[HINT: 像 Tianmu 专注于特定模型]
    end
```

StoneDB 的启示：不同存储引擎服务于不同的查询模式，通过统一的存储接口切换。

### 2. 知识网格的过滤思路

知识网格的 DPN + 多级过滤可以借鉴到项目的索引设计中：

```mermaid
flowchart TD
    Q[查询到达] --> META[元数据过滤层<br/>类似 DPN]
    META -->|MIN/MAX 过滤| BLOCK[候选 block]
    BLOCK -->|Zone Map 过滤| PAGE[候选 page]
    PAGE -->|精确读取| DATA[返回结果]
```

我们的项目中已经实现了类似的 Zone Map（在 BTree AM 中），但不完整。可以借鉴：

- **DPN 的 MIN/MAX 统计**：每个数据块维护 MIN/MAX/COUNT 信息
- **Data Pack 三分类**：相关/无关/可疑的过滤逻辑
- **多级级联过滤**：从粗到细的过滤流水线

### 3. 批量写入与压缩

```mermaid
graph LR
    subgraph "项目当前"
        W1[逐行写入] --> W2[WAL 日志]
        W2 --> W3[Buffer Pool 刷盘]
    end

    subgraph "借鉴 StoneDB"
        S1[Data Pack 批处理] --> S2[压缩后写入]
        S2 --> S3[DPN 批量构建]
        S3 --> S4[元数据缓存]
    end
```

可借鉴点：
- **批量压缩写入**：在时序引擎中，批量积累数据后压缩写入
- **自定义压缩策略**：根据数据类型选择压缩算法（我们的 Delta 编码已在时序引擎中实现）
- **元数据缓存**：块级别的统计信息常驻内存

### 4. 列式处理的启发

虽然项目目前以行存为主，但列式处理的思路对向量引擎和时序引擎有启发：

```mermaid
graph TD
    COL[列式处理启发] --> COL1[列裁剪: 只读取查询涉及的列]
    COL --> COL2[向量化: 批量处理而非逐行]
    COL --> COL3[压缩: 同类型数据密集压缩]
    COL --> COL4[元数据过滤: 知识网格思路]

    COL1 --> PROJ1[HINT: 行存引擎的 Project 下推]
    COL2 --> PROJ2[HINT: SIMD 向量化执行]
    COL3 --> PROJ3[HINT: 时序引擎压缩]
    COL4 --> PROJ4[HINT: Zone Map 增强]
```

## 技术债务与改进

| 项目当前状态 | 可借鉴的 StoneDB 方案 | 优先级 |
|-------------|---------------------|--------|
| Zone Map 只支持简单的 MIN/MAX | 多级知识网格过滤（DPN + 直方图 + CMAP） | 中 |
| 时序引擎压缩算法单一 | 多算法自适应选择（类似 20+ 压缩算法） | 中 |
| Buffer Pool 替换策略固定 | Data Pack 缓存的 LRU 参数可调 | 低 |
| 没有列存支持 | Tianmu 列存引擎的设计思路 | 长期 |

## 项目提升计划

```mermaid
graph LR
    P1[Phase 1: Zone Map 增强<br/>借鉴 DPN 设计] --> P2[Phase 2: 压缩策略<br/>数据类型感知]
    P2 --> P3[Phase 3: 多级过滤<br/>知识网格风格]
    P3 --> P4[Phase 4: 列存实验<br/>时序引擎列存化]
```

## 要点总结

- StoneDB 的双引擎架构对项目的多模态引擎设计有直接参考价值
- 知识网格的 DPN + 多级过滤思路可以增强项目的 Zone Map 实现
- 批量压缩写入模式适合项目的时序引擎和向量引擎
- 列式处理的列裁剪、向量化、压缩思路值得长期借鉴

## 思考题

1. 如果要在项目中实现一个"知识网格"风格的过滤层，应该如何设计 API？
2. 我们的时序引擎能否借鉴 StoneDB 的 Data Pack 设计，实现块级别的统计信息缓存？
3. 项目的多模态引擎接口（storage_engine_t）和 StoneDB 的 handler 接口设计，有哪些异同？