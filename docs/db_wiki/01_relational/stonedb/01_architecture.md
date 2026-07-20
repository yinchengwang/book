# StoneDB 整体架构

## 学习目标

- 理解 StoneDB 的三层逻辑架构设计
- 掌握 Tianmu 引擎与 MySQL 内核的集成方式
- 了解知识网格在查询路径中的位置

## 核心概念

- **三层架构**：应用层（连接管理/认证/访问控制）→ 服务层（SQL 接口/解析器/优化器）→ 存储引擎层（Tianmu/InnoDB）
- **Tianmu 引擎**：StoneDB 自研的列式存储引擎，用于 OLAP 加速
- **知识网格**：由 Data Pack Node + Knowledge Node 组成的两级元数据系统
- **双引擎架构**：InnoDB 处理 OLTP、Tianmu 处理 OLAP，用户可指定表使用哪种引擎

## 三层架构

```mermaid
graph TB
    subgraph "应用层 Application Layer"
        A1[连接管理<br/>线程池]
        A2[认证授权<br/>用户/密码/IP/端口]
        A3[访问控制<br/>权限检查]
    end

    subgraph "服务层 Services Layer"
        S1[SQL 接口<br/>DML/DDL/存储过程]
        S2[查询缓存<br/>Hash + 结果集]
        S3[解析器<br/>词法/语法分析]
        S4[MySQL 优化器<br/>成本优化]
        S5[MySQL 执行器<br/>操作引擎]
    end

    subgraph "存储引擎层 Storage Engine Layer"
        T1[InnoDB 引擎<br/>行存/OLTP]
        T2[Tianmu 引擎<br/>列存/OLAP]
    end

    subgraph "Tianmu 内部模块"
        TM1[StoneDB 优化器<br/>知识网格优化]
        TM2[StoneDB 执行器<br/>列式执行]
        TM3[知识网格管理器<br/>Data Pack/Node]
        TM4[压缩/解压模块<br/>20+ 算法]
        TM5[数据加载器<br/>LOAD DATA]
    end

    Client[客户端] --> A1
    A1 --> A2 --> A3 --> S1
    S1 --> S2 --> S3 --> S4 --> S5
    S5 --> T1
    S5 --> T2
    T2 --> TM1 --> TM2 --> TM3 --> TM4 --> TM5
```

## 查询处理流程

```mermaid
sequenceDiagram
    participant C as 客户端
    participant P as 解析器
    participant O as MySQL 优化器
    participant E as MySQL 执行器
    participant T as Tianmu 引擎

    C->>P: SQL 查询
    P->>P: 词法/语法分析
    P->>O: 解析树
    O->>O: 生成执行计划
    O->>E: 执行计划
    E->>T: 调用 Tianmu 读取数据
    T->>T: 知识网格过滤 Data Pack
    T->>T: 解压候选 Data Pack
    T->>T: 列式执行
    T->>E: 返回结果
    E->>C: 结果集
```

## 知识网格架构

```mermaid
graph LR
    subgraph "知识网格 Knowledge Grid"
        L1[KN: 知识节点<br/>直方图/CMAP/Pack-to-Pack]
        L2[DPN: 数据包节点<br/>MIN/MAX/SUM/COUNT/AVG]
        L3[DP: 数据包<br/>65536 行/包]
    end

    L1 --> L2
    L2 --> L3

    Query[查询] --> L1
    L1 -->|过滤无关 DP| L2
    L2 -->|精确过滤| L3
    L3 -->|解压| Result[结果集]
```

## 双引擎存储策略

```mermaid
graph TD
    subgraph "StoneDB 表存储"
        T[用户表] --> E{创建时指定引擎}
        E -->|ENGINE=Tianmu| COL[列存 Tianmu]
        E -->|ENGINE=InnoDB| ROW[行存 InnoDB]
    end

    subgraph "Tianmu 列存"
        COL --> C1[按列存储]
        C1 --> C2[列内数据同类型]
        C2 --> C3[密集压缩 40:1]
        C3 --> C4[只读需要的列]
    end

    subgraph "InnoDB 行存"
        ROW --> R1[按行存储]
        R1 --> R2[聚簇索引]
        R2 --> R3[行级锁]
        R3 --> R4[MVCC 事务]
    end
```

## 要点总结

- StoneDB 采用三层架构：应用层 → 服务层 → 存储引擎层
- Tianmu 是自研列存引擎，与 InnoDB 共存，用户通过 `ENGINE=Tianmu` 指定
- 知识网格是 Tianmu 的核心：Data Pack Node（元数据）+ Knowledge Node（高级过滤信息）
- 查询先经过知识网格过滤，再解压必要的 Data Pack，大幅减少 I/O

## 思考题

1. StoneDB 将优化器分为 MySQL 优化器和 StoneDB 优化器两部分，这种分工是如何协作的？
2. 知识网格的过滤能力依赖于 Data Pack 粒度的统计信息，如果数据分布极不均匀，过滤效果会怎样？
3. 在双引擎架构下，跨引擎 JOIN（Tianmu 表 JOIN InnoDB 表）是如何实现的？