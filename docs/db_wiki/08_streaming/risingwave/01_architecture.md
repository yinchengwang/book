# RisingWave 架构

## 学习目标

- 理解 RisingWave 作为流处理数据库的核心定位
- 掌握物化视图增量计算的实现原理
- 了解 Epoch/Barrier 机制如何实现 Exactly-Once 语义

## 正文

### 1. 架构概览

RisingWave 是一个专为流处理设计的分布式 SQL 数据库，核心特点是 **持续物化视图**：

```mermaid
graph TB
    subgraph "RisingWave 集群"
        F["Frontend Node"]
        M["Meta Node"]
        C1["Compute-1"]
        C2["Compute-2"]
        C3["Compute-3"]
    end
    
    subgraph "外部系统"
        K["Kafka"]
        P["PostgreSQL"]
        S["S3"]
    end
    
    K --> C1
    P --> C2
    C1 --> S
    F --> M
    F --> C1
    F --> C2
    F --> C3
```

### 2. 核心组件

| 组件 | 职责 |
|------|------|
| Frontend | SQL 解析、优化、查询计划生成 |
| Meta Node | 集群管理、Worker 调度、DDL 执行 |
| Compute Node | 流处理算子执行、状态管理 |

```mermaid
flowchart LR
    subgraph "查询处理流程"
        SQL["SQL 语句"] --> P["Parser"]
        P --> A["Analyzer"]
        A --> O["Optimizer"]
        O --> Q["Query Plan"]
    end
    
    Q --> FE["Frontend"]
    FE --> CO["Stream Executor"]
    CO --> ST["State Backend"]
```

### 3. 物化视图增量更新

RisingWave 的核心创新是 **增量视图维护（Incremental View Maintenance）**：

```mermaid
sequenceDiagram
    participant S as Source (Kafka)
    participant M as Materialized View
    participant B as Barrier
    
    S->>M: Insert batch 1 (100 rows)
    M->>M: 计算增量
    M->>M: 更新状态
    S->>M: Insert batch 2 (50 rows)
    M->>M: 计算增量
    B->>M: Barrier
    M->>M: 提交 epoch 2
    Note over M: 增量更新，无需全量重算
```

**对比传统批处理**：
- 传统：每次更新触发全量重算
- RisingWave：只计算变化的部分

### 4. 算子树结构

RisingWave 将 SQL 查询编译为流处理算子树：

```mermaid
flowchart TB
    subgraph "算子树"
        E1["Exchange<br/>分布式输入"]
        subgraph "算子层"
            F["Filter<br/>过滤"]
            P["Project<br/>投影"]
            A["Aggregate<br/>聚合"]
            J["Join<br/>连接"]
        end
        M["Materialize<br/>物化输出"]
    end
    
    E1 --> F
    F --> P
    P --> A
    A --> J
    J --> M
    
    subgraph "状态后端"
        S1["HashAgg State"]
        S2["Join State"]
    end
    
    A -.->|"状态"| S1
    J -.->|"状态"| S2
```

**核心算子**：
| 算子 | 功能 |
|------|------|
| Exchange | 数据分片和重分布 |
| Filter | 条件过滤 |
| Project | 列选择和计算 |
| Aggregate | 聚合计算（Sum/Avg/Count/Max/Min） |
| Join | 流与流、流与表的连接 |
| Materialize | 结果物化到状态后端 |

### 5. Epoch/Barrier 机制

RisingWave 使用 **Barrier** 实现分布式快照和 Exactly-Once 语义：

```mermaid
sequenceDiagram
    participant N1 as Node-1
    participant N2 as Node-2
    participant N3 as Node-3
    participant M as Meta Node
    
    rect rgb(200, 220, 255)
        Note over N1,N3: Epoch 1
        N1->>N2: Barrier 1
        N2->>N3: Barrier 1
        N1-->>M: Barrier 1 完成
        N2-->>M: Barrier 1 完成
        N3-->>M: Barrier 1 完成
        M->>N1: 提交 Epoch 1
        M->>N2: 提交 Epoch 1
        M->>N3: 提交 Epoch 1
    end
    
    rect rgb(255, 230, 200)
        Note over N1,N3: Epoch 2
        N1->>N2: Barrier 2
        N2->>N3: Barrier 2
        N3-->>M: Barrier 2 完成
        M->>N1: 提交 Epoch 2
    end
```

**Barrier 作用**：
1. 标记一个 Epoch 的开始
2. 协调所有算子的进度
3. 触发状态快照
4. 支持故障恢复

### 6. 状态后端

RisingWave 支持多种状态后端：

```mermaid
graph LR
    subgraph "状态后端选项"
        R["RocksDB<br/>本地持久化"]
        H["HashAgg<br/>内存聚合"]
        S["Sort<br/>内存排序"]
    end
    
    subgraph "State Access"
        A["算子状态访问"]
        A --> R
        A --> H
        A --> S
    end
```

**状态管理特点**：
- RocksDB：支持大状态，持久化可靠
- 内存状态：高性能，适合小状态
- 增量 Checkpoint：定期保存状态快照

## 要点总结

1. **SQL-based 流处理**：使用标准 SQL 定义流计算
2. **增量计算**：物化视图只计算变化部分，避免全量重算
3. **Exactly-Once**：Barrier 机制保证端到端一致性
4. **分布式执行**：算子树在多节点并行执行
5. **灵活状态**：支持多种状态后端适应不同场景

## 思考题

1. RisingWave 的增量计算与传统批处理系统相比有何优势？
2. Barrier 机制如何保证在节点故障时的状态一致性？
3. 算子树中的 Exchange 算子如何实现数据的均匀分布？
