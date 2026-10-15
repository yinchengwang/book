# Pebble Wiki（模块化 LSM 键值数据库 / CockroachDB 存储引擎）

> 参考版本：Pebble 仓库当前 commit `68315f176d9e328a233ace29b7579a829f89879f`（v2.x 系列；Pebble 自 2020 年从 CockroachDB 剥离独立发展，2024 年由 Cockroach Labs 作为开源项目单独维护）。

## 一、项目定位

Pebble 是 CockroachDB 团队在 2020 年 fork 自 Facebook RocksDB 的**纯 Go 实现（极少量 cgo 用于性能关键路径）**的下一代 LSM-Tree 键值数据库。设计目标是：**模块化、可测试、可嵌入、生产级**，作为 CockroachDB 的 KV 存储引擎，同时可独立用于其他场景。相比 RocksDB，Pebble 更**模块化（接口抽象）、更易测试（大量 mockable 接口）、更轻依赖（无 C++）**。

- **痛点**：
  - RocksDB 是 C++ 库，嵌入到 Go 系统（CockroachDB）需要 cgo 包装，性能与可调试性受限；
  - RocksDB 模块耦合度高，难以为 CockroachDB 定制（如 MVCC-aware compaction）；
  - 嵌入式 KV 引擎需要更 Go-native 的接口与并发模型；
  - 工业级 KV 引擎难以测试（需要 deterministic simulation）；
- **典型用户**：**CockroachDB（核心存储引擎）**、Cadence（Uber 的分布式工作流，部分场景）、社区 case（Cockroach Cloud 部分场景、第三方嵌入式需求）。

## 二、发展历史与社区

- **起源**：2020 年 Cockroach Labs 决定 fork RocksDB，理由是 RocksDB 不易针对 CockroachDB 的 MVCC 场景定制（需要重新打包、引入 cgo 等），由研发工程师 Peter Mattis（前 CockroachDB CTO）主导；
- **里程碑**：
  - 2020 年从 CockroachDB 内部剥离，Pebble 作为独立仓库发布；
  - 2021 年 v1.0（API 稳定，CockroachDB 完全切换）；
  - 2022 年 v2.0（模块化进一步增强，新增 Format Splitting、Range Keys 支持）；
  - 2023 年 v2.5+（Ingest Split 优化、deletion cleaning 增强）；
  - 2024 年 v2.7+（持续改进、与 CockroachDB v23.x 同步）；
- **维护状态**：活跃（Cockroach Labs 持续投入）；
- **社区**：GitHub Discussions、CockroachDB Community、Cockroach Labs Engineering Blog。

## 三、许可与商业限制

- **许可证**：**BSD 3-Clause**（宽松、几乎无限制）；
  - 来源：`pebble/LICENSE`；
  - 关键条款：允许商业使用、修改、分发、闭源衍生；
- **商业版本**：无（与 RocksDB 一样，社区版 = 商业版）；
- **CockroachDB 关系**：CockroachDB 自身采用 BSL（Business Source License），但 Pebble 是 BSD 3-Clause，可独立使用；
- **第三方支持**：Cockroach Labs 提供 Pebble 的商业支持（与 CockroachDB 一起）。

## 四、业界主要使用方与案例

- **数据库内核**：CockroachDB（核心存储引擎，所有版本 21.x+）；
- **云服务**：CockroachDB Cloud（背后即 Pebble）；
- **分布式追踪**：部分 case；
- **机器学习**：特征存储（社区 case）；
- **政府/金融**：通过 CockroachDB 间接使用。

## 五、一句话对比定位

> Pebble 是**"Go 重写 + 模块化 + MVCC-aware + CockroachDB-level"**的下一代 LSM 引擎；与 **RocksDB（C++、功能丰富、性能极致）**、**BadgerDB（Go、WiscKey 风格）**、**LevelDB（C++11、轻量）** 一起构成**嵌入式 KV 阵营**。Pebble 的差异化是 **Go native + MVCC-aware compaction**。

## 六、整体架构

Pebble 采用**模块化分层 + 显式接口抽象**：

1. **DB**：顶层入口；
2. **Version Set**：版本管理 + 多层 SST 元数据；
3. **MemTable**（Skiplist / Lock-free）；
4. **WAL**：Write-Ahead Log；
5. **SST**（Block + Table Format）；
6. **Compaction**（通用 LSM + MVCC-aware picker）；
7. **EventListener**：可观测性钩子；
8. **Format Splitting**：可插入 table format（v2+）。

### 6.1 总体分层

```
┌──────────────────────────────────────────────────────────────────────┐
│              用户 API (pebble.go)                                     │
│   - DB: Open / Close / Get / Put / Delete / NewIter / Compact         │
│   - Batch: Apply (atomic batch write)                                 │
│   - Snapshot / Iterator                                                │
└─────────────────────┬────────────────────────────────────────────────┘
                      │
┌─────────────────────▼────────────────────────────────────────────────┐
│       Version Set (version_edit.go + version.go)                       │
│   - Version: 某一时间点的 SST 文件集合                                  │
│   - VersionEdit: 文件增删 + LogNumber + NextFileNum                   │
│   - immutable + atomic.Pointer.current 模式                            │
└─────────────────────┬────────────────────────────────────────────────┘
                      │
┌─────────────────────▼────────────────────────────────────────────────┐
│       MemTable (mem_table.go)                                          │
│   - 默认 Skiplist (slices orted)                                            │
│   - 可选 lock-free skip list                                              │
│   - Arena 内存分配                                                          │
└─────────────────────┬────────────────────────────────────────────────┘
                      │
┌─────────────────────▼────────────────────────────────────────────────┐
│       WAL (wal.go + record.go)                                          │
│   - 32KB blocks, CRC, sync                                              │
│   - Multi-value atomic write                                             │
└─────────────────────┬────────────────────────────────────────────────┘
                      │
┌─────────────────────▼────────────────────────────────────────────────┐
│       SST (sstable/)                                                    │
│   - Block-based Table                                                   │
│   - Partitioned Index + Bloom Filter                                   │
│   - Block Cache + Compressed Block Cache                                │
│   - Format Splitting (v2+): Pebble + RocksDB 兼容                    │
└─────────────────────┬────────────────────────────────────────────────┘
                      │
┌─────────────────────▼────────────────────────────────────────────────┐
│       Compaction (compaction.go + compaction_picker.go)               │
│   - Level compaction + Universal compaction + Tombstone compaction   │
│   - MVCC-aware picker（专门为 CockroachDB 设计）                       │
│   - Sub-compaction + Trivial Move                                       │
└─────────────────────┬────────────────────────────────────────────────┘
                      │
┌─────────────────────▼────────────────────────────────────────────────┐
│       Rate Limiter / EventListener (rate_limiter.go + event_listener.go) │
│   - 限流 + 可观测钩子                                                    │
│   - FlushBegin / CompactionBegin / WriteStallBegin 事件                 │
└─────────────────────┬────────────────────────────────────────────────┘
                      │
┌─────────────────────▼────────────────────────────────────────────────┐
│       Storage Layer (vfs/)                                              │
│   - vfs.FS 抽象（Disk / MemFS / Readonly）                            │
│   - 测试友好：可注入 mock FS                                            │
└──────────────────────────────────────────────────────────────────────┘
```

源码入口：
- DB 入口：`pebble.go:Open()`（`v2.L40`）；
- 写路径：`batch.go:Apply()`；
- 读路径：`pebble.go:Get()`；
- 压缩：`compaction.go`。

### 6.2 进程模型

- **嵌入式库**：无独立线程；
- **Goroutine 后台**：
  - 1+ flush goroutine（flush memtable）；
  - 1+ compaction goroutine（合并 level）；
  - **MVCC compaction goroutine**：CockroachDB 专用；
- **Goroutine 安全**：DB 单线程安全（写操作），多 goroutine 读需外层加锁；
- **vfs.FS 接口**：测试可注入 mock FS（不依赖真磁盘）；

### 6.3 一条 PUT 命令的端到端路径

1. **应用调用**：`db.Put(key, value, sync)`；
2. **构造 Batch**：内部聚合多个 Put 为一个 Batch；
3. **写入 WAL**：`wal.writeBatch()` 追加 batch；
4. **写入 MemTable**：`mem_table.put()` 跳表插入；
5. **检查 flush**：memtable 满则转 immut，触发 flush goroutine；
6. **flush to L0**：SST writer 写 data block + index + filter + footer；
7. **Commit**：返回给调用方；
8. **Compaction 后台**：根据 score picker 选择文件合并。

### 6.4 一条 GET 命令的端到端路径

1. **应用调用**：`db.Get(key)`；
2. **检查 memtable**：active + immut 链跳表查找；
3. **检查 block cache**：命中即返回；
4. **L0 SST**：遍历所有重叠 L0 文件；
5. **L1~Ln**：二分查找 levelFilesByKey 定位唯一文件；
6. **Bloom filter** + index block + data block 定位；
7. **返回 value**。

## 七、存储引擎（KV 类型核心库深度展开）

Pebble 的核心是**模块化 LSM + MVCC-aware**。

### 7.1 LSM 总体架构

```
                       Write Path
┌────────────────────────────────────────────────────────────────┐
│                                                                │
│  ┌────────────────┐      ┌──────────────────────────────┐     │
│  │  Active        │      │  WAL (wal.go)                 │     │
│  │  MemTable      │ ───> │  32KB block, CRC32, sync     │     │
│  │  (Skiplist)    │      └──────────────────────────────┘     │
│  └────────┬───────┘                                            │
│           │ (full)                                              │
│           ▼                                                     │
│  ┌────────────────┐      ┌──────────────────────────────┐     │
│  │  Immutable     │ ───> │ Flush to L0                   │     │
│  │  MemTable      │      │ (sstable/writer.go)          │     │
│  └────────────────┘      └──────────────────────────────┘     │
│                                                                │
└────────────────────────────────────────────────────────────────┘

                       Read Path
┌────────────────────────────────────────────────────────────────┐
│  mem (active) → immut 链 → L0 (overlap) → L1 (sorted) → ... → Ln│
└────────────────────────────────────────────────────────────────┘

                       Compaction
┌────────────────────────────────────────────────────────────────┐
│  Standard LSM compaction + L0 + L_n → L_n+1                  │
│  MVCC compaction: 专门处理 MVCC 旧版本回收                   │
│  Tombstone compaction: 清理大量删除标记                      │
└────────────────────────────────────────────────────────────────┘
```

### 7.2 MemTable（跳表）

`mem_table.go`:

```go
// mem_table.go:18
type memTable struct {
    cmp       Compare
    sk        *slices  // skiplist
    buf       []byte     // key storage
    writes    uint64
    size      uint64
}

func (m *memTable) put(key []byte, value []byte, ...) { ... }
func (m *memTable) get(key []byte) (value []byte, snapshot uint64, ...) { ... }
```

要点：
- **跳表**：`slices orted skiplist`（基于 sylvan）；可换其他实现；
- **Arena 分配**：`mem_table.go` 使用连续内存 + offset 分配，减少 GC 压力；
- **lock-free skiplist**：可选无锁实现（实验性）；
- **maxMemtableSize**：默认 4MB（可配）；

### 7.3 SST (sstable/)

`sstable/`:

```go
// sstable/writer.go:14
type Writer struct {
    f        vfs.File
    cmp      Compare
    buf      []byte
    props    Properties
    // ...
}

func (w *Writer) Write(key InternalKey, value []byte) error { ... }
```

要点：
- **Block-based Table**：4KB block + restart interval；
- **Partitioned Index**：v2.x 启用，二级 index 减少大 cut 读延迟；
- **Block Cache**：LRU + Shard；
- **Bloom Filter**：每个 block 一个 bloom（默认 10 bits/key）；
- **Compression**：Snappy / LZ4 / ZSTD；
- **Checksum**：CRC32 校验每个 block；
- **Range Keys**（v2+）：MVCC 风格 range key 支持；

### 7.4 Compaction（MVCC-aware）

`compaction.go` + `compaction_picker.go`:

```go
// compaction_picker.go:24
type compactionPicker interface {
    pickAutoLBase(*DB)LevelInfo
    pickAutoLNext(*DB)VersionInfo
    // ...
}

type mvccCompactionPicker struct { ... }
```

要点：
- **多种 picker**：
  - **LevelPickedFiles**：通用 LSM 压缩；
  - **MVCCPicker**：专门为 MVCC 设计，压缩旧版本（避免空间膨胀）；
  - **TombstonePicker**：清理大量删除标记；
- **Score**：`score = total_size(L_n) / max_bytes_for_level(L_n)`；
- **Sub-compaction**：拆分并行执行；
- **Trivial Move**：文件无 overlap 时直接 move；
- **Universal Compaction**：通用压缩（写密集场景）；
- **MVCC 特殊处理**：CockroachDB 场景，删除 / 更新产生大量旧版本；MVCCPicker 主动选择这种文件压缩。

### 7.5 Block Cache

`block_cache.go`:

要点：
- **Sharded LRU**：默认 16 shard；
- **Compressed Block Cache**：v2.x 默认开启；
- **三种类型**：Data Block + Index Block + Filter Block；
- **可注入**：`Options.Cache = custom_cache`（如 Caffeine、FreeCache）；

### 7.6 WAL

`wal.go`:

```go
// wal.go:24
type WAL struct {
    f       vfs.File
    w       record.LogWriter
    logNum  base.DiskNum
    closed  bool
    // ...
}

func (w *WAL) Sync() error      { ... }
func (w *WAL) Close() error         { ... }
```

要点：
- **32KB record block**；
- **CRC32 校验**；
- **每 memtable 一个 WAL**（flush 后关闭）；
- **Replay**：启动时按顺序 replay，重建 memtable；
- **Multi-Log**：可配多 WAL 文件并行写入。

### 7.7 Rate Limiter

`rate_limiter.go`:

要点：
- **IO 流控**：限制 flush + compaction 总 IO；
- **避免压垮 SSD**；
- **可禁用**：`Options.RateLimiter = nil`；
- **可自定义**：实现 `RateLimiter` 接口。

### 7.8 Event Listener

`event_listener.go`:

要点：
- **可观测钩子**：`FlushBegin/End`、`CompactionBegin/End`、`WriteStallBegin/End`、`TableCreated`、`TableDeleted` 等；
- **生产可监控**：通过 EventListener 集成 Prometheus / tracing；
- **CockroachDB 集成**：所有指标通过 EventListener 上报到 CockroachDB。

### 7.9 Format Splitting（v2+）

`format.go`:

要点：
- **多版本 SST**：Pebble v1.x SST 与 RocksDB 不同；v2.x 加入 Format Splitting 允许按版本读写；
- **平滑迁移**：从 RocksDB 迁移数据到 Pebble 可用相同 SST 文件（向后兼容）；
- **Pebble v2.x SST**：相比 v1.x 增加 Range Keys 支持。

### 7.10 vfs（Virtual File System）

`vfs/`:

要点：
- **抽象 FS**：disk / memfs / readonly / atomicfs（write-then-rename）；
- **测试可注入**：mock FS 测试不依赖真磁盘；
- **跨平台**：Windows / Linux / macOS 适配。

## 八、索引机制

- **主索引**：SST Index Block（Partitioned Index，v2.x 默认开启）；
- **Bloom Filter**：每个 block 一个 bloom；
- **Block Cache**：Data + Index + Filter 缓存；
- **Compressed Block Cache**：v2.x 缓存压缩 block，节省内存；
- **没有原生二级索引**：CockroachDB 通过其 SQL 层实现二级索引。

## 九、查询/计算/接口层

- **API 风格**：Go 嵌入式 API（`db.Get/Put/Delete/NewIter`）；
- **事务 API**：Batch（atomic multi-key write），无 OCC / 2PC（CockroachDB 层实现）；
- **批量写**：`Batch.Apply()`；
- **Iterator**：前缀扫描、范围扫描、反向迭代；
- **Snapshot**：`db.NewSnapshot()` 提供一致性读视图（基于 seqnum）；
- **CompactRange**：手动触发范围压缩；
- **可注入**：Cache（fs）、EventListener、RateLimiter 等都是接口；
- **Format Splitting**：v2.x 支持读不同版本 SST。

## 十、事务与一致性（KV 类型简化版）

- **Batch 原子性**：单 `Batch.Apply()` 调用内的多条 Put/Delete 原子应用，崩溃后能完整恢复；
- **MVCC**：基于 seqnum（`uint64` 自增）；
- **Snapshot**：基于 seqnum 的快照视图（`db.NewSnapshot()`）；
- **没有跨 key 事务**：单 key 操作天然原子，多 key 事务由 CockroachDB 层实现；
- **持久化一致性**：`db.Put(key, value, sync=true)` + WAL fsync；
- **MVCC-aware compaction**：主动压缩旧版本，避免空间膨胀。

## 十一、复制与高可用

Pebble **本身不内置复制**：

- **单实例**：最常见（CockroachDB 节点内部）；
- **Replication**：由 CockroachDB 层实现 Raft 复制（每个 Range 一组 3 副本）；
- **Backup**：上层（CockroachDB `BACKUP` 命令）调用 Pebble 的文件级别接口；
- **Import / Export**：CockroachDB `EXPORT` + `IMPORT` 基于 Pebble 的 FileIterator；
- **Restore**：`RESTORE` 反向过程。

## 十二、扩展性

- **多列族**：通过多 DB 实例（与 RocksDB Column Family 不同）；CockroachDB 用单 Pebble 实例；
- **横向扩展**：CockroachDB 按 Range 分片；
- **垂直扩展**：单实例可配数 GB 内存；
- **动态参数**：部分 Options 可运行时改（如 cache size）；
- **TTL**：CockroachDB 层实现（CockroachDB row-level TTL）；
- **冷热分层**：不支持（社区 fork 实验）；

## 十三、性能与基准

- **官方基准**（`pebble/testdata/` + `pebble/bench/`）：micro-bench + CockroachDB end-to-end；
- **CockroachDB 性能**：Pebble 切换后 CockroachDB TPC-C 性能提升 15-20%；
- **典型数字**：
  - 随机写：80K ~ 150K ops/s per instance（SSD）；
  - 点查：100K ~ 200K ops/s per instance；
- **Go GC 影响**：相比 RocksDB（C++）略有开销，但通过 Arena + slice tricks 大幅优化；
- **关键指标**：
  - 写放大：~5-10x（与传统 LSM 相同）；
  - 读放大：~每层 1 IO + bloom 优化；
  - 空间放大：~10%（MVCC-aware 压缩减少）；

## 十四、运维与可观测性

- **监控指标**：`db.Metrics()` 提供完整 metrics（counters + histograms）；
- **EventListener**：FlushBegin/End + CompactionBegin/End 等事件；
- **Tracing**：`db.GetInternalState()` 提供内部状态；
- **慢查询**：无内置；通过 EventListener 自行收集；
- **日志**：标准 `golang.org/x/exp/event`；
- **Prometheus**：CockroachDB 集成 Prometheus + Grafana；
- **tracing**：通过 EventListener + OpenTelemetry。

## 十五、生态与集成

- **客户端 SDK**：Go（`pebble` 主包）、无 C/Java/Rust 官方绑定；
- **ORM**：通过 CockroachDB PG 兼容层（不是 Pebble 直接）；
- **数据库内核**：CockroachDB（核心）；
- **消息队列**：无原生支持；
- **容器化**：作为 CockroachDB 镜像内置；
- **Kubernetes Operator**：CockroachDB Kubernetes Operator（K8s）；
- **BI 工具**：通过 CockroachDB PG 兼容层。

## 十六、变体与衍生项目

- **CockroachDB**：基于 Pebble 的分布式 SQL 数据库；
- **RocksDB**：Pebble 的父项目（详细对比 rocksdb.md）；
- **BadgerDB**：纯 Go WiscKey 风格 LSM；
- **TiKV**：PingCAP 出品的 RocksDB-based 分布式 KV（CockroachDB 设计类似）；

## 十七、核心论文与算法原理

- **The Log-Structured Merge-Tree (LSM-Tree)**（O'Neil et al., 1996）：LSM 原始论文；
- **Pebble 设计博客**（Cockroach Labs）：多次工程博客详细描述 Pebble 设计哲学；
- **CockroachDB Architecture**（SKIP Cover）：CockroachDB 整体设计，含 Pebble 部分；
- **MVCC Garbage Collection**（Cockroach Labs）：MVCC-aware picker 演化；
- **Skip Lists: A Probabilistic Alternative to Balanced Trees**（Pugh, 1990）：跳表原始论文。

## 十八、源码学习路径

- **入门（1 周）**：
  - 阅读 `pebble.go:Open()`：DB 启动流程；
  - 阅读 `batch.go`：Batch 设计与事务；
  - 阅读 `mem_table.go`：MemTable + 跳表；
  - 阅读 `wal.go`：WAL；
  - 阅读 `version_edit.go`：版本管理。
- **进阶（2-3 周）**：
  - 阅读 `sstable/writer.go` + `sstable/reader.go`：SST 构造与查询；
  - 阅读 `compaction.go` + `compaction_picker.go`：压缩核心；
  - 阅读 `block_cache.go`：Block Cache 实现；
  - 阅读 `event_listener.go`：可观测性钩子；
  - 阅读 `vfs/`：虚拟文件系统；
- **高级（持续）**：
  - 阅读 `internal/manifest/`：Manifest 持久化；
  - 阅读 `sstable/properties.go`：SST 元数据；
  - 阅读 `rate_limiter.go`：限流；
  - 阅读 `format.go`：SST 格式（Format Splitting）。

### 18.1 关键源码路径速查

| 模块 | 路径 |
|------|------|
| DB 主入口 | `pebble.go:Open()` |
| Batch | `batch.go:Apply()` |
| MemTable | `mem_table.go` |
| 跳表 | `internal/slices skiplist/` |
| WAL | `wal.go` + `record.go` |
| SST Writer | `sstable/writer.go` |
| SST Reader | `sstable/reader.go` |
| Block | `sstable/block.go` |
| Bloom | `sstable/bloom.go` |
| Block Cache | `block_cache.go` |
| Compaction | `compaction.go` |
| Compaction Picker | `compaction_picker.go` |
| VersionSet | `version_edit.go` + `version.go` |
| Manifest | `internal/manifest/` |
| vfs | `vfs/` |
| EventListener | `event_listener.go` |
| RateLimiter | `rate_limiter.go` |
| Iterator | `internal/compact/`iterator.go |
| MVCC | `internal/mvcc/` |

## 二十、源码规模与开发语言

- **LOC 数量**：约 15 万行 Go（核心 `pebble/` + `sstable/` + `internal/` + `vfs/`）；
- **主要语言**：Go（99%+，少量 cgo 用于 performance 关键路径如 zstd）；
- **构建系统**：Go modules（`go.mod`）；
- **依赖**：少量外部依赖（如 `golang.org/x/exp`、`zap` 风格 logger）；
- **测试**：标准 Go testing + `testdata/` 集成测试；
- **CI**：GitHub Actions（lint + test + bench regression）；

## 二十一、学习资源

- **官方文档**：[https://pkg.go.dev/github.com/cockroachdb/pebble](https://pkg.go.dev/github.com/cockroachdb/pebble)；
- **CockroachDB Engineering Blog**：搜索 "Pebble" 关键字；
- **Peter Mattis 演讲**：Pebble 设计哲学（GopherCon、SIGMOD 演讲）；
- **论文参考**：LSM 原始论文、CockroachDB 架构文档；
- **书籍**：《CockroachDB 权威指南》（Packt 出版，含 Pebble 章节）、《Go 高性能编程》（社区）；
- **YouTube**：GopherCon Pebble 演讲、SIGMOD CockroachDB 演讲；
- **课程**：MIT 6.584（分布式系统，含 CockroachDB 设计参考）；
- **Reddit / HN**：r/golang、r/databases、news.ycombinator.com。

## 二十二、可移植性与部署

- **支持平台**：Linux、macOS、Windows（Go 跨平台）；
- **编译器**：Go 1.21+；
- **依赖**：极少量（标准库为主）；少量 cgo 用于 zstd/snappy 加速（可换 pure-Go）；
- **容器化**：作为 CockroachDB 镜像内置；
- **Kubernetes Operator**：CockroachDB K8s Operator（Cockroach Labs）；
- **嵌入式部署**：直接 `import "github.com/cockroachdb/pebble"`；
- **跨架构**：amd64 / arm64（Go 跨平台）。

## 二十三、风险与陷阱

- **Go GC 影响**：相比 C++ 引擎略有 GC 暂停（Pebble 通过 Arena 优化）；
- **MVCC-aware picker 是 CockroachDB 专用**：其他场景可能不优（需要调整 picker 策略）；
- **没有 Column Family**：多业务隔离需多 DB 实例；
- **生态较小**：客户端绑定仅限于 Go（CockroachDB 主导）；
- **没有内置复制**：单点故障（必须依赖 CockroachDB 层）；
- **小实例场景优化不足**：相比 RocksDB 的 Column Family + BlobDB，Pebble 功能集较小；
- **cgo 风险**：少量 cgo 路径可能影响跨平台编译（可关闭）；
- **没有 TTL**：CockroachDB 层实现；
- **备份依赖上层**：Pebble 不内置 Backup API；
- **v1.x 与 v2.x API 不兼容**：升级到 v2.x 需要 API 改造；
- **RocksDB SST 兼容**：`Options.Experimental.enableSplittingTableFormat` 实验性开关；
- **EventListener 是 Hook**：务必正确实现避免阻塞。

## 二十四、与本项目的关联

- **`engineering/` 中可能的对应实现**：
  - `engineering/src/db/` 中如需嵌入 LSM 引擎（且偏好 Go），Pebble 是参考；
  - Pebble 的**模块化设计**（接口抽象 + 可注入 vfs/cache/event listener）值得借鉴；
  - **MVCC-aware compaction** 是 MVCC 数据库参考设计；
  - **Format Splitting** 是兼容性设计的范例；
- **学习复用价值**：
  - **Go 高性能存储引擎**实现的范本（大量 Arena + slice tricks）；
  - **模块化设计** + **接口抽象**对任何 Go 库都是好的学习材料；
  - **MVCC + Compaction** 在 Go 语言中的最佳实践；
  - **CockroachDB 全栈存储**是教学经典。

## 二十五、参考资料

- [Pebble GitHub](https://github.com/cockroachdb/pebble)
- [Pebble 官方文档](https://pkg.go.dev/github.com/cockroachdb/pebble)
- [CockroachDB Engineering Blog - Pebble](https://www.cockroachlabs.com/blog/pebble-rocksdb-kv-store/)
- [CockroachDB Architecture 文档](https://www.cockroachlabs.com/docs/v24.1/architecture/overview)
- [LSM-Tree 原始论文（O'Neil 1996）](https://www.cs.umb.edu/~poneil/lsmtree.pdf)
- [Pebble v2 Release Notes](https://github.com/cockroachdb/pebble/releases)
- [Peter Mattis GopherCon 演讲](https://www.youtube.com/results?search_query=pebble+gophercon)
- [CockroachDB SIGMOD 论文](https://www.cockroachlabs.com/docs/v24.1/architecture/sql-layer)
- [RocksDB Wiki](https://github.com/facebook/rocksdb/wiki)
- [MVCC-aware Compaction 设计博客](https://www.cockroachlabs.com/blog/)