# sql-planner Specification

## Purpose
TBD - created by archiving change mmdb-htap-evolution. Update Purpose after archive.
## Requirements
### Requirement: 逻辑计划生成

系统 SHALL 实现逻辑计划生成器，能够将 SQL AST 转换为逻辑执行计划。

#### Scenario: SELECT 逻辑计划
- **WHEN** 输入 SELECT 查询的 AST
- **THEN** Planner SHALL 输出 LogicalSelect 计划节点
- **THEN** 计划 SHALL 包含 SeqScan、Filter、Projection 等逻辑算子

#### Scenario: JOIN 逻辑计划
- **WHEN** 输入包含 JOIN 的查询
- **THEN** Planner SHALL 输出 LogicalJoin 节点
- **THEN** JOIN 类型（Inner/Left/Right）SHALL 被正确标记

#### Scenario: 聚合逻辑计划
- **WHEN** 输入包含 GROUP BY 的查询
- **THEN** Planner SHALL 输出 LogicalAgg 节点
- **THEN** 分组键 SHALL 被正确传递

---

### Requirement: 物理计划生成

系统 SHALL 实现物理计划生成器，能够将逻辑计划转换为物理执行计划。

#### Scenario: SeqScan vs IndexScan 选择
- **WHEN** 查询有 WHERE 条件且有索引
- **THEN** Planner SHALL 考虑使用 IndexScan 而非 SeqScan
- **THEN** 选择 SHALL 基于代价估算结果

#### Scenario: HashJoin vs NestedLoop 选择
- **WHEN** 查询包含 JOIN
- **THEN** Planner SHALL 根据数据量选择 HashJoin 或 NestedLoop
- **THEN** 大表 JOIN 小表 SHALL 选择 HashJoin

#### Scenario: 物理算子映射
- **WHEN** 生成物理计划
- **THEN** LogicalSeqScan SHALL 映射为 PhysSeqScan
- **THEN** LogicalIndexScan SHALL 映射为 PhysIndexScan
- **THEN** LogicalJoin SHALL 根据情况映射为 PhysHashJoin 或 PhysNestLoopJoin

---

### Requirement: 代价模型

系统 SHALL 实现代价模型，能够估算不同执行计划的代价。

#### Scenario: 统计信息收集
- **WHEN** Planner 需要估算代价
- **THEN** 系统 SHALL 能够获取表的行数（rowcount）
- **THEN** 系统 SHALL 能够获取列的基数（ndistinct）
- **THEN** 系统 SHALL 能够获取列的值分布（histogram）

#### Scenario: 代价估算
- **WHEN** 估算 SeqScan 的代价
- **THEN** 代价 SHALL 基于行数和每行成本计算
- **THEN** 估算 IndexScan 的代价
- **THEN** 代价 SHALL 基于索引高度、匹配行数和回表成本计算

#### Scenario: JOIN 代价估算
- **WHEN** 估算 HashJoin 的代价
- **THEN** 代价 SHALL 包括构建哈希表成本和探测成本
- **THEN** 代价 SHALL 考虑内存溢出到磁盘的可能性

---

### Requirement: 执行计划优化

系统 SHALL 实现执行计划优化规则。

#### Scenario: 谓词下推
- **WHEN** JOIN 条件包含可下推的谓词
- **THEN** 优化器 SHALL 将谓词下推到 JOIN 下方
- **THEN** 减少 JOIN 处理的数据量

#### Scenario: 列裁剪
- **WHEN** SELECT 只引用部分列
- **THEN** 优化器 SHALL 裁剪不需要的列
- **THEN** 减少 I/O 和内存开销

#### Scenario: 连接顺序优化
- **WHEN** 多表 JOIN
- **THEN** 优化器 SHALL 尝试不同的连接顺序
- **THEN** 选择总代价最小的顺序

#### Scenario: 物化视图改写
- **WHEN** 查询可以复用已有物化视图
- **THEN** 优化器 SHALL 自动改写查询使用物化视图
- **THEN** 减少重复计算

#### Scenario: 向量索引下推
- **WHEN** 查询包含 VECTOR_SEARCH 条件
- **THEN** 优化器 SHALL 将向量搜索下推到存储层
- **THEN** 减少数据传输量

---

### Requirement: 多模态计划生成

系统 SHALL 支持为多模态查询生成执行计划。

#### Scenario: 向量查询计划
- **WHEN** 查询包含 VECTOR_SEARCH
- **THEN** Planner SHALL 生成 VectorScan 物理算子
- **THEN** 过滤条件 SHALL 被正确组合

#### Scenario: 时序查询计划
- **WHEN** 查询包含 TIME_SERIES_AGG
- **THEN** Planner SHALL 生成 TimeSeriesAgg 物理算子
- **THEN** 时间窗口 SHALL 被正确解析

#### Scenario: 跨模型查询计划
- **WHEN** 查询涉及多个数据模型
- **THEN** Planner SHALL 为每个模型生成子计划
- **THEN** 子计划 SHALL 通过 Exchange 算子连接

