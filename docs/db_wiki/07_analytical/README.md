# 分析型数据库横向对比

## 分类概述

分析型数据库（OLAP）专为复杂查询和数据挖掘设计，采用列式存储、向量化执行等技术处理大规模数据集。相较于 OLTP 数据库，在聚合查询、多维分析场景具有数量级的性能优势，是数据仓库和商业智能的核心组件。

## 库一览

- **ClickHouse** - 列式分析数据库，矢量执行，高压缩，OLAP 性能杀手
- **Apache Druid** - 高性能时序分析数据库，亚秒级查询，实时数据摄入
- **StarRocks** - 国产高性能分析数据库，现代化向量化执行，MySQL 兼容

## 功能对比表

| 维度 | ClickHouse | Apache Druid | StarRocks |
|------|------------|--------------|-----------|
| 编程语言 | C++ | Java | C++ |
| 开源协议 | Apache 2.0 | Apache 2.0 | Apache 2.0 |
| 首次发布 | 2016 | 2011 | 2022 |
| GitHub Stars | 38k+ | 6k+ | 12k+ |
| 存储模型 | 列式 MergeTree | 列式 DataSource | 列式 Duplicate/Aggregate |
| 实时摄入 | 草稿写入/物化视图 | 原生 Kafka/数据源 | Routine Load/物化视图 |
| 向量执行 | 原生 | 有限 | 原生 |
| 物化视图 | 原生支持 | 有限 | 原生支持 |
| 数据湖 | S3/OSS/HDFS | 外部表/Hudi/Iceberg | Hive/Iceberg/Hudi |
| 分布式 | 原生分片 | Broker/Druid | 原生 CBO |
| SQL 方言 | ClickHouse SQL | Druid SQL | MySQL/PostgreSQL |
| 外部表 | 原生 | 有限 | 原生 |
| 一句话定位 | 列式 OLAP 极致性能 | 实时分析+低延迟查询 | 现代向量化分析引擎 |

## 选型指南

- **极致 OLAP 性能**：推荐 ClickHouse（列式+向量化）
- **实时分析/BI**：推荐 Apache Druid（实时摄入+亚秒查询）
- **MySQL 迁移/兼容性**：推荐 StarRocks（MySQL 协议）
- **数据湖分析**：推荐 StarRocks/ClickHouse（外部表支持好）

## 学习路径

1. **先学 ClickHouse** — 最流行，概念清晰，性能卓越
2. **再学 StarRocks** — 理解现代化向量化执行
3. **然后学 Apache Druid** — 理解实时分析架构

## 关联项目

- `db/storage/` — 项目存储引擎，可扩展 OLAP 能力
- `docs/storage-architecture.md` — 分析能力在存储架构中的规划
