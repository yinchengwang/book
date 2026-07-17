# 数据库内容重组

## Purpose

`data/learn-deep/db/` 目录仅保留通用数据库架构知识，产品专属内容（MySQL、Redis、PostgreSQL 等）迁移到 `data/learn-deep/illustrate/` 目录下的独立产品子目录。

## ADDED Requirements

### Requirement: 通用数据库架构知识保留

`data/learn-deep/db/` 下 SHALL 仅保留以下通用数据库架构知识点（11 个）：SQL DDL、SQL DML、列存储、ACID、SQL DCL、乐观并发控制、混合查询、分布式共识（Raft/Paxos）、数据分片、高可用与故障转移、数据库性能调优。

#### Scenario: 通用知识点仍在 db 目录

- **WHEN** 用户访问 `learn.html#db/db-acid`
- **THEN** 页面 SHALL 从 `data/learn-deep/db/language/db-acid.md` 加载内容

#### Scenario: 产品专属知识点不在 db 目录

- **WHEN** 用户访问 MySQL 产品专属内容
- **THEN** 文件 SHALL 存放在 `data/learn-deep/illustrate/mysql/` 目录下，而非 `data/learn-deep/db/`

### Requirement: 产品专属内容迁移

以下产品专属知识点 SHALL 迁移到 `data/learn-deep/illustrate/{product}/` 目录：

- **MySQL**（23 个）：存储引擎架构、数据页与块结构、行存储记录格式、数据文件组织、Buffer Pool、Change Buffer、WAL、Checkpoint、B+树索引、B+树实现、哈希索引、SQL 解析器、查询优化器、查询执行引擎、MVCC、ReadView、Undo Log、InnoDB 锁机制、死锁检测、Redo Log 详解、ARIES 恢复算法
- **Redis**（6 个）：事件驱动模型、RESP 协议、对象系统、持久化、主从复制、集群
- **PostgreSQL**（3 个）：空间索引、快照隔离与 Write Skew、两阶段提交
- **Elasticsearch**（1 个）：全文索引与倒排索引
- **RocksDB**（3 个）：Compaction、LSM-Tree、SSTable
- **SQLite**（2 个）：架构与嵌入存储、B-Tree 与页面管理

#### Scenario: MySQL 文件迁移

- **WHEN** 迁移 MySQL 知识点
- **THEN** `db-storage-overview.md` 从 `data/learn-deep/db/language/` 移动至 `data/learn-deep/illustrate/mysql/`

#### Scenario: Redis 文件迁移

- **WHEN** 迁移 Redis 知识点
- **THEN** `db-redis-event.md` 从 `data/learn-deep/db/engineering/` 移动至 `data/learn-deep/illustrate/redis/`

### Requirement: 数据库知识点元数据映射

`data/app/items-registry.js` 中数据库相关产品专属知识点的 `stack` 字段 SHALL 保持为 `db`，`product` 字段 SHALL 标识对应产品名（mysql/redis/postgres/elasticsearch/rocksdb/sqlite）。向量数据库相关知识点（faiss/diskann/milvus/pinecone）的 `stack` 字段 SHALL 为 `vdb`。

#### Scenario: MySQL 知识点 product 字段

- **WHEN** 查询 `db-storage-overview` 知识点
- **THEN** 其 `stack` 为 `"db"`，`product` 为 `"mysql"`

#### Scenario: 向量数据库知识点 product 字段

- **WHEN** 查询 `db-hnsw` 知识点
- **THEN** 其 `stack` 为 `"vdb"`，`product` 为 `"faiss"`
