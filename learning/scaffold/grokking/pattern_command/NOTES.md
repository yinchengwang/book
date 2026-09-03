# 命令模式 — 工程对比笔记

## 关键概念

| 概念 | 本例 | 数据库工程对照 |
|------|------|---------------|
| 命令封装 | InsertCommand / DeleteCommand | txn_begin / txn_commit / txn_rollback |
| 撤销 | undo() | txn_rollback → MVCC 回滚段 |
| 重做 | redo() | WAL redo → 前镜像重放 |
| 命令历史 | CommandHistory (undo_stack / redo_stack) | WAL 日志段序列 |

## 工程对照

### 1. 数据库事务 (engineering/src/db/txn/)

数据库事务本身就是命令模式的经典应用：
- `txn_begin()` = 创建 Command 对象，记录当前快照
- `txn_commit()` = execute()，将事务所有修改写入 WAL
- `txn_rollback()` = undo()，利用 MVCC 回滚段或 shadow page 恢复旧版本

每种 DML 操作（INSERT/UPDATE/DELETE）都是一个 ConcreteCommand，事务管理器（TransactionManager）充当 Invoker，管理命令的生命周期。

### 2. WAL 日志 (engineering/src/db/core/wal.c)

WAL（Write-Ahead Logging）机制与命令模式高度吻合：
- 每个 WAL Record 就是一个序列化后的命令，记录 page_id、offset、old_data（undo）和 new_data（redo）
- Redo 阶段：按 LSN 顺序回放 WAL Record，相当于 execute()
- Undo 阶段：逆序应用 old_data，回到事务开始前的状态
- Checkpoint 相当于命令历史的快照机制，避免无限增长

### 3. 对比差异

| 维度 | 命令模式 | 数据库 WAL |
|------|---------|-----------|
| 持久化 | 内存栈 | 磁盘日志文件 |
| 并发 | 顺序执行 | 支持并发事务，WAL 锁管理 |
| 粒度 | 字符/行级 | 页面级（8KB page） |
| 崩溃恢复 | 不支持 | 支持（通过 redo/undo） |

## 面试要点

1. **命令模式 vs 策略模式**：命令模式封装"动作+参数"，支持撤销；策略模式封装"算法"，可替换。一个操作，一个算法。
2. **命令队列的应用**：线程池、消息队列、操作日志审计系统。
3. **宏命令**：CompositeCommand 组合多个子命令，支持批量撤销。
4. **与函数式编程关系**：Python 的 `functools.partial` 或 lambda 可以替代简单命令，但复杂命令（需 undo、状态追踪）仍需类实现。
