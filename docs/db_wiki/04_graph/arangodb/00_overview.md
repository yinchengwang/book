# ArangoDB 项目概览

## 学习目标

- 了解 ArangoDB 作为多模型数据库的定位
- 掌握 ArangoDB 同时支持图/文档/ KV 的设计

## 项目定位

> ArangoDB 是一个原生多模型数据库，同时支持图、文档和键值数据模型，用同一个查询语言 AQL。

**基本信息**：
- 开发方：ArangoDB Inc.
- 首次发布：2011 年
- 开源协议：Apache 2.0
- GitHub Stars：约 13k

## 核心设计

```mermaid
graph TD
    A[ArangoDB] --> B[多模型<br/>图+文档+KV]
    A --> C[AQL 查询语言<br/>统一接口]
    A --> D[ArangoSearch<br/>全文搜索]
    A --> E[Foxx 微服务<br/>JS 扩展]
    A --> F[集群模式<br/>分片复制]

    B --> B1[同一数据库]
    B --> B2[灵活切换]
    C --> C1[类似 SQL 语法]
    C --> C2[图案 + 文档
```

## 多模型架构

```aql
-- 文档操作
INSERT {name: "Alice", age: 30} INTO users
UPDATE "user_key" WITH {age: 31} IN users
FOR doc IN users FILTER doc.age > 25 RETURN doc

-- 图操作（使用同一集合作为顶点）
FOR v, e, p IN 1..3 OUTBOUND "users/Alice" GRAPH "friends"
  RETURN p

-- 混合查询
FOR u IN users
  FILTER u.age > 25
  FOR friend, edge IN OUTBOUND u._id GRAPH "friends"
    RETURN {user: u, friend: friend.name}
```

## 要点总结

- 一次学习，三种数据模型
- AQL 统一查询语言
- 支持 Foxx 微服务框架
- 适合需要多种模型的场景

## 思考题

1. ArangoDB 的多模型与分别使用独立的数据库相比有何优势？
2. ArangoSearch 的全文索引如何实现？
3. Foxx 微服务相比独立的 API 服务有什么特点？