# etcd WAL 与快照

## 学习目标

- 理解 etcd 的 WAL 写前日志
- 掌握快照机制和 WAL 裁剪

## WAL 架构

```mermaid
graph TB
    A[etcd WAL] --> B[WAL 文件序列]
    A --> C[日志条目]
    A --> D[快照 Snapshot]
    A --> E[WAL 裁剪]

    B --> B1[0-0000000000.wal]
    B --> B2[0-0000000100.wal]
    B --> B3[定期切割]

    C --> C1[Raft 日志条目]
    C --> C2[日志类型: Normal/ConfChange]
    C --> C3[CRC 校验]

    D --> D1[全量快照]
    D --> D2[增量快照]
    D --> D3[Snap 文件]

    E --> E1[Snapshot 后裁剪]
    E --> E2[保留最近 N 个文件]
```

## WAL 文件结构

```go
// wal/wal.go

type WAL struct {
    // 日志文件
    dir string
    // 当前文件
    f *os.File
    // 编码器
    encoder *encoder
    // 解码器
    decoder *decoder

    // 元数据
    metadata []byte

    // 日志状态
    state raftpb.HardState
    // 快照
    snapshot *raftpb.Snapshot

    // CRC 校验
    mu sync.Mutex
    // 当前 CRC
    curCrc uint32
    // 上次 CRC
    lastCrc uint32
}
```

## 写入流程

```go
// WAL 写入
// 1. 序列化 Raft 日志条目
// 2. 计算 CRC 校验
// 3. 追加到当前 WAL 文件
// 4. 调用 fsync 确保写入磁盘
// 5. 文件超过阈值（64MB）时切割

// 保存
func (w *WAL) Save(st raftpb.HardState, ents []raftpb.Entry) error {
    // 1. 写入日志条目
    // 2. 写入 HardState
    // 3. 更新 CRC
    // 4. 调用 Sync()
    return nil
}

// 切割条件
// 文件大小 > 64MB
// 文件序号递增
// 从前一个文件中断处开始
```

## 快照机制

```go
// 快照创建
// 当 Raft 日志条数 > 10000 时触发
// 或者手动触发

// 快照内容
// 1. 当前所有键值对
// 2. 最后一个包含的索引
// 3. 元数据（集群 ID、成员 ID）

// 快照加载
// 1. 检查快照文件
// 2. 加载到内存
// 3. 设置 Raft 日志状态
// 4. 裁剪 WAL 文件
```

## 故障恢复流程

```mermaid
sequenceDiagram
    participant N as 节点重启
    participant W as WAL
    participant S as Snapshot
    participant R as Raft

    N->>W: 扫描 WAL 文件
    W->>W: 读取所有日志条目
    N->>S: 检查最新快照
    S->>N: 返回快照数据
    N->>R: 加载快照到内存
    R->>R: 回放 WAL 日志
    R->>R: 恢复状态机
    N->>W: 裁剪旧 WAL 文件
    N->>N: 启动完成
```

## 要点总结

- WAL 是 Raft 日志的持久化载体
- 每个日志条目有 CRC 校验
- 快照触发后裁剪旧 WAL 文件
- 故障恢复时先加载快照再回放 WAL

## 思考题

1. WAL 中为什么需要保存 HardState？包含哪些信息？
2. 快照和 WAL 裁剪的配合策略是什么？
3. 如果 WAL 文件损坏，如何恢复？