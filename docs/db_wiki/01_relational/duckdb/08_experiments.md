# DuckDB 实验指南

## 学习目标

- 掌握 DuckDB CLI、Python API 的基本使用方法
- 学会使用 EXPLAIN 分析查询计划，理解向量化执行、列裁剪、谓词下推等优化
- 通过实验验证列式存储、压缩算法、统计过滤的性能效果

## 安装与启动

### 安装

**Python 安装**：

```bash
pip install duckdb
```

**CLI 安装**：

```bash
# Windows
winget install DuckDB.cli

# macOS
brew install duckdb

# Linux
wget https://github.com/duckdb/duckdb/releases/download/v1.2.0/duckdb_cli-linux-amd64.zip
unzip duckdb_cli-linux-amd64.zip
```

### 启动 DuckDB CLI

```bash
# 内存数据库
duckdb

# 文件数据库
duckdb mydb.duckdb

# 从命令行执行 SQL
duckdb -c "SELECT 42 AS answer"
```

## 基本操作

### 创建表与导入数据

```sql
-- 创建表
CREATE TABLE users (
    id INTEGER PRIMARY KEY,
    name VARCHAR,
    age INTEGER,
    salary DOUBLE,
    created_at TIMESTAMP
);

-- 从 CSV 导入
CREATE TABLE sales AS
SELECT * FROM read_csv_auto('sales.csv');

-- 从 Parquet 导入
CREATE TABLE orders AS
SELECT * FROM read_parquet('orders.parquet');

-- 从 JSON 导入
CREATE TABLE logs AS
SELECT * FROM read_json_auto('logs.json');
```

### 查询数据

```sql
-- 基本查询
SELECT * FROM users WHERE age > 30;

-- 聚合查询
SELECT
    region,
    SUM(revenue) AS total_revenue,
    AVG(revenue) AS avg_revenue,
    COUNT(*) AS transaction_count
FROM sales
GROUP BY region
ORDER BY total_revenue DESC
LIMIT 10;

-- 窗口函数
SELECT
    user_id,
    order_date,
    amount,
    SUM(amount) OVER (PARTITION BY user_id ORDER BY order_date) AS cumulative_amount,
    LAG(amount) OVER (PARTITION BY user_id ORDER BY order_date) AS prev_amount
FROM orders;
```

### 导出数据

```sql
-- 导出为 Parquet
COPY (SELECT * FROM users WHERE age > 30)
TO 'filtered_users.parquet';

-- 导出为 CSV
COPY sales TO 'sales_export.csv' (HEADER, DELIMITER ',');

-- 导出为 JSON
COPY orders TO 'orders.json';
```

## EXPLAIN 查询分析

### 查看查询计划

```sql
EXPLAIN SELECT region, SUM(revenue)
FROM sales
WHERE revenue > 10000
GROUP BY region;
```

**输出示例**：

```
┌─────────────────────────────┐
│         QUERY PLAN          │
├─────────────────────────────┤
│ • HASH_GROUP_BY             │
│   └─ Expressions: region    │
│                             │
│ • FILTER                    │
│   └─ revenue > 10000        │
│                             │
│ • TABLE_SCAN                │
│   └─ Table: sales           │
│   └─ Columns: region,       │
│              revenue         │
└─────────────────────────────┘
```

### 列裁剪验证

```sql
-- 只查询单列
EXPLAIN SELECT name FROM users;
```

**观察输出**：`TABLE_SCAN` 中 `Columns: name`，证明 DuckDB 只读取需要的列。

### 谓词下推验证

```sql
-- 带过滤条件的查询
EXPLAIN SELECT * FROM sales WHERE region = 'Asia';
```

**观察输出**：`FILTER` 算子应该出现在 `TABLE_SCAN` 之后，证明谓词未被下推（DuckDB 依赖 Zone Map 过滤）。

### Join 策略分析

```sql
-- Hash Join 示例
EXPLAIN SELECT u.name, o.amount
FROM users u
JOIN orders o ON u.id = o.user_id;
```

**输出示例**：

```
┌─────────────────────────────┐
│         QUERY PLAN          │
├─────────────────────────────┤
│ • HASH_JOIN                 │
│   └─ Condition: u.id =      │
│              o.user_id       │
│                             │
│   └─ Left:                  │
│     • TABLE_SCAN users      │
│                             │
│   └─ Right:                 │
│     • TABLE_SCAN orders     │
└─────────────────────────────┘
```

## 性能对比实验

### 列式存储 vs 行式存储

**实验设计**：对比 DuckDB（列存）与 SQLite（行存）在分析查询上的性能。

**数据准备**：

```sql
-- 在 DuckDB 中创建测试表
CREATE TABLE test_data AS
SELECT
    range AS id,
    'User_' || range AS name,
    random() * 1000 AS value1,
    random() * 500 AS value2,
    random() * 2000 AS value3,
    random() * 300 AS value4,
    random() * 1500 AS value5
FROM range(0, 1000000);

-- 导出为 SQLite 可导入的格式
COPY test_data TO 'test_data.csv';

-- 在 SQLite 中导入
-- sqlite3 test.db
-- > CREATE TABLE test_data (...);
-- > .import test_data.csv test_data
```

**查询测试**：

```sql
-- 单列聚合（列存优势）
SELECT SUM(value1) FROM test_data;

-- 全列查询（行存优势）
SELECT * FROM test_data WHERE id < 100;
```

**预期结果**：DuckDB 在单列聚合上快 10-50 倍，SQLite 在全列查询上略快。

### 压缩算法验证

```sql
-- 创建压缩表
CREATE TABLE compressed_data AS
SELECT
    range AS id,
    CASE (range % 10)
        WHEN 0 THEN 'Category_A'
        WHEN 1 THEN 'Category_B'
        ELSE 'Category_C'
    END AS category
FROM range(0, 10000000);

-- 查看表大小
PRAGMA table_info('compressed_data');
SHOW TABLES;
```

**验证压缩效果**：通过 `SHOW TABLES` 查看表的磁盘占用，对比原始数据大小。

### Zone Map 统计过滤

```sql
-- 创建带统计信息的表
CREATE TABLE sales_with_stats AS
SELECT
    range AS id,
    (range % 100) + 1 AS region_id,
    random() * 10000 AS revenue,
    '2026-' || (range % 12 + 1)::VARCHAR || '-01' AS sale_date
FROM range(0, 10000000);

-- 查询利用 Zone Map 过滤
SELECT COUNT(*) FROM sales_with_stats WHERE revenue > 9999;
```

**验证方法**：使用 `EXPLAIN ANALYZE` 查看实际读取的数据块数量。

```sql
EXPLAIN ANALYZE SELECT COUNT(*) FROM sales_with_stats WHERE revenue > 9999;
```

**预期输出**：`rows_read` 远小于总行数，证明 Zone Map 跳过了大部分数据块。

## 向量化执行验证

### 批量大小影响

```python
# Python 实验：验证向量化执行的性能
import duckdb
import time

conn = duckdb.connect(":memory:")

# 创建测试表
conn.execute("""
    CREATE TABLE large_table AS
    SELECT
        range AS id,
        random() * 1000 AS value
    FROM range(0, 10000000)
""")

# 测试聚合查询
start = time.time()
result = conn.execute("SELECT SUM(value) FROM large_table").fetchone()
elapsed = time.time() - start

print(f"DuckDB 聚合耗时: {elapsed:.3f} 秒")
```

### SIMD 加速验证

DuckDB 在编译时自动检测 CPU 的 SIMD 支持（AVX2/AVX-512）。可以通过环境变量强制禁用 SIMD：

```bash
# 禁用 SIMD
export DUCKDB_SIMD=OFF

# 运行 DuckDB
duckdb -c "SELECT SUM(value) FROM large_table"
```

**对比实验**：禁用 SIMD 后查询性能下降 20-40%。

## 并发测试

### 单连接写入

```python
import duckdb
import threading

conn = duckdb.connect("concurrent_test.duckdb")
conn.execute("CREATE TABLE test (id INTEGER, value VARCHAR)")

# 单线程写入
def insert_data(start_id):
    for i in range(start_id, start_id + 1000):
        conn.execute("INSERT INTO test VALUES (?, ?)", [i, f"value_{i}"])

# 多线程写入（会失败）
threads = []
for i in range(10):
    t = threading.Thread(target=insert_data, args=(i * 1000,))
    threads.append(t)
    t.start()

for t in threads:
    t.join()
```

**预期结果**：DuckDB 不支持多线程并发写入，会抛出错误。

### 多连接读取

```python
# 多连接并发读取（支持）
connections = [duckdb.connect("concurrent_test.duckdb") for _ in range(5)]

def read_data(conn):
    result = conn.execute("SELECT COUNT(*) FROM test").fetchone()
    print(result)

threads = [threading.Thread(target=read_data, args=(conn,)) for conn in connections]
for t in threads:
    t.start()
for t in threads:
    t.join()
```

**预期结果**：DuckDB 支持多连接并发读取。

## PRAGMA 调优

### 内存限制

```sql
-- 设置内存限制为 4GB
SET memory_limit = '4GB';

-- 设置线程数
SET threads = 4;
```

### 日志级别

```sql
-- 开启日志
SET enable_logging = true;

-- 设置日志级别
SET log_level = 'DEBUG';
```

## 要点总结

- DuckDB 的 EXPLAIN 输出清晰展示向量化执行、列裁剪、Join 策略
- 列式存储在单列聚合查询上比行式存储快 10-50 倍
- Zone Map 统计信息实现谓词下推，跳过无用数据块
- 向量化执行 + SIMD 加速带来 20-40% 性能提升
- DuckDB 不支持多连接并发写入，但支持多连接并发读取
- 通过 PRAGMA 命令可以调整内存、线程、日志等参数

## 思考题

1. 在 EXPLAIN 输出中，`TABLE_SCAN` 的 `Columns` 字段如何体现列式存储的优势？
2. 为什么 DuckDB 的 Zone Map 过滤在 `revenue > 9999` 这种极端值查询中效果最明显？如果条件改为 `revenue > 5000`，Zone Map 还能跳过多少数据块？
3. 多线程并发写入失败的根本原因是什么？DuckDB 为什么不实现行级锁来支持并发写入？
4. 禁用 SIMD 后，向量化执行引擎的性能下降来自哪些方面？如何量化 SIMD 加速的贡献？