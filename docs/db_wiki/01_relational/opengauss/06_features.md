# openGauss 核心特性

## 学习目标

- 掌握 openGauss 的核心特性
- 理解 openGauss 的企业级增强
- 对比 openGauss 与 PostgreSQL 的特性差异

## 核心特性

### 1. 三存储引擎

```mermaid
graph TB
    A[CREATE TABLE] --> B{ASTORE / CSTORE / MOT}

    B -->|ASTORE| C[行存<br/>OLTP 场景]
    B -->|CSTORE| D[列存<br/>OLAP 场景]
    B -->|MOT| E[内存表<br/>高性能 OLTP]

    C --> F[堆表存储<br/>类似 PG]
    D --> G[列存文件<br/>CU 压缩]
    E --> H[内存索引<br/>Masstree]
```

```sql
-- ASTORE（默认）
CREATE TABLE t1 (id INT, name TEXT);

-- CSTORE（列存）
CREATE TABLE t2 (id INT, name TEXT) WITH (ORIENTATION = COLUMN);

-- MOT（内存表）
CREATE FOREIGN TABLE t3 (id INT, name TEXT)
SERVER mot_server OPTIONS (orientation 'row');
```

### 2. MOT 内存表

```mermaid
sequenceDiagram
    participant Client AS 客户端
    participant MOT AS MOT 引擎
    participant Log AS WAL
    participant Checkpoint AS 检查点

    Client->>MOT: 写入数据
    MOT->>MOT: 乐观锁检查
    MOT->>Log: 写 WAL
    MOT-->>Client: 成功

    MOT->>Checkpoint: 定期检查点
    Checkpoint->>Checkpoint: 保存内存快照
```

### 3. 全密态数据库

```mermaid
sequenceDiagram
    participant Client AS 客户端
    participant Proxy AS 加密代理
    participant Server AS 服务端

    Client->>Proxy: 明文数据
    Proxy->>Proxy: 客户端加密
    Proxy->>Server: 密文数据
    Server->>Server: 密文存储 + 计算
    Server-->>Proxy: 密文结果
    Proxy->>Proxy: 客户端解密
    Proxy-->>Client: 明文结果
```

### 4. AI 优化器

```mermaid
graph TB
    A[AI 优化器] --> B[慢 SQL 检测]
    A --> C[智能索引推荐]
    A --> D[执行计划预测]

    B --> E[收集执行统计]
    B --> F[识别慢查询]

    C --> G[分析查询模式]
    C --> H[推荐索引]

    D --> I[预测执行时间]
    D --> J[选择最优计划]
```

## 特性对比

| 特性 | openGauss | PostgreSQL |
|------|-----------|------------|
| 存储引擎 | ASTORE + CSTORE + MOT | Heap |
| 内存表 | MOT | 不支持 |
| 列存 | CSTORE | 不支持（需扩展） |
| 全密态 | 支持 | 不支持 |
| AI 优化器 | 支持 | 不支持 |
| LLVM JIT | 支持（增强） | 支持（PG 11+） |
| SMP 并行 | 支持 | 支持 |
| Oracle 兼容 | 部分支持 | 不支持 |

## 安全特性

| 特性 | openGauss | PostgreSQL |
|------|-----------|------------|
| 全密态查询 | 支持 | 不支持 |
| 动态脱敏 | 支持 | 不支持 |
| 审计日志 | 支持 | 支持 |
| 行级安全 | 支持 | 支持 |
| SSL/TLS | 支持 | 支持 |

## 高可用特性

| 特性 | openGauss | PostgreSQL |
|------|-----------|------------|
| 主备复制 | 同步/异步 | 流复制 |
| 级联备库 | 支持 | 支持 |
| 逻辑复制 | 支持 | 支持 |
| 自动故障切换 | 支持（DCF） | 需第三方工具 |
| 分布式一致性 | DCF（Paxos） | 不支持 |

## 要点总结

- openGauss 核心差异：三存储引擎、MOT 内存表、全密态数据库
- AI 优化器提供慢 SQL 检测和智能索引推荐
- 安全增强：全密态、动态脱敏、审计日志
- 高可用增强：DCF 分布式一致性框架
- 与 PG 相比：功能丰富、性能优化、安全增强

## 思考题

1. openGauss 的 MOT 内存表相比 Redis 等内存数据库，在事务支持和数据持久化上有何优势？
2. openGauss 的全密态数据库对查询性能的影响有多大？哪些场景适合使用全密态？
3. openGauss 的 AI 优化器如何与传统的代价优化器协同工作？