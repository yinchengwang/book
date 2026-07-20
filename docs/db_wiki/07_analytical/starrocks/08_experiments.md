# StarRocks 动手实验

## 学习目标

- 掌握 StarRocks Docker 单节点部署
- 熟悉建表、数据导入、物化视图的完整流程
- 理解主键更新和数据湖查询

## 实验环境准备

### Docker 单节点部署

```bash
# 1. 拉取 StarRocks 镜像
docker pull starrocks/all-in-one-ubuntu:3.2-latest

# 2. 启动 StarRocks（单节点）
docker run -d \
    --name starrocks \
    -p 9030:9030 \      # MySQL 协议端口
    -p 8030:8030 \      # HTTP 端口
    -p 8040:8040 \      # BE HTTP 端口
    -v /tmp/starrocks-data:/opt/starrocks/fe/meta \
    -v /tmp/starrocks-be:/opt/starrocks/be/storage \
    starrocks/all-in-one-ubuntu:3.2-latest

# 3. 等待启动完成
sleep 30

# 4. 验证服务
docker exec starrocks mysql -uroot -h127.0.0.1 -P9030 -e "SHOW FRONTENDS; SHOW BACKENDS;"
```

### 连接 StarRocks

```bash
# 使用 MySQL 客户端连接
docker exec -it starrocks mysql -uroot -h127.0.0.1 -P9030

# 或在宿主机连接
mysql -h127.0.0.1 -P9030 -uroot
```

## 实验 1: 建表与基本操作

### 1.1 创建数据库

```sql
-- 创建测试数据库
CREATE DATABASE IF NOT EXISTS test_db;

USE test_db;

-- 查看数据库
SHOW DATABASES;
```

### 1.2 创建 OLAP 表

```sql
-- 创建 Duplicate 模型表
CREATE TABLE dup_orders (
    order_id BIGINT,
    order_time DATETIME,
    user_id BIGINT,
    product_id BIGINT,
    amount DECIMAL(12, 2),
    quantity INT,
    status VARCHAR(20)
)
ENGINE = OLAP
DUPLICATE KEY(order_id, order_time)
PARTITION BY RANGE (order_time) (
    PARTITION p202401 VALUES LESS THAN ('2024-02-01'),
    PARTITION p202402 VALUES LESS THAN ('2024-03-01'),
    PARTITION p202403 VALUES LESS THAN ('2024-04-01')
)
DISTRIBUTED BY HASH(order_id) BUCKETS 10
PROPERTIES (
    "replication_num" = "1",
    "storage_medium" = "HDD"
);

-- 查看表结构
DESC dup_orders;

-- 查看建表语句
SHOW CREATE TABLE dup_orders;
```

### 1.3 创建聚合模型表

```sql
-- 创建聚合模型表（预聚合）
CREATE TABLE ads_hourly_metrics (
    stat_hour DATETIME,
    product_id BIGINT,
    pv BIGINT,
    uv BIGINT,
    order_count BIGINT,
    order_amount DECIMAL(16, 2)
)
ENGINE = OLAP
AGGREGATE KEY(stat_hour, product_id)
DISTRIBUTED BY HASH(stat_hour) BUCKETS 10
PROPERTIES (
    "replication_num" = "1"
);
```

### 1.4 创建主键模型表

```sql
-- 创建主键模型表（支持更新）
CREATE TABLE dim_user (
    user_id BIGINT PRIMARY KEY,
    username VARCHAR(50),
    email VARCHAR(100),
    vip_level TINYINT,
    status TINYINT,
    last_login DATETIME,
    update_time DATETIME
)
ENGINE = OLAP
PRIMARY KEY(user_id)
ORDER BY user_id
DISTRIBUTED BY HASH(user_id) BUCKETS 10
PROPERTIES (
    "replication_num" = "1"
);
```

## 实验 2: 数据导入

### 2.1 Stream Load（本地文件）

```bash
# 创建测试数据文件
cat > /tmp/orders.csv << 'EOF'
1001,2024-01-01 10:00:00,10001,2001,99.99,2,paid
1002,2024-01-01 10:05:00,10002,2002,199.99,1,paid
1003,2024-01-01 10:10:00,10003,2001,49.99,1,pending
1004,2024-01-01 10:15:00,10001,2003,299.99,1,shipped
1005,2024-01-01 10:20:00,10004,2002,149.99,3,paid
EOF

# Stream Load 导入
curl -X PUT -u root: \
    -H "Expect:100-continue" \
    -H "column_separator:," \
    -H "columns: order_id,order_time,user_id,product_id,amount,quantity,status" \
    -T /tmp/orders.csv \
    http://127.0.0.1:8030/api/test_db/dup_orders/_stream_load

# 验证导入
mysql -h127.0.0.1 -P9030 -uroot -e "SELECT COUNT(*) FROM test_db.dup_orders;"
```

### 2.2 INSERT 导入

```sql
-- INSERT 单条
INSERT INTO dup_orders VALUES
    (1006, '2024-01-01 11:00:00', 10005, 2004, 399.99, 2, 'paid');

-- INSERT 多条
INSERT INTO dup_orders VALUES
    (1007, '2024-01-01 11:05:00', 10006, 2001, 59.99, 1, 'pending'),
    (1008, '2024-01-01 11:10:00', 10007, 2002, 89.99, 2, 'shipped');

-- INSERT FROM SELECT
INSERT INTO ads_hourly_metrics
SELECT
    DATE_TRUNC('hour', order_time) AS stat_hour,
    product_id,
    COUNT(*) AS pv,
    COUNT(DISTINCT user_id) AS uv,
    0 AS order_count,
    0 AS order_amount
FROM dup_orders
GROUP BY 1, 2;
```

### 2.3 Routine Load（Kafka）

```sql
-- 注意：需要先启动 Kafka，以下示例仅供参考

-- 创建 Routine Load
CREATE ROUTINE LOAD test_db.kafka_orders ON dup_orders
COLUMNS (order_id, order_time, user_id, product_id, amount, quantity, status)
PROPERTIES (
    "desired_concurrent_number" = "3",
    "max_batch_interval" = "20",
    "max_batch_rows" = "100000"
)
FROM KAFKA (
    "kafka_broker_list" = "kafka:9092",
    "kafka_topic" = "orders"
);

-- 查看 Routine Load 状态
SHOW ROUTINE LOAD;

-- 暂停/恢复
PAUSE ROUTINE LOAD test_db.kafka_orders;
RESUME ROUTINE LOAD test_db.kafka_orders;
```

## 实验 3: 物化视图

### 3.1 创建同步物化视图

```sql
-- 创建预聚合物化视图
CREATE MATERIALIZED VIEW mv_product_hourly AS
SELECT
    DATE_TRUNC('hour', order_time) AS hour,
    product_id,
    SUM(amount) AS total_amount,
    COUNT(*) AS order_count,
    COUNT(DISTINCT user_id) AS unique_users
FROM dup_orders
GROUP BY 1, 2;

-- 查看物化视图
SHOW MATERIALIZED VIEW;

-- 刷新物化视图
REFRESH MATERIALIZED VIEW mv_product_hourly;
```

### 3.2 创建异步物化视图

```sql
-- 创建异步刷新物化视图
CREATE MATERIALIZED VIEW mv_hourly_agg
BUILD REFRESH ASYNC
AS
SELECT
    DATE_TRUNC('hour', order_time) AS hour,
    product_id,
    SUM(amount) AS total_amount,
    COUNT(*) AS cnt
FROM dup_orders
GROUP BY 1, 2;

-- 查看刷新状态
SHOW MATERIALIZED VIEW WHERE Name = 'mv_hourly_agg';

-- 手动刷新
ALTER MATERIALIZED VIEW mv_hourly_agg REFRESH;

-- 设置定时刷新
ALTER MATERIALIZED VIEW mv_hourly_agg REFRESH ASYNC EVERY(INTERVAL 1 HOUR);
```

### 3.3 验证查询改写

```sql
-- 查询原始表
SELECT
    product_id,
    SUM(amount) AS total
FROM dup_orders
WHERE order_time >= '2024-01-01 00:00:00'
GROUP BY product_id;

-- 查询物化视图（自动改写）
SELECT
    product_id,
    SUM(total_amount) AS total
FROM mv_product_hourly
WHERE hour >= '2024-01-01 00:00:00'
GROUP BY product_id;

-- 查看查询改写
EXPLAIN SELECT
    product_id,
    SUM(amount) AS total
FROM dup_orders
WHERE DATE_TRUNC('hour', order_time) >= '2024-01-01 00:00:00'
GROUP BY product_id;
```

## 实验 4: 主键更新

### 4.1 Upsert 操作

```sql
-- 插入用户数据
INSERT INTO dim_user VALUES
    (10001, 'alice', 'alice@test.com', 1, 1, NOW(), NOW()),
    (10002, 'bob', 'bob@test.com', 2, 1, NOW(), NOW());

-- 查询用户
SELECT * FROM dim_user WHERE user_id IN (10001, 10002);

-- Upsert（主键存在则更新，不存在则插入）
INSERT INTO dim_user (user_id, username, vip_level, status, last_login, update_time)
VALUES
    (10001, 'alice_updated', 3, 1, NOW(), NOW()),
    (10003, 'charlie', 'charlie@test.com', 1, 1, NOW(), NOW())
ON DUPLICATE KEY UPDATE
    username = VALUES(username),
    vip_level = VALUES(vip_level),
    last_login = VALUES(last_login),
    update_time = VALUES(update_time);

-- 验证更新
SELECT * FROM dim_user WHERE user_id IN (10001, 10002, 10003);
```

### 4.2 UPDATE 操作

```sql
-- 更新指定行
UPDATE dim_user
SET vip_level = 3,
    update_time = NOW()
WHERE user_id = 10001
  AND vip_level = 2;

-- 批量更新
UPDATE dim_user
SET status = 2,
    update_time = NOW()
WHERE vip_level = 1;

-- 查看更新结果
SELECT * FROM dim_user;
```

### 4.3 DELETE 操作

```sql
-- 删除指定行
DELETE FROM dim_user
WHERE user_id = 10003;

-- 删除后查询验证
SELECT * FROM dim_user WHERE user_id = 10003;

-- 条件删除
DELETE FROM dim_user
WHERE status = 0
  AND last_login < DATE_SUB(NOW(), INTERVAL 30 DAY);
```

## 实验 5: 数据湖查询

### 5.1 创建外部 Catalog（模拟）

```sql
-- 注意：需要有实际的 Hive/Iceberg 环境才能执行

-- 创建 Hive Catalog
-- CREATE EXTERNAL CATALOG hive_catalog
-- PROPERTIES (
--     "type" = "hive",
--     "hive.metastore.uris" = "thrift://hive-metastore:9083"
-- );

-- 创建 Iceberg Catalog
-- CREATE EXTERNAL CATALOG iceberg_catalog
-- PROPERTIES (
--     "type" = "iceberg",
--     "iceberg.catalog.type" = "hive",
--     "iceberg.catalog.hive.metastore.uris" = "thrift://hive-metastore:9083"
-- );

-- 查看 Catalog
SHOW CATALOGS;

-- 切换到外部数据源
-- USE CATALOG hive_catalog;
-- SHOW DATABASES FROM hive_catalog;
```

### 5.2 联邦查询示例

```sql
-- 假设有 Hive 外部表 orders
-- 可以在 StarRocks 中直接查询
-- SELECT * FROM hive_catalog.hive_db.orders LIMIT 10;

-- 联邦 Join 查询示例
-- SELECT
--     s.order_id,
--     s.amount,
--     h.customer_name
-- FROM starrocks_db.orders s
-- LEFT JOIN hive_catalog.hive_db.customers h
--     ON s.customer_id = h.id
-- WHERE s.order_time >= '2024-01-01';
```

## 实验清理

```sql
-- 删除测试数据
USE test_db;
DROP TABLE IF EXISTS dup_orders;
DROP TABLE IF EXISTS ads_hourly_metrics;
DROP TABLE IF EXISTS dim_user;
DROP MATERIALIZED VIEW IF EXISTS mv_product_hourly;
DROP MATERIALIZED VIEW IF EXISTS mv_hourly_agg;
DROP DATABASE IF EXISTS test_db;

-- 退出 MySQL
EXIT;
```

```bash
# 停止并删除容器
docker stop starrocks
docker rm starrocks

# 清理临时文件
rm -f /tmp/orders.csv

# 清理数据目录
rm -rf /tmp/starrocks-data /tmp/starrocks-be
```

## 实验报告

| 实验 | 内容 | 完成情况 |
|------|------|----------|
| 实验 1 | 创建 Duplicate/Aggregate/Primary Key 表 | ☐ |
| 实验 2 | Stream Load / INSERT 数据导入 | ☐ |
| 实验 3 | 创建同步/异步物化视图，验证查询改写 | ☐ |
| 实验 4 | 主键 Upsert / UPDATE / DELETE 操作 | ☐ |
| 实验 5 | 数据湖 Catalog 和联邦查询 | ☐ |

## 思考题

1. Duplicate/Aggregate/Primary Key 三种表模型分别在什么场景下使用？
2. 同步物化视图和异步物化视图的性能差异和使用场景是什么？
3. 主键模型的 Upsert 操作如何保证原子性？
4. 在什么场景下 StarRocks 的数据湖查询比直接查询 Hive 更高效？
