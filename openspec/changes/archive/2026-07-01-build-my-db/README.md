# Build My DB - 数据库内核

一个用 C 语言实现的轻量级关系型数据库内核，支持 SQL 查询、事务、索引和 MVCC 并发控制。

## 目录

- [架构设计](#架构设计)
- [快速开始](#快速开始)
- [SQL 使用示例](#sql-使用示例)
- [CLI 工具](#cli-工具)
- [API 参考](#api-参考)
- [性能特性](#性能特性)

---

## 架构设计

### 整体架构

```
┌─────────────────────────────────────────────────────────────┐
│                        CLI 交互层                           │
│                  (db_cli - 交互式 Shell)                     │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                      SQL 处理层                              │
│  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────────┐    │
│  │ 词法分析 │→│ 语法分析 │→│ 语义分析 │→│  查询执行器  │    │
│  │ (Lexer) │  │ (Parser)│  │Semantic │  │ (Executor)  │    │
│  └─────────┘  └─────────┘  └─────────┘  └─────────────┘    │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                      索引子系统                              │
│  ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐ ┌──────────┐  │
│  │ B+Tree │ │  Hash  │ │ Bitmap │ │  ART   │ │ SkipList │  │
│  └────────┘ └────────┘ └────────┘ └────────┘ └──────────┘  │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                      存储引擎                                │
│  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────────┐    │
│  │ 页面管理 │  │ 磁盘读写 │  │ 缓存池  │  │   WAL 日志  │    │
│  └─────────┘  └─────────┘  └─────────┘  └─────────────┘    │
└─────────────────────────────────────────────────────────────┘
```

### 核心模块

| 模块 | 路径 | 功能 |
|------|------|------|
| **storage** | `src/db/storage/` | 存储引擎核心（页面、磁盘、缓存池） |
| **sql** | `src/db/sql/` | SQL 解析和执行（词法、语法、执行器） |
| **index** | `src/db/index/` | 索引实现（B+Tree、Hash、Bitmap 等） |
| **mvcc** | `src/db/concurrency/` | MVCC 多版本并发控制 |
| **optimizer** | `src/db/optimizer/` | 查询优化器 |
| **constraints** | `src/db/constraints/` | 约束检查（主键、唯一、外键） |
| **replication** | `src/db/replication/` | 主从复制 |
| **sharding** | `src/db/sharding/` | 分片支持 |
| **view** | `src/db/view/` | 视图（普通视图、物化视图） |
| **trigger** | `src/db/trigger/` | 触发器 |
| **performance** | `src/db/performance/` | 性能优化（SIMD、并行查询） |
| **cli** | `src/db/cli/` | 交互式 CLI 工具 |

### 页面布局

```
┌────────────────────────────────────────┐
│           Database File                │
├────────────────────────────────────────┤
│  Page 0: File Header                   │
│    - Magic Number                      │
│    - Page Size                         │
│    - Version                           │
├────────────────────────────────────────┤
│  Page 1: Free Space Map                │
│    - Bitmap of free pages              │
├────────────────────────────────────────┤
│  Page 2-N: Data Pages                  │
│    - Table Data                        │
│    - Index Pages                       │
│    - MVCC Version Chains               │
└────────────────────────────────────────┘
```

### MVCC 实现

```
┌─────────────────────────────────────────────┐
│              Row Version Chain              │
│                                             │
│  ┌─────────┐    ┌─────────┐    ┌─────────┐ │
│  │ Ver 3   │───▶│ Ver 2   │───▶│ Ver 1   │ │
│  │ (newest)│    │         │    │ (oldest)│ │
│  └─────────┘    └─────────┘    └─────────┘ │
│       │              │              │       │
│  xmax = 5      xmax = NULL      xmax = 2   │
│  t_xmin = 3    t_xmin = 2       t_xmin = 1 │
└─────────────────────────────────────────────┘

Transaction Snapshot:
  - t_xmin: 最小活跃事务 ID
  - t_xmax: 最大已完成事务 ID + 1
  - active[]: 活跃事务列表
```

---

## 快速开始

### 编译

```bash
# 使用 CMake 编译
mkdir build && cd build
cmake ..
cmake --build .

# 或者直接编译
cmake --build build
```

### 运行 CLI

```bash
# 交互模式
./build/db_cli.exe

# 单命令模式
./build/db_cli.exe -c "CREATE TABLE users (id INT)"
./build/db_cli.exe -c "INSERT INTO users VALUES (1, 'Alice')"
./build/db_cli.exe -c "SELECT * FROM users"

# 执行 SQL 文件
./build/db_cli.exe -f examples/test.sql
```

---

## SQL 使用示例

### DDL - 表管理

```sql
-- 创建表
CREATE TABLE users (
    id INT,
    name VARCHAR(100),
    email VARCHAR(200)
);

-- 创建带索引的表
CREATE TABLE orders (
    order_id INT,
    customer_id INT,
    total DECIMAL(10,2),
    created_at TEXT
);

-- 删除表
DROP TABLE orders;
DROP TABLE users;
```

### DML - 数据操作

```sql
-- 插入数据
INSERT INTO users VALUES (1, 'Alice', 'alice@example.com');
INSERT INTO users VALUES (2, 'Bob', 'bob@example.com');
INSERT INTO users VALUES (3, 'Charlie', 'charlie@example.com');

-- 指定列插入
INSERT INTO users (id, name) VALUES (4, 'David');

-- 插入多行
INSERT INTO users VALUES (5, 'Eve', 'eve@example.com');
INSERT INTO users VALUES (6, 'Frank', 'frank@example.com');
```

### SELECT - 查询

```sql
-- 查询所有
SELECT * FROM users;

-- 查询指定列
SELECT name, email FROM users;

-- 条件查询
SELECT * FROM users WHERE id = 1;
SELECT * FROM users WHERE name = 'Alice';

-- 比较运算
SELECT * FROM users WHERE id > 2;
SELECT * FROM users WHERE id <= 3;
SELECT * FROM users WHERE id <> 2;

-- 组合条件
SELECT * FROM users WHERE id > 1 AND id < 5;
SELECT * FROM users WHERE id = 1 OR id = 3;

-- NULL 检查
SELECT * FROM users WHERE email IS NULL;
SELECT * FROM users WHERE email IS NOT NULL;

-- 取反
SELECT * FROM users WHERE NOT id = 1;
```

### UPDATE - 更新

```sql
-- 更新单列
UPDATE users SET name = 'Alicia' WHERE id = 1;

-- 更新多列
UPDATE users SET name = 'Bobby', email = 'bobby@example.com' WHERE id = 2;

-- 无条件更新（更新所有行）
UPDATE users SET email = 'updated@example.com';

-- 表达式更新
UPDATE users SET id = id + 10 WHERE id < 5;
```

### DELETE - 删除

```sql
-- 条件删除
DELETE FROM users WHERE id = 6;

-- 删除所有数据
DELETE FROM users;
```

---

## CLI 工具

### 内置命令

| 命令 | 说明 |
|------|------|
| `.help` | 显示帮助信息 |
| `.quit` / `.exit` | 退出 CLI |
| `.tables` | 列出所有表 |
| `.schema` | 显示表结构 |
| `.open FILE` | 打开指定数据库 |

### 使用示例

```
==============================================
  Build My DB - 交互式 SQL Shell
  输入 .help 查看帮助
  输入 .quit 退出
==============================================

db> CREATE TABLE users (id INT, name VARCHAR(100));
CREATE TABLE users (id INT, name VARCHAR(100));
操作成功。

db> INSERT INTO users VALUES (1, 'Alice');
INSERT INTO users VALUES (1, 'Alice');
操作成功，影响 1 行。
执行时间: 0.15 ms

db> INSERT INTO users VALUES (2, 'Bob');
INSERT INTO users VALUES (2, 'Bob');
操作成功，影响 1 行。
执行时间: 0.12 ms

db> SELECT * FROM users WHERE id = 1;
SELECT * FROM users WHERE id = 1;
+--------------+
| id | name    |
+--------------+
| 1  | Alice   |
+--------------+
(1 行)
执行时间: 0.08 ms

db> .quit
再见！
```

### 命令行参数

```bash
# 交互模式（默认）
./db_cli.exe

# 执行单条 SQL
./db_cli.exe -c "SELECT * FROM users"

# 执行 SQL 文件
./db_cli.exe -f script.sql

# 显示帮助
./db_cli.exe -h
```

### SQL 文件示例

```sql
-- examples/sample.sql

-- 创建测试表
CREATE TABLE products (
    id INT,
    name VARCHAR(200),
    price DECIMAL(10,2),
    stock INT
);

-- 插入测试数据
INSERT INTO products VALUES (1, 'Apple iPhone 15', 7999.00, 100);
INSERT INTO products VALUES (2, 'Samsung Galaxy S24', 6999.00, 80);
INSERT INTO products VALUES (3, 'Google Pixel 8', 5999.00, 50);

-- 查询所有产品
SELECT * FROM products;

-- 查询高价产品
SELECT * FROM products WHERE price > 6000;

-- 更新库存
UPDATE products SET stock = 90 WHERE id = 1;

-- 删除产品
DELETE FROM products WHERE id = 3;
```

---

## API 参考

### KV 数据库 API

```c
#include <db/kv.h>

// 打开/创建数据库
kv_t *db = kv_open("./mydb.db");

// 基本操作
kv_put(db, "key1", "value1", 6);
char buf[256];
size_t len = kv_get(db, "key1", buf, sizeof(buf));
bool exists = kv_exists(db, "key1");
kv_delete(db, "key1");

// 扫描
kv_iter_t *iter = kv_scan(db, "prefix");
while (kv_iter_next(iter, key, value)) {
    printf("%s = %s\n", key, value);
}
kv_iter_free(iter);

// 关闭
kv_close(db);
```

### SQL 执行器 API

```c
#include <db/sql/sql.h>
#include <db/sql/sql_exec.h>

// 创建执行器
sql_exec_t *exec = sql_exec_create(db);

// 解析 SQL
sql_node_t *node = sql_parse_one("SELECT * FROM users WHERE id = 1");

// 执行查询
sql_result_t *result = sql_exec(exec, node);

// 处理结果
size_t cols = sql_result_num_columns(result);
size_t rows = sql_result_num_rows(result);
for (size_t i = 0; i < rows; i++) {
    void *values;
    sql_result_get_row(result, i, &values);
    // 处理 values...
    free(values);
}

// 释放资源
sql_result_free(result);
sql_node_free(node);
sql_exec_destroy(exec);
```

### 事务 API

```c
#include <db/txn.h>

// 开启事务
txn_id_t txn = txn_begin(db);

// 执行操作
kv_put(db, "key", "value", 5);

// 提交或回滚
if (success) {
    txn_commit(db, txn);
} else {
    txn_rollback(db, txn);
}
```

---

## 性能特性

### 已实现

| 特性 | 说明 |
|------|------|
| **SIMD 优化** | 支持 SSE/AVX/NEON 向量化指令 |
| **并行查询** | 多线程并行执行查询 |
| **查询缓存** | LRU 缓存查询结果 |
| **批量操作** | 高效的批量插入/更新 |
| **LRU 缓存池** | 页面缓存减少磁盘 IO |
| **WAL 预写日志** | 崩溃恢复保证 |

### CPU 利用率

```c
#include <db/performance/performance.h>

// 获取 CPU 核心数
int cpus = get_cpu_count();

// 检查 SIMD 支持
if (simd_is_supported()) {
    const char *type = simd_get_type();  // "SSE/AVX" 或 "NEON"
}

// 创建并行执行器
parallel_config_t config = {
    .num_workers = cpus,
    .chunk_size = 1024,
    .auto_tuning = true
};
parallel_executor_t *executor = parallel_executor_create(&config);
```

### 内存管理

```c
// 获取可用内存
uint64_t mem = get_available_memory();

// 估算最佳工作线程数
int workers = estimate_optimal_workers(0);  // 0 = CPU 密集型
```

---

## 限制与未来工作

### 当前限制

- 不支持 `JOIN` 查询
- 不支持子查询
- 不支持聚合函数（COUNT、SUM、AVG 等）
- 不支持 `ORDER BY`、`GROUP BY`
- 不支持 `LIMIT`、`OFFSET`
- 不支持事务隔离级别配置

### 待实现功能

- [ ] JOIN 引擎
- [ ] 聚合函数
- [ ] 子查询
- [ ] 高级索引（B+Tree 唯一索引、表达式索引）
- [ ] 完整的事务隔离级别
- [ ] SQL 标准合规性增强

---

## 许可

MIT License