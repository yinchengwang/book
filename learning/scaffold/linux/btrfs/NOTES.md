# Btrfs 文件系统学习笔记

本文档记录 Btrfs 文件系统特性在项目工程代码中的实际应用对照。

## 工程对照

### 1. Btrfs COW 与数据库中的 COW/MVCC

Btrfs 的写时复制（COW）机制与数据库的 MVCC（多版本并发控制）在思想上同源，但实现层次不同。工程中事务系统的实现位于 `c:\code\book\engineering\src\db\storage\wal\wal.c` 和 `c:\code\book\engineering\include\db\txn.h`。

**COW 对照表：**

| 概念 | Btrfs COW | 数据库 MVCC / 快照隔离 |
|------|----------|----------------------|
| 基本思想 | 修改时复制，旧版本保留 | 更新创建新版本，旧版本保留 |
| 存储单元 | Extent（128KB 连续块） | 元组（Tuple）或页面（8KB） |
| 版本管理 | B-Tree 节点 refcount | 事务 ID (XID) + 可见性规则 |
| 旧版本回收 | 引用计数为 0 时回收 extent | VACUUM 清理死元组 |
| 快照 | 子卷快照（只复制根指针） | 基于事务 ID 的一致性读 |

**数据库中的"快照"：**

```c
// txn.h (c:\code\book\engineering\include\db\txn.h)
// 数据库事务快照隔离的核心思想：
// - 每个事务看到的是事务开始时的一致快照
// - 通过事务 ID (XID) 判断可见性
// - 类比 Btrfs 的子卷快照机制

typedef enum txn_state_e {
    TXN_STATE_ACTIVE = 0,     // 活动：正在修改，其他事务不可见
    TXN_STATE_COMMITTED = 1,  // 已提交：修改对其他事务可见
    TXN_STATE_ABORTED = 2,    // 已回滚：修改全部撤销
} txn_state_t;
```

### 2. Btrfs B-Tree 与数据库 BTree 索引

Btrfs 使用 B-Tree 组织所有元数据，工程中也有 BTree 索引实现：

```c
// btreeam.h (c:\code\book\engineering\include\db\btreeam.h)
// 数据库 BTree 索引 vs Btrfs B-Tree：

// 相同点：
// 1. 都是平衡树结构，保证 O(log n) 查找
// 2. 都支持范围扫描
// 3. 都支持键值对存储

// 不同点：
// 1. Btrfs B-Tree 使用 COW 更新（永不原地修改）
// 2. 数据库 BTree 可以原地更新 + WAL 保护
// 3. Btrfs B-Tree 存储的是 extent 引用
// 4. 数据库 BTree 存储的是行指针 (CTID)
```

### 3. 快照隔离（Snapshot Isolation）对照

Btrfs 的文件系统快照与数据库的快照隔离在语义上有一一对应关系：

| Btrfs 快照 | 数据库快照隔离 |
|-----------|-------------|
| `btrfs subvolume snapshot -r` | `BEGIN TRANSACTION ISOLATION LEVEL SNAPSHOT` |
| 创建时零拷贝（仅复制根指针） | 事务开始时获取快照 LSN |
| 读快照 = 读创建时的文件系统状态 | 读快照 = 读事务开始时的数据库状态 |
| 写原卷触发 COW → 快照不受影响 | 写原元组创建新版本 → 快照事务看到旧版本 |
| 快照可克隆为可写 | 可串行化快照隔离 (SSI) |

### 4. 透明压缩与数据库压缩

Btrfs 的文件系统级透明压缩在数据库中有多层对应：

| 层级 | Btrfs | 数据库工程 |
|------|-------|-----------|
| 存储层 | extent 写入前压缩 | 页面级压缩（可扩展） |
| 索引层 | 无（压缩在块设备层） | 前缀压缩 / 字典编码 |
| 传输层 | `btrfs send` 可压缩 | 复制协议压缩 |
| 算法 | zlib / lzo / zstd | 同（可选用相同算法族） |

### 5. Btrfs RAID 与数据库高可用

Btrfs 的 RAID 能力提供存储层冗余，数据库还需应用层高可用：

```c
// 工程高可用体系（phase9-distributed-evolution.md）：
// 存储层：Btrfs RAID1 → 磁盘故障自动恢复
// 数据层：WAL 日志 → 崩溃恢复
// 共识层：Raft 共识 → 多节点故障切换
// 协调层：Coordinator → 分布式事务协调

// Btrfs RAID 解决的是"盘坏了"的问题
// Raft 解决的是"节点宕了"的问题
```

### 6. Btrfs 与数据库的互补使用场景

在实际生产中，Btrfs 和数据库可以互补：

1. **Btrfs 快照用于数据库备份**：
   - 先 `txn_begin` + 获取一致性点
   - 创建 Btrfs 快照（秒级）
   - 快照可挂载、可发送到远程

2. **Btrfs 压缩减少数据库存储**：
   - 挂载时启用 `compress=zstd`
   - 对只读/读多写少的数据库表效果显著

3. **Btrfs send/receive 用于异地灾备**：
   - 增量发送数据库文件到远程
   - 比 rsync 更高效（块级增量）

## 参考源码

- `c:\code\book\engineering\include\db\txn.h` — 事务管理器（MVCC 核心接口）
- `c:\code\book\engineering\src\db\storage\wal\wal.c` — WAL 日志（崩溃恢复）
- `c:\code\book\engineering\include\db\btreeam.h` — BTree 索引访问方法
- `c:\code\book\engineering\include\db\buf.h` — Buffer Pool（页面级缓存）
- `c:\code\book\engineering\src\db\storage\buffer\bufmgr.c` — Buffer Pool 实现
- `c:\code\book\engineering\include\db\disk.h` — 底层磁盘 I/O 接口
