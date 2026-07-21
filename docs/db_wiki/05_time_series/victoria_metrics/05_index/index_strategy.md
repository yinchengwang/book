# VictoriaMetrics 索引策略

## 学习目标

- 理解时序数据库索引设计的核心挑战与独特需求
- 掌握 VictoriaMetrics 的时间索引、倒排索引和 Series 索引体系
- 了解多级时间分区索引的层次结构与查询路径
- 学习布隆过滤器在时序索引中的应用场景
- 对比项目 index/ 模块（BTree / Hash / Bitmap）与 VictoriaMetrics 索引的异同

## 核心概念

- **Metric Name（指标名称）**：时序数据的名称标识，如 `http_requests_total`，是 VictoriaMetrics 索引的第一级分类键
- **Label（标签）**：键值对形式的元数据，如 `job="prometheus"` 或 `instance="localhost:9090"`，用于过滤和分组
- **Series（时间序列）**：由 Metric Name + Labels 唯一标识的数据流，格式为 `metric_name{label1="value1",label2="value2"}`
- **倒排索引（Inverted Index）**：以 Label Key:Value 为词条，映射到包含该 Label 的 Series ID 列表，用于加速 Label 过滤查询
- **时间分区索引（Time Partition Index）**：按时间范围对数据进行分片，每个分区内维护独立的索引结构，实现查询时的时间范围裁剪
- **布隆过滤器（Bloom Filter）**：概率性数据结构，用于快速判断 Series ID 是否存在于某个数据分区中，减少不必要的磁盘 I/O

## 时序数据库索引设计的核心挑战

### 1. 高基数问题

时序数据的 Label 组合爆炸导致 Series 数量巨大（百万级甚至亿级），传统 B+Tree 索引无法高效管理。

```mermaid
graph LR
    subgraph "Label 组合爆炸"
        T1["job=prometheus<br/>instance=host1:9090<br/>__name__=http_requests"]
        T2["job=prometheus<br/>instance=host2:9090<br/>__name__=http_requests"]
        T3["job=node<br/>instance=host1:9100<br/>__name__=cpu_usage"]
        T4["job=node<br/>instance=host2:9100<br/>__name__=cpu_usage"]
        T5["job=alertmanager<br/>instance=host1:9093<br/>__name__=alerts_total"]
        T6["...更多组合"]
    end

    subgraph "Series 数量"
        C["1000 instances × 100 jobs × 50 metrics<br/>= 5,000,000 Series"]
        C2["10000 instances × 500 jobs × 200 metrics<br/>= 1,000,000,000 Series"]
    end

    T1 --> C
    T6 --> C2
```

### 2. 时间维度优先

时序查询的核心模式是以时间范围为第一过滤条件，索引必须支持高效的时间范围裁剪。

### 3. 写入密集

时序数据以追加写入为主，索引必须支持高吞吐写入，避免写入放大。

### 4. 查询模式多样

| 查询类型 | 示例 | 索引需求 |
|----------|------|----------|
| 时间范围查询 | `time > now() - 1h` | 时间分区裁剪 |
| Label 过滤 | `{job="prometheus"}` | 倒排索引 |
| 正则匹配 | `{job=~"node.*"}` | 正则索引优化 |
| 混合查询 | `{job="prometheus"}[5m]` | 倒排 + 时间裁剪 |
| 聚合查询 | `sum by (job)` | Label 分组 |
| 最新值查询 | `last_over_time(metric[1h])` | 反向时间索引 |

## VictoriaMetrics 索引体系

### 整体架构

```mermaid
graph TB
    subgraph "VictoriaMetrics 索引体系"
        direction TB

        I1["Series Index<br/>Series ID 映射"]
        I2["Inverted Index<br/>倒排索引 (index.db)"]
        I3["Time Index<br/>时间分区索引"]
        I4["Bloom Filter<br/>布隆过滤器"]
    end

    subgraph "存储层"
        S1["TSF Files<br/>时序数据文件"]
        S2["index.db<br/>倒排索引文件"]
        S3["WAL<br/>预写日志"]
    end

    subgraph "查询路径"
        Q["查询请求"] --> P["查询计划器"]
        P --> |"Label 过滤"| I2
        P --> |"时间范围"| I3
        I2 --> |"Series ID 集"| F["Series 交集运算"]
        I3 --> |"时间分区"| F
        F --> |"过滤后的 Series"| S1
        S1 --> A["聚合计算"]
        A --> R["结果"]
    end

    I1 --> S2
    I2 --> S2
    I3 --> S1
    I4 --> S2
```

### 1. Series 索引

Series 索引维护所有活跃 Series 的全局字典，负责 Series 标识符到 Series ID 的双向映射。

**Series 标识符格式**：

```
# 完整格式
metric_name{label1="value1",label2="value2"}

# 示例
http_requests_total{job="prometheus",instance="localhost:9090",handler="/api/v1/query"}
```

**Series 索引结构**：

```mermaid
graph TB
    subgraph "Series Index 结构"
        SI["Series Index<br/>哈希表 + 有序数组"]
        SI --> M1["Series 标识符 → Series ID<br/>Hash Map"]
        SI --> M2["Series ID → Series 标识符<br/>有序数组"]

        M1 --> E1["key: http_requests_total{job='prom',ins='h1'}<br/>id: 42"]
        M1 --> E2["key: http_requests_total{job='prom',ins='h2'}<br/>id: 43"]
        M1 --> E3["key: cpu_usage{job='node',ins='h1'}<br/>id: 44"]

        M2 --> E4["id: 42 → http_requests_total{job='prom',ins='h1'}"]
        M2 --> E5["id: 43 → http_requests_total{job='prom',ins='h2'}"]
        M2 --> E6["id: 44 → cpu_usage{job='node',ins='h1'}"]
    end

    subgraph "写入流程"
        W["数据写入 (remote_write)"] --> P1["解析 Prometheus 格式"]
        P1 --> P2["提取 Series 标识符"]
        P2 --> P3{"Series 存在?"}
        P3 -->|"否"| P4["分配新 Series ID"]
        P3 -->|"是"| P5["获取已有 Series ID"]
        P4 --> P6["更新倒排索引"]
        P5 --> P6
        P6 --> P7["写入内存缓冲区"]
    end
```

**Series 索引的特点**：

- 使用哈希表实现 Series 标识符 → Series ID 的快速查找（O(1)）
- 使用有序数组实现 Series ID → Series 标识符的紧凑存储
- 新 Series 首次写入时自动注册，无需 DDL 操作
- Series ID 使用自增整数，保证紧凑性和顺序性

### 2. 倒排索引（Label 索引）

倒排索引以 Label Key:Value 为词条，映射到包含该 Label 的 Series ID 列表，用于加速 Label 过滤查询。

**倒排索引结构**：

```mermaid
graph TB
    subgraph "Inverted Index 倒排索引"
        Dict["Label 词典<br/>Label → Posting List 映射"]
        Dict --> E1["job=prometheus<br/>Posting List"]
        Dict --> E2["job=node<br/>Posting List"]
        Dict --> E3["instance=host1:9090<br/>Posting List"]
        Dict --> E4["instance=host2:9090<br/>Posting List"]
        Dict --> E5["__name__=http_requests<br/>Posting List"]
        Dict --> E6["__name__=cpu_usage<br/>Posting List"]

        E1 --> PL1["Series ID: [42, 43, 50, 51, ...]<br/>（变长编码数组）"]
        E2 --> PL2["Series ID: [44, 45, 52, 53, ...]"]
        E3 --> PL3["Series ID: [42, 44, 50, 52, ...]"]
        E4 --> PL4["Series ID: [43, 45, 51, 53, ...]"]
        E5 --> PL5["Series ID: [42, 43, 50, 51, ...]"]
        E6 --> PL6["Series ID: [44, 45, 52, 53, ...]"]
    end

    subgraph "查询流程"
        Q["{job='prometheus',instance='host1:9090'}"]
        Q1["查找 job=prometheus 的 Posting List"]
        Q2["查找 instance=host1:9090 的 Posting List"]
        Q3["交集运算"]
        Q --> Q1
        Q --> Q2
        Q1 --> Q3
        Q2 --> Q3
        Q3 --> R["Series ID: [42, 50, ...]"]
    end
```

**Posting List 存储格式**：

| 组件 | 存储结构 | 说明 |
|------|----------|------|
| Label 词典 | 哈希表 | Label Key:Value 到 Posting List 位置的映射 |
| Posting List | 变长整数数组 | 存储 Series ID 列表，按 ID 升序排列 |
| 压缩方式 | Varint + 差值编码 | 大幅减少存储空间 |

**Posting List 压缩**：

```mermaid
graph LR
    subgraph "原始 Posting List"
        A1["原始: [42, 43, 50, 51, 52, 60, 75, ...]"]
    end

    subgraph "差值编码"
        A2["差值: [42, 1, 7, 1, 1, 8, 15, ...]"]
    end

    subgraph "Varint 编码"
        A3["变长字节<br/>小数字占更少空间"]
    end

    A1 --> A2 --> A3
```

**多点查询的交集运算**：

```mermaid
flowchart TD
    Q["Label 条件: {job='prometheus',instance='host1:9090',handler='/api/v1/query'}"]
    Q --> L1["加载 PL(job=prometheus): [42, 43, 50, 51, ...]"]
    Q --> L2["加载 PL(instance=host1): [42, 44, 50, 52, ...]"]
    Q --> L3["加载 PL(handler=/api): [42, 43, 50, 55, ...]"]
    
    L1 --> S1{"选择最短的 PL<br/>作为驱动表"}
    L2 --> S1
    L3 --> S1
    
    S1 -->|"PL(handler) 最短"| S2["驱动: PL(handler)<br/>遍历检查是否在<br/>其他 PL 中"]
    S2 --> S3["遍历 PL(handler):<br/>42 → 在 PL(job) 中? 在 PL(instance) 中?<br/>→ 加入结果集"]
    S3 --> S4["下一个: 43 → 不在 PL(instance) 中? 跳过"]
    S4 --> S5["下一个: 50 → 在 PL(job) 中? 在 PL(instance) 中?<br/>→ 加入结果集"]
    S5 --> R["最终 Series ID 集: [42, 50, ...]"]
```

### 3. 多级时间分区索引

时序数据按时间分区，每个分区内维护独立的索引，实现查询时的时间范围裁剪。

**时间分区层次**：

```mermaid
graph TB
    subgraph "多级时间分区"
        Root["Partition Root<br/>数据目录"]
        Root --> M1["Partition 2024-01<br/>月分区"]
        Root --> M2["Partition 2024-02<br/>月分区"]
        Root --> M3["Partition 2024-03<br/>月分区"]

        M1 --> D1["Partition 2024-01-01<br/>日分区"]
        M1 --> D2["Partition 2024-01-02<br/>日分区"]
        M1 --> D3["Partition 2024-01-03<br/>日分区"]

        D1 --> H1["Partition 2024-01-01-00<br/>小时分区"]
        D1 --> H2["Partition 2024-01-01-01<br/>小时分区"]
        D1 --> H3["Partition 2024-01-01-02<br/>小时分区"]
    end

    subgraph "分区索引"
        H1 --> S1["Series Index<br/>该分区内的 Series"]
        H1 --> S2["Inverted Index<br/>该分区内的 Label → Series"]
        H1 --> S3["Time Block Index<br/>Block 时间范围 → 文件偏移"]
    end

    subgraph "时间范围裁剪"
        Q["查询: time > '2024-01-15'<br/>AND time < '2024-02-10'"]
        Q --> C1["裁剪: 跳过 2024-01-01 到 2024-01-14"]
        Q --> C2["选择: 2024-01-15 到 2024-02-09"]
        C2 --> C3["仅扫描相关分区的索引"]
    end
```

**TSF 文件内部的时间索引**：

```mermaid
graph TB
    subgraph "TSF File 结构"
        H["Header<br/>Magic + Version + 元数据"]
        H --> D["Data Blocks<br/>压缩的数据块"]
        D --> I["Index Block<br/>时间索引区"]
        I --> F["Footer<br/>索引偏移"]
    end

    subgraph "Index Block 内容"
        I1["Series ID: 42<br/>Time Range: 1700000000 ~ 1700000360<br/>Block 数: 6"]
        I1 --> T1["Block 0: time [1700000000, 1700000060]<br/>Offset: 0x1000, Size: 2048"]
        I1 --> T2["Block 1: time [1700000060, 1700000120]<br/>Offset: 0x1800, Size: 1984"]
        I1 --> T3["Block 2: time [1700000120, 1700000180]<br/>Offset: 0x2000, Size: 2048"]

        I2["Series ID: 43<br/>Time Range: 1700000000 ~ 1700000180<br/>Block 数: 3"]
        I2 --> T4["Block 0: time [1700000000, 1700000060]<br/>Offset: 0x2800, Size: 1536"]
        I2 --> T5["Block 1: time [1700000060, 1700000120]<br/>Offset: 0x2E00, Size: 1536"]
    end

    subgraph "查询路径"
        Q2["查询: Series ID 42, time > 1700000200"]
        Q2 --> P1["定位到 TSF File"]
        P1 --> P2["读取 Index Block"]
        P2 --> P3["二分查找 Series ID 42"]
        P3 --> P4["检查时间范围: 1700000200 ∈ [1700000000, 1700000360]"]
        P4 --> P5["定位 Block 3: time [1700000180, 1700000240]"]
        P5 --> P6["读取 Block 偏移，解压数据块"]
    end
```

### 4. 布隆过滤器在时序索引中的应用

布隆过滤器用于快速判断 Series ID 是否存在于某个数据分区或数据文件中，避免不必要的文件读取。

**布隆过滤器在写入和查询中的位置**：

```mermaid
graph TB
    subgraph "写入阶段"
        W1["新 Series 数据写入"]
        W1 --> W2["Series ID 添加到<br/>Bloom Filter"]
        W2 --> W3["设置位图中<br/>对应位为 1"]
        W3 --> W4["写入 TSF File<br/>Bloom Filter 作为元数据存储"]
    end

    subgraph "查询阶段"
        Q1["查询 Series ID X"]
        Q1 --> Q2["Bloom Filter 查询<br/>X 是否可能存在?"]
        Q2 --> Q3{"检查结果"}
        Q3 -->|"不存在"| Q4["跳过该文件<br/>（确定不存在）"]
        Q3 -->|"可能存在"| Q5["读取 Index Block<br/>进一步确认"]
        Q5 --> Q6{"Series ID 存在?"}
        Q6 -->|"是"| Q7["读取数据块"]
        Q6 -->|"否"| Q8["假阳性，跳过<br/>（概率极低）"]
    end

    subgraph "Bloom Filter 参数"
        B1["预期元素数: 100万"]
        B2["假阳性率: 0.1%"]
        B3["位图大小: ~1.8 MB"]
        B4["哈希函数数: 10"]
    end
```

**VictoriaMetrics 中 Bloom Filter 的存储位置**：

| 位置 | 用途 | 粒度 |
|------|------|------|
| TSF File 头部 | 存储该文件包含的所有 Series ID 的 Bloom Filter | 文件级 |
| index.db 头部 | 存储该索引文件包含的所有 Label 的 Bloom Filter | 文件级 |
| 内存缓存 | 热数据的 Bloom Filter 缓存，加速重复查询 | 内存级 |

**布隆过滤器 vs 精确索引**：

| 特性 | Bloom Filter | 精确索引（BTree / Hash） |
|------|-------------|--------------------------|
| 空间占用 | 位图，约 1.8 MB/100万元素 | 树/哈希表，约 10-50 MB/100万元素 |
| 查询速度 | O(k)，常数级 | O(log n) 或 O(1) 平均值 |
| 确定性 | 概率性（有假阳性） | 确定性 |
| 删除支持 | 不支持（标准 Bloom Filter） | 支持 |
| 适用场景 | 快速排除不存在的元素 | 精确查找和范围查询 |

### 5. 查询路径全流程

```mermaid
flowchart TD
    Q["查询请求<br/>SELECT sum(rate(http_requests_total[5m]))<br/>WHERE job='prometheus' AND instance='host1:9090'<br/>AND time > now() - 1h"]
    
    Q --> QP["查询计划器<br/>MetricsQL 解析"]
    QP --> T1{"时间范围筛选"}
    T1 --> T2["计算时间范围: now() - 1h → now()"]
    T2 --> T3["时间分区裁剪: 定位到相关分区"]
    
    T3 --> T4{"Label 过滤"}
    T4 --> T5["查询倒排索引<br/>job=prometheus → PL1<br/>instance=host1:9090 → PL2"]
    T5 --> T6["Posting List 交集<br/>PL1 ∩ PL2 → Series ID 集"]
    
    T6 --> T7{"数据文件定位"}
    T7 --> T8["对每个目标 Series ID<br/>使用 Bloom Filter 筛选<br/>可能包含数据的 TSF 文件"]
    T8 --> T9["读取 TSF 文件的 Index Block<br/>定位数据块"]
    
    T9 --> T10["数据读取"]
    T10 --> T11["读取数据块<br/>XOR 解压<br/>时间戳过滤"]
    T11 --> T12["聚合计算<br/>rate() + sum()"]
    T12 --> R["返回结果"]
```

## 与项目 index/ 模块的对比

本项目 index/ 模块（位于 `engineering/include/db/index/`）包含丰富的索引实现，下表从时序场景角度对比 VictoriaMetrics 的索引策略与项目中的索引类型。

### 核心索引对比

| 维度 | VictoriaMetrics 索引 | 项目 BTree (`db/index/btree`) | 项目 Hash (`db/index/hash`) | 项目 Bitmap (`db/index/bitmap`) |
|------|---------------------|-------------------------------|-----------------------------|-------------------------------|
| 数据结构 | 倒排索引 + 哈希表 + 有序数组 | B-Tree（有序键值对） | 哈希表（CCEH/Cuckoo/PG_Hash） | 位图索引 |
| 用途 | Series ID 映射 + Label 过滤 | 通用有序索引 | 等值查找 | 集合运算 |
| 有序性 | 部分有序（时间分区内） | 全有序 | 无序 | 无序 |
| 时间范围查询 | 直接支持（时间分区裁剪） | 支持（范围扫描） | 不支持 | 不支持 |
| 多标签联合查询 | 倒排索引交集运算 | 复合索引前缀匹配 | 多键查询需多次查找 | 位图 AND/OR 运算 |
| 写入性能 | 高（追加写入 + 批量合并） | 中等（随机插入可能分裂） | 高（O(1) 插入） | 中等（位图更新） |
| 压缩 | Varint + 差值编码 + XOR | 无/页级压缩 | 无 | 位图压缩（WAH/RLB） |
| 高基数支持 | 优秀（Posting List 压缩） | 差（树膨胀） | 中等（哈希冲突） | 差（位图稀疏） |

### 索引类型用途匹配

| 项目场景 | 推荐索引类型 | 对应 VictoriaMetrics 索引 |
|----------|-------------|-------------------------|
| Series 标识符 → Series ID 映射 | **Hash**（CCEH / PG_Hash） | Series Index 哈希表 |
| Series ID → Series 标识符 映射 | **BTree** 或有序数组 | Series Index 有序数组 |
| Label:Value → Series 列表 | **倒排索引**（`db/index/fulltext`） | Inverted Index |
| 时间分块内数据定位 | **BTree**（时间戳有序） | Time Index |
| 快速排除不存在文件 | **Bloom Filter**（`db/index/hash/bloom`） | TSF File Bloom Filter |
| 时间范围裁剪 | **BTree** 范围扫描 | Time Partition Index |
| 多标签集合运算 | **Bitmap**（位图 AND/OR） | Posting List 交集 |

### 与项目 Bloom Filter 的对比

项目中的 Bloom Filter（`db/index/hash/bloom.h`）与 VictoriaMetrics 使用的 Bloom Filter 在本质上是同一数据结构，但应用场景存在差异：

| 维度 | 项目 Bloom Filter | VictoriaMetrics 中的 Bloom Filter |
|------|------------------|---------------------------------|
| 配置参数 | `expected_items` + `false_positive_rate` | 同上，基于数据文件大小自动计算 |
| 存储位置 | 内存使用 | 持久化到 TSF/index.db 文件头部 |
| 粒度 | 用户自定义 | 文件级（每个 TSF 文件一个） |
| 哈希函数 | 内部实现（k 个独立哈希） | 基于 MurmurHash3 的变体 |
| 主要用途 | 通用存在性判断 | 加速 Series ID 查找排除 |

### 对比总结

**VictoriaMetrics 的优势**：
- 倒排索引天然支持多 Label 组合查询，无需预定义复合索引
- 时间分区索引实现自动的时间范围裁剪，对时序查询友好
- Posting List 的差值编码 + Varint 压缩极大降低了高基数场景的索引空间
- XOR 压缩算法针对时序数据的数值特征优化，压缩率远超通用算法

**项目 index/ 模块的优势**：
- BTree 支持更通用的有序键值存储和范围查询
- Hash 索引（CCEH、Cuckoo、PG_Hash）在等值查找场景性能优于倒排索引
- Bitmap 索引在低基数、确定性集合运算场景效率更高
- 支持更多索引类型（RTree、GIN、GiST、TTree、Skip List、向量索引等），覆盖更广泛的应用场景

## 要点总结

- VictoriaMetrics 的索引体系由 **Series Index**（Series 标识符映射）、**倒排索引**（Label 过滤）和**时间分区索引**（时间范围裁剪）三部分组成
- **Series Index** 使用哈希表实现 Series 标识符 → Series ID 的 O(1) 查找，有序数组实现反向映射
- **倒排索引**以 Label Key:Value 为词条构建 Posting List，支持多 Label 联合查询的交集运算
- Posting List 使用**差值编码 + Varint 压缩**，大幅降低高基数场景的存储空间
- **多级时间分区**（月/日/小时）实现自动的时间范围裁剪，缩小查询范围
- **布隆过滤器**用于快速排除不存在的 Series ID，避免不必要的文件读取
- 项目 index/ 模块的 BTree、Hash、Bitmap 等索引类型与 VictoriaMetrics 的索引体系各有侧重，可相互补充
- 时序数据库的索引设计核心挑战是**高基数 + 时间维度优先 + 写入密集**，VictoriaMetrics 的倒排索引 + 时间分区方案为此提供了成熟的参考实现

## 思考题

1. VictoriaMetrics 使用倒排索引而非 B+Tree 作为 Label 索引，为什么在高基数场景下倒排索引优于 B+Tree？请从存储空间和查询效率两个角度分析。

2. 布隆过滤器存在假阳性率，VictoriaMetrics 如何处理假阳性带来的误判？如果 Series ID 数量超过 Bloom Filter 的预期容量，会发生什么？

3. 在多级时间分区中，如何选择分区粒度（月/日/小时）？过粗和过细的分区各有什么优缺点？

4. VictoriaMetrics 的 Posting List 交集运算使用"最短驱动表"策略，这种策略在什么情况下可能不优？能否结合项目中的 Bitmap 索引进行优化？

5. 本项目中的 Bloom Filter 实现（`db/index/hash/bloom.h`）能否直接用于加速 VictoriaMetrics 风格的 TSF 文件查询？需要做哪些适配？

6. VictoriaMetrics 的 XOR 压缩算法如何利用时序数据的数值特征实现高压缩率？与通用的 Snappy/LZ4 压缩相比有何优势？
