# quiz-product-file-org

## Purpose

将传统 DB 和向量 DB 的题库按产品组织为独立文件，每个产品文件内部按象限分组、按难度排序，通过 `Object.assign` 合并回 `QUESTION_BANK.db` 和 `QUESTION_BANK.vdb`。

## Requirements

### Requirement: 传统 DB 题目按产品文件组织

从 `quiz-questions-db.js` 中提取各产品专属题目，放入独立文件。

#### Scenario: 产品文件清单
- **WHEN** 文件组织完成
- **THEN** `data/` 目录下存在以下 DB 产品文件：
  - `quiz-questions-db-mysql.js`（MySQL/InnoDB 专属概念）
  - `quiz-questions-db-postgres.js`（PostgreSQL 专属概念）
  - `quiz-questions-db-redis.js`（Redis 专属概念）
  - `quiz-questions-db-rocksdb.js`（LSM-Tree/RocksDB 系概念）
  - `quiz-questions-db-elasticsearch.js`（Elasticsearch 倒排索引概念）
  - `quiz-questions-db-general.js`（跨产品通用概念）

#### Scenario: 产品归属判断
- **WHEN** 题目分配到产品文件时
- **THEN** 遵循以下归属规则：
  - 某产品独占概念（如 Redis RESP 协议）→ 归该产品文件
  - 该产品是代表性实现（如 InnoDB 与 B+ 树）→ 归该产品文件
  - 跨产品通用概念（如 ACID/共识算法）→ 归 general.js
- **AND** 每个知识点的题目不重复分配（一份题目只在一个文件中）

### Requirement: 向量 DB 题目按产品文件组织

从 `quiz-questions-db.js` 中提取向量 DB 题目，放入独立产品文件。

#### Scenario: 产品文件清单
- **WHEN** 文件组织完成
- **THEN** `data/` 目录下存在以下 vDB 产品文件：
  - `quiz-questions-vdb-faiss.js`（Faiss 系索引算法）
  - `quiz-questions-vdb-milvus.js`（Milvus 系统专属）
  - `quiz-questions-vdb-chroma.js`（Chroma 专属）
  - `quiz-questions-vdb-diskann.js`（DiskANN 磁盘索引）
  - `quiz-questions-vdb-pinecone.js`（Pinecone，先建框架）
  - `quiz-questions-vdb-general.js`（跨产品通用概念）

### Requirement: 产品文件结构

每个产品文件内部按象限分组组织，题目按象限和难度有序排列。

#### Scenario: 文件内部结构
- **WHEN** 查看产品文件时
- **THEN** 文件内题目先按象限分组
- **AND** 组内按难度排序（basic → intermediate → advanced）
- **AND** 每个题目携带 `quadrant` 和 `ring` 字段

#### Scenario: 合并机制
- **WHEN** 页面加载所有产品文件的 `<script>` 标签后
- **THEN** `QUESTION_BANK.db` 包含所有传统 DB 产品文件的题目集合
- **AND** `QUESTION_BANK.vdb` 包含所有向量 DB 产品文件的题目集合
- **AND** `Object.assign` 合并后无重复 ID

#### Scenario: 产品文件行数
- **WHEN** 文件创建完成
- **THEN** 每个产品文件不超过 500 行
- **AND** `general.js` 不超过 800 行
