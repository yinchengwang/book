# ts-continuous-agg Specification

## Purpose
TBD - created by archiving change mmdb-htap-evolution. Update Purpose after archive.
## Requirements
### Requirement: 连续聚合概述

系统 SHALL 实现时序连续聚合（Continuous Aggregate）。

#### Scenario: 创建连续聚合
- **WHEN** 执行 `CREATE CONTINUOUS AGGREGATE hourly_sales AS SELECT time_bucket('1h', ts), SUM(amount) FROM sales GROUP BY 1`
- **THEN** 物化视图 SHALL 被创建
- **THEN** 后台刷新 SHALL 被配置

#### Scenario: 实时增量更新
- **WHEN** 新数据被插入
- **WHEN** 触发器被激活
- **THEN** 增量变更 SHALL 被捕获
- **THEN** 物化视图 SHALL 被增量更新

#### Scenario: 自动刷新策略
- **WHEN** 配置刷新策略
- **THEN** 周期性刷新 SHALL 被执行
- **THEN** 刷新间隔 SHALL 可配置

---

### Requirement: 变更数据捕获

系统 SHALL 实现变更数据捕获（CDC）。

#### Scenario: WAL 日志捕获
- **WHEN** 数据被修改
- **THEN** 变更 SHALL 被记录到 CDC 日志
- **THEN** 物化视图订阅 SHALL 读取变更

#### Scenario: 增量计算
- **WHEN** 处理变更数据
- **THEN** 增量差值 SHALL 被计算
- **THEN** 物化视图 SHALL 被高效更新

---

### Requirement: 降采样策略

系统 SHALL 实现自动降采样。

#### Scenario: 多精度聚合
- **WHEN** 创建不同精度的连续聚合
- **THEN** 原始数据 SHALL 聚合为 1 小时精度
- **THEN** 1 小时精度 SHALL 聚合为 1 天精度

#### Scenario: 自动降采样
- **WHEN** 数据变老
- **THEN** 自动降采样 SHALL 被触发
- **THEN** 存储空间 SHALL 被节省

---

### Requirement: 连续聚合查询

系统 SHALL 支持连续聚合的查询优化。

#### Scenario: 查询重写
- **WHEN** 查询可以由连续聚合满足
- **THEN** 查询 SHALL 被自动改写
- **THEN** 直接查询物化视图 SHALL 被执行

#### Scenario: 时间范围选择
- **WHEN** 查询新数据
- **THEN** 直接查询原始数据 SHALL 被使用
- **WHEN** 查询历史数据
- **THEN** 查询物化视图 SHALL 被使用

