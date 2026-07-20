# 向量数据库横向对比

## 分类概述

向量数据库专为高维向量检索设计，支持近似最近邻（ANN）搜索，广泛应用于 AI 语义搜索、推荐系统、图像/音频检索等场景。本分类涵盖从嵌入式库到云服务、从单机型到分布式架构的完整生态，支持混合检索（向量+标量过滤）。

## 库一览

- **Faiss** - Facebook AI 相似性搜索库，多种 ANN 算法，GPU 加速
- **Milvus** - 云原生向量数据库，分布式架构，混合搜索
- **Qdrant** - 高性能向量搜索引擎，Rust 实现，滤波能力强
- **Weaviate** - 语义向量数据库，原生多模态支持，GraphQL API
- **Chroma** - AI 原生向量数据库，LangChain 生态深度集成
- **LanceDB** - 嵌入式向量数据库，Rust 实现，持久化优先
- **pgvector** - PostgreSQL 向量扩展，SQL 兼容，学习成本低
- **Vald** - 云原生向量搜索引擎，Kubernetes 原生，高可用

## 功能对比表

| 维度 | Faiss | Milvus | Qdrant | Weaviate | Chroma | LanceDB | pgvector | Vald |
|------|-------|--------|--------|----------|--------|---------|---------|------|
| 编程语言 | C++/Python | Go | Rust | Go | Python | Rust | C | Go |
| 开源协议 | Apache 2.0 | Apache 2.0 | Apache 2.0 | BSD | Apache 2.0 | Apache 2.0 | PostgreSQL | Apache 2.0 |
| 首次发布 | 2017 | 2019 | 2021 | 2018 | 2023 | 2023 | 2021 | 2020 |
| GitHub Stars | 28k+ | 29k+ | 18k+ | 11k+ | 15k+ | 14k+ | 12k+ | 2k+ |
| 索引算法 | IVF/HNSW/PQ/LSH | IVF/HNSW/DiskANN | HNSW/BruteForce | HNSW | HNSW | HNSW/DiskANN | HNSW/IVFFlat | EGPUSSH |
| 距离度量 | L2/IP/Cosine/Hamming | L2/IP/Cosine/Hamming | Dot/Cosine/L2 | Cosine/Dot | L2/IP/Cosine | L2/IP/Cosine | L2/IP/Cosine | L2/IP/Cosine |
| 部署方式 | 嵌入式库 | K8s/云/单机 | K8s/云/单机 | K8s/云/单机 | 单机/嵌入式 | 嵌入式 | PostgreSQL 扩展 | K8s |
| 分布式 | 需手写 | 原生支持 | 分片 | 分片 | 单机 | 单机 | PostgreSQL 扩展 | 原生支持 |
| 混合搜索 | 需手写 | 原生支持 | 原生支持 | 原生支持 | 有限 | 原生支持 | SQL WHERE | 原生支持 |
| 规模 | 取决于硬件 | PB 级 | TB 级 | TB 级 | GB 级 | TB 级 | 取决于 PG | PB 级 |
| 嵌入能力 | 是 | 否 | 否 | 是 | 是 | 是 | 否 | 否 |
| 一句话定位 | 基础 ANN 算法库 | 云原生向量平台 | 高性能向量引擎 | 多模态语义搜索 | AI 原生嵌入库 | 持久化向量数据库 | PG 向量扩展 | K8s 向量引擎 |

## 选型指南

- **AI 应用快速原型**：推荐 Chroma（上手最快，LangChain 集成）
- **生产级向量检索**：推荐 Milvus、Qdrant（云原生、高可用）
- **已有 PostgreSQL 栈**：推荐 pgvector（无需新增组件）
- **追求极致性能**：推荐 Faiss（GPU 加速）、LanceDB（Rust 高效）
- **多模态数据**：推荐 Weaviate（原生支持图像/文本/音频）
- **Kubernetes 部署**：推荐 Vald（云原生高可用）

## 学习路径

1. **先学 pgvector** — 最易上手，SQL 语义理解向量
2. **再学 Faiss** — 理解底层 ANN 算法原理（IVF/HNSW/PQ）
3. **然后学 Qdrant/Milvus** — 理解分布式向量检索架构
4. **最后学 Weaviate/Chroma** — 理解多模态和 AI 集成

## 关联项目

- `index/vector/` — 项目自研向量索引实现
- `index/hnsw/` — HNSW 算法实现
- `reference/open-source/` — Faiss/Weaviate/Milvus 源码参考
