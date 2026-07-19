# CockroachDB 实验指南

## 学习目标

- 掌握 CockroachDB CLI 的基本使用方法
- 学会使用 EXPLAIN 分析分布式查询计划
- 通过实验验证 Range 分片、Raft 复制、分布式事务等核心机制

## 安装与启动

### 安装

```bash
# macOS
brew install cockroach

# Linux
wget -qO- https://binaries.cockroachdb.com/cockroach-v25.1.0.linux-amd64.tgz | tar xvz
cp cockroach-v25.1.0.linux-amd64/cockroach /usr/local/bin/

# Windows (使用 chocolatey)
choco install cockroachdb
```

### 启动单节点集群

```bash
# 启动单节点
cockroach start-single-node --insecure --listen-addr=localhost:26257

# 启动 3 节点集群（节点 1）
cockroach start --insecure --listen-addr=localhost:26257 --join=localhost:26257

# 节点 2
cockroach start --insecure --listen-addr=localhost:26258 --join=localhost:26257

# 节点 3
cockroach start --insecure --listen-addr=localhost:26259 --join=localhost:26257
```

### 使用 SQL Shell

```bash
# 连接 SQL Shell
cockroach sql --insecure --host=localhost:26257

# 查看节点状态
cockroach node status --insecure --host=localhost:26257
```

## 基本操作

### 创建表与插入数据

```sql
-- 创建数据库
CREATE DATABASE testdb;
USE testdb;

-- 创建表
CREATE TABLE users (
    id INT PRIMARY KEY,
    name VARCHAR(100),
    email VARCHAR(100),
    age INT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 插入数据
INSERT INTO users VALUES
    (1, 'Alice', 'alice@example.com', 30),
    (2, 'Bob', 'bob@example.com', 25),
    (3, 'Charlie', 'charlie@example.com', 35);

-- 查询数据
SELECT * FROM users WHERE age > 30;
```

### 查看分布式信息

```sql
-- 查看表在哪些 Range 上
SHOW RANGES FROM TABLE users;

-- 查看集群信息
SHOW CLUSTER SETTINGS;

-- 查看 Range 分裂情况
SHOW EXPERIMENTAL_REPLICA TRACE;
```

## EXPLAIN 查询分析

### 查看查询计划

```sql
EXPLAIN SELECT * FROM users WHERE age > 30;
```

**输出示例**：

```
tree | field | description
------+-------+---------------------------------
      |       | distributed
      |       | vectorized
 scan |       |
      | table | users@primary
      | spans | FULL SCAN
      | filter | age > 30
```

### 查看 DistSQL 物理计划

```sql
EXPLAIN (DISTSQL) SELECT * FROM users WHERE age > 30;
```

**输出示例**：

```
┌─────────────────────────────────────┐
│        DistSQL Physical Plan        │
├─────────────────────────────────────┤
│ • Node 1: TableReader               │
│   └─ Table: users@primary           │
│   └─ Spans: FULL SCAN               │
│   └─ Filter: age > 30               │
│                                     │
│ • Node 2: TableReader               │
│   └─ Table: users@primary           │
│   └─ Spans: FULL SCAN               │
│   └─ Filter: age > 30               │
│                                     │
│ • Node 3: TableReader               │
│   └─ Table: users@primary           │
│   └─ Spans: FULL SCAN               │
│   └─ Filter: age > 30               │
│                                     │
│ • Gateway: UNION ALL                │
│   └─ Receive from Node 1, 2, 3      │
└─────────────────────────────────────┘
```

**观察要点**：

- `distributed: true`：表示查询被分发到多个节点
- `TableReader`：每个节点执行本地扫描
- `FULL SCAN`：全表扫描（分布式情况下，每个节点扫描本地 Range）

### Join 计划分析

```sql
EXPLAIN (DISTSQL) SELECT u.name, o.amount
FROM users u JOIN orders o ON u.id = o.user_id
WHERE u.age > 30;
```

**输出示例**：

```
• HashJoin
│  equality: (id) = (user_id)
│
├── • TableReader (users)
│   └── filter: age > 30
│
└── • TableReader (orders)
    └── spans: FULL SCAN
```

## Range 分片验证

### 验证 Range 自动分裂

```sql
-- 创建表
CREATE TABLE test_range (
    id INT PRIMARY KEY,
    data VARCHAR(100)
);

-- 批量插入数据（触发 Range 分裂）
INSERT INTO test_range
SELECT generate_series(1, 1000000), 'test_data_' || generate_series(1, 1000000);

-- 查看 Range 分布
SHOW RANGES FROM TABLE test_range;
```

**预期输出**：多个 Range，每个约 512MB。

### 验证 Range 路由

```sql
-- 查看具体某个 Key 所在的 Range
SHOW RANGE FROM TABLE test_range FOR ROWS (500000);

-- 查看 Range 所在的节点
SELECT range_id, lease_holder, replicas
FROM [SHOW RANGES FROM TABLE test_range];
```

## Raft 复制验证

### 查看 Raft 状态

```bash
# 查看 Range 的 Raft 状态
cockroach debug range-describe --database=testdb --table=test_range --insecure
```

### 故障转移测试

```bash
# 停止一个节点
cockroach quit --insecure --host=localhost:26258

# 查看集群状态
cockroach node status --insecure --host=localhost:26257
# 预期：节点 2 显示为 DEAD，服务不受影响

# 重新启动节点
cockroach start --insecure --listen-addr=localhost:26258 --join=localhost:26257
# 预期：节点自动加入集群，副本自动补全
```

### 数据一致性验证

```sql
-- 在节点 1 写入
INSERT INTO test_range VALUES (1000001, 'raft_test');

-- 在节点 2 读取（即使刚刚故障恢复）
SELECT * FROM test_range WHERE id = 1000001;
-- 预期：数据一致，Raft 保证强一致性
```

## 分布式事务验证

### 跨表事务

```sql
-- 创建测试表
CREATE TABLE accounts (
    id INT PRIMARY KEY,
    balance DECIMAL
);

CREATE TABLE transactions (
    txn_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    from_account INT,
    to_account INT,
    amount DECIMAL,
    txn_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 插入测试数据
INSERT INTO accounts VALUES (1, 1000.00), (2, 1000.00);

-- 执行转账事务
BEGIN;
UPDATE accounts SET balance = balance - 100 WHERE id = 1;
UPDATE accounts SET balance = balance + 100 WHERE id = 2;
INSERT INTO transactions (from_account, to_account, amount) VALUES (1, 2, 100);
COMMIT;

-- 验证一致性
SELECT * FROM accounts;
SELECT * FROM transactions;
```

### Write Intent 冲突检测

```sql
-- 会话 1
BEGIN;
UPDATE accounts SET balance = balance - 100 WHERE id = 1;

-- 会话 2（在另一个连接中）
BEGIN;
UPDATE accounts SET balance = balance - 50 WHERE id = 1;
-- 预期：检测到 Write Intent 冲突，等待或返回重试错误
```

## 性能测试

### YCSB 基准测试

```bash
# 安装 YCSB
git clone https://github.com/brianfrankcooper/YCSB.git

# 运行测试
./bin/ycsb load jdbc -P workloads/workloada \
    -p db.driver=org.postgresql.Driver \
    -p db.url=jdbc:postgresql://localhost:26257/testdb

# 运行工作负载
./bin/ycsb run jdbc -P workloads/workloada \
    -p db.driver=org.postgresql.Driver \
    -p db.url=jdbc:postgresql://localhost:26257/testdb
```

### TPC-C 基准测试

```sql
-- 使用 CockroachDB 内置的 TPC-C 测试
cockroach workload init tpcc \
    --warehouses=10 \
    "postgresql://root@localhost:26257?sslmode=disable"

cockroach workload run tpcc \
    --warehouses=10 \
    "postgresql://root@localhost:26257?sslmode=disable"
```

## 要点总结

- CockroachDB CLI 提供 `cockroach sql` SQL Shell 和 `cockroach node` 集群管理
- EXPLAIN (DISTSQL) 显示分布式物理计划，可以看到跨节点并行执行
- SHOW RANGES 验证 Range 自动分片和分布
- 故障转移测试验证 Raft 自动选举和副本补全
- 分布式事务测试验证 Write Intent 冲突检测和 2PC 提交
- YCSB 和 TPC-C 基准测试验证性能

## 思考题

1. 在 EXPLAIN (DISTSQL) 输出中，如何判断查询是跨节点并行执行还是单节点执行？什么情况下查询不会被分布式执行？
2. SHOW RANGES 输出中，lease_holder 和 replicas 字段分别表示什么？Range 的 Leader 和 Follower 如何分布在不同的节点上？
3. 故障转移测试中，节点故障后 Raft 选举新 Leader 需要多长时间？这个时间受哪些因素影响？
4. Write Intent 冲突检测在高并发场景下，冲突率如何估算？如何通过应用层优化减少冲突？