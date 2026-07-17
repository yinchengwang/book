# 分布式同步 学习笔记

## 核心概念

- **分布式锁**: 基于 Redis/etcd/ZK 的互斥锁，支持租约自动释放
- **乐观锁**: 假设冲突少，操作前 CAS 检查版本号
- **悲观锁**: 假设冲突多，操作前直接加锁
- **两阶段锁 (2PL)**: 先加锁后操作→最后释放，保证事务串行化
- **CAS (Compare-And-Swap)**: 版本号匹配才执行写操作

## 锁策略选择

| 场景 | 推荐策略 | 原因 |
|------|----------|------|
| 读多写少 | 乐观锁 | 冲突少，CAS 开销低 |
| 写密集 | 悲观锁 | 冲突频繁，CAS 浪费 |
| 跨进程 | 分布式锁 | ZK/Redis 协调 |
| 短操作 | 乐观锁 | 无锁等待 |
| 长操作 | 悲观锁 | 避免反复重试 |

## 工程对照

`engineering/src/db/lock/lock.c` 是本项目中锁管理的工程实现。`lock_t` 结构体定义了锁类型（行锁/表锁/意向锁），`lock_acquire()` 和 `lock_release()` 实现了加锁/解锁的完整流程。`lock_manager_t` 管理整个锁池，支持死锁检测（`lock_deadlock_detect()`）——这是一个系统设计中"分布式锁"在单机数据库的实现：每个事务获取锁后修改数据，释放锁时通知等待队列。`engineering/include/db/lock.h` 中定义了锁模式（共享锁/排他锁/意向锁），这与乐观锁/悲观锁的分类对应——共享锁是悲观读锁，行级锁是悲观写锁。`engineering/src/db/txn/txn.c` 中的事务管理结合了锁和 MVCC 实现事务隔离——MVCC 本身就是一种"乐观并发控制"策略（读不阻塞写）。`engineering/include/db/txn.h` 中定义了 `txn_id`、`txn_state`（ACTIVE/COMMITTED/ABORTED）等数据结构。`engineering/src/db/lock/lock.c` 中的死锁检测通过等待图（Wait-For Graph）实现，发现循环等待后选择 victim 回滚——这是分布式锁中"锁超时"机制的对应。

## 面试要点

1. 分布式锁必须考虑锁租约（lease）——防止持有者崩溃导致死锁
2. Redis SET NX EX 实现分布式锁简单高效，Redis Redlock 算法更可靠
3. CAS 的 ABA 问题可通过版本号/时间戳解决
