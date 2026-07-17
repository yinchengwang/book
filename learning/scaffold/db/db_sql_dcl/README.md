# SQL DCL 事务控制语言学习卡

## 简介

本学习卡演示 SQL DCL（Data Control Language，事务控制语言）的核心概念，包括事务的开始、提交、回滚，以及 Savepoint 和 Autocommit 机制。

## SQL 事务控制命令

| 命令 | 说明 |
|------|------|
| `BEGIN` / `START TRANSACTION` | 开始一个新事务 |
| `COMMIT` | 提交事务，使所有更改永久生效 |
| `ROLLBACK` | 回滚事务，撤销所有未提交的更改 |
| `SAVEPOINT name` | 在事务内创建命名保存点 |
| `ROLLBACK TO SAVEPOINT name` | 回滚到指定的 Savepoint |
| `SET AUTOCOMMIT = ON/OFF` | 开启/关闭自动提交模式 |

## 编译与运行

```bash
# 编译
make

# 运行
make run
# 或直接执行
./sql_dcl
```

## 学习要点

### 1. 事务的 ACID 特性
- **Atomicity（原子性）**：事务是最小执行单元，要么全部成功，要么全部失败
- **Consistency（一致性）**：事务执行前后，数据库状态保持一致
- **Isolation（隔离性）**：并发事务之间相互隔离
- **Durability（持久性）**：提交后的更改永久保存在磁盘上

### 2. Savepoint 的作用
Savepoint 允许在事务内部创建中间标记，实现部分回滚，而无需放弃整个事务。例如：

```sql
BEGIN;
INSERT INTO orders VALUES (1, 'Book');
SAVEPOINT after_first_insert;
INSERT INTO orders VALUES (2, 'Pen');
ROLLBACK TO SAVEPOINT after_first_insert;  -- 只撤销第二条
COMMIT;  -- 只提交第一条
```

### 3. Autocommit 模式
- `AUTOCOMMIT = ON`：每条 SQL 语句自动在独立事务中执行并提交
- `AUTOCOMMIT = OFF`（默认）：需要显式 BEGIN 开始事务，COMMIT 提交

### 4. 嵌套 Savepoint
大多数数据库支持多个 Savepoint，形成链表结构。回滚到某个 Savepoint 会删除之后创建的所有 Savepoint。

## 代码结构

```
db_sql_dcl/
├── main.c    # 事务控制演示实现
├── Makefile  # 编译配置
├── README.md # 本文档
└── NOTES.md  # 工程对照笔记
```
