# graph-query Specification

## Purpose
TBD - created by archiving change mmdb-htap-evolution. Update Purpose after archive.
## Requirements
### Requirement: 图查询语言概述

系统 SHALL 实现图查询语言支持。

#### Scenario: MATCH 子句
- **WHEN** 执行 `MATCH (a:Person) RETURN a`
- **THEN** 所有 Person 标签的顶点 SHALL 被返回

#### Scenario: 模式匹配
- **WHEN** 执行 `MATCH (a)-[r]->(b) RETURN a, b`
- **THEN** 匹配的路径 SHALL 被返回

#### Scenario: WHERE 子句
- **WHEN** 执行 `MATCH (a:Person) WHERE a.age > 30 RETURN a`
- **THEN** 过滤条件 SHALL 被应用

---

### Requirement: openCypher 兼容语法

系统 SHALL 支持 openCypher 的核心语法子集。

#### Scenario: 顶点模式
- **WHEN** 使用 `(n:Label)`
- **THEN** 顶点 SHALL 被匹配
- **THEN** 标签 SHALL 被过滤

#### Scenario: 边模式
- **WHEN** 使用 `[r:REL_TYPE]`
- **THEN** 边 SHALL 被匹配
- **THEN** 边类型 SHALL 被过滤

#### Scenario: 可变边
- **WHEN** 使用 `[*1..3]`
- **THEN** 1 到 3 跳的路径 SHALL 被匹配
- **WHEN** 使用 `[*]`
- **THEN** 任意长度的路径 SHALL 被匹配

#### Scenario: 方向约束
- **WHEN** 使用 `->`
- **THEN** 有向边 SHALL 被匹配
- **WHEN** 使用 `-`
- **THEN** 无向边 SHALL 被匹配

---

### Requirement: RETURN 子句

系统 SHALL 支持 RETURN 子句。

#### Scenario: 返回顶点
- **WHEN** `RETURN n`
- **THEN** 顶点属性 SHALL 被返回

#### Scenario: 返回表达式
- **WHEN** `RETURN count(n)`
- **THEN** 聚合结果 SHALL 被返回

#### Scenario: DISTINCT 去重
- **WHEN** `RETURN DISTINCT n.name`
- **THEN** 重复值 SHALL 被去除

#### Scenario: ORDER BY
- **WHEN** `RETURN n ORDER BY n.age`
- **THEN** 结果 SHALL 按指定列排序

#### Scenario: LIMIT
- **WHEN** `RETURN n LIMIT 10`
- **THEN** 结果 SHALL 被限制为 10 条

---

### Requirement: 聚合查询

系统 SHALL 支持图聚合查询。

#### Scenario: COUNT 聚合
- **WHEN** `MATCH (a)-[r]->(b) RETURN count(r)`
- **THEN** 边计数 SHALL 被返回

#### Scenario: GROUP BY 聚合
- **WHEN** `MATCH (a)-[r]->(b) RETURN a.name, count(*) GROUP BY a.name`
- **THEN** 按顶点分组的计数 SHALL 被返回

#### Scenario: 聚合函数
- **WHEN** 使用聚合函数
- **THEN** COUNT, SUM, AVG, MIN, MAX SHALL 被支持

---

### Requirement: SQL 扩展函数

系统 SHALL 提供图 SQL 扩展函数。

#### Scenario: GRAPH_TRAVERSE 函数
- **WHEN** 执行 `SELECT GRAPH_TRAVERSE('social', start_id, depth => 3)`
- **THEN** 从起点开始的遍历结果 SHALL 被返回

#### Scenario: GRAPH_SHORTEST_PATH 函数
- **WHEN** 执行 `SELECT GRAPH_SHORTEST_PATH('social', src, dst)`
- **THEN** 最短路径 SHALL 被计算
- **THEN** 路径 SHALL 被返回

#### Scenario: GRAPH_NEIGHBORS 函数
- **WHEN** 执行 `SELECT GRAPH_NEIGHBORS('social', node_id, depth => 2)`
- **THEN** 邻居顶点 SHALL 被返回

---

### Requirement: 路径查询

系统 SHALL 支持路径相关查询。

#### Scenario: 最短路径
- **WHEN** 查询两点间最短路径
- **THEN** 最短路径 SHALL 被找到
- **THEN** 路径长度 SHALL 被最小化

#### Scenario: 所有简单路径
- **WHEN** 查询所有简单路径
- **THEN** 不包含重复顶点的路径 SHALL 被返回
- **THEN** 路径数量 SHALL 可限制

#### Scenario: 路径长度约束
- **WHEN** 查询指定长度的路径
- **THEN** 指定跳数的路径 SHALL 被匹配

