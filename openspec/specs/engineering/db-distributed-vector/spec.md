# DB 分布式与向量数据库知识点

## Purpose

扩展 quiz-tech.js 的 DB_TECH_DATA，新增分布式数据库（Raft/Paxos/TiDB 等约 15 个）和向量数据库（HNSW/IVF/PQ/DiskANN 等约 15 个）知识点，并为每个知识点补充面试向题目，消除 DB 题库 30 个知识点的覆盖缺口。

## Requirements

### Requirement: DB 技术栈新增分布式知识点

quiz-tech.js 的 DB_TECH_DATA SHALL 新增约 15 个分布式数据库知识点（分布式事务、Raft、Paxos、TiDB、ShardingSphere、MVCC、LSM-Tree、WAL 等），每个知识点 SHALL 有对应的 5-10 道题目。

#### Scenario: 元数据与题库 ID 一致

- **WHEN** 在 quiz-tech.js 中新增知识点元数据 `{ id: "db-raft", ... }`
- **THEN** quiz-questions-db.js 中 SHALL 存在 `"db-raft": [...]` 对应的题目数组

#### Scenario: 知识点分类正确

- **WHEN** 新增分布式知识点
- **THEN** quadrant SHALL 为 "distributed" 或已有的合适分类，ring SHALL 为 "intermediate" 或 "advanced"

### Requirement: DB 技术栈新增向量数据库知识点

quiz-tech.js 的 DB_TECH_DATA SHALL 新增约 15 个向量数据库知识点，覆盖 HNSW、IVF、PQ 量化、DiskANN、混合检索、索引调优、Milvus 架构等，每个知识点 SHALL 有对应题目。

#### Scenario: 向量索引算法题目准确

- **WHEN** 用户答题时选择向量数据库知识点
- **THEN** 题目 SHALL 覆盖算法原理、参数调优（nlist/nprobe/M/efConstruction）、精度-速度权衡等实际应用场景

#### Scenario: 索引调优题包含量化内容

- **WHEN** 知识点为 db-pq（Product Quantization）
- **THEN** 题目 SHALL 包含子空间数量、码本大小、召回率与压缩比权衡等调优参数

### Requirement: 现有 DB 知识点缺失部分补全

当前 quiz-questions-db.js 已有 29 个知识点的题库，但 quiz-tech.js 定义了 59 个知识点，缺口 30 个。SHALL 优先补全高频面试题（Redis 底层、LSM/WAL、Buffer Pool、Bloom Filter 等）。

#### Scenario: 题库覆盖率检验

- **WHEN** 枚举 DB_TECH_DATA 所有知识点 ID
- **THEN** quiz-questions-db.js 中每个 ID SHALL 对应至少一道题目

### Requirement: 分布式与向量 DB 题目偏向实战与面试

新增题目 SHALL 以面试高频场景为 scenario，内容包含参数选择依据、故障排查、性能权衡。

#### Scenario: 面试向题目格式

- **WHEN** 新增 db-raft 知识点题目
- **THEN** 至少 2 道题 SHALL 涉及 Leader 选举过程、日志复制、脑裂处理等面试常问点
