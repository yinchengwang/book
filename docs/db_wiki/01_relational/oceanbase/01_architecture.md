# OceanBase 架构详解

## 学习目标

- 掌握 OceanBase 的四层架构设计
- 理解 OBServer 节点的角色和职责
- 对比 OceanBase 与 TiDB、CockroachDB 的架构差异

## 四层架构

```mermaid
graph TB
    A[客户端] --> B[SQL 层]
    B --> C[事务层]
    C --> D[分区层]
    D --> E[存储层]

    F[SQL 层] --> G[解析器]
    F --> H[优化器]
    F --> I[执行器]
    F --> J[分布式执行]

    K[事务层] --> L[MVCC]
    K --> M[GTS 全局时间戳]
    K --> N[两阶段提交]

    O[分区层] --> P[Partition 分片]
    O --> Q[Paxos Group]
    O --> R[负载均衡]

    S[存储层] --> T[自研 LSM-Tree]
    S --> U[行存 + 列存]
    S --> V[在线合并]
```

## OBServer 节点

每个 OBServer 是一个独立的进程，包含所有四层功能。

```mermaid
graph TB
    A[OBServer 节点] --> B[SQL 引擎]
    A --> C[事务引擎]
    A --> D[分区管理]
    A --> E[存储引擎]

    F[职责] --> G[计算：执行 SQL 查询]
    F --> H[存储：管理本地数据]
    F --> I[共识：参与 Paxos 协议]
```

### OBServer 角色

- **主副本（Leader）**：承担读写请求
- **从副本（Follower）**：只读，参与 Paxos 投票
- **学习者（Learner）**：只读，不投票（可选）

## 请求流程

```mermaid
sequenceDiagram
    participant Client AS 客户端
    participant SQL AS SQL 层
    participant Txn AS 事务层
    participant Part AS 分区层
    participant Storage AS 存储层

    Client->>SQL: 发送 SQL 语句
    SQL->>SQL: 解析 → 优化 → 生成执行计划
    SQL->>Txn: 开启事务
    Txn->>Txn: 分配 GTS 时间戳
    SQL->>Part: 路由到分区主副本
    Part->>Storage: 读取/写入数据
    Storage-->>Part: 返回结果
    Part-->>SQL: 返回结果
    SQL->>Txn: 提交事务
    Txn->>Txn: 两阶段提交
    Txn-->>Client: 返回响应
```

## 与 TiDB 架构对比

| 维度 | OceanBase | TiDB |
|------|-----------|------|
| 节点角色 | OBServer（计算+存储合一） | TiDB Server（计算） + TiKV（存储） |
| 架构模式 | 对等架构 | 计算存储分离 |
| 调度器 | 内置（无独立组件） | PD（Placement Driver） |
| 存储引擎 | 自研 LSM-Tree | RocksDB |
| 列存 | 内置（行存+列存混合） | TiFlash（独立组件） |

## 与 CockroachDB 架构对比

| 维度 | OceanBase | CockroachDB |
|------|-----------|------------|
| 节点角色 | OBServer（对等） | CockroachDB Node（对等） |
| 共识协议 | Multi-Paxos | Raft |
| 分片单位 | Partition（分区表） | Range（512MB） |
| 存储引擎 | 自研 LSM-Tree | RocksDB |
| 列存 | 支持 | 不支持 |

## 与 PostgreSQL 架构对比

| 维度 | OceanBase | PostgreSQL |
|------|-----------|------------|
| 节点角色 | 分布式节点 | 单体进程 |
| 共识协议 | Multi-Paxos | 无（流复制） |
| 水平扩展 | 支持 | 不支持 |
| 存储引擎 | LSM-Tree | 堆表（Heap） |

## 要点总结

- OceanBase 采用四层架构：SQL、事务、分区、存储
- 每个 OBServer 既做计算也做存储，无单点瓶颈
- 与 TiDB 相比：对等架构 vs 计算存储分离
- 与 CockroachDB 相比：Paxos vs Raft，自研引擎 vs RocksDB
- 与 PostgreSQL 相比：分布式 vs 单体

## 思考题

1. OceanBase 的对等架构在运维和扩展上有何优劣势？
2. OBServer 的角色（Leader/Follower/Learner）如何影响负载均衡？
3. 如果一个 OBServer 节点宕机，Paxos 如何保证数据一致性？