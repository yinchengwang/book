# 锁机制 - 工程对照

## 本学习卡演示的锁概念在工程轨的实现对照

### 1. 锁类型实现

**PostgreSQL 风格锁**

- **工程实现位置**: `engineering/include/db/concurrency/lock.h`
- **核心结构**: `lock_request_t`（锁请求）
- **锁类型枚举**:
  - `SHARE_LOCK`: 共享锁
  - `EXCLUSIVE_LOCK`: 排他锁
  - `ACCESS_SHARE_LOCK`: 访问共享锁（PostgreSQL 特有）

**锁管理器**

- **位置**: `engineering/src/db/concurrency/lock_manager.c`
- **核心函数**:
  - `lock_acquire()`: 获取锁
  - `lock_release()`: 释放锁
  - `lock_upgrade()`: 锁升级

### 2. 锁兼容性矩阵

**PostgreSQL 锁模式**

```c
// 锁模式兼容性（部分）
ACCESS_SHARE_LOCK   与 ACCESS_SHARE_LOCK 兼容
ROW_SHARE_LOCK      与 ROW_EXCLUSIVE_LOCK 兼容
ROW_EXCLUSIVE_LOCK  与 ROW_SHARE_LOCK、SHARE_LOCK 兼容
SHARE_LOCK          与 ROW_EXCLUSIVE_LOCK 兼容
SHARE_UPDATE_EXCLUSIVE 与多种模式部分兼容
```

### 3. 死锁检测

**死锁检测器**

- **位置**: `engineering/src/db/concurrency/deadlock.c`
- **核心算法**: 等待图（Wait-For Graph）
- **关键函数**:
  - `deadlock_detect()`: 检测死锁
  - `deadlock_resolve()`: 解决死锁（回滚代价最小的事务）

### 4. 锁粒度实现

**行级锁**

- **位置**: `engineering/include/db/concurrency/row_lock.h`
- **特点**: 锁定单行，并发度最高
- **实现**: HTE（Heavyweight Lock）封装

**页面锁**

- **位置**: `engineering/include/db/buf.h`（Buffer 页锁）
- **特点**: 锁定 8KB 数据页
- **使用场景**: 批量操作优化

**表级锁**

- **位置**: `engineering/src/db/concurrency/table_lock.c`
- **特点**: 锁定整张表
- **使用场景**: DDL 操作、批量导入

### 5. 两阶段锁协议（2PL）

**实现位置**: 锁管理器集成在事务管理器中

**两阶段**:

1. **扩展阶段**: 获取锁，不释放
2. **收缩阶段**: 提交或回滚时统一释放所有锁

**变体**:

- 严格两阶段锁（Strict 2PL）: 排他锁持有到事务结束
- 强严格两阶段锁（Strong Strict 2PL）: 所有锁持有到事务结束

### 6. 锁超时机制

**配置参数**

```c
// GUC 参数
lock_timeout     // 锁等待超时
statement_timeout // 语句超时
idle_in_transaction_session_timeout // 空闲事务超时
```

### 7. 行锁实现细节

**行锁结构**（PostgreSQL 风格）

```c
typedef struct {
    TransactionId xmax;      // 锁定/删除事务 ID
    LockTupleHeader hdr;     // 头部的锁信息
} HeapTupleHeader;
```

**可见性判断**:

- xmax = 0: 元组未被锁定/删除
- xmax != 0: 需检查 xmax 事务状态

### 8. 轻量级锁 vs 重锁

**轻量级锁（LWLatch）**

- **用途**: 进程内同步（内存页、热点数据结构）
- **特点**: 零依赖、无进程间开销

**重量级锁（HWLock）**

- **用途**: 跨进程/跨事务协调
- **特点**: 支持死锁检测、锁升级

---

## 学习要点总结

| 学习卡概念 | 工程实现文件 | 核心函数/结构 |
|-----------|-------------|--------------|
| S/X 锁类型 | `lock.h` | `LockMode` 枚举 |
| 锁获取/释放 | `lock_manager.c` | `lock_acquire/release()` |
| 死锁检测 | `deadlock.c` | `deadlock_detect()` |
| 行级锁 | `row_lock.h` | `HeapTupleHeader.xmax` |
| 锁超时 | `guc.c` | `lock_timeout` 参数 |

## 进一步阅读

- `engineering/src/db/concurrency/lock_manager.c` - 锁管理器核心实现
- `engineering/src/db/concurrency/deadlock.c` - 死锁检测算法
- `engineering/include/db/concurrency/lwlock.h` - 轻量级锁实现
- PostgreSQL 官方文档: Lock Management
