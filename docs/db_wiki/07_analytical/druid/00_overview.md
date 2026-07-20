# Apache Druid 项目概览

## 学习目标

- 了解 Druid 作为实时 OLAP 数据库的定位
- 掌握 Druid 的 Segment 和 Lambda 架构

## 项目定位

> Apache Druid 是一个实时分析数据库，针对大规模数据的快速查询而设计。

**基本信息**：
- 开发方：Apache 基金会 / Imply
- 首次发布：2011 年
- 开源协议：Apache 2.0
- GitHub Stars：约 13k

## 核心设计

```mermaid
graph TB
    A[Apache Druid] --> B[Segment 存储</列式]
    A --> C[实时摄取</流式 & 批]
    A --> D[分布式查询</Broker 路由]
    A --> E[位图索引</快速过滤]
    A --> F[Lambda 架构</实时+历史]

    B --> B1[不可变 Segment]
    B --> B2[时间分区]
    C --> C1[Kafka 集成]
    C --> C2[Kafka Indexing Service]
    D --> D1[Broker + Historical]
    E --> E1[Roaring Bitmap]
```

## 架构

```mermaid
graph TB
    subgraph "Overlord"
        O1[Overlord]
    end

    subgraph "Ingestion"
        M[Middle Manager]
        P[Peon]
    end

    subgraph "存储"
        D[Deep Storage<br/>S3/HDFS]
        H[Historical 节点]
    end

    subgraph "查询"
        B[Broker]
    end

    M --> P
    P --> D
    D --> H
    H --> B
    O1 --> M
```

## 要点总结

- Segment 列式存储
- 实时摄取 Kafka 数据
- 位图索引加速过滤
- Lambda 架构混合存储
