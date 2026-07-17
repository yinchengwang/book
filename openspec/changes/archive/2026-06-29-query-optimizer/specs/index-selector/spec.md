# 索引选择器规格

## ADDED Requirements

### Requirement: 索引可用性检测

优化器 SHALL 检测列是否有可用索引。

#### Scenario: 主键索引
- **WHEN** 表 t 有主键 id
- **AND** 查询 `WHERE id = 10`
- **THEN** 检测到可用索引

#### Scenario: 多列索引
- **WHEN** 表 t 有索引 (a, b)
- **AND** 查询 `WHERE a = 1 AND b = 2`
- **THEN** 检测到完整匹配索引

#### Scenario: 前缀匹配索引
- **WHEN** 表 t 有索引 (a, b)
- **AND** 查询 `WHERE a = 1`
- **THEN** 检测到前缀匹配索引

#### Scenario: 无可用索引
- **WHEN** 查询 `WHERE col LIKE '%abc'`
- **AND** 无表达式索引
- **THEN** 报告无可用索引

### Requirement: 索引匹配度评估

系统 SHALL 评估每个可用索引对查询的适用程度。

#### Scenario: 完全匹配
- **WHEN** 查询条件与索引完全匹配
- **AND** 无额外过滤
- **THEN** 匹配度 = 1.0 (100%)

#### Scenario: 部分匹配
- **WHEN** 查询 `WHERE a = 1 AND b > 2`
- **AND** 索引 (a, b)
- **THEN** 匹配度 = 0.9（a 精确匹配，b 范围）

#### Scenario: 无匹配
- **WHEN** 查询 `WHERE b = 1`
- **AND** 索引 (a, b)
- **THEN** 匹配度 = 0.0（无法使用）

### Requirement: 索引成本估算

系统 SHALL 估算使用索引的扫描成本。

#### Scenario: 唯一索引点查
- **WHEN** 使用唯一索引等值查询
- **AND** 索引深度 = 3
- **THEN** 成本 = 3 × random_page_cost + 1 × cpu_tuple_cost

#### Scenario: 范围扫描
- **WHEN** 索引范围查询返回 100 行
- **AND** 索引页 = 5, 表页 = 100
- **THEN** 成本 = 5 × random_page_cost + 100 × random_page_cost

#### Scenario: 回表成本
- **WHEN** 索引覆盖不完整，需要回表
- **THEN** 成本 = 索引扫描成本 + 回表页数 × random_page_cost

### Requirement: 多索引选择

当有多个可用索引时，系统 SHALL 选择最优的。

#### Scenario: 单列 vs 多列
- **WHEN** 查询 `WHERE a = 1 AND b = 2`
- **AND** 有单列索引 (a) 和 (b)
- **AND** 有多列索引 (a, b)
- **THEN** 选择多列索引 (a, b)

#### Scenario: 多索引组合
- **WHEN** 查询 `WHERE a = 1 AND c = 3`
- **AND** 有索引 (a) 和 (c)
- **THEN** 考虑索引组合或全表扫描

#### Scenario: 索引成本比较
- **WHEN** 多个索引可用
- **AND** 每个索引成本不同
- **THEN** 选择成本最低的索引

### Requirement: 索引提示

系统 SHALL 支持用户指定的索引提示。

#### Scenario: 强制使用索引
- **WHEN** 查询 `SELECT * FROM t USE INDEX (idx_name) WHERE col = 1`
- **THEN** 优先使用指定的索引

#### Scenario: 忽略索引
- **WHEN** 查询 `SELECT * FROM t IGNORE INDEX (idx_name) WHERE col = 1`
- **THEN** 不使用指定的索引