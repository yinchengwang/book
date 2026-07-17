# NewSQL · 学习笔记

## 核心概念

1. **NewSQL**：保留 SQL 和 ACID 的关系型数据库，同时具备 NoSQL 的水平扩展能力
2. **TiDB**：计算存储分离（TiDB 计算层 + TiKV 存储层 + PD 元数据管理）
3. **CockroachDB**：每个数据 Range 是 Raft 复制组，支持跨地域部署
4. **HTAP**：一份数据同时支持 TP（行存）和 AP（列存）负载

## NewSQL 关键技术

| 技术 | 说明 |
|------|------|
| 自动分片 | 数据自动分裂/合并，无需 DBA 干预 |
| 分布式事务 | 2PC/ Percolator 模型支持跨节点事务 |
| 多副本+Raft | 每个分片 3~5 副本，Leader 故障自动选举 |
| 计算下推 | Coprocessor 聚合计算下推到存储节点 |

## 工程对照

本项目的 `engineering/src/db/` 中实现了分布式数据库的一些基础组件：
`shard.c` 的分片路由、`raft.c` 的 Raft 共识、`dist_txn.c` 的分布式事务
协调器。与本文 TiDB/CockroachDB 架构模拟不同的是，工程实现贴近真实的
分布式系统问题——Leader 选举超时、网络分区下的可用性降级、分布式
死锁检测、TTL 过期清理。NewSQL 的真正价值在于分布式 SQL 的透明化
——对用户来说像单机 MySQL，底层却运行在数十台机器上。
