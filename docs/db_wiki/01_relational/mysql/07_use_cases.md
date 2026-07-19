# MySQL 使用场景与选型

## 学习目标

- 了解 MySQL 在不同业务场景下的适用性
- 掌握 MySQL 与其他数据库（PostgreSQL、MongoDB、Redis、ES）的选型对比
- 理解 MySQL 的读写分离架构与高可用方案

## 核心概念

- **OLTP 场景**：MySQL 的核心战场，适合高并发短查询
- **读写分离**：主库写、从库读，通过中间件（ProxySQL/MyCat）路由
- **分库分表**：水平扩展的解决方案，ShardingSphere/MyCat 实现
- **高可用架构**：MHA/MGR/Orchestrator 实现自动故障切换
- **云数据库**：RDS/Aurora/PolarDB/TiDB 等兼容 MySQL 协议的云服务

## 典型应用场景

### 互联网 OLTP 系统

MySQL 是互联网公司最常用的关系型数据库，适合以下业务：

```mermaid
graph TB
    subgraph "互联网 OLTP 场景"
        EC[电商系统<br/>订单/商品/用户]
        SOC[社交平台<br/>帖子/评论/好友]
        FIN[金融系统<br/>账户/交易/风控]
        CMS[内容管理<br/>文章/标签/分类]
        GAME[游戏系统<br/>玩家/道具/排行榜]
    end

    EC --> M[MySQL 集群]
    SOC --> M
    FIN --> M
    CMS --> M
    GAME --> M

    M --> RW[读写分离]
    M --> HA[高可用]
    M --> SCALE[分库分表]
```

**典型架构**：

```mermaid
graph TB
    subgraph "应用层"
        APP[应用服务]
        CACHE[Redis 缓存]
    end

    subgraph "数据库中间件"
        PS[ProxySQL / MyCat<br/>读写分离 + 路由]
    end

    subgraph "MySQL 集群"
        M1[Master<br/>写入]
        S1[Slave 1<br/>读]
        S2[Slave 2<br/>读]
        S3[Slave 3<br/>读/备份]
    end

    APP --> CACHE
    APP --> PS
    PS --> M1
    PS --> S1
    PS --> S2
    PS --> S3
    M1 -->|binlog 复制| S1
    M1 -->|binlog 复制| S2
    M1 -->|binlog 复制| S3
```

**关键技术**：

- 读写分离：主库处理写操作，从库处理读操作
- 连接池：Druid/HikariCP 减少连接创建开销
- 缓存层：Redis 缓存热点数据，减少数据库压力
- 分库分表：ShardingSphere 实现水平扩展

### 电商系统

```mermaid
graph LR
    A[电商系统] --> B[订单表<br/>高写入]
    A --> C[商品表<br/>高读取]
    A --> D[用户表<br/>等值查询]
    A --> E[库存表<br/>强一致性]

    B --> F[分库分表<br/>按用户ID]
    C --> G[Redis 缓存<br/>商品详情]
    D --> H[聚簇索引<br/>按主键查询]
    E --> I[行锁 + 事务<br/>防止超卖]
```

**MySQL 的优势**：

- 聚簇索引在等值查询和范围扫描上性能优异
- InnoDB 行锁支持高并发写入
- 主从复制架构降低查询延迟
- 事务支持保证订单和库存的一致性

### 金融系统

```mermaid
graph TB
    subgraph "金融系统"
        ACCT[账户系统<br/>余额/流水]
        TXN[交易系统<br/>转账/支付]
        RISK[风控系统<br/>规则/黑白名单]
        SETTLE[结算系统<br/>对账/清算]
    end

    ACCT -->|ACID 事务| M[MySQL]
    TXN -->|XA 事务| M
    RISK -->|查询分析| M
    SETTLE -->|批量处理| M
```

**优势**：

- InnoDB 的 ACID 事务保证资金安全
- 行锁在并发转账时避免死锁
- 两阶段提交保证分布式事务一致性
- 审计日志通过 binlog 实现

### 内容管理系统（CMS）

```mermaid
graph LR
    A[CMS 系统] --> B[用户/角色<br/>权限管理]
    A --> C[文章/分类<br/>树形结构]
    A --> D[标签/搜索<br/>全文索引]

    C --> E[递归 CTE<br/>查询树形结构]
    D --> F[FULLTEXT Index<br/>MySQL 5.6+]
```

**MySQL 8.0+ 的优势**：

- 递归 CTE 支持树形结构查询（分类、菜单）
- 全文索引支持文章搜索
- JSON 类型支持灵活的内容结构

## 选型对比

### MySQL vs PostgreSQL

```mermaid
graph TB
    subgraph "MySQL 优势"
        M1[聚簇索引<br/>等值查询快]
        M2[主从复制<br/>生态成熟]
        M3[易用性强<br/>上手快]
        M4[云生态<br/>RDS/Aurora]
    end

    subgraph "PostgreSQL 优势"
        P1[复杂查询<br/>JOIN/CTE/窗口函数]
        P2[扩展机制<br/>索引/数据类型]
        P3[GIS 能力<br/>PostGIS]
        P4[标准兼容<br/>SQL标准]
    end

    subgraph "选型建议"
        Q1[简单 OLTP -> MySQL]
        Q2[复杂分析 -> PG]
        Q3[GIS -> PG]
        Q4[高可用 -> MySQL]
    end
```

| 维度 | MySQL | PostgreSQL |
|------|-------|-----------|
| 存储架构 | 聚簇索引（IOT） | Heap 表 |
| 隔离级别默认 | RR | RC |
| 复制方式 | binlog 逻辑复制 | WAL 物理流复制 |
| 复杂查询 | 较弱 | 强 |
| JSON 支持 | 虚拟列+索引 | JSONB GIN 索引 |
| GIS | 基本 R-Tree | 强大的 PostGIS |
| 扩展性 | 可插拔引擎 | 可扩展访问方法 |
| 运维成本 | 低 | 中 |
| 云服务 | 最丰富（RDS/Aurora/PolarDB） | 丰富（RDS/AlloyDB/CockroachDB） |

### MySQL vs MongoDB

| 维度 | MySQL | MongoDB |
|------|-------|---------|
| 数据模型 | 关系型（表+行+列） | 文档型（JSON/BSON） |
| 事务 | 强 ACID | 4.0+ 支持多文档事务 |
| 扩展性 | 分库分表（复杂） | 原生分片（auto-sharding） |
| Schema 变更 | 需要 ALTER TABLE | 无 Schema 约束 |
| Join 能力 | 强 | 弱（$lookup 性能差） |
| 场景 | 结构化数据 | 半结构化/快速原型 |

### MySQL vs Redis

| 维度 | MySQL | Redis |
|------|-------|-------|
| 存储 | 磁盘持久化 | 内存+可选的持久化 |
| 模型 | 关系型 | KV / 多种数据结构 |
| 查询 | SQL 复杂查询 | 简单键值操作 |
| 延迟 | 毫秒级 | 微秒级 |
| 场景 | 持久化存储 | 缓存/会话/计数器 |

### MySQL vs Elasticsearch

| 维度 | MySQL | Elasticsearch |
|------|-------|--------------|
| 核心能力 | 数据存储/事务 | 全文搜索/分析 |
| 查询方式 | SQL | DSL（JSON） |
| 索引 | B+Tree | 倒排索引 |
| 实时性 | 强一致性 | 近实时（NRT） |
| 场景 | 在线事务 | 日志搜索/全文检索 |

## 读写分离架构

### 基于中间件的读写分离

```mermaid
graph TB
    subgraph "应用层"
        APP1[APP 1]
        APP2[APP 2]
        APP3[APP 3]
    end

    subgraph "中间件层"
        PROXY[ProxySQL / MySQL Router<br/>读写分离 + 连接池]
    end

    subgraph "数据库层"
        M[Master<br/>写操作]
        S1[Slave 1<br/>读操作]
        S2[Slave 2<br/>读操作]
        S3[Slave 3<br/>读操作]
    end

    APP1 --> PROXY
    APP2 --> PROXY
    APP3 --> PROXY
    PROXY -->|INSERT/UPDATE/DELETE| M
    PROXY -->|SELECT| S1
    PROXY -->|SELECT| S2
    PROXY -->|SELECT| S3
    M -->|binlog| S1
    M -->|binlog| S2
    M -->|binlog| S3
```

**中间件选择**：

| 中间件 | 特点 | 适用场景 |
|--------|------|---------|
| ProxySQL | 高性能、丰富的路由规则、连接池 | 生产环境首选 |
| MySQL Router | Oracle 官方、轻量 | 简单读写分离 |
| MyCat | 分库分表 + 读写分离 | 需要分库分表 |
| ShardingSphere | 分库分表 + 读写分离 + 分布式事务 | 复杂分片场景 |

## 高可用架构

### MHA（Master High Availability）

```mermaid
sequenceDiagram
    participant Client
    participant MHA as MHA Manager
    participant M as Master
    participant S1 as Slave 1
    participant S2 as Slave 2

    M ->> S1: binlog 复制
    M ->> S2: binlog 复制

    Note over M: Master 宕机
    MHA ->> M: 检测到 Master 不可用
    MHA ->> S1: 选择新 Master
    MHA ->> S1: 应用差异 binlog
    MHA ->> S2: 变更复制源
    S2 ->> S1: 连接新 Master
    MHA ->> Client: 返回新 Master 地址
```

### 组复制（MGR）

```mermaid
graph TB
    subgraph "MGR 集群"
        N1[Node 1<br/>Primary]
        N2[Node 2<br/>Secondary]
        N3[Node 3<br/>Secondary]

        N1 <-->|Paxos 共识| N2
        N2 <-->|Paxos 共识| N3
        N3 <-->|Paxos 共识| N1
    end

    subgraph "故障转移"
        N1 -.-x FAIL[Node 1 宕机]
        N2 -->|选举| NEW[新 Primary]
        N3 -->|认可| NEW
    end
```

## 云数据库方案

### AWS RDS for MySQL

- 托管服务，自动备份、自动故障切换
- 支持 Multi-AZ 部署，跨可用区高可用
- 只读副本（Read Replica）最多 15 个
- 支持自动扩缩容

### AWS Aurora MySQL

- 兼容 MySQL 5.7/8.0 协议
- 存储与计算分离，6 副本跨 3 AZ
- 写入吞吐量是标准 MySQL 的 5x
- 故障切换秒级完成

### 阿里云 PolarDB MySQL

- 兼容 MySQL 8.0
- 计算与存储分离，共享存储
- 一写多读，最多 16 个只读节点
- 存储自动扩容，最大 100TB

## 分库分表

### 垂直分库

```mermaid
graph TB
    subgraph "原始单库"
        DB[单库<br/>用户表/订单表/商品表]
    end

    subgraph "垂直拆分后"
        DB1[用户库<br/>用户表]
        DB2[订单库<br/>订单表]
        DB3[商品库<br/>商品表]
    end

    DB -->|拆分| DB1
    DB -->|拆分| DB2
    DB -->|拆分| DB3
```

### 水平分表

```mermaid
graph TB
    subgraph "原始表"
        TB[订单表<br/>1亿行]
    end

    subgraph "水平分表后"
        T1[orders_0000<br/>userId % 16 = 0]
        T2[orders_0001<br/>userId % 16 = 1]
        TD[...]
        T16[orders_0015<br/>userId % 16 = 15]
    end

    TB -->|按 userId 哈希| T1
    TB -->|按 userId 哈希| T2
    TB -->|按 userId 哈希| TD
    TB -->|按 userId 哈希| T16
```

**分片策略**：

| 策略 | 说明 | 适用场景 |
|------|------|---------|
| 范围分片 | 按时间/ID 范围分片 | 日志、时序数据 |
| 哈希分片 | 按用户 ID 哈希分片 | 用户数据 |
| 列表分片 | 按地区/业务线分片 | 多租户系统 |
| 一致性哈希 | 减少分片变更时的数据迁移 | 动态扩容 |

## 要点总结

- MySQL 的核心战场是 OLTP 场景，高并发读写、简单查询为主
- 读写分离 + 主从复制是互联网公司最常用的架构模式
- 分库分表是 MySQL 水平扩展的主要手段，但增加了复杂度
- 云数据库（RDS/Aurora/PolarDB）降低了运维复杂度，是当前主流选择
- 在复杂分析、GIS、全文检索等场景，MySQL 不如 PostgreSQL 和专用数据库
- 选型时应根据业务场景选择最合适的数据库，而不是一味使用 MySQL

## 思考题

1. 在什么场景下，MySQL 的读写分离架构会引入一致性问题？如何解决？
2. 分库分表后，跨分片的查询和事务如何处理？
3. 为什么许多互联网公司从 MySQL 迁移到 TiDB/CockroachDB？MySQL 的分布式扩展遇到了什么瓶颈？
4. 如果你的项目使用 MySQL 作为存储，InnoDB 的哪些特性可以借鉴到你的存储引擎中？