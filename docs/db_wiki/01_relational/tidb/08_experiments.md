# TiDB 实验指南

## 学习目标

- 掌握 TiDB 的基础操作：集群部署、SQL 执行、监控
- 理解 TiDB 的 Region 分布、Raft 状态、性能调优
- 对比 TiDB 与 CockroachDB 的实验差异

## 集群部署

### 单机部署（TiUP）

```bash
# 1. 安装 TiUP
curl --proto '=https' --tlsv1.2 -sSf https://tiup-mirrors.pingcap.com/install.sh | sh
source ~/.bash_profile

# 2. 启动本地集群
tiup playground v7.0.0 --db 1 --pd 1 --kv 1 --tiflash 1

# 3. 连接 TiDB
mysql -h 127.0.0.1 -P 4000 -u root

# 4. 查看集群状态
tiup playground display
```

### K8s 部署（TiDB Operator）

```bash
# 1. 安装 TiDB Operator
kubectl apply -f https://github.com/pingcap/tidb-operator/releases/download/v1.14.0/tidb-operator.yaml

# 2. 部署 TiDB 集群
kubectl apply -f tidb-cluster.yaml

# 3. 连接 TiDB
kubectl port-forward svc/tidb-cluster-tidb 4000:4000
mysql -h 127.0.0.1 -P 4000 -u root
```

## 基础操作

### 数据库和表创建

```sql
-- 创建数据库
CREATE DATABASE mydb;
USE mydb;

-- 创建表（MySQL 语法）
CREATE TABLE users (
    id INT PRIMARY KEY,
    name VARCHAR(100),
    age INT,
    INDEX idx_age (age)
);

-- 插入数据
INSERT INTO users VALUES (1, 'Alice', 30);
INSERT INTO users VALUES (2, 'Bob', 25);
INSERT INTO users VALUES (3, 'Eve', 35);

-- 查询数据
SELECT * FROM users WHERE age > 30;
```

### Region 管理

```sql
-- 查看表的 Region 分布
SHOW TABLE users REGIONS;

-- 输出示例
+-----------+-----------+---------+-----------+-----------------+
| REGION_ID | START_KEY | END_KEY | LEADER_ID | PEERS           |
+-----------+-----------+---------+-----------+-----------------+
| 1         | t1_r1     | t1_r2   | 101       | 101,102,103     |
+-----------+-----------+---------+-----------+-----------------+

-- 查看指定 Region 信息
SHOW REGION 1;
```

### 性能分析

```sql
-- 查看执行计划
EXPLAIN SELECT * FROM users WHERE age = 30;
EXPLAIN ANALYZE SELECT * FROM users WHERE age = 30;

-- 输出示例
+---------------------+---------+------+--------------+----------------+
| id                  | task    | est. | operator info| actRows        |
+---------------------+---------+------+--------------+----------------+
| IndexReader_6       | root    | 1.00 | index:idx_age| 1              |
| └─IndexRangeScan_5  | cop[tikv]| 1.00 | range:[30,30]| 1              |
+---------------------+---------+------+--------------+----------------+
```

## Region 和 Raft 实验

### Region 分布查看

```bash
# 使用 pd-ctl 工具
tiup ctl pd -u http://127.0.0.1:2379 -i

# 查看 Region 信息
>> region
{
  "count": 10,
  "regions": [
    {
      "id": 1,
      "start_key": "t1_r1",
      "end_key": "t1_r2",
      "leader": {"store_id": 1},
      "peers": [1, 2, 3]
    }
  ]
}

# 查看 Store 信息
>> store
{
  "count": 3,
  "stores": [
    {
      "store": {"id": 1, "address": "127.0.0.1:20160"},
      "status": {"region_count": 5, "leader_count": 3}
    }
  ]
}
```

### Raft 状态查看

```bash
# 查看 TiKV Raft 状态
curl http://127.0.0.1:20180/status
{
  "raft_state": {
    "hard_state": {
      "term": 10,
      "vote": 1,
      "commit": 100
    },
    "applied": 100
  }
}
```

## HTAP 实验

### TiFlash 副本创建

```sql
-- 创建 TiFlash 副本
ALTER TABLE users SET TIFLASH REPLICA 1;

-- 查看 TiFlash 副本状态
SHOW TABLE users TIFLASH REPLICA;

-- 强制查询走 TiFlash
SET SESSION tidb_isolation_read_engines = 'tiflash';
SELECT COUNT(*) FROM users;
```

### HTAP 性能对比

```sql
-- 行存查询（TiKV）
SET SESSION tidb_isolation_read_engines = 'tikv';
EXPLAIN ANALYZE SELECT COUNT(*), AVG(age) FROM users WHERE age > 25;
-- 执行时间：100ms

-- 列存查询（TiFlash）
SET SESSION tidb_isolation_read_engines = 'tiflash';
EXPLAIN ANALYZE SELECT COUNT(*), AVG(age) FROM users WHERE age > 25;
-- 执行时间：20ms（5x 加速）
```

## 性能调优

### 参数配置

```sql
-- 调整 Region 大小
SET CONFIG tikv `coprocessor.region-max-size` = '96MiB';

-- 调整 TiDB Server 并发度
SET CONFIG tidb `performance.max-procs` = 8;

-- 开启 Coprocessor 缓存
SET CONFIG tikv `coprocessor.copr-cache.capacity-mb` = 1000;
```

### 监控面板

```bash
# 访问 Grafana 监控
http://127.0.0.1:3000

# 关键监控指标
- PD: Region Health, Store Balance
- TiKV: QPS, Latency P99, Raft Propose Rate
- TiFlash: MPP Query Count, Memory Usage
```

## 与 CockroachDB 实验对比

| 维度 | TiDB | CockroachDB |
|------|------|------------|
| 部署工具 | TiUP / TiDB Operator | cockroach start / K8s |
| SQL 客户端 | MySQL 客户端 | cockroach sql |
| Region 查看 | SHOW TABLE ... REGIONS | SHOW RANGES FROM TABLE |
| 执行计划 | EXPLAIN ANALYZE | EXPLAIN (DISTSQL) |
| 监控面板 | Grafana | Prometheus + Grafana |
| HTAP 实验 | TiFlash 副本 | 不支持 |

## 要点总结

- TiUP 工具快速部署本地集群，TiDB Operator 部署 K8s 集群
- Region 查看：`SHOW TABLE ... REGIONS`
- HTAP 实验：TiFlash 列存查询性能提升 5x+
- 性能调优：调整 Region 大小、并发度、缓存
- 监控：Grafana 面板监控 PD/TiKV/TiFlash

## 思考题

1. TiDB 的 Region 大小（96MB）相比 CockroachDB 的 Range（512MB），在热点数据处理上有何差异？
2. TiFlash 的列存查询在什么场景下性能提升最明显？OLTP 查询是否适合走 TiFlash？
3. TiDB 的 Grafana 监控相比 CockroachDB 的监控面板，在易用性和指标丰富度上有何差异？