# OceanBase 其他索引类型

## 学习目标

- 掌握 OceanBase 的全文索引、表达式索引等特殊索引
- 理解 OceanBase 的索引能力与 TiDB、CockroachDB 的差异
- 对比 OceanBase 的索引能力与 PostgreSQL

## 全文索引

OceanBase 支持全文索引（Full-Text Index）。

### 创建全文索引

```sql
CREATE FULLTEXT INDEX idx_content ON articles(content);

SELECT * FROM articles WHERE MATCH(content) AGAINST('database');
```

### 实现原理

- **索引类型**：倒排索引（Inverted Index）
- **分词器**：支持中文分词
- **存储位置**：局部索引（分区内）

## 表达式索引

OceanBase 支持表达式索引（Functional Index）。

```sql
-- 创建表达式索引
CREATE INDEX idx_lower_name ON users((LOWER(name)));

SELECT * FROM users WHERE LOWER(name) = 'alice';
```

### 使用场景

- 函数调用优化
- 计算字段索引
- 复杂查询加速

## 空间索引

OceanBase 不支持原生空间索引（GIS）。

### 空间数据类型

```sql
CREATE TABLE locations (
    id INT PRIMARY KEY,
    point GEOMETRY
);
```

### 空间查询

```sql
SELECT * FROM locations WHERE ST_Distance(point, POINT(0, 0)) < 10;
```

**限制**：不支持 GiST 空间索引，空间查询性能较差。

## 函数索引

OceanBase 支持函数索引（基于表达式的索引）。

```sql
-- 创建函数索引
CREATE INDEX idx_year ON orders((YEAR(order_date)));

SELECT * FROM orders WHERE YEAR(order_date) = 2024;
```

## 与 TiDB 索引对比

| 维度 | OceanBase | TiDB |
|------|-----------|------|
| 全文索引 | 支持（倒排索引） | 不支持 |
| 表达式索引 | 支持 | 支持 |
| 空间索引 | 不支持 | 不支持 |
| JSON 索引 | 支持（虚拟列） | 支持（虚拟列） |
| 函数索引 | 支持 | 不支持 |

## 与 CockroachDB 索引对比

| 维度 | OceanBase | CockroachDB |
|------|-----------|------------|
| 全文索引 | 支持 | 支持（GIN/GiST） |
| 表达式索引 | 支持 | 支持 |
| 空间索引 | 不支持 | 支持（GiST） |
| JSON 索引 | 支持（虚拟列） | 支持（Inverted Index） |

## 与 PostgreSQL 索引对比

| 维度 | OceanBase | PostgreSQL |
|------|-----------|------------|
| 全文索引 | 支持（倒排索引） | 支持（GIN） |
| 表达式索引 | 支持 | 支持 |
| 空间索引 | 不支持 | 支持（GiST） |
| JSON 索引 | 支持（虚拟列） | 支持（GIN） |
| 部分索引 | 支持 | 支持 |

## 要点总结

- OceanBase 支持全文索引、表达式索引、函数索引
- 不支持原生空间索引（GiST）
- 与 TiDB 相比：全文索引是差异点
- 与 CockroachDB 相比：空间索引能力不足
- 与 PostgreSQL 相比：空间索引能力不足

## 思考题

1. OceanBase 的全文索引在分布式环境下的性能如何？如何优化全文搜索？
2. OceanBase 为什么不支持 GiST 空间索引？如果实现空间索引，有哪些技术挑战？
3. 表达式索引在 OceanBase 中如何维护？对写入性能有何影响？