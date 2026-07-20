# TimescaleDB 动手实验

## 环境准备

```bash
# Docker 启动 TimescaleDB
docker run -d --name timescaledb \
  -p 5432:5432 \
  -e POSTGRES_PASSWORD=password \
  timescale/timescaledb:latest-pg15

# 连接
psql -h localhost -p 5432 -U postgres -d postgres
```

## 实验 1：创建 Hypertable

```sql
-- 创建普通表
CREATE TABLE sensor_data (
    time        TIMESTAMPTZ NOT NULL,
    sensor_id   INTEGER,
    temperature DOUBLE PRECISION,
    humidity    DOUBLE PRECISION
);

-- 转换为 Hypertable
SELECT create_hypertable('sensor_data', 'time',
    chunk_time_interval => INTERVAL '1 day');

-- 插入数据
INSERT INTO sensor_data VALUES
    (NOW(), 1, 22.5, 60.0),
    (NOW(), 2, 23.0, 58.5),
    (NOW() - INTERVAL '1 hour', 1, 21.0, 62.0);

-- 查看 Chunk
SELECT show_chunks('sensor_data');
```

## 实验 2：压缩实验

```sql
-- 启用压缩
ALTER TABLE sensor_data SET (
    timescaledb.compress,
    timescaledb.compress_segmentby = 'sensor_id'
);

-- 添加压缩策略（1 天后压缩）
SELECT add_compression_policy('sensor_data', INTERVAL '1 day');

-- 查看压缩状态
SELECT
    chunk_name,
    before_compression_total_bytes,
    after_compression_total_bytes
FROM timescaledb_information.compression_stats;
```

## 实验 3：连续聚合

```sql
-- 创建连续聚合
CREATE MATERIALIZED VIEW sensor_hourly
WITH (timescaledb.continuous) AS
SELECT
    time_bucket('1 hour', time) AS hour,
    sensor_id,
    AVG(temperature) AS avg_temp,
    MAX(temperature) AS max_temp,
    MIN(temperature) AS min_temp
FROM sensor_data
GROUP BY hour, sensor_id;

-- 查询聚合数据
SELECT * FROM sensor_hourly ORDER BY hour DESC LIMIT 10;
```

## 实验结果记录

| 实验项目 | 预期结果 | 实际结果 |
|---------|---------|---------|
| 创建 Hypertable | 自动分区 | |
| 插入数据 | 正常插入 | |
| 查看 Chunk | 1 个 Chunk | |
| 压缩 | 空间减少 40%+ | |

## 要点总结

- Hypertable 是 TimescaleDB 的核心概念
- 压缩可节省 40-90% 空间
- 连续聚合自动增量更新
- SQL 完全兼容 PostgreSQL