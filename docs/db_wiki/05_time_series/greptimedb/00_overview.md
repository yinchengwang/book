# GreptimeDB 项目概览

## 学习目标

- 了解 GreptimeDB 作为国产云原生时序数据库的定位
- 掌握 GreptimeDB 的分布式架构和 PromQL 兼容

## 项目定位

> GreptimeDB 是国产开源的云原生时序数据库，兼容 Prometheus 和 OpenTSDB 协议，支持 SQL 和 PromQL 查询。

**基本信息**：
- 开发方：Greptime（杭州势到科技）
- 首次发布：2022 年
- 开源协议：Apache 2.0
- GitHub Stars：约 5k

## 核心设计

```mermaid
graph TB
    A[GreptimeDB] --> B[分布式架构<br/>Shared-nothing]
    A --> C[多模态存储<br/>时序+宽表]
    A --> D[PromQL 兼容<br/>监控指标]
    A --> E[SQL 接口<br/>标准 SQL]
    A --> F[多种协议<br/>InfluxDB/OpenTSDB]

    B --> B1[无状态 Frontend]
    B --> B2[Datanode 存储]
    C --> C1[时序数据]
    C --> C2[Table 格式]
    D --> D1[100+ 函数]
    E --> E1[标准 SQL]
```

## 架构特点

```mermaid
graph TB
    subgraph "Frontend"
        F1[Frontend 1]
        F2[Frontend 2]
    end

    subgraph "Datanode"
        D1[Datanode 1]
        D2[Datanode 2]
        D3[Datanode 3]
    end

    subgraph "Meta"
        M1[Meta Server<br/>Raft]
    end

    F1 --> M1
    F2 --> M1
    F1 --> D1
    F1 --> D2
    F2 --> D2
    F2 --> D3
```

## 要点总结

- 国产开源，云原生设计
- PromQL 100% 兼容
- 多模态存储支持
- 多种协议接入

## 思考题

1. GreptimeDB 的分布式架构与 VictoriaMetrics 有何不同？
2. GreptimeDB 的 PromQL 兼容对监控系统迁移有何意义？
3. 多模态存储（时序+宽表）的设计理念是什么？