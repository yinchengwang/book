# OceanBase Hash 索引

## 学习目标

- 掌握 OceanBase 的 Hash 索引支持
- 理解 Hash 索引的使用场景
- 对比 OceanBase 与 TiDB、CockroachDB 的 Hash 索引差异

## Hash 索引支持

OceanBase 不支持原生 Hash 索引，但支持 Hash 相关优化。

### 为什么不支持 Hash 索引

1. **范围查询优先**：BTree 索引支持范围查询
2. **分区表分片**：分区表本身使用 Hash 分区
3. **Hash Join 优化**：Hash Join 提供等值查找能力

### 替代方案

#### Hash 分区表

```sql
-- 创建 Hash 分区表（相当于全局 Hash 索引）
CREATE TABLE users (
    id INT PRIMARY KEY,
    name VARCHAR(100)
) PARTITION BY HASH(id) PARTITIONS 16;
```

#### Hash Join

```sql
SELECT * FROM users JOIN orders ON users.id = orders.user_id;
-- 优化器选择 Hash Join
```

## Hash 分区

```mermaid
graph TB
    A[表数据] --> B[Hash(id) % 16]

    B --> C[Partition 0<br/>id mod 16 = 0]
    B --> D[Partition 1<br/>id mod 16 = 1]
    B --> E[Partition 2<br/>id mod 16 = 2]
    B --> F[...]

    C --> G[OBServer 1]
    D --> H[OBServer 2]
    E --> I[OBServer 3]
```

## 与 TiDB Hash 索引对比

| 维度 | OceanBase | TiDB |
|------|-----------|------|
| 原生 Hash 索引 | 不支持 | 不支持 |
| Hash 分区表 | 支持 | 不支持 |
| Hash Join | 支持 | 支持 |
| 替代方案 | Hash 分区表 | BTree 索引 |

## 与 CockroachDB Hash 索引对比

| 维度 | OceanBase | CockroachDB |
|------|-----------|------------|
| 原生 Hash 索引 | 不支持 | 不支持 |
| Hash 分区表 | 支持 | 不支持 |
| Hash Join | 支持 | 支持 |
| 替代方案 | Hash 分区表 | BTree 索引 |

## 与 PostgreSQL Hash 索引对比

| 维度 | OceanBase | PostgreSQL |
|------|-----------|------------|
| 原生 Hash 索引 | 不支持 | 支持 |
| Hash 分区表 | 支持 | 不支持（需扩展） |
| Hash Join | 支持 | 支持 |
| 适用场景 | 无 | 等值查找 |

## 要点总结

- OceanBase 不支持原生 Hash 索引
- 替代方案：Hash 分区表、Hash Join
- Hash 分区表提供分布式 Hash 能力
- 与 TiDB/CockroachDB 相同：都不支持原生 Hash 索引
- 与 PostgreSQL 不同：PostgreSQL 支持 Hash 索引

## 思考题

1. OceanBase 为什么不支持原生 Hash 索引？如果实现 Hash 索引，会遇到哪些技术挑战？
2. Hash 分区表相比原生 Hash 索引，在查询和写入性能上有何差异？
3. 在没有 Hash 索引的情况下，OceanBase 如何优化等值查询？