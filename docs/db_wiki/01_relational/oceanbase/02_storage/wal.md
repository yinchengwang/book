# OceanBase 存储引擎 — WAL（写前日志）

## 学习目标

- 掌握 OceanBase 的 WAL 机制
- 理解 OceanBase 的双层日志设计
- 对比 OceanBase 与 TiDB、CockroachDB 的 WAL 差异

## WAL 架构

```mermaid
graph TB
    A[事务提交] --> B[Redo Log<br/>重做日志]
    B --> C[MemTable 写入]

    D[日志流转] --> E[MemTable Log<br/>内存日志]
    E --> F[Redo Log File<br/>磁盘日志]
    F --> G[归档日志<br/>备份]

    H[崩溃恢复] --> I[读取 Redo Log]
    I --> J[回放日志]
    J --> K[恢复 MemTable]
```

## Redo Log 结构

```mermaid
graph TB
    A[Redo Log 文件] --> B[Log Block 1<br/>32KB]
    A --> C[Log Block 2]
    A --> D[Log Block 3]
    A --> E[...]

    B --> F[Header<br/>LSN + Checksum]
    B --> G[Log Entries<br/>日志条目]

    G --> H[事务 ID]
    G --> I[操作类型]
    G --> J[Key + Value]
```

## 两阶段提交

```mermaid
sequenceDiagram
    participant Client AS 客户端
    participant Coord AS 协调者
    participant Part1 AS 分区 1
    participant Part2 AS 分区 2

    Client->>Coord: BEGIN
    Client->>Coord: INSERT INTO t1 VALUES (1)
    Client->>Coord: INSERT INTO t2 VALUES (2)
    Client->>Coord: COMMIT

    Coord->>Part1: PREPARE（上锁 + 写日志）
    Coord->>Part2: PREPARE（上锁 + 写日志）
    Part1-->>Coord: OK
    Part2-->>Coord: OK

    Coord->>Part1: COMMIT（提交 + 删除锁）
    Coord->>Part2: COMMIT（提交 + 删除锁）
    Part1-->>Coord: OK
    Part2-->>Coord: OK

    Coord-->>Client: COMMIT 成功
```

## 崩溃恢复

```mermaid
sequenceDiagram
    participant Startup AS 启动进程
    participant Log AS Redo Log
    participant Mem AS MemTable
    participant SST AS SSTable

    Startup->>Log: 读取最后检查点
    Startup->>Log: 扫描日志条目
    Startup->>Mem: 回放日志（重做）
    Startup->>SST: 恢复完成
```

## 与 TiDB WAL 对比

| 维度 | OceanBase | TiDB |
|------|-----------|------|
| 日志类型 | Redo Log | Raft Log + RocksDB WAL |
| 日志协议 | 自研 | Raft 协议 |
| 两阶段提交 | 支持 | Percolator 两阶段 |
| 崩溃恢复 | 重做日志回放 | Raft Log 回放 |
| 日志归档 | 支持 | 不支持 |

## 与 CockroachDB WAL 对比

| 维度 | OceanBase | CockroachDB |
|------|-----------|------------|
| 日志类型 | Redo Log | Raft Log + RocksDB WAL |
| 日志协议 | 自研 | Raft 协议 |
| 两阶段提交 | 支持 | Write Intent 两阶段 |
| 崩溃恢复 | 重做日志回放 | Raft Log 回放 |

## 与 PostgreSQL WAL 对比

| 维度 | OceanBase | PostgreSQL |
|------|-----------|------------|
| 日志类型 | Redo Log | WAL（Write-Ahead Log） |
| 日志结构 | Log Block | WAL Segment |
| 两阶段提交 | 支持 | 支持（PREPARE TRANSACTION） |
| 崩溃恢复 | 重做日志回放 | WAL 回放 |
| 归档日志 | 支持 | 支持（archive_mode） |

## 要点总结

- OceanBase 使用 Redo Log 作为写前日志
- 日志结构：Log Block（32KB）+ Log Entries
- 支持两阶段提交和崩溃恢复
- 与 TiDB/CockroachDB 相比：Redo Log vs Raft Log
- 与 PostgreSQL 相比：自研 Redo Log vs WAL

## 思考题

1. OceanBase 的 Redo Log 与 Raft Log 相比，在日志复制和故障恢复上有何差异？
2. OceanBase 的两阶段提交如何保证跨分区事务的原子性？
3. OceanBase 的崩溃恢复过程中，如何处理未完成的事务？