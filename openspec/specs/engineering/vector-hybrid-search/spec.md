# vector-hybrid-search Specification

## Purpose
TBD - created by archiving change mmdb-htap-evolution. Update Purpose after archive.
## Requirements
### Requirement: 混合检索概述

系统 SHALL 实现向量混合检索，支持向量相似度与属性过滤的组合。

#### Scenario: 基础混合搜索
- **WHEN** 执行 `VECTOR_SEARCH(embedding, query, k, filter)`
- **THEN** 向量搜索 SHALL 与过滤条件组合
- **THEN** 只返回满足过滤条件的向量

#### Scenario: 多条件过滤
- **WHEN** 过滤条件包含多个属性
- **THEN** 所有条件 SHALL 同时满足
- **THEN** AND 逻辑 SHALL 被应用

#### Scenario: OR 条件过滤
- **WHEN** 过滤条件包含 OR
- **THEN** 任一条件满足 SHALL 被返回

---

### Requirement: Pre-filter 策略

系统 SHALL 实现 Pre-filter 过滤策略。

#### Scenario: 预过滤执行
- **WHEN** 使用 pre-filter 策略
- **THEN** 过滤 SHALL 在向量搜索前执行
- **THEN** 只对符合条件的向量进行相似度计算

#### Scenario: 预过滤优缺点
- **WHEN** 过滤条件选择性强
- **THEN** Pre-filter SHALL 效率高
- **WHEN** 过滤条件选择性弱
- **THEN** Pre-filter SHALL 效率低

---

### Requirement: Post-filter 策略

系统 SHALL 实现 Post-filter 过滤策略。

#### Scenario: 后过滤执行
- **WHEN** 使用 post-filter 策略
- **THEN** 向量搜索 SHALL 先执行
- **THEN** 过滤 SHALL 在搜索后应用

#### Scenario: 后过滤优缺点
- **WHEN** 过滤条件选择性弱
- **THEN** Post-filter SHALL 效率高
- **WHEN** 过滤条件选择性强
- **THEN** Post-filter SHALL 可能需要搜索更多向量

---

### Requirement: 自动策略选择

系统 SHALL 根据过滤条件自动选择最优策略。

#### Scenario: 自适应策略
- **WHEN** 执行混合搜索
- **WHEN** 系统 SHALL 分析过滤条件的选择性
- **THEN** 自动选择 pre-filter 或 post-filter

#### Scenario: Hint 覆盖
- **WHEN** 用户指定 hint（如 `/*+ FILTER_STRATEGY(pre) */`）
- **THEN** 用户策略 SHALL 被优先使用

---

### Requirement: SQL 扩展函数

系统 SHALL 提供 VECTOR_SEARCH SQL 扩展函数。

#### Scenario: 基本向量搜索
- **WHEN** 执行 `SELECT * FROM vectors ORDER BY VECTOR_DISTANCE(embedding, query_vec) LIMIT 10`
- **THEN** 向量搜索 SHALL 被执行
- **THEN** 结果 SHALL 按距离排序

#### Scenario: 带过滤的向量搜索
- **WHEN** 执行 `VECTOR_SEARCH(embedding, query_vec, k => 10, filter => 'category = "electronics"')`
- **THEN** 过滤 SHALL 被应用
- **THEN** 结果 SHALL 只包含匹配项

#### Scenario: 距离计算
- **WHEN** 使用 VECTOR_DISTANCE 函数
- **THEN** L2 距离 SHALL 被计算（默认）
- **THEN** COSINE 距离 SHALL 可选

---

### Requirement: 过滤条件语法

系统 SHALL 支持丰富的过滤条件语法。

#### Scenario: 比较运算符
- **WHEN** 过滤条件使用 `=`, `<>`, `<`, `>`, `<=`, `>=`
- **THEN** 比较 SHALL 被正确处理

#### Scenario: IN 条件
- **WHEN** 过滤条件使用 `IN (value1, value2, ...)`
- **THEN** 任一匹配 SHALL 被返回

#### Scenario: BETWEEN 条件
- **WHEN** 过滤条件使用 `BETWEEN low AND high`
- **THEN** 范围匹配 SHALL 被处理

#### Scenario: LIKE 条件
- **WHEN** 过滤条件使用 `LIKE pattern`
- **THEN** 模式匹配 SHALL 被支持

#### Scenario: NULL 检查
- **WHEN** 过滤条件使用 `IS NULL` 或 `IS NOT NULL`
- **THEN** NULL 值检查 SHALL 被处理

---

### Requirement: 性能优化

系统 SHALL 实现混合搜索的性能优化。

#### Scenario: 索引裁剪
- **WHEN** 过滤条件有索引支持
- **THEN** 索引 SHALL 被用于过滤
- **THEN** 搜索范围 SHALL 被减少

#### Scenario: 批处理
- **WHEN** 执行多个混合搜索
- **THEN** 批处理 SHALL 被应用
- **THEN** 减少网络往返

#### Scenario: 结果缓存
- **WHEN** 相同搜索被重复执行
- **THEN** 结果 SHALL 被缓存
- **THEN** 响应时间 SHALL 减少

