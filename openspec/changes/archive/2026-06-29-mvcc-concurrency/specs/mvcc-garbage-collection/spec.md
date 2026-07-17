# MVCC 垃圾回收规格

## ADDED Requirements

### Requirement: VACUUM 触发条件

当满足以下任一条件时，系统 SHALL 触发 VACUUM：
- 死亡元组数量超过阈值（表页面数的 20%）
- 手动执行 VACUUM 命令
- 后台定时任务触发

#### Scenario: 自动触发 VACUUM
- **WHEN** 表的死亡元组比例超过 20%
- **THEN** 后台 VACUUM 进程开始清理

#### Scenario: 手动触发 VACUUM
- **WHEN** 用户执行 VACUUM table_name
- **THEN** 立即对指定表执行清理

#### Scenario: 批量 VACUUM
- **WHEN** 用户执行 VACUUM (FULL)
- **THEN** 对表进行紧凑清理，可能需要表锁

### Requirement: 版本回收条件

版本 SHALL 在满足以下所有条件时可以被回收：
1. 版本已被删除（xmax != 0）
2. xmax 事务已提交
3. 没有活跃事务的 snapshot->xmin <= xmax
4. 版本不在任何活跃事务的版本链中

#### Scenario: 安全回收已删除旧版本
- **WHEN** 版本 V1 的 xmax=已提交的 T1
- **AND** 所有活跃事务的 xmin > T1
- **THEN** V1 可以被回收

#### Scenario: 保护活跃事务可见版本
- **WHEN** 快照 S1 包含事务 T1
- **AND** V1 的 xmin = T1
- **THEN** 即使 V1 已被删除，也不能回收

#### Scenario: 保护自身事务版本
- **WHEN** 当前事务正在修改某行
- **THEN** 该行的最新版本不能被回收

### Requirement: Undo 日志回收

系统 SHALL 回收不再需要的 Undo 日志段。

#### Scenario: 回收已处理 Undo
- **WHEN** Undo 段的所有记录都已应用（无活跃事务需要）
- **THEN** Undo 段可以被覆写

#### Scenario: 保留长事务 Undo
- **WHEN** 存在运行时间超过 1 小时的长事务
- **THEN** 保留该事务相关的所有 Undo

### Requirement: 增量 GC

VACUUM SHALL 分批执行，每次处理部分页面，避免长时间阻塞。

#### Scenario: 增量清理页面
- **WHEN** 执行 VACUUM
- **THEN** 每次处理若干页面（如 100 个）
- **AND** 可以随时中断和继续

#### Scenario: GC 进度记录
- **WHEN** VACUUM 中断
- **THEN** 记录已处理的页面位置
- **AND** 下次 VACUUM 从中断处继续

### Requirement: GC 配置参数

系统 SHALL 提供配置项控制 GC 行为：
- `vacuum_threshold`: 触发 VACUUM 的最小死亡元组数
- `vacuum_cost_delay`: GC 的成本延迟（毫秒）
- `autovacuum_naptime`: 自动 VACUUM 间隔（秒）

#### Scenario: 配置最小阈值
- **WHEN** 设置 vacuum_threshold = 1000
- **THEN** 只有死亡元组超过 1000 时才触发自动 VACUUM

#### Scenario: 配置 GC 延迟
- **WHEN** 设置 vacuum_cost_delay = 20
- **AND** GC 累积成本达到阈值
- **THEN** GC 暂停 20 毫秒