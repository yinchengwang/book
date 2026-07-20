# Apache Druid 架构设计

## 学习目标

- 理解 Druid 的列式 Segment 存储结构
- 掌握 Druid 的 Lambda 架构设计
- 了解 Druid 节点类型和数据摄入流程

## Segment 列式存储

Druid 的核心是 Segment 数据结构，它是 Druid 查询性能的关键。

```mermaid
graph TB
    subgraph "Segment 文件结构"
        S1["filename.zip"]
        S1 --> S2["loadSpec"]
        S1 --> S3["intervals (时间范围)"]
        S1 --> S4["columns (列信息)"]
        S1 --> S5["aggregators (聚合器)"]
        S1 --> S6["index.zip"]
    end

    S3 --> S3_1["interval: 2024-01-01/2024-01-02"]
    S4 --> S4_1["__time: LONG"]
    S4 --> S4_2["device: STRING"]
    S4 --> S4_3["country: STRING"]
    S4 --> S4_4["latency_ms: DOUBLE"]

    S6 --> S6_1["bitmap 索引"]
```

### Segment 格式

```json
{
  "type": "s3_zip",
  "bucket": "druid-storage",
  "key": "wikipedia/2024-01-01/2024-01-01_0.zip",

  "intervals": ["2024-01-01T00:00:00.000Z/2024-01-02T00:00:00.000Z"],

  "columns": {
    "__time":    { "type": "LONG",   "size": 1000000 },
    "device":    { "type": "STRING", "size": 500000 },
    "country":   { "type": "STRING", "size": 500000 },
    "latency":   { "type": "DOUBLE", "size": 8000000 }
  },

  "aggregators": {
    "count":     { "type": "count" },
    "latency_sum": { "type": "doubleSum", "fieldName": "latency" },
    "latency_min": { "type": "doubleMin", "fieldName": "latency" }
  },

  "index.zip": {
    "bitmap": {
      "device=mobile":  "RBM",
      "device=desktop": "RBM",
      "country=CN":     "RBM"
    }
  }
}
```

### 列存储设计

```mermaid
graph LR
    subgraph "列式压缩"
        R1["Row 1: mobile, CN, 50ms"]
        R2["Row 2: desktop, US, 100ms"]
        R3["Row 3: mobile, CN, 30ms"]
    end

    subgraph "按列存储"
        C1["device: [mobile, desktop, mobile]"]
        C2["country: [CN, US, CN]"]
        C3["latency: [50, 100, 30]"]
    end

    subgraph "Bitmap 索引"
        B1["mobile: {0, 2}"]
        B2["desktop: {1}"]
        B3["CN: {0, 2}"]
        B4["US: {1}"]
    end
```

## Lambda 架构

Druid 采用 Lambda 架构，同时处理实时数据和历史数据。

```mermaid
graph TB
    subgraph "数据源"
        K[Kafka/Kinesis]
        B[Batch 数据源]
    end

    subgraph "实时层 Real-time"
        R1[Tranquility Service]
        R2[Stream Push]
        R3[Memory Segment]
        R4["实时查询 (< 分钟)"]
    end

    subgraph "批处理层 Batch"
        H1[Hadoop/Spark]
        H2[批处理索引]
        H3[Historical Segment]
    end

    subgraph "Deep Storage"
        D[S3/HDFS]
    end

    K --> R1
    R1 --> R2
    R2 --> R3
    R3 --> R4

    B --> H1
    H1 --> H2
    H2 --> D
    D --> H3

    R3 --> D
    H3 --> R4

    R4 --> Q[Query Broker]
```

### 实时层设计

```mermaid
graph LR
    subgraph "Tranquility"
        T[Tranquility Server]
        T --> T1[接收实时数据]
        T --> T2[内存聚合]
        T --> T3[发布 Segment"]
    end

    subgraph "MiddleManager/Peon"
        MM[MiddleManager]
        MM --> P1[Peon 1: Kafka Index Task]
        MM --> P2[Peon 2: Kafka Index Task]

        P1 --> S1[实时 Segment]
        P2 --> S2[实时 Segment]
    end

    T --> MM
```

## 数据摄入机制

### Kafka Indexing Service

```yaml
# ingestion/spec.json
{
  "type": "kafka",
  "dataSchema": {
    "dataSource": "pageviews",
    "parser": {
      "type": "string",
      "parseSpec": {
        "format": "json",
        "timestampSpec": { "column": "timestamp" },
        "dimensionsSpec": {
          "dimensions": ["page", "user", "country"]
        }
      }
    },
    "metricsSpec": [
      { "type": "count", "name": "views" },
      { "type": "doubleSum", "name": "latency", "fieldName": "latency_ms" }
    ]
  },
  "tuningConfig": {
    "taskCount": 1,
    "replicas": 0,
    "taskDuration": "PT1H"
  },
  "ioConfig": {
    "topic": "pageviews",
    "consumerProperties": { "bootstrap.servers": "kafka:9092" }
  }
}
```

### 摄入流程

```mermaid
sequenceDiagram
    participant K as Kafka
    participant P as Peon Task
    participant M as MiddleManager
    participant D as Deep Storage
    participant H as Historical

    K->>P: 消费数据流
    P->>P: 内存聚合
    Note over P: 实时 Segment 构建
    P->>D: 推送完成 Segment
    D->>H: Historical 加载
    H->>H: Segment 可查询
```

## 节点类型

### 协调节点（Overlord/Coordinator）

```mermaid
graph TB
    subgraph "Overlord (摄入控制)"
        O1[接受任务请求]
        O2[分配 Peon Task]
        O3[监控任务状态]
    end

    subgraph "Coordinator (数据管理)"
        C1[检查 Segment 分布]
        C2[负载均衡]
        C3[副本管理]
        C4[历史数据清理]
    end

    O1 --> Z[ZooKeeper]
    C1 --> Z
    O2 --> Z
```

### 数据节点

```mermaid
graph TB
    subgraph "Historical Node"
        H1[Segment 加载]
        H2[磁盘 I/O]
        H3[批量扫描]
        H4[内存缓存]
    end

    subgraph "MiddleManager"
        MM1[任务容器管理]
        MM1 --> P1[Peon 1]
        MM1 --> P2[Peon 2]
        MM1 --> P3[Peon N]
    end

    subgraph "Broker"
        B1[查询路由]
        B2[结果合并]
        B3[缓存]
    end
```

### 节点职责表

| 节点类型 | 职责 | 资源需求 |
|---------|------|----------|
| Coordinator | 管理 Segment 分布、负载均衡 | 低（1-2 核） |
| Overlord | 管理摄入任务 | 低（1-2 核） |
| Historical | 存储和查询历史 Segment | 高（内存密集） |
| MiddleManager | 运行实时摄入任务 | 中（计算密集） |
| Broker | 接收查询、路由、合并结果 | 中（网络密集） |

## 位图索引

Druid 使用 Roaring Bitmap 加速过滤查询。

```mermaid
graph LR
    subgraph "过滤查询"
        Q["WHERE country='CN'"]
    end

    subgraph "Roaring Bitmap"
        R["country=CN Bitmap"]
        R --> R1["Container 0: {0, 2, 1000}"]
        R --> R2["Container 1: {5, 88, 256}"]
        R --> R3["Container 15: {100, 200}"]
    end

    subgraph "快速定位"
        R1 --> F1["读取列数据"]
        F1 --> R2
    end
```

### Bitmap 索引压缩

```cpp
// Roaring Bitmap 压缩类型

// 1. Array Container (低密度)
// 存储: [0, 5, 100] -> 3 * 2bytes = 6 bytes
// 适用: 基数 < 4096

// 2. Bitmap Container (高密度)
// 存储: [0-65535] -> 65536 bits = 8KB
// 适用: 基数 > 4096

// 3. Run-Length Encoding (连续值)
// 存储: [0-1000] -> run-length 编码
// 适用: 连续值序列
```

### 过滤执行

```sql
-- Druid 过滤查询
SELECT
    page,
    SUM(latency) AS total_latency
FROM pageviews
WHERE country = 'CN'
  AND device = 'mobile'
  AND __time >= '2024-01-01'
  AND __time < '2024-01-02'
GROUP BY page
LIMIT 100;

-- 执行流程
-- 1. country='CN' -> Bitmap A
-- 2. device='mobile' -> Bitmap B
-- 3. A AND B -> Bitmap C
-- 4. 时间范围过滤 -> 扫描列数据
-- 5. 聚合计算
```

## 架构设计要点

```mermaid
graph TB
    A[Druid 架构] --> B["✓ 列式存储 + Bitmap 索引"]
    A --> C["✓ Lambda 架构 (实时 + 历史)"]
    A --> D["✓ 不可变 Segment"]
    A --> E["✓ Schema-on-read"]
    A --> F["✓ 预聚合计算"]

    B --> B1["查询性能优异"]
    C --> C1["实时性 < 1 秒"]
    D --> D1["写入放大"]
    E --> E1["灵活 Schema"]
    F --> F1["减少重复计算"]
```

## 要点总结

1. **Segment 存储**：列式存储 + Bitmap 索引，支持快速过滤
2. **Lambda 架构**：实时层处理最近数据，历史层处理历史数据
3. **节点分工**：Coordinator/Overlord/Broker/Historical 各司其职
4. **数据摄入**：支持 Kafka Push/Pull，Tranquility 流式摄入
5. **位图索引**：Roaring Bitmap 高效压缩，支持位运算
6. **不可变性**：Segment 写入后不可变，简化并发控制

## 思考题

1. Druid 的 Segment 和 ClickHouse 的 MergeTree Part 有什么异同？
2. 为什么 Druid 选择 Roaring Bitmap 而不是普通 Bitmap？
3. Lambda 架构的实时层和批处理层如何保证数据一致性？
4. MiddleManager/Peon 架构相比直接启动进程有什么优势？
