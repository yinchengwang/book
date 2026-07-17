# distributed-transaction Specification

## Purpose
TBD - created by archiving change mmdb-htap-evolution. Update Purpose after archive.
## Requirements
### Requirement: 两阶段提交（2PC）

系统 SHALL 实现两阶段提交协议。

#### Scenario: Prepare 阶段
- **WHEN** 协调者发起提交
- **WHEN** 所有参与者 SHALL 被要求 Prepare
- **THEN** 参与者 SHALL 锁定资源
- **THEN** 参与者 SHALL 投票 Yes 或 No

#### Scenario: Commit 阶段
- **WHEN** 所有参与者投票 Yes
- **WHEN** 协调者 SHALL 发起 Commit
- **THEN** 参与者 SHALL 提交事务
- **THEN** 锁 SHALL 被释放

#### Scenario: Abort 处理
- **WHEN** 任何参与者投票 No
- **WHEN** 协调者 SHALL 发起 Abort
- **THEN** 参与者 SHALL 回滚事务
- **THEN** 锁 SHALL 被释放

---

### Requirement: SAGA 模式

系统 SHALL 实现 SAGA 长事务模式。

#### Scenario: 正向操作
- **WHEN** 执行 SAGA 事务
- **WHEN** 各参与服务 SHALL 执行正向操作
- **THEN** 每个操作 SHALL 有对应的补偿操作

#### Scenario: 补偿回滚
- **WHEN** 某个操作失败
- **WHEN** 已执行的操作 SHALL 按逆序补偿
- **THEN** 系统 SHALL 恢复到一致状态

#### Scenario: SAGA 状态跟踪
- **WHEN** SAGA 事务执行中
- **WHEN** 状态 SHALL 被跟踪
- **THEN** 失败恢复 SHALL 能继续执行

---

### Requirement: 分布式 MVCC

系统 SHALL 实现分布式 MVCC。

#### Scenario: 全局事务 ID (TSO)
- **WHEN** 分配全局事务 ID
- **WHEN** 时间戳 SHALL 由 Timestamp Oracle 生成
- **THEN** 事务 ID SHALL 全局唯一
- **THEN** 事务 SHALL 按时间戳排序

#### Scenario: 分布式快照
- **WHEN** 创建分布式快照
- **WHEN** 快照 SHALL 包含所有分片的状态
- **THEN** 跨分片读一致性 SHALL 被保证

#### Scenario: 分布式提交
- **WHEN** 提交分布式事务
- **WHEN** 所有分片 SHALL 同步提交
- **THEN** 提交序列 SHALL 被保证

---

### Requirement: 分布式隔离级别

系统 SHALL 支持分布式环境下的隔离级别。

#### Scenario: 分布式 SI
- **WHEN** 使用 Snapshot Isolation
- **WHEN** 跨分片事务 SHALL 使用分布式快照
- **THEN** 隔离性 SHALL 被保证

#### Scenario: 分布式 SSI
- **WHEN** 使用 Serializable SI
- **WHEN** 跨分片谓词锁 SHALL 被协调
- **THEN** 可串行化 SHALL 被保证

---

### Requirement: 故障恢复

系统 SHALL 实现分布式事务的故障恢复。

#### Scenario: 协调者故障恢复
- **WHEN** 协调者在 Commit 前故障
- **WHEN** 参与者 SHALL 等待协调者恢复
- **THEN** 恢复后 SHALL 继续提交或回滚

#### Scenario: 参与者故障恢复
- **WHEN** 参与者在 Prepare 后故障
- **WHEN** 恢复后 SHALL 检查事务状态
- **THEN** 根据协调者决策 SHALL 完成提交或回滚

