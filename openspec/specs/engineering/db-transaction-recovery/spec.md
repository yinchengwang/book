# DB 事务与恢复深度内容

## Purpose

为数据库事务与恢复机制提供深度技术文章，涵盖 MVCC 多版本并发控制、InnoDB 锁机制、Redo Log 详解、ARIES 恢复算法及分布式共识等核心知识点，每个知识点对应一篇 8 段式深度文章。

## Requirements

### Requirement: DB 事务与恢复深度文章

DB 事务与恢复的每篇深度文章 SHALL 覆盖以下知识点（选取核心 ~5 篇）：

| 知识点 ID | 标题 | 难度 |
|-----------|------|------|
| `db-mvcc-principle` | MVCC 多版本并发控制原理 | intermediate |
| `db-locking` | InnoDB 锁机制详解 | intermediate |
| `db-redo-log-detail` | Redo Log 详解与组提交 | intermediate |
| `db-aries` | ARIES 恢复算法 | advanced |
| `db-consensus` | 分布式共识 Raft/Paxos | advanced |

每篇文章 SHALL 遵循 8 段式模版。

#### 特殊要求：事务/恢复文章 SHALL 额外包含

- 隔离级别与异常现象的 SQL 演示示例
- Lock/Redo/Undo 的协作关系图
- 分布式共识的核心阶段对比（Raft vs Paxos）
- 面试高频追问点（MVCC 可见性判断、死锁检测、ARIES 三阶段）

#### Scenario: 事务文章覆盖面

- **WHEN** 用户阅读 `db-mvcc-principle`
- **THEN** 文章 SHALL 包含版本链结构（DB_TRX_ID/DB_ROLL_PTR）的图示、快照读 vs 当前读的区别、可见性判断算法
