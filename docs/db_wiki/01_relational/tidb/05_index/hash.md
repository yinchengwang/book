# TiDB Hash 索引

## 学习目标

- 掌握 TiDB 的 Hash 索引替代方案
- 理解 TiDB 为什么不支持原生 Hash 索引
- 对比 TiDB 的 Hash 能力与 CockroachDB 的差异

## Hash 索引支持

TiDB 不支持原生 Hash 索引（MySQL 的 HASH 索引）。

### 为什么 TiDB 不支持 Hash 索引

1. **分布式架构**：Hash 索引在分布式环境中维护复杂
2. **LSM-Tree 引擎**：TiKV 使用 RocksDB，Hash 索引难以与 LSM-Tree 配合
3. **范围查询优先**：BTree 索引支持范围查询，Hash 索引不支持

### 替代方案

#### Hash Join

TiDB 支持 Hash Join 优化：

```sql
SELECT * FROM users JOIN orders ON users.id = orders.user_id;
```

**执行计划**：

```
HashJoin
├── TableScan(users)  -- Build 端（小表）
└── TableScan(orders) -- Probe 端（大表）
```

#### Hash Aggregation

```sql
SELECT status, COUNT(*) FROM orders GROUP BY status;
```

**执行计划**：

```
HashAgg
└── TableScan(orders)
```

## 与 CockroachDB 对比

| 维度 | TiDB | CockroachDB |
|------|------|------------|
| 原生 Hash 索引 | 不支持 | 不支持 |
| Hash Join | 支持 | 支持 |
| Hash Aggregation | 支持 | 支持 |
| 替代方案 | BTree 索引 | BTree 索引 |

## 与 PostgreSQL 对比

| 维度 | TiDB | PostgreSQL |
|------|------|------------|
| 原生 Hash 索引 | 不支持 | 支持 |
| Hash Join | 支持 | 支持 |
| Hash Aggregation | 支持 | 支持 |
| 适用场景 | 无 | 等值查找 |

## 要点总结

- TiDB 不支持原生 Hash 索引
- 替代方案：Hash Join、Hash Aggregation
- 与 CockroachDB 相同：都不支持原生 Hash 索引
- 与 PostgreSQL 不同：PostgreSQL 支持 Hash 索引

## 思考题

1. TiDB 为什么不支持原生 Hash 索引？如果实现 Hash 索引，会遇到哪些技术挑战？
2. 在没有 Hash 索引的情况下，TiDB 如何优化等值查询？
3. Hash Join 和 Hash Aggregation 在 TiDB 中如何实现？