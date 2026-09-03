# Linux RAID 阵列学习笔记

本文档记录 RAID 阵列冗余设计在项目工程代码中的实际应用对照。

## 工程对照

### 存储冗余设计与数据库高可用/复制架构对照

RAID 在磁盘层面通过镜像、奇偶校验等技术实现数据冗余和故障恢复，数据库系统则在更高层面（节点间）通过复制、分片、共识协议实现高可用。两者解决的是同一类问题——"单点故障时如何保证数据不丢失、服务不中断"，只是作用层次不同。

### 1. RAID 1 镜像与数据库主从复制

RAID 1 将数据完整写入两块磁盘，数据库的主从复制（或同步复制）将数据复制到多个节点：

```c
// c:\code\book\engineering\src\db\storage\wal\wal.c
// WAL 日志的刷新和复制机制
// 类比 RAID 1: 镜像写入 → 保证数据在多个位置一致

// RAID 1: 写数据 → 盘 0 + 盘 1 (同步镜像写)
// DB:    写 WAL → 本地 + 远程副本 (同步/异步复制)
int wal_flush(wal_t *wal) {
    // 1. 写入本地 WAL 文件
    if (write(wal->fd, wal->buf, wal->offset) < 0) return -1;
    // 2. fsync 确保落盘 (类比 RAID 1 的镜像写确认)
    if (fsync(wal->fd) < 0) return -1;
    // 3. (未来) 发送到远程副本
    wal->offset = 0;
    return 0;
}
```

**RAID 与数据库复制的层次对照：**

| 层次 | RAID 实现 | 数据库实现 |
|------|----------|-----------|
| 存储层 | RAID 1 镜像 / RAID 5 校验 | 多副本存储 (3 副本) |
| 日志层 | 磁盘写缓存 + 电池备份 | WAL 日志复制 (同步/异步) |
| 事务层 | 原子写 (sector atomic) | 分布式事务 (2PC / Raft) |
| 恢复层 | 热备盘 + 重建 | 从副本恢复 + 检查点回放 |

### 2. RAID 5 奇偶校验与纠删码

RAID 5 使用 XOR 奇偶校验实现单盘容错，分布式存储使用纠删码（Erasure Coding）实现更高效的冗余：

```c
// 工程中的 WAL 缓冲区管理
// c:\code\book\engineering\src\db\storage\wal\wal_buf.c
// WAL 记录保证写入顺序和完整性
// 类比 RAID 5: 校验块保证数据完整性

// RAID 5:  D0 XOR D1 = P  → 任意一块丢失可恢复
// WAL:     Redo 日志     → 任意时刻崩溃可恢复
```

### 3. 条带化与数据分片

RAID 0 将数据条带化到多块磁盘以提升性能，数据库将数据分片到多个节点以实现水平扩展：

```c
// c:\code\book\engineering\src\db\storage\kv\kv_engine.c
// KV 存储引擎的数据分布策略
// 类比 RAID 0 条带化: 数据分散到多个存储单元

// RAID 0:  条带 0→盘0, 条带 1→盘1, 条带 2→盘0, ...
// 分片:    hash(key) % N → 确定目标分片节点
```

### 4. 磁盘重建与节点恢复

RAID 的磁盘重建过程（从校验信息恢复数据）与数据库的节点恢复（从 WAL/副本恢复数据）有相同的设计目标：

```c
// c:\code\book\engineering\src\db\storage\buffer\bufmgr.c
// Buffer Pool 的页面读取流程：缓存未命中时从磁盘读取
// 类比 RAID 重建: 从冗余信息中恢复丢失的数据块

// RAID 重建: 丢失扇区 → XOR 其他扇区 + 校验块 → 恢复
// DB 恢复:   丢失页面 → WAL Redo / 从副本拉取  → 恢复
```

### 5. 数据完整性保护

RAID 通过校验和（checksum）检测静默数据损坏，数据库同样需要页面级校验：

```c
// c:\code\book\engineering\src\db\storage\integrity\data_integrity.c
// 数据完整性校验模块
// 类比 RAID 的校验和: 检测并修复静默数据损坏

// 页面校验流程:
//   1. 读取页面数据 + 校验和
//   2. 重新计算校验和
//   3. 不匹配 → 从副本恢复（类似 RAID 重建）
```

### 6. 关键设计原则对照

| 设计原则 | RAID 体现 | 数据库体现 |
|----------|----------|-----------|
| 冗余 | 镜像 / 奇偶校验 | 多副本 / 纠删码 |
| 故障检测 | SMART + 校验和 | 心跳 + 租约超时 |
| 自动恢复 | 热备盘 + 重建 | 自动故障转移 (Failover) |
| 性能优化 | 条带化 + 缓存 | 分片 + Buffer Pool |
| 一致性保证 | 原子扇区写入 | MVCC + 分布式共识 |
| 容量效率 | RAID5: (N-1)/N | 3副本: 1/3 (或 EC 更高) |

RAID 的设计哲学是"用冗余换可靠性"，这与数据库高可用架构完全一致。理解 RAID 的条带化、镜像、奇偶校验和重建机制，有助于深入理解分布式数据库中的数据分布、副本管理和故障恢复设计。工程中的 Buffer Pool 脏页回写策略（Clock-Sweep）和 WAL 检查点机制都可以看作是 RAID 回写策略（write-back vs write-through）在更高抽象层次上的复现。

## 参考源码

- `c:\code\book\engineering\src\db\storage\buffer\bufmgr.c` — Buffer Pool 缓存管理
- `c:\code\book\engineering\src\db\storage\wal\wal.c` — WAL 日志
- `c:\code\book\engineering\src\db\storage\wal\wal_buf.c` — WAL 缓冲区管理
- `c:\code\book\engineering\src\db\storage\kv\kv_engine.c` — KV 存储引擎
- `c:\code\book\engineering\src\db\storage\integrity\data_integrity.c` — 数据完整性校验
- `c:\code\book\engineering\src\db\storage\txn\txn.c` — 事务管理
