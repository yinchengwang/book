# MVCC · 学习笔记

## 核心概念

1. **MVCC（Multi-Version Concurrency Control）**：每个写操作创建新版本，旧版本保留供并发读
2. **版本链**：同一行的多个版本通过指针串联，最新版本在数据页，旧版本在 Undo Log 回滚段
3. **ReadView**：事务开始时对系统状态的快照——哪些事务活跃、最小活跃 ID、最大活跃 ID
4. **快照隔离（Snapshot Isolation）**：事务读到的始终是事务开始时的数据版本
5. **可见性规则**：事务 ID < min(m_ids) 可见；事务 ID 在 m_ids 中不可见；大于 max(m_ids) 不可见

## MVCC 优势

| 对比项 | 基于锁的并发 | MVCC |
|--------|------------|------|
| 读写冲突 | 写阻塞读 | 读不阻塞写，写不阻塞读 |
| 读一致性 | 需要快照或可重复读 | 天然快照隔离 |
| 回滚 | 需要 undo 日志 | 版本链直接提供 undo |

## 工程对照

本项目的 `engineering/src/db/txn/mvcc.c` 实现了 MVCC 子系统。核心接口包括
`mvcc_begin()` 创建 ReadView、`mvcc_read()` 根据可见性规则返回可见版本、
`mvcc_write()` 创建新版本并链接版本链。实现与本文演示的核心差异在于：
工程实现使用 PostgreSQL 风格的事务 ID（txid）和 Slot 管理，通过 `MVCCSnapshot`
结构维护活跃事务列表，而本文使用 SQLite 的 WAL 模式来模拟效果。
