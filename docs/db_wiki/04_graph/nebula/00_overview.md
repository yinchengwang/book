# NebulaGraph 项目概览

## 学习目标

- 了解 NebulaGraph 作为国产高性能分布式图数据库的定位
- 掌握 NebulaGraph 的架构设计和 nGQL 查询语言

## 项目定位

> NebulaGraph 是国产开源的高性能分布式图数据库，支持千亿顶点和万亿边的大规模图数据。

**基本信息**：
- 开发方：vesoft（杭州维表科技）
- 首次发布：2019 年
- 开源协议： Apache 2.0
- GitHub Stars：约 12k

## 核心设计

```mermaid
graph TB
    A[NebulaGraph] --> B[分布式架构<br/>Shared-nothing]
    A --> C[nGQL 查询语言<br/>类 SQL 图案]
    A --> D[Storage Engine<br/>Raft + Vid]
    A --> E[Meta Service<br/>元数据管理]
    A --> F[高性能<br/>千亿顶点]

    B --> B1[无中心节点]
    B --> B2[水平扩展]
    C --> C1[支持 GO/FETCH 等]
    D --> D1[Key-Value 底层]
    D --> D2[Vid 分片]
```

## 架构特点

```mermaid
graph TB
    subgraph "客户端层"
        CLI[Nebula Console]
        API[NebulaGraph API]
    end

    subgraph "图查询层"
        GQL[Graph Service]
    end

    subgraph "元数据层"
        META[Meta Service<br/>Raft 集群]
    end

    subgraph "存储层"
        ST1[Storage 1]
        ST2[Storage 2]
        STN[Storage N]
    end

    CLI --> GQL
    API --> GQL
    GQL --> META
    GQL --> ST1
    GQL --> ST2
```

## nGQL 示例

```ngql
-- 创建图空间
CREATE SPACE my_graph (vid_type=FIXED_STRING(30));

-- 创建标签和边
CREATE TAG person(name string, age int);
CREATE EDGE know(likeness int);

-- 插入数据
INSERT VERTEX person(name, age) VALUES "Alice":("Alice", 30);
INSERT VERTEX person(name, age) VALUES "Bob":("Bob", 25);
INSERT EDGE know(likeness) VALUES "Alice"->"Bob":(90);

-- 图查询
GO FROM "Alice" OVER know YIELD dst(edge);
```

## 要点总结

- 国产开源，Apache 2.0 许可证
- 分布式 Shared-nothing 架构
- nGQL 类 SQL，易学易用
- 支持千亿顶点万亿边

## 思考题

1. NebulaGraph 的 Vid（顶点 ID）分片策略与 Neo4j 有何不同？
2. Meta Service 的 Raft 集群在集群中扮演什么角色？
3. Storage Engine 基于 KV 存储的设计有什么优缺点？