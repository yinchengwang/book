# Undo Log 学习卡

## 简介

Undo Log（回滚日志）是数据库事务系统的重要组成部分，用于在事务失败时将数据恢复到事务开始前的状态。

### 核心原理

1. **事务回滚**：当事务执行失败或用户主动回滚时，利用 Undo Log 中的旧数据恢复被修改的记录
2. **MVCC 支持**：在多版本并发控制中，Undo Log 用于构建数据的历史版本链
3. **页面恢复**：数据库崩溃恢复时，Undo Log 可用于回滚未提交事务的修改

### Undo Log 记录结构

| 字段 | 说明 |
|------|------|
| txn_id | 事务 ID，标识该记录属于哪个事务 |
| type | 操作类型（INSERT/UPDATE/DELETE） |
| page_id | 所属页面号，用于定位修改位置 |
| offset | 页面内偏移量，定位具体记录 |
| old_data | 旧数据副本（UPDATE/DELETE 时有效） |

### 回滚策略

- **INSERT 回滚**：物理删除该行记录
- **UPDATE 回滚**：用 old_data 覆盖当前行数据
- **DELETE 回滚**：用 old_data 重建该行记录

## 编译与运行

```bash
# 编译
make

# 运行
make run

# 清理
make clean
```

## 学习要点

1. 理解 Undo Log 在 ACID 特性中 Atomicity 的作用
2. 掌握三种操作类型的回滚策略差异
3. 了解 Undo Log 与 MVCC 版本链的关系
4. 理解崩溃恢复中 Undo 的执行时机

## 进阶阅读

- `engineering/src/db/concurrency/mvcc_version.c` - MVCC Undo 实现
- `engineering/src/db/storage/wal/wal.c` - WAL 的 undo 支持
