# OceanBase 页面布局

## 学习目标

- 掌握 OceanBase 的 Micro Block 和 Macro Block 结构
- 理解 OceanBase 的 LSM-Tree 存储层次
- 对比 OceanBase 与 TiDB、CockroachDB 的页面布局差异

## 存储层次

```mermaid
graph TB
    A[SSTable] --> B[Macro Block<br/>2MB～4MB]
    B --> C[Micro Block<br/>16KB～64KB]
    C --> D[行数据<br/>或列数据]

    E[行存] --> F[Micro Block 包含多行]
    G[列存] --> H[Macro Block 包含单列]
```

## Micro Block 结构

```mermaid
graph TB
    A[Micro Block<br/>16KB～64KB] --> B[Header<br/>元数据]
    A --> C[Data<br/>数据区]
    A --> D[Checksum<br/>校验和]

    B --> E[Block Type: ROW/COLUMN]
    B --> F[Row Count: 100]
    B --> G[Data Size: 32KB]
    B --> H[Compress Type: NONE/LZ4/ZSTD]

    C --> I[行 1]
    C --> J[行 2]
    C --> K[...]
    C --> L[行 N]
```

## Macro Block 结构

```mermaid
graph TB
    A[Macro Block<br/>2MB～4MB] --> B[Header]
    A --> C[Index<br/>索引区]
    A --> D[Micro Blocks<br/>数据区]

    B --> E[Macro Block ID]
    B --> F[Partition ID]
    B --> G[Compress Type]

    C --> H[Micro Block 1 偏移: 0KB]
    C --> I[Micro Block 2 偏移: 64KB]
    C --> J[...]

    D --> K[Micro Block 1]
    D --> L[Micro Block 2]
    D --> M[Micro Block 3]
    D --> N[...]
```

## SSTable 结构

```mermaid
graph TB
    A[SSTable<br/>文件] --> B[Macro Block 1]
    A --> C[Macro Block 2]
    A --> D[Macro Block 3]
    A --> E[...]
    A --> F[Bloom Filter<br/>布隆过滤器]
    A --> F2[Range Index<br/>范围索引]

    F --> G[快速判断 Key 是否存在]
    F2 --> H[定位 Macro Block 范围]
```

## 合并（Compaction）流程

```mermaid
sequenceDiagram
    participant L0 AS Level 0
    participant L1 AS Level 1
    participant L2 AS Level 2
    participant Result AS 合并结果

    Note over L0: MemTable 刷盘
    L0->>Result: 参与合并
    L1->>Result: 参与合并
    L2->>Result: 不参与

    Note over Result: 多路归并
    Result->>Result: 合并后写入新 SSTable
    Result->>Result: 删除旧 SSTable
```

## 布隆过滤器

```c
// 简化版布隆过滤器
typedef struct bloom_filter_s {
    int num_hashes;       // 哈希函数数量
    int bit_array_size;   // 位数组大小
    unsigned char *bits;  // 位数组
} bloom_filter_t;
```

## 与 TiDB 页面布局对比

| 维度 | OceanBase | TiDB |
|------|-----------|------|
| 存储单元 | Micro Block（16KB～64KB） | SSTable Block（4KB～64KB） |
| 大块结构 | Macro Block（2MB～4MB） | SSTable 文件 |
| 布隆过滤器 | 支持 | 支持 |
| 索引结构 | Range Index | Data Block Index |
| 压缩 | LZ4/ZSTD | LZ4/ZSTD |
| 合并策略 | 在线合并（Major Compaction） | 全量合并 |

## 与 CockroachDB 页面布局对比

| 维度 | OceanBase | CockroachDB |
|------|-----------|------------|
| 存储单元 | Micro Block | SSTable Block |
| 大块结构 | Macro Block | SSTable 文件 |
| 布隆过滤器 | 支持 | 支持 |
| 压缩 | LZ4/ZSTD | Snappy/LZ4/ZSTD |

## 与 PostgreSQL 页面布局对比

| 维度 | OceanBase | PostgreSQL |
|------|-----------|------------|
| 页面大小 | Micro Block（16KB～64KB） | Page（8KB） |
| 存储结构 | LSM-Tree | 堆表 |
| 压缩 | 支持（LZ4/ZSTD） | TOAST |
| 布隆过滤器 | 支持 | 不支持 |

## 要点总结

- OceanBase 的存储层次：SSTable → Macro Block → Micro Block
- Micro Block 是最小存储单元（16KB～64KB）
- Macro Block 是大块结构（2MB～4MB）
- 支持布隆过滤器和范围索引加速查询
- 合并策略：在线合并（Major Compaction）
- 与 TiDB/CockroachDB 相比：自研 Micro/Macro Block vs RocksDB SSTable Block

## 思考题

1. OceanBase 的 Macro Block 设计相比 RocksDB 的 SSTable 文件，在大数据块缓存上有何优势？
2. 在线合并（Major Compaction）相比全量合并，在写放大和查询性能上有何差异？
3. 布隆过滤器的假阳性率如何影响 OceanBase 的点查性能？