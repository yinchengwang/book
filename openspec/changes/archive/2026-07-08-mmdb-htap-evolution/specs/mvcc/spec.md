# 规格：mvcc

## ADDED Requirements

### Requirement: MVCC 多版本并发控制

系统 SHALL 实现 MVCC 多版本并发控制，支持读写不阻塞。

#### Scenario: 元组版本链
- **WHEN** 执行 UPDATE
- **THEN** 旧版本元组 SHALL 被保留
- **THEN** 新版本元组 SHALL 被创建
- **THEN** 旧版本 SHALL 指向新版本（通过 ctid）

#### Scenario: 元组可见性判断
- **WHEN** 读取元组
- **THEN** 系统 SHALL 根据 ReadView 判断元组是否可见
- **THEN** 可见的元组 SHALL 被返回
- **THEN** 不可见的元组 SHALL 被跳过

#### Scenario: 版本清理
- **WHEN** 旧版本不再被任何事务需要
- **THEN** VACUUM SHALL 清理过期版本
- **THEN** 释放的空间 SHALL 被重用

---

### Requirement: 事务 ID 管理

系统 SHALL 实现事务 ID 管理。

#### Scenario: 事务 ID 分配
- **WHEN** 开始新事务
- **THEN** 新事务 SHALL 被分配唯一的事务 ID
- **THEN** 事务 ID SHALL 递增分配

#### Scenario: 事务状态跟踪
- **WHEN** 事务状态变化
- **THEN** 活动事务 SHALL 被跟踪
- **THEN** 已提交事务 SHALL 记录提交时间
- **THEN** 已回滚事务 SHALL 被标记

#### Scenario: XID Wraparound 防护
- **WHEN** 事务 ID 接近上限
- **THEN** 系统 SHALL 执行 Freeze 处理
- **THEN** 防止 XID 环绕问题

---

### Requirement: ReadView 生成

系统 SHALL 实现 ReadView（读视图）生成。

#### Scenario: Read Committed 隔离级别
- **WHEN** 设置隔离级别为 READ COMMITTED
- **THEN** 每个语句 SHALL 生成新的 ReadView
- **THEN** 其他事务的提交 SHALL 对当前语句可见

#### Scenario: Snapshot Isolation 隔离级别
- **WHEN** 设置隔离级别为 REPEATABLE READ 或 SERIALIZABLE
- **THEN** 事务开始时 SHALL 生成 ReadView
- **THEN** 整个事务 SHALL 使用相同的 ReadView

#### Scenario: ReadView 结构
- **WHEN** 创建 ReadView
- **THEN** ReadView SHALL 包含 xmin（最小活跃事务 ID）
- **THEN** ReadView SHALL 包含 xmax（最大已分配事务 ID）
- **THEN** ReadView SHALL 包含活跃事务列表

---

### Requirement: 隔离级别支持

系统 SHALL 支持多种事务隔离级别。

#### Scenario: READ COMMITTED 实现
- **WHEN** 事务以 READ COMMITTED 级别运行
- **THEN** 只能读取已提交的数据
- **THEN** 每个语句看到的数据是一致的

#### Scenario: REPEATABLE READ 实现
- **WHEN** 事务以 REPEATABLE READ 级别运行
- **THEN** 事务期间看到的数据是一致的
- **THEN** 避免脏读和不可重复读

#### Scenario: SERIALIZABLE 实现
- **WHEN** 事务以 SERIALIZABLE 级别运行
- **THEN** 通过谓词锁避免幻读
- **THEN** 冲突事务 SHALL 被回滚

---

### Requirement: MVCC 与 WAL 集成

系统 SHALL 实现 MVCC 与 WAL 的深度集成。

#### Scenario: WAL 记录版本信息
- **WHEN** 写入数据
- **THEN** WAL SHALL 记录事务 ID 信息
- **THEN** WAL SHALL 记录元组版本链信息

#### Scenario: WAL 回放可见性判断
- **WHEN** 从 WAL 恢复数据
- **THEN** 系统 SHALL 进行可见性判断
- **THEN** 只恢复可见的版本

#### Scenario: 检查点与 MVCC
- **WHEN** 创建检查点
- **THEN** 检查点 SHALL 包含 MVCC 相关信息
- **THEN** 恢复时 SHALL 能正确重建 MVCC 状态

---

### Requirement: VACUUM 垃圾回收

系统 SHALL 实现过期版本的垃圾回收。

#### Scenario: 过期版本识别
- **WHEN** 执行 VACUUM
- **THEN** 不再可见的版本 SHALL 被识别
- **THEN** 死元组 SHALL 被标记为可复用

#### Scenario: Freeze 处理
- **WHEN** 元组的事务 ID 太老
- **THEN** 元组 SHALL 被 Freeze
- **THEN** FrozenXmin SHALL 被更新

#### Scenario: VACUUM 触发
- **WHEN** 死元组数量超过阈值
- **THEN** VACUUM SHALL 被自动触发
- **THEN** 后台 VACUUM SHALL 与前台操作并发
