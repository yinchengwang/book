# 工程对照：MVCC 快照与线程模式

## 概述

本笔记对照学习材料中的 `std::thread` 概念与 `engineering/src/db/concurrency/mvcc_snapshot.c` 中 MVCC 并发控制涉及的线程模式。

## MVCC 快照结构回顾

```c
// engineering/src/db/concurrency/mvcc_snapshot.h (推断)
typedef struct {
    mvcc_txn_id_t xmin;       // 快照开始时的事务 ID 下界
    mvcc_txn_id_t xmax;      // 快照开始时最大活跃事务 ID
    mvcc_txn_id_t *xip_list; // 快照开始时活跃事务 ID 列表
    int xip_count;           // 活跃事务数量
} mvcc_snapshot_t;
```

### 快照创建中的线程安全考量

在 `mvcc_snapshot_create()` 中（第 15-46 行），快照创建涉及：
1. **内存分配**：使用 `malloc` 分配快照结构体和 xip_list
2. **数据复制**：从传入的 `active_txns` 数组复制到 `xip_list`

**与学习代码的对照**：
- 学习代码使用 `thread_local` 为每个线程维护独立状态
- 工程代码通过 xip_list 为每个快照记录创建时的活跃事务集合

## 线程模式对照

### 1. 线程局部状态

| 学习代码 | 工程代码 |
|---------|---------|
| `thread_local int counter` | `mvcc_snapshot_t *snapshot` |
| 每个线程独立的计数器 | 每个事务独立的快照 |

**相似性**：两者都为线程/事务提供独立的上下文存储。

### 2. 线程 ID 管理

学习代码中：
```cpp
std::this_thread::get_id()  // 获取当前线程标识
```

工程代码中：
```c
mvcc_txn_id_t txn_id;  // 事务 ID 作为线程标识
```

**相似性**：都需要唯一标识并发执行单元。

### 3. 活跃事务列表搜索

```c
// mvcc_snapshot.c 第 76-81 行
for (int i = 0; i < snapshot->xip_count; i++) {
    if (snapshot->xip_list[i] == txn_id) {
        return true;
    }
}
```

**与学习代码对照**：`main.cpp` 中的计数器递增演示了并发场景下的数据竞争，而 MVCC 通过快照隔离来避免脏读。

### 4. 序列化/反序列化

```c
// mvcc_snapshot_serialize() - 第 99-133 行
size_t mvcc_snapshot_serialize(const mvcc_snapshot_t *snapshot,
                                void *buf,
                                size_t buf_size);
```

**线程安全考量**：
- 序列化本身不涉及多线程
- 但序列化后的数据可能跨线程传递（如 WAL 日志）
- 需要考虑字节序和对齐问题

## 并发控制模式

### MVCC 的线程隔离机制

```
┌─────────────────────────────────────────────────────────┐
│                     事务 T1                              │
│  ┌─────────────────┐                                   │
│  │ 获取快照 S1      │ → 记录当前活跃事务 {T2, T3}        │
│  └─────────────────┘                                   │
│           ↓                                            │
│  ┌─────────────────┐                                   │
│  │ 读取数据        │ → 对 T1 可见：xmin < xid 且        │
│  │                 │           (xid < xmax 且           │
│  │                 │            xid ∉ {T2, T3})         │
│  └─────────────────┘                                   │
└─────────────────────────────────────────────────────────┘
           ↓
┌─────────────────────────────────────────────────────────┐
│                     事务 T2                              │
│  ┌─────────────────┐                                   │
│  │ 获取快照 S2      │ → 记录当前活跃事务 {T1}           │
│  └─────────────────┘                                   │
└─────────────────────────────────────────────────────────┘
```

### 与 std::thread 的设计对比

| 维度 | 学习代码 | 工程代码 |
|------|---------|---------|
| 并发单元 | 操作系统线程 | 数据库事务 |
| 状态隔离 | thread_local | MVCC 快照 |
| 标识符 | std::thread::id | mvcc_txn_id_t |
| 同步点 | join()/detach() | 检查点/WAL |
| 数据一致性 | mutex + 原子操作 | 快照隔离级别 |

## 核心启示

1. **线程局部存储的普遍性**：`thread_local` 是语言层面的线程隔离，而 MVCC 是数据库层面的事务隔离，本质都是为并发执行单元提供独立的视图。

2. **快照的价值**：`mvcc_snapshot_create()` 创建的快照是一个"时间点视图"，类似于在学习代码中用 `thread_local` 保存某个时刻的状态。

3. **标识符的作用**：线程需要 `std::thread::id`，事务需要 `mvcc_txn_id_t`，都是为了唯一标识并发单元，便于调试和关联。

4. **跨线程数据传递**：学习代码通过 lambda 捕获传递数据，工程代码通过序列化/反序列化传递快照（写入 WAL 时）。

## 代码位置

- MVCC 快照实现：`engineering/src/db/concurrency/mvcc_snapshot.c`
- MVCC 头文件：`engineering/include/db/concurrency/mvcc.h`
- 相关模块：
  - `engineering/src/db/concurrency/mvcc_txn.c`（事务管理）
  - `engineering/src/db/concurrency/mvcc_xid.c`（事务 ID 管理）
  - `engineering/src/db/wal/wal.c`（WAL 日志持久化）
