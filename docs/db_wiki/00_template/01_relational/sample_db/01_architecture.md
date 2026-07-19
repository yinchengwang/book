# sample_db 架构设计

## 学习目标
- 理解 sample_db 的整体架构分层
- 掌握各层之间的数据流

## 整体架构

```mermaid
graph TD
    Client[客户端] --> Network[网络层]
    Network --> Protocol[协议层]
    Protocol --> Query[查询引擎]
    Query --> Parser[解析器]
    Parser --> Optimizer[优化器]
    Optimizer --> Executor[执行器]
    Executor --> Storage[存储引擎]
    Storage --> Cacher[缓存层]
    Storage --> Disk[磁盘]

    subgraph "存储引擎内部"
        Storage --> Buffer[Buffer Pool]
        Buffer --> Page[页面管理]
        Page --> WAL[WAL 日志]
    end
```

## 各层职责

### 网络层
[说明]
### 协议层
[说明]
### 查询引擎
[说明]
### 存储引擎
[说明]
### 工具层
[说明]

## 关键数据流

```mermaid
sequenceDiagram
    participant C as 客户端
    participant Q as 查询引擎
    participant S as 存储引擎
    participant D as 磁盘

    C->>Q: SQL 请求
    Q->>Q: 解析 → 优化
    Q->>S: 执行计划
    S->>D: 页面 I/O
    D-->>S: 数据页
    S-->>Q: 结果集
    Q-->>C: 响应
```

## 要点总结

- 架构的核心分层思路
- 各层间的接口设计

## 思考题

1. 为什么 sample_db 选择这种分层？
2. 如果去掉某一层会有什么后果？