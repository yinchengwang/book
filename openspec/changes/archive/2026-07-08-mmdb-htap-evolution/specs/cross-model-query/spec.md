# 规格：cross-model-query

## ADDED Requirements

### Requirement: 跨模型查询概述

系统 SHALL 支持跨数据模型的联合查询。

#### Scenario: 向量 + 关系模型 JOIN
- **WHEN** 查询向量与关系表 JOIN
- **WHEN** `SELECT p.*, v.embedding FROM products p JOIN vectors v ON p.id = v.id`
- **THEN** 两个模型的关联 SHALL 被正确处理

#### Scenario: 图 + 关系模型 JOIN
- **WHEN** 查询图与关系表 JOIN
- **WHEN** `SELECT u.*, g.friends FROM users u JOIN graph_table(g) ON u.id = g.user_id`
- **THEN** 图数据 SHALL 被转换为关系格式

#### Scenario: 时序 + 关系模型 JOIN
- **WHEN** 查询时序与关系表 JOIN
- **WHEN** `SELECT d.name, t.temp FROM devices d JOIN timeseries t ON d.id = t.device_id`
- **THEN** 时间序列 SHALL 被正确关联

---

### Requirement: 跨模型执行计划

系统 SHALL 为跨模型查询生成执行计划。

#### Scenario: 执行计划生成
- **WHEN** 解析跨模型查询
- **WHEN** Planner SHALL 分析查询涉及的数据模型
- **THEN** 每个模型的子计划 SHALL 被生成
- **THEN** Exchange 算子 SHALL 连接子计划

#### Scenario: 执行顺序优化
- **WHEN** 跨模型查询
- **WHEN** Planner SHALL 确定执行顺序
- **THEN** 先执行过滤性强的子查询
- **THEN** 减少数据传递量

---

### Requirement: 异构模型 JOIN

系统 SHALL 支持异构数据模型的 JOIN 操作。

#### Scenario: 小表广播
- **WHEN** JOIN 一边数据量很小
- **THEN** 小表 SHALL 被广播到所有节点
- **THEN** 减少数据移动

#### Scenario: 分布式 JOIN
- **WHEN** JOIN 两边都是大表
- **WHEN** 数据分布在不同节点
- **THEN** Shuffle JOIN SHALL 被执行
- **THEN** 数据 SHALL 按 JOIN 键重新分区

---

### Requirement: 跨模型优化规则

系统 SHALL 实现跨模型查询优化规则。

#### Scenario: 谓词下推
- **WHEN** 过滤条件可以下推到某个模型
- **THEN** 过滤 SHALL 在该模型内部执行
- **THEN** 减少跨模型数据传输

#### Scenario: 列裁剪
- **WHEN** 只引用部分列
- **THEN** 不需要的列 SHALL 被裁剪
- **THEN** 减少数据传输量

#### Scenario: 物化视图复用
- **WHEN** 查询可以由物化视图满足
- **THEN** 物化视图 SHALL 被优先使用
- **THEN** 减少重复计算
