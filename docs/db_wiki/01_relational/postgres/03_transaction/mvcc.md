# MVCC 多版本并发控制

## 学习目标

- 理解 PostgreSQL MVCC 的核心机制：Heap 多版本 + xmin/xmax + Snapshot
- 掌握快照隔离下的行可见性判断流程
- 熟悉 VACUUM / HOT 等清理与优化策略

## 核心概念

- **MVCC（Multi-Version Concurrency Control）**：每个写操作产生新版本，读操作基于 Snapshot 选择可见版本
- **xmin / xmax**：Tuple 头部的两个事务 ID
- **Snapshot**：事务开始时获取的一致性视图，含 xmin/xmax/active_xip 数组
- **Hint Bits**：Tuple 头部的标志位，缓存"已提交/未提交"判断
- **CLOG（Commit Log）**：所有事务提交状态的位图
- **VACUUM**：清理死元组、回收空间
- **HOT（Heap-Only Tuples）**：原地 UPDATE 优化，省去索引更新

## MVCC 的核心理念

PG 的 MVCC 不是"在数据上锁"，而是"每个 Tuple 都是一个版本"。

```mermaid
graph LR
    A[INSERT] --> V1[版本 1<br/>xmin=100, xmax=0]
    B[UPDATE] --> V2[版本 2<br/>xmin=200, xmax=0]
    V1 -.xmax=200.-> V2
    C[DELETE] --> X[旧版本 xmax=300<br/>标记删除]
    X -.-> R[VACUUM 回收]
```

**核心规则**：对于一个事务而言，每个 Tuple 的可见性独立判断。

## Tuple 头部关键字段

```mermaid
graph TD
    subgraph "HeapTupleHeaderData"
        X1[t_xmin<br/>4B 插入事务]
        X2[t_xmax<br/>4B 删除/锁定事务]
        X3[t_cid<br/>4B 命令ID]
        X4[t_xvac<br/>4B VACUUM FULL xact]
        X5[t_infomask2<br/>2B]
        X6[t_infomask<br/>2B 关键位]
        X7[t_hoff<br/>1B 头部偏移]
    end

    X1 --> X2
    X2 --> X3
    X3 --> X4
    X4 --> X5
    X5 --> X6
    X6 --> X7

    subgraph "t_infomask 关键位"
        F1[HEAP_XMIN_COMMITTED<br/>xmin 已提交]
        F2[HEAP_XMIN_INVALID<br/>xmin 取消]
        F3[HEAP_XMAX_COMMITTED<br/>xmax 已提交]
        F4[HEAP_XMAX_INVALID<br/>xmax 取消]
        F5[HEAP_UPDATED<br/>xmax 来自 UPDATE]
        F6[HEAP_HASVARWIDTH<br/>变长字段]
    end
```

**关键状态位**：

| 标志 | 含义 |
|------|------|
| `HEAP_XMIN_COMMITTED` | xmin 事务已提交 |
| `HEAP_XMIN_INVALID` | xmin 事务回滚 |
| `HEAP_XMAX_COMMITTED` | xmax 事务已提交（行已删除） |
| `HEAP_XMAX_INVALID` | xmax 事务回滚或为 0（行未删除） |
| `HEAP_UPDATED` | xmax 是另一个 UPDATE 操作 |
| `HEAP_MARKED_FOR_UPDATE` | SELECT FOR UPDATE 标记 |
| `HEAP_KEYS_UPDATED` | UPDATE 修改了索引列 |

## Snapshot 快照

每个事务在第一次执行 SQL 时（`GetSnapshotData`）创建一个 Snapshot：

```mermaid
graph LR
    subgraph "SnapshotData"
        S1[xmin: 32B<br/>下边界]
        S2[xmax: 32B<br/>上边界]
        S3[xcnt: int<br/>active_xip 长度]
        S4[active_xip: xid[]<br/>活跃事务]
        S5[subxcnt: int]
        S6[subxip: xid[]<br/>子事务]
        S7[suboverflowed: bool]
        S8[takenDuringRecovery: bool]
        S9[curcid: int<br/>命令ID]
    end

    S1 --> S2
    S2 --> S3
    S3 --> S4
    S4 --> S5
    S5 --> S6
    S6 --> S7
    S7 --> S8
    S8 --> S9
```

**Snapshot 含义**：

- `xmin` < `xact` < `xmax`：在这个范围内的事务
- `xact` < `xmin`：`xact` 在快照前已提交 → 可见
- `xact` >= `xmax`：`xact` 在快照后启动 → 不可见
- `xact` 在 `active_xip` 中：活跃 → 不可见

## 可见性判断流程

```mermaid
flowchart TD
    A[HeapTupleSatisfiesMVCC] --> B[xmin = 自己事务?]
    B -->|是| C[COMMITTED_BY_SELF<br/>可见]
    B -->|否| D[查 Hint Bits]

    D --> E{XMIN_COMMITTED?}
    E -->|是| F[xmax = 自己事务?]
    E -->|否| G[查 CLOG]

    G --> H{CLOG 说已提交?}
    H -->|是| I[设置 XMIN_COMMITTED hint]
    H -->|否| J[设置 XMIN_INVALID hint]

    F --> K{xmax == 0?}
    K -->|是| L[可见]
    K -->|否| M[查 XMAX 系列 hint / CLOG]

    M --> N{XMAX_COMMITTED?}
    N -->|是| O[不可见 被删除]
    N -->|否| P[XMAX_INVALID?]
    P -->|是| L
    P -->|否| Q[查 active_xip]
    Q -->|xmax 在 active| R[当前事务视角: 不可见]
    Q -->|xmax 不在 active| S[CLOG: 是否提交]
    S -->|已提交| O
    S -->|未提交| R
```

**简化版规则**：

```sql
visible(tuple) = 
    (xmin committed) AND
    (xmax == 0 OR xmax == self OR 
     (xmax != committed AND xmax not in active_xip))
```

## MVCC 写路径

### INSERT

```mermaid
sequenceDiagram
    participant T as 当前事务
    participant Heap as Heap Page
    participant WAL

    T->>T: 获得 XID (GetCurrentTransactionId)
    T->>Heap: heap_insert(tuple)
    T->>Heap: 设置 xmin = XID, xmax = 0
    T->>WAL: XLogInsert HEAP_INSERT
    T-->>Heap: pd_lsn = WAL position
```

### UPDATE

PG 的 UPDATE 是 "DELETE + INSERT" 的语义：

```mermaid
sequenceDiagram
    participant T as 当前事务
    participant Heap as Heap Page

    T->>Heap: heap_update(oldtup, newtup)
    T->>Heap: 旧 Tuple xmax = XID
    T->>Heap: 写入新 Tuple xmin = XID, xmax = 0
    T->>Heap: ctid 指向新位置
    T-->>Heap: 若修改索引列, 同步更新索引
```

如果**只更新非索引列**，PG 启用 **HOT**（Heap-Only Tuple）：

```mermaid
graph LR
    A[旧 Tuple<br/>xmax=200] -->|LP_REDIRECT| B[新 Tuple<br/>xmin=200]
    style A stroke:#f66
    style B stroke:#6f6
```

HOT 优化：省去索引更新、版本在同一页内、减小 WAL。

### DELETE

```mermaid
flowchart TD
    A[heap_delete] --> B[找到目标 Tuple]
    B --> C[设置 xmax = current XID]
    C --> D[t_infomask |= HEAP_MARKED_FOR_UPDATE]
    D --> E[XLogInsert HEAP_DELETE]
    E --> F[Tuple 物理保留<br/>等 VACUUM]
```

## Snapshot 类型

PG 有四种 Snapshot，分别用于不同场景：

```mermaid
graph LR
    A[Snapshot 类型] --> B[MVCCTimeStamp<br/>默认 SELECT]
    A --> C[SelfSnapshot<br/>自己的事务]
    A --> D[AnySnapshot<br/>任何版本]
    A --> E[CatalogSnapshot<br/>系统表]
    A --> F[HistoricMVCC<br/>历史快照/事务回滚]
    A --> G[NonMVCC<br/>特殊操作]

    B --> B1[READ COMMITTED 默认]
    C --> C1[自己的修改可见]
    D --> D1[VACUUM 等]
    E --> E1[pg_catalog 短事务快照]
    F --> F1[Flashback / 事务恢复]
    G --> G1[DML 内部]
```

## CLOG 与 Hint Bits

### CLOG（Commit Log）

每个事务提交/回滚状态用 2 bit 存储在 CLOG 中：

```mermaid
graph LR
    A[CLOG] --> B[0x00 = IN_PROGRESS<br/>正在运行]
    A --> C[0x01 = COMMITTED<br/>已提交]
    A --> D[0x10 = ABORTED<br/>已回滚]
    A --> E[0x11 = SUB_COMMITTED<br/>子事务提交]
```

CLOG 位置：`pg_xact/` 目录，文件名 8KB 一个，分页存储。

### Hint Bits

CLOG 查询是 IO 操作，PG 通过 Hint Bits 把结果缓存到 Tuple 头部：

```mermaid
flowchart TD
    A[首次访问 Tuple] --> B[查 CLOG]
    B --> C{状态?}
    C -->|COMMITTED| D[设置 HEAP_XMIN_COMMITTED]
    C -->|ABORTED| E[设置 HEAP_XMIN_INVALID]
    D --> F[下次访问直接读 hint]
    E --> F
```

Hint Bits 一旦设置，几乎不会失效，访问效率极高。

## VACUUM 机制

### 为什么需要 VACUUM

```mermaid
graph LR
    A[连续 UPDATE/DELETE] --> B[死元组累积]
    B --> C[表膨胀 Table Bloat]
    C --> D[顺序扫描慢]
    C --> E[索引变大]
    C --> F[查询性能下降]

    B --> G[VACUUM]
    G --> H[回收空间]
    G --> I[更新 FSM/VM]
    G --> J[冻结老事务]
```

### VACUUM 流程

```mermaid
flowchart TD
    A[VACUUM 启动] --> B[扫描所有页]
    B --> C[标记可回收的死元组]
    C --> D[释放 ItemId 槽位]
    D --> E[更新 FSM Free Space Map]
    E --> F[更新 VM Visibility Map]
    F --> G{需要 Freeze?}
    G -->|是| H[冻结 xmin < freeze_limit 的事务]
    G -->|否| I[结束]
    H --> I
```

**autovacuum** 守护进程会自动触发，参数：

- `autovacuum_vacuum_threshold`：默认 50
- `autovacuum_vacuum_scale_factor`：默认 0.2（表 20% 变化触发）
- `autovacuum_naptime`：默认 1 分钟检查间隔

### HOT（Heap-Only Tuple）

当 UPDATE 不修改索引列，新版本与旧版本在同一页面：

```mermaid
graph TD
    A[原 Tuple<br/>xmax=200<br/>ctid=block0/item1] -->|LP_REDIRECT| B[新 Tuple<br/>xmin=200<br/>ctid=block0/item2]
    style A stroke:#fa6
    style B stroke:#6fa
```

HOT 收益：
- 无索引更新（关键）
- 旧版本通过 ctid 链找到新版本
- VACUUM 时一次性回收

PG 10+ 的 **Pruning** 进一步优化：在 HOT Update 后页面接近满时主动回收死元组。

## MVCC 与锁的关系

MVCC 提供读不阻塞写、写不阻塞读；但 DDL 与特定操作仍需锁：

```mermaid
graph TD
    A[操作类型] --> B[纯 MVCC]
    A --> C[需要锁]

    B --> B1[SELECT 不阻塞 UPDATE]
    B --> B2[UPDATE 不阻塞 SELECT]

    C --> C1[DDL ACCESS EXCLUSIVE]
    C --> C2[VACUUM FULL 持锁]
    C --> C3[CREATE INDEX CONCURRENTLY]
    C --> C4[ALTER TABLE 重写]
```

## MVCC 的代价

| 收益 | 代价 |
|------|------|
| 读不阻塞写 | 写不删除旧版本 |
| UPDATE 是新版本 | 死元组膨胀 |
| 快照隔离简单 | 需要 VACUUM |
| 行级并发 | Tuple 比纯行大（23B header） |
| 高并发写 | Update 触发索引更新 |

## 与其他数据库的对比

| 维度 | PostgreSQL | MySQL InnoDB | Oracle |
|------|-----------|--------------|--------|
| 版本存储 | Heap Tuple | 聚簇索引 + undo | undo tablespace |
| 回滚段 | 无（用 VACUUM 清理） | undo log | undo segment |
| 快照粒度 | 语句级（默认） | 语句级（默认） | 语句级 |
| 旧版本清理 | VACUUM + autovacuum | purge 线程 | SMON 后台 |
| 可见性依据 | xmin/xmax + Snapshot | trx_id + read view | SCN + undo |
| 行 header | 23 字节 | 5 字节事务ID + 6B roll ptr | 大 |

## 要点总结

- PG 用 **Heap 多版本 + Snapshot 隔离** 实现 MVCC
- Tuple 头部携带 `xmin` / `xmax` / `t_infomask`，可见性靠 Hint Bits + CLOG 缓存
- **UPDATE = DELETE + INSERT**，老版本由 VACUUM 异步清理
- HOT 优化省去索引更新，Pruning 让页内回收更轻量
- autovacuum 是 PG 健康运行的关键，调优 `autovacuum_*` 参数至关重要

## 思考题

1. 为什么 PG 没有像 InnoDB 那样用 undo log 来回滚，而是直接保留多版本？两种设计在哪些场景下差异最大？
2. 一个超长事务（如 pg_dump）长时间持有 Snapshot，会对 VACUUM 产生什么影响？
3. 如果表上频繁做 UPDATE，但没有 VACUUMM 介入，会出现什么状况？autovacuum 调优的常见手段有哪些？