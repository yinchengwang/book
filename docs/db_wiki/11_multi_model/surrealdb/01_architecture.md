# SurrealDB 架构设计

## 学习目标

- 理解 SurrealDB 的整体架构分层
- 掌握多模态数据模型的设计思路
- 了解 SurrealQL 查询语言的处理流程

## 整体架构

```mermaid
graph TB
    subgraph "客户端层"
        SDK_JS["JavaScript SDK"]
        SDK_RUST["Rust SDK"]
        SDK_PY["Python SDK"]
        REST["REST API"]
        WS["WebSocket"]
    end

    subgraph "服务层"
        ROUTER["请求路由器"]
        AUTH["认证系统<br/>Scope/Token"]
        SUB["实时订阅<br/>WebSocket"]
    end

    subgraph "查询处理层"
        PARSER["SurrealQL 解析器"]
        PLANNER["查询计划器"]
        EXEC["执行引擎"]
    end

    subgraph "数据模型层"
        DOC["文档模型<br/>JSON/嵌套"]
        GRAPH["图模型<br/>顶点/边"]
        KV["KV 模型<br/>键值存储"]
    end

    subgraph "存储引擎层"
        ROCKSDB["RocksDB<br/>文件存储"]
        MEM["内存存储"]
        TIBS["TiKV/TiDB 后端"]
        PG["PostgreSQL 后端"]
    end

    SDK_JS --> ROUTER
    SDK_RUST --> ROUTER
    SDK_PY --> ROUTER
    REST --> ROUTER
    WS --> ROUTER
    ROUTER --> AUTH
    ROUTER --> SUB
    ROUTER --> PARSER
    PARSER --> PLANNER
    PLANNER --> EXEC
    EXEC --> DOC
    EXEC --> GRAPH
    EXEC --> KV
    DOC --> ROCKSDB
    GRAPH --> ROCKSDB
    KV --> ROCKSDB
    ROCKSDB --> MEM
    ROCKSDB --> TIBS
    ROCKSDB --> PG
```

## SurrealQL 查询处理流程

```mermaid
sequenceDiagram
    participant C as 客户端
    participant P as 解析器
    participant PL as 计划器
    participant E as 执行引擎
    participant S as 存储引擎

    C->>P: SurrealQL 查询语句
    P->>P: 词法分析/语法分析
    P->>PL: 抽象语法树 (AST)
    PL->>PL: 查询优化
    PL->>E: 执行计划
    E->>E: 多模型路由
    E->>S: 文档查询
    E->>S: 图遍历
    E->>S: KV 操作
    S-->>E: 结果集
    E-->>PL: 结果合并
    PL-->>P: 格式化结果
    P-->>C: JSON 响应
```

## 数据模型

### Record 记录

```sql
-- 文档模型：灵活的 JSON 结构
CREATE person SET
    name = '张三',
    age = 25,
    email = 'zhangsan@example.com',
    tags = ['developer', 'rust']
```

### Graph Edge 图边

```sql
-- 图模型：顶点和边的关系
CREATE friend:123456
    SET in = person:1,
    out = person:2,
    since = '2024-01-01';

-- 图遍历查询
SELECT ->friend->person.* FROM person:1
```

### KV 键值

```sql
-- KV 模型：简单的键值存储
CREATE config:app SET
    key = 'max_connections',
    value = '100'
```

## 认证系统

```mermaid
graph TB
    subgraph "认证方式"
        TOKEN["Token 认证"]
        SCOPE["Scope 认证<br/>自定义验证规则"]
        JWT["JWT 令牌"]
        OAUTH["OAuth 集成"]
    end

    subgraph "权限模型"
        NS["Namespace 级"]
        DB["Database 级"]
        TB["Table 级"]
        ROW["Row 级"]
    end

    TOKEN --> NS
    SCOPE --> DB
    JWT --> TB
    OAUTH --> ROW
```

## 要点总结

- **三层架构**：SurrealQL 解析层、执行引擎层、存储引擎层
- **多模态数据**：文档、图、KV 三种模型统一管理
- **SurrealQL**：类 SQL 语法，支持图遍历
- **多种存储**：RocksDB 默认，支持 TiKV 和 PostgreSQL

## 思考题

1. SurrealDB 如何在同一个查询中混合使用文档、图和 KV 模型？
2. 图遍历查询在 RocksDB 中如何高效执行？
