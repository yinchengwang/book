# MVCC 快照规格

## ADDED Requirements

### Requirement: 快照数据结构

事务快照 SHALL 包含以下信息：
- `xmin`: 快照创建时的最小活动事务 ID
- `xmax`: 快照创建时的最大已分配事务 ID
- `xip_list`: 快照创建时活跃的事务 ID 列表

#### Scenario: 创建新快照
- **WHEN** 事务 T1 开始并执行 SELECT
- **THEN** 创建快照 S1，xmin=当前 xmin, xmax=当前 xmax, xip_list=当前活跃事务

#### Scenario: 快照在事务期间不变
- **WHEN** 事务 T1 在执行多个 SELECT
- **THEN** 所有 SELECT 使用同一个快照 S1

### Requirement: 快照获取

快照在事务开始时获取，并 SHALL 保持不变直到事务结束。

#### Scenario: 读取已提交快照
- **WHEN** 会话配置为 READ COMMITTED
- **THEN** 每个语句执行前重新获取快照

#### Scenario: 读取一致性快照
- **WHEN** 会话配置为 REPEATABLE READ 或 SERIALIZABLE
- **THEN** 事务开始时获取快照，整个事务期间使用同一快照

### Requirement: 快照序列化

快照信息 SHALL 可以序列化和反序列化，用于 WAL 恢复。

#### Scenario: 序列化快照
- **WHEN** 事务在检查点时处于活跃状态
- **THEN** 快照数据写入检查点文件

#### Scenario: 反序列化恢复快照
- **WHEN** 系统崩溃后恢复
- **AND** 读取检查点文件
- **THEN** 从检查点恢复活跃事务的快照

### Requirement: 快照可见性判断接口

系统 SHALL 提供函数判断给定版本对快照是否可见。

```c
bool mvcc_version_visible(const mvcc_snapshot_t *snapshot,
                          int64_t xmin,
                          int64_t xmax);
```

#### Scenario: 事务内自身修改可见
- **WHEN** 快照事务 ID = xmin
- **THEN** 版本可见

#### Scenario: 已提交事务创建可见
- **WHEN** xmin < snapshot->xmax 且 xmin 不在活跃列表
- **THEN** 版本可见

#### Scenario: 活跃事务删除不可见
- **WHEN** xmax != 0 且 xmax 在活跃列表或 xmax >= snapshot->xmax
- **THEN** 版本不可见

#### Scenario: 已提交事务删除不可见
- **WHEN** xmax != 0 且 xmax < snapshot->xmax 且 xmax 不在活跃列表
- **THEN** 版本不可见（已被删除）