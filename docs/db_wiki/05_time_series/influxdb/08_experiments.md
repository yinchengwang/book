# InfluxDB 动手实验

## 环境准备

```bash
# Docker 启动 InfluxDB
docker run -d --name influxdb \
  -p 8086:8086 \
  influxdb:latest

# 访问 http://localhost:8086
# 创建 API Token: Settings → Tokens
```

## 实验 1：写入数据

```bash
# 使用 InfluxDB Line Protocol 写入
# 通过 HTTP API
curl -i -X POST "http://localhost:8086/api/v2/write?org=myorg&bucket=sensor&precision=ns" \
  -H "Authorization: Token mytoken" \
  -H "Content-Type: text/plain" \
  --data-binary '
temperature,sensor_id=1,location=beijing value=22.5
temperature,sensor_id=2,location=shanghai value=25.0
humidity,sensor_id=1,location=beijing value=60.0
'

# 或使用 influx CLI
docker exec influxdb influx write \
  -b sensor -o myorg -t mytoken \
  'temperature,sensor_id=1 value=22.5'
```

## 实验 2：查询数据

```bash
# 使用 InfluxQL 查询
docker exec influxdb influx query '
from(bucket: "sensor")
  |> range(start: -1h)
  |> filter(fn: (r) => r._measurement == "temperature")
  |> filter(fn: (r) => r.sensor_id == "1")
  |> aggregateWindow(every: 1m, fn: mean)
'
```

## 实验 3：连续查询

```sql
-- 创建数据库
CREATE DATABASE "sensor_data";

-- 创建连续查询
CREATE CONTINUOUS QUERY "cq_cpu_avg" ON "sensor_data"
BEGIN
    SELECT mean(value) AS avg_value
    INTO "cpu_avg"
    FROM "temperature"
    GROUP BY time(5m), sensor_id
END;

-- 查看连续查询
SHOW CONTINUOUS QUERIES;
```

## 实验结果记录

| 实验项目 | 预期结果 | 实际结果 |
|---------|---------|---------|
| 写入 Line Protocol | 数据写入成功 | |
| 查询最近 1 小时 | 返回数据 | |
| 创建连续查询 | 查询自动执行 | |

## 要点总结

- Line Protocol 是 InfluxDB 的数据格式
- InfluxQL 类 SQL 易学
- HTTP API 便于程序集成
- Telegraf 提供丰富数据源