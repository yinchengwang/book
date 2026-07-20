# Badger 架构设计

## 学习目标

- 理解 Badger 的 LSM-Tree 架构设计
- 掌握键值分离的 ValueLog 机制
- 了解写入/读取/压缩流程

## 架构总览

```mermaid
graph TB
    subgraph "写入路径"
        W["写入请求"] --> TXN["事务层<br/>Oracle"]
        TXN --> WAL["WAL 日志"]
        WAL --> MT["MemTable<br/>SkipList"]
        MT --> F{"MemTable 满?"}
        F -->|是| IMT["Immutable MemTable"]
        IMT --> L0["L0 层 SSTable"]
        L0 --> COMP["Compaction"]
    end

    subgraph "存储结构"
        LSM["LSM-Tree<br/>存储 Key + Value 指针"]
        VL["ValueLog<br/>存储实际 Value"]
        LSM --> VL
    end

    subgraph "压缩流程"
        COMP --> LC["Leveled Compaction"]
        LC --> L1["Level 1"]
        LC --> L2["Level 2"]
        L1 --> L2
    end
```

## 键值分离设计

Badger 的核心创新：将 Key 和 Value 分离存储。

### 传统 LSM 问题

```mermaid
graph LR
    subgraph "传统 LSM（RocksDB/LevelDB）"
        K1["Key1 + Value1"] --> SST1["SSTable 1"]
        K2["Key2 + Value2"] --> SST1
        K3["Key3 + Value3"] --> SST1
        SST1 --> C["Compaction"]
        C --> R["重写整个 SSTable<br/>大 Value 导致写放大"]
    end
```

### Badger 解决方案

```mermaid
graph LR
    subgraph "Badger 键值分离"
        K1["Key1 + vptr1"] --> SST["SSTable<br/>只存 Key"]
        K2["Key2 + vptr2"] --> SST
        V1["Value1"] --> VL["ValueLog"]
        V2["Value2"] --> VL
        SST -->|vptr| VL
    end
```

**优势**：
- **减少写放大**：Compaction 只处理 Key，不重写 Value
- **SSD 友好**：减少写入量，延长 SSD 寿命
- **适合大 Value**：大 Value 场景优势明显

## 写入路径详解

```mermaid
sequenceDiagram
    participant C as Client
    participant O as Oracle
    participant W as WAL
    participant M as MemTable
    participant V as ValueLog

    C->>O: Begin Transaction
    O->>O: 分配 commit timestamp
    C->>V: Write Value to ValueLog
    V-->>C: 返回 vptr (offset, len)
    C->>W: Write WAL entry (key + vptr)
    C->>M: Insert to MemTable (key + vptr)
    C->>O: Commit
    O->>O: Update watermark
```

### MemTable 结构

```go
// MemTable 使用 SkipList 实现
type MemTable struct {
    sl       *skl.Skiplist  // 跳表
    wal      *wal.WAL       // 关联的 WAL
    buf      *bytes.Buffer  // 缓冲区
    size     int64          // 当前大小
    maxSize  int64          // 最大大小
}
```

### WAL 格式

```
+----------------+----------------+----------------+
| Entry Header   | Key            | Value Ptr      |
+----------------+----------------+----------------+
| checksum(4B)   | key_len(4B)    | val_offset(8B) |
| key_len(4B)    | key_data(NB)   | val_len(4B)    |
| val_len(4B)    |                |                |
+----------------+----------------+----------------+
```

## 读取路径详解

```mermaid
flowchart TB
    R["读取请求"] --> M1{"MemTable?"}
    M1 -->|命中| RT1["返回 vptr"]
    M1 -->|未命中| M2{"Immutable MemTable?"}
    M2 -->|命中| RT2["返回 vptr"]
    M2 -->|未命中| BF{"Bloom Filter<br/>L0-Ln"}
    BF -->|可能存在| SST["SSTable 查找"]
    BF -->|不存在| MISS["返回 Not Found"]
    SST --> IDX["Index Block"]
    IDX --> DATA["Data Block"]
    DATA --> VPTR["获取 vptr"]
    VPTR --> VLOG["ValueLog 读取"]
    VLOG --> VAL["返回 Value"]
```

### SSTable 结构

```
+------------------+
| Data Block 1     |
| Data Block 2     |
| ...              |
| Data Block N     |
+------------------+
| Index Block      |  --> 指向各 Data Block
+------------------+
| Bloom Filter     |  --> 快速排除不存在的 Key
+------------------+
| Footer           |
+------------------+
```

## ValueLog 详解

```mermaid
graph TB
    subgraph "ValueLog 文件结构"
        VF["ValueLog File<br/>1GB per file"]
        VF --> E1["Entry 1<br/>key + value"]
        VF --> E2["Entry 2<br/>key + value"]
        VF --> EN["Entry N"]
        
        E1 --> H["Entry Header<br/>key_len + val_len + meta"]
        E1 --> K["Key Data"]
        E1 --> V["Value Data"]
    end

    subgraph "垃圾回收"
        GC["GC Goroutine"] --> S["扫描 ValueLog"]
        S --> D{"Value 有效?"}
        D -->|否| SKIP["跳过"]
        D -->|是| R["重写到新 ValueLog"]
        R --> U["更新 LSM 中的 vptr"]
    end
```

### ValueLog GC 触发条件

1. **空间阈值**：无效数据比例超过 50%
2. **定时触发**：后台定期扫描
3. **手动触发**：调用 `DB.RunValueLogGC()`

## Compaction 策略

```mermaid
graph TB
    subgraph "Leveled Compaction"
        L0["L0: 多个 SSTable<br/>Key 可能重叠"]
        L1["L1: ~10MB<br/>Key 不重叠"]
        L2["L2: ~100MB<br/>Key 不重叠"]
        
        L0 --> C1["Compaction L0→L1"]
        L1 --> C2["Compaction L1→L2"]
        C1 --> L1
        C2 --> L2
    end
```

### 层级大小配置

| Level | 大小倍数 | 目标大小 |
|-------|---------|---------|
| L0 | - | ~4 个 SSTable |
| L1 | 10x | ~10 MB |
| L2 | 100x | ~100 MB |
| L3 | 1000x | ~1 GB |
| Ln | 10^n | ~10^n MB |

## 要点总结

- **键值分离**：Key 存 LSM，Value 存 ValueLog，减少写放大
- **ValueLog GC**：后台回收无效 Value 空间
- **Leveled Compaction**：层级合并，每层 Key 不重叠
- **并发设计**：Oracle + Watermark 实现事务

## 思考题

1. 键值分离在什么场景下优势最大？什么场景下反而成为劣势？
2. ValueLog GC 如何避免对前台请求的影响？
3. Badger 的 Leveled Compaction 与 RocksDB 的 Universal Compaction 有何区别？
