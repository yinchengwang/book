# 规格：row-level-lock

## ADDED Requirements

### Requirement: 行级锁实现

系统 SHALL 实现行级锁，支持细粒度并发控制。

#### Scenario: 获取行级写锁
- **WHEN** 执行 `SELECT ... FOR UPDATE`
- **THEN** 目标行 SHALL 被加写锁
- **THEN** 其他事务 SHALL 无法修改该行

#### Scenario: 获取行级读锁
- **WHEN** 执行 `SELECT ... FOR SHARE`
- **THEN** 目标行 SHALL 被加读锁
- **THEN** 其他事务可以读但不能修改

#### Scenario: 锁队列
- **WHEN** 多个事务竞争同一行锁
- **THEN** 等待事务 SHALL 排队
- **THEN** FIFO 顺序 SHALL 被保证

---

### Requirement: 锁模式

系统 SHALL 支持多种锁模式。

#### Scenario: AccessShareLock
- **WHEN** 普通 SELECT
- **THEN** AccessShareLock SHALL 被获取
- **THEN** 与 AccessExclusiveLock 互斥

#### Scenario: RowShareLock
- **WHEN** SELECT FOR UPDATE/SHARE
- **THEN** RowShareLock SHALL 被获取

#### Scenario: RowExclusiveLock
- **WHEN** INSERT/UPDATE/DELETE
- **THEN** RowExclusiveLock SHALL 被获取

#### Scenario: AccessExclusiveLock
- **WHEN** ALTER TABLE/DROP TABLE
- **THEN** AccessExclusiveLock SHALL 被获取
- **THEN** 阻塞所有其他锁

---

### Requirement: LockTag 唯一标识

系统 SHALL 实现 LockTag 来唯一标识锁对象。

#### Scenario: 锁对象标识
- **WHEN** 锁住某行
- **THEN** LockTag SHALL 包含：relfilenode, blocknum, offset
- **THEN** 唯一标识该行

#### Scenario: 锁粒度
- **WHEN** 需要锁住某行
- **THEN** 锁 SHALL 精确到行级别
- **THEN** 同表其他行 SHALL 不受影响

---

### Requirement: 死锁检测

系统 SHALL 实现死锁检测机制。

#### Scenario: 等待图构建
- **WHEN** 事务等待锁
- **THEN** Wait-For Graph SHALL 被构建
- **THEN** 节点表示事务，边表示等待关系

#### Scenario: 环检测
- **WHEN** Wait-For Graph 包含环
- **THEN** 死锁 SHALL 被检测
- **THEN** 环中的某个事务 SHALL 被回滚

#### Scenario: 死锁处理
- **WHEN** 检测到死锁
- **THEN** 最年轻的事务 SHALL 被回滚
- **THEN** 错误消息 SHALL 被返回

#### Scenario: 锁超时回滚
- **WHEN** 锁等待超过 `deadlock_timeout`
- **THEN** 等待事务 SHALL 被回滚
- **THEN** 错误 SHALL 被报告

---

### Requirement: 锁升级

系统 SHALL 支持锁升级以减少锁开销。

#### Scenario: 表级锁升级
- **WHEN** 单表行锁数量超过阈值
- **THEN** 行锁 SHALL 被升级为表锁
- **THEN** 减少锁结构数量

---

### Requirement: 谓词锁（SSI）

系统 SHALL 实现谓词锁以支持 SERIALIZABLE 隔离级别。

#### Scenario: KeyRange 锁
- **WHEN** 在 SERIALIZABLE 级别执行范围查询
- **THEN** 谓词锁 SHALL 被获取
- **THEN** 防止幻读

#### Scenario: SIREAD 跟踪
- **WHEN** 事务读取数据
- **THEN** SIREAD SHALL 被记录
- **THEN** 用于冲突检测

#### Scenario: 序列化异常检测
- **WHEN** 检测到不可序列化的执行
- **THEN** 事务 SHALL 被回滚
- **THEN** 错误码为 40001 (serialization_failure)

---

### Requirement: GUC 参数

系统 SHALL 提供锁相关的配置参数。

#### Scenario: lock_timeout 配置
- **WHEN** 设置 `lock_timeout = '1s'`
- **THEN** 锁等待超过 1 秒 SHALL 超时

#### Scenario: deadlock_timeout 配置
- **WHEN** 设置 `deadlock_timeout = '500ms'`
- **THEN** 死锁检测间隔 SHALL 为 500ms

#### Scenario: max_locks_per_transaction 配置
- **WHEN** 设置 `max_locks_per_transaction`
- **THEN** 每个事务的锁槽数量 SHALL 被限制
