# 事务 · 学习笔记

## 核心概念

1. **ACID**：原子性（全做或全不做）、一致性（约束不变）、隔离性（并发互不干扰）、持久性（提交即永存）
2. **脏读**：读到其他事务未提交的数据
3. **不可重复读**：同一事务中两次读同一行得到不同结果（因其他事务提交了 UPDATE）
4. **幻读**：同一事务中两次范围查询得到不同行集（因其他事务提交了 INSERT）
5. **SERIALIZABLE**：最高隔离级别，事务串行执行，无并发问题

## 隔离级别对比

| 级别 | 脏读 | 不可重复读 | 幻读 |
|------|------|-----------|------|
| READ UNCOMMITTED | 可能 | 可能 | 可能 |
| READ COMMITTED | 避免 | 可能 | 可能 |
| REPEATABLE READ | 避免 | 避免 | 可能 |
| SERIALIZABLE | 避免 | 避免 | 避免 |

## 工程对照

本项目的 `engineering/src/db/txn/` 实现了事务系统。`txn.c` 管理事务的
BEGIN/COMMIT/ROLLBACK 生命周期，通过 `txn_start()` 分配事务 ID，
`txn_commit()` 刷 WAL 日志并释放锁。`txn.c` 使用 PostgreSQL 风格的
四层隔离级别实现（参见 `guc.h` 中的 default_transaction_isolation 参数），
但当前默认实现是 Read Committed——语句级别快照。与本文 SQLite 演示不同的
是，工程实现需要自行管理锁表（`lock.c`）、版本链（`mvcc.c`）和日志。
