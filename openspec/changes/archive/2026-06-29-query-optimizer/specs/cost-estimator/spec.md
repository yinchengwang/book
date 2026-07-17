# 代价估算器规格

## ADDED Requirements

### Requirement: 代价模型定义

系统 SHALL 使用简单线性代价模型：

```
cost = page_io_cost + cpu_tuple_cost * rows
```

- `page_io_cost`: 页面IO成本，顺序扫描 = page_count × seq_page_cost
- `cpu_tuple_cost`: CPU处理每行成本

#### Scenario: 顺序扫描代价
- **WHEN** 表有 1000 行，占用 10 页
- **AND** seq_page_cost = 1.0, cpu_tuple_cost = 0.01
- **THEN** 代价 = 10 × 1.0 + 1000 × 0.01 = 20

#### Scenario: 索引扫描代价
- **WHEN** 使用索引扫描读取 100 行
- **AND** 索引页数 = 2, 表页数 = 10
- **AND** random_page_cost = 4.0
- **THEN** 代价 ≈ 2 × random_page_cost + 100 × cpu_tuple_cost

### Requirement: 统计信息存储

表元数据 SHALL 存储以下统计信息：

```c
typedef struct table_stats {
    int64_t row_count;        /* 估计行数 */
    int32_t page_count;       /* 页面数 */
    double rel_density;       /* 行密度 (0-1) */
} table_stats_t;

typedef struct column_stats {
    int64_t n_distinct;       /* 不同值数量 */
    double null_frac;         /* NULL 比例 */
    double min_value;         /* 最小值 */
    double max_value;         /* 最大值 */
} column_stats_t;
```

#### Scenario: 自动统计
- **WHEN** 创建表并插入数据
- **THEN** 系统自动维护 row_count 和 page_count

#### Scenario: 手动 ANALYZE
- **WHEN** 用户执行 ANALYZE table_name
- **THEN** 更新表的统计信息

### Requirement: 选择性估算

系统 SHALL 根据条件估算过滤后剩余行数。

#### Scenario: 等值条件
- **WHEN** 列有 N 个不同值，查询 `WHERE col = value`
- **AND** 无统计直方图
- **THEN** 估计选择率 = 1 / n_distinct

#### Scenario: 范围条件
- **WHEN** 查询 `WHERE col BETWEEN low AND high`
- **AND** 有 min/max 统计
- **THEN** 估计选择率 = (high - low) / (max - min)

#### Scenario: 无统计默认值
- **WHEN** 列无统计信息
- **THEN** 使用默认值 0.1 (10% 选择率)

### Requirement: 代价比较

优化器 SHALL 比较候选计划并选择代价最低的。

#### Scenario: 全表扫描 vs 索引扫描
- **WHEN** 查询可使用索引
- **THEN** 计算两种方式的代价，选择较低的

#### Scenario: 多表连接顺序
- **WHEN** 多表连接
- **THEN** 枚举不同连接顺序，选择总代价最低的

### Requirement: 代价输出

EXPLAIN 输出 SHALL 包含代价信息。

#### Scenario: EXPLAIN 输出代价
```
-> Seq Scan on orders  (cost=0.00..20.00 rows=1000)
  -> Hash Join  (cost=30.00..45.00 rows=500)
```

#### Scenario: EXPLAIN ANALYZE 实际代价
```
-> Seq Scan on orders  (cost=20.00 rows=1000 actual=1002 rows=1002)
```