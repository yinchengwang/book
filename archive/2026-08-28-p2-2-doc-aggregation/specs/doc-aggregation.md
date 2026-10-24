# P2-2 文档聚合管道 规格

## 能力: doc-aggregation

文档聚合管道提供类 MongoDB 风格的链式文档处理能力。

### ADDED Requirements

#### Requirement: $match 过滤阶段
系统 SHALL 提供 `$match` Stage 用于按表达式过滤文档集合。

#### Requirement: $group 分组阶段
系统 SHALL 提供 `$group` Stage 用于按字段分组并执行累加器函数
（$sum、$avg、$count、$min、$max）。

#### Requirement: $sort 排序阶段
系统 SHALL 提供 `$sort` Stage 用于按字段升序或降序排序文档。

#### Requirement: $limit/$skip 分页阶段
系统 SHALL 提供 `$limit` 和 `$skip` Stage 用于结果集分页。

#### Requirement: 管道链式执行
系统 SHALL 支持多个 Stage 按序串联执行，前一个 Stage 的输出作为下一个 Stage 的输入。

#### Requirement: 表达式求值
系统 SHALL 支持 JSONPath 字段访问（点号分隔）和算术/字符串表达式求值。

#### Scenario: 完整管道执行
- **WHEN** 用户提交 `$match` → `$group` → `$sort` → `$limit` 管道
- **THEN** 系统依次执行每个 Stage，最终返回限制条数的有序分组结果

#### Scenario: 表达式求值
- **WHEN** 表达式引用 `meta.score` 字段
- **THEN** 系统从文档对应 JSONPath 路径读取数值并参与算术运算