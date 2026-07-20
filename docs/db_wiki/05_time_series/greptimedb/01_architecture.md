# GreptimeDB 架构设计

## 学习目标

- 理解 GreptimeDB 的分布式架构
- 掌握 GreptimeDB 的存储引擎设计

## 整体架构

```mermaid
graph TB
    subgraph "Frontend"
        F1[Frontend 1<br/>无状态]
        F2[Frontend 2<br/>无状态]
    end

    subgraph "Meta"
        M1[Meta 1<br/>Raft Leader]
        M2[Meta 2]
        M3[Meta 3]
    end

    subgraph "Datanode"
        D1[Datanode 1]
        D2[Datanode 2]
        D3[Datanode 3]
    end

    F1 --> M1
    F2 --> M1
    F1 --> D1
    F1 --> D2
    F2 --> D2
    F2 --> D3
    M1 -.->|Raft| M2
    M2 -.->|Raft| M3
```

## 存储引擎

```mermaid
graph TB
    subgraph "GreptimeDB 存储层"
        WAL[WAL<br/>预写日志]
        MEM[Memtable<br/>内存表]
        SST[SST Files<br/>排序字符串表]
        MET[Metadata<br/>元数据]
    end

    subgraph "引擎特性"
        P1[Parquet 列存]
        P2[LSM-Tree 架构]
        P3[时间窗口压缩]
    end

    WAL --> MEM
    MEM --> SST
    SST --> P1
    SST --> P2
    SST --> P3
```

## 表结构设计

```sql
-- 创建时序表
CREATE TABLE sensor_data (
    ts TIMESTAMP TIME INDEX,
    sensor_id STRING TAG,
    location STRING TAG,
    temperature DOUBLE,
    humidity DOUBLE,
    ts TIMESTAMP,
    PRIMARY KEY (sensor_id, location)
) WITH (
    'append_mode' = 'true',
    'ttl' = '7d'
);

-- 时间索引必须指定
-- Tags 会被索引
-- Fields 存储数据值
```

## 要点总结

- Frontend 无状态，水平扩展
- Datanode 基于 LSM-Tree 存储
- 支持 Parquet 列式存储
- 兼容多种协议（Prometheus/InfluxDB）