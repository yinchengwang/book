# 图数据库横向对比

## 分类概述

图数据库以图结构存储数据，使用节点和边表示实体和关系，擅长处理高度关联的数据。相较于关系型数据库，图数据库在社交网络分析、推荐系统、知识图谱、欺诈检测等场景具有天然优势，可高效执行多跳关系查询。

## 库一览

- **Neo4j** - 最成熟的商业图数据库，Cypher 查询语言，企业级支持
- **ArangoDB** - 多模态图数据库，同时支持文档和 KV，原生多模型
- **Dgraph** - 分布式图数据库，GraphQL 原生，水平扩展能力强
- **JanusGraph** - 分布式图数据库，基于 Gremlin，底层可接 Cassandra/ES/HBase
- **NebulaGraph** - 国产高性能图数据库，OpenCypher 兼容，微博/快手生产验证

## 功能对比表

| 维度 | Neo4j | ArangoDB | Dgraph | JanusGraph | NebulaGraph |
|------|-------|----------|--------|------------|-------------|
| 编程语言 | Java | C++ | Go | Java | C++ |
| 开源协议 | GPLv3/Aura | Apache 2.0 | Apache 2.0 | Apache 2.0 | Apache 2.0 |
| 首次发布 | 2007 | 2012 | 2016 | 2017 | 2019 |
| GitHub Stars | 12k+ | 12k+ | 24k+ | 7k+ | 8k+ |
| 存储模型 | 原生属性图 | 多模型(图/文档/KV) | 原生属性图 | 外挂存储 | 原生属性图 |
| 查询语言 | Cypher/GQL | AQL/GraphQL/Cypher | GraphQL±/DQL | Gremlin | OpenCypher/nGQL |
| 分片策略 | 手动分区 | 智能分片 | 唯一分片键 | Hash 分区 | Hash 分片 |
| 事务支持 | ACID | ACID | 只读事务 | ACID | ACID |
| 一致性 | 单机强一致 | 混合一致性 | 线性一致性 | 可配置 | 可配置 |
| 水平扩展 | 企业版支持 | 原生支持 | 原生支持 | 支持 | 原生支持 |
| 一句话定位 | 企业级图数据库标杆 | 多模态融合图库 | 分布式 GraphQL 图库 | 后端可插拔图库 | 国产高性能图引擎 |

## 选型指南

- **企业级应用**：推荐 Neo4j（生态成熟、企业支持）
- **多模态需求**：推荐 ArangoDB（图+文档+KV 一体）
- **超大规模图**：推荐 Dgraph、NebulaGraph（原生分布式）
- **需要灵活后端**：推荐 JanusGraph（存储层可插拔）
- **国内项目/合规**：推荐 NebulaGraph（国产、社区活跃）

## 学习路径

1. **先学 Neo4j** — 最成熟，概念清晰，Cypher 简洁优雅
2. **再学 ArangoDB** — 理解多模型融合和 AQL 查询
3. **然后学 JanusGraph** — 理解分布式图处理和 Gremlin
4. **最后学 Dgraph/NebulaGraph** — 分布式架构和底层实现

## 关联项目

- `graph_engine` — 项目多模态存储引擎中的图存储模块
- `docs/storage-architecture.md` — 图存储在多模态架构中的定位
