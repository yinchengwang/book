# Isolation 隔离级别

## 学习目标

- 理解 PostgreSQL 支持的四种隔离级别及其实现差异
- 掌握 Read Committed 与 Repeatable Read 在 MVCC 快照获取时机上的区别
- 了解 Serializable 隔离级别的 SSI（可串行化快照隔离）实现原理

## 核心概念

- **Read Uncommitted**：PG 不支持——实际上映射为 Read Committed
- **Read Committed**（默认）：每条语句执行前获取独立快照
- **Repeatable Read**：事务开始时获取快照，整个事务复用
- **Serializable**：基于 SSI（Serializable Snapshot Isolation）检测写冲突
- **Predicate Lock**：谓词锁，用于 SSI 检测幻读冲突
- **SSI（Serializable Snapshot Isolation）**：通过 SIREAD 锁检测读/写冲突，实现真正的可串行化

## 隔离级别概览

```mermaid
graph TD
    A[SQL 标准隔离级别] --> B[Read Uncommitted<br/>→ PG 映射为 RC]
    A --> C[Read Committed<br/>PG 默认]
    A --> D[Repeatable Read]
    A --> E[Serializable]

    B --> F[脏读<br/>PG 不会发生]
    C --> G[每次语句刷新快照]
    D --> H[事务级快照<br/>无幻读?]
    E --> I[SSI 算法<br/>可串行化]

    C -.-> J[不可重复读<br/>RC 可能出现]
    D -.-> K[可重复读<br/>但可能幻读]
    E -.-> L[完全可串行化]
```

**PG 的实际行为**：

| 隔离级别 | PG 实际 | 脏读 | 不可重复读 | 幻读 | 写倾斜 |
|----------|---------|------|------------|------|--------|
| Read Uncommitted | Read Committed | 不可能 | 可能 | 可能 | 可能 |
| Read Committed（默认） | Read Committed | 不可能 | 可能 | 可能 | 可能 |
| Repeatable Read | 快照隔离 | 不可能 | 不可能 | 可能 | 可能 |
| Serializable | SSI | 不可能 | 不可能 | 不可能 | 不可能 |

## Read Committed（默认）

```mermaid
sequenceDiagram
    participant Tx1 as 事务 1 (T1)
    participant Snapshot as 快照
    participant Tx2 as 事务 2 (T2)
    participant Data as 数据行

    Tx1->>Snapshot: BEGIN
    Tx1->>Data: SELECT * FROM t (语句 1)
    Note over Tx1,Snapshot: 快照 1: T1 启动时活跃
    Data-->>Tx1: 行 A: a=1

    Tx2->>Data: UPDATE t SET a=2 WHERE id=1
    Tx2->>Data: COMMIT

    Tx1->>Data: SELECT * FROM t (语句 2)
    Note over Tx1,Snapshot: 快照 2: T2 已提交
    Data-->>Tx1: 行 A: a=2 (T2 的修改可见)
    Note over Tx1: 不可重复读: 两次 SELECT 结果不同
```

**Read Committed 规则**：

- 每条 SQL 语句开始时获取新 Snapshot
- 读到的数据是"该语句启动时刻的最新已提交数据"
- **不可重复读**：同一个事务内两次 SELECT 可能不同

**PG 的 RC 实现**：

```mermaid
flowchart TD
    A[ExecQuery] --> B[GetTransactionSnapshot<br/>→ 当前活跃事务列表]
    B --> C[执行扫描]
    C --> D{扫描期间有新提交?}
    D -->|是| E[Tuple 可见性判断<br/>用当前语句快照]
    D -->|否| F[继续]
```

## Repeatable Read

```mermaid
sequenceDiagram
    participant Tx1 as 事务 1 (T1)
    participant Snapshot as 快照
    participant Tx2 as 事务 2 (T2)

    Tx1->>Snapshot: BEGIN
    Tx1->>Snapshot: 第一个 SQL → 获取快照 S1
    Note over Snapshot: S1 = [T1, T2(如果活跃)]

    Tx2->>Tx2: UPDATE t SET a=2
    Tx2->>Tx2: COMMIT

    Tx1->>Data: SELECT * FROM t
    Note over Tx1: 快照 S1 看不到 T2 的修改
    Data-->>Tx1: 行 A: a=1 (旧版本)

    Tx1->>Data: SELECT * FROM t (again)
    Note over Tx1: 还是用 S1
    Data-->>Tx1: 行 A: a=1 (一致)

    Tx1->>Tx1: UPDATE t SET a=10 WHERE id=1
    Note over Tx1: T2 已提交修改 a=2
    Note over Tx1: 检测到冲突 → 可串行化失败
    Note over Tx1: ERROR: could not serialize access
```

**Repeatable Read 规则**：

- 事务第一个 SQL 时获取 Snapshot，整个事务**不再刷新**
- 看到的事务状态始终是事务开始时的快照
- 写操作检测"先写后写"冲突 → 检测到则 abort

## Serializable（SSI）

PG 的 Serializable 不是简单的锁升级，而是基于 **SSI（Serializable Snapshot Isolation）** 算法：

```mermaid
graph TD
    A[SSI 核心原理] --> B[SIREAD Lock<br/>谓词锁]
    A --> C[读写冲突检测<br/>r/w anti-dependency]
    A --> D[冲突图循环<br/>→ Abort]

    B --> B1[谓词: WHERE id=1 锁条件]
    B --> B2[粒度: 索引键/页/表]
    B --> B3[不阻塞读]
    B --> B4[只记录读过的范围]

    C --> C1[r → w: 读旧版本, 写新版本]
    C --> C2[w → r: 写后读, 不可串行化]
    C --> C3[循环: T1→T2→T1 → 冲突]

    D --> D1[检测到循环 → abort 一方]
```

### SSI 的三种依赖

```mermaid
graph LR
    subgraph "读写依赖（r/w anti-dependency）"
        T1[事务 1] -->|读 v1| A[值 v1]
        T2[事务 2] -->|写 v2| A
    end

    subgraph "写读依赖（w/r anti-dependency）"
        B[值 v2] -->|写| T2
        B -->|读| T1
    end

    subgraph "写写依赖（w/w anti-dependency）"
        C[值] -->|写| T1
        C -->|写| T2
    end
```

**冲突图**：

```mermaid
graph TD
    T1[事务 1] -->|r/w| T2[事务 2]
    T2 -->|r/w| T3[事务 3]
    T3 -->|r/w| T1
    style T1 stroke:#f66,stroke-width:3px
    style T2 stroke:#f66
    style T3 stroke:#f66
```

循环检测到后，abort 最晚提交的事务。

### Predicate Lock（谓词锁）

谓词锁是 SSI 的基础：

```mermaid
graph LR
    A[Predicate Lock] --> B[粒度: 索引键]
    A --> C[粒度: 页面范围]
    A --> D[粒度: 全表]

    E[查询 WHERE id=1] --> B
    F[查询 WHERE id BETWEEN 1 AND 100] --> C
    G[全表扫描] --> D
```

**锁存储**：Predicate Lock 存储在共享内存中，每个 SIREAD Lock 记录 `(xid, relation, key/range/tag)`。内存不足时会提升粒度（coarsen）。

### SSI 写冲突示例

```sql
-- 会话 1                              -- 会话 2
BEGIN ISOLATION LEVEL SERIALIZABLE;     BEGIN ISOLATION LEVEL SERIALIZABLE;
SELECT * FROM t WHERE id=1;             SELECT * FROM t WHERE id=1;
-- 读到 a=1                             -- 读到 a=1
UPDATE t SET a=2 WHERE id=1;            UPDATE t SET a=3 WHERE id=1;
-- 等待...                              -- COMMIT;
-- ERROR: could not serialize access
```

## 隔离级别对比

```mermaid
graph TD
    A[隔离级别选择] --> B[默认 RC]
    A --> C[RR]
    A --> D[SERIALIZABLE]

    B --> B1[适用: 大多数 OLTP]
    B --> B2[代价: 不可重复读]
    B --> B3[无谓词锁开销]

    C --> C1[适用: 报表/统计]
    C --> C2[代价: 写冲突 abort]
    C --> C3[无谓词锁开销]

    D --> D1[适用: 金融/强一致性]
    D --> D2[代价: 谓词锁 + 更多 abort]
    D --> D3[需要 SIREAD Locks 内存]
```

## 快照获取时机

```mermaid
sequenceDiagram
    participant RC as Read Committed
    participant RR as Repeatable Read
    participant S as Serializable

    Note over RC: BEGIN
    RC->>RC: 语句 1 → 快照 S1
    RC->>RC: 语句 2 → 快照 S2 (刷新)
    RC->>RC: 语句 3 → 快照 S3 (刷新)
    Note over RC: 每个语句独立快照

    Note over RR: BEGIN
    RR->>RR: 第一个 SQL → 快照 S (固定)
    RR->>RR: 语句 2 → 快照 S (不变)
    RR->>RR: 语句 3 → 快照 S (不变)
    Note over RR: 事务级快照

    Note over S: BEGIN
    S->>S: 第一个 SQL → 快照 S (固定)
    S->>S: 语句 2 → 快照 S
    S->>S: 语句 3 → 快照 S + 冲突检测
    Note over S: 事务级快照 + SSI
```

## 隔离级别的配置与查看

```sql
-- 设置隔离级别
BEGIN ISOLATION LEVEL READ COMMITTED;
BEGIN ISOLATION LEVEL REPEATABLE READ;
BEGIN ISOLATION LEVEL SERIALIZABLE;

-- 查看当前隔离级别
SHOW transaction_isolation;

-- 查看死锁/abort 信息
SELECT * FROM pg_stat_database WHERE datname = 'mydb';
```

## 性能影响

```mermaid
graph LR
    A[隔离级别性能] --> B[RC: 基准线]
    A --> C[RR: 快照复用省开销]
    A --> D[SERIALIZABLE: 谓词锁]
    D --> E[SIREAD 锁内存]
    D --> F[更多 abort 重试]
    D --> G[谓词锁粒度提升→误判]

    B --> H[快照获取频繁]
    C --> I[写冲突多时 abort]
```

**生产环境建议**：

- 绝大多数应用使用默认 **Read Committed**
- 需要报表一致性时使用 **Repeatable Read**（快照隔离已经很强）
- 只有强一致性需求（金融、库存）才用 **Serializable**
- Serializable 下 abort 率高，应用需要设计重试逻辑

## 与 MySQL/InnoDB 对比

| 维度 | PostgreSQL | MySQL InnoDB |
|------|-----------|--------------|
| 默认隔离级别 | Read Committed | Repeatable Read |
| RR 实现 | 快照隔离（无幻读） | 快照隔离 + gap lock（防幻读） |
| Serializable | SSI（谓词锁） | `SELECT ... LOCK IN SHARE MODE` |
| 幻读保护 | RR 无幻读保护，SERIALIZABLE 有 | RR 有 gap lock 保护 |
| 写冲突 | abort | 等待锁释放 |
| gap lock | 无 | 有（行锁之间的间隙） |

## 要点总结

- PG 的 Read Committed 是默认级别，每语句独立快照 → **不可重复读**
- Repeatable Read 使用事务级快照 → **快照隔离**（可重复读，但可能幻读）
- Serializable 使用 **SSI 算法**，谓词锁 + 冲突图检测 → 完全可串行化
- SSI 需要额外内存存 SIREAD 锁，abort 率会上升
- 与 MySQL RR 相比，PG 没有 gap lock，写冲突靠 abort 而非阻塞

## 思考题

1. PG 的 Repeatable Read 为什么不防幻读？这和 MySQL RR 用 gap lock 防幻读的设计理念有何不同？
2. Serializable 隔离级别下 abort 率较高，实际业务场景中应该如何处理 abort 后的重试？
3. 为什么 PG 不把 Read Uncommitted 实现为真正的脏读？这个设计决策背后的权衡是什么？