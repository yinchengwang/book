# Redpanda 核心特性

## 学习目标

- 掌握 Redpanda 与 Kafka 的协议兼容性
- 了解 Redpanda 的事务支持和 Exactly-Once 语义
- 熟悉 Tiered Storage 和监控体系

## 正文

### 1. Kafka 协议兼容性

Redpanda 实现了与 Apache Kafka 协议的完整兼容：

| Kafka 特性 | Redpanda 支持 | 说明 |
|------------|---------------|------|
| Producer API | ✅ | 二进制兼容 |
| Consumer API | ✅ | 二进制兼容 |
| Admin API | ✅ | 主题创建/删除/配置 |
| Connect API | ✅ |  connector 框架 |
| REST Proxy | ✅ | HTTP 接口 |

**优势**：
- 现有 Kafka 应用无需修改代码即可迁移
- 可以直接使用 Kafka 生态工具（kafkacat、schema-registry-ui 等）

```mermaid
graph LR
    subgraph "应用层"
        A["Kafka Client<br/>(Java/Python/Go)"]
    end
    
    subgraph "协议兼容层"
        R["Redpanda<br/>Kafka Compatible"]
        K["Kafka Broker"]
    end
    
    A --> R
    A --> K
    
    K -.->|"Kafka 迁移"| R
```

### 2. Schema Registry

Redpanda 内置 Schema Registry，支持 Avro 和 Protobuf：

```bash
# 注册 Schema
curl -X POST http://localhost:8081/subjects/orders-value/versions \
  -H "Content-Type: application/vnd.schemaregistry.v1+json" \
  -d '{
    "schema": "{\"type\":\"record\",\"name\":\"Order\",\"fields\":[{\"name\":\"id\",\"type\":\"long\"},{\"name\":\"amount\",\"type\":\"double\"}]}"
  }'

# 获取 Schema
curl http://localhost:8081/subjects/orders-value/versions/latest
```

**功能**：
- Schema 版本管理
- Schema 兼容性检查
- 自动序列化/反序列化

### 3. 事务支持（Exactly-Once）

```mermaid
sequenceDiagram
    participant P as Producer
    participant T as Transaction Coordinator
    participant P0 as Partition-0
    participant P1 as Partition-1
    
    P->>T: Begin Transaction
    T-->>P: Transaction ID
    
    P->>P0: Produce Record (txid: abc)
    P->>P1: Produce Record (txid: abc)
    
    P0-->>P: ACK
    P1-->>P: ACK
    
    P->>T: Commit Transaction
    T->>P0: Mark Committed
    T->>P1: Mark Committed
    T-->>P: Commit ACK
```

**幂等性保证**：
- 开启 `enable.idempotence: true` 后，单分区消息不重复
- 事务开启后，跨分区消息原子提交

### 4. 消费者组

```mermaid
graph TB
    subgraph "Consumer Group: order-processor"
        C1["Consumer-1<br/>Partition 0,1"]
        C2["Consumer-2<br/>Partition 2,3"]
        C3["Consumer-3<br/>Partition 4,5"]
    end
    
    subgraph "Topic: orders (6 partitions)"
        P0["P0"]
        P1["P1"]
        P2["P2"]
        P3["P3"]
        P4["P4"]
        P5["P5"]
    end
    
    C1 --> P0
    C1 --> P1
    C2 --> P2
    C2 --> P3
    C3 --> P4
    C3 --> P5
```

**消费者组特性**：
- Rebalance 自动分区再分配
- Offset 自动提交或手动提交
- 多消费者负载均衡

### 5. Tiered Storage

Redpanda 支持将历史数据卸载到 S3 兼容存储：

```bash
# 配置 Tiered Storage
rpk topic create my-topic --redpanda.remote.write=true --redpanda.remote.read=true
```

**优势**：
- 降低本地存储成本
- 支持超大规模数据保留
- 热数据本地访问，冷数据远程读取

```mermaid
graph TB
    subgraph "Tiered Storage 架构"
        H["热数据<br/>本地 NVMe SSD"]
        C["冷数据<br/>S3/兼容对象存储"]
    end
    
    subgraph "访问模式"
        R["读取请求"]
        H <--> R
        C <-->|"按需读取"| R
    end
```

### 6. 监控指标

Redpanda 提供丰富的 Prometheus 指标：

```bash
# 暴露指标
curl http://localhost:9644/metrics
```

**关键指标**：

| 指标名称 | 说明 |
|----------|------|
| `redpanda_produce_messages_total` | 生产消息总数 |
| `redpanda_consume_messages_total` | 消费消息总数 |
| `redpanda_kafka_request_latencies_seconds` | 请求延迟 |
| `redpanda_disk_used_bytes` | 磁盘使用量 |
| `redpanda_rpc_request_errors_total` | RPC 错误数 |

## 要点总结

1. **Kafka 兼容**：二进制兼容，现有应用零修改迁移
2. **Schema Registry**：内置 Avro/Protobuf 支持
3. **Exactly-Once**：完整事务支持，跨分区原子提交
4. **消费者组**：自动 Rebalance，负载均衡
5. **Tiered Storage**：冷热数据分离，降低存储成本
6. **Prometheus 监控**：丰富指标，便于运维

## 思考题

1. Redpanda 的 Kafka 兼容性与 Kafka 的 Broker 兼容模式有何区别？
2. 在什么场景下需要开启事务支持？事务对性能有何影响？
3. Tiered Storage 如何实现冷数据的按需读取和缓存？
