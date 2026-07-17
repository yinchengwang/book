# MVCC 并发控制

## Why

当前数据库使用排他锁实现事务，读取和写入会相互阻塞，无法支持真正的并发读写。MVCC（Multi-Version Concurrency Control）通过维护数据的多个版本，让读写操作互不阻塞，显著提升并发性能。这是构建高性能数据库的必备能力。

## What Changes

### 版本存储
- 行级版本链：每个数据行维护一个版本链表
- 版本结构：包含事务 ID、时间戳、指向前后版本的指针
- Undo 日志：保存旧版本数据，支持回滚和版本构建

### 快照管理
- 事务快照：记录活跃事务列表和最小未分配事务 ID
- 快照读：基于快照判断可见性，返回匹配版本
- 当前读：读取最新版本数据（加锁）

### 读一致性
- SI（Snapshot Isolation）隔离级别
- 写倾斜检测：检测并发事务间的写冲突
- 首次更新 wins 策略

### 垃圾回收
- VACUUM 机制：清理过期版本
- 版本链修剪：移除对活跃事务不可见的旧版本
- 增量清理：避免全量扫描

## Capabilities

### New Capabilities

- `mvcc-version-chain`: 行级版本链数据结构，支持插入/更新/删除时创建新版本
- `mvcc-snapshot`: 事务快照管理，追踪活跃事务并判断版本可见性
- `mvcc-visibility`: 可见性判断逻辑，根据快照确定返回哪个版本
- `mvcc-garbage-collection`: VACUUM 垃圾回收，清理过期版本释放空间

### Modified Capabilities

- `transaction-wal`: WAL 需要记录版本链指针，支持 MVCC 下的回滚和恢复

## Impact

### 新增文件

```
src/db/concurrency/
├── mvcc_version.c/h      # 版本链管理
├── mvcc_snapshot.c/h     # 快照管理
├── mvcc_visibility.c/h   # 可见性判断
└── mvcc_gc.c/h           # 垃圾回收

include/db/concurrency/
└── mvcc.h                # 公共接口

test/db/
└── test_mvcc.cpp         # MVCC 测试
```

### 修改文件

- `src/db/storage/txn.c` - 集成 MVCC 版本链
- `src/db/storage/table.c` - 行存储支持版本指针
- `src/db/storage/wal.c` - WAL 支持 MVCC 恢复
- `include/db/txn.h` - 事务结构增加 MVCC 字段