# Druid 查询执行引擎

## 学习目标

- 理解 Druid 的分布式查询执行流程
- 掌握 Broker 查询路由与结果合并机制
- 理解物化视图与预聚合的原理
- 分析与项目 algo/ 模块的关联与借鉴点

## 分布式查询架构

Druid 采用 Broker-Historical 架构，查询由 Broker 节点路由到多个 Historical 节点并行执行。

### 查询执行流程

```mermaid
sequenceDiagram
    participant C as Client (SQL/API)
    participant B as Broker
    participant H1 as Historical 1
    participant H2 as Historical 2
    participant DS as Deep Storage

    C->>B: SELECT SUM(latency) GROUP BY city
    B->>B: 解析查询、生成子查询
    B->>B: 查找 Segment 元数据
    B->>H1: 分发子查询
    B->>H2: 分发子查询
    H1->>DS: 加载 Segment
    H2->>DS: 加载 Segment
    H1->>H1: 本地执行查询
    H2->>H2: 本地执行查询
    H1-->>B: 返回部分结果
    H2-->>B: 返回部分结果
    B->>B: 合并各节点结果
    B-->>C: 返回最终结果
```

### 查询引擎架构

```mermaid
graph TB
    subgraph "客户端层"
        C1["Native API<br/>JSON-over-HTTP"]
        C2["SQL API<br/>Calcite SQL 解析"]
    end

    subgraph "Broker 节点"
        B1["Query Planner"]
        B2["Query Distributor"]
        B3["Result Merger"]
    end

    subgraph "查询计划"
        P1["Segment 裁剪<br/>按时间/维度过滤"]
        P2["子查询生成<br/>每个 Segment 一个"]
        P3["并行执行计划"]
    end

    subgraph "执行节点"
        H1["Historical 1<br/>本地执行"]
        H2["Historical 2<br/>本地执行"]
        H3["Historical N<br/>本地执行"]
    end

    subgraph "结果合并"
        R1["部分结果收集"]
        R2["排序/去重"]
        R3["聚合/限制"]
        R4["最终结果"]
    end

    C1 --> B1
    C2 --> B1
    B1 --> P1 --> P2 --> P3
    P3 --> B2
    B2 --> H1
    B2 --> H2
    B2 --> H3
    H1 --> R1
    H2 --> R1
    H3 --> R1
    R1 --> R2 --> R3 --> R4
    R4 --> B3 --> C1
```

## Broker 查询路由

Broker 是 Druid 查询的入口节点，负责解析、路由和合并。

### 查询生命周期

```mermaid
graph LR
    subgraph "Broker 内部流程"
        A["接收查询"] --> B["Segment 裁剪"]
        B --> C["子查询分发"]
        C --> D["部分结果收集"]
        D --> E["结果合并"]
        E --> F["返回结果"]
    end

    subgraph "Segment 裁剪条件"
        S1["时间范围: __time >= '2024-01-01'"]
        S2["维度条件: country = 'CN'"]
        S3["Segment 元数据: interval, size"]
    end

    A --> S1
    A --> S2
    B --> S3
```

**Broker 的 Segment 裁剪流程**：

1. 根据查询的 `__time` 条件，筛选可能包含数据的 Segment
2. 根据维度条件，检查 Segment 的位图索引是否命中
3. 为每个命中的 Segment 生成子查询，分发到对应的 Historical 节点

### 查询类型与路由

| 查询类型 | 路由策略 | 合并方式 |
|---------|---------|----------|
| Timeseries | 时间序列聚合 | 按时间戳合并 |
| TopN | Top K 查询 | 堆排序合并 |
| GroupBy | 分组聚合 | 哈希聚合合并 |
| Scan | 数据扫描 | 行追加合并 |
| Search | 搜索查询 | 位图合并 |

## 查询执行模型

Druid 支持两种查询方式：原生 JSON API 和 SQL（基于 Calcite）。

### 原生查询示例

```json
{
  "queryType": "groupBy",
  "dataSource": "pageviews",
  "granularity": "hour",
  "dimensions": ["device"],
  "aggregations": [
    { "type": "count", "name": "views" },
    { "type": "doubleSum", "name": "total_latency", "fieldName": "latency" }
  ],
  "filter": {
    "type": "and",
    "fields": [
      {
        "type": "selector",
        "dimension": "country",
        "value": "CN"
      },
      {
        "type": "interval",
        "dimension": "__time",
        "intervals": ["2024-01-01T00:00:00.000Z/2024-01-02T00:00:00.000Z"]
      }
    ]
  },
  "limitSpec": {
    "type": "default",
    "limit": 100,
    "columns": [
      { "dimension": "total_latency", "direction": "descending" }
    ]
  }
}
```

### SQL 查询示例

Druid 使用 Apache Calcite 进行 SQL 解析，将 SQL 转换为原生查询：

```sql
SELECT
    TIME_FLOOR(__time, 'PT1H') AS hour,
    device,
    COUNT(*) AS views,
    SUM(latency) AS total_latency
FROM pageviews
WHERE country = 'CN'
  AND __time >= '2024-01-01'
  AND __time < '2024-01-02'
GROUP BY 1, 2
ORDER BY total_latency DESC
LIMIT 100;
```

**SQL 到原生查询的转换**：

```mermaid
graph TB
    subgraph "Calcite 解析"
        SQL["SQL 语句"] --> P["Parser"]
        P --> V["Validator"]
        V --> L["Logical Plan"]
    end

    subgraph "Druid 转换"
        L --> D["Druid Rel"]
        D --> R["Rule 优化"]
        R --> N["Native Query<br/>JSON"]
    end

    subgraph "执行"
        N --> B["Broker 分发"]
        B --> H["Historical 执行"]
    end
```

## 结果合并机制

Druid 的查询结果合并采用分阶段策略。

### Timeseries 合并

```mermaid
graph TB
    subgraph "节点 1 部分结果"
        T1_1["hour: 10:00, count: 100"]
        T1_2["hour: 11:00, count: 150"]
    end

    subgraph "节点 2 部分结果"
        T2_1["hour: 10:00, count: 80"]
        T2_2["hour: 11:00, count: 120"]
    end

    subgraph "合并结果"
        R1["hour: 10:00, count: 180"]
        R2["hour: 11:00, count: 270"]
    end

    T1_1 --> R1
    T2_1 --> R1
    T1_2 --> R2
    T2_2 --> R2
```

### GroupBy 合并

GroupBy 合并采用分桶策略：

```mermaid
graph TB
    subgraph "部分结果（桶 0）"
        G1_1["device=mobile, views=50"]
        G1_2["device=desktop, views=30"]
    end

    subgraph "部分结果（桶 1）"
        G2_1["device=mobile, views=70"]
        G2_2["device=desktop, views=20"]
    end

    subgraph "合并哈希表"
        H["哈希表合并"]
        H --> H1["mobile: 50 + 70 = 120"]
        H --> H2["desktop: 30 + 20 = 50"]
    end

    G1_1 --> H
    G1_2 --> H
    G2_1 --> H
    G2_2 --> H
```

### TopN 合并

TopN 使用堆排序合并：

```mermaid
graph TB
    subgraph "节点 1 TopN"
        N1_1["mobile: 100"]
        N1_2["desktop: 80"]
        N1_3["tablet: 60"]
    end

    subgraph "节点 2 TopN"
        N2_1["mobile: 90"]
        N2_2["desktop: 70"]
        N2_3["tablet: 50"]
    end

    subgraph "全局 TopN（堆排序）"
        HN["大小为 N 的最小堆"]
        HN --> H1["mobile: 190"]
        HN --> H2["desktop: 150"]
        HN --> H3["tablet: 110"]
    end

    N1_1 --> HN
    N1_2 --> HN
    N1_3 --> HN
    N2_1 --> HN
    N2_2 --> HN
    N2_3 --> HN
```

## 物化视图与预聚合

Druid 的预聚合是实时摄入的一部分，在数据摄入阶段完成部分聚合计算。

### 摄入时预聚合

```mermaid
graph TB
    subgraph "原始数据流"
        R1["时间: 10:00, 维度: mobile, 延迟: 50ms"]
        R2["时间: 10:00, 维度: mobile, 延迟: 60ms"]
        R3["时间: 10:00, 维度: desktop, 延迟: 30ms"]
    end

    subgraph "聚合窗口（1 分钟）"
        W1["10:00 ~ 10:01"]
    end

    subgraph "预聚合结果"
        A1["hour: 10:00, mobile, count=2, sum=110"]
        A2["hour: 10:00, desktop, count=1, sum=30"]
    end

    R1 --> W1
    R2 --> W1
    R3 --> W1
    W1 --> A1
    W1 --> A2
```

### 预聚合配置

```json
{
  "dataSchema": {
    "dataSource": "pageviews",
    "timestampSpec": {
      "column": "timestamp",
      "format": "iso"
    },
    "dimensionsSpec": {
      "dimensions": ["page", "device", "country"]
    },
    "metricsSpec": [
      { "type": "count", "name": "views" },
      { "type": "doubleSum", "name": "latency", "fieldName": "latency_ms" },
      { "type": "doubleMin", "name": "min_latency", "fieldName": "latency_ms" },
      { "type": "doubleMax", "name": "max_latency", "fieldName": "latency_ms" }
    ],
    "granularitySpec": {
      "segmentGranularity": "day",
      "queryGranularity": "hour"
    }
  }
}
```

### 聚合函数类型

| 聚合函数 | 类型 | 说明 |
|---------|------|------|
| count | 内置 | 计数 |
| longSum/doubleSum | 内置 | 求和 |
| longMin/doubleMin | 内置 | 最小值 |
| longMax/doubleMax | 内置 | 最大值 |
| doubleFirst/doubleLast | 内置 | 首个/末个值 |
| cardinality | 内置 | 基数估计 |
| hyperUnique | 内置 | HyperLogLog 去重 |
| thetaSketch | 内置 | Theta Sketch 去重 |
| quantiles | 扩展 | 分位数估计 |

### 预聚合的优势

| 维度 | 无预聚合 | 有预聚合 |
|------|---------|----------|
| 存储量 | 1:1（原始数据） | 5:1 ~ 20:1 |
| 查询响应 | 秒级到分钟级 | 毫秒级到秒级 |
| 聚合计算 | 每次查询计算 | 摄入时已计算 |
| 数据量 | 原始行数 | 按时间窗口聚合后 |

### 聚合粒度配置

```json
{
  "granularitySpec": {
    "segmentGranularity": "day",
    "queryGranularity": "hour",
    "rollup": true
  }
}
```

- `segmentGranularity`: Segment 分区粒度（day/week/month）
- `queryGranularity`: 预聚合时间粒度（minute/hour/day）
- `rollup`: 是否开启预聚合（true/false）

## 缓存机制

Druid 使用多级缓存加速重复查询。

### 缓存层级

```mermaid
graph TB
    subgraph "缓存层级"
        subgraph "L1: Broker 缓存"
            B1["查询结果缓存<br/>内存"]
        end

        subgraph "L2: Historical 缓存"
            H1["Segment 数据缓存<br/>内存映射"]
            H2["列数据缓存<br/>解压后"]
        end

        subgraph "L3: 外部缓存"
            E1["Memcached<br/>分布式缓存"]
        end
    end

    Q["查询请求"] --> B1
    B1 -->|"缓存命中"| R["直接返回"]
    B1 -->|"缓存未命中"| H1
    H1 --> H2
    H2 --> E1
```

### 缓存策略

| 缓存类型 | 存储内容 | 过期策略 | 命中率 |
|---------|---------|----------|--------|
| Broker 结果缓存 | 查询最终结果 | 时间过期 | 高（重复查询） |
| Historical 数据缓存 | Segment 列数据 | Segment 生命周期 | 中（热点数据） |
| 外部 Memcached | 查询结果片段 | LRU | 高（分布式） |

## 与项目 algo/ 模块的关联

### 项目聚合实现现状

项目在 `engineering/include/db/core/agg.h` 中实现了基础聚合框架：

```c
// 项目聚合函数接口
typedef struct AggFunc_s {
    int (*init)(AggState *state);
    int (*process)(AggState *state, const void *values, size_t n);
    int (*finalize)(AggState *state, void *result);
} AggFunc;

// 现有聚合函数
AggFunc agg_count;       // 计数
AggFunc agg_sum;         // 求和
AggFunc agg_avg;         // 平均
AggFunc agg_min;         // 最小值
AggFunc agg_max;         // 最大值
```

### 可扩展的聚合算子

借鉴 Druid 的预聚合设计，项目可扩展以下能力：

```c
// 建议新增：engineering/include/db/core/pre_agg.h

// 预聚合时间窗口
typedef struct {
    int64_t window_start;       // 窗口起始时间戳
    int64_t window_end;         // 窗口结束时间戳
    size_t num_dimensions;      // 维度数量
    char **dimension_values;    // 维度值
    double sum_value;           // 预聚合和
    uint64_t count_value;       // 预聚合计数
    double min_value;           // 预聚合最小值
    double max_value;           // 预聚合最大值
} PreAggWindow;

// 预聚合结果写入
int pre_agg_write(const char *table_name, PreAggWindow *window);
```

### 项目 Broker 实现现状

项目在 Phase 9 中实现了分布式协调器和查询路由：

```c
// 项目查询路由接口（engineering/include/db/dist/coordinator.h）
typedef struct Coordinator_s {
    void (*register_node)(NodeInfo *node);
    void (*route_query)(const char *query, NodeInfo **targets, size_t *n);
    void (*merge_results)(void **partials, size_t n, void *final);
} Coordinator;
```

**借鉴 Druid 的扩展点**：

1. **Segment 裁剪**：项目可扩展 Segment 元数据查询，实现类似 Druid 的 Segment 裁剪

```c
// 建议新增：engineering/include/db/core/segment_prune.h

typedef struct {
    int64_t interval_start;     // Segment 时间范围起点
    int64_t interval_end;       // Segment 时间范围终点
    size_t num_rows;            // Segment 行数
    uint8_t *min_max_index;     // MinMax 索引
} SegmentMeta;

// Segment 裁剪：根据查询条件跳过无关 Segment
int segment_prune_filter(SegmentMeta *segments, size_t n,
                          int64_t query_start, int64_t query_end,
                          size_t *pruned_indices, size_t *num_pruned);
```

2. **结果合并**：项目可扩展两阶段合并，支持 Timeseries/TopN/GroupBy 等合并策略

```c
// 建议新增：engineering/include/db/core/result_merge.h

typedef enum {
    MERGE_TIMESERIES,   // 时间序列合并（按时间戳聚合）
    MERGE_TOPN,         // TopN 合并（堆排序）
    MERGE_GROUPBY,      // GroupBy 合并（哈希聚合）
    MERGE_SCAN          // Scan 合并（行追加）
} MergeStrategy;

typedef struct {
    MergeStrategy strategy;
    void (*merge_partial)(void *partials, size_t n, void *result);
    void (*sort_result)(void *result, size_t limit);
} ResultMerger;
```

3. **缓存系统**：项目可引入 Broker 级别的结果缓存和 Segment 级别的数据缓存

```c
// 建议新增：engineering/include/db/core/query_cache.h

typedef struct {
    uint8_t *cache_data;        // 缓存数据
    size_t cache_size;          // 缓存大小
    int64_t expire_time;        // 过期时间
    uint32_t query_hash;        // 查询哈希（用于查找）
} QueryCacheEntry;

// 缓存操作
QueryCacheEntry *query_cache_get(uint32_t query_hash);
void query_cache_put(uint32_t query_hash, const void *data, size_t size);
void query_cache_invalidate(const char *table_name);
```

## 查询执行流程图

```mermaid
graph TB
    subgraph "客户端"
        CLI["SQL / Native API"]
    end

    subgraph "Broker 节点"
        PARSE["SQL 解析<br/>Calcite"]
        PLAN["查询计划<br/>Segment 裁剪"]
        DIST["子查询分发<br/>并行分发"]
        CACHE["结果缓存<br/>检查"]
        MERGE["结果合并<br/>Timeseries/GroupBy/TopN"]
    end

    subgraph "Historical 节点"
        LOCAL["本地查询执行"]
        COL["列式扫描<br/>位图索引"]
        AGG["本地聚合<br/>预聚合数据"]
    end

    subgraph "存储层"
        SEG["Segment 加载<br/>Deep Storage"]
        BITMAP["位图索引<br/>Roaring Bitmap"]
        COMPRESS["解压缩<br/>LZ4/ZSTD"]
    end

    CLI --> PARSE
    PARSE --> PLAN
    PLAN --> CACHE
    CACHE -->|"缓存命中"| CLI
    CACHE -->|"缓存未命中"| DIST
    DIST --> LOCAL
    LOCAL --> COL --> BITMAP
    LOCAL --> AGG
    BITMAP --> COMPRESS
    COMPRESS --> SEG
    LOCAL --> MERGE
    MERGE --> CLI
```

## 要点总结

1. **分布式查询**：Broker 负责路由和合并，Historical 节点负责本地执行
2. **Segment 裁剪**：按时间范围和位图索引跳过无关 Segment
3. **结果合并**：Timeseries 按时间合并，GroupBy 哈希合并，TopN 堆排序合并
4. **SQL 支持**：基于 Apache Calcite，将 SQL 转换为原生 JSON 查询
5. **预聚合**：摄入时按时间窗口聚合，减少存储量和查询计算量
6. **多级缓存**：Broker 结果缓存 + Historical 数据缓存 + 外部 Memcached
7. **项目关联**：项目已有聚合框架和分布式协调器，可扩展 Segment 裁剪、结果合并和缓存系统

## 思考题

1. Druid 的 Broker 在查询合并时，为什么对 GroupBy 使用哈希合并而对 TopN 使用堆排序？
2. 预聚合的 rollup 机制在什么场景下会导致数据精度损失？如何处理？
3. Broker 的 Segment 裁剪能否做到精确裁剪？如果 Segment 的位图索引过期了怎么办？
4. Druid 的查询缓存失效策略是什么？在实时数据不断写入的情况下如何保证缓存一致性？
5. 项目的 `Coordinator` 接口如何扩展以支持类似 Druid 的 Segment 裁剪和结果合并？
6. 如果项目要引入预聚合机制，应该如何设计摄入时的聚合窗口？