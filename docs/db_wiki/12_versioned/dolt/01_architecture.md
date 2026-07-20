# Dolt 架构设计

## 学习目标

- 理解 Dolt 的版本控制与数据库结合架构
- 掌握 Prolly-Tree 的核心原理
- 了解 Git 概念在数据库中的映射

## 整体架构

```mermaid
graph TB
    subgraph "客户端层"
        DOLT_CLI["dolt CLI"]
        SQL_CLI["MySQL 客户端"]
        DOLTHUB["DoltHub"]
    end

    subgraph "SQL 层"
        SQL_ENGINE["SQL 引擎<br/>Go MySQL Server"]
        ANALYZER["查询分析器"]
        EXEC["执行器"]
    end

    subgraph "版本控制层"
        BRANCH["分支管理"]
        COMMIT["Commit 链"]
        MERGE["合并引擎"]
        DIFF["Diff 计算"]
    end

    subgraph "存储引擎层"
        PROLLY["Prolly-Tree<br/>内容可寻址 B-Tree"]
        CHUNK["Chunk Store"]
        NOMS["Noms 数据模型"]
    end

    subgraph "磁盘存储"
        DATA["数据文件"]
        MANIFEST["Manifest"]
        CHUNK_DB["Chunk 数据库"]
    end

    DOLT_CLI --> SQL_ENGINE
    SQL_CLI --> SQL_ENGINE
    DOLTHUB --> BRANCH
    SQL_ENGINE --> ANALYZER
    ANALYZER --> EXEC
    EXEC --> BRANCH
    EXEC --> COMMIT
    BRANCH --> PROLLY
    COMMIT --> PROLLY
    MERGE --> PROLLY
    DIFF --> PROLLY
    PROLLY --> CHUNK
    CHUNK --> NOMS
    NOMS --> DATA
    NOMS --> MANIFEST
    NOMS --> CHUNK_DB
```

## Prolly-Tree 结构

```mermaid
graph TB
    subgraph "Prolly-Tree B-Tree"
        ROOT["根节点<br/>Chunk Hash: abc123"]
        INT1["内部节点<br/>Chunk Hash: def456"]
        INT2["内部节点<br/>Chunk Hash: ghi789"]
        LEAF1["叶子节点<br/>Chunk Hash: jkl012"]
        LEAF2["叶子节点<br/>Chunk Hash: mno345"]
        LEAF3["叶子节点<br/>Chunk Hash: pqr678"]
    end

    ROOT --> INT1
    ROOT --> INT2
    INT1 --> LEAF1
    INT1 --> LEAF2
    INT2 --> LEAF3
```

## Commit 链

```mermaid
sequenceDiagram
    participant U as 用户
    participant S as SQL 引擎
    participant V as 版本控制层
    participant P as Prolly-Tree

    U->>S: INSERT INTO users VALUES (...)
    S->>V: 记录变更
    U->>S: INSERT INTO users VALUES (...)
    S->>V: 记录变更
    U->>S: COMMIT
    S->>V: 创建 Commit 对象
    V->>P: 生成 Prolly-Tree 快照
    P-->>V: Root Hash: abc123
    V->>V: 写入 Commit 链
    V-->>S: Commit 成功
    S-->>U: 提交成功
```

## 要点总结

- **SQL + Git**：同时支持 SQL 查询和版本控制
- **Prolly-Tree**：内容可寻址的 B-Tree 变体
- **Commit 链**：类似 Git 的提交历史
- **分支管理**：支持创建、切换、合并分支

## 思考题

1. Prolly-Tree 与标准 B-Tree 相比有何优势？
2. Dolt 如何处理大文件的版本控制？
