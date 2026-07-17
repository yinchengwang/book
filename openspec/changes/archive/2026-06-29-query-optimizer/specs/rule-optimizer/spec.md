# 规则优化器规格

## ADDED Requirements

### Requirement: 谓词下推

优化器 SHALL 将过滤条件下推到尽可能低的算子。

#### Scenario: 选择下推
- **WHEN** 执行 `SELECT * FROM t WHERE col = 10`
- **AND** t 有主键索引
- **THEN** 生成计划：IndexScan(condition: col=10)

#### Scenario: 多表连接下推
- **WHEN** 执行 `SELECT * FROM t1 JOIN t2 ON t1.id = t2.id WHERE t1.status = 'active'`
- **THEN** 计划为：HashJoin(t1.Filter(status='active'), t2)

#### Scenario: 聚合前过滤
- **WHEN** 执行 `SELECT COUNT(*) FROM t WHERE col > 100`
- **THEN** 过滤条件下推到扫描

### Requirement: 常量折叠

优化器 SHALL 在编译时计算常量表达式。

#### Scenario: 简单常量计算
- **WHEN** 执行 `SELECT 1 + 2, 'hello' || ' ' || 'world'`
- **THEN** 优化为 `SELECT 3, 'hello world'`

#### Scenario: 条件常量
- **WHEN** 执行 `SELECT * FROM t WHERE id = 5 AND 1 = 1`
- **THEN** 优化为 `SELECT * FROM t WHERE id = 5`

#### Scenario: 恒真条件消除
- **WHEN** 执行 `SELECT * FROM t WHERE 1 = 1 AND id > 0`
- **THEN** 优化为 `SELECT * FROM t WHERE id > 0`

### Requirement: 列裁剪

优化器 SHALL 只读取查询需要的列。

#### Scenario: 指定列查询
- **WHEN** 执行 `SELECT id, name FROM t`
- **THEN** 扫描只读取 id 和 name 列

#### Scenario: COUNT 优化
- **WHEN** 执行 `SELECT COUNT(*) FROM t`
- **THEN** 使用快速计数，不读取整行

### Requirement: 子查询展开

简单相关子查询 SHALL 被展开为连接。

#### Scenario: IN 子查询
- **WHEN** 执行 `SELECT * FROM t WHERE id IN (SELECT id FROM s)`
- **AND** 子查询简单（无聚合、无复杂条件）
- **THEN** 优化为 `SELECT t.* FROM t JOIN (SELECT DISTINCT id FROM s) s2 ON t.id = s2.id`

#### Scenario: EXISTS 子查询
- **WHEN** 执行 `SELECT * FROM t WHERE EXISTS (SELECT 1 FROM s WHERE s.id = t.id)`
- **THEN** 优化为 SEMI JOIN

### Requirement: 冗余消除

优化器 SHALL 消除冗余的算子。

#### Scenario: 重复过滤消除
- **WHEN** 执行 `SELECT * FROM (SELECT * FROM t WHERE x > 5) WHERE x > 5`
- **THEN** 优化为单层过滤

#### Scenario: 恒真过滤消除
- **WHEN** 执行 `SELECT * FROM t WHERE TRUE`
- **THEN** 消除过滤算子

#### Scenario: 无用投影消除
- **WHEN** 执行 `SELECT * FROM (SELECT a, b, c FROM t)`
- **THEN** 消除中间投影，直接输出 a, b, c