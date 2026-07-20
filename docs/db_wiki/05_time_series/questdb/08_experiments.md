# QuestDB 动手实验

## 环境准备

```bash
# Docker 启动 QuestDB
docker run -d --name questdb \
  -p 8812:8812 \
  -p 9000:9000 \
  -p 9009:9009 \
  questdb/questdb:latest

# Web Console: http://localhost:9000
# PostgreSQL Wire: localhost:8812
# InfluxDB Line Protocol: localhost:9009
```

## 实验 1：SQL 操作

```sql
-- 创建表
CREATE TABLE sensor_data (
    ts TIMESTAMP TIMESTAMP,
    sensor_id INT,
    temperature DOUBLE,
    humidity DOUBLE
) TIMESTAMP(ts);

-- 插入数据
INSERT INTO sensor_data VALUES
    ('2024-01-01T00:00:00.000Z', 1, 22.5, 60.0),
    ('2024-01-01T00:00:01.000Z', 2, 23.0, 58.5),
    ('2024-01-01T01:00:00.000Z', 1, 22.8, 61.0);

-- SAMPLE BY 查询
SELECT
    date_trunc('hour', ts) AS hour,
    sensor_id,
    AVG(temperature) AS avg_temp
FROM sensor_data
SAMPLE BY 1h;
```

## 实验 2：高速摄取

```bash
# InfluxDB Line Protocol 写入
echo "temperature,sensor_id=1,location=beijing value=22.5 $(date +%s)000000000" | \
nc -u localhost 9009

# 观察 Web Console 数据
# http://localhost:9000
```

## 实验 3：窗口函数

```sql
-- 滑动平均
SELECT
    ts,
    sensor_id,
    temperature,
    AVG(temperature) OVER (
        PARTITION BY sensor_id
        ORDER BY ts
        ROWS BETWEEN 4 PRECEDING AND CURRENT ROW
    ) AS moving_avg_5
FROM sensor_data;

-- 排名
SELECT
    ts,
    sensor_id,
    temperature,
    ROW_NUMBER() OVER (
        PARTITION BY sensor_id
        ORDER BY temperature DESC
    ) AS rank
FROM sensor_data;
```

## 实验结果记录

| 实验项目 | 预期结果 | 实际结果 |
|---------|---------|---------|
| 创建表 | 表创建成功 | |
| 插入数据 | 数据入库 | |
| SAMPLE BY | 按时聚合 | |

## 要点总结

- QuestDB 标准 SQL 支持
- SAMPLE BY 简化时序聚合
- 高速 ILP 摄取
- Web Console 可视化