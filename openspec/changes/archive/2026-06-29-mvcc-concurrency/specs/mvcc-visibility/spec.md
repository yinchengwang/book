# MVCC 可见性判断规格

## ADDED Requirements

### Requirement: 基本可见性规则

版本对快照可见 SHALL 满足以下所有条件：
1. xmin 事务已提交
2. xmin < snapshot->xmax
3. xmin 不在活跃事务列表中
4. xmax = 0 或 xmax 事务未提交或 xmax 在活跃列表中或 xmax >= snapshot->xmax

#### Scenario: 正常可见版本
- **WHEN** 版本 V1 由已提交的旧事务创建，未被删除
- **THEN** V1 对新快照可见

#### Scenario: 未提交事务版本不可见
- **WHEN** 版本 V1 由活跃事务 T1 创建
- **AND** 快照 S1 包含 T1
- **THEN** V1 对 S1 不可见

#### Scenario: 已删除版本不可见
- **WHEN** 版本 V1 的 xmax 为已提交的删除事务
- **AND** xmax < snapshot->xmax
- **AND** xmax 不在活跃列表
- **THEN** V1 对快照不可见

#### Scenario: 自身事务版本始终可见
- **WHEN** 版本 V1 由当前事务创建
- **THEN** V1 对当前事务可见

### Requirement: 版本链可见性搜索

系统 SHALL 从版本链头部向下遍历，找到第一个可见版本。

#### Scenario: 第一个版本可见
- **WHEN** HEAD → V1(可见) → NULL
- **THEN** 返回 V1

#### Scenario: 需要跳过不可见版本
- **WHEN** HEAD → V2(xmax=活跃事务) → V1(可见) → NULL
- **THEN** 跳过 V2，返回 V1

#### Scenario: 无可见版本
- **WHEN** HEAD → V2(xmax=已提交) → V1(xmax=已提交) → NULL
- **THEN** 返回 NULL

### Requirement: 脏读检测

系统 SHALL 在尝试读取未提交版本时检测脏读并报错或重试。

#### Scenario: 脏读检测 - 返回错误
- **WHEN** 尝试读取未提交版本
- **THEN** 返回错误码或抛出异常

#### Scenario: 脏读检测 - 自动重试
- **WHEN** 读取未提交版本且配置为自动重试
- **THEN** 等待事务提交后重新读取

### Requirement: 写冲突检测

系统 SHALL 当两个事务尝试修改同一行时检测写冲突。

#### Scenario: 写写冲突 - 先提交者胜出
- **WHEN** 事务 T1 和 T2 同时修改同一行
- **AND** T1 先提交
- **THEN** T2 的修改被拒绝，回滚

#### Scenario: 写写冲突 - 后提交者重试
- **WHEN** T2 提交时检测到 T1 已提交
- **THEN** T2 执行 ROLLBACK，客户端可重试

### Requirement: 范围扫描可见性

范围扫描时，系统 SHALL 对每行进行可见性判断。

#### Scenario: 范围扫描过滤不可见行
- **WHEN** 执行 SELECT * FROM t WHERE id > 10
- **AND** 部分行已被删除或未提交
- **THEN** 只返回对快照可见的行