# Redpanda 流式存储引擎

## 学习目标

- 理解 Redpanda 的日志结构存储架构
- 掌握 Raft 日志与追加写机制
- 了解分层存储（S3 Tiered Storage）的设计原理
- 对比 Redpanda 存储与项目 storage/ 模块的异同

## 正文

### 1. 日志结构存储概览

Redpanda 采用日志结构存储（Log-Structured Storage）作为核心存储模型。所有消息以追加写方式写入日志文件，支持高吞吐量的顺序写入。

```mermaid
graph TB
    subgraph "日志结构存储架构"
        P["Producer 批量写入"] --> L["Log Segment"]
        L --> S1["Segment 0<br/>[0-999]"]
        L --> S2["Segment 1<br/>[1000-1999]"]
        L --> S3["Segment 2<br/>[2000-...]"]
        
        S1 --> I1["Index File<br/>.index"]
        S2 --> I2["Index File<br/>.index"]
        S3 --> I3["Index File<br/>.index"]
    end
    
    subgraph "磁盘布局"
        D["Data Directory"]
        D --> F1["00000000000000000000.log"]
        D --> F2["00000000000000001000.log"]
        D --> I1[".index 文件"]
    end
```

**核心设计原则**：

| 原则 | 说明 |
|------|------|
| 追加写 | 所有写入操作追加到日志末尾，避免随机写 |
| 顺序 I/O | 利用磁盘顺序读写的高带宽特性 |
| 不可变 | 已写入的日志段文件不可修改 |
| 分段管理 | 日志按大小或时间切分为多个段文件 |

### 2. Raft 日志与追加写

Redpanda 将 Raft 日志与消息日志统一，每个 Partition 对应一个 Raft 复制组：

```mermaid
sequenceDiagram
    participant P as Producer
    participant L as Leader (Partition)
    participant F1 as Follower-1
    participant F2 as Follower-2
    participant D as Disk
    
    P->>L: Produce Request (Batch)
    L->>D: 追加写入本地日志
    L->>F1: AppendEntries RPC
    L->>F2: AppendEntries RPC
    F1->>F1: 追加写入本地日志
    F2->>F2: 追加写入本地日志
    F1-->>L: ACK (High Watermark)
    F2-->>L: ACK (High Watermark)
    L->>L: 更新 commit_index
    L-->>P: Produce Response (Offset)
```

**追加写的关键优势**：

1. **零随机写**：所有写入都在文件末尾，最大化利用磁盘带宽
2. **批量聚合**：多个小消息合并为大批量，减少 I/O 次数
3. **WAL 语义**：日志本身即 WAL，无需额外写前日志
4. **崩溃恢复**：通过日志重放恢复数据

```cpp
// 伪代码：追加写核心逻辑
future<offset_t> partition::append(batch_t batch) {
    // 1. 分配偏移量
    offset_t base_offset = _last_offset + 1;
    
    // 2. 追加到活跃段
    co_await _active_segment->append(batch);
    
    // 3. 更新索引
    _index.append(base_offset, file_position);
    
    // 4. Raft 复制
    co_await _raft->replicate(batch);
    
    // 5. 更新 last_offset
    _last_offset = base_offset + batch.size() - 1;
    
    co_return base_offset;
}
```

### 3. Log Segment 与索引

每个 Partition 由多个 Log Segment 组成，每个 Segment 有独立的索引文件：

```mermaid
graph LR
    subgraph "Segment 内部结构"
        S["Log Segment"]
        S --> R0["Record 0<br/>Offset: 0"]
        S --> R1["Record 1<br/>Offset: 1"]
        S --> R2["Record 2<br/>Offset: 2"]
        S --> RN["..."]
        
        I["Index (.index)"]
        I --> E0["Entry 0<br/>Offset→Position"]
        I --> E1["Entry 1<br/>Offset→Position"]
    end
    
    subgraph "索引查找"
        Q["查询 Offset 150"] --> B["二分查找索引"]
        B --> P["定位到 Segment<br/>文件偏移量"]
        P --> R["扫描找到精确记录"]
    end
```

**索引结构**：

| 索引类型 | 文件后缀 | 用途 |
|----------|----------|------|
| Offset Index | `.index` | 偏移量到文件位置的映射 |
| Time Index | `.timeindex` | 时间戳到偏移量的映射 |
| Compaction Index | `.compaction_index` | 日志压缩支持 |

**索引稀疏性**：

索引采用稀疏索引策略，每隔 N 条记录创建一个索引项（默认 4096 字节）。查找时先通过索引定位大致位置，再顺序扫描找到目标记录。

### 4. 分层存储（S3 Tiered Storage）

Redpanda 支持将历史数据卸载到 S3 兼容的对象存储，实现冷热数据分离：

```mermaid
graph TB
    subgraph "分层存储架构"
        H["热数据层<br/>本地 NVMe SSD"]
        C["冷数据层<br/>S3/MinIO/兼容存储"]
        M["元数据层<br/>Partition Manifest"]
    end
    
    subgraph "数据流向"
        W["写入请求"] --> H
        H -->|"段文件滚动"| U["上传到 S3"]
        U --> C
        
        R["读取请求"] -->|"热数据"| H
        R -->|"冷数据"| C
        C -->|"按需拉取"| H
    end
    
    H --> M
    C --> M
```

**Tiered Storage 配置示例**：

```yaml
# redpanda.yaml
cloud_storage_enabled: true
cloud_storage_region: us-east-1
cloud_storage_bucket: my-bucket
cloud_storage_access_key: ${AWS_ACCESS_KEY_ID}
cloud_storage_secret_key: ${AWS_SECRET_ACCESS_KEY}
```

**分层存储策略**：

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `segment_size` | 段文件大小 | 128MB |
| `retention_time` | 本地保留时间 | 24h |
| `upload_threshold` | 上传阈值 | 段滚动后立即上传 |
| `cache_size` | 本地缓存大小 | 1GB |

**读取路径**：

```mermaid
sequenceDiagram
    participant C as Consumer
    participant R as Redpanda
    participant L as Local Cache
    participant S as S3 Storage
    
    C->>R: Fetch Request (Offset: 50000)
    R->>R: 检查 Manifest
    
    alt 热数据
        R->>L: 读取本地段文件
        L-->>R: 返回数据
    else 冷数据
        R->>L: 检查缓存
        alt 缓存命中
            L-->>R: 返回数据
        else 缓存未命中
            R->>S: GET Object
            S-->>R: 返回段文件
            R->>L: 写入缓存
            L-->>R: 返回数据
        end
    end
    
    R-->>C: Fetch Response
```

### 5. 数据分区与复制

Redpanda 的 Partition 对应一个 Raft 复制组：

```mermaid
graph TB
    subgraph "Topic: orders (3 Partitions, RF=3)"
        P0["Partition 0"]
        P1["Partition 1"]
        P2["Partition 2"]
    end
    
    subgraph "Partition 0 Raft 组"
        N1["Node 1<br/>Leader"]
        N2["Node 2<br/>Follower"]
        N3["Node 3<br/>Follower"]
        
        N1 -->|"AppendEntries"| N2
        N1 -->|"AppendEntries"| N3
    end
    
    subgraph "Partition 1 Raft 组"
        N4["Node 2<br/>Leader"]
        N5["Node 3<br/>Follower"]
        N6["Node 1<br/>Follower"]
        
        N4 -->|"AppendEntries"| N5
        N4 -->|"AppendEntries"| N6
    end
```

**复制策略**：

| 参数 | 说明 |
|------|------|
| `replication_factor` | 副本数（通常 3） |
| `acks` | 生产者确认级别（0/1/-1/all） |
| `min_isr` | 最小同步副本数 |

**ISR（In-Sync Replicas）管理**：

```mermaid
stateDiagram-v2
    [*] --> CatchingUp: 新 Follower 加入
    CatchingUp --> InSync: 追上 Leader 日志
    InSync --> CatchingUp: 落后超过阈值
    InSync --> [*]: 正常运行
    CatchingUp --> [*]: 长期落后被移除
```

### 6. 与项目 storage/ 模块对比

```mermaid
graph TB
    subgraph "Redpanda 存储架构"
        R1["追加写日志"]
        R2["Raft 复制"]
        R3["段文件 + 稀疏索引"]
        R4["S3 分层存储"]
    end
    
    subgraph "项目存储架构"
        P1["Buffer Pool"]
        P2["WAL 写前日志"]
        P3["页面文件"]
        P4["存储后端抽象"]
    end
    
    R1 -.->|"追加写 vs 随机写"| P1
    R2 -.->|"内置 Raft"| P2
    R3 -.->|"段 vs 页面"| P3
    R4 -.->|"冷热分离"| P4
```

**对比分析**：

| 维度 | Redpanda | 项目 storage/ 模块 |
|------|----------|-------------------|
| 存储模型 | 日志结构追加写 | 页面结构随机写 |
| 缓存机制 | OS Page Cache | Buffer Pool (Clock-Sweep) |
| 日志机制 | 日志即数据（统一） | WAL + 数据文件（分离） |
| 复制协议 | 内置 Raft | 需外部集成 |
| 冷数据存储 | S3 Tiered Storage | 暂无（可扩展） |
| 索引类型 | 稀疏索引 + 顺序扫描 | BTree/Hash 精确索引 |

**项目 storage 模块关键组件**：

```c
// storage_backend.h - 存储后端抽象
typedef struct storage_backend_ops {
    page_id_t (*alloc_page)(void *ctx);
    int (*read_page)(void *ctx, page_id_t page_id, page_t *page);
    int (*write_page)(void *ctx, page_id_t page_id, const page_t *page);
    int (*batch_write)(void *ctx, const page_id_t *page_ids,
                       const page_t **pages, int count);
    int (*sync)(void *ctx);
} storage_backend_ops_t;

// 支持多种后端
// - STORAGE_BACKEND_MEMORY: 纯内存
// - STORAGE_BACKEND_PAGE_FILE: 页面文件
// - STORAGE_BACKEND_MMAP: 内存映射文件
// - STORAGE_BACKEND_FAISS: Faiss 格式
```

**可借鉴的设计**：

1. **追加写日志**：项目可引入 append-only log 用于流式数据场景
2. **稀疏索引**：适合大规模顺序扫描场景
3. **分层存储**：将冷数据卸载到对象存储
4. **段文件管理**：简化日志清理和压缩

### 7. 存储性能优化

```mermaid
graph LR
    subgraph "写入优化"
        B1["批量聚合"]
        B2["追加写"]
        B3["零拷贝"]
        B4["DMA 传输"]
    end
    
    subgraph "读取优化"
        R1["预读取"]
        R2["缓存热数据"]
        R3["稀疏索引"]
    end
    
    subgraph "存储优化"
        S1["段压缩"]
        S2["日志清理"]
        S3["Tiered Storage"]
    end
```

**性能优化技术**：

| 技术 | 说明 | 效果 |
|------|------|------|
| 批量聚合 | 合并小消息为大批次 | 减少 I/O 次数 |
| 追加写 | 避免随机写 | 最大化磁盘带宽 |
| 零拷贝 | sendfile 系统调用 | 减少 CPU 和内存拷贝 |
| 稀疏索引 | 减少索引内存占用 | 降低内存压力 |
| 预读取 | 顺序读取时预取 | 提高读取吞吐 |

## 要点总结

1. **日志结构存储**：追加写模式，顺序 I/O，高吞吐
2. **Raft 日志统一**：消息日志即 Raft 日志，无需额外 WAL
3. **分层存储**：热数据本地 SSD，冷数据 S3 对象存储
4. **稀疏索引**：平衡内存占用和查找效率
5. **与项目对比**：追加写 vs 随机写，日志统一 vs WAL 分离

## 思考题

1. 日志结构存储相比传统页面存储有哪些优势？适用场景是什么？
2. Redpanda 如何在没有独立 WAL 的情况下保证数据持久性？
3. 分层存储的冷热数据边界如何确定？读取延迟如何控制？
4. 项目的 storage/ 模块能否引入追加写模式？需要哪些改造？
