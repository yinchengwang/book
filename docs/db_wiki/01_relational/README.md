# 关系型数据库横向对比

## 分类概述

关系型数据库是数据库领域最成熟、应用最广泛的类型，采用表格模型存储数据，通过 SQL 进行关系代数和关系演算操作。本分类覆盖从嵌入式到分布式、从开源到商业的完整生态，涵盖 OLTP 事务处理与分析查询两大场景。

## 库一览

- **PostgreSQL** - 功能最强大的开源关系型数据库，SQL 标准遵循度高，扩展性强
- **MySQL** - 全球最流行的开源数据库，Web 应用标配，Oracle 维护
- **CockroachDB** - 云原生分布式 SQL，Google Spanner 开源实现，高可用
- **TiDB** - 国产分布式 NewSQL，MySQL 兼容，HTAP 混合负载
- **DuckDB** - 嵌入式分析型 SQL，OLAP 场景杀手级性能，零依赖
- **SQLite3** - 嵌入式关系型数据库，世界上最部署量最高的数据库引擎
- **StoneDB** - 国产实时分析型 MySQL，列式存储，TPC-H 性能优异
- **OceanBase** - 蚂蚁集团自研分布式数据库，支付宝核心系统验证
- **openGauss** - 华为开源企业级关系型数据库，高并发事务处理

## 功能对比表

| 维度 | PostgreSQL | MySQL | CockroachDB | TiDB | DuckDB | SQLite3 | StoneDB | OceanBase | openGauss |
|------|-----------|-------|-------------|------|--------|---------|---------|-----------|-----------|
| 编程语言 | C | C/C++ | Go | Go | C++ | C | C++ | C/C++ | C/C++ |
| 开源协议 | PostgreSQL | GPLv2 | BSL 1.1 | Apache 2.0 | MIT | Public Domain | GPLv2 | APL 2.0 | MPL 2.0 |
| 首次发布 | 1996 | 1995 | 2017 | 2015 | 2019 | 2000 | 2021 | 2010 | 2019 |
| GitHub Stars | 12k+ | 9k+ | 28k+ | 38k+ | 18k+ | N/A | 2k+ | 6k+ | 5k+ |
| SQL 兼容性 | SQL:2016 | SQL:2003 | SQL:2016 | MySQL 5.7 | SQL:2016 | SQL:92 | MySQL 5.7 | MySQL/Oracle | SQL:2003 |
| 事务隔离级别 | SSI | RC/RR | SERIALIZABLE | SI | RR | DEFERRED | RC/RR | SI/RC/RR | SI |
| 存储引擎 | 堆表/Append-Only | InnoDB/MyISAM | RocksDB | TiFlash/RocksDB | 列式压缩 | B-Tree/WSL | 列式+行式 | 分布式 LSM | 集中式 |
| 复制方式 | 流复制/WAL | 半同步/MGR | Raft | Raft | 无(嵌入式) | 文件复制 | 主从复制 | Paxos/Raft | Raft |
| 分区支持 | 原生 | 原生 | 原生 | 原生 | 分区表 | 分区表 | 分区表 | 分区表 | 分区表 |
| 云原生 | K8s Operator | 托管服务 | 原生 | 原生 | 嵌入式 | 嵌入式 | MySQL 扩展 | 原生 | K8s Operator |
| 分布式 | 扩展件/Citus | Vitess/ProxySQL | 原生 | 原生 | 嵌入式 | 嵌入式 | 扩展 | 原生 | 扩展 |
| 一句话定位 | 功能最全的通用 RDBMS | Web 应用标配 | 全球化低延迟 SQL | HTAP 混合负载 | 嵌入式 OLAP 引擎 | 无服务器数据库 | MySQL 实时分析 | 金融级分布式 | 企业级 OLTP |

## 选型指南

- **传统 Web 应用**：推荐 MySQL（成熟生态）、PostgreSQL（功能更强）
- **金融级事务**：推荐 OceanBase、CockroachDB、openGauss（高可用强一致）
- **大规模分布式**：推荐 TiDB、CockroachDB（原生分布式+水平扩展）
- **数据分析/OLAP**：推荐 DuckDB（嵌入式分析）、StoneDB（MySQL 扩展）
- **边缘/嵌入式**：推荐 SQLite3（零依赖、高可靠）
- **云原生 SaaS**：推荐 CockroachDB、TiDB（K8s 友好、弹性伸缩）

## 学习路径

1. **先学 SQLite3** — 最容易上手，无依赖，理解 RDBMS 核心概念
2. **再学 MySQL/PostgreSQL** — 生产环境主流选择，掌握 SQL 和调优
3. **然后学 TiDB/CockroachDB** — 理解分布式 SQL 和 HTAP 架构
4. **最后学 DuckDB** — 深入 OLAP 引擎和向量化执行

## 关联项目

- `db/storage/` — 项目自研存储引擎，参考 PG 架构
- `db/btreeam/` — BTree 索引实现，对标 PostgreSQL 索引机制
- `docs/storage-architecture.md` — PostgreSQL 风格存储架构文档
