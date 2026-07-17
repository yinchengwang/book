# MVCC 机制详解

## 1. 概述

MVCC（Multi-Version Concurrency Control，多版本并发控制）是一种并发控制协议，通过维护数据的多个版本来实现读写不阻塞。

### 1.1 为什么需要 MVCC？

传统锁机制的痛点：

| 方案 | 问题 |
|------|------|
| 读锁（S 锁） | 读操作互相阻塞 |
| 写锁（X 锁） | 读写互相阻塞 |
| 乐观锁 | 冲突时重试开销大 |

MVCC 解决了这些问题：

```
传统锁 vs MVCC：

传统锁：
T1: 读 ────────────────────────────────────▶
T2:    写 ─────────▶ (等待 T1 释放读锁)

MVCC：
T1: 读 v1 ─────────────────────────────────▶
T2:    写 v2 ─────────▶ (无需等待，并发进行)
```

### 1.2 MVCC 的优势

| 特性 | 说明 |
|------|------|
| 读写不阻塞 | 读操作从不阻塞写，写操作从不阻塞读 |
| 快照隔离 | 每个事务看到一致的数据快照 |
| 回滚支持 | 保留旧版本支持事务回滚 |
| 时间旅行 | 可以查询历史某个时间点的数据 |

## 2. 核心概念

### 2.1 事务 ID（Transaction ID）

每个事务有一个唯一的 ID（xid）：

```c
typedef uint32_t TransactionId;  // 64位系统中可能更大

// 特殊值
#define InvalidTransactionId     0
#define FrozenTransactionId      1  // 冻结的事务，视为"总是过去"
// BootstrapTransactionId = 2  // 系统启动时使用
```

### 2.2 版本链（Version Chain）

每行数据可能有多个版本，形成版本链：

```
版本链结构：

┌─────────────────────────────────────────────────────────────┐
│                                                             │
│  T1: INSERT (xmin=100, xmax=0)                              │
│           │                                                 │
│           ▼                                                 │
│  ┌─────────────────┐                                        │
│  │  HeapTupleData  │                                        │
│  │  xmin: 100      │ ──→ 提交后可见                         │
│  │  xmax: 0        │                                        │
│  │  data: "v1"     │                                        │
│  └────────┬────────┘                                        │
│           │ T2: UPDATE (xmin=200, xmax=100)                 │
│           ▼                                                 │
│  ┌─────────────────┐                                        │
│  │  HeapTupleData  │                                        │
│  │  xmin: 200      │ ──→ 提交后可见                         │
│  │  xmax: 0        │                                        │
│  │  data: "v2"     │                                        │
│  └────────┬────────┘                                        │
│           │ T3: DELETE (xmin=300, xmax=200)                 │
│           ▼                                                 │
│  ┌─────────────────┐                                        │
│  │  HeapTupleData  │                                        │
│  │  xmin: 300      │                                        │
│  │  xmax: 200      │ ──→ 未提交则可见，提交后不可见          │
│  │  data: "v2"     │                                        │
│  └─────────────────┘                                        │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 2.3 关键字段

| 字段 | 说明 |
|------|------|
| `xmin` | 创建此版本的事务 ID |
| `xmax` | 删除/更新此版本的事务 ID（0 表示未被删除） |
| `tcid` | 命令 ID（同一事务内的多语句可见性） |
| `xctid` | 指向此版本的后继版本的指针 |

## 3. 快照机制

### 3.1 快照数据结构

```c
typedef struct {
    TransactionId xmin;        // 最小活跃事务 ID
    TransactionId xmax;        // 最大事务 ID（之后的都未开始）
    int           xip_count;   // 活跃事务数量
    TransactionId *xip_list;   // 活跃事务 ID 数组
} SnapshotData;
```

### 3.2 快照语义

```
快照时空图：

        xmin      xmax
          │        │
          ▼        ▼
    ─────┼────────┼──────────────────────────────────────────▶ xid
          │        │
     已提交    正在运行           未开始
     的事务    的事务

可见性规则：
1. xid < xmin：事务已提交，一定可见
2. xmin ≤ xid < xmax：
   - 如果 xid 在 xip_list 中：未提交，不可见
   - 如果 xid 不在 xip_list 中：已提交，可见
3. xid ≥ xmax：事务还未开始，不可见
```

### 3.3 快照获取

```c
/**
 * @brief 获取当前事务的快照
 *
 * 快照在事务开始时获取，事务期间不变：
 * - xmin = 当前活跃的最小事务 ID
 * - xmax = 下一个将分配的事务 ID
 * - xip_list = 当前所有活跃事务的 ID 列表
 */
Snapshot GetSnapshotData(void) {
    Snapshot snapshot;

    snapshot.xmin = GlobalXmin;        // 全局最小活跃事务
    snapshot.xmax = NextXid;           // 下一个事务 ID
    snapshot.xip_count = 0;

    // 遍历 ProcArray 获取所有活跃事务
    for (each backend process) {
        if (process->xid != 0 && process->xid < snapshot.xmax) {
            snapshot.xip_list[snapshot.xip_count++] = process->xid;
        }
    }

    return snapshot;
}
```

## 4. 可见性判断算法

### 4.1 基本可见性规则

```
可见性判断伪代码：

bool VersionIsVisible(Snapshot snap, HeapTuple tuple) {
    // 规则 1：自身事务创建的版本始终可见
    if (tuple->t_data->t_xmin == MyXid) {
        // 如果是 UPDATE/DELETE，检查 xmax
        if (tuple->t_data->t_xmax != 0) {
            // 自身事务的删除需要检查是否在同一个命令中
            if (tuple->t_data->t_xmax != MyXid ||
                tuple->t_tableOid != MyTableOid) {
                return false;
            }
        }
        return true;
    }

    // 规则 2：xmax 已提交 = 不可见
    if (tuple->t_data->t_xmax != 0) {
        if (TransactionIdDidCommit(tuple->t_data->t_xmax)) {
            return false;  // 被其他事务删除
        }
    }

    // 规则 3：xmin 未提交 = 不可见
    if (!TransactionIdDidCommit(tuple->t_data->t_xmin)) {
        return false;
    }

    // 规则 4：xmin 在快照的活跃列表中 = 不可见
    if (TransactionIdInArray(tuple->t_data->t_xmin, snap->xip_list)) {
        return false;
    }

    // 规则 5：xid 在快照范围内但不在活跃列表 = 已提交 = 可见
    if (tuple->t_data->t_xmin < snap->xmax) {
        return true;
    }

    return false;
}
```

### 4.2 可见性决策表

| xmin | xmax | xmin 在活跃列表 | xmin 已提交 | xmax 已提交 | 结果 |
|------|------|----------------|------------|------------|------|
| T1 | 0 | 否 | 是 | - | **可见** |
| T1 | 0 | 是 | - | - | 不可见 |
| T1 | T2 | 否 | 是 | 是 | **不可见**（被删除）|
| T1 | T2 | 否 | 是 | 否 | **可见**（删除未提交）|
| T2 | 0 | - | 否 | - | 不可见 |

## 5. 事务并发场景

### 5.1 读已提交（Read Committed）

每次语句执行时重新获取快照：

```
时间线：
─────────────────────────────────────────────────────────────────▶
     │           │           │           │
     T1          T2          T1          T2
   BEGIN       BEGIN       UPDATE      COMMIT
     │           │           │           
   SELECT       │           │           
 (快照1)        │           │           
     │           │           │           
     │         SELECT ───────┼───── (看到 T1 的更新！)
     │       (快照2)         │           
     │           │           │           
   COMMIT        │           │           
     │           │           │           
     │           │         COMMIT        
```

### 5.2 可重复读（Repeatable Read）

事务开始时获取快照，整个事务期间不变：

```
时间线：
─────────────────────────────────────────────────────────────────▶
     │           │           │           │
     T1          T2          T1          T2
   BEGIN       BEGIN       UPDATE      COMMIT
     │           │           │           
   SELECT ──────┼───── (看到 v1，始终一致)
 (快照1)        │           
     │           │           
     │         UPDATE ──── (创建 v2)
     │           │           
     │         COMMIT        
     │           │           
     │           │           
   SELECT ──────┼───── (还是看到 v1！)
 (同一快照)     │           
     │           │           
   COMMIT        │           
```

### 5.3 序列化异常

MVCC 的快照隔离可能产生序列化异常：

```
Write Skew 异常：

场景：医生值班表
- 医生 A 和医生 B 在两个不同的病房
- 规则：每个病房至少有一个医生值班
- 当前：A 在病房1，B 在病房2

时间线：
T1: BEGIN
T2: BEGIN
T1: SELECT * FROM schedule WHERE ward=1  -- 返回 A
T2: SELECT * FROM schedule WHERE ward=2  -- 返回 B
T1: UPDATE schedule SET doctor='C' WHERE ward=2  -- 违反规则！
T2: UPDATE schedule SET doctor='D' WHERE ward=1  -- 违反规则！
T1: COMMIT
T2: COMMIT

结果：两个病房都没有医生了（但两个事务都成功提交了）
```

## 6. Undo 日志机制

### 6.1 Undo 记录结构

```c
typedef struct {
    TransactionId xid;           // 事务 ID
    UndoRecordType type;         // INSERT/DELETE/UPDATE
    RelFileNode rnode;           // 表的物理文件
    BlockNumber blkno;           // 页面号
    OffsetNumber offset;         // 行号
    void *old_data;              // 修改前的数据
    void *new_data;              // 修改后的数据
    URecPtr   prev_undo;         // 前一个 Undo 记录的指针
} UndoRecord;
```

### 6.2 Undo 链

```
Undo 链结构：

┌─────────────────────────────────────────────────────────────┐
│  事务 T1 的 Undo 空间                                        │
│                                                             │
│  UndoRecord[3] ──→ UndoRecord[2] ──→ UndoRecord[1]         │
│       ▲               ▲              ▲                      │
│       │               │              │                      │
│   UPDATE v3→v2    UPDATE v2→v1   INSERT v1                  │
│                                                             │
│  每个 UPDATE 创建一条 Undo，记录旧值                         │
└─────────────────────────────────────────────────────────────┘
```

### 6.3 回滚操作

```c
/**
 * @brief 回滚单个操作
 *
 * 使用 Undo 记录恢复旧值：
 * 1. 读取 Undo 记录
 * 2. 根据类型恢复数据
 * 3. 更新行指针
 */
void ExecUndo(void *record) {
    UndoRecord *undo = (UndoRecord *)record;

    switch (undo->type) {
        case UNDO_INSERT:
            // 删除插入的元组
            heap_delete(undo->rnode, undo->blkno, undo->offset);
            break;

        case UNDO_DELETE:
            // 恢复已删除的元组
            heap_insert(undo->rnode, undo->old_data);
            break;

        case UNDO_UPDATE:
            // 用旧值覆盖新值
            heap_update(undo->rnode, undo->blkno, undo->offset,
                       undo->old_data);
            break;
    }
}
```

## 7. 垃圾回收（Vacuum）

### 7.1 垃圾产生的原因

```
版本链断裂后，旧版本成为垃圾：

UPDATE t SET x=2 WHERE id=1;

之前： HEAD ──→ [x=1]
之后： HEAD ──→ [x=2] ──→ [x=1] (不再可达，变成垃圾)

DELETE FROM t WHERE id=1;

之前： HEAD ──→ [x=1]
之后： HEAD ──→ [x=1] (标记为删除，但空间未释放)
```

### 7.2 垃圾回收策略

| 策略 | 触发条件 | 优点 | 缺点 |
|------|----------|------|------|
| 手动 VACUUM | 用户执行 | 精确控制 | 需要人工干预 |
| 自动 VACUUM | 阈值触发 | 自动化 | 可能影响性能 |
| 增量 GC | 持续运行 | 平滑 | 实现复杂 |

### 7.3 垃圾判定条件

```c
/**
 * @brief 判断元组是否为垃圾
 *
 * 垃圾元组的判定：
 * 1. xmax 已提交且不是当前事务
 * 2. 没有其他事务需要访问此版本
 */
bool HeapTupleIsDead(HeapTuple tuple, Snapshot snapshot) {
    TransactionId xmax = tuple->t_data->t_xmax;

    // 规则 1：从未被删除
    if (xmax == InvalidTransactionId) {
        return false;
    }

    // 规则 2：被当前事务删除
    if (xmax == snapshot->xid) {
        return false;
    }

    // 规则 3：删除事务未提交
    if (!TransactionIdDidCommit(xmax)) {
        return false;
    }

    // 规则 4：删除事务已提交，且没有活跃事务需要旧版本
    if (TransactionIdDidCommit(xmax) &&
        xmax < snapshot->xmin) {
        return true;  // 垃圾
    }

    return false;
}
```

## 8. 与 PostgreSQL 的差异

### 8.1 实现差异

| 特性 | PostgreSQL | 本实现 |
|------|-----------|--------|
| 事务 ID | 64 位 | 32 位 |
| 快照存储 |ProcArray + CLOG | 内存数组 |
| 可见性判断 | 复杂的函数链 | 简化版 |
| Undo 机制 | Ring Buffer | 简化版 |
| GC | autovacuum | 手动触发 |

### 8.2 PostgreSQL 的 SSI

PostgreSQL 使用 SSI（Serializable Snapshot Isolation）来解决写偏斜：

```c
// 检测序列化冲突
bool DetectSerializableConflict(TransactionId xid, BlockNumber blk) {
    // SSI 使用 SIREAD 锁来检测潜在的冲突
    for (each predicate lock) {
        if (predicate_lock.overlaps(read_set)) {
            if (has_conflicting_write(xid, predicate_lock)) {
                return true;  // 序列化冲突
            }
        }
    }
    return false;
}
```

## 9. 性能考量

### 9.1 版本链长度

版本链越长，查找可见版本的开销越大：

```
版本链长度对性能的影响：

链长 = 1:  HEAD ──→ [v1]          ──▶ 1 次查找
链长 = 3:  HEAD ──→ [v4] ──→ [v3] ──→ [v2] ──→ [v1]
                                          ──▶ 4 次查找
链长 = N:  HEAD ──→ [vN] ──→ ... ──→ [v1]
                                          ──▶ N 次查找
```

### 9.2 优化策略

| 策略 | 说明 |
|------|------|
| 热点数据短链 | UPDATE 频繁的表需要更频繁的 VACUUM |
| 索引辅助 | 索引直接指向最新版本，避免遍历 |
| 版本池 | 回收旧版本但保留一段时间 |
| 列存储 | 只存储变化的列 |

## 10. 总结

MVCC 的核心要点：

1. **多版本**：每个修改创建新版本，读操作不阻塞
2. **快照隔离**：事务看到一致性快照
3. **可见性判断**：基于事务 ID 和快照判断版本是否可见
4. **Undo 机制**：支持事务回滚
5. **垃圾回收**：清理不再需要的旧版本
6. **隔离级别**：不同实现支持不同级别的隔离

理解 MVCC 是理解现代数据库并发控制的基础。

---

*文档版本：v1.0*
*最后更新：2026-07-12*
