# 存储架构 — Buffer Pool

## 学习目标

- 理解 StoneDB 中双引擎对 Buffer Pool 的不同使用方式
- 掌握 Tianmu 引擎的知识网格缓存策略

## 核心概念

- **InnoDB Buffer Pool**：MySQL 原生 Buffer Pool，用于缓存行存数据页
- **Tianmu 内存管理**：Tianmu 引擎独立管理内存，不依赖 InnoDB Buffer Pool
- **Data Pack 缓存**：解压后的 Data Pack 在内存中缓存
- **知识网格常驻内存**：Data Pack Node 和 Knowledge Node 通常常驻内存

## InnoDB Buffer Pool（OLTP 路径）

使用 `ENGINE=InnoDB` 创建的表，走标准 MySQL Buffer Pool 机制：

```mermaid
graph LR
    A[页面请求] --> B[Buffer Pool<br/>InnoDB]
    B -->|命中| C[直接返回]
    B -->|未命中| D[磁盘加载]
    D --> B
    B --> E[脏页刷盘]
```

- 配置参数：`innodb_buffer_pool_size`
- 替换策略：LRU（改进版，带 midpoint）
- 单位：16KB 页面

## Tianmu 内存管理（OLAP 路径）

Tianmu 引擎不通过 InnoDB Buffer Pool 管理数据，而是自己维护一套内存系统：

```mermaid
graph TD
    subgraph "Tianmu 内存体系"
        KG[知识网格<br/>常驻内存]
        DC[Data Pack 缓存<br/>LRU]
        WB[写入缓冲区<br/>批量刷新]
    end

    KG --> KGN[Data Pack Node<br/>MIN/MAX/SUM...]
    KG --> KNN[Knowledge Node<br/>直方图/CMAP/P-P]

    DC --> DP[解压后的 Data Pack<br/>65536 行]

    WB --> BATCH[批量写入磁盘]
```

### 知识网格（常驻）

知识网格是 Tianmu 的"元数据缓存"：

- **Data Pack Node**：每个 Data Pack 的 MIN/MAX/SUM/COUNT/AVG 等统计信息
- **Knowledge Node**：直方图、CMAP、Pack-to-Pack 矩阵
- 大小：通常只占原始数据的 1% 以下
- 策略：启动时加载，常驻内存，随数据变更增量更新

### Data Pack 缓存

当查询需要解压 Data Pack 时，解压后的数据缓存在内存中：

- 缓存单位：一个完整的 Data Pack（65536 行 × 列宽）
- 替换策略：LRU
- 缓存上限：通过 `tianmu_buffer_size` 或类似参数控制

## 缓存查询路径

```mermaid
flowchart TD
    Q[查询到达 Tianmu 引擎] --> KG{知识网格<br/>能否回答?}
    KG -->|能, 仅靠元数据| R1[直接返回<br/>不读盘不解压]
    KG -->|不能| F{Data Pack 缓存<br/>是否命中?}
    F -->|命中| R2[使用缓存数据]
    F -->|未命中| IO[从磁盘读取压缩 DP]
    IO --> DEC[解压 Data Pack]
    DEC --> R2
    R2 --> R1
```

## 要点总结

- InnoDB 表走标准 MySQL Buffer Pool，Tianmu 表独立管理内存
- Tianmu 的知识网格常驻内存，仅占原始数据 1% 以下
- Data Pack 缓存采用 LRU 策略，存储解压后的数据
- 大多数聚合查询仅靠知识网格即可回答，无需解压数据

## 思考题

1. Tianmu 不依赖 InnoDB Buffer Pool 的设计带来了哪些利弊？
2. 知识网格常驻内存对于 10TB 级别的数据而言，内存开销大概是多大？
3. 如果 Data Pack 缓存频繁 LRU 颠簸，应该调整哪些参数？