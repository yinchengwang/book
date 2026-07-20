# VictoriaMetrics 动手实验

## 环境准备

```bash
# Docker 单节点启动
docker run -d --name victoriametrics \
  -p 8428:8428 \
  -v vm-data:/storage \
  victoriametrics/victoria-metrics:latest

# 监控 UI: http://localhost:8428
```

## 实验 1：写入数据

```bash
# remote_write API（Prometheus 风格）
curl -X POST "http://localhost:8428/api/v1/write" \
  --data-binary '
http_requests_total{job="api", instance="1"} 100
http_requests_total{job="api", instance="2"} 150
cpu_usage{host="server1"} 75.5
'

# 通过 vmctl 工具
vmctl prometheus --help
```

## 实验 2：PromQL 查询

```bash
# 基础查询
curl -G "http://localhost:8428/api/v1/query" \
  --data-urlencode 'query=http_requests_total{job="api"}'

# 范围查询
curl -G "http://localhost:8428/api/v1/query_range" \
  --data-urlencode 'query=http_requests_total{job="api"}' \
  --data-urlencode 'start=1704067200' \
  --data-urlencode 'end=1704070800' \
  --data-urlencode 'step=60'
```

## 实验 3：MetricsQL 扩展

```bash
# 预测
curl -G "http://localhost:8428/api/v1/query" \
  --data-urlencode 'query=predict_linear(cpu_usage[1h], 3600)'

# 异常检测
curl -G "http://localhost:8428/api/v1/query" \
  --data-urlencode 'query=anomaly_rate(cpu_usage[5m], mode="stddev", k=3)'
```

## 实验结果记录

| 实验项目 | 预期结果 | 实际结果 |
|---------|---------|---------|
| remote_write 写入 | 数据写入成功 | |
| PromQL 查询 | 返回数据 | |
| MetricsQL 扩展 | 预测/异常结果 | |

## 要点总结

- remote_write 兼容 Prometheus
- MetricsQL 是 PromQL 超集
- 高压缩率，节省存储
- 单节点支持百万指标