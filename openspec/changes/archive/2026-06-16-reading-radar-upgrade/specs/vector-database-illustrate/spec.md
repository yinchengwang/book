# 向量数据库图解

## Purpose

向量数据库（Faiss、DiskANN、Milvus、Pinecone）相关内容作为独立图解系列，从 db 栈迁移至 `data/learn-deep/illustrate/` 目录下，与标量数据库产品并列。

## ADDED Requirements

### Requirement: 向量数据库产品目录

`data/learn-deep/illustrate/` 下 SHALL 为以下向量数据库产品建立独立子目录：

- **Faiss**（7 个）：向量索引基础、IVF 家族索引、HNSW 索引原理、图索引详解、乘积量化、量化技术全景、标量量化与 SIMD 距离计算
- **DiskANN**（1 个）：磁盘向量索引
- **Milvus**（4 个）：系统架构、段管理、索引构建与调度、混合查询
- **Pinecone**（2 个）：Serverless 架构、存储与索引

#### Scenario: Faiss 文件存放

- **WHEN** 查看 Faiss 向量索引内容
- **THEN** 文件 SHALL 存放在 `data/learn-deep/illustrate/faiss/` 目录下

#### Scenario: Milvus 文件存放

- **WHEN** 查看 Milvus 系统架构内容
- **THEN** 文件 SHALL 存放在 `data/learn-deep/illustrate/milvus/` 目录下

### Requirement: 向量数据库知识点元数据

`data/app/items-registry.js` 中所有向量数据库相关知识点（15 个，产品为 faiss/diskann/milvus/pinecone）的 `stack` 字段 SHALL 为 `vdb`，`product` 字段 SHALL 标识对应产品名称。这些知识点的内容文件统一存放在 `data/learn-deep/illustrate/{product}/` 目录下。

#### Scenario: 向量数据库知识点查询

- **WHEN** 查询 `db-hnsw` 知识点
- **THEN** 其 `stack` 为 `"vdb"`，`product` 为 `"faiss"`

#### Scenario: 向量数据库文件路径

- **WHEN** 用户访问 `learn.html#vdb/db-hnsw`
- **THEN** 页面 SHALL 从 `data/learn-deep/illustrate/faiss/db-hnsw.md` 加载内容

### Requirement: 混合查询定位

通用混合查询（标量+向量联合搜索）知识点 `db-hybrid-search.md`  SHALL 存放在 `data/learn-deep/db/engineering/` 目录下作为通用数据库架构知识，同时在向量数据库图解系列（Faiss/Milvus）中可引用该通用概念。

#### Scenario: 混合查询知识点位置

- **WHEN** 查看混合查询通用知识
- **THEN** 文件 SHALL 存放在 `data/learn-deep/db/engineering/db-hybrid-search.md`
