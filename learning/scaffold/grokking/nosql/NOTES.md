# NoSQL · 学习笔记

## 核心概念

1. **KV 存储**：最简单的 NoSQL 模型，O(1) 读写，适合缓存和会话管理
2. **文档数据库**：JSON/BSON 文档，灵活 Schema，嵌套结构天然支持复杂对象
3. **列族存储**：按列族组织宽表，适用时序数据和稀疏矩阵，列可动态添加
4. **图数据库**：节点 + 边 + 属性，适合深度关联查询（社交关系、推荐路径）

## 对比

| 类型 | 典型产品 | 数据模型 | 查询方式 | 一致性 |
|------|---------|---------|---------|--------|
| KV | Redis | Key→Value | GET/SET | 强/最终 |
| 文档 | MongoDB | JSON 文档 | 类 JSON 查询 | 最终 |
| 列族 | HBase/Cassandra | 行键→列族→列 | CQL/Scan | 最终 |
| 图 | Neo4j | 节点→边→属性 | Cypher/Gremlin | ACID |

## 工程对照

本项目的 `engineering/src/db/mm_storage.h` 定义了多模态存储引擎接口，除
SQL 关系模型外，还支持 KV（`kv_engine`）、向量（`vector_engine`）、
时序（`ts_engine`）、文档（`doc_engine`）、空间（`spatial_engine`）、
图（`graph_engine`）和层次数据（`yang_engine`）七种模型。每种引擎通过
统一的 `EngineOps` 接口对外暴露操作。该设计与本文 NoSQL 四大分类比较的
不同在于，实际多模型引擎需要在共享的 Buffer Pool 和 WAL 之上抽象出
不同数据模型的存取语义。
