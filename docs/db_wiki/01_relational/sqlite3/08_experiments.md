# SQLite3 实验设计

## 学习目标

1. 掌握 SQLite3 的**实验设计方法**（从简单到复杂）
2. 学会使用 `sqlite3` 命令行工具和 `EXPLAIN` 分析查询
3. 理解 SQLite 的**性能基准测试**方法
4. 熟悉 SQLite 的**调优技巧**（PRAGMA、索引、事务）
5. 能够设计实验验证 SQLite 的各项特性

---

## 核心概念

### 1. 实验环境搭建

**安装 SQLite**：

```bash
# Linux (Debian/Ubuntu)
sudo apt-get install sqlite3

# macOS
brew install sqlite3

# Windows
# 下载预编译二进制：https://www.sqlite.org/download.html

# 验证安装
sqlite3 --version
# 3.42.0 2023-05-16 12:36:15
```

**创建测试数据库**：

```bash
# 创建数据库
sqlite3 test.db

# 查看帮助
sqlite> .help

# 退出
sqlite> .exit
```

---

### 2. 基础实验：创建表与插入数据

**实验目的**：理解 SQLite 的基本操作和事务机制。

**实验步骤**：

```sql
-- 1. 创建表
CREATE TABLE users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    email TEXT UNIQUE,
    age INTEGER,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- 2. 插入单条数据
INSERT INTO users (name, email, age)
VALUES ('Alice', 'alice@example.com', 30);

-- 3. 批量插入（事务优化）
BEGIN TRANSACTION;
INSERT INTO users (name, email, age) VALUES ('Bob', 'bob@example.com', 25);
INSERT INTO users (name, email, age) VALUES ('Charlie', 'charlie@example.com', 35);
INSERT INTO users (name, email, age) VALUES ('David', 'david@example.com', 28);
COMMIT;

-- 4. 查询数据
SELECT * FROM users WHERE age > 25;

-- 5. 使用 EXPLAIN 分析查询
EXPLAIN QUERY PLAN SELECT * FROM users WHERE age > 25;
-- 输出：
-- QUERY PLAN
-- `--SCAN users
```

**实验结果分析**：
- `INTEGER PRIMARY KEY AUTOINCREMENT` 自动生成 rowid
- 批量插入使用事务，性能提升 10x ~ 100x
- `EXPLAIN QUERY PLAN` 显示查询使用全表扫描（SCAN）

---

### 3. 索引实验

**实验目的**：验证索引对查询性能的影响。

**实验步骤**：

```sql
-- 1. 创建索引前：全表扫描
EXPLAIN QUERY PLAN SELECT * FROM users WHERE age = 30;
-- QUERY PLAN
-- `--SCAN users USING INDEX sqlite_autoindex_users_1 (age=?)

-- 2. 创建索引
CREATE INDEX idx_users_age ON users(age);

-- 3. 创建索引后：索引扫描
EXPLAIN QUERY PLAN SELECT * FROM users WHERE age = 30;
-- QUERY PLAN
-- `--SEARCH users USING INDEX idx_users_age (age=?)

-- 4. 创建复合索引
CREATE INDEX idx_users_age_name ON users(age, name);

-- 5. 复合索引查询
EXPLAIN QUERY PLAN SELECT * FROM users WHERE age = 30 AND name = 'Alice';
-- QUERY PLAN
-- `--SEARCH users USING INDEX idx_users_age_name (age=? AND name=?)

-- 6. 索引覆盖（covering index）
EXPLAIN QUERY PLAN SELECT age, name FROM users WHERE age = 30;
-- QUERY PLAN
-- `--SEARCH users USING COVERING INDEX idx_users_age_name (age=?)
```

**性能对比实验**：

```bash
# 插入 100 万条测试数据
python3 <<EOF
import sqlite3
import random
import time

conn = sqlite3.connect('test.db')
cursor = conn.cursor()

# 创建表
cursor.execute('''
    CREATE TABLE test_table (
        id INTEGER PRIMARY KEY,
        value TEXT,
        number INTEGER
    )
''')

# 批量插入 100 万行
start = time.time()
cursor.execute('BEGIN TRANSACTION')
for i in range(1000000):
    cursor.execute('INSERT INTO test_table (value, number) VALUES (?, ?)',
                   (f'item_{i}', random.randint(1, 1000)))
cursor.execute('COMMIT')
print(f'插入 100 万行耗时: {time.time() - start:.2f} 秒')

conn.commit()
conn.close()
EOF

# 无索引查询耗时
sqlite3 test.db "SELECT COUNT(*) FROM test_table WHERE number = 500;"
# 耗时: 约 0.3 秒

# 创建索引
sqlite3 test.db "CREATE INDEX idx_number ON test_table(number);"

# 有索引查询耗时
sqlite3 test.db "SELECT COUNT(*) FROM test_table WHERE number = 500;"
# 耗时: 约 0.001 秒
```

**实验结果**：
- 索引查询速度提升 300x
- 复合索引支持多列查询
- 覆盖索引避免访问表数据

---

### 4. 事务模式实验

**实验目的**：对比 DELETE、WAL、MEMORY 三种事务模式。

**实验步骤**：

```sql
-- 1. DELETE 模式（默认）
PRAGMA journal_mode = DELETE;
-- journal_mode
-- DELETE

-- 2. WAL 模式
PRAGMA journal_mode = WAL;
-- journal_mode
-- wal

-- 3. MEMORY 模式
PRAGMA journal_mode = MEMORY;
-- journal_mode
-- memory

-- 4. 测试并发写入
-- 在两个终端同时执行：
-- 终端 1:
sqlite3 test.db
BEGIN TRANSACTION;
UPDATE test_table SET number = number + 1 WHERE id = 1;
-- （不提交）

-- 终端 2:
sqlite3 test.db
SELECT * FROM test_table WHERE id = 1;
-- DELETE 模式：阻塞等待
-- WAL 模式：立即返回（读写并发）
```

**性能测试**：

```bash
# DELETE 模式写入性能
sqlite3 test_delete.db "PRAGMA journal_mode=DELETE;"
python3 <<EOF
import sqlite3, time
conn = sqlite3.connect('test_delete.db')
cursor = conn.cursor()
cursor.execute('CREATE TABLE t (id INTEGER PRIMARY KEY, val TEXT)')
start = time.time()
for i in range(10000):
    cursor.execute('INSERT INTO t (val) VALUES (?)', (f'val_{i}',))
    conn.commit()
print(f'DELETE 模式插入 10000 行: {time.time() - start:.2f} 秒')
EOF

# WAL 模式写入性能
sqlite3 test_wal.db "PRAGMA journal_mode=WAL;"
python3 <<EOF
import sqlite3, time
conn = sqlite3.connect('test_wal.db')
cursor = conn.cursor()
cursor.execute('CREATE TABLE t (id INTEGER PRIMARY KEY, val TEXT)')
start = time.time()
cursor.execute('BEGIN TRANSACTION')
for i in range(10000):
    cursor.execute('INSERT INTO t (val) VALUES (?)', (f'val_{i}',))
cursor.execute('COMMIT')
print(f'WAL 模式批量插入 10000 行: {time.time() - start:.2f} 秒')
EOF
```

**实验结果**：

| 模式 | 插入 10000 行耗时 | 优势 | 劣势 |
|------|------------------|------|------|
| DELETE | 约 15 秒 | 兼容性好 | 写阻塞读 |
| WAL | 约 0.5 秒 | 读写并发 | 需额外文件 |
| MEMORY | 约 0.1 秒 | 极快 | 崩溃丢数据 |

---

### 5. 并发控制实验

**实验目的**：理解 SQLite 的并发模型和锁机制。

**实验步骤**：

```python
import sqlite3
import threading
import time

# 测试并发写入
def writer_thread(db_path, thread_id):
    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()
    for i in range(100):
        try:
            cursor.execute('INSERT INTO concurrent_test (thread_id, value) VALUES (?, ?)',
                           (thread_id, i))
            conn.commit()
        except sqlite3.OperationalError as e:
            print(f'线程 {thread_id} 遇到锁冲突: {e}')
            time.sleep(0.01)
    conn.close()

# 创建测试表
conn = sqlite3.connect('test_concurrent.db')
conn.execute('CREATE TABLE concurrent_test (id INTEGER PRIMARY KEY, thread_id INTEGER, value INTEGER)')
conn.commit()
conn.close()

# 启动 10 个线程并发写入
threads = []
for i in range(10):
    t = threading.Thread(target=writer_thread, args=('test_concurrent.db', i))
    threads.append(t)
    t.start()

for t in threads:
    t.join()

# 查看结果
conn = sqlite3.connect('test_concurrent.db')
count = conn.execute('SELECT COUNT(*) FROM concurrent_test').fetchone()[0]
print(f'成功插入行数: {count}')
conn.close()
```

**实验结果**：
- DELETE 模式：大部分写入会遇到 `SQLITE_BUSY` 错误
- WAL 模式：成功率高，但仍有限制（单写者）

---

### 6. 性能调优实验

**实验目的**：掌握 SQLite 的性能调优技巧。

**调优参数**：

```sql
-- 1. 页面大小（创建数据库前设置）
PRAGMA page_size = 4096;  -- 4KB（默认）

-- 2. 缓存大小
PRAGMA cache_size = -64000;  -- 64MB

-- 3. 同步模式
PRAGMA synchronous = NORMAL;  -- 平衡性能与安全

-- 4. 事务模式
PRAGMA journal_mode = WAL;

-- 5. 外键约束
PRAGMA foreign_keys = ON;

-- 6. 内存限制
PRAGMA memory_limit = 1048576;  -- 1MB

-- 7. 临时存储
PRAGMA temp_store = MEMORY;

-- 8. 分析与优化
ANALYZE;  -- 收集统计信息，帮助查询优化器

-- 9. 清理碎片
VACUUM;  -- 重建数据库，回收空间

-- 10. 查看数据库信息
PRAGMA database_list;
PRAGMA table_info(users);
PRAGMA index_list(users);
```

**性能对比**：

```bash
# 无优化
sqlite3 test_no_opt.db <<EOF
CREATE TABLE t (id INTEGER PRIMARY KEY, val TEXT);
.timer on
INSERT INTO t (val) SELECT randomblob(100) FROM generate_series(1, 10000);
.timer off
EOF
# Run Time: real 2.345 user 0.123 sys 0.456

# 优化后
sqlite3 test_opt.db <<EOF
PRAGMA journal_mode = WAL;
PRAGMA synchronous = NORMAL;
PRAGMA cache_size = -64000;
CREATE TABLE t (id INTEGER PRIMARY KEY, val TEXT);
.timer on
BEGIN TRANSACTION;
INSERT INTO t (val) SELECT randomblob(100) FROM generate_series(1, 10000);
COMMIT;
.timer off
EOF
# Run Time: real 0.123 user 0.045 sys 0.030
```

---

### 7. 高级特性实验

**1. 全文搜索（FTS5）**：

```sql
-- 创建 FTS5 虚拟表
CREATE VIRTUAL TABLE documents USING fts5(title, content);

-- 插入数据
INSERT INTO documents (title, content) VALUES
    ('SQLite Introduction', 'SQLite is a lightweight embedded database'),
    ('PostgreSQL Guide', 'PostgreSQL is an advanced open-source database');

-- 全文搜索
SELECT * FROM documents WHERE documents MATCH 'database';
-- 输出：
-- SQLite Introduction|SQLite is a lightweight embedded database
-- PostgreSQL Guide|PostgreSQL is an advanced open-source database

-- 高亮显示
SELECT highlight(documents, 1, '<b>', '</b>') FROM documents WHERE documents MATCH 'database';
```

**2. 空间索引（R-Tree）**：

```sql
-- 创建 R-Tree 虚拟表
CREATE VIRTUAL TABLE locations USING rtree(
    id,
    min_x, max_x,
    min_y, max_y
);

-- 插入空间数据
INSERT INTO locations VALUES
    (1, 0, 10, 0, 10),  -- 矩形 1
    (2, 5, 15, 5, 15),  -- 矩形 2
    (3, 10, 20, 10, 20); -- 矩形 3

-- 查询相交矩形
SELECT * FROM locations WHERE min_x <= 10 AND max_x >= 5 AND min_y <= 10 AND max_y >= 5;
```

**3. JSON 支持**：

```sql
-- 创建 JSON 列
CREATE TABLE events (
    id INTEGER PRIMARY KEY,
    data JSON
);

-- 插入 JSON 数据
INSERT INTO events (data) VALUES
    ('{"name": "Alice", "age": 30, "tags": ["python", "sqlite"]}'),
    ('{"name": "Bob", "age": 25, "tags": ["java", "mysql"]}');

-- JSON 查询
SELECT json_extract(data, '$.name') AS name FROM events;
-- 输出：
-- Alice
-- Bob

-- JSON 路径查询
SELECT * FROM events WHERE json_extract(data, '$.age') > 26;
```

---

## 要点总结

1. **实验设计原则**：从简单到复杂，逐步验证假设
2. **EXPLAIN 工具**：分析查询计划，优化性能
3. **事务优化**：批量插入使用事务，性能提升 10x ~ 100x
4. **索引优化**：合理创建索引，查询速度提升 100x ~ 1000x
5. **WAL 模式**：并发场景首选，读写可并发
6. **PRAGMA 调优**：根据场景调整 `synchronous`、`cache_size`、`journal_mode`
7. **高级特性**：FTS5 全文搜索、R-Tree 空间索引、JSON 支持

---

## 思考题

1. **索引选择**：如何判断哪些列需要创建索引？复合索引的列顺序如何决定？
2. **事务边界**：批量插入时，事务过大会有什么问题？如何平衡事务大小与性能？
3. **WAL vs DELETE**：在什么场景下选择 WAL 模式，什么场景下选择 DELETE 模式？
4. **并发极限**：SQLite 的并发写入极限是多少？如何通过实验测定？
5. **VACUUM 时机**：什么情况下需要执行 `VACUUM`？频繁执行 `VACUUM` 有什么影响？

---

## 参考资源

- [SQLite 官方教程](https://www.sqlite.org/tutorial.html)
- [SQLite 性能优化](https://www.sqlite.org/speed.html)
- [SQLite PRAGMA](https://www.sqlite.org/pragma.html)
- [SQLite FTS5](https://www.sqlite.org/fts5.html)
- [SQLite R-Tree](https://www.sqlite.org/rtree.html)