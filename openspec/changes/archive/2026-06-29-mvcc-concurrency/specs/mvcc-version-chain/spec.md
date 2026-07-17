# MVCC 版本链规格

## ADDED Requirements

### Requirement: 行版本结构

数据库中的每一行数据都维护一个版本链，版本链 SHALL 包含以下字段：
- `xmin`: 创建该版本的事务 ID
- `xmax`: 删除该版本的事务 ID（0 表示未被删除）
- `data`: 行数据 payload
- `t_ctid`: 指向下一个版本的指针（页号 + 槽号）
- `t_xvac`: Undo 记录指针（用于 GC）

#### Scenario: 新建表时无版本
- **WHEN** 用户执行 CREATE TABLE t (id INT, name VARCHAR)
- **THEN** 表的每一行初始版本链为空

#### Scenario: 插入数据创建新版本
- **WHEN** 事务 T1 执行 INSERT INTO t VALUES (1, 'Alice')
- **THEN** 创建版本 V1，xmin=T1, xmax=0, data=(1,'Alice'), t_ctid 指向 NULL

#### Scenario: 更新数据创建新版本
- **WHEN** 事务 T2 执行 UPDATE t SET name='Bob' WHERE id=1
- **THEN** 创建版本 V2，xmin=T2, xmax=0, t_ctid 指向 V1
- **AND** V1 的 xmax 更新为 T2

#### Scenario: 删除数据标记版本
- **WHEN** 事务 T3 执行 DELETE FROM t WHERE id=1
- **THEN** 当前可见版本的 xmax 设置为 T3

### Requirement: 版本链遍历

系统 SHALL 支持从版本链头部遍历到尾部，找到对指定快照可见的版本。

#### Scenario: 遍历找到可见版本
- **WHEN** 快照包含事务 T1，但不包含 T2
- **AND** 版本链为 HEAD → V2(xmin=T2) → V1(xmin=T1) → NULL
- **THEN** 返回 V1 作为可见版本

#### Scenario: 无可见版本返回空
- **WHEN** 快照不包含事务 T1 和 T2
- **AND** 版本链为 HEAD → V2(xmin=T2) → V1(xmin=T1) → NULL
- **THEN** 返回空（行对当前快照不可见）

### Requirement: Undo 日志记录

每次修改行时，旧版本数据 SHALL 记录到 Undo 日志，供未来 GC 使用。

#### Scenario: 更新时记录 Undo
- **WHEN** 事务 T1 更新行 R 的 name 字段从 'Alice' 改为 'Bob'
- **THEN** Undo 日志记录 R 的旧值和指向旧 Undo 的指针

#### Scenario: 回滚时恢复 Undo
- **WHEN** 事务 T1 执行 ROLLBACK
- **THEN** 从 Undo 日志恢复行的旧值
- **AND** 标记新版本为已删除

### Requirement: 版本号分配

事务 ID SHALL 使用单调递增的整数，每个事务分配唯一的 ID。

#### Scenario: 顺序分配事务 ID
- **WHEN** 事务 T1 提交后，T2 开始
- **THEN** T2 的事务 ID 大于 T1

#### Scenario: 事务 ID 回绕检测
- **WHEN** 事务 ID 达到最大值
- **THEN** 系统报告错误，建议执行 VACUUM 后重启