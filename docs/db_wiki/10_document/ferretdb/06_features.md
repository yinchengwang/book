# FerretDB 关键特性

## 学习目标

- 掌握 FerretDB 的核心特性与能力边界
- 理解 MongoDB Wire Protocol 兼容性的实现方式
- 了解 PostgreSQL 后端带来的优势
- 掌握多种后端存储的支持情况

## MongoDB 5.0 Wire Protocol 兼容

FerretDB 实现了 MongoDB Wire Protocol，使得现有的 MongoDB 驱动和工具无需修改即可使用：

```mermaid
graph LR
    subgraph "MongoDB 生态"
        DRIVERS["MongoDB Drivers<br/>Java / Python / Node.js / Go / C#"]
        TOOLS["MongoDB Tools<br/>mongosh / mongodump / mongorestore"]
        COMPASS["MongoDB Compass<br/>GUI 管理工具"]
    end

    subgraph "FerretDB"
        PROTO["Wire Protocol 5.0<br/>OpMsg / OpQuery"]
        CMDS["命令实现<br/>find / insert / update / delete"]
    end

    DRIVERS --> PROTO
    TOOLS --> PROTO
    COMPASS --> PROTO
    PROTO --> CMDS
```

### 兼容性详情

| 特性类别 | MongoDB 原生 | FerretDB 支持 | 说明 |
|----------|-------------|---------------|------|
| CRUD 操作 | 完整 | 完整 | find / insert / update / delete |
| 聚合管道 | 完整 | 部分 | 支持 $match/$group/$sort/$limit/$skip/$project/$unwind |
| 索引管理 | 完整 | 部分 | 单字段/复合/唯一索引，地理空间暂不支持 |
| 事务 | 多文档事务 | 部分 | 依赖 PostgreSQL 事务 |
| Change Streams | 完整 | 不支持 | 待实现 |
| MapReduce | 完整 | 不支持 | 建议使用聚合管道 |
| GridFS | 完整 | 不支持 | 大文件存储 |
| 全文搜索 | 完整 | 部分 | 依赖 PostgreSQL 全文搜索 |

### 已实现的命令

```mermaid
graph TB
    subgraph "CRUD 命令"
        FIND["find / findOne"]
        INSERT["insertOne / insertMany"]
        UPDATE["updateOne / updateMany / replaceOne"]
        DELETE["deleteOne / deleteMany"]
        COUNT["count / countDocuments"]
    end

    subgraph "聚合命令"
        AGG["aggregate"]
        MATCH["$match"]
        GROUP["$group"]
        SORT["$sort"]
        LIMIT["$limit"]
        PROJECT["$project"]
        UNWIND["$unwind"]
    end

    subgraph "管理命令"
        LISTCOLL["listCollections"]
        CREATE["create"]
        DROP["drop"]
        ISMASTER["isMaster / hello"]
        BUILDINFO["buildInfo"]
    end

    subgraph "索引命令"
        CREATEIDX["createIndexes"]
        DROPIDX["dropIndexes"]
        LISTIDX["listIndexes"]
    end

    FIND --> AGG
    INSERT --> AGG
    UPDATE --> AGG
    DELETE --> AGG
    COUNT --> AGG
    AGG --> MATCH
    AGG --> GROUP
    AGG --> SORT
    AGG --> LIMIT
    AGG --> PROJECT
    AGG --> UNWIND
```

## PostgreSQL 存储优势

使用 PostgreSQL 作为后端存储，FerretDB 继承了 PostgreSQL 的多项核心优势：

### JSONB 灵活存储

```sql
-- PostgreSQL JSONB 提供高效的文档存储
-- 每个文档存储为一个 JSONB 行
SELECT _ferretdb_document 
FROM "mydb"."mycollection"
WHERE _ferretdb_document->>'status' = 'active';

-- JSONB 支持索引
CREATE INDEX idx_status ON "mydb"."mycollection" 
    USING btree ((_ferretdb_document->>'status'));

-- GIN 索引支持任意字段查询
CREATE INDEX idx_gin ON "mydb"."mycollection" 
    USING gin (_ferretdb_document);
```

| PostgreSQL 特性 | 对 FerretDB 的意义 |
|-----------------|-------------------|
| JSONB 类型 | 原生支持嵌套文档、数组存储 |
| B-tree 索引 | 支持范围查询、排序 |
| GIN 索引 | 支持 JSONB 内任意字段的快速查询 |
| ACID 事务 | 保证数据一致性 |
| 并发控制 | MVCC 提供高并发性能 |
| 扩展生态 | 可利用 PostGIS、pgvector 等扩展 |

### PG 索引能力

```mermaid
graph TB
    subgraph "PostgreSQL 索引类型"
        BTREE["B-tree<br/>范围查询、排序"]
        GIN["GIN<br/>JSONB 全文搜索"]
        HASH["Hash<br/>等值查询"]
        BRIN["BRIN<br/>大表范围扫描"]
    end

    subgraph "FerretDB 索引映射"
        SINGLE["单字段索引<br/>→ B-tree on JSONB path"]
        COMPOUND["复合索引<br/>→ 多列 B-tree"]
        TEXT["文本索引<br/>→ GIN 索引"]
        UNIQUE["唯一索引<br/>→ UNIQUE B-tree"]
    end

    BTREE --> SINGLE
    BTREE --> COMPOUND
    GIN --> TEXT
    BTREE --> UNIQUE
    HASH -.->|"可能使用"| SINGLE
    BRIN -.->|"大表优化"| COMPOUND
```

### PG 事务支持

FerretDB 利用 PostgreSQL 的 ACID 事务能力：

```mermaid
sequenceDiagram
    participant C as 客户端
    participant F as FerretDB
    participant P as PostgreSQL

    C->>F: 开始事务 (startSession)
    F->>P: BEGIN TRANSACTION
    C->>F: insert 操作
    F->>P: INSERT INTO ...
    C->>F: update 操作
    F->>P: UPDATE ... WHERE ...
    alt 提交
        C->>F: commitTransaction
        F->>P: COMMIT
        P-->>F: 事务成功
    else 回滚
        C->>F: abortTransaction
        F->>P: ROLLBACK
        P-->>F: 已回滚
    end
```

## 多种后端支持

FerretDB 设计了可插拔的后端架构，支持多种存储引擎：

```mermaid
graph TB
    subgraph "FerretDB 核心"
        HANDLER["Handler 层<br/>协议处理"]
        BACKEND["Backend 接口<br/>存储抽象"]
    end

    subgraph "后端实现"
        PG["PostgreSQL<br/>生产就绪"]
        HANA["SAP HANA<br/>实验性支持"]
        TIGER["TigerBeetle<br/>实验性支持"]
        SQLITE["SQLite<br/>开发中"]
    end

    HANDLER --> BACKEND
    BACKEND --> PG
    BACKEND --> HANA
    BACKEND --> TIGER
    BACKEND --> SQLITE
```

### 后端对比

| 后端 | 状态 | 适用场景 | 特点 |
|------|------|----------|------|
| PostgreSQL | 生产就绪 | 生产环境首选 | 成熟稳定，功能完整，社区活跃 |
| SAP HANA | 实验性 | 企业 SAP 环境 | 内存数据库，高性能分析 |
| TigerBeetle | 实验性 | 金融场景 | 分布式事务，强一致性 |
| SQLite | 开发中 | 嵌入式场景 | 轻量级，单文件，零配置 |

### 后端选择指南

```mermaid
graph LR
    START["选择后端"] --> Q1{生产环境?}
    Q1 -->|是| PG["PostgreSQL"]
    Q1 -->|否| Q2{需要嵌入式?}
    Q2 -->|是| SQLITE["SQLite<br/>(开发中)"]
    Q2 -->|否| Q3{SAP 环境?}
    Q3 -->|是| HANA["SAP HANA<br/>(实验性)"]
    Q3 -->|否| Q4{金融事务?}
    Q4 -->|是| TIGER["TigerBeetle<br/>(实验性)"]
    Q4 -->|否| PG
```

## 特性总览

```mermaid
graph TB
    subgraph "核心特性"
        COMPAT["MongoDB 协议兼容<br/>5.0 Wire Protocol"]
        PG_STORE["PostgreSQL 存储<br/>JSONB + 事务"]
        MULTI["多后端支持<br/>可插拔架构"]
    end

    subgraph "衍生优势"
        COST["成本优势<br/>使用现有 PG 基础设施"]
        MATURE["成熟稳定<br/>PG 30+ 年积累"]
        COMPLIANCE["合规能力<br/>PG 安全特性"]
        EXT["扩展生态<br/>PostGIS / pgvector"]
    end

    COMPAT --> COST
    PG_STORE --> MATURE
    PG_STORE --> COMPLIANCE
    PG_STORE --> EXT
    MULTI --> COST
```

## 要点总结

- **协议兼容**：实现 MongoDB 5.0 Wire Protocol，支持大多数 CRUD 和聚合命令
- **PG 存储**：利用 PostgreSQL 的 JSONB、索引、事务能力，数据可靠且高效
- **多后端**：可插拔后端架构，PostgreSQL 生产就绪，其他后端实验性支持
- **衍生优势**：降低成本、继承成熟特性、满足合规要求、可扩展生态

## 思考题

1. FerretDB 为什么选择实现 MongoDB 5.0 而不是最新版本的 Wire Protocol？这对兼容性有什么影响？
2. 相比于 MongoDB 原生的 WiredTiger 存储引擎，PostgreSQL 的 JSONB 存储在性能上有哪些优势和劣势？
3. 如果要在 FerretDB 中实现 Change Streams 支持，需要利用 PostgreSQL 的哪些机制？
