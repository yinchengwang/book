# RisingWave 实践实验

## 学习目标

- 掌握 RisingWave 的 Docker Compose 部署
- 熟练使用 SQL 创建 SOURCE、MATERIALIZED VIEW
- 理解时间窗口和 JOIN 在流处理中的实际效果

## 正文

### 1. 环境准备

#### 实验 1.1：Docker Compose 部署

```bash
# 创建 docker-compose.yml
cat > docker-compose.yml << 'EOF'
version: '3.8'

services:
  meta:
    image: risingwave/risingwave:v1.10.0
    command: "meta-node"
    ports:
      - "5690:5690"
    environment:
      - META_ADDR=127.0.0.1:5690
    volumes:
      - meta:/tmp/risingwave/meta

  frontend:
    image: risingwave/risingwave:v1.10.0
    command: "frontend-node"
    ports:
      - 4566:4566
    environment:
      - META_ADDR=127.0.0.1:5690
    depends_on:
      - meta

  compute:
    image: risingwave/risingwave:v1.10.0
    command: "compute-node"
    ports:
      - 5688:5688
    environment:
      - META_ADDR=127.0.0.1:5690
    depends_on:
      - meta

  minio:
    image: minio/minio
    command: server /data --console-address ":9001"
    environment:
      - MINIO_ROOT_PASSWORD=password
      - MINIO_ROOT_USER=password
    ports:
      - 9000:9000
      - 9001:9001

volumes:
  meta:
EOF

# 启动集群
docker-compose up -d
```

#### 实验 1.2：连接 RisingWave

```bash
# 使用内置 psql 连接
docker exec -it risingwave-frontend-1 psql -h 127.0.0.1 -p 4566 -U root

# 或使用本地 psql
psql -h localhost -p 4566 -U root
```

### 2. SOURCE 实验

#### 实验 2.1：创建 Kafka 数据源

```sql
-- 创建 Kafka SOURCE
CREATE SOURCE IF NOT EXISTS orders (
    order_id BIGINT,
    customer_id BIGINT,
    amount DECIMAL,
    created_at TIMESTAMP
) WITH (
    connector = 'kafka',
    topic = 'orders',
    properties.bootstrap.server = 'localhost:9092',
    scan.startup.mode = 'earliest'
) FORMAT PLAIN ENCODE JSON;

-- 查看 SOURCE 定义
DESCRIBE orders;

-- 查看 SOURCE 状态
SELECT * FROM rw_sources;
```

#### 实验 2.2：创建 CDC SOURCE

```sql
-- PostgreSQL CDC SOURCE
CREATE SOURCE IF NOT EXISTS pg_orders (
    _key BIGINT,
    _value JSONB
) WITH (
    connector = 'postgres-cdc',
    host = 'postgres:5432',
    port = '5432',
    username = 'postgres',
    password = 'postgres',
    database = 'ecommerce',
    schema = 'public',
    table = 'orders'
) FORMAT PLAIN ENCODE JSON;

-- MySQL CDC SOURCE
CREATE SOURCE IF NOT EXISTS mysql_orders (
    _key VARCHAR,
    _value JSONB
) WITH (
    connector = 'mysql-cdc',
    hostname = 'mysql',
    port = '3306',
    username = 'root',
    password = 'password',
    database = 'ecommerce',
    table = 'orders'
) FORMAT PLAIN ENCODE JSON;
```

### 3. 物化视图实验

#### 实验 3.1：简单聚合

```sql
-- 创建物化视图
CREATE MATERIALIZED VIEW order_stats AS
SELECT 
    customer_id,
    COUNT(*) as order_count,
    SUM(amount) as total_amount,
    AVG(amount) as avg_amount,
    MAX(created_at) as last_order_time
FROM orders
GROUP BY customer_id;

-- 查询物化视图
SELECT * FROM order_stats ORDER BY total_amount DESC LIMIT 10;

-- 查看物化视图定义
SELECT definition FROM pg_matviews WHERE matviewname = 'order_stats';
```

#### 实验 3.2：带过滤条件的视图

```sql
-- 创建高价值客户视图
CREATE MATERIALIZED VIEW vip_customers AS
SELECT 
    customer_id,
    COUNT(*) as order_count,
    SUM(amount) as total_spent
FROM orders
WHERE amount > 100
GROUP BY customer_id
HAVING SUM(amount) > 1000;

-- 实时监控
SELECT * FROM vip_customers WHERE order_count > 10;
```

### 4. 时间窗口实验

#### 实验 4.1：滚动窗口

```sql
-- 每小时订单统计
CREATE MATERIALIZED VIEW hourly_orders AS
SELECT 
    TUMBLE(created_at, INTERVAL '1' HOUR) as window,
    COUNT(*) as order_count,
    SUM(amount) as gmv
FROM orders
GROUP BY TUMBLE(created_at, INTERVAL '1' HOUR);

-- 查询最近 3 小时
SELECT * FROM hourly_orders 
WHERE window > NOW() - INTERVAL '3' HOUR
ORDER BY window DESC;
```

#### 实验 4.2：跳跃窗口

```sql
-- 最近 24 小时每小时滚动统计
CREATE MATERIALIZED VIEW daily_sales_hopping AS
SELECT 
    window_start,
    window_end,
    COUNT(*) as order_count,
    SUM(amount) as gmv
FROM HOP(orders, created_at, INTERVAL '1' HOUR, INTERVAL '24' HOUR)
GROUP BY window_start, window_end;

-- 对比不同窗口
SELECT 
    window_start,
    SUM(order_count) OVER (
        PARTITION BY TUMBLE(window_start, INTERVAL '1' DAY)
        ORDER BY window_start
    ) as daily_cumulative
FROM daily_sales_hopping;
```

### 5. JOIN 实验

#### 实验 5.1：流与静态表 JOIN

```sql
-- 创建地区维度表
CREATE TABLE regions (
    region_id BIGINT PRIMARY KEY,
    region_name VARCHAR
);

INSERT INTO regions VALUES 
    (1, '华东'),
    (2, '华南'),
    (3, '华北');

-- 订单与地区关联
CREATE MATERIALIZED VIEW regional_sales AS
SELECT 
    r.region_name,
    COUNT(*) as order_count,
    SUM(o.amount) as gmv
FROM orders o
JOIN regions r ON o.region_id = r.region_id
GROUP BY r.region_name;
```

#### 实验 5.2：流与流 JOIN

```sql
-- 页面浏览流
CREATE SOURCE page_views (
    user_id BIGINT,
    page_id BIGINT,
    view_time TIMESTAMP
) WITH (
    connector = 'kafka',
    topic = 'page_views',
    properties.bootstrap.server = 'localhost:9092'
) FORMAT PLAIN ENCODE JSON;

-- 订单流
CREATE SOURCE order_events (
    user_id BIGINT,
    order_id BIGINT,
    order_time TIMESTAMP
) WITH (
    connector = 'kafka',
    topic = 'orders',
    properties.bootstrap.server = 'localhost:9092'
) FORMAT PLAIN ENCODE JSON;

-- 用户行为关联
CREATE MATERIALIZED VIEW user_journey AS
SELECT 
    p.user_id,
    p.page_id,
    o.order_id,
    o.order_time - p.view_time as time_to_convert
FROM page_views p
JOIN order_events o ON p.user_id = o.user_id
WHERE o.order_time BETWEEN p.view_time AND p.view_time + INTERVAL '1' HOUR';
```

### 6. CDC 实验

#### 实验 6.1：实时同步到 Kafka

```sql
-- 创建源 SOURCE
CREATE SOURCE IF NOT EXISTS customers (
    id BIGINT,
    name VARCHAR,
    email VARCHAR
) WITH (
    connector = 'postgres-cdc',
    host = 'postgres:5432',
    database = 'ecommerce',
    table = 'customers'
) FORMAT PLAIN ENCODE JSON;

-- 同步到 Kafka
CREATE SINK customer_kafka AS
SELECT id, name, email FROM customers
WITH (
    connector = 'kafka',
    topic = 'customer_updates',
    properties.bootstrap.server = 'localhost:9092'
) FORMAT PLAIN ENCODE JSON;
```

## 实验报告

完成以下实验并记录结果：

| 实验 | SQL 操作 | 预期结果 | 实际结果 |
|------|----------|----------|----------|
| 1.1 | Docker Compose 启动 | 集群正常 | |
| 1.2 | psql 连接 | 连接成功 | |
| 2.1 | 创建 Kafka SOURCE | SOURCE 激活 | |
| 2.2 | 创建 CDC SOURCE | CDC 生效 | |
| 3.1 | 创建物化视图 | 视图增量更新 | |
| 3.2 | 创建过滤视图 | 条件过滤正确 | |
| 4.1 | 滚动窗口 | 每小时聚合 | |
| 4.2 | 跳跃窗口 | 滑动聚合 | |
| 5.1 | 流与表 JOIN | 维度关联 | |
| 5.2 | 流与流 JOIN | 事件关联 | |
| 6.1 | CDC 同步 | 数据同步 | |

## 思考题

1. 如何监控物化视图的更新延迟？
2. 在时间窗口实验中，窗口边界的数据如何处理？
3. 流与流 JOIN 中的状态如何清理以避免内存溢出？
