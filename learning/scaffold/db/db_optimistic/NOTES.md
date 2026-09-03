# 工程对照笔记

## 与 PostgreSQL MVCC 的关联

PostgreSQL 的 MVCC（多版本并发控制）实际上是一种**乐观并发控制**的实现方式。

关键区别在于版本管理策略：

| 特性 | 纯 OCC（演示代码） | PostgreSQL MVCC |
|------|-------------------|-----------------|
| 版本存储 | 单一记录 + 版本号 | 每个事务可见独立版本 |
| 清理机制 | 无（单用户模拟） | VACUUM 清理旧版本 |
| 读不阻塞写 | 支持 | 支持 |
| 写不阻塞读 | 支持 | 支持 |

## PostgreSQL 的 xmin/xmax 系统

PostgreSQL 内部使用 `xmin`（创建版本的事务ID）和 `xmax`（删除/更新版本的事务ID）来实现多版本。

```c
// PostgreSQL 内部元组结构简化版
typedef struct {
    TransactionId t_xmin;   // 插入该版本的事务ID
    TransactionId t_xmax;   // 删除/更新该版本的事务ID
    uint32        t_ctid;   // 指向版本链中的下一个版本
    // ... 其他字段
} HeapTupleFields;
```

事务ID（TransactionId）是 32 位递增整数，当达到上限时会"回卷"，需要 VACUUM 处理。

## MySQL InnoDB 的 MVCC 实现

MySQL InnoDB 通过 **undo log** + **ReadView** 实现 MVCC：

1. **undo log**：记录数据修改前的镜像，形成版本链
2. **ReadView**：事务启动时生成的"快照"，决定能看到哪个版本

```sql
-- 不同隔离级别使用不同的 ReadView 策略
SET TRANSACTION ISOLATION LEVEL READ COMMITTED;  -- 每次语句创建新 ReadView
SET TRANSACTION ISOLATION LEVEL REPEATABLE READ; -- 事务开始时创建 ReadView
```

## 乐观锁 vs 悲观锁

```
悲观锁（Pessimistic Locking）：
    假设冲突必然发生，读取时立即加锁
    - 优点：避免冲突后重试的开销
    - 缺点：并发度低，锁竞争严重
    - 代表：2PL, SELECT FOR UPDATE

乐观锁（Optimistic Locking）：
    假设冲突较少，提交时才检测
    - 优点：并发度高，无锁读
    - 缺点：冲突时需回滚并重试
    - 代表：OCC with version, CAS
```

## 应用层乐观锁实现

在实际业务中，乐观锁常通过版本号字段实现：

```sql
-- 用户表带版本号
CREATE TABLE users (
    id      INT PRIMARY KEY,
    name    VARCHAR(100),
    version INT DEFAULT 1  -- 版本号
);

-- 更新时检查版本号
UPDATE users
SET name = '新名字', version = version + 1
WHERE id = 1 AND version = 5;
-- 影响行数为 0 表示版本冲突
```

## 分布式乐观锁

在分布式系统中，乐观锁可以通过 **CAS (Compare-And-Swap)** 实现：

```c
// 原子操作：只有当前值等于 expected时才更新
bool atomic_compare_and_swap(int *ptr, int expected, int new_value);

// Redis 中的 CAS 实现
SET user:1:balance 1000 XX  // 只有 key 存在时才设置
WATCH user:1                // 监视键
MULTI
INCRBY user:1:balance -50
EXEC                         // 如果 WATCH 的键未被修改才执行
```

## 本演示代码与工程实现的差异

| 方面 | 演示代码 | 真实数据库 |
|------|---------|-----------|
| 并发模型 | 模拟串行，无真实并发 | 多线程/多进程真实并发 |
| 数据持久化 | 全在内存 | 磁盘持久化 + WAL |
| 事务边界 | 手动模拟 | 数据库事务机制 |
| 冲突检测 | 内存版本号比较 | 事务ID或时间戳比较 |
| 回滚机制 | 简单回退变量 | undo log + 页面回滚 |

## 参考资料

- PostgreSQL 源码：`src/include/access/htup_details.h`（元组头结构）
- InnoDB 论文：Sidlauskas, D. et al. "A Tale of Two SMOs"（B-Tree 与 MVCC 结合）
- 《Designing Data-Intensive Applications》第 7 章：Transactions
