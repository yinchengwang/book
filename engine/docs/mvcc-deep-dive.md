# MVCC 深度解析：面试必备知识

> 本文档深入解析 MVCC（多版本并发控制）机制，对比 MySQL 与 PostgreSQL 的实现差异，
> 分析隔离级别、死锁等问题，是面试准备的必备资料。

## 目录

1. [MVCC 基础原理](#1-mvcc-基础原理)
2. [MySQL vs PostgreSQL MVCC 对比](#2-mysql-vs-postgresql-mvcc-对比)
3. [隔离级别实现原理](#3-隔离级别实现原理)
4. [幻读问题与解决](#4-幻读问题与解决)
5. [死锁检测与预防](#5-死锁检测与预防)

---

## 1. MVCC 基础原理

### 1.1 什么是 MVCC

**MVCC (Multi-Version Concurrency Control)** 是一种并发控制机制，通过为每个事务提供数据的多个版本，实现读写不阻塞的并发控制。

### 1.2 核心思想

```
┌─────────────────────────────────────────────────────────────┐
│                        MVCC 核心思想                          │
├─────────────────────────────────────────────────────────────┤
│  写操作不阻塞读操作                                           │
│  读操作不阻塞写操作                                           │
│  每次修改生成新版本，保留旧版本                               │
│  通过版本可见性判断实现隔离                                   │
└─────────────────────────────────────────────────────────────┘
```

### 1.3 关键概念

#### XMIN / XMAX

| 字段 | 含义 | 说明 |
|------|------|------|
| **xmin** | 创建事务ID | 创建这个版本的事务ID |
| **xmax** | 删除事务ID | 删除这个版本的事务ID（0表示未删除） |

#### 快照

```c
typedef struct mvcc_snapshot {
    txn_id_t xmin;        // 最小活动事务ID
    txn_id_t xmax;        // 最大已分配事务ID
    txn_id_t *xip_list;   // 活跃事务ID列表
    int xip_count;        // 活跃事务数量
} mvcc_snapshot_t;
```

#### 可见性判断规则

```
版本可见条件：
1. xmin 事务已提交（不在活跃列表且 xmin < snapshot->xmax）
2. 版本未被删除（xmax = 0）或 xmax 事务未提交
3. 自身事务创建的版本始终可见
```

### 1.4 版本链

```
时间 ──────────────────────────────────────────────────────►

事务T1:  INSERT ──► [xmin=T1, xmax=0] ──► [xmin=T2, xmax=0]
                                │              │
                                ▼              ▼
                           版本V1           版本V2（最新）

事务T2:  UPDATE ─────────────────────────► [xmin=T2, xmax=0]
                                              版本V2

事务T1:  DELETE ─────────────────────────► [xmin=T1, xmax=T1]
                                              版本V1（已删除）
```

---

## 2. MySQL vs PostgreSQL MVCC 对比

### 2.1 架构差异

| 特性 | MySQL (InnoDB) | PostgreSQL |
|------|----------------|------------|
| **MVCC 存储** | 回滚段 (Undo Log) | 表行中直接存储 |
| **版本链位置** | Undo 日志链 | Heap 行本身 |
| **可见性判断** | 读取视图 (ReadView) | 事务快照 |
| **事务ID** | trx_id (6字节) | xid (4字节，可回绕) |
| **垃圾回收** | Purge 线程 | VACUUM 进程 |

### 2.2 MySQL InnoDB MVCC

```
┌─────────────────────────────────────────────────────────────┐
│                     MySQL InnoDB MVCC                        │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌─────────┐    ┌─────────┐    ┌─────────┐                  │
│  │ Rollback│    │ Rollback│    │ Rollback│                  │
│  │ Segment │───►│ Segment │───►│ Segment │───► ...          │
│  └────┬────┘    └────┬────┘    └─────────┘                  │
│       │              │                                      │
│       ▼              ▼                                      │
│  ┌─────────┐    ┌─────────┐                                 │
│  │ Undo Log│    │ Undo Log│  ← 旧版本存储在 Undo            │
│  │  V1     │    │  V2     │                                 │
│  └─────────┘    └─────────┘                                 │
│                                                              │
│  表数据行：                                                   │
│  ┌─────────────────────────────────────────┐                │
│  │ DB_ROW_ID | DB_TRX_ID | DB_ROLL_PTR     │                │
│  │          |  12345    |  指向Undo的指针  │                │
│  └─────────────────────────────────────────┘                │
│                                                              │
│  ReadView 结构：                                             │
│  ┌─────────────────────────────────────────┐                │
│  │ m_ids: [活跃事务列表]                    │                │
│  │ min_trx_id: 最小活跃事务                 │                │
│  │ max_trx_id: 最大事务ID+1                 │                │
│  │ creator_trx_id: 当前事务ID               │                │
│  └─────────────────────────────────────────┘                │
└─────────────────────────────────────────────────────────────┘
```

#### InnoDB 可见性判断算法

```cpp
bool is_visible(trx_id, read_view) {
    // 1. 自身事务创建的始终可见
    if (trx_id == read_view.creator_trx_id) {
        return true;
    }

    // 2. trx_id < min_trx_id：事务已提交，可见
    if (trx_id < read_view.min_trx_id) {
        return true;
    }

    // 3. trx_id >= max_trx_id：事务在快照后开始，不可见
    if (trx_id >= read_view.max_trx_id) {
        return false;
    }

    // 4. trx_id 在活跃列表中：事务未提交，不可见
    if (trx_id in read_view.m_ids) {
        return false;
    }

    // 5. 其他情况可见
    return true;
}
```

### 2.3 PostgreSQL MVCC

```
┌─────────────────────────────────────────────────────────────┐
│                     PostgreSQL MVCC                          │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  表数据行直接存储 MVCC 信息：                                 │
│  ┌─────────────────────────────────────────┐                │
│  │ xmin | xmax | ctid | xvac | 数据...     │                │
│  │ 100  | 0    | (0,1) | -    | ...        │                │
│  └─────────────────────────────────────────┘                │
│       │     │      │                        │                │
│       │     │      └──► 指向下一个版本的指针 │                │
│       │     │                                  │                │
│       │     └──► xmax != 0 表示已删除        │                │
│       │                                        │                │
│       └──► xmin = 创建事务ID                  │                │
│                                                              │
│  事务快照（事务开始时获取）：                                  │
│  ┌─────────────────────────────────────────┐                │
│  │ xmin:  最小活动事务ID                    │                │
│  │ xmax:  最大已分配事务ID                  │                │
│  │ xip_list: 活跃事务ID数组                 │                │
│  └─────────────────────────────────────────┘                │
└─────────────────────────────────────────────────────────────┘
```

#### PostgreSQL 可见性判断算法

```c
bool MVCC_VersionVisible(snapshot, xmin, xmax, cur_txn_id) {
    // 1. 自身事务创建的版本始终可见
    if (xmin == cur_txn_id) {
        return true;
    }

    // 2. xmin 事务必须已提交
    if (xmin >= snapshot->xmax) {
        return false;  // 事务在快照创建后开始
    }
    if (txn_is_active(xmin)) {
        return false;  // xmin 事务仍在活跃
    }

    // 3. 检查删除事务 xmax
    if (xmax != 0) {
        if (txn_is_active(xmax)) {
            return false;  // 删除事务未提交
        }
        if (xmax < snapshot->xmax) {
            return false;  // 删除事务已提交
        }
    }

    return true;
}
```

### 2.4 关键差异总结

| 对比维度 | MySQL InnoDB | PostgreSQL |
|----------|--------------|------------|
| **存储方式** | Undo Log 外置 | 行内直接存储 |
| **读取最新数据** | 需要加锁 | 可无锁读取最新 |
| **版本回收** | Purge 线程 | VACUUM 后台进程 |
| **事务ID** | 6字节，大范围 | 4字节，需回绕处理 |
| **GC 时机** | 增量 Purge | VACUUM 批量处理 |
| **索引更新** | 需要更新索引 | HOT 减少索引更新 |

### 2.5 各自优势

#### MySQL InnoDB 优势

1. **回滚段可复用**：Undo Log 空间可循环使用
2. **读取最新无锁**：普通 SELECT 可无锁读取最新数据
3. **purge 更精细**：后台线程持续清理，无需专门维护

#### PostgreSQL 优势

1. **索引更新更少**：HOT 机制减少索引维护
2. **存储更紧凑**：不需要额外的 trx_id/roll_ptr 列
3. **可见性判断更快**：直接在行数据中判断

---

## 3. 隔离级别实现原理

### 3.1 SQL 标准隔离级别

```
┌─────────────────────────────────────────────────────────────┐
│                    SQL 隔离级别                              │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│   隔离级别          │ 脏读  │ 不可重复读 │ 幻读              │
│  ─────────────────┼──────┼───────────┼───────              │
│   READ UNCOMMITTED │  可能  │    可能    │   可能            │
│   READ COMMITTED   │  不可能│    可能    │   可能            │
│   REPEATABLE READ  │  不可能│    不可能  │   可能*           │
│   SERIALIZABLE     │  不可能│    不可能  │   不可能          │
│                                                              │
│   * MySQL InnoDB: 通过 Next-Key Lock 防止幻读               │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### 3.2 各隔离级别实现

#### READ UNCOMMITTED

**实现方式**：读取最新版本，不做任何检查

```c
// 最宽松的隔离级别
Version *read_uncommitted(Key key) {
    return get_latest_version(key);  // 直接获取最新
}
```

**特点**：
- 可能读到其他事务未提交的修改
- 实际很少使用

#### READ COMMITTED

**实现方式**：每次读取时创建新快照

```c
// PostgreSQL 默认级别
Version *read_committed(Key key) {
    Snapshot snap = create_snapshot();  // 每次读取都创建新快照
    return find_visible_version(key, snap);
}
```

**特点**：
- 同一个事务内，两次读取可能看到不同数据
- 每次语句执行时更新快照

#### REPEATABLE READ

**实现方式**：事务开始时创建快照，整个事务复用

```c
// MySQL InnoDB 默认级别
Version *repeatable_read(Key key) {
    if (txn->snapshot == NULL) {
        txn->snapshot = create_snapshot();  // 事务开始时创建
    }
    return find_visible_version(key, txn->snapshot);
}
```

**特点**：
- 同一事务内多次读取结果一致
- 需要防止幻读（通过锁或 MVCC）

#### SERIALIZABLE

**实现方式**：所有操作串行化

```c
// 最严格级别
Version *serializable_read(Key key) {
    lock_exclusive(key);  // 获取排他锁
    return find_visible_version(key, create_snapshot());
}
```

**特点**：
- 最高隔离级别
- 性能开销最大
- 通过锁或 SSI 检测序列化冲突

### 3.3 PostgreSQL 隔离级别实现

```c
/**
 * PostgreSQL 隔离级别与快照
 */

// READ COMMITTED：每个语句开始时更新快照
void exec_stmt(Statement *stmt) {
    if (isolation_level == READ_COMMITTED) {
        txn->snapshot = refresh_snapshot();  // 更新快照
    }
    execute(stmt);
}

// REPEATABLE READ：快照在事务开始时固定
void txn_begin() {
    if (isolation_level == REPEATABLE_READ ||
        isolation_level == SERIALIZABLE) {
        txn->snapshot = create_snapshot();  // 固定快照
    }
}
```

### 3.4 MySQL 隔离级别实现

```cpp
/**
 * MySQL InnoDB 隔离级别实现
 */

// READ COMMITTED：每次生成新的 ReadView
ReadView* trx_t::create_view() {
    if (isolation_level >= READ_COMMITTED) {
        return new ReadView();  // 每次都创建新视图
    }
    return view;  // 复用已有视图
}

// REPEATABLE READ：事务开始时创建 ReadView
void trx_start_for_read() {
    if (isolation_level == REPEATABLE_READ) {
        view = create_read_view();
    }
}

// Next-Key Lock 防止幻读
int row_search_mvcc(...) {
    if (isolation_level == REPEATABLE_READ) {
        // 使用 Next-Key Lock 锁住记录及间隙
        lock_rec_lock(TRUE,  // GAP_LOCK
                      page, offset,
                      LOCK_ORDINARY);  // Next-Key Lock
    }
}
```

---

## 4. 幻读问题与解决

### 4.1 什么是幻读

**幻读**：在同一事务中，两次执行相同的查询，第二次读取到了第一次没有的新行。

```
时间 ──────────────────────────────────────────────────────►

T1 事务:
  SELECT * FROM orders WHERE status = 'pending';  -- 返回 3 行
                                                      │
                                                      ▼
  UPDATE orders SET status = 'processed';      -- 修改了 5 行（幻读！）
                                                      │
                                                      ▼
  SELECT * FROM orders WHERE status = 'pending';  -- 返回 0 行

问题：T1 事务两次查询结果不一致，中间插入了新行
```

### 4.2 幻读 vs 不可重复读

| 概念 | 定义 | 区别 |
|------|------|------|
| **不可重复读** | 同一行数据两次读取结果不同 | 修改了已有行 |
| **幻读** | 同一查询两次结果集不同 | 新增/删除了行 |

### 4.3 解决方案

#### 方案1：锁（Locking）

```sql
-- 使用排他锁锁定查询范围
SELECT * FROM orders
WHERE status = 'pending'
FOR UPDATE;  -- 锁定范围内所有行

-- 或者锁定整个表
LOCK TABLES orders WRITE;
```

#### 方案2：Next-Key Lock（MySQL）

```c
// InnoDB 使用 Next-Key Lock 锁住记录及间隙
// 对于索引 key=100，锁定 [prev_key, 100] 和 [100, next_key]
// 防止在 100 附近插入新记录

// 锁结构
typedef struct lock_t {
    lock_mode_t mode;    // LOCK_ORDINARY (Next-Key)
    index_t *index;      // 锁定的索引
    unsigned char *page; // 页面
    int n_bits;          // 位图长度
} lock_t;
```

#### 方案3：Predicate Lock（PostgreSQL）

```c
// PostgreSQL 使用谓词锁
// 锁定满足条件的记录，而非具体记录

// 谓词锁结构
typedef struct predicate_lock_t {
    LockMode mode;
    RangeType *predicate;  // 锁定的条件范围
    TransactionId xid;
} predicate_lock_t;

// SSI (Serializable Snapshot Isolation) 使用谓词锁检测冲突
bool has_conflict(Transaction tx1, Transaction tx2) {
    return predicate_overlap(tx1.predicate, tx2.predicate);
}
```

#### 方案4：MVCC 配合索引

```c
// PostgreSQL 方案：索引扫描时检查可见性
Version *index_scan(IndexScan *scan) {
    // 1. 使用索引定位记录
    // 2. 检查每条记录的 MVCC 可见性
    // 3. 对范围查询，扫描索引项并过滤不可见项
    while ((key = index_next(scan)) != NULL) {
        if (MVCC_VersionVisible(snapshot, key.xmin, key.xmax)) {
            yield(key);
        }
    }
}
```

### 4.4 各数据库实现

#### MySQL InnoDB

```sql
-- REPEATABLE READ 隔离级别下
SET TRANSACTION ISOLATION LEVEL REPEATABLE READ;

-- 第一次查询
SELECT * FROM orders WHERE status = 'pending';
-- InnoDB 自动对扫描范围加 Next-Key Lock

-- 其他事务无法插入 status='pending' 的新行
INSERT INTO orders (status) VALUES ('pending');  -- 被阻塞或报错
```

#### PostgreSQL

```sql
-- PostgreSQL 使用 SSI 检测序列化冲突
SET TRANSACTION ISOLATION LEVEL SERIALIZABLE;

-- PostgreSQL 会检测读写冲突
-- 如果检测到序列化异常，回滚事务并重试
```

---

## 5. 死锁检测与预防

### 5.1 什么是死锁

**死锁**：两个或多个事务相互等待对方持有的锁，导致都无法继续执行。

```
┌─────────────────────────────────────────────────────────────┐
│                        死锁示例                              │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│   事务A:                      事务B:                         │
│   ┌──────────────────┐        ┌──────────────────┐          │
│   │ LOCK X on R1     │        │ LOCK X on R2     │          │
│   │ (成功)           │        │ (成功)           │          │
│   │                  │        │                  │          │
│   │ LOCK X on R2     │◄───────│ LOCK X on R1     │          │
│   │ (等待 R2)        │        │ (等待 R1)        │          │
│   └──────────────────┘        └──────────────────┘          │
│            │                          │                      │
│            └──────────┬───────────────┘                      │
│                       ▼                                      │
│              形成等待环路                                      │
│              死锁！                                           │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### 5.2 死锁检测算法

#### Wait-Die 方案

```c
/**
 * Wait-Die 方案（非抢占式）
 *
 * 规则：年轻事务等待，老事务终止
 * - 事务 T1 请求资源被 T2 持有
 * - 如果 T1 比 T2 年轻（开始更晚）：T1 等待
 * - 如果 T1 比 T2 老：T1 终止（die）
 */
bool wait_or_die(Transaction t1, Transaction t2, Resource r) {
    if (t1.start_time > t2.start_time) {
        // T1 更年轻，等待
        wait(t1, r);
        return true;
    } else {
        // T1 更老，终止自己
        abort(t1);
        return false;
    }
}
```

#### Wound-Wait 方案

```c
/**
 * Wound-Wait 方案（抢占式）
 *
 * 规则：老事务抢占，年轻事务等待
 * - 事务 T1 请求资源被 T2 持有
 * - 如果 T1 比 T2 老：T2 被抢占（wound），T1 获得资源
 * - 如果 T1 比 T2 年轻：T1 等待
 */
bool wound_or_wait(Transaction t1, Transaction t2, Resource r) {
    if (t1.start_time < t2.start_time) {
        // T1 更老，抢占 T2
        preempt(t2);
        acquire(t1, r);
        return true;
    } else {
        // T1 更年轻，等待
        wait(t1, r);
        return true;
    }
}
```

#### 等待图检测（Wait-For Graph）

```c
/**
 * 死锁检测：构建等待图
 *
 * 节点：事务
 * 边：T1 -> T2 表示 T1 等待 T2 持有的资源
 */
typedef struct {
    TransactionId from;   // 等待的事务
    TransactionId to;     // 持有资源的事务
} WaitEdge;

bool detect_deadlock() {
    // 构建等待图
    Graph g = build_wait_graph();

    // 检测环路
    if (has_cycle(g)) {
        // 找到死锁，选择一个事务回滚
        Transaction victim = select_victim();
        abort(victim);
        return true;
    }
    return false;
}
```

### 5.3 PostgreSQL 死锁处理

```c
/**
 * PostgreSQL 死锁检测
 */

// 死锁检测定时执行
void DeadLockCheck() {
    // 1. 扫描所有锁等待关系
    // 2. 构建等待图
    // 3. 检测环路
    // 4. 选择 victim 并回滚

    if (has_deadlock()) {
        Transaction victim = select_deadlock_victim();
        ereport(WARNING,
            (errmsg("detected deadlock while waiting, "
                    "transaction %d will be rolled back",
                    victim)));
        AbortTransaction(victim);
    }
}

// Victim 选择策略：优先选择年轻事务
Transaction select_deadlock_victim() {
    Transaction min_txn = NULL;
    int min_cost = INT_MAX;

    for (Transaction t : deadlock_cycle) {
        int cost = calculate_abort_cost(t);
        if (cost < min_cost) {
            min_cost = cost;
            min_txn = t;
        }
    }
    return min_txn;
}
```

### 5.4 MySQL 死锁处理

```cpp
/**
 * MySQL InnoDB 死锁检测
 */

// InnoDB 使用等待图检测死锁
bool lock_deadlock_check() {
    // 构建等待图
    for (Lock *wait_lock : waiting_locks) {
        // 从等待的锁开始追溯
        Transaction *wait_trx = wait_lock->trx;
        Lock *held_lock = find_conflicting_lock(wait_lock);

        if (held_lock == NULL) continue;

        Transaction *hold_trx = held_lock->trx;

        // 检查是否形成环路
        if (wait_trx->is_in_wait_graph(hold_trx)) {
            // 发现死锁，选择 victim
            Transaction *victim = choose_deadlock_victim(
                wait_trx, hold_trx);
            rollback_deadlock_victim(victim);
            return true;
        }
    }
    return false;
}

// Victim 选择：优先回滚锁数量最少的事务
Transaction* choose_deadlock_victim(Transaction *t1, Transaction *t2) {
    int lock_count1 = t1->get_lock_count();
    int lock_count2 = t2->get_lock_count();

    // 回滚持有锁较少的事务
    return (lock_count1 < lock_count2) ? t1 : t2;
}
```

### 5.5 死锁预防策略

#### 1. 统一锁获取顺序

```c
/**
 * 预防死锁：按固定顺序获取锁
 */

// 错误的做法：不同事务按不同顺序获取锁
void txn_a() {
    lock(R1);
    lock(R2);  // 事务B可能先锁R2
}

void txn_b() {
    lock(R2);
    lock(R1);  // 死锁！
}

// 正确的做法：所有事务按相同顺序获取锁
void txn_a() {
    lock(R1);
    lock(R2);
}

void txn_b() {
    lock(R1);  // 必须先锁R1
    lock(R2);
}
```

#### 2. 减少锁持有时间

```c
/**
 * 预防死锁：减少锁持有时间
 */

// 不好的做法：长时间持有锁
void bad_transaction() {
    lock(resource);
    do_something_slow();  // 耗时的业务逻辑
    unlock(resource);
}

// 好的做法：尽快释放锁
void good_transaction() {
    lock(resource);
    data = read_data();
    unlock(resource);

    process_data(data);  // 锁外处理

    lock(resource);
    write_data(data);
    unlock(resource);
}
```

#### 3. 使用低隔离级别

```sql
-- 使用 READ COMMITTED 代替 REPEATABLE READ
SET TRANSACTION ISOLATION LEVEL READ COMMITTED;

-- 或者使用 SELECT ... FOR UPDATE NOWAIT
SELECT * FROM orders WHERE id = 1 FOR UPDATE NOWAIT;
-- 如果无法立即获得锁，立即报错而非等待
```

#### 4. 超时机制

```c
/**
 * 死锁预防：锁等待超时
 */

#define LOCK_WAIT_TIMEOUT 5  // 5秒超时

int lock_acquire_with_timeout(Lock *lock, Transaction *txn) {
    start_time = get_current_time();

    while (!try_lock(lock, txn)) {
        if (get_current_time() - start_time > LOCK_WAIT_TIMEOUT) {
            // 超时，释放所有锁并回滚
            release_all_locks(txn);
            return -1;  // 超时错误
        }
        sleep(100);  // 短暂等待后重试
    }
    return 0;
}
```

### 5.6 死锁处理总结

| 策略 | 描述 | 适用场景 |
|------|------|----------|
| **Wait-Die** | 年轻等待，老终止 | 长时间事务较多 |
| **Wound-Wait** | 老抢占，年轻的等待 | 短事务较多 |
| **等待图检测** | 检测环路并回滚 victim | 通用方案 |
| **超时机制** | 锁等待超时后回滚 | 简单实现 |
| **统一顺序** | 按固定顺序获取锁 | 可控场景 |

---

## 附录：面试高频问题

### Q1: MVCC 是如何实现读写不阻塞的？

**答**：MVCC 通过为每个事务提供数据快照实现读写分离。读操作读取快照数据，不阻塞写操作；写操作创建新版本，不阻塞读操作。真正冲突时（如写-写）才需要等待。

### Q2: MySQL 和 PostgreSQL 的 MVCC 实现有什么区别？

**答**：主要区别在于版本存储位置。MySQL 将版本存储在 Undo Log 中，通过 roll_ptr 指针关联；PostgreSQL 直接在表行中存储 xmin/xmax。MySQL 可以无锁读取最新数据，PostgreSQL 通过 HOT 减少索引更新。

### Q3: 什么是 Next-Key Lock？如何防止幻读？

**答**：Next-Key Lock = Record Lock + Gap Lock。它锁定记录本身及记录前后的间隙，防止其他事务在间隙中插入新记录，从而防止幻读。

### Q4: 什么是 SSI（Serializable Snapshot Isolation）？

**答**：SSI 是 PostgreSQL 实现的序列化快照隔离。它在 MVCC 基础上增加谓词锁，通过检测读写冲突来防止序列化异常，比传统锁方式性能更好。

### Q5: 死锁检测的等待图算法原理是什么？

**答**：构建有向图，节点是事务，边表示等待关系（Ti -> Tj 表示 Ti 等待 Tj 持有的资源）。检测图中是否存在环路，存在则表示死锁。回滚环路中的一个事务即可解除死锁。

---

*文档版本: v1.0*
*最后更新: 2026-07-12*
