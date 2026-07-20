# SurrealDB 使用场景与选型对比

## 学习目标

- 理解 SurrealDB 的最佳适用场景
- 掌握与其他多模态数据库的选型对比

## 适用场景

```mermaid
graph TD
    A[SurrealDB 适用场景] --> B[全栈应用]
    A --> C[实时协作]
    A --> D[IoT 数据]
    A --> E[内容管理]

    B --> B1[前后端共享数据库]
    B --> B2[嵌入式部署减少延迟]

    C --> C1[WebSocket 实时同步]
    C --> C2[多人编辑]

    D --> D1[灵活 Schema]
    D --> D2[时序数据]

    E --> E1[文档 + 关系]
    E --> E2[图遍历标签系统]
```

## 选型对比

| 维度 | SurrealDB | Neo4j | MongoDB | PostgreSQL |
|------|-----------|-------|---------|------------|
| 数据模型 | 文档+图+KV | 图 | 文档 | 关系 |
| 查询语言 | SurrealQL | Cypher | MQL | SQL |
| 图遍历 | 支持 | 强 | 弱 | 无 |
| 实时订阅 | 原生支持 | 需扩展 | Change Stream | 需扩展 |
| 嵌入式 | 支持 | 不支持 | 不支持 | 不支持 |
| 开源协议 | Apache 2.0 | GPLv3 | SSPL | PostgreSQL |
| 社区 | 28k stars | 高 | 极高 | 极高 |

## 决策流程

```mermaid
flowchart TD
    A[需要数据库?] --> B{是否需要图遍历?}
    B -->|是| C{是否需要文档灵活性?}
    B -->|否| D{是否需要文档模型?}
    C -->|是| E[SurrealDB]
    C -->|否| F[Neo4j]
    D -->|是| G{是否需要实时订阅?}
    D -->|否| H[PostgreSQL]
    G -->|是| I{是否需要嵌入式?}
    G -->|否| J[MongoDB]
    I -->|是| E
    I -->|否| J
```

## 要点总结

- **全栈应用**是 SurrealDB 的核心场景
- **嵌入式部署**和**多模态**是核心优势
- 与 Neo4j 相比增加了文档和 KV 能力

## 思考题

1. SurrealDB 与 FaunaDB 有何异同？
2. 嵌入式部署如何处理数据同步？
