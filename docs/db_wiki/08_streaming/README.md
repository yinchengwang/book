# 流式数据库横向对比

## 分类概述

流式数据库（Streaming SQL）将流处理与关系型 SQL 能力结合，支持在无限数据流上进行持续查询、物化视图和实时分析。相较于传统批处理，流式数据库能够在毫秒级延迟下处理持续到达的数据，适合实时推荐、实时监控、在线特征工程等场景。

## 库一览

- **RisingWave** - 国产流式数据库，PostgreSQL 兼容，分布式流处理
- **Redpanda** - 高性能流平台，Kafka API 兼容，Raft 共识，零 JVM

## 功能对比表

| 维度 | RisingWave | Redpanda |
|------|------------|----------|
| 编程语言 | Rust | C++ |
| 开源协议 | Apache 2.0 | BSL/MCL |
| 首次发布 | 2021 | 2021 |
| GitHub Stars | 9k+ | 10k+ |
| 架构模型 | 流处理引擎 | 事件流平台 |
| Exactly-Once | 原生 | 事务模式 |
| SQL 支持 | 完整 PostgreSQL | 有限(C++ API) |
| 连接器 | 丰富(PG/Kafka/S3) | Kafka 兼容生态 |
| 背压 | 自动 | 可配置 |
| 物化视图 | 原生 | 无 |
| 延迟 | 毫秒级 | 微秒级 |
| 生态 | PostgreSQL 生态 | Kafka 生态 |
| 一句话定位 | SQL 流处理数据库 | 高性能 Kafka 替代 |

## 选型指南

- **SQL 流处理需求**：推荐 RisingWave（完整 SQL + 物化视图）
- **Kafka 性能优化**：推荐 Redpanda（零 JVM 高性能）
- **实时特征工程**：推荐 RisingWave（SQL 友好）
- **事件驱动架构**：推荐 Redpanda（Kafka 兼容）

## 学习路径

1. **先学 Redpanda** — 理解流平台基础概念
2. **再学 RisingWave** — 理解流 SQL 和物化视图
3. **理解 Flink/Spark Streaming** — 扩展流处理知识

## 关联项目

- `docs/storage-architecture.md` — 实时数据处理在架构中的定位
