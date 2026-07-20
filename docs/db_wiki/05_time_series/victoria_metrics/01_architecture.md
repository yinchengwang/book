# VictoriaMetrics 架构设计

## 学习目标

- 理解 VictoriaMetrics 的 LSM-Tree 存储
- 掌握 VictoriaMetrics 的高压缩机制

## 存储架构

```mermaid
graph TB
    subgraph "写入路径"
        W[remote_write] --> RB[Relay Buffer]
        RB --> WB[Write Buffer<br/>In-Memory]
        WB --> SN[Snapshot]
        SN --> L0[Level 0<br/>Snapshots]
        L0 --> L1[Level 1<br/>Merged]
        L1 --> LN[Level N]
    end

    subgraph "存储文件"
        TSF[tsf files<br/>最终存储]
        IDX[index.db<br/>倒排索引]
    end

    LN --> TSF
```

## 高压缩机制

```mermaid
graph LR
    A[原始数据] -->|10x| B[VictoriaMetrics<br/>压缩后]

    subgraph "压缩技术"
        T1[时序去重]
        T2[增量编码]
        T3[XOR 压缩]
        T4[长周期压缩]
    end

    A --> T1
    T1 --> T2
    T2 --> T3
    T3 --> T4
    T4 --> B
```

## 数据结构

```go
// VictoriaMetrics 存储格式
// 1. ts f files: 时序数据文件
//    - 包含时间戳和值
//    - XOR 压缩

// 2. index.db: 倒排索引
//    - series → metric name
//    - label → series
//    - 用于快速查询过滤

// 写入流程
// 1. 写入 Write Buffer（内存）
// 2. 定期刷盘为 Snapshot
// 3. 后台压缩合并
```

## 集群架构

```mermaid
graph TB
    subgraph "VictoriaMetrics Cluster"
        GW[vminsert<br/>写入网关]
        VMS[vmselect<br/>查询节点]
        VMStorage[vmstorage<br/>存储节点]
    end

    subgraph "存储节点"
        ST1[Storage 1]
        ST2[Storage 2]
        STN[Storage N]
    end

    GW --> ST1
    GW --> ST2
    GW --> STN
    VMS --> ST1
    VMS --> ST2
    VMS --> STN
```

## 要点总结

- LSM-Tree 存储，高写入吞吐
- XOR 压缩实现 10x 数据减少
- 倒排索引加速标签查询
- 集群支持数据分片和复制