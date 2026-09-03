# Compaction 策略深度笔记

## 1. 背景：为什么需要 Compaction

LSM-Tree（Log-Structured Merge-Tree）将所有写入视为顺序写入，通过 WAL + MemTable + SSTable 的多层结构实现高写入吞吐。但随着数据不断写入，各层会积累大量 SSTable 文件，需要周期性合并（Compaction）来：

1. **空间回收**：删除标记删除的记录和过期版本
2. **查询优化**：减少需要扫描的文件数量
3. **层级整理**：维持各层的有序性和大小约束

## 2. Leveled Compaction（RocksDB 默认策略）

### 机制

```
L0: [sst1, sst2, sst3]     <- 接受所有新写入
L1: [sst4 (有序)]          <- 固定大小 T
L2: [sst5 (有序)]          <- T * 10
L3: [sst6 (有序)]          <- T * 100
...
```

- **L0 特殊性**：L0 层的 SSTable 可以有 key 范围重叠，因为所有新数据先写入 L0
- **向下合并**：当 Ln 层写满时，将其全部数据与 Ln+1 的对应范围合并
- **大小增长**：每层容量是上一层的 T 倍（RocksDB 默认 T=10）

### 写放大分析

```
写放大 = (每层大小 × 跨越层数) / 实际数据量
       ≈ (T-1)/T × log_T(N)
       ≈ O(log_T(N))
```

对于 100GB 数据、T=10 的典型场景：
- 写放大约 20-30x
- 每次 Compaction 可能读写整个层级

### 空间放大分析

```
空间放大 = 各层容量之和 / 有效数据量
        ≈ 1 + 1/T + 1/T² + ... < 1.12
        ≈ O(1) ≈ 1.1x
```

Leveled 的空间放大接近 1，因为每层刚好容纳其容量上限。

## 3. Tiered Compaction（Fractal Tree / 部分 NoSQL 使用）

### 机制

```
L0: [sst1, sst2, ..., sst10]    <- 可容纳 10 个文件
L1: [sst11, sst12]              <- 可容纳 10 个文件
L2: [sst13]                     <- 可容纳 10 个文件
...
```

- **同层多文件**：每层可以有多个 SSTable，文件之间 key 范围无重叠要求
- **满层合并**：当某层写满时，将该层所有文件与下一层合并
- **放大效应**：合并时读写量 = 当前层全部数据

### 写放大分析

```
写放大 = 总写入量 / 原始数据量
       ≈ (T-1)/T × (1 - 1/T^(k+1)) / (1 - 1/T)
       ≈ O(1) ≈ 2x (T 很大时)
```

### 空间放大分析

```
空间放大 = (T + 1) / T ≈ 1 + 1/T
        ≈ 1.1x 到 2x
```

因为需要容纳多个层的并发数据，空间放大比 Leveled 高。

## 4. 写放大 vs 空间放大权衡

| 特性 | Leveled | Tiered |
|------|---------|--------|
| 写放大 | 较高 (10-30x) | 较低 (1.5-2x) |
| 空间放大 | 较低 (~1.1x) | 较高 (~1.5-2x) |
| 读放大 | 较低 | 较高 |
| Compaction 开销 | 分散 | 突发 |

### 场景选择

**选择 Leveled 当**：
- 读操作远多于写操作
- 需要严格控制空间使用
- 写入 QPS 不极端高

**选择 Tiered 当**：
- 写入吞吐是瓶颈
- SSD 写入寿命敏感
- 可以接受稍高的空间开销

## 5. RocksDB 的 Leveled/Niversal Compaction

RocksDB 在 Leveled 基础上提供了 Universal Compaction（类似 Tiered），以及 Niversal Compaction（两者的混合）：

```cpp
// RocksDB Compaction 风格配置
Options.compaction_style = kCompactionStyleLevel;    // Leveled
Options.compaction_style = kCompactionStyleUniversal; // Tiered
```

### Universal Compaction 的空间放大控制

```cpp
// 通过 target_size 控制空间放大
Options.compaction_options_universal.max_sizeAmplificationPercent = 200;  // 最大 2x 空间放大
```

## 6. 参考资料

- RocksDB Wiki: https://github.com/facebook/rocksdb/wiki/Compaction
- O'Neil et al., "LSM-Tree": 采用 Leveled Compaction 的原始论文
- "The Bufferless and Bufferless LSM-tree" 比较了两种策略的优劣
