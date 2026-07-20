# 搜索引擎横向对比

## 分类概述

全文搜索引擎提供强大的文本搜索能力，支持倒排索引、相关性排序、分词、模糊匹配等功能。广泛应用于站内搜索、日志分析、知识库检索等场景，相较于数据库 LIKE 查询具有显著的性能和功能优势。

## 库一览

- **Elasticsearch** - 全球最流行的分布式搜索引擎，ELK 生态核心，大规模搜索
- **Meilisearch** - 轻量级极速搜索引擎，开箱即用，Rust 实现
- **ParadeDB** - Postgres 原生搜索扩展，BM25 排序，pg 生态集成
- **Tantivy** - Rust 实现的高性能全文搜索引擎，Meilisearch 底层
- **ZincSearch** - 轻量级 Elasticsearch 替代，Go 实现，低资源

## 功能对比表

| 维度 | Elasticsearch | Meilisearch | ParadeDB | Tantivy | ZincSearch |
|------|---------------|-------------|----------|---------|------------|
| 编程语言 | Java | Rust | Rust | Rust | Go |
| 开源协议 | Elastic/SSPL | MIT | Apache 2.0 | MIT | Apache 2.0 |
| 首次发布 | 2010 | 2018 | 2022 | 2016 | 2022 |
| GitHub Stars | 67k+ | 47k+ | 7k+ | 11k+ | 12k+ |
| 底层引擎 | Lucene | Tantivy | Tantivy | Tantivy | Tantivy |
| 配置复杂度 | 高 | 低 | 中 | 中 | 低 |
| 拼写容错 | 原生支持 | 原生支持 | 有限 | 需手写 | 有限 |
| 嵌入能力 | 有限 | 原生支持 | 原生支持 | 是 | 有限 |
| 分布式 | 原生支持 | 有限 | PostgreSQL | 嵌入式 | 有限 |
| 协议 | REST/JSON | REST/JSON | SQL/PG | 库/REST | REST/JSON |
| 资源消耗 | 高 | 低 | 低 | 低 | 低 |
| 一句话定位 | 企业级分布式搜索平台 | 极速轻量搜索 | PG 原生搜索扩展 | Rust 高性能索引库 | 轻量 ES 替代 |

## 选型指南

- **大规模企业搜索**：推荐 Elasticsearch（成熟生态）
- **快速原型/小规模**：推荐 Meilisearch（开箱即用）
- **已有 PostgreSQL 栈**：推荐 ParadeDB（无需新组件）
- **极致性能/嵌入式**：推荐 Tantivy（Rust 高性能）
- **低资源环境**：推荐 ZincSearch（轻量替代）

## 学习路径

1. **先学 Meilisearch** — 最易上手，概念清晰
2. **再学 Elasticsearch** — 理解分布式搜索架构
3. **然后学 Tantivy** — 理解倒排索引底层实现
4. **最后学 ParadeDB/ZincSearch** — 特定生态集成

## 关联项目

- `index/vector/` — 项目向量索引，与全文搜索结合实现混合检索
- `self_made/` — 手写数据结构练习
