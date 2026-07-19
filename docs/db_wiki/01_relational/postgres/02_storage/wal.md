# WAL 预写日志

## 学习目标

- 理解 PostgreSQL WAL 的写入协议与持久化机制
- 掌握 LSN、Checkpoint、Redo 恢复的基本原理
- 熟悉 WAL Records、Full Page Writes、Replication 的关系

## 核心概念

- **WAL（Write-Ahead Logging）**：写前日志，所有数据页修改前必须先写日志
- **LSN（Log Sequence Number）**：WAL 中的逻辑位置，64 位单调递增
- **WAL Record**：单条日志项，包含 xl_prev、xl_xid、xl_info、xl_len 等头部
- **XLogInsert**：向 WAL Buffer 写入记录的接口
- **Full Page Writes (FPW)**：checkpoint 后第一次修改页时写入完整页面镜像
- **Checkpoint**：周期性触发，把所有脏页落盘，截断旧的 WAL
- **pg_wal/**：WAL 文件目录，文件名 16 进制 LSN，每 16MB 一个 segment
- **walwriter / checkpointer**：负责刷盘的辅助进程

## 写入路径

PG 的 WAL 协议核心规则：**在修改数据页之前，先把 WAL 记录刷盘**。

```mermaid
sequenceDiagram
    participant B as Backend
    participant WB as WAL Buffers
    participant W as walwriter
    participant D as pg_wal/
    participant P as 数据页 in shared_buffers

    B->>B: heap_insert(...)
    B->>WB: XLogInsert(HEAP_INSERT)
    WB->>WB: 追加到 WAL Insert Slot
    WB-->>B: 返回当前 LSN (InsertRecPtr)

    B->>P: 修改数据页<br/>pd_lsn = InsertRecPtr
    B->>B: MarkBufferDirty

    Note over B,WB: COMMIT 时
    B->>WB: XLogInsert(XLOG_COMMIT)
    WB->>WB: 锁定 commit WAL 位置
    B->>W: 等待 flush 直至 LSN >= commit LSN
    W->>D: WAL 记录落盘
    W-->>B: flush 完成
    B-->>B: COMMIT 成功
```

## WAL 记录结构

每条 WAL Record 由头部 + 数据组成：

```mermaid
graph LR
    subgraph "XLogRecord 24 字节"
        X1[xl_tot_len<br/>4B 整条记录长度]
        X2[xl_xid<br/>4B 事务ID]
        X3[xl_prev<br/>8B 前一条 LSN]
        X4[xl_info<br/>1B 信息标志]
        X5[xl_rmid<br/>1B Resource Manager ID]
        X6[xl_crc<br/>4B CRC-32]
    end

    X1 --> X2
    X2 --> X3
    X3 --> X4
    X4 --> X5
    X5 --> X6

    X6 --> DATA[块数据<br/>block reference + data]
    DATA --> BK[BkpBlock<br/>可选备份块]
    DATA --> MAIN[主数据 payload]
```

**Resource Manager ID（rmid）** 区分日志来源：

- `RM_HEAP`：Heap AM 操作（INSERT/UPDATE/DELETE）
- `RM_BTREE`：BTree 索引
- `RM_XLOG`：WAL 内部操作（CHECKPOINT_SHUTDOWN、LOGICAL_MESSAGE 等）
- `RM_XACT`：事务 COMMIT/ABORT
- `RM_STANDBY`：备库相关
- `RM_REPLORIGIN`：复制起点

## LSN 与 pg_wal

LSN 是 64 位逻辑位置，高 32 位是文件号，低 32 位是文件内偏移：

```mermaid
graph LR
    A[LSN: 0/01600000] --> B[文件 0x00<br/>大小 16MB]
    A --> C[偏移 0x01600000]

    D[LSN: 1/00000000] --> E[文件 0x01]
    D --> F[偏移 0]

    G[LSN: 0/01A00000] --> H[文件 0x00]
    G --> I[偏移 0x01A00000]
```

**pg_wal 文件命名**：

- 文件名 24 字符 16 进制（如 `000000010000000000000001`）
- 默认大小 16MB（`--wal-segsize` 编译期配置）
- 历史文件由 checkpoint 触发回收

## 关键流程

### WAL Insert

`XLogInsert` 是写入入口，内部步骤：

```mermaid
flowchart TD
    A[XLogInsert rmid, info, data] --> B[获取 WAL Insert Lock<br/>LWLock]
    B --> C[在 WAL Insert Slot<br/>复制 record 数据]
    C --> D[计算 CRC]
    D --> E[拷贝到 WAL Buffers<br/>组装 xl_prev 链]
    E --> F[释放 WAL Insert Lock]
    F --> G[返回 LSN 位置]
```

**WAL Insert Lock** 是 WAL 写入的关键临界区，PG 13+ 用 `WALInsertLockUpdateContributors` 等机制优化。

### WAL Flush

`XLogFlush(LSN)` 确保指定 LSN 之前的所有 WAL 都落盘：

```mermaid
flowchart TD
    A[XLogFlush targetLSN] --> B{已 flush LSN >= target?}
    B -->|是| C[立即返回]
    B -->|否| D[获取 WAL Write Lock]
    D --> E[计算写入段文件]
    E --> F[issue_xlog_fsync<br/>调 fsync]
    F --> G[更新 LogwrtRqst.Write]
    G --> H[释放 WAL Write Lock]
```

### Checkpoint

Checkpoint 是"一致性快照"事件，由 checkpointer 进程触发：

```mermaid
flowchart TD
    A[Checkpointer 定时触发] --> B[计算 Redo RecPtr<br/>= 当前 Insert LSN]
    B --> C[遍历所有 dirty buffer<br/>同步刷盘]
    C --> D[落盘完成]
    D --> E[写 CHECKPOINT WAL record<br/>含 Redo RecPtr]
    E --> F[更新 Control File<br/>记录最新 checkpoint]
    F --> G[回收不需要的 WAL 段]
```

**触发条件**：

- `checkpoint_timeout`（默认 5 分钟）
- `max_wal_size`（默认 1GB）
- 手动 `CHECKPOINT` 命令
- 关闭时 `smart` 或 `fast` 模式

**Full Page Writes**：checkpoint 之后**每个页面的第一次修改**会把完整页面镜像写入 WAL：

```mermaid
graph TD
    A[Page 修改] --> B{这是 checkpoint 后<br/>首次修改?}
    B -->|是| C[写 FPW: 完整 8KB 页面]
    B -->|否| D[写增量 WAL Record]
    C --> E[Record 长度较大]
    D --> F[Record 长度较小]
```

`wal_compression = on` 可以压缩 FPW，节省空间但消耗 CPU。

## 恢复（Recovery）

启动时如果发现 `pg_control` 与 `pg_wal/` 不一致，进入恢复模式：

```mermaid
flowchart TD
    A[PostgreSQL 启动] --> B{上次是否干净关闭?}
    B -->|否| C[进入 crash recovery]
    B -->|是| D[正常启动]

    C --> E[读取 Control File<br/>获取 checkpoint LSN]
    E --> F[从 Redo RecPtr 开始<br/>扫描 WAL]
    F --> G{Record 类型?}
    G -->|XLOG_HEAP_INSERT| H[重做 Heap Insert]
    G -->|XLOG_BTREE_*| I[重做 BTree 操作]
    G -->|XLOG_XACT_COMMIT| J[标记事务已提交]
    G -->|XLOG_XACT_ABORT| K[标记事务已回滚]

    H --> L[更新页 LSN]
    I --> L
    J --> L
    K --> L

    L --> M{到达最新 checkpoint?}
    M -->|否| G
    M -->|是| N[恢复到一致状态<br/>进入正常启动]
```

**ARIES 风格**：PG 用 `xl_prev` 链表 + Record-level redo，崩溃后只需从最近 checkpoint 重做。

## 复制（WAL Streaming）

WAL 是物理复制的"事实来源"：

```mermaid
graph LR
    subgraph "主库"
        W1[pg_wal/]
        W1 -->|wal_level=replica| W2[后端 walsender]
    end

    W2 -->|TCP 流式 WAL| S1[备库 walreceiver]

    subgraph "备库"
        S1 --> S2[pg_wal/]
        S2 --> S3[Startup 进程]
        S3 --> S4[Replay WAL<br/>更新数据页]
    end
```

**关键参数**：

- `wal_level`：`minimal` / `replica` / `logical`
- `max_wal_senders`：流复制连接数
- `wal_keep_size` / `max_slot_wal_keep_size`：保留 WAL
- `synchronous_commit`：是否同步复制

## WAL 配置参数

| 参数 | 默认值 | 推荐 | 说明 |
|------|--------|------|------|
| `wal_level` | replica | replica | 复制级别 |
| `wal_buffers` | 16MB | 16MB-64MB | WAL 缓冲区 |
| `wal_compression` | off | on (压缩盘) | FPW 压缩 |
| `checkpoint_timeout` | 5min | 15-30min | Checkpoint 间隔 |
| `checkpoint_completion_target` | 0.9 | 0.9 | 写盘目标时间比例 |
| `max_wal_size` | 1GB | 4GB-16GB | 触发 checkpoint 的 WAL 量 |
| `min_wal_size` | 80MB | 1GB-2GB | WAL 段文件保留下限 |
| `fsync` | on | on | 必须开启！ |
| `synchronous_commit` | on | on (强持久化) | 提交时刷盘 |
| `full_page_writes` | on | on | FPW 开启 |

## LSN 监控

```sql
-- 当前 Insert 位置
SELECT pg_current_xlog_insert_location(); -- PG 12 之前
SELECT pg_current_wal_insert_lsn();       -- PG 13+

-- 当前 Flush 位置
SELECT pg_current_wal_lsn();

-- WAL 状态视图
SELECT * FROM pg_stat_wal;

-- WAL 文件列表
SELECT * FROM pg_ls_waldir() LIMIT 20;
```

## WAL 与其他组件的协同

```mermaid
graph TD
    WAL[WAL Records]
    WAL --> R1[Redo 恢复]
    WAL --> R2[流复制]
    WAL --> R3[Logical Decoding]
    WAL --> R4[Archive 归档]
    WAL --> R5[Logical Replication<br/>test_decoding]

    R1 --> C[Crash Recovery]
    R2 --> SR[Standby Replay]
    R3 --> LR[CDC / pgoutput]
    R4 --> BA[Base Backup<br/>pg_basebackup]
```

## 要点总结

- WAL 是 PG 持久化的核心：**数据页修改前必须先写日志**
- LSN 是 64 位逻辑位置，由 WAL Insert / Write / Flush 三个指针推进
- Checkpoint + Full Page Writes 保证崩溃后可恢复
- WAL 同时支撑流复制、归档、逻辑解码
- `synchronous_commit = on` 与 `fsync = on` 是持久性的最低保障

## 思考题

1. 为什么 checkpoint 后第一次页面修改必须写入完整页面（FPW）？如果不写会怎样？
2. `synchronous_commit = on/off` 的取舍是什么？在哪些场景下值得关闭？
3. WAL 段文件过大（16MB）和过小（1MB）各有什么影响？