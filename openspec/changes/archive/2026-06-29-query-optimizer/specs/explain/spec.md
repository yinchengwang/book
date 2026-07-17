# EXPLAIN 执行计划规格

## ADDED Requirements

### Requirement: EXPLAIN 命令支持

系统 SHALL 支持 EXPLAIN 命令显示执行计划。

#### Scenario: 基本 EXPLAIN
- **WHEN** 用户执行 `EXPLAIN SELECT * FROM t WHERE id = 1`
- **THEN** 返回执行计划的文本表示

#### Scenario: EXPLAIN ANALYZE
- **WHEN** 用户执行 `EXPLAIN ANALYZE SELECT * FROM t WHERE id = 1`
- **THEN** 返回计划并实际执行，显示实际代价

#### Scenario: EXPLAIN FORMAT
- **WHEN** 用户执行 `EXPLAIN (FORMAT JSON) SELECT * FROM t`
- **THEN** 返回 JSON 格式的计划

### Requirement: 计划树结构

执行计划 SHALL 以树形结构组织。

```
Plan
└── Seq Scan
    └── Filter: id > 10
```

#### Scenario: 简单扫描计划
```
-> Seq Scan on orders
    Filter: (status = 'pending')
    Rows out: 100
```

#### Scenario: 连接计划
```
-> Hash Join
    -> Seq Scan on customers
    -> Hash
        -> Seq Scan on orders
```

#### Scenario: 子查询计划
```
-> Seq Scan on t
    SubPlan 1
    -> Seq Scan on s
```

### Requirement: 计划节点信息

每个计划节点 SHALL 包含以下信息：

- **Node Type**: 节点类型 (Seq Scan, Index Scan, Hash Join, etc.)
- **Relation**: 涉及的表
- **Cost**: 启动代价和总代价 (startup..total)
- **Rows**: 估计输出行数
- **Width**: 估计行宽度（字节）

#### Scenario: 扫描节点信息
```
-> Seq Scan on orders  (cost=0.00..20.50 rows=1000 width=150)
```

#### Scenario: 连接节点信息
```
-> Hash Join  (cost=30.00..45.00 rows=500)
    -> Seq Scan on customers  (cost=0.00..15.00 rows=100)
    -> Hash  (cost=15.00..15.00 rows=1000)
        -> Seq Scan on orders  (cost=0.00..10.00 rows=1000)
```

### Requirement: 代价格式化

代价 SHALL 以人类可读格式显示。

#### Scenario: 代价显示
- **WHEN** 代价 = 20.50
- **THEN** 显示为 `20.50` 或 `20.5`

#### Scenario: 大代价显示
- **WHEN** 代价 = 1234567.89
- **THEN** 显示为 `1234567.89`

#### Scenario: 零代价显示
- **WHEN** 启动代价 = 0
- **THEN** 显示为 `0.00`

### Requirement: 缩进格式

计划 SHALL 使用缩进显示层次关系。

#### Scenario: 缩进层级
```
-> Top Node
    -> Child Node
        -> Grandchild Node
```

#### Scenario: 缩进宽度
- **WHEN** 每个层级缩进 4 个空格
- **THEN** 层级 0: 无缩进，层级 1: 4 空格，层级 2: 8 空格

### Requirement: 条件信息

过滤条件 SHALL 作为节点属性显示。

#### Scenario: 过滤条件
```
-> Seq Scan on orders
    Filter: (status = 'pending' AND amount > 100)
```

#### Scenario: 连接条件
```
-> Hash Join
    Hash Cond: (orders.customer_id = customers.id)
```

#### Scenario: 索引条件
```
-> Index Scan using idx_customer_id on orders
    Index Cond: (customer_id = 1)
```