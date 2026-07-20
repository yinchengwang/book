# openGauss 实验指南

## 学习目标

- 掌握 openGauss 的部署和运维工具
- 学会性能分析和调优方法
- 对比 openGauss 与 PostgreSQL 的实验差异

## 部署方式

### 1. 快速部署（Docker）

```bash
# 拉取 openGauss 镜像
docker pull opengauss/opengauss:latest

# 启动单节点
docker run -d --name opengauss \
  -p 5432:5432 \
  -e GS_PASSWORD=Test@123 \
  opengauss/opengauss:latest

# 连接 openGauss
gsql -d postgres -h 127.0.0.1 -p 5432 -U gaussdb -W 'Test@123'
```

### 2. 生产部署（OM）

```bash
# 安装 OM（运维管理工具）
# 初始化配置文件
gs_om -t init -X clusterconfig.xml

# 启动集群
gs_om -t start

# 查看状态
gs_om -t status
```

## 基本操作

### 连接集群

```bash
# gsql 客户端连接
gsql -d postgres -h 192.168.1.100 -p 5432 -U gaussdb

# 查看版本
SELECT version();

# 查看存储引擎
SELECT * FROM pg_tablespace;
```

### 存储引擎实验

```sql
-- 创建 ASTORE 表（行存）
CREATE TABLE t_astore (id INT, name TEXT);

-- 创建 CSTORE 表（列存）
CREATE TABLE t_cstore (id INT, name TEXT) WITH (ORIENTATION = COLUMN);

-- 创建 MOT 表（内存表）
CREATE FOREIGN TABLE t_mot (id INT, name TEXT)
SERVER mot_server OPTIONS (orientation 'row');
```

### 性能分析

```sql
-- 查看执行计划
EXPLAIN ANALYZE SELECT * FROM t_astore WHERE id = 1;

-- 查看慢查询
SELECT * FROM dbe_perf.SLOW_QUERY_INFO;

-- 查看系统统计
SELECT * FROM pg_stat_statements;
```

## 实验清单

### 实验 1：MOT 内存表性能对比

```sql
-- 创建 ASTORE 表
CREATE TABLE t_heap (id INT PRIMARY KEY, val TEXT);
INSERT INTO t_heap SELECT generate_series(1, 100000), 'test';

-- 创建 MOT 表
CREATE FOREIGN TABLE t_mot (id INT PRIMARY KEY, val TEXT)
SERVER mot_server OPTIONS (orientation 'row');
INSERT INTO t_mot SELECT generate_series(1, 100000), 'test';

-- 对比性能
EXPLAIN ANALYZE SELECT * FROM t_heap WHERE id = 50000;
EXPLAIN ANALYZE SELECT * FROM t_mot WHERE id = 50000;
```

### 实验 2：列存压缩

```sql
-- 创建列存表
CREATE TABLE t_cstore (id INT, name TEXT, val TEXT)
WITH (ORIENTATION = COLUMN);

-- 插入数据
INSERT INTO t_cstore SELECT i, 'name_' || i, 'value_' || i
FROM generate_series(1, 100000) AS i;

-- 查看表大小
SELECT pg_size_pretty(pg_total_relation_size('t_cstore'));
```

### 实验 3：主备同步

```sql
-- 主库查看
SELECT * FROM pg_stat_replication;

-- 备库查看
SELECT * FROM pg_stat_wal_receiver;

-- 手动切换
gs_ctl switchover -D /data/standby;
```

## 性能调优

### 参数调优

```sql
-- 查看参数
SHOW work_mem;
SHOW shared_buffers;

-- 调整内存参数
ALTER SYSTEM SET work_mem = '64MB';
ALTER SYSTEM SET shared_buffers = '2GB';

-- 调整并行参数
ALTER SYSTEM SET query_dop = 4;
ALTER SYSTEM SET enable_parallel_ddl = on;
```

### 监控指标

| 指标 | 说明 | 正常范围 |
|------|------|----------|
| CPU 使用率 | 节点 CPU 使用率 | < 80% |
| 内存使用率 | 节点内存使用率 | < 80% |
| 磁盘使用率 | 磁盘空间使用率 | < 70% |
| QPS | 每秒查询数 | 取决于场景 |
| TPS | 每秒事务数 | 取决于场景 |

## 与 PostgreSQL 实验对比

| 实验 | openGauss | PostgreSQL |
|------|-----------|------------|
| 部署工具 | gs_om / gsql | pg_ctl / psql |
| 监控系统 | dbe_perf 视图 | pg_stat_* 视图 |
| 慢查询分析 | dbe_perf.SLOW_QUERY_INFO | pg_stat_statements |
| 存储引擎 | ASTORE/CSTORE/MOT | Heap |

## 要点总结

- openGauss 提供 Docker 和 gs_om 部署
- 实验重点：三存储引擎对比、列存压缩、主备同步
- 性能调优：work_mem、shared_buffers、query_dop
- 与 PG 相比：gsql vs psql，gs_om vs pg_ctl

## 思考题

1. openGauss 的 MOT 内存表在性能测试中，相比 ASTORE 行存表有多少倍的性能提升？
2. 列存引擎（CSTORE）的压缩率如何？对查询性能有何影响？
3. 如何通过 dbe_perf 视图诊断 openGauss 的性能瓶颈？