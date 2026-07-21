# GreptimeDB 索引策略

## 学习目标

- 理解时序数据库索引设计的核心挑战与解决方案
- 掌握 GreptimeDB 时间索引、倒排索引、Series 索引的设计原理
- 了解多级时间分区索引与布隆过滤器在时序场景的应用
- 对比项目 index/ 模块（BTree/Hash/Bitmap）与 GreptimeDB 索引的异同

## 核心概念

### 时序数据库索引的挑战

```mermaid
graph TB
    subgraph "时序索引核心挑战"
        C1[高写入吞吐] --> S1[索引写入不能阻塞]
        C2[时间范围查询] --> S2[时间分区优化]
        C3[Tag 高基数] --> S3[倒排索引压缩]
        C4[多维度过滤] --> S4[多索引组合]
    end

    subgraph "GreptimeDB 解决方案"
        S1 --> R1[LSM-Tree 追加写]
        S2 --> R2[时间分区 + Time Index]
        S3 --> R3[Tag 倒排索引 + BitMap]
        S4 --> R4[索引选择器 + 合并过滤]
    end
```

**时序数据特点**：
- **时间有序性**：数据按时间戳递增写入，时间范围查询是最常见模式
- **Tag 高基数**：`host_id`、`pod_name` 等标签可能有数万甚至数百万不同值
- **写入密集**：监控场景每秒数百万数据点写入
- **查询模式固定**：时间范围 + Tag 过滤 + 聚合是典型查询

### GreptimeDB 索引体系

```mermaid
graph TB
    subgraph "GreptimeDB 三层索引"
        T1[Time Index<br/>时间索引] --> T1_1[主键索引]
        T1 --> T1_2[范围扫描优化]

        T2[Tag Index<br/>标签索引] --> T2_1[倒排索引]
        T2 --> T2_2[BitMap 过滤]

        T3[Field Index<br/>字段索引] --> T3_1[布隆过滤器]
        T3 --> T3_2[谓词下推]
    end

    subgraph "辅助索引"
        B1[Bloom Filter<br/>快速排除]
        B2[Zone Map<br/>统计信息]
        B3[Min/Max<br/>范围过滤]
    end

    T1 -.-> B2
    T2 -.-> B1
    T3 -.-> B3
```

---

## 一、时间索引（Time Index）

### 1.1 设计原理

**强制时间索引**：GreptimeDB 表定义中必须指定 `TIME INDEX`，时间戳列自动建立索引。

```sql
CREATE TABLE system_metrics (
    ts TIMESTAMP TIME INDEX,        -- 强制时间索引
    host STRING TAG,                -- Tag 列
    cpu DOUBLE,                     -- Field 列
    memory DOUBLE,
    PRIMARY KEY (host)              -- Series Key
);
```

**时间索引特点**：
1. **自动创建**：每个表必须有且仅有一个 TIME INDEX
2. **范围优化**：时间范围查询通过 Time Index 快速定位
3. **分区对齐**：时间索引与 Region 分区配合，实现分区裁剪

### 1.2 时间索引实现

```mermaid
graph LR
    subgraph "写入路径"
        W1[数据写入] --> W2[时间戳排序]
        W2 --> W3[Memtable 缓存]
        W3 --> W4[SST 刷盘]
    end

    subgraph "SST 文件结构"
        S1[Time Range<br/>Min-Max]
        S2[Row Groups<br/>时间对齐]
        S3[Statistics<br/>Zone Map]
    end

    W4 --> S1
    W4 --> S2
    W4 --> S3
```

**SST 文件时间统计**：
```
SST File: data_20240101_0001.parquet
├── Metadata
│   ├── Time Range: [2024-01-01 00:00:00, 2024-01-01 01:00:00]
│   ├── Row Count: 1,000,000
│   └── Size: 50 MB
├── Row Group 1
│   ├── Time Min: 2024-01-01 00:00:00
│   ├── Time Max: 2024-01-01 00:15:00
│   └── Rows: 250,000
└── Row Group 2
    ├── Time Min: 2024-01-01 00:15:00
    ├── Time Max: 2024-01-01 00:30:00
    └── Rows: 250,000
```

**时间范围查询流程**：
```mermaid
sequenceDiagram
    participant Q as 查询请求
    participant M as Meta
    participant R as Region
    participant S as SST

    Q->>M: SELECT * WHERE ts > T1 AND ts < T2
    M->>M: 时间分区裁剪
    M->>R: 定位相关 Region
    R->>R: SST 文件过滤（Zone Map）
    R->>S: 只读取时间范围内的 SST
    S-->>Q: 返回数据
```

### 1.3 多级时间分区索引

```mermaid
graph TB
    subgraph "多级时间分区"
        L1[Year/Month 分区<br/>粗粒度]
        L2[Day 分区<br/>中粒度]
        L3[Hour Segment<br/>细粒度]
    end

    subgraph "查询裁剪示例"
        Q1[查询: 2024-01-15 10:00 ~ 12:00]
        Q1 --> F1[Year=2024 ✓]
        Q1 --> F2[Month=01 ✓]
        Q1 --> F3[Day=15 ✓]
        Q1 --> F4[Hour=10,11,12 ✓]
    end

    L1 --> F1
    L2 --> F2
    L3 --> F4
```

**分区策略**：

| 分区级别 | 时间跨度 | 分区数量 | 查询裁剪效率 |
|---------|---------|---------|-------------|
| Year/Month | 月 | 12/年 | 粗粒度过滤 |
| Day | 天 | 365/年 | 中粒度过滤 |
| Hour | 小时 | 8760/年 | 细粒度定位 |
| Segment | 分钟 | 无限 | 精确定位 |

**分区裁剪效果**：
```sql
-- 查询 2024-01-15 10:00 ~ 12:00 的数据
-- 无分区裁剪：扫描 365 天 × 24 小时 = 8760 个文件
-- 有分区裁剪：只扫描 3 小时 = 3 个文件
-- 裁剪率：99.97%
```

---

## 二、倒排索引（Inverted Index）

### 2.1 Tag 索引设计

**Tag 倒排索引**：Tag 列（如 `host`、`region`）建立倒排索引，实现 Tag 过滤加速。

```mermaid
graph LR
    subgraph "Tag 倒排索引"
        T1[Tag: host = server1]
        T2[Tag: host = server2]
        T3[Tag: region = beijing]
    end

    subgraph "倒排列表"
        L1[Row IDs: 1, 5, 10, 15]
        L2[Row IDs: 2, 6, 11, 16]
        L3[Row IDs: 1, 2, 5, 6]
    end

    T1 --> L1
    T2 --> L2
    T3 --> L3
```

**倒排索引结构**：
```
Tag Index: host
├── "server1" → BitMap [1, 0, 0, 0, 1, 0, 0, 0, 0, 1, ...]
├── "server2" → BitMap [0, 1, 0, 0, 0, 1, 0, 0, 0, 0, ...]
└── "server3" → BitMap [0, 0, 1, 0, 0, 0, 1, 0, 0, 0, ...]

Tag Index: region
├── "beijing"  → BitMap [1, 1, 0, 0, 1, 1, 0, 0, ...]
└── "shanghai" → BitMap [0, 0, 1, 1, 0, 0, 1, 1, ...]
```

### 2.2 BitMap 索引实现

```mermaid
graph TB
    subgraph "BitMap 存储格式"
        B1[原始 BitMap<br/>1 bit/行]
        B2[RLE 压缩<br/>行程编码]
        B3[Roaring BitMap<br/>混合压缩]
    end

    subgraph "压缩效果"
        C1[1M 行: 125KB]
        C2[稀疏数据: 10KB]
        C3[密集+稀疏混合: 最优]
    end

    B1 --> C1
    B2 --> C2
    B3 --> C3
```

**Roaring BitMap 原理**：
```c
// 项目中 bitmap_index.h 的实现（借鉴 GreptimeDB）
typedef struct bitmap {
    uint32_t *data;     // 位数据
    int n_bits;         // 位数
    int n_words;        // word 数量
} bitmap_t;

// Roaring BitMap 优化
// - 低 16 位密集：用数组存储
// - 高 16 位稀疏：用 BitMap 存储
// - 动态选择最优存储格式
```

**BitMap 查询操作**：
```c
// 项目 bitmap_index.h 支持的操作
int bitmap_and(const bitmap_index_t *idx,
               int value1, int value2,
               int *doc_ids, int *count);

int bitmap_or(const bitmap_index_t *idx,
              int value1, int value2,
              int *doc_ids, int *count);

// GreptimeDB 的 Tag 组合查询
// SELECT * WHERE host = 'server1' AND region = 'beijing'
// = BitMap AND 操作
// SELECT * WHERE host = 'server1' OR host = 'server2'
// = BitMap OR 操作
```

### 2.3 倒排索引查询流程

```mermaid
sequenceDiagram
    participant Q as 查询
    participant P as Parser
    participant I as Index
    participant B as BitMap
    participant S as Storage

    Q->>P: WHERE host='server1' AND region='beijing'
    P->>I: 提取 Tag 过滤条件
    I->>B: host='server1' → BitMap1
    I->>B: region='beijing' → BitMap2
    B->>B: BitMap1 AND BitMap2 → Result BitMap
    B->>S: 只读取 BitMap 标记的行
    S-->>Q: 返回过滤后数据
```

---

## 三、Series 索引

### 3.1 Series Key 设计

**Series Key = Primary Key（Tags 组合）**：
```sql
CREATE TABLE metrics (
    ts TIMESTAMP TIME INDEX,
    host STRING TAG,
    region STRING TAG,
    cpu DOUBLE,
    PRIMARY KEY (host, region)  -- Series Key
);
```

**Series 索引特点**：
- 每个 Series Key 组合对应一个时间序列
- Series Key 用 B+ 树或哈希索引
- 时间序列内部按时间排序

```mermaid
graph TB
    subgraph "Series 索引结构"
        S1[Series: host=server1, region=beijing]
        S2[Series: host=server1, region=shanghai]
        S3[Series: host=server2, region=beijing]

        S1 --> T1[时间序列 1<br/>ts1, ts2, ts3, ...]
        S2 --> T2[时间序列 2<br/>ts1, ts2, ts3, ...]
        S3 --> T3[时间序列 3<br/>ts1, ts2, ts3, ...]
    end

    subgraph "索引查询"
        Q[查询 Series Key] --> H[Hash Index]
        H --> S1
        H --> S2
        H --> S3
    end
```

### 3.2 Series ID 映射

**Series ID 生成**：
```
Series Key: (host=server1, region=beijing)
Hash: MD5/SHA256(host + region)
Series ID: uint64 唯一标识

映射表：
┌────────────────────────────────┬────────────┐
│ Series Key                     │ Series ID  │
├────────────────────────────────┼────────────┤
│ (host=server1, region=beijing) │ 12345      │
│ (host=server1, region=shanghai)│ 12346      │
│ (host=server2, region=beijing) │ 12347      │
└────────────────────────────────┴────────────┘
```

**项目中的哈希索引对比**：
```c
// 项目 cceh.h: 可扩展哈希索引
cceh_index_t *cceh_index_create(uint32_t segment_capacity,
                                 uint32_t initial_global_depth);

// 项目 pg_hash.h: PostgreSQL 风格线性哈希
typedef struct pg_hash_index pg_hash_index_t;

// GreptimeDB Series ID 索引类似设计
// - 高并发插入
// - 动态扩容
// - 哈希冲突处理
```

---

## 四、布隆过滤器在时序索引中的应用

### 4.1 布隆过滤器原理

**布隆过滤器作用**：快速判断元素**一定不存在**或**可能存在**。

```mermaid
graph LR
    subgraph "布隆过滤器结构"
        BF[BitArray<br/>m 位]
        H1[Hash1] --> BF
        H2[Hash2] --> BF
        H3[Hash3] --> BF
    end

    subgraph "查询结果"
        Q1[全部 0 → 一定不存在] --> FAST[快速排除]
        Q2[全部 1 → 可能存在] --> CHECK[需要精确检查]
    end

    BF --> Q1
    BF --> Q2
```

### 4.2 时序数据库中的布隆过滤器应用

```mermaid
graph TB
    subgraph "SST 文件布隆过滤器"
        S1[SST File 1]
        S2[SST File 2]
        S3[SST File 3]

        S1 --> BF1[Bloom Filter: Tag 值]
        S2 --> BF2[Bloom Filter: Tag 值]
        S3 --> BF3[Bloom Filter: Tag 值]
    end

    subgraph "查询过滤"
        Q[WHERE host='serverX']
        Q --> BF1
        Q --> BF2
        Q --> BF3

        BF1 -->|不存在| SKIP1[跳过 SST1]
        BF2 -->|可能存在| CHECK1[读取 SST2]
        BF3 -->|不存在| SKIP2[跳过 SST3]
    end
```

**项目中的布隆过滤器实现**：
```c
// 项目 bloom.h: 布隆过滤器 API
typedef struct bloom_filter bloom_filter_t;

typedef struct bloom_config {
    size_t expected_items;       // 预期元素数量
    double false_positive_rate;  // 期望假阳性率（默认 0.01）
} bloom_config_t;

// 创建布隆过滤器
bloom_filter_t *bloom_create(const bloom_config_t *config);

// 添加元素
int bloom_add(bloom_filter_t *filter, const void *key, size_t keylen);

// 查询元素
bool bloom_query(const bloom_filter_t *filter, const void *key, size_t keylen);
```

### 4.3 多级布隆过滤器

```mermaid
graph TB
    subgraph "Region 级布隆过滤器"
        R1[Region 1] --> RB1[Bloom: Tag 集合]
        R2[Region 2] --> RB2[Bloom: Tag 集合]
    end

    subgraph "SST 级布隆过滤器"
        S1[SST File 1] --> SB1[Bloom: 具体 Tag 值]
        S2[SST File 2] --> SB2[Bloom: 具体 Tag 值]
    end

    subgraph "Row Group 级布隆过滤器"
        RG1[Row Group 1] --> RGB1[Bloom: Field 值]
    end

    RB1 --> SB1
    SB1 --> RGB1
```

**过滤效果示例**：
```
查询: WHERE host = 'server999' AND cpu > 80

Level 1: Region Bloom Filter
  - Region 1: 可能存在 → 继续
  - Region 2: 不存在 → 跳过（节省 50% IO）

Level 2: SST Bloom Filter
  - SST 1: 不存在 → 跳过
  - SST 2: 可能存在 → 继续
  - SST 3: 不存在 → 跳过（节省 66% IO）

Level 3: Row Group Zone Map
  - cpu Max = 70 → 跳过（不满足 cpu > 80）
```

---

## 五、索引结构与查询路径 Mermaid 图

### 5.1 完整索引结构

```mermaid
graph TB
    subgraph "GreptimeDB 索引层次"
        META[Meta Server<br/>路由表]

        META --> REG[Region 路由]

        REG --> R1[Region 1<br/>T1 ~ T2]
        REG --> R2[Region 2<br/>T2 ~ T3]

        R1 --> IDX1[索引层]
        R2 --> IDX2[索引层]
    end

    subgraph "索引层详细结构"
        IDX1 --> TI[Time Index<br/>B+ Tree]
        IDX1 --> TAGI[Tag Index<br/>Inverted]
        IDX1 --> BF[Bloom Filter]
        IDX1 --> ZM[Zone Map]

        TI --> SST1[SST Files]
        TAGI --> SST1
        BF --> SST1
        ZM --> SST1
    end

    subgraph "SST 文件内部"
        SST1 --> RG[Row Groups]
        RG --> COL[Column Chunks]
        COL --> STATS[Min/Max Stats]
    end
```

### 5.2 查询执行路径

```mermaid
sequenceDiagram
    participant Client as 客户端
    participant Frontend as Frontend
    participant Meta as Meta Server
    participant Region as Region
    participant Index as Index
    participant SST as SST File

    Client->>Frontend: SELECT * FROM metrics<br/>WHERE ts > T1 AND ts < T2<br/>AND host = 'server1'

    Frontend->>Meta: 查询 Region 路由
    Meta-->>Frontend: 返回 Region 列表

    Frontend->>Region: 发送查询请求

    Region->>Region: 时间范围过滤<br/>（Zone Map）

    Region->>Index: Tag 索引查询<br/>host = 'server1'

    Index->>Index: Bloom Filter 过滤<br/>排除不相关 SST

    Index->>Index: BitMap AND 操作<br/>组合多个 Tag 过滤

    Index-->>Region: 返回候选 Row IDs

    Region->>SST: 只读取候选行

    SST-->>Region: 返回数据

    Region-->>Frontend: 返回结果

    Frontend-->>Client: 返回最终结果
```

### 5.3 索引选择决策

```mermaid
flowchart TD
    START[查询条件] --> CHECK{条件类型?}

    CHECK -->|时间范围| TIME[使用 Time Index]
    CHECK -->|Tag 过滤| TAG[使用 Tag Index]
    CHECK -->|Field 过滤| FIELD[使用 Zone Map]
    CHECK -->|组合条件| COMBINE[多索引组合]

    TIME --> SCAN[时间分区裁剪]
    SCAN --> READ[读取相关 SST]

    TAG --> BLOOM[Bloom Filter 快速排除]
    BLOOM --> BITMAP[BitMap 精确过滤]
    BITMAP --> READ

    FIELD --> ZONE[Zone Map Min/Max]
    ZONE --> READ

    COMBINE --> COST[代价估算]
    COST --> SELECT[选择最优索引]
    SELECT --> READ
```

---

## 六、与项目 index/ 模块对比

### 6.1 索引类型对比

| 索引类型 | GreptimeDB | 项目 index/ | 适用场景 | 实现复杂度 |
|---------|------------|-------------|----------|-----------|
| **时间索引** | Time Index (强制) | 无独立实现 | 时间范围查询 | 中 |
| **倒排索引** | Tag Index + BitMap | bitmap_index.h | Tag 过滤、全文检索 | 高 |
| **B+ 树索引** | Series Key Index | btreeam.h / bptree.h | 等值查询、范围扫描 | 中 |
| **哈希索引** | Series ID 映射 | cceh.h / pg_hash.h | 等值查询 | 低 |
| **布隆过滤器** | SST/Region 级 | bloom.h | 快速排除 | 低 |
| **Zone Map** | SST 统计信息 | 无 | 范围过滤 | 低 |

### 6.2 架构对比

```mermaid
graph TB
    subgraph "GreptimeDB 索引架构"
        G1[Time Index] --> G2[Tag Inverted Index]
        G2 --> G3[Bloom Filter]
        G3 --> G4[SST Storage]

        G1 -.->|分区裁剪| G4
        G2 -.->|BitMap 过滤| G4
        G3 -.->|快速排除| G4
    end

    subgraph "项目 index/ 模块"
        P1[btreeam.h<br/>B+ Tree] --> P4[Storage]
        P2[bitmap_index.h<br/>BitMap] --> P4
        P3[bloom.h<br/>Bloom Filter] --> P4
        P5[cceh.h<br/>Hash Index] --> P4
    end

    G1 -.->|借鉴: 时间分区| P1
    G2 -.->|借鉴: 倒排索引| P2
    G3 -.->|已有| P3
```

### 6.3 代码映射

**项目 index/ 模块结构**：
```
engineering/include/db/index/
├── btree/
│   └── btree.h          # BTree 索引（可对应 Time Index）
├── bplus_tree/
│   └── bptree.h         # B+ Tree 索引（可对应 Series Index）
├── bitmap/
│   └── bitmap_index.h   # BitMap 索引（可对应 Tag Index）
├── hash/
│   ├── bloom.h          # 布隆过滤器
│   ├── cceh.h           # 可扩展哈希
│   └── pg_hash.h        # PostgreSQL 线性哈希
└── vector_index/        # 向量索引（HNSW, IVF 等）
```

**GreptimeDB 索引实现位置**（源码参考）：
```
greptimedb/
├── mito2/
│   └── src/
│       ├── region.rs        # Region 管理
│       └── sst/
│           └── parquet.rs   # SST 文件 + Zone Map
├── query/
│   └── src/
│       └── index/
│           └── inverted_index.rs  # 倒排索引
└── storage/
    └── src/
        └── bloom_filter.rs  # 布隆过滤器
```

### 6.4 可借鉴设计点

**1. 时间索引增强**：
```c
// 当前: ts_engine.c 无独立时间索引
int ts_engine_query(void *rel, int64_t start_time, int64_t end_time, ...);

// 借鉴 GreptimeDB: 增加时间索引层
typedef struct ts_time_index_s {
    int64_t min_time;
    int64_t max_time;
    uint64_t segment_id;    // 指向 Segment
    uint64_t row_count;
} ts_time_index_t;

// 查询时先检查 Time Index
int ts_time_index_filter(ts_time_index_t *idx,
                         int64_t start, int64_t end,
                         uint64_t *segment_ids, uint64_t *count);
```

**2. Tag 倒排索引集成**：
```c
// 利用项目已有的 bitmap_index.h
#include "db/index/bitmap/bitmap_index.h"

// 时序表增加 Tag 索引
typedef struct ts_table_s {
    char *name;
    bitmap_index_t *tag_index;  // Tag 倒排索引
    // ...
} ts_table_t;

// Tag 过滤查询
int ts_query_with_tag(ts_table_t *table,
                       const char *tag_key, const char *tag_value,
                       ts_query_results_t *results);
```

**3. 多级布隆过滤器**：
```c
// 利用项目已有的 bloom.h
#include "db/index/hash/bloom.h"

// Segment 级布隆过滤器
typedef struct ts_segment_s {
    bloom_filter_t *tag_bloom;  // Tag 值布隆过滤器
    // ...
} ts_segment_t;

// 查询时快速排除
if (!bloom_query(segment->tag_bloom, tag_value, strlen(tag_value))) {
    // 该 Segment 不包含该 Tag 值，跳过
    continue;
}
```

---

## 七、性能优化建议

### 7.1 索引设计最佳实践

```mermaid
graph TB
    subgraph "索引设计原则"
        P1[高基数 Tag 用倒排索引]
        P2[低基数 Tag 用 BitMap]
        P3[时间范围用 Time Index]
        P4[组合条件用多索引合并]
    end

    subgraph "避免反模式"
        A1[不要对 Field 建索引]
        A2[不要过度索引]
        A3[避免高基数 Field]
    end
```

**索引选择建议**：

| 场景 | 推荐索引 | 理由 |
|------|---------|------|
| `WHERE ts > T1 AND ts < T2` | Time Index | 分区裁剪 |
| `WHERE host = 'server1'` | Tag Index + BitMap | 快速过滤 |
| `WHERE host = 's1' AND region = 'bj'` | 多 Tag Index + BitMap AND | 组合过滤 |
| `WHERE cpu > 80` | Zone Map（SST 统计） | 范围过滤 |
| `WHERE message LIKE '%error%'` | 全文索引 | 文本检索 |

### 7.2 查询优化技巧

**1. 利用时间分区裁剪**：
```sql
-- 好：明确时间范围，触发分区裁剪
SELECT * FROM metrics
WHERE ts > NOW() - INTERVAL '1 hour'
  AND host = 'server1';

-- 差：无时间范围，全表扫描
SELECT * FROM metrics
WHERE host = 'server1';
```

**2. Tag 过滤优先**：
```sql
-- 好：Tag 过滤减少数据量
SELECT * FROM metrics
WHERE host = 'server1'       -- Tag 过滤先执行
  AND cpu > 80               -- Field 过滤后执行
  AND ts > NOW() - INTERVAL '1 hour';

-- 差：Field 过滤先执行，效率低
SELECT * FROM metrics
WHERE cpu > 80
  AND host = 'server1';
```

**3. 避免高基数 Field 查询**：
```sql
-- 避免：request_id 基数太高（每个请求一个值）
SELECT * FROM logs
WHERE request_id = 'abc123';

-- 优化：增加 Tag 字段预聚合
SELECT * FROM logs
WHERE service = 'api-server'  -- 低基数 Tag
  AND ts > NOW() - INTERVAL '1 hour';
```

---

## 要点总结

1. **三层索引体系**：Time Index（时间范围）、Tag Index（倒排 + BitMap）、Series Index（Series Key 映射）
2. **多级时间分区**：Year/Month → Day → Hour → Segment，实现 99%+ 分区裁剪
3. **布隆过滤器应用**：SST 级快速排除，减少 60-80% 无效 IO
4. **BitMap 压缩**：Roaring BitMap 实现稀疏+密集混合压缩，节省 90% 内存
5. **项目借鉴路径**：时间索引（btreeam.h）→ Tag 索引（bitmap_index.h）→ 布隆过滤器（bloom.h）

## 思考题

1. GreptimeDB 为什么强制要求每个表必须有 TIME INDEX？如果允许没有时间索引，会有什么问题？

2. 对比 GreptimeDB 的 Tag Index 与传统关系数据库的倒排索引（如 PostgreSQL GIN），在时序场景下有什么优势？

3. 假设你的监控系统有 100 个 host，每个 host 每秒上报 100 个指标，持续运行 1 年。请设计时间分区策略和 Tag 索引结构，并估算存储空间和查询性能。

4. 项目中的 `bitmap_index.h` 已经实现了 BitMap 索引，如果要将其集成到 `ts_engine.c` 中，需要修改哪些接口？请给出设计方案。
