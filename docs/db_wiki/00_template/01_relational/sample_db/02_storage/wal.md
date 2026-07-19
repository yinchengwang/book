# WAL 日志

## 学习目标
- 理解 WAL（Write-Ahead Logging）的作用和原理
- 掌握 WAL 的写入流程和恢复机制

## 核心概念

- **WAL**：写前日志，先写日志再写数据，保证持久性
- **LSN**：日志序列号，单调递增
- **Checkpoint**：检查点，标记哪些日志已刷盘

## WAL 架构

```mermaid
graph TD
    subgraph "WAL 系统"
        WALBuf[WAL Buffer]
        WALFile[WAL 文件]
        Checkpoint[检查点]
    end

    TXN[事务提交] --> WALBuf
    WALBuf -->|刷盘| WALFile
    WALFile --> Checkpoint
    Checkpoint --> DataFile[数据文件]
```

## 写入流程

```mermaid
sequenceDiagram
    participant T as 事务
    participant B as WAL Buffer
    participant L as WAL 日志
    participant D as 数据页

    T->>B: 写入日志记录
    T->>B: 提交请求
    B->>L: 刷盘 (fsync)
    L-->>T: 确认
    Note over D: 后台异步刷数据页
```

## 恢复流程

```mermaid
flowchart TD
    A[数据库启动] --> B[读取最新 Checkpoint]
    B --> C[从 Checkpoint LSN 开始回放]
    C --> D{还有日志?}
    D -->|是| E[应用日志记录]
    E --> D
    D -->|否| F[恢复完成]
```

## 要点总结

- WAL 保证事务持久性，先日志后数据
- Checkpoint 减少恢复时需要回放的日志量

## 思考题

1. 为什么 WAL 刷盘必须在数据页刷盘之前？
2. 检查点频率如何影响性能和恢复时间？