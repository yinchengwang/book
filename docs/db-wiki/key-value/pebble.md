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

## 二十六、Clock-Pro 缓存替换算法：三链结构与 ghost entry

Pebble 的 block cache 没有沿用纯 LRU，而是实现了 **Clock-Pro**。动机很直接：LSM 的读放大决定点查要碰多个 block，而一次范围扫描会**顺序触达成百上千个 block**。纯 LRU 下，这批「只读一次」的 block 会把扫描前建立的热 block 全部挤出缓存；Clock-Pro 用一条 ghost list 记住「被淘汰但可能还会被再访问」的 block，用二轮扫描（second chance）保护真正的热点，因此在 scan-heavy 负载下命中率显著优于 LRU。

源码：`internal/cache/clockpro.go`（approx，参考子模块未 init，行号需校对）中的 `shard` / `entry` 状态机与 `metaAdd` / `evict` / `runHandCold` / `runHandHot` / `runHandTest`；条目定义在 `internal/cache/entry.go`（approx）的 `entry` 与 `entryType`（`etHot` / `etCold` / `etTest`）；分片构造在 `internal/cache/cache.go`（approx）的 `New` / `NewWithShards`。注意实现已迁到 `internal/cache/`，而非仓库根的 `cache/`。

### 26.1 三条 page list 与 reference bit 语义

每个 shard 独立持有一条热点链表与两个手针：

| list | entry 标记 | 含义 | 持有 value？ |
|------|-----------|------|------------|
| cold | `etCold` | 新进入缓存的 block 的默认归属（`newEntry` 里初始化为 `ptype: etCold`） | 是 |
| hot | `etHot` | 被引用过一轮、已证明是热点的 block | 是 |
| test / ghost | `etTest` | value 已丢弃、只保留 key 的「被淘汰过」占位 | 否（`setValue(nil)`） |

`entry.referenced atomic.Bool` 是整个算法的核心：每次命中时置位；clock hand 第一次扫到该 entry 时**清位**，第二次扫到仍未被置位才真正降级或淘汰。这等价于「一轮扫描 = 一次宽限期」，把 LRU 的精确时间序退化成近似序，但把维护成本从「每次访问都改 doubly-linked list」降到「每次访问只改一个原子位」。

```text
// 伪代码（与 runHandCold / runHandHot / runHandTest 的语义等价）
runHandCold():
  while targetSize() <= sizeHot + sizeCold and handCold != nil:
      e := handCold
      if e.ptype == etCold:
          if e.referenced:                 // 轮内已被再次访问 → 升级
              e.referenced = false
              e.ptype = etHot
              sizeCold -= e.size;  sizeHot += e.size
          else:                            // 二轮未见 → 降为 ghost
              e.setValue(nil)              // 丢掉 value，只留 key
              e.ptype = etTest
              sizeCold -= e.size;  sizeTest += e.size
              while targetSize() < sizeTest:
                  runHandTest()
      handCold = handCold.next()
      while targetSize() - coldTarget <= sizeHot and handHot != nil:
          runHandHot()

runHandHot():
  if handHot == handTest:  runHandTest()   // ghost 链较短时先扫 ghost
  e := handHot
  if e.ptype == etHot:
      if e.referenced:  e.referenced = false  // 宽限一轮，不降级
      else:
          e.ptype = etCold                    // 热点降温
          sizeHot -= e.size;  sizeCold += e.size
  handHot = handHot.next()

runHandTest():
  if sizeCold > 0 and handTest == handCold:
      runHandCold()
  e := handTest
  if e.ptype == etTest:
      sizeTest   -= e.size
      coldTarget -= e.size        // ★ ghost 被彻底淘汰 → cold 下界下调
      coldTarget  = max(coldTarget, 0)
      metaDel(e)                   // 彻底删除 ghost 条目
  handTest = handTest.next()
```

### 26.2 ghost entry 与自适应 cold 配额

`coldTarget` 是 Pebble 相对原论文最重要的改动。设 `N = targetSize()`，实现维持三条不变量：

```text
sizeHot + sizeCold <= N                 常驻 block 总量不超过缓存容量
sizeHot           <= N - coldTarget    hot 配额上限（等价于 cold 的下界为 coldTarget）
sizeTest          <= N                 ghost 链也以 N 为上界

初始  coldTarget = maxSize = N   ⇒   sizeHot = 0（起步时全部进 cold）
每次淘汰一个 size 为 s 的 ghost：  coldTarget ← max(0, coldTarget - s)
```

也就是说：ghost 条目被彻底丢弃，意味着「它被淘汰之后依然没被再访问」——这个负面信号被用来下调 cold 的下界、给 hot 让出配额。稳态下

```text
sizeHot ≈ N - (N - Σ{已被淘汰的 ghost 的 size}) = Σ{已淘汰 ghost 的 size}（下限截到 0）
```

原论文中 hot 固定为 `γ·N`（γ 典型取 20%）需要人工调参；Pebble 把它换成由 ghost 历史推导出的**自适应**量，且 `Options` 中不对外暴露 γ 参数。代价是多了一条不变量 `sizeTest <= N`，收益是无需针对负载调参。

### 26.3 复杂度与并发特征

| 操作 | 摊还复杂度 | 关键点 |
|------|-----------|--------|
| `shard.get` / `shard.set` | O(1) | 命中只置一个原子位 + 挂链表尾；链表指针仅在 hand 扫到时才改 |
| `shard.evict()` | O(k)，k = 本次逐出的 entry 数 | 每个 entry 在 cold / hot hand 上至多各被扫一次 |
| 分片隔离 | — | `cache.New(size)` 取 `m = 4 * runtime.GOMAXPROCS(0)`；若 `size/m < 4 MiB` 则压缩到 4 片（默认值以 `options.go` 为准） |

高频 Get 只写一个 `atomic.Bool`，把 LRU「每次访问都改 doubly-linked list」造成的 cache line 争用降下来——这是 Clock-Pro 在多核 Go 运行时里比 LRU 更划算的根本原因。每个 shard 各自持有一套三链与三个手针，天然无跨核协调（唯一的锁是 shard 级的 `sync.RWMutex`）。

此外还有按文件整批逐出的路径：`shard.evictFile(handleID, fileNum)` 与 `shard.evictFileRun(fkey)`（approx）。SST 文件被删除时同步清掉缓存里的相关 block，避免缓存条目泄漏到已删文件上。

## 二十七、并发 compaction 调度：槽位公式与 elision-only / trivial move

RocksDB 默认同一层同时只跑一个 compaction；Pebble 允许**并发 compaction**，并用三个压力信号动态决定并发槽位数。

源码：`options.go`（approx）中的 `CompactionConcurrencyRange` / `L0CompactionConcurrency` / `CompactionDebtConcurrency` / `CompactionGarbageFractionForMaxConcurrency`；核心计算在 `compaction_picker.go`（approx）的 `compactionPickerByScore.getCompactionConcurrency()`；compaction 类型枚举在 `compaction.go`（approx）的 `compactionKind`；elision-only 的挑选逻辑在 `compaction_picker.go`（approx）的 `pickElisionOnlyCompaction()`。各默认值以 `options.go` 的 `EnsureDefaults` 为准。

### 27.1 并发槽位公式

```text
lower, upper := opts.CompactionConcurrencyRange()      // 默认 func() (int,int) { return 1, 1 }
if lower >= upper: return upper

// 信号 1：L0 read-amp（sublevel 深度，与 DB 大小无关）
n1 = 0
if opts.L0CompactionConcurrency > 0:                  // 默认 10
    n1 = latestVersionState.l0Organizer.MaxDepthAfterOngoingCompactions() / opts.L0CompactionConcurrency

// 信号 2：compaction debt（防止 base compaction 一结束并发数立刻回落）
n2 = 0
if opts.CompactionDebtConcurrency > 0:                // 默认 1 << 30，即 1 GiB
    n2 = int(estimatedCompactionDebt() / opts.CompactionDebtConcurrency)

// 信号 3：可压缩垃圾占比（DEL + RANGEDEL 是写放大的主因）
n3 = 0
if f := opts.CompactionGarbageFractionForMaxConcurrency(); f > 0 and dbSizeBytes > 0:
    g = (delBytes.PointDels + delBytes.RangeDels) / dbSizeBytes
    g = g / dbSizeBytes
    n3 = int((g / f) * float64(upper - lower))

extra = max(n1, n2, n3, 0)
return min(lower + extra, upper)
```

取 `max` 而非求和：三个信号测的是**同一压力的不同侧面**（读压力 / 落后字节 / 垃圾占比），任一超阈值都说明需要更多并发；三者取 `max` 后再 `min(·, upper)` 封顶，避免信号抖动直接打满磁盘带宽。

信号 2 的必要性在源码注释里写得很直白：base compaction 结束后那一瞬间 debt 归零，但刚被 compact 出来的字节还没下沉到更低层，此时若并发数立刻回落会造成 LSM 塌陷。

### 27.2 省 IO 的 compaction 捷径

`compactionKind` 枚举了多条**不重写数据**的捷径（`compaction.go`，approx）：

| kind | 触发条件 | IO 成本 | 说明 |
|------|---------|--------|------|
| `compactionKindMove`（trivial move） | 输入文件与输出层无 overlap，且 `grandparents.AggregateSizeSum() <= maxOverlapBytes` | **O(1)** | 只改 Version 里的文件引用；输入文件既不重写也不 obsolete |
| `compactionKindElisionOnly` | 仅最底层 `numLevels-1` 存在 obsolete 记录，且 `candidate.SeqNums.High < env.earliestSnapshotSeqNum` | **O(1)** | 只丢过期记录，**不产生输出文件** |
| `compactionKindDeleteOnly` | 宽 range tombstone 完全覆盖某些 sstable | **O(1)** | 直接删文件 |
| `compactionKindCopy` | 跨 shared / local storage，`compactionKindMove` 不可用时 | O(bytes) | 逐字节拷贝并分配新 TableNum，保留原文件 |
| `compactionKindBlobFileRewrite` / `compactionKindVirtualRewrite` | blob 文件超龄 / 虚拟 SST 需物化 | O(bytes) | 较新引入的 compaction 类型 |

```text
// pickElisionOnlyCompaction 的等价伪代码
pickElisionOnlyCompaction(env):
  if opts.private.disableElisionOnlyCompactions: return nil
  candidate = elisionOnlyAnnotator.LevelAnnotation(vers.Levels[numLevels-1])
  if candidate == nil: return nil
  if candidate.SeqNums.High >= env.earliestSnapshotSeqNum: return nil   // ★ 安全性闸门
  return pickedCompactionFromCandidateFile(candidate, env,
                                          numLevels-1, numLevels-1,
                                          compactionKindElisionOnly)
```

那条 `SeqNums.High < earliestSnapshotSeqNum` 判断是 elision-only 的**正确性前提**：只有当待丢弃记录的全部版本都早于最老的活跃 snapshot 时，删掉它们才不会让任何一个读者「本应读到 key 却发现缺失」。这也是它被限制在**最底层**的原因——非底层文件的旧版本可能被下层的更新遮蔽，丢错层就等于丢数据。

trivial move 的判据同样值得注意：除了「与输出层无 overlap」，还要检查与**祖父层（grandparents）**的重叠字节总量。看似可以白搬的文件，若很快就会与祖父层产生大量 overlap，搬过去只是把 merge 债推迟而非消除，因此宁可走正规 compaction。

### 27.3 收益与代价

- trivial move / elision-only / delete-only 把这三类 compaction 的 IO 成本从 `O(bytes)` 压到 `O(1)` 元数据操作——这是 LSM 稳态下「绝大部分 compaction 都很便宜」的原因；
- 并发槽位让 `n` 个 compaction 共享设备带宽：写放大不变，但墙钟时间从 `Σ tᵢ` 降到 `≈ max tᵢ`（实际吞吐受 `RateLimiter` 约束，见 §7.7）；
- 代价是同一层可能有多个 compaction 在跑，需要额外的 overlap / level 冲突检查（`setupInputs` 接收 `inProgressCompactions` 参数）与更复杂的文件生命周期管理。

## 二十八、MVCC 序列号与 Indexed Batch

Pebble 把 MVCC 收敛成一个 56 位的序列号（SeqNum），内联在 key 尾部；Batch 则是「一段连续 SeqNum 区间的原子提交单位」。

源码：`internal/base/internal.go`（approx）的 `SeqNum` / `SeqNumMax` / `SeqNumBatchBit` / `InternalKey` / `MakeTrailer` / `MakeSearchKey`；`batch.go`（approx）的 `Batch.nextSeqNum()` / `Batch.SeqNum()` / `Batch.setSeqNum()` / `batchIter` / `indexedBatch`；`commit.go`（approx）的 `commitPipeline.AllocateSeqNum()` 与 `commitPipeline.publish()`；`snapshot.go`（approx）的 `Snapshot`；批内索引在 `internal/batchskl/`（approx）。

### 28.1 SeqNum 的位布局

```text
SeqNumMax      = 1<<56 - 1              // 有效位宽 56 位
SeqNumBatchBit = 1<<55                  // 标记「batch 内未提交条目」

type InternalKey struct {
    UserKey []byte                      // 用户 key，按用户比较器升序
    Trailer InternalKeyTrailer
}

MakeTrailer(seqNum, kind) = (InternalKeyTrailer(seqNum) << 8) | InternalKeyTrailer(kind)
//                                     ├──── 56 bit seqnum ────┘    └── 8 bit kind ──┘

kind 取值（internal/base/internal.go，approx）：
  InternalKeyKindDelete      = 0      InternalKeyKindSingleDelete = 7
  InternalKeyKindSet         = 1      InternalKeyKindRangeDelete  = 15
  InternalKeyKindMerge       = 2      InternalKeyKindMax         = 30
  InternalKeyKindLogData     = 3
```

排序规则是**用户 key 升序 + 同 key 内 trailer 降序**。由于 seqnum 占高位、kind 占低 8 位，trailer 降序等价于「seqnum 大的在前；seqnum 相同时 kind 大的在前」。于是点查只需在定位到的 user key 分组内取**第一个 seqnum ≤ 可见快照**的记录，天然拿到最新版本，无需额外的版本链指针。

查找时用最大 trailer 作哨兵：`MakeSearchKey(userKey) = MakeInternalKey(userKey, SeqNumMax, InternalKeyKindMax)`，保证搜索键排在同 user key 的所有真实版本之前。

### 28.2 Batch 的 repr 与 SeqNum 区间

Batch 在内存里是一段连续字节缓冲（`batch.go` 顶部注释，approx）：

```text
+-------------+------------+--- ... ---+
| SeqNum (8B) | Count (4B) |  Entries  |
+-------------+------------+--- ... ---+

每条 record：
+-----------+-----------------+-------------------+
| Kind (1B) | Key (varstring) | Value (varstring) |
+-----------+-----------------+-------------------+

varstring = varint32 长度前缀 + N 字节数据
```

`Commit` 时 `commitPipeline` 为整个 Batch 分配**一段连续**的 SeqNum 区间：

```text
区间 = [ b.SeqNum(), b.SeqNum() + b.Count() )       // 连续 Count() 个 seqnum

// commitPipeline.publish 的核心：单调棘轮
for {
    curSeqNum := env.visibleSeqNum.Load()
    newSeqNum := t.SeqNum() + base.SeqNum(t.Count())
    if newSeqNum <= curSeqNum: break                 // 已被其它 goroutine 发布
    if env.visibleSeqNum.CompareAndSwap(curSeqNum, newSeqNum): break
}
```

`commitPipeline.pending` 是无锁的单生产者多消费者队列，入队时持有 `commitPipeline.mutex`，保证**发布顺序 == WAL 写入顺序**；`publish` 用 CAS 棘轮只保证 `visibleSeqNum` 单调递增，多个并发 committer 可以互相「代发」，不必串行等待。批内所有条目共享同一段 SeqNum 且在 `apply` 完成（写入 MemTable）之后才 publish，因此**同一个 Batch 的写入要么整体对读者可见、要么整体不可见**。

未提交条目用 batch 内偏移 + batch bit 编码：

```text
Batch.nextSeqNum() = base.SeqNum(len(b.data)) | base.SeqNumBatchBit
```

`SeqNumBatchBit`（1<<55）的存在是为了让 batch 内未提交的条目排在所有已提交条目**之后**——否则一个更早提交但排序靠后的写，会被这个 batch 的未提交条目遮蔽。`batchIter` 持有自己的 `snapshot`（同样带 batch bit），遍历时用 `if ikey.SeqNum() >= i.snapshot { ikey = i.iter.Next() }` 跳过偏移之后的条目，从而实现 batch 内的 read-your-writes 可见性。

Snapshot 侧：`DB.NewSnapshot()` 读取 `d.mu.versions.visibleSeqNum.Load()` 存入 `Snapshot.seqNum`，并把 Snapshot 挂进 intrusive 双向链表（`snapshotList`）；compaction 通过 `env.earliestSnapshotSeqNum` 得知最老的活跃快照，据此判断旧版本能否安全丢弃（即 §27.2 中 elision-only 的那道闸门）。

### 28.3 Indexed Batch

普通 Batch 只写不读时，`repr` 就是最省内存的表示。一旦需要在提交前读回自己的写入，就得为其建索引：

```text
// Batch.Get（batch.go，approx）
func (b *Batch) Get(key []byte) ([]byte, io.Closer, error) {
    if b.index == nil { return nil, nil, ErrNotIndexed }
    return b.db.getInternal(key, b, nil /* snapshot */)
}

type indexedBatch struct {                    // 走 sync.Pool 复用，避免每次重建
    batch Batch
    index batchskl.Skiplist                   // internal/batchskl，按需构建
}
// rangeDelIndex / rangeKeyIndex 同理：
//   batchskl.NewSkiplist(&b.data, comparer.Compare, comparer.AbbreviatedKey)
```

索引是**懒构建**的：由 `DB.NewIndexedBatch()` 创建的 Batch，只有在调用 `Get` / `NewIter`，或 `Commit` 时需要 flush 的路径上，才会从 `repr` 重建 `batchskl` 跳表；纯写入的 Batch 不会付出索引成本。

| Batch 形态 | 单点读复杂度 | 额外内存 |
|-----------|------------|---------|
| 普通（未索引） | O(N)，线性扫描 repr | 0 |
| Indexed | O(log N)，batchskl 跳表 | O(M)，M = 跳表节点数 |

原子性的边界因此很清晰：Pebble 保证的是**单个 Batch 内的原子性**（共享一段连续 SeqNum），跨 Batch 的原子性需由上层（如 CockroachDB 的事务层）自行实现——见 §10 对「没有跨 key 事务」的说明。

## 二十九、参考资料

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