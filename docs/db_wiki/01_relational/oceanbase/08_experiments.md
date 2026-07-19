# OceanBase 实验指南

## 学习目标

- 掌握 OceanBase 的部署和运维工具
- 学会性能分析和调优方法
- 理解 OceanBase 的监控和诊断

## 部署方式

### 1. 快速部署（Docker）

```bash
# 拉取 OceanBase 镜像
docker pull oceanbase/oceanbase-ce:latest

# 启动单节点集群
docker run -d --name ob \
  -p 2881:2881 \
  oceanbase/oceanbase-ce:latest

# 连接 OceanBase
mysql -h127.0.0.1 -P2881 -uroot@test -p
```

### 2. 生产部署（OBD）

```bash
# 安装 OBD（OceanBase Deployer）
pip install ob-deploy

# 初始化配置文件
obd cluster deploy mycluster -c config.yaml

# 启动集群
obd cluster start mycluster
```

## 基本操作

### 连接集群

```bash
# MySQL 客户端连接
mysql -h192.168.1.100 -P2881 -uroot@sys -p

# 创建租户（资源隔离）
CREATE RESOURCE UNIT unit1 MAX_CPU 4, MIN_CPU 2;
CREATE RESOURCE POOL pool1 UNIT 'unit1', UNIT_NUM 1;
CREATE TENANT tenant1 REPLICA_NUM = 1, RESOURCE_POOL_LIST = ('pool1');
```

### 分区表操作

```sql
-- 创建 Hash 分区表
CREATE TABLE orders (
    id INT PRIMARY KEY,
    user_id INT,
    amount DECIMAL(10,2)
) PARTITION BY HASH(user_id) PARTITIONS 16;

-- 查看分区信息
SHOW CREATE TABLE orders;

-- 插入数据
INSERT INTO orders VALUES (1, 100, 100.00);

-- 查询分区路由
EXPLAIN SELECT * FROM orders WHERE user_id = 100;
```

### 性能分析

```sql
-- 查看执行计划
EXPLAIN EXTENDED SELECT * FROM orders WHERE user_id = 100;

-- 查看实时执行统计
SHOW QUERY_RESPONSE_TIME;

-- 查看慢查询
SELECT * FROM oceanbase.GV$OB_SQL_AUDIT
WHERE query_time > 1000000; -- 超过 1 秒
```

## 实验清单

### 实验 1：分区表分片

```sql
-- 创建 Hash 分区表
CREATE TABLE test_hash (
    id INT PRIMARY KEY,
    name VARCHAR(100)
) PARTITION BY HASH(id) PARTITIONS 8;

-- 插入 1000 条数据
INSERT INTO test_hash SELECT seq, 'name' FROM generate_series(1, 1000) AS seq;

-- 查看分区分布
SELECT partition_id, count(*) FROM test_hash GROUP BY partition_id;
```

### 实验 2：Paxos 共识

```sql
-- 查看副本分布
SHOW PARAMETERS LIKE '%server_list%';

-- 手动切换主副本
ALTER SYSTEM SWITCH REPLICA partition_id = 'p0' leader = '192.168.1.101:2882';

-- 查看主副本信息
SELECT * FROM oceanbase.GV$OB_PARTITIONS WHERE role = 'LEADER';
```

### 实验 3：HTAP 混合负载

```sql
-- 创建混合存储表
CREATE TABLE orders (
    id INT PRIMARY KEY,
    user_id INT,
    amount DECIMAL(10,2)
) WITH COLUMN_GROUP = 'all';

-- OLTP 查询（行存）
SELECT * FROM orders WHERE id = 123;

-- OLAP 查询（列存）
SELECT user_id, SUM(amount) FROM orders GROUP BY user_id;
```

## 性能调优

### 参数调优

```sql
-- 查看参数
SHOW PARAMETERS LIKE '%memory%';

-- 调整内存参数
ALTER SYSTEM SET memstore_limit_percentage = 50;

-- 调整并发参数
ALTER SYSTEM SET max_concurrency = 128;
```

### 监控指标

| 指标 | 说明 | 正常范围 |
|------|------|----------|
| CPU 使用率 | 节点 CPU 使用率 | < 80% |
| 内存使用率 | 节点内存使用率 | < 80% |
| 磁盘使用率 | 磁盘空间使用率 | < 70% |
| QPS | 每秒查询数 | 取决于场景 |
| TPS | 每秒事务数 | 取决于场景 |

## 与 TiDB 实验对比

| 实验 | OceanBase | TiDB |
|------|-----------|------|
| 部署工具 | OBD（OceanBase Deployer） | TiUP |
| 监控系统 | OCP（OceanBase Cloud Platform） | TiDB Dashboard + Grafana |
| 慢查询分析 | GV$OB_SQL_AUDIT | INFORMATION_SCHEMA.SLOW_QUERY |
| 分片查看 | SHOW PARTITIONS | SHOW TABLE REGIONS |

## 要点总结

- OceanBase 提供 OBD 部署工具和 Docker 快速部署
- 支持租户级资源隔离
- 分区表是核心分片机制
- 性能分析通过 SQL AUDIT 和执行计划
- 与 TiDB 相比：OBD vs TiUP，OCP vs TiDB Dashboard

## 思考题

1. OceanBase 的租户隔离机制如何实现？与 PostgreSQL 的 schema 隔离有何差异？
2. 如何通过 OceanBase 的 SQL AUDIT 诊断性能瓶颈？
3. OceanBase 的分区表分片与 TiDB 的 Region 分片，在运维上有何差异？