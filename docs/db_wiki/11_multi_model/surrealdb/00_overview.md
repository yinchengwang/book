# SurrealDB 项目概览

## 学习目标

- 了解 SurrealDB 的定位和特点
- 掌握 SurrealDB 的多模态数据模型与 SurrealQL 查询语言

## 项目定位

> 多模态数据库，同时支持文档、图、KV 数据模型，使用 SurrealQL 查询语言，可嵌入式或客户端部署

**基本信息**：

- 开发方：SurrealDB Team
- 开源协议：Apache 2.0
- GitHub Stars：~28k

## 核心设计

```mermaid
graph TB
    subgraph "数据模型"
        DOC["文档模型<br/>JSON/嵌套"]
        GRAPH["图模型<br/>顶点/边"]
        KV["KV 模型<br/>键值存储"]
    end

    subgraph "SurrealQL"
        Q1["SELECT"]
        Q2["CREATE"]
        Q3["UPDATE"]
        Q4["RELATE"]
        Q5["GRAPH"]
    end

    subgraph "查询处理"
        PARSER["解析器"]
        PLANNER["查询计划器"]
        EXEC["执行引擎"]
    end

    subgraph "存储层"
        FILE["文件存储<br/>RocksDB"]
        MEM["内存存储"]
        TB["Turso/libSQL"]
        PG["PostgreSQL"]
    end

    subgraph "部署模式"
        EMBED["嵌入式<br/>Rust/JS"]
        SERVER["服务端"]
        CLIENT["客户端"]
    end

    DOC --> Q1
    GRAPH --> Q4
    KV --> Q2
    Q1 --> PARSER
    Q2 --> PARSER
    Q3 --> PARSER
    Q4 --> PARSER
    Q5 --> PARSER
    PARSER --> PLANNER
    PLANNER --> EXEC
    EXEC --> FILE
    EXEC --> MEM
    EXEC --> TB
    EXEC --> PG
```

## 要点总结

- **多模态数据**：同时支持文档、图、KV 三种数据模型
- **SurrealQL**：类 SQL 但专为图数据设计的查询语言
- **嵌入式部署**：可嵌入 Rust 或 JavaScript 应用，减少网络开销
- **多种存储后端**：支持 RocksDB 文件存储、内存存储、Turso/libSQL、PostgreSQL
- **ACID 事务**：完整的事务支持，保证数据一致性
- **实时订阅**：支持数据变更的实时推送
- **权限模型**：基于角色的访问控制和行级安全
- **无模式设计**：支持灵活的数据结构，无需预定义 Schema

## 相关资源

- GitHub: https://github.com/surrealdb/surrealdb
- 文档: https://surrealdb.com/docs/
