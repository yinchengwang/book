# ClickHouse 动手实验

## 学习目标

- 掌握 ClickHouse 单节点 Docker 部署
- 熟悉建表、导入、聚合查询的完整流程
- 理解物化视图和副本配置的实际操作

## 实验环境准备

### Docker 单节点部署

```bash
# 1. 拉取 ClickHouse 最新镜像
docker pull clickhouse/clickhouse-server:latest

# 2. 启动 ClickHouse 容器
docker run -d \
    --name clickhouse-server \
    -p 8123:8123 \        # HTTP 接口
    -p 9000:9000 \        # Native 接口
    -p 9009:9009 \        # 副本通信
    --ulimit nofile=262144:262144 \
    clickhouse/clickhouse-server:latest

# 3. 等待启动完成
sleep 10

# 4. 验证服务
docker exec clickhouse-server clickhouse-client --query "SELECT 1"

# 5. 查看系统信息
docker exec clickhouse-server clickhouse-client --query "SELECT version()"
```

### 连接 ClickHouse

```bash
# 使用 clickhouse-client 连接
docker exec -it clickhouse-server clickhouse-client

# 或者使用 HTTP 接口
curl http://localhost:8123/query?query=SELECT%201
```

## 实验 1: 创建表与基本操作

### 1.1 创建 MergeTree 表

```sql
-- 连接到 ClickHouse
-- docker exec -it clickhouse-server clickhouse-client

-- 创建测试数据库
CREATE DATABASE IF NOT EXISTS test_db;

-- 创建传感器数据表
CREATE TABLE test_db.sensor_data (
    event_date Date,
    event_time DateTime,
    sensor_id UInt32,
    temperature Float32,
    humidity Float32,
    pressure Float32,
    battery_level UInt8
) ENGINE = MergeTree()
ORDER BY (sensor_id, event_time)
PARTITION BY toYYYYMM(event_date)
SETTINGS index_granularity = 8192;

-- 查看表结构
DESCRIBE TABLE test_db.sensor_data;

-- 查看创建语句
SHOW CREATE TABLE test_db.sensor_data;
```

### 1.2 插入测试数据

```sql
-- 插入单条数据
INSERT INTO test_db.sensor_data VALUES
    ('2024-01-01', '2024-01-01 10:00:00', 1, 22.5, 60.0, 1013.25, 85);

-- 批量插入数据
INSERT INTO test_db.sensor_data VALUES
    ('2024-01-01', '2024-01-01 10:05:00', 1, 22.6, 59.8, 1013.20, 85),
    ('2024-01-01', '2024-01-01 10:10:00', 1, 22.7, 59.5, 1013.15, 84),
    ('2024-01-01', '2024-01-01 10:05:00', 2, 23.0, 58.0, 1013.30, 90),
    ('2024-01-01', '2024-01-01 10:10:00', 2, 23.1, 57.8, 1013.25, 90);

-- 生成大量测试数据（10 万条）
INSERT INTO test_db.sensor_data
SELECT
    toDate('2024-01-01') + toIntervalDay(number % 365),
    toDateTime('2024-01-01 00:00:00') + toIntervalMinute(number % 1440),
    number % 100 + 1 AS sensor_id,
    20.0 + rand() % 100 / 10.0 AS temperature,
    50.0 + rand() % 500 / 10.0 AS humidity,
    1000.0 + rand() % 300 / 10.0 AS pressure,
    rand() % 101 AS battery_level
FROM numbers(100000);
```

### 1.3 查询验证

```sql
-- 查看数据量
SELECT count() FROM test_db.sensor_data;

-- 查看分区
SELECT partition, rows, formatReadableSize(data_compressed_bytes) AS size
FROM system.parts
WHERE table = 'sensor_data' AND database = 'test_db';

-- 基本查询
SELECT
    sensor_id,
    count() AS readings,
    avg(temperature) AS avg_temp,
    min(temperature) AS min_temp,
    max(temperature) AS max_temp
FROM test_db.sensor_data
GROUP BY sensor_id
ORDER BY sensor_id;
```

## 实验 2: 数据导入导出

### 2.1 CSV 文件导入

```bash
# 创建测试 CSV 文件
cat > /tmp/sensor.csv << 'EOF'
event_date,event_time,sensor_id,temperature,humidity,pressure,battery_level
2024-01-01,2024-01-01 08:00:00,1,21.5,62.0,1013.0,100
2024-01-01,2024-01-01 08:00:00,2,22.0,61.0,1013.1,98
2024-01-01,2024-01-01 08:00:00,3,23.5,60.0,1013.2,95
EOF

# 导入 CSV 文件
docker exec -it clickhouse-server clickhouse-client --query="
    INSERT INTO TABLE test_db.sensor_data FORMAT CSV" < /tmp/sensor.csv

# 验证导入
SELECT count() FROM test_db.sensor_data;
```

### 2.2 JSONEachRow 格式导入

```bash
# 创建 JSON 数据文件
cat > /tmp/sensor.json << 'EOF'
{"event_date":"2024-01-01","event_time":"2024-01-01 09:00:00","sensor_id":10,"temperature":24.5,"humidity":55.0,"pressure":1014.0,"battery_level":80}
{"event_date":"2024-01-01","event_time":"2024-01-01 09:00:00","sensor_id":11,"temperature":25.0,"humidity":54.0,"pressure":1014.1,"battery_level":79}
EOF

# 导入 JSON 数据
docker exec -it clickhouse-server clickhouse-client --query="
    INSERT INTO test_db.sensor_data FORMAT JSONEachRow" < /tmp/sensor.json
```

### 2.3 数据导出

```sql
-- 导出到 CSV
docker exec -it clickhouse-server clickhouse-client --query="
    SELECT * FROM test_db.sensor_data WHERE sensor_id = 1 LIMIT 10
    FORMAT CSV" > /tmp/export.csv

-- 导出到 JSON
docker exec -it clickhouse-server clickhouse-client --query="
    SELECT * FROM test_db.sensor_data WHERE sensor_id = 1 LIMIT 10
    FORMAT JSONEachRow" > /tmp/export.json

-- 查看导出文件
cat /tmp/export.csv
cat /tmp/export.json
```

## 实验 3: 聚合查询实战

### 3.1 时间序列聚合

```sql
-- 按小时聚合
SELECT
    toStartOfHour(event_time) AS hour,
    sensor_id,
    count() AS readings,
    avg(temperature) AS avg_temp,
    avg(humidity) AS avg_humidity,
    min(battery_level) AS min_battery
FROM test_db.sensor_data
WHERE event_date = '2024-01-01'
GROUP BY hour, sensor_id
ORDER BY hour, sensor_id
LIMIT 20;

-- 按天聚合（所有传感器汇总）
SELECT
    event_date,
    count() AS total_readings,
    uniqExact(sensor_id) AS sensor_count,
    avg(temperature) AS avg_temp,
    min(temperature) AS min_temp,
    max(temperature) AS max_temp
FROM test_db.sensor_data
GROUP BY event_date
ORDER BY event_date;
```

### 3.2 分位数分析

```sql
-- 温度分位数分析
SELECT
    sensor_id,
    count() AS n,
    avg(temperature) AS mean,
    quantile(0.25)(temperature) AS q25,
    quantile(0.50)(temperature) AS q50,
    quantile(0.75)(temperature) AS q75,
    quantile(0.95)(temperature) AS q95,
    quantile(0.99)(temperature) AS q99
FROM test_db.sensor_data
GROUP BY sensor_id
ORDER BY sensor_id;
```

### 3.3 窗口函数

```sql
-- 计算移动平均（过去 5 个时间点的平均温度）
SELECT
    sensor_id,
    event_time,
    temperature,
    avg(temperature) OVER (
        PARTITION BY sensor_id
        ORDER BY event_time
        ROWS BETWEEN 2 PRECEDING AND 2 FOLLOWING
    ) AS temp_ma5
FROM test_db.sensor_data
WHERE sensor_id <= 3
ORDER BY sensor_id, event_time
LIMIT 30;

-- 计算累计最大值
SELECT
    sensor_id,
    event_time,
    temperature,
    max(temperature) OVER (
        PARTITION BY sensor_id
        ORDER BY event_time
        ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW
    ) AS max_temp_so_far
FROM test_db.sensor_data
WHERE sensor_id = 1
ORDER BY event_time
LIMIT 20;
```

## 实验 4: 物化视图

### 4.1 创建聚合物化视图

```sql
-- 创建预聚合表
CREATE TABLE test_db.sensor_hourly (
    hour DateTime,
    sensor_id UInt32,
    reading_count UInt64,
    temp_sum Float32,
    temp_min Float32,
    temp_max Float32,
    humidity_sum Float32
) ENGINE = SummingMergeTree()
ORDER BY (hour, sensor_id);

-- 创建物化视图
CREATE MATERIALIZED VIEW test_db.sensor_hourly_mv
TO test_db.sensor_hourly
AS
SELECT
    toStartOfHour(event_time) AS hour,
    sensor_id,
    count() AS reading_count,
    sum(temperature) AS temp_sum,
    min(temperature) AS temp_min,
    max(temperature) AS temp_max,
    sum(humidity) AS humidity_sum
FROM test_db.sensor_data
GROUP BY hour, sensor_id;

-- 插入新数据触发物化视图更新
INSERT INTO test_db.sensor_data VALUES
    ('2024-06-01', '2024-06-01 14:30:00', 1, 30.0, 45.0, 1015.0, 70);

-- 查询物化视图
SELECT * FROM test_db.sensor_hourly LIMIT 10;
```

### 4.2 创建去重物化视图

```sql
-- 创建去重目标表
CREATE TABLE test_db.user_events (
    event_date Date,
    event_id UUID,
    user_id UInt64,
    event_type String
) ENGINE = ReplacingMergeTree(event_date)
ORDER BY event_id;

-- 创建物化视图
CREATE MATERIALIZED VIEW test_db.user_events_mv
TO test_db.user_events
AS
SELECT
    toDate(event_time) AS event_date,
    generateUUIDv4() AS event_id,
    sensor_id AS user_id,
    'sensor_data' AS event_type
FROM test_db.sensor_data
WHERE sensor_id BETWEEN 1 AND 10;

-- 查询去重结果
SELECT uniq(user_id) FROM test_db.user_events;
```

## 实验 5: 副本配置

### 5.1 查看当前副本状态

```sql
-- 查看系统副本
SELECT * FROM system.replicas WHERE database = 'test_db';

-- 查看分片信息
SELECT * FROM system.clusters WHERE cluster = 'cluster';

-- 查看活跃 parts
SELECT
    table,
    partition,
    name,
    active,
    rows
FROM system.parts
WHERE database = 'test_db'
ORDER BY table, partition;
```

### 5.2 手动 Merge 操作

```sql
-- 查看待合并的 parts
SELECT
    database,
    table,
    partition,
    count() AS parts_count,
    sum(rows) AS total_rows
FROM system.parts
WHERE database = 'test_db'
  AND active = 0
GROUP BY database, table, partition;

-- 强制触发 Merge（使用 ALTER）
-- 注意：在生产环境中不建议手动触发
ALTER TABLE test_db.sensor_data MATERIALIZE TEMPORARY TABLE;

-- 查看 Merge 进度
SELECT * FROM system.merges WHERE database = 'test_db';
```

### 5.3 清理和优化

```sql
-- 优化表（合并 parts）
OPTIMIZE TABLE test_db.sensor_data FINAL;

-- 查看优化后的 parts
SELECT
    name,
    active,
    rows,
    formatReadableSize(bytes_on_disk) AS disk_size
FROM system.parts
WHERE database = 'test_db'
  AND table = 'sensor_data';

-- 删除过期数据
ALTER TABLE test_db.sensor_data DROP PARTITION '2023-01';
```

## 实验清理

```bash
# 清理测试数据
docker exec -it clickhouse-server clickhouse-client --query="DROP DATABASE IF EXISTS test_db;"

# 停止并删除容器
docker stop clickhouse-server
docker rm clickhouse-server

# 清理临时文件
rm -f /tmp/sensor.csv /tmp/sensor.json /tmp/export.csv /tmp/export.json
```

## 实验报告

完成以下实验报告：

| 实验 | 内容 | 完成情况 |
|------|------|----------|
| 实验 1 | 创建 MergeTree 表，插入 10 万条测试数据 | ☐ |
| 实验 2 | CSV 和 JSON 格式的数据导入导出 | ☐ |
| 实验 3 | 时间序列聚合、分位数、窗口函数查询 | ☐ |
| 实验 4 | 创建 SummingMergeTree 物化视图 | ☐ |
| 实验 5 | 查看副本状态，执行 OPTIMIZE | ☐ |

## 思考题

1. MergeTree 的 `ORDER BY` 键对查询性能有什么影响？
2. 物化视图在数据插入时是否有延迟？如何权衡物化视图的数量？
3. OPTIMIZE TABLE 的使用场景和注意事项是什么？
4. 在实际生产环境中，百万级数据量的表如何优化查询性能？
