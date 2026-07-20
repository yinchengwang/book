# Dgraph 项目概览

## 学习目标

- 了解 Dgraph 作为分布式图数据库的定位
- 掌握 Dgraph 的 Badger LSM-Tree 存储和 GraphQL 查询

## 项目定位

> Dgraph 是一个分布式图数据库，使用 Go 实现，支持 GraphQL 风格的查询语言，支持 trillion 级别数据。

**基本信息**：
- 开发方：Dgraph Labs
- 首次发布：2016 年
- 开源协议：Apache 2.0
- GitHub Stars：约 24k

## 核心设计

```mermaid
graph TD
    A[Dgraph] --> B[分布式架构<br/>Raft 组]
    A --> C[Badger 存储<br/>LSM-Tree]
    A --> D[GraphQL+- 查询<br/>DQL]
    A --> E[无单点<br/>水平扩展]
    A --> F[倒排索引<br/>快速过滤]

    B --> B1[每个分片 Raft 组]
    B --> B2[Zero 协调]
    C --> C1[键值存储]
    C --> C2[写入优化]
    D --> D1[类似 GraphQL]
    D --> D2[图案匹配]
```

## 架构特点

```mermaid
graph TB
    subgraph "Alpha 节点"
        A1[Alpha 1<br/>Raft Group 1]
        A2[Alpha 2<br/>Raft Group 2]
        A3[Alpha 3<br/>Raft Group 3]
    end

    subgraph "Zero 节点"
        Z1[Zero 1]
        Z2[Zero 2]
        Z3[Zero 3]
    end

    subgraph "数据分布"
        P1[Predicate 1-100]
        P2[Predicate 101-200]
        P3[Predicate 201-300]
    end

    Z1 --> A1
    Z1 --> A2
    Z1 --> A3
    A1 --> P1
    A2 --> P2
    A3 --> P3
```

## DQL 示例

```graphql
type Person {
  name: string @index(term)
  age: int
  knows: [Person]
}

# 查询
{
  query(func: eq(name, "Alice")) {
    name
    age
    knowsperson: knows {
      name
    }
  }
}

# 聚合查询
{
  query(func: has(Person.name)) {
    count(uid)
  }
}
```

## 要点总结

- 按 Predicate 分片，不同于按顶点分片
- Badger LSM-Tree 存储引擎
- GraphQL 风格查询语言 DQL
- Zero 协调器管理集群

## 思考题

1. Dgraph 按 Predicate 分片与按顶点分片相比有何优劣？
2. Badger LSM-Tree 存储对图数据写入性能有何影响？
3. Zero 协调器与 Raft Leader 的关系是什么？