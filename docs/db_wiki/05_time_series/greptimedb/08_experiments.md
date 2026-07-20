# GreptimeDB 动手实验

## 环境准备

```bash
# Docker Compose 部署
cat > docker-compose.yml << 'EOF'
version: '3'
services:
  greptime:
    image: greptime/greptimedb:latest
    ports:
      - 4000:4000
      - 4001:4001
    command: "standalone start"
EOF

docker-compose up -d

# 连接: mysql -h localhost -P 4000
```

## 实验 1：SQL 操作

```sql
-- 创建表
CREATE TABLE monitor (
    ts TIMESTAMP TIME INDEX,
    host STRING TAG,
    cpu DOUBLE,
    memory DOUBLE
);

-- 插入数据
INSERT INTO monitor VALUES
    ('2024-01-01 00:00:00', 'server1', 75.5, 60.0),
    ('2024-01-01 00:00:01', 'server2', 65.0, 55.0),
    ('2024-01-01 00:01:00', 'server1', 76.0, 61.0);

-- 查询
SELECT * FROM monitor WHERE host = 'server1';
```

## 实验 2：PromQL 查询

```promql
# 通过 HTTP API
curl -G "http://localhost:4000/v1/promql" \
  --data-urlencode 'query=http_requests_total{job="api"}'

# 范围查询
curl -G "http://localhost:4000/v1/promql" \
  --data-urlencode 'query=http_requests_total{job="api"}[5m]' \
  --data-urlencode 'start=1704067200' \
  --data-urlencode 'end=1704070800' \
  --data-urlencode 'step=60s'
```

## 实验 3：数据保留

```sql
-- 设置 TTL
ALTER TABLE monitor WITH (
    ttl = '7d'
);

-- 查看表信息
SHOW CREATE TABLE monitor;
```

## 实验结果记录

| 实验项目 | 预期结果 | 实际结果 |
|---------|---------|---------|
| 创建表 | 表创建成功 | |
| 插入数据 | 数据入库 | |
| PromQL 查询 | 返回数据 | |

## 要点总结

- GreptimeDB SQL + PromQL 双接口
- 云原生 Kubernetes 部署
- PromQL 100% 兼容
- 国产开源，文档友好