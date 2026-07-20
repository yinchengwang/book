# 时序数据库横向对比

## 分类概述

时序数据库（TSDB）专门用于存储和时间相关的数据序列，擅长处理传感器数据、监控指标、金融行情、IoT 数据等高写入吞吐场景。核心能力包括高效压缩、时间窗口查询、连续聚合、下采样和数据保留策略。

## 库一览

- **InfluxDB** - 最流行的时序数据库，InfluxQL/Flux 双查询语言，云服务
- **TimescaleDB** - PostgreSQL 扩展，时序数据超表，云原生超表分区
- **QuestDB** - 高性能时序数据库，类 SQL 查询，列式存储，SIMD 加速
- **VictoriaMetrics** - 高性能低资源时序数据库，Prometheus 兼容，单实例高吞吐
- **GreptimeDB** - 国产开源时序数据库，融合时序+分析，Rust 实现

## 功能对比表

| 维度 | InfluxDB | TimescaleDB | QuestDB | VictoriaMetrics | GreptimeDB |
|------|----------|-------------|---------|-----------------|------------|
| 编程语言 | Go | C | Java/C++ | Go | Rust |
| 开源协议 | MIT | Apache 2.0 | Apache 2.0 | Apache 2.0 | Apache 2.0 |
| 首次发布 | 2013 | 2017 | 2016 | 2019 | 2022 |
| GitHub Stars | 27k+ | 20k+ | 10k+ | 16k+ | 6k+ |
| 压缩算法 | TSM | PostgreSQL | 自研列式 | 自研 | Parquet/ORC |
| 查询语言 | InfluxQL/Flux | SQL | 类 SQL | PromQL/SQL | SQL |
| 数据保留 | 连续查询/RP | 超表保留策略 | 保留策略 | 保留策略 | 保留策略 |
| 连续聚合 | 原生 CQ | 原生连续聚合 | 下采样 | Recording Rules | 原生 |
| 写入吞吐 | 高 | 高 | 极高 | 极高 | 高 |
| 存储引擎 | TSM/IOx | Hypertable | 列式内存映射 | 自研 | 融合存储 |
| 云原生 | 原生 | 超表/分布式 | 嵌入式 | 原生 | 原生 |
| 一句话定位 | 全功能时序平台 | PG 扩展时序库 | 极速时序写入 | 高性能监控存储 | 融合时序分析 |

## 选型指南

- **监控/可观测性**：推荐 VictoriaMetrics（Prometheus 兼容、低资源）
- **已有 PostgreSQL 栈**：推荐 TimescaleDB（无需新组件）
- **极高写入吞吐**：推荐 QuestDB（SIMD 向量化）
- **InfluxDB 生态**：推荐 InfluxDB（Flux 强大）
- **融合分析需求**：推荐 GreptimeDB（时序+ OLAP）

## 学习路径

1. **先学 InfluxDB** — 最流行，概念完整，上手快
2. **再学 TimescaleDB** — 理解超表分区和时间压缩
3. **然后学 VictoriaMetrics** — 理解高性能存储和 PromQL
4. **最后学 QuestDB/GreptimeDB** — 底层实现和融合架构

## 关联项目

- `ts_engine` — 项目多模态存储引擎中的时序模块
- `docs/storage-architecture.md` — 时序数据在多模态架构中的定位
