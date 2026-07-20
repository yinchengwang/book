# Apache Druid 动手实验

## 学习目标

- 掌握 Druid Docker 单节点部署
- 熟悉 Kafka 摄入数据的基本流程
- 理解 Bitmap 索引和查询优化

## 实验环境准备

### Docker 快速启动

```bash
# 1. 创建网络
docker network create druid-net

# 2. 启动 Zookeeper
docker run -d \
    --name zookeeper \
    --network druid-net \
    -e ZOOKEEPER_CLIENT_PORT=2181 \
    zookeeper:3.8

# 3. 启动 Kafka
docker run -d \
    --name kafka \
    --network druid-net \
    -e KAFKA_ZOOKEEPER_CONNECT=zookeeper:2181 \
    -e KAFKA_LISTENERS=PLAINTEXT://kafka:9092 \
    -e KAFKA_ADVERTISED_LISTENERS=PLAINTEXT://kafka:9092 \
    apache/kafka:3.5.0

# 4. 等待 Kafka 启动
sleep 30

# 5. 创建测试 topic
docker exec kafka kafka-topics.sh \
    --create --topic events \
    --bootstrap-server localhost:9092 \
    --partitions 1 --replication-factor 1
```

### Druid 快速启动（micro-quickstart）

```bash
# 1. 启动 Druid
docker run -d \
    --name druid \
    --network druid-net \
    -p 8888:8888 \
    -p 8082:8082 \
    -p 8081:8081 \
    apache/druid:27.0.0 \
    micro-quickstart

# 2. 等待启动（约 2-3 分钟）
sleep 60

# 3. 验证 Druid 控制台
curl http://localhost:8888/status

# 4. 查看所有服务
docker exec druid ls /opt/druid/var/sv
```

### 访问 Druid 控制台

```
URL: http://localhost:8888
```

## 实验 1: Kafka 数据摄入

### 1.1 准备测试数据

```bash
# 创建测试数据 JSON 文件
cat > /tmp/events.json << 'EOF'
{"timestamp":"2024-01-01T10:00:00Z","user_id":"u001","event_type":"click","page":"home","country":"CN","device":"mobile"}
{"timestamp":"2024-01-01T10:01:00Z","user_id":"u002","event_type":"view","page":"product","country":"US","device":"desktop"}
{"timestamp":"2024-01-01T10:02:00Z","user_id":"u001","event_type":"purchase","page":"checkout","country":"CN","device":"mobile"}
{"timestamp":"2024-01-01T10:03:00Z","user_id":"u003","event_type":"click","page":"search","country":"JP","device":"tablet"}
{"timestamp":"2024-01-01T10:04:00Z","user_id":"u002","event_type":"view","page":"home","country":"US","device":"desktop"}
EOF
```

### 1.2 发送数据到 Kafka

```bash
# 安装 kafka-tools（如果需要）
# macOS: brew install kafka
# Ubuntu: apt install kafka-bin

# 发送数据到 Kafka
cat /tmp/events.json | \
docker exec -i kafka kafka-console-producer.sh \
    --bootstrap-server localhost:9092 \
    --topic events
```

### 1.3 定义 Druid 数据源

```bash
# 通过 Druid 控制台或 API 创建数据源
# 访问 http://localhost:8888 -> Load data -> Kafka

# 或者使用 API 提交 supervisor
curl -X POST -H 'Content-Type: application/json' \
    http://localhost:8888/druid/indexer/v1/supervisor \
    -d '{
  "type": "kafka",
  "spec": {
    "ioConfig": {
      "type": "kafka",
      "consumerProperties": {
        "bootstrap.servers": "kafka:9092"
      },
      "topic": "events",
      "inputFormat": {
        "type": "json"
      }
    },
    "tuningConfig": {
      "type": "kafka",
      "maxRowsInMemory": 1000
    },
    "dataSchema": {
      "dataSource": "events",
      "timestampSpec": {
        "column": "timestamp",
        "format": "auto"
      },
      "dimensionsSpec": {
        "dimensions": ["user_id", "event_type", "page", "country", "device"]
      },
      "metricsSpec": [
        { "type": "count", "name": "count" }
      ],
      "granularitySpec": {
        "segmentGranularity": "DAY",
        "queryGranularity": "MINUTE"
      }
    }
  }
}'
```

### 1.4 验证数据摄入

```bash
# 查看 supervisor 状态
curl http://localhost:8888/druid/indexer/v1/supervisor/events/status

# 查看 segments
curl http://localhost:8888/druid/v2/datasources

# 查看数据源信息
curl http://localhost:8888/druid/coordinator/v1/datasources/events
```

## 实验 2: 查询数据

### 2.1 GroupBy 查询

```bash
# GroupBy 查询
curl -X POST \
    http://localhost:8888/druid/v2?pretty \
    -H 'Content-Type: application/json' \
    -d '{
  "queryType": "groupBy",
  "dataSource": "events",
  "intervals": ["2024-01-01/2024-01-02"],
  "granularity": "all",
  "dimensions": ["event_type", "country"],
  "aggregations": [
    { "type": "count", "name": "events" }
  ]
}'
```

### 2.2 TimeSeries 查询

```bash
# 时间序列查询
curl -X POST \
    http://localhost:8888/druid/v2?pretty \
    -H 'Content-Type: application/json' \
    -d '{
  "queryType": "timeseries",
  "dataSource": "events",
  "intervals": ["2024-01-01/2024-01-02"],
  "granularity": "minute",
  "aggregations": [
    { "type": "count", "name": "events" }
  ]
}'
```

### 2.3 TopN 查询

```bash
# TopN 查询（按事件数排名）
curl -X POST \
    http://localhost:8888/druid/v2?pretty \
    -H 'Content-Type: application/json' \
    -d '{
  "queryType": "topN",
  "dataSource": "events",
  "intervals": ["2024-01-01/2024-01-02"],
  "granularity": "all",
  "dimension": "user_id",
  "threshold": 10,
  "metric": "events",
  "aggregations": [
    { "type": "count", "name": "events" }
  ]
}'
```

### 2.4 Scan 查询

```bash
# Scan 查询（返回原始行）
curl -X POST \
    http://localhost:8888/druid/v2?pretty \
    -H 'Content-Type: application/json' \
    -d '{
  "queryType": "scan",
  "dataSource": "events",
  "intervals": ["2024-01-01/2024-01-02"],
  "columns": ["timestamp", "user_id", "event_type", "page"],
  "limit": 100
}'
```

## 实验 3: Bitmap 索引

### 3.1 了解 Bitmap 索引结构

Druid 使用 Roaring Bitmap 存储维度值的索引：

```bash
# 查看 segment 元数据
curl http://localhost:8888/druid/v2/datasources/events?interval=2024-01-01/2024-01-02
```

### 3.2 验证 Bitmap 过滤效果

```bash
# 查询 1：country = CN（Bitmap 过滤）
curl -X POST \
    http://localhost:8888/druid/v2?pretty \
    -H 'Content-Type: application/json' \
    -d '{
  "queryType": "groupBy",
  "dataSource": "events",
  "intervals": ["2024-01-01/2024-01-02"],
  "granularity": "all",
  "filter": {
    "type": "selector",
    "dimension": "country",
    "value": "CN"
  },
  "aggregations": [
    { "type": "count", "name": "events" }
  ]
}'

# 查询 2：country = CN AND device = mobile（Bitmap AND）
curl -X POST \
    http://localhost:8888/druid/v2?pretty \
    -H 'Content-Type: application/json' \
    -d '{
  "queryType": "groupBy",
  "dataSource": "events",
  "intervals": ["2024-01-01/2024-01-02"],
  "granularity": "all",
  "filter": {
    "type": "and",
    "fields": [
      { "type": "selector", "dimension": "country", "value": "CN" },
      { "type": "selector", "dimension": "device", "value": "mobile" }
    ]
  },
  "aggregations": [
    { "type": "count", "name": "events" }
  ]
}'
```

### 3.3 验证 IN 查询（Bitmap OR）

```bash
# 查询 3：country IN (CN, US, JP)（Bitmap OR）
curl -X POST \
    http://localhost:8888/druid/v2?pretty \
    -H 'Content-Type: application/json' \
    -d '{
  "queryType": "groupBy",
  "dataSource": "events",
  "intervals": ["2024-01-01/2024-01-02"],
  "granularity": "all",
  "filter": {
    "type": "in",
    "dimension": "country",
    "values": ["CN", "US", "JP"]
  },
  "aggregations": [
    { "type": "count", "name": "events" }
  ]
}'
```

## 实验 4: Lambda 架构

### 4.1 批量数据摄入

```bash
# 准备批量数据
cat > /tmp/batch_events.json << 'EOF'
{"timestamp":"2024-01-01T00:00:00Z","user_id":"b001","event_type":"login","page":"home","country":"CN","device":"desktop"}
{"timestamp":"2024-01-01T00:01:00Z","user_id":"b002","event_type":"login","page":"home","country":"US","device":"mobile"}
{"timestamp":"2024-01-01T00:02:00Z","user_id":"b001","event_type":"logout","page":"home","country":"CN","device":"desktop"}
EOF

# 创建本地批量摄入任务
curl -X POST \
    http://localhost:8888/druid/indexer/v1/task \
    -H 'Content-Type: application/json' \
    -d '{
  "type": "index",
  "spec": {
    "ioConfig": {
      "type": "index",
      "inputSource": {
        "type": "inline",
        "data": "{\"timestamp\":\"2024-01-01T00:00:00Z\",\"user_id\":\"b001\",\"event_type\":\"login\",\"page\":\"home\",\"country\":\"CN\",\"device\":\"desktop\"}\n{\"timestamp\":\"2024-01-01T00:01:00Z\",\"user_id\":\"b002\",\"event_type\":\"login\",\"page\":\"home\",\"country\":\"US\",\"device\":\"mobile\"}"
      },
      "inputFormat": { "type": "json" }
    },
    "tuningConfig": {
      "type": "index",
      "maxRowsInMemory": 1000
    },
    "dataSchema": {
      "dataSource": "events",
      "timestampSpec": { "column": "timestamp", "format": "auto" },
      "dimensionsSpec": {
        "dimensions": ["user_id", "event_type", "page", "country", "device"]
      },
      "metricsSpec": [
        { "type": "count", "name": "count" }
      ],
      "granularitySpec": {
        "segmentGranularity": "DAY",
        "queryGranularity": "MINUTE"
      }
    }
  }
}'
```

### 4.2 验证 Lambda 查询

```bash
# 查询所有数据（实时 + 批量）
curl -X POST \
    http://localhost:8888/druid/v2?pretty \
    -H 'Content-Type: application/json' \
    -d '{
  "queryType": "groupBy",
  "dataSource": "events",
  "intervals": ["2024-01-01/2024-01-03"],
  "granularity": "all",
  "dimensions": ["event_type"],
  "aggregations": [
    { "type": "count", "name": "events" }
  ]
}'
```

## 实验清理

```bash
# 停止 Druid supervisor
curl -X DELETE http://localhost:8888/druid/indexer/v1/supervisor/events

# 停止容器
docker stop druid kafka zookeeper
docker rm druid kafka zookeeper

# 清理网络
docker network rm druid-net

# 清理临时文件
rm -f /tmp/events.json /tmp/batch_events.json
```

## 实验报告

| 实验 | 内容 | 完成情况 |
|------|------|----------|
| 实验 1 | Kafka 实时摄入数据到 Druid | ☐ |
| 实验 2 | GroupBy/TimeSeries/TopN/Scan 查询 | ☐ |
| 实验 3 | Bitmap 索引过滤（AND/OR/NOT） | ☐ |
| 实验 4 | Lambda 架构（实时 + 批量数据） | ☐ |

## 思考题

1. Druid 的 Segment 与 ClickHouse 的 MergeTree Part 有什么本质区别？
2. 为什么 Druid 的实时摄入比 ClickHouse 更快？
3. 在什么情况下应该使用 TopN 而不是 GroupBy？
4. Lambda 架构如何保证实时层和批量层的数据一致性？
