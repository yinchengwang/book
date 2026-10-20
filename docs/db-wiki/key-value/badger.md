# BadgerDB Wiki（Go 实现 WiscKey 风格 LSM 键值数据库）

> 参考版本：BadgerDB 仓库当前 commit `679bf5b72c9e1788fe7454a28fb4e140221b6668`（v4.x 系列；v3.x 引入 ACID 事务，v4.x 改进 WAL 与备份）。

## 一、项目定位

BadgerDB（简称 Badger）是 Dgraph Labs 出品的**纯 Go 实现、WiscKey 风格分离 KV、嵌入式 LSM-Tree 键值数据库**。它的设计灵感来自 WiscKey 论文（Xia et al., 2017），将 key 与 value 分别存储：key 走 LSM-Tree（紧凑高效），value 走独立的 Value Log（append-only）。这种分离显著降低写放大，特别适合 SSD 上的 value 较大或非顺序 key 工作负载。

- **痛点**：
  - Go 语言生态缺乏成熟、生产可用的嵌入式 LSM 引擎（早期只有 goleveldb 基于 LevelDB）；
  - RocksDB 系引擎（gorocksdb）需要 C++ 依赖，与 Go 的 "compile once, run anywhere" 不符；
  - 传统 LSM 写放大大（10-30 倍），对 SSD 不友好；
  - Go GC 对大量小 KV 不友好，需要紧凑内存布局；
  - 业务（DGraph 图数据库、Jaeger 分布式追踪、Hugo 等）需要纯 Go 嵌入 KV。
- **典型用户**：Dgraph（图数据库核心）、Jaeger（tracing store）、Hugo（静态网站生成器）、CockroachDB 早期版本（部分场景）、若干区块链项目、用户态数据库如 Tigris（部分缓存）。

## 二、发展历史与社区

- **起源**：2016 年 Dgraph Labs（Manish Rai Jain 创立）开发 Badger，主要为支持自家图数据库 Dgraph；
- **里程碑**：
  - 2017 年开源（Apache 2.0）；
  - 2018 年 v1.5（WiscKey 风格分离 KV + Trivial Move）；
  - 2019 年 v2.0（事务、Snapshot 隔离、MVCC）；
  - 2020 年 v3.0（流式 API、批量写优化）；
  - 2021 年 v3.20+（Encryption at rest、Backup 增强）；
  - 2022 年 v4.0（重构 ValueLog 缓存、自动压缩改进）；
  - 2023 年 v4.2+（持续优化、Checksum 强化）；
- **维护状态**：活跃（每季度小版本）；社区维护活跃；
- **社区**：GitHub Discussions、Dgraph Slack、BadgerDB Forum。

## 三、许可与商业限制

- **许可证**：**Apache License 2.0**（最宽松的 OSS 许可）；
  - 来源：`badger/LICENSE`；
  - 关键条款：允许商业使用、修改、分发、闭源衍生，无专利限制，无 copyleft；
- **商业版本**：无；Dgraph 公司提供商业图数据库产品，但 Badger 本身纯开源；
- **生态关系**：Badger 是 Dgraph 的核心存储引擎，也是 Jaeger、Hugo 等的开源项目首选 KV。

## 四、业界主要使用方与案例

- **图数据库**：Dgraph（核心引擎）；
- **可观测性**：Jaeger（tracing store 后端）、OpenTelemetry 部分模块；
- **静态站点**：Hugo（hugo-bin / partial-cache）；
- **区块链**：Go Ethereum（LevelDB → Badger 实验性）；
- **CI/CD**：Drone CI（部分数据存储）；
- **IoT**：边缘计算嵌入式 KV 存储；
- **机器学习**：特征存储、模型缓存（社区 case）；
- **API 网关**：Tyk（部分缓存场景）；
- **国内**：PingCAP（早期调研）、字节跳动（部分内部工具）。

## 五、一句话对比定位

> BadgerDB 是**"纯 Go + WiscKey 风格 + LSM"**的嵌入式 KV 引擎；与 **LevelDB（纯 C++11、轻量 LSM）**、**RocksDB（C++、功能丰富 LSM）**、**Pebble（Go 重写、RocksDB fork）** 一起构成**嵌入式 KV 阵营**；Badger 的差异化是 WiscKey 分离 + 纯 Go。

## 六、整体架构

Badger 采用**WiscKey 风格 LSM**：

1. **Write**：写入 MemTable（WAL）+ LSM Tree（key only）；
2. **Flush**：MemTable → L0 SST（只存 key + value 引用）；
3. **Read**：LSM（key 定位）+ ValueLog（取 value）；
4. **GC**：ValueLog 周期性 garbage collect（清理无效 value）；
5. **Compaction**：与传统 LSM 类似（Level compaction）。

### 6.1 总体分层

```
┌──────────────────────────────────────────────────────────────────────┐
│              用户 API (db.go + txn.go + batch.go)                     │
│   - DB: Open / Close / View / Update / NewIterator                    │
│   - Txn: Get / Set / Delete / Commit                                  │
│   - Batch: Write (atomic)                                          │
│   - Iterator: Seek / Next / Rewind                                    │
└─────────────────────┬────────────────────────────────────────────────┘
                      │
┌─────────────────────▼────────────────────────────────────────────────┐
│       Transaction 层 (txn.go + batch.go)                              │
│   - Txn: optimistic / pessimistic                                    │
│   - Batch: atomic write batch                                         │
│   - MVCC + Conflict Detection                                         │
│   - Snapshot: timestamp-based isolation                                │
└─────────────────────┬────────────────────────────────────────────────┘
                      │
┌─────────────────────▼────────────────────────────────────────────────┐
│       MemTable (memtable.go)                                          │
│   - 默认 Skiplist (skiplist.go)                                       │
│   - 可选 Hash 索引                                                    │
│   - 全 key + seq + value-pointer（不存 value）                       │
└─────────────────────┬────────────────────────────────────────────────┘
                      │
┌─────────────────────▼────────────────────────────────────────────────┐
│       WAL (wal.go)                                                    │
│   - 每个 MemTable 一个 WAL（轮转内存表生成新 WAL）                    │
│   - 32KB record block + CRC32                                         │
│   - Replay: 启动时 replay 重构 MemTable                              │
└─────────────────────┬────────────────────────────────────────────────┘
                      │
┌─────────────────────▼────────────────────────────────────────────────┐
│       SST (table/)                                                    │
│   - Block-based Table + Bloom Filter                                 │
│   - SST 存 key + value-pointer (vptr: file_id + offset)              │
│   - Key 紧凑压缩（减小存储）                                          │
└─────────────────────┬────────────────────────────────────────────────┘
                      │
┌─────────────────────▼────────────────────────────────────────────────┐
│       ValueLog (value.go) - WiscKey 关键                              │
│   - 独立 append-only 文件存 value（不是 LSM 的一部分）                │
│   - value-pointer: (file_id, offset, len)                             │
│   - GC: 定期扫描 SST 检查引用，删除孤立 value                         │
└─────────────────────┬────────────────────────────────────────────────┘
                      │
┌─────────────────────▼────────────────────────────────────────────────┐
│       Levels & Compaction (levels.go + compaction.go)                 │
│   - Level compaction (Score-based picker)                             │
│   - Trivial Move                                                      │
│   - 多层 SST 管理（Badger 默认 7 层）                                │
└─────────────────────┬────────────────────────────────────────────────┘
                      │
┌─────────────────────▼────────────────────────────────────────────────┐
│       Manifest (manifest.go)                                          │
│   - 版本管理：每次 compaction 写 manifest                              │
│   - Crash recovery：replay manifest 重建 levels                      │
└──────────────────────────────────────────────────────────────────────┘
```

源码入口：
- DB 入口：`db.go:Open()`
- 事务：`txn.go`
- MemTable：`memtable.go`
- WAL：`wal.go`
- SST：`table/builder.go` + `table/iterator.go`
- ValueLog：`value.go`
- Compaction：`levels.go` + `compaction.go`

### 6.2 进程模型

- **嵌入式库**：无独立服务线程；
- **Goroutine 后台**：
  - 1 个 flush goroutine（memtable flush）；
  - 1 个 compaction goroutine（合并层）；
  - 1 个 value log GC goroutine（清理 value log）；
  - 1 个 manifest writer goroutine（持久化 manifest）；
- **Goroutine 安全**：DB 实例对单线程安全；多个 goroutine 调用需外层加锁或使用 Txn；
- **GC 影响**：Badger 4.x 大幅优化 GC（值指针在 SST，避免长引用）。

### 6.3 一条 PUT 命令的端到端路径

1. **应用调用**：`db.Update(func(txn *Txn) error { return txn.Set(key, value) })`；
2. **构造 Txn**：自动构造 + 选并发模型；
3. **写入 MemTable**：插入跳表（key + seq + value-pointer 引用）；
4. **写入 WAL**：append-only record（含 key + value-pointer）；
5. **写入 ValueLog**：append value 到 value log 文件（batch 化）；
6. **MemTable 满** → flush 到 L0 SST；
7. **提交事务** → commit（version 持久化）；
8. **可能触发 compaction / GC**（后台 goroutine）。

### 6.4 一条 GET 命令的端到端路径

1. **应用调用**：`db.View(func(txn *Txn) error { return txn.Get(key) })`；
2. **MemTable 查**（active + immut）：跳表查找（key + seq）；
3. **找到 value-pointer**：可能存于 memtable 或 SST；
4. **从 ValueLog 读 value**：根据 (file_id, offset) 读 value log；
6. **反序列化** + 返回。

## 七、存储引擎（KV 类型核心库深度展开）

Badger 的核心是 **WiscKey 风格 LSM-Tree + 独立 ValueLog**。

### 7.1 WiscKey 总体架构

```
                       Write Path
┌─────────────────────────────────────────────────────────────────┐
│                                                                 │
│  ┌────────────────┐      ┌──────────────────────────────┐       │
│  │  Active        │      │  WAL (wal.go)                │       │
│  │  MemTable      │ ───> │  key + value-pointer         │       │
│  │  (skiplist)    │      └──────────────────────────────┘       │
│  └────────┬───────┘                                              │
│           │ (full)                                               │
│           ▼                                                      │
│  ┌────────────────┐      ┌──────────────────────────────┐       │
│  │  Immutable     │ ───> │ Flush to L0 SST               │       │
│  │  MemTable      │      │ (table/builder.go)            │       │
│  └────────────────┘      │ key + value-pointer only      │       │
│                          └──────────────────────────────┘       │
│                                                                 │
│  ┌────────────────┐                                         │
│  │  ValueLog       │ <─── write append value here         │
│  │  (value.go)     │      (separate file per "file id")  │
│  └────────────────┘                                         │
└─────────────────────────────────────────────────────────────────┘

                       Read Path
┌─────────────────────────────────────────────────────────────────┐
│  Check mem → immut → L0 → L1 → ... → Ln (key lookup)           │
│  Then fetch value from ValueLog via value-pointer              │
└─────────────────────────────────────────────────────────────────┘

                       Compaction
┌─────────────────────────────────────────────────────────────────┐
│  Level compaction (传统 LSM):                                    │
│  - Score > 1 → 选 L_n 一个文件 + L_n+1 重叠文件                  │
│  - 多路归并 → 新 L_n+1 文件                                      │
│  - Value-pointer 保留，不动 value log                            │
│                                                                 │
│  ValueLog GC:                                                    │
│  - 扫描所有 SST, 收集有效 value-pointer 集合                     │
│  - 重新写入 valid value 到新 ValueLog                            │
│  - 删除旧 ValueLog                                               │
└─────────────────────────────────────────────────────────────────┘
```

### 7.2 MemTable（基于跳表）

`memtable.go`:

```go
// memtable.go:14
type memTable struct {
    skiplist *skl.Skiplist
    wal      *wal
    maxVer   uint64
    incr     uint64
}

func (mt *memTable) put(key []byte, value y.ValueStruct) error { ... }
func (mt *memTable) get(key []byte) (y.ValueStruct, bool) { ... }
```

要点：
- **跳表**：`skiplist.go` 实现纯 Go 跳表；
- **key 编码**：`[user_key + seq + type]`（Type 为 1B）；
- **value-pointer**：value 实际存在 ValueLog，SST/memtable 只存 `ValueStruct{valPtr: y.ValuePtr{file_id, offset, len}}`；
- **大小限制**：`Options.MemTableSize` 默认 64MB（v4.x 调整）；

### 7.3 ValueLog）

`value.go`:

```go
// value.go:75
type ValueLog struct {
    dirPath          string
    filesMap         map[uint32]*logFile
    maxFid           uint32
    writableOffset   uint32
    numEntriesWritten uint32
    // ...
}

func (vlog *ValueLog) write(reqs []*request) (func(), error) { ... }
func (vlog *ValueLog) getReadValuePtr(valuePtr y.ValuePtr) ([]byte, func(), error) { ... }
```

要点：
- **WiscKey 关键**：value 不进 LSM，独立 append-only 文件；
- **Value-pointer**：`(file_id, offset, len)` 三元组，存于 SST/memtable；
- **Batch write**：vlog.write(reqs) 一次写多个 value（减少 syscall）；
- **mmap**：读 value 通过 mmap 直接访问文件；
- **Header**：每个 value record 含 `crc + len + value`，便于定位；
- **文件切分**：`Options.ValueLogFileSize` 默认 1GB；
- **GC**：周期性 vlog.gc() 整理孤立 value。

### 7.4 SST (Block-based Table)

`table/builder.go` + `table/iterator.go`:

要点：
- **Block-based Table**：4KB block + restart interval + 二分查找；
- **每个 block 单独 bloom filter**：减少不必要读；
- **key + value-pointer 存于 SST**：value 在 value log（值较小但数量大时尤为高效）；
- **压缩**：Snappy 默认（可关）；
- **Checksum**：CRC32 校验每个 block；
- **文件命名**：`00001.sst`、`00002.sst`...（单调递增 fid）；

### 7.5 WAL（Write-Ahead Log）

`wal.go`:

```go
// wal.go:24
type WAL struct {
    f       *os.File
    sz      int
    bo      int
    writeAt uint32
    buf     []byte
}

func (w *WAL) write(e *logEntry) error { ... }
```

要点：
- **每个 memtable 一个 WAL**：memtable flush 后旧 WAL 自动关闭；
- **32KB record**：跨多个 KV 拼接；
- **CRC32 校验**；
- **Replay**：启动时按顺序 replay 所有 WAL，重建 memtable 状态。

### 7.6 Level Compaction

`levels.go` + `compaction.go`:

要点：
- **Score**：`score = total_size(L_n) / max_bytes_for_level(L_n)`；
- **文件选择**：与 RocksDB 类似（最高分 + L_n+1 重叠文件）；
- **Trivial Move**：L_n 文件与 L_n+1 无重叠时直接 move；
- **多线程**：Badger v2+ compaction 后台 goroutine 异步；
- **Sub-compaction**：拆分大 compaction 任务（v4.x 增强）。

### 7.7 ValueLog GC

`value.go:vlog.gc()`:

要点：
- **触发条件**：`vlog.length - valid_data > threshold`；
- **流程**：
  1. 扫描 L0~L_max，收集所有有效 value-pointer 集合；
  2. 读取有效 value 到内存；
  3. 写新 ValueLog；
  4. 原子切换 vptr 引用；
  5. 删除旧 ValueLog。
- **收益**：减少磁盘占用，降低读放大。

### 7.8 Manifest（版本管理）

`manifest.go`:

要点：
- **每次 compaction 写 manifest**：记录新 SST + 删除旧 SST；
- **Replay**：启动时按顺序 replay manifest，重建 levels；
- **Magic**：manifest 文件头 magic number 校验完整性。

## 八、索引机制

- **主索引**：SST 的 Index Block（指向 Data Block）；
- **Bloom Filter**：每个 block 一个 bloom；
- **跳表**：memtable 内查询；
- **Key 比较器**：默认字典序（用户可自定义）；
- **没有二级索引**：Badger 是纯 KV；
- **Transaction + Conflict**：基于 seqnum 的 MVCC。

## 九、查询/计算/接口层

- **API 风格**：纯 Go 嵌入式 API（`db.Open` / `db.Update` / `db.View`）；
- **事务 API**：`Update` / `View` + lambda 风格；
- **批量写**：`Batch`（atomic batch write）；
- **Iterator**：前缀扫描、范围扫描、反向迭代（v3+）；
- **流式 API**：`db.NewStream()` 流式导出/导入；
- **CRDT / Watch**：无（社区包 `badgerhold` 提供索引抽象）；
- **Language binding**：纯 Go，无 CGO；
- **backup/restore**：`Backup()` + `Load()` API（v3+）。

## 十、事务与一致性（KV 类型简化版）

Badger v2+ 提供**完整 ACID 事务**：

- **ACID 跨 key 事务**：`db.Update(func(txn *Txn) error { ... })`；
- **乐观并发控制**（OCC）：默认；
- **冲突检测**：基于 seqnum + key 集合；
- **Snapshot Isolation**：默认隔离级别；
- **冲突重试**：用户通过 retry loop 处理；
- **MVCC**：基于 `version + read_ts`；
- **持久化一致性**：WAL fsync（`Options.SyncWrites`）；
- **事务冲突**：`TxnConflict` 错误，调用方重试。

## 十一、复制与高可用

Badger **本身不内置复制**，由上层应用实现：

- **单实例**：最常见；
- **Replication**：上层（Dgraph 等）实现 Raft / 集群复制；
- **Backup**：v3+ 提供 `DB.Backup()` API；
- **Restore**：`DB.Load()` 从备份恢复；
- **Stream**：通过 `DB.NewStream()` 流式导入导出。

## 十二、扩展性

- **多列族**：不支持（与 LevelDB 同），但可运行多个 DB 实例；
- **横向扩展**：上层（Dgraph）按 shard 分片；
- **垂直扩展**：单实例可配数 GB 内存；
- **动态参数**：少量配置项可运行时调整（如通过 Options）；
- **加密**：v4.x 支持 Encryption at rest（AES）；
- **冷热分层**：不支持（社区 fork 实验）；

## 十三、性能与基准

- **官方基准**（`bench/`）：micro-bench（fillrandom、readrandom）；
- **典型数字**：
  - 随机写：50K ~ 100K ops/s per instance（SSD）；
  - 点查：80K ~ 150K ops/s per instance；
  - WiscKey 优势：value 较大时（> 1KB）写放大大低于 RocksDB；
- **写放大**：~5-10x（WiscKey 风格，比 RocksDB 低）；
- **读放大**：~每层 1 IO + 1 ValueLog IO；
- **GC 影响**：GC 期间性能抖动（CPU + IO）；
- **Go GC 影响**：4.x 优化后 GC 暂停 < 1ms（high throughput）；

## 十四、运维与可观测性

- **监控指标**：`DB.Levels()` 报告 levels 状态；
- **慢查询**：通过 `expvar` 或自定义 hook；
- **日志**：标准 `log` 包 + `moul/log` 风格；
- **Backup**：`DB.Backup()` API；
- **tracing**：无内置支持；
- **Prometheus**：第三方 Exporter（如 `badger-exporter`）。

## 十五、生态与集成

- **客户端 SDK**：Go（`badger` 主包）、C（无）、Python（无 native binding）；
- **ORM**：BadgerHold（基于 Badger 的对象索引库）、`badtoml`（TOML 风格存储）；
- **嵌入式数据库**：Dgraph（图）、Hugo（partial cache）、Jaeger（tracing store）；
- **消息队列**：无原生支持（社区 case）；
- **容器化**：作为 Go 库嵌入到上层 Docker 镜像；
- **Kubernetes Operator**：无（嵌入式）；

## 十六、变体与衍生项目

- **Dgraph**：基于 Badger 的分布式图数据库；
- **BadgerHold**：基于 Badger 的对象索引 + 查询抽象库；
- **RocksDB-Go**：基于 gorocksdb（CGO 包装）；
- **Pebble**：CockroachDB fork RocksDB 后独立 Go 重写；
- **Speedb**：C++ fork RocksDB，专门优化 SSD；
- **Trinity**：字节跳动 fork RocksDB + LSM；
- **Collection Store**：基于 Badger 的 Collection API（社区）。

## 十七、核心论文与算法原理

- **WiscKey: Separating Keys from Values in SSD-conscious Storage**（Wuhan University + Facebook, FAST 2017）：Badger 设计灵感来源；
- **The Log-Structured Merge-Tree (LSM-Tree)**（O'Neil et al., 1996）：LSM 原始论文；
- **Skip Lists: A Probabilistic Alternative to Balanced Trees**（Pugh, 1990）：跳表原始论文；
- **Optimistic Concurrency Control**（Kung & Robinson, 1981）：Badger 事务模型参考；
- **BadgerDB 设计与实现**（Manish Rai Jain 博客）；
- **BadgerDB 论文**（Dgraph Labs 工程博客）。

## 十八、源码学习路径

- **入门（1 周）**：
  - 阅读 `db.go:Open()`：DB 启动流程；
  - 阅读 `txn.go`：事务实现；
  - 阅读 `memtable.go`：MemTable + 跳表；
  - 阅读 `value.go`：ValueLog 设计与 GC；
  - 阅读 `wal.go`：WAL；
- **进阶（2-3 周）**：
  - 阅读 `levels.go` + `compaction.go`：压缩核心；
  - 阅读 `table/builder.go` + `table/iterator.go`：SST 构造与查询；
  - 阅读 `iterator.go`：Iterator 合并 + 跳表 + SST；
  - 阅读 `manifest.go`：版本管理；
  - 阅读 `batch.go`：批量事务；
- **高级（持续）**：
  - 阅读 `value.go:vlog.gc()`：ValueLog GC；
  - 阅读 `db.go:Close()`：关闭流程；
  - 阅读 `stream/`：流式导入导出；
  - 阅读 `backup.go`（v3+）：备份与恢复。

### 18.1 关键源码路径速查

| 模块 | 路径 |
|------|------|
| DB 入口 | `db.go:Open()` |
| 事务 | `txn.go` |
| 批量事务 | `batch.go` |
| MemTable | `memtable.go` |
| 跳表 | `skiplist/skiplist.go` |
| ValueLog | `value.go` |
| WAL | `wal.go` |
| SST Builder | `table/builder.go` |
| SST Iterator | `table/iterator.go` |
| Block | `table/block.go` |
| Bloom Filter | `bloom/bloom.go` |
| Levels | `levels.go` |
| Compaction | `compaction.go` |
| Iterator | `iterator.go` |
| Manifest | `manifest.go` |
| Stream | `stream/` |
| Backup | `backup.go` (v3+) |
| Encryption | `encryption/` (v4+) |

## 二十、源码规模与开发语言

- **LOC 数量**：约 12 万行 Go（核心 `db/` + `table/` + `memtable/` + `value/` + `wal/` + `levels/` 等）；
- **主要语言**：Go（100%，无 CGO）；
- **构建系统**：Go modules（`go.mod`）；
- **测试**：标准 Go testing + 基准 `bench/`；
- **CI**：GitHub Actions（lint + test）；
- **零依赖**：仅 Go 标准库 + 测试用 yaml/bolt 等。

## 二十一、学习资源

- **官方文档**：[https://pkg.go.dev/github.com/dgraph-io/badger](https://pkg.go.dev/github.com/dgraph-io/badger)；
- **官方博客**：[https://blog.dgraph.io/post/badger/](https://blog.dgraph.io/post/badger/)；
- **Manish Rai Jain 博客**：[https://manishrjain.com/](https://manishrjain.com/)；
- **论文参考**：WiscKey 论文、LSM 原始论文、跳表论文；
- **书籍**：《Go 嵌入式存储系统设计》（社区）、《Mastering Go》（Mihalis Tsoukalos 含 Go KV 章节）；
- **YouTube**：Dgraph 会议演讲、BadgerDB 主题分享；
- **课程**：MIT 6.824（Go 项目参考）、CMU Database Group；
- **Reddit / HN**：r/golang、r/databases、news.ycombinator.com。

## 二十二、可移植性与部署

- **支持平台**：Linux、macOS、Windows（纯 Go 跨平台）；
- **编译器**：Go 1.18+；
- **依赖**：零 CGO 依赖（仅 Go 标准库）；
- **容器化**：作为 Go 库嵌入到上层 Docker 镜像（如 Dgraph 镜像）；
- **嵌入式部署**：直接 `import "github.com/dgraph-io/badger"`；
- **云原生**：上层 Dgraph 等提供 Kubernetes Operator；
- **跨架构**：amd64 / arm64（Go 跨平台）。

## 二十三、风险与陷阱

- **ValueLog GC 抖动**：GC 期间 CPU + IO 占用高；可通过 `ValueLogLoadingMode` 控制；
- **value 太小时反而不优**：< 1KB value 时 WiscKey 优势不明显（指针开销）；
- **没有 Column Family**：多业务隔离需多 DB 实例；
- **并发事务需 OCC 友好**：高冲突率导致频繁重试；
- **Go GC 影响**：大量小 key 仍有 GC 压力（v4.x 优化后大幅改善）；
- **没有 TTL**：社区 fork `badtoml` 提供 TTL；
- **没有内置压缩**：snappy 可选关，需注意 SSD 占用；
- **崩溃恢复**：默认 `SyncWrites=false` 可能丢数据；
- **Range Scan 性能**：大量范围扫描 + 冷 SST 可能慢；
- **没有原生二级索引**：需 BadgerHold 或应用层管理；
- **encryption 性能**：v4+ encryption 引入少量开销；
- **GC pause**：大 DB 在 value log GC 期间有 pause。

## 二十四、与本项目的关联

- **`engineering/` 中可能的对应实现**：
  - `engineering/src/algo/` 中可借鉴 WiscKey 思想：分离 key/value 路径；
  - 跳表（skiplist）实现可作为 `learning/ds-c/orig/skiplist.c` 教学代码；
  - WAL + MemTable 设计可参考 `engineering/src/db/wal.c`；
- **学习复用价值**：
  - **WiscKey 风格分离 KV** 在 SSD 场景值得借鉴；
  - **纯 Go 实现** 对 Go 项目嵌入极为友好；
  - **跳表 + SST + ValueLog GC** 是 LSM 教学好案例；
  - **OCC 事务模型**实现简洁，可参考。

## 二十六、Value Log 分离写入与 GC 回收算法

§7.3 概述了 Value Log 的字段与读写入口，§7.7 概述了 GC 的五个步骤；本节补上**判定算法**层面：写路径如何分流、GC 如何选文件、如何判定一条记录是垃圾、重写代价有多大。

### 26.1 写路径分流与 value pointer 编码

每个 Entry 进入 write channel 前先过一次阈值判定（默认 `Options.ValueThreshold = maxValueThreshold`，量级 1MB；v3.2103 起可用 `VLogPercentile` 打开按分位数动态抬高阈值的模式）：

```
// 写请求预处理（简化伪代码）
for req in requests:
    e := req.Entry
    if len(e.Value) < opt.ValueThreshold:
        e.meta &^= bitValuePointer        // value 内联进 LSM
        lsm_payload = e.Value
    else:
        vptr := vlog.write(req)           // value 追加进 Value Log
        e.meta |= bitValuePointer         // LSM 里只放 value pointer
        lsm_payload = vptr.Encode()       // 12 字节：Fid(4) | Len(4) | Offset(4)
    lsm_entry = e.KeyWithTs() + meta + expiresAt + lsm_payload
```

存储代价对比（|K| = key 长度，|V| = value 长度，t = 8 字节版本号）：

| value 存放位置 | LSM 中每 key 字节数 | compaction 重写成本 |
|---|---|---|
| 内联 LSM（\|V\| < ValueThreshold） | \|K\| + t + meta + 8 + \|V\| | 与 \|V\| 成正比 |
| value pointer（\|V\| ≥ ValueThreshold） | \|K\| + t + meta + 8 + **12** | 固定 12 字节，与 \|V\| 无关 |

这就是 WiscKey 分离的核心收益：compaction 只搬运 12 字节指针，把 |V| 级别的重写成本转嫁给 append-only 的 Value Log（顺序写、无覆盖放大）。代价是读路径多一次 vlog 随机读——这也解释了 §十三中"value 太小时反而不优"（< 1KB 时指针与元数据开销占比过高）。

### 26.2 GC 触发与候选文件选择

v3 以后 GC **不再自动触发**，由调用方显式执行 `db.RunValueLogGC(discardRatio)`；§7.7 描述的"总量超阈值才清"是 v1/v2 的自动 GC 模型。`discardRatio ∈ (0,1)` 表示"至少要能回收这么多比例才值得重写"。

可回收量不是靠全量扫描得到的，而是靠 `discard.go` 中 mmap 持久化的 `discardStats`：LSM compaction 每丢弃一个版本，就把被丢弃 value 所在 fid 的可回收字节累加进去。GC 因此退化成"读一个统计表"。

```
// pickLog：选出本轮值得重写的 vlog 文件（简化伪代码）
pickLog(discardRatio):
    fid, discarded := vlog.discardStats.MaxDiscard()   // 可回收字节最多的文件
    if fid == 0:                     return randomFid() // 统计缺失 → 随机挑
    lf := vlog.files[fid]
    ratio := float64(discarded) / float64(lf.size)
    if fid == vlog.maxFid:           return ErrNoRewrite // 活跃文件永不参与
    if ratio < discardRatio:         return ErrNoRewrite // 回收不划算
    return lf
```

三条安全约束：

1. **活跃文件（`maxFid`）永不重写**，否则在途 append 会丢；
2. 只有被 compaction 明确丢弃过的 fid 才进 `discardStats`，未统计到的文件默认不动——这让 GC 成本与"实际产生的垃圾量"成正比；
3. 全流程受容量为 1 的 `garbageCh` 互斥，并发重复调用直接返回 `ErrRejected`。

### 26.3 存活判定与 rewrite

重写判据是"**LSM 里这个 key 当前指向谁**"。每条 vlog entry 的 key 尾部带 version（`y.ParseTs(e.Key)`），与 LSM 中的 `y.ValueStruct.Version` 比对：

```
// discardEntry：判定一条 vlog entry 是否为垃圾（源码同名函数的条件）
discardEntry(e, vs):
    if vs.Version != y.ParseTs(e.Key):              return true  // ① LSM 已是更新版本
    if isDeletedOrExpired(vs.Meta, vs.ExpiresAt): return true  // ② 已删除/已过期
    if (vs.Meta & bitValuePointer) == 0:           return true  // ③ value 已内联回 LSM
    if (vs.Meta & bitFinTxn) > 0:                  return true  // ④ 仅是事务提交标记
    return false
```

rewrite 主循环（简化伪代码）：

```
rewrite(lf):
    for e in lf.iterate():                       # 按 offset 顺序扫全文件
        vs := db.get(y.ParseKey(e.Key))          # O(log_T N) 定位 LSM 中的最新版本
        if discardEntry(e, vs):        continue  # 垃圾，不重写
        vp := vs.ValuePointer()
        if vp.Fid > lf.fid:             continue  # 已迁到更新的文件
        if vp.Fid == lf.fid && vp.Offset > e.offset: continue  # 同文件内已被新版本覆盖
        newVp := vlog.write(e)                   # 追加到当前活跃文件
        db.batchSet(y.ParseKey(e.Key), newVp)    # LSM 指针改指新位置（带 version 校验）
    把 lf 加入待删队列，numActiveIterators > 0 时延迟 unlink
```

关键点：**先写新数据、再改 LSM 指针、最后删旧文件**，三步严格有序；任一步崩溃都能恢复（旧 entry 仍在，新 entry 至多多一份，不会丢）。旧文件的 unlink 推迟到活跃迭代器归零，避免正在扫描 vlog 的 Iterator 读到已被删除的 inode——这是 §二十三"GC 期间有 pause"与抖动风险的根因之一。

### 26.4 复杂度与写放大

设单条 value 平均大小 s、GC 可回收比例 r：

| 环节 | 成本 |
|---|---|
| 回收量统计 | 由 compaction 顺带累加，摊到写路径上，近似 O(1)/entry |
| rewrite 单文件 | O(文件字节 / s) 次 LSM 查找，即 O(文件字节 × log_T N / s) |
| vlog 生命周期写放大 | 1 + r + r² + … = **1/(1-r)**：r=0.5 → 2×，r=0.7 → 3.3× |
| GC 抖动 | 单文件 rewrite 期间 vlog 顺序写 + LSM 批量写，CPU/IO 出现尖峰 |

因为 1/(1-r) 是几何级数，**r 越大越省空间、写放大越高**：r=0 意味着永不回收（空间放大无上界），r→1 则每轮全量重写。空间收益有界、写放大无界，所以官方文档推荐 0.5~0.7 作为折中。

> 源码：`value.go`（canonical，模块未 init，行号需校对）`type valueLog`、`valuePointer`、`discardEntry`、`pickLog`、`rewrite`、`doRunGC`、`ErrNoRewrite`；`discard.go`（approx）`discardStats` / `Update` / `MaxDiscard`；`db.go`（approx）`RunValueLogGC`；`structs.go`（approx）`Entry` 与 `bitValuePointer` / `bitFinTxn` 等 meta 位；`y/y.go`（approx）`KeyWithTs` / `ParseTs` / `ParseKey`。默认值以 `options.go` 为准。

## 二十七、事务 MVCC：时间戳分配与冲突检测

§十 已给出 OCC + Snapshot Isolation 的结论，本节展开**算法层**：时间戳从哪来、冲突怎么判、两个水位线为什么必须存在。

### 27.1 内部 key 编码：版本号内嵌且倒序

Badger 不为 MVCC 单独维护版本链，而是把 version 编进 key 尾部 8 字节，取补码 `math.MaxUint64 - ts`，使**新版本在字典序中排在前面**：

```
internal_key = user_key || (math.MaxUint64 - ts)        // y.KeyWithTs
user_key = y.ParseKey(internal_key); ts = y.ParseTs(internal_key)   // 去掉/还原尾部 8 字节
```

`Get(user_key, ts)` 在 MemTable / SST 中的定位过程：seek 到 `user_key` 的第一个位置，跳过尾部 ts 大于目标 ts 的记录，取第一个 ≤ 目标 ts 的版本。compaction 只保留每个 user_key 的最新可达版本，被丢弃的版本正是 §26.2 中 `discardStats` 的来源——**MVCC 的版本回收与 Value Log 的空间回收共用同一套"LSM 丢弃"信号**。

### 27.2 read timestamp 分配：两个水位线

`readTs` 不是"当前时间"，而是"**已经分配出去的所有 commit ts 的上界**"，且分配后必须等所有 ≤ 它的提交落盘可见：

```
// txn.go 中 oracle.readTs() 的真实逻辑
func (o *oracle) readTs() uint64 {
    if o.isManaged { panic("ReadTs should not be retrieved for managed DB") }
    var readTs uint64
    o.Lock()
    readTs = o.nextTxnTs - 1        // ① 取所有已分配 commitTs 的上界
    o.readMark.Begin(readTs)        // ② 登记为"活跃读者"
    o.Unlock()
    // ③ 等所有 ≤ readTs 的事务写完 Value Log + LSM 才返回
    y.Check(o.txnMark.WaitForMark(context.Background(), readTs))
    return readTs
}
```

两个水位线各管一头：

| 水位线 | 语义 | 推进者 |
|---|---|---|
| `readMark` | 活跃读事务集合；`DoneUntil()` 给出"低于此版本即可丢弃"的回收线 | 读事务 Begin/Done |
| `txnMark` | 已分配 commitTs、但尚未落盘完成的事务 | 写事务分配时 Begin，落盘完成后 Done |

- `txnMark.WaitForMark(readTs)` 保证 **① 不会读到半提交数据**：新事务开始读之前，所有更早的提交必须已可读；
- `readMark.DoneUntil()` 保证 **② 不会回收活跃读者仍需要的版本**：compaction 的丢弃水位、vlog GC 的候选筛选都以它为准（统一入口 `discardAtOrBelow()`）。

这两条合起来，正是 §十 中"持久化一致性"与"冲突重试"得以成立的前提：一致性来自 `txnMark` 的等待，不来自读侧加锁。

### 27.3 冲突检测：指纹集合求交

写事务在 `Txn.modify` 时把每个被写 key 的指纹记入 `conflictKeys map[uint64]struct{}`（指纹取 `z.MemHash(key)`，不必留整份 key）。读集合不需要记录——**冲突 = 我的写集合 ∩ 你在我 readTs 之后提交的写集合 ≠ ∅**：

```
// newCommitTs / hasConflict 的逻辑（简化伪代码）
newCommitTs(txn):
    lock(writeChLock)                        // 保证进入 write channel 的顺序 == commitTs 顺序
    for c in oracle.committedTxns:           // 最近提交历史
        if c.ts <= txn.readTs:    continue  // 我开始读之前就提交的，与我无关
        if intersect(c.conflictKeys, txn.conflictKeys):
            return 0, ErrConflict            // 键集合相交 → 写-写冲突
    ts := oracle.nextTxnTs++
    oracle.txnMark.Begin(ts)                 // 登记"已分配、未落盘"
    oracle.committedTxns.append({ts, txn.conflictKeys})
    cleanupCommittedTransactions()           // 丢弃 ≤ readMark.DoneUntil() 的历史
    unlock()
    return ts, nil
```

工程细节：

- **只查写-写冲突**：读-写冲突由版本可见性天然排除（我读到的是 readTs 快照，你写的新版本我看不见）——这是 Snapshot Isolation 的典型取舍，代价是允许 write skew（写偏差）；
- **历史必须裁剪**：`committedTxns` 不裁剪会无限增长，`cleanupCommittedTransactions` 以 `readMark.DoneUntil()` 为界删除不可能再被任何活跃读者用于判冲突的条目；
- **指纹碰撞**：64 位哈希的碰撞概率约 O(K²/2^64)，工程上忽略；
- **Managed 模式**：`NewTransactionAt(readTs, update)` 由调用方直接指定 readTs，跳过 `readTs()` 的水位线登记，同步责任上移给上层（例如 Dgraph 用 Raft 的 applied index 当 readTs）。

### 27.4 WaterMark 数据结构与复杂度

`y.WaterMark`（`y/watermark.go`）是单文件实现的多读者进度条：一个 goroutine 消费请求队列，内部用最小堆 + 引用计数维护"连续完成到哪个 index"：

```
type WaterMark struct {
    doneUntil atomic.Uint64   // 连续完成水位（64 位对齐，必须放最前）
    lastIndex atomic.Uint64
    Name      string          // 如 "badger.PendingReads"
    markCh    chan mark       // 容量 100 的请求队列
}
// process goroutine 内部状态：
indices  uint64Heap                     // 活跃 index 的最小堆
pending  map[uint64]int                // 每个 index 的 Begin/Done 引用计数
waiters  map[uint64][]chan struct{}    // 阻塞在 WaitForMark 上的等待者

processOne(mark):
    pending[mark.index] += (mark.done ? -1 : +1)
    if !mark.done: indices.push(mark.index)
    for indices 非空 && pending[indices.min()] <= 0:
        until := indices.pop(); doneUntil.CAS(until)
    close(waiters[k]) for all k <= until      # 放行等待者
```

复杂度（P = 活跃 index 数，即并发事务数）：

| 操作 | 复杂度 | 备注 |
|---|---|---|
| `Begin(index)` / `Done(index)` | O(log P) | 入最小堆 + 引用计数 |
| `WaitForMark(ctx, index)` | O(1) 判定 + 阻塞 | 判定只读 `doneUntil` 原子量 |
| `DoneUntil()` | O(1) | 一次原子读，供 compaction / vlog GC 使用 |
| 内存 | O(P) | 每个活跃事务一个堆元素 + 计数项 |

### 27.5 隔离级别与代价小结

- **隔离级别**：Snapshot Isolation（不满足可串行化，允许写偏差）；检测到冲突返回 `ErrConflict`，由用户重试整个事务（对应 §二十三"OCC 需友好、高冲突率导致频繁重试"）；
- **纯读事务开销**：不写 `readMark.Begin/Done` 时近似为零成本，这也是 §九中 `db.View` 廉价的原因；
- **冲突检测代价**：稳态 O(并发提交窗口数 × 本事务写 key 数)，窗口大小受 `readMark.DoneUntil()` 裁剪，**与历史事务总数无关**；
- **全局提交序**：`writeChLock` + 单调递增的 `nextTxnTs` 构成串行化点，且提交顺序与写 channel 顺序严格一致，这是崩溃恢复（replay WAL / vlog）能重建确定状态的前提。

> 源码：`txn.go`（canonical，模块未 init，行号需校对）`type oracle`（`readTs` / `newCommitTs` / `hasConflict` / `doneCommit` / `doneRead` / `discardAtOrBelow` / `cleanupCommittedTransactions`）、`type committedTxn`、`Txn.modify`、`ErrConflict`；`y/watermark.go`（approx）`type WaterMark` 与 `Begin` / `Done` / `DoneUntil` / `WaitForMark`；`y/y.go`（approx）`KeyWithTs` / `ParseTs` / `ParseKey` / `SameKey`；`managed_db.go`（approx）`NewTransactionAt`。注：部分资料把 oracle 写作独立文件 `oracle.go`，v4 主线已合并进 `txn.go`，以 `txn.go` 为准。

## 二十八、LSM 层目标计算与 Compaction 调度

§7.6 给出了 score 的一行定义，§8 提到 Bloom filter；本节展开**目标容量怎么算、优先级怎么排、文件怎么选**，并给出读放大的量化代价。

### 28.1 层容量目标：等比数列 + 动态 base level

Badger 采用 RocksDB 式**动态 leveled 布局**：目标容量按等比数列递减，而 base level 由当前 LSM 的实际大小反推（不是固定 L1），避免大库把大量数据挤在上层。

```
// levelTargets() 的核心（简化伪代码）
target(L)        = opt.BaseLevelSize * opt.LevelSizeMultiplier^(L - baseLevel)
maxTotalSize()   = Σ target(L)  for L in [baseLevel, opt.MaxLevels)
```

`DefaultOptions` 的默认参数（以 `options.go` 为准，v1 时代命名与取值差异很大）：`BaseLevelSize = 10MB`、`LevelSizeMultiplier = 10`、`BaseTableSize = 2MB`、`TableSizeMultiplier = 2`、`MaxLevels = 7`。于是顶层与 base level 的容量上限相差 10⁵ ≈ 十万倍；base level 每下移一层，可容纳的总数据量扩大 10 倍。`levelTargets()` 同时算出 `keySplits`——各层预计要切成多少个 SST，供 compaction 的文件切分与 `subcompact` 参考。

`baseLevel` 必须被夹在 ≥ 1：历史上出现过当 LSM 规模增长到约 TB 量级时 `levelTargets()` 算出 `baseLevel = 0`、导致 L0→L0 崩溃的 bug，修复方式就是 clamp。

### 28.2 Compaction 优先级：三级筛选

```
// pickCompactLevels() 的三步（简化伪代码）
// ① 原始打分
prios = []
for li in levels:
    if li.level == 0:
        score = len(li.tables) / opt.NumLevelZeroTables     // 默认 5，按文件数而非字节数
    else:
        score = (size(li) - delSize(li)) / target(li)      // delSize = 已被 compaction 标记删除的字节
    if score < 1.0: continue
    prios.append({li, score})

// ② 自底向上调整：下层已爆 → 上层优先级下调，避免上层做无用功
for i from baseLevel down to 0:
    if prios[i].adjusted >= 1.0:
        prios[i-1].adjusted /= prios[i].score

// ③ 保留 adjusted ≥ 1.0 的层，降序排序；第 0 个 compactor 永远优先跑 L0
return sorted(prios where adjusted >= 1.0)
```

L0 用**文件数**计分，因为 L0 的 SST 之间 key range 允许重叠——限制文件数才是限制 L0 读放大的手段。`NumLevelZeroTables = 5` 触发 compaction，`NumLevelZeroTablesStall = 15` 触发 write stall，这就是 §6.2 中"后台 compaction goroutine"的两个阈值语义。

### 28.3 文件选择与重叠判定

选中层 L 之后，取哪几个 SST、要不要连带合并 L+1 的 SST，由 key range 决定：

```
// keyRange 构造（compaction.go）
left  = y.KeyWithTs(y.ParseKey(smallest), math.MaxUint64)  // 左端取 MaxUint64，纳入同 key 的所有版本
right = y.KeyWithTs(y.ParseKey(biggest),   0)               // 右端取 0
overlaps(A, B) = A.left <= B.right && B.left <= A.right

// fillTables 的核心（简化伪代码）
fillTables(cd):
    for t in sortByHeuristic(thisLevel.tables):      # 按 MaxVersion 升序，优先老的
        kr := getKeyRange(t)
        if cstatus.overlapsWith(thisLevel.level, kr): continue   # 与在途 compaction 重叠则跳过
        cd.top.append(t); cd.thisRange = kr
        cd.bot = nextLevel.overlappingTables(kr)      # 二分查找全部重叠 SST
        cd.nextRange = (cd.bot 为空) ? cd.thisRange : getKeyRange(cd.bot...)
    register(cd)      # 把 thisRange 并入 cstatus.ranges，并把涉及的 SST 标记为 busy
```

两个要点：

- **trivial move**：若 `cd.bot` 为空（L+1 无任何 SST 与之重叠），无需归并，直接把 `thisLevel` 的 SST **改名**挪到 L+1 —— 省掉一整轮 O(S) 读写，这正是 §7.6 提到的 Trivial Move；
- **互斥**：`compactStatus.ranges` 记录所有在途 compaction 占用的 key range，`overlapsWith` 拒绝与之重叠的候选，保证同一批 SST 不会被两个 compaction 同时改写；L0→L0 的同层多文件情形由 `fillTablesL0ToL0` 单独处理。

### 28.4 Bloom filter 尺寸与读放大

Badger **每个 SST 一个 bloom filter**（不是每个 block 一个），随索引一起落盘，读路径在解 block 之前用 `t.DoesNotHave(hash)` 就能否掉整个 SST。给定每 SST 的 key 数 n 与目标误判率 p，最优位数为：

```
m = ⌈ -n · ln p / (ln 2)² ⌉  bits
```

默认 `BloomFalsePositive = 0.01` ⇒ m ≈ 9.6·n bits ≈ **1.2 字节/key**（n = 100 万 ⇒ 约 1.2MB）。这是 Bloom filter 的空间下界，也说明它只能挡掉"肯定不存在"的查询，剩下 ≤ 1% 的假阳性查询仍然要走完整的 LSM 查找。

读放大：`LevelSizeMultiplier = 10` 时，点查最多触碰 L0 的若干 SST + 每层 1 个 SST，总代价 ≈ O(log₁₀ N) 次 SST 打开 + 1 次 Value Log 读；bloom 把"每层都可能打开 SST"这个常数压掉，把多余 IO 挡在 filter 判定这一步（对应 §十三"读放大：每层 1 IO + 1 ValueLog IO"）。

### 28.5 复杂度汇总

| 阶段 | 复杂度 | 备注 |
|---|---|---|
| `pickCompactLevels` | O(L)，L = MaxLevels = 7 | 每层一次统计 |
| `overlappingTables` | O(log M) | M = 该层 SST 数，二分查找 |
| `sortByHeuristic` | O(M log M) | 按 MaxVersion 升序，与 RocksDB 的 oldest-largest-first 同构 |
| `compactBuildTables` 归并 | O(S/B) 次迭代、写 S 字节 | S = 参与 compaction 的总字节，B = SST 大小 |
| trivial move | O(1) | 只改文件名，不动数据 |
| 点查 | O(log_T N) 次 SST + 1 次 Value Log 读 | 每层最多 1 个（bloom 提前否掉） |

> 源码：`levels.go`（canonical，模块未 init，行号需校对）`type levelsController`、`levelTargets()`、`pickCompactLevels()`、`fillTables()`、`fillTablesL0ToL0`、`sortByHeuristic`、`subcompact`；`compaction.go`（approx）`type compactDef`、`getKeyRange`、`keyRange.overlapsWith`、`compactStatus.overlapsWith` / `compareAndAdd`、`compactBuildTables`；`level_handler.go`（approx）`type levelHandler`、`overlappingTables`；`bloom/bloom.go` + `table/builder.go`（approx）`type Filter` 及其在索引中的序列化；`table/table.go`（approx）`DoesNotHave`。默认参数以 `options.go` 为准。

## 二十九、参考资料

- [BadgerDB 官方文档](https://pkg.go.dev/github.com/dgraph-io/badger)
- [BadgerDB GitHub](https://github.com/dgraph-io/badger)
- [WiscKey 论文（Xia et al. 2017）](https://www.usenix.org/system/files/conference/fast17/fast17-xia.pdf)
- [LSM-Tree 原始论文（O'Neil 1996）](https://www.cs.umb.edu/~poneil/lsmtree.pdf)
- [Skip Lists 论文（Pugh 1990）](https://15721.codetables.org/slides/2023/07_pugh_1990.pdf)
- [Dgraph 博客 - Badger 设计](https://blog.dgraph.io/post/badger/)
- [Manish Rai Jain - Badger 详解](https://blog.dgraph.io/post/badger-v1/)
- [Badger v3 事务文档](https://github.com/dgraph-io/badger/blob/v3.2103.1/README.md)
- [BadgerHold - 对象索引](https://github.com/timshannon/badgerhold)
- [Dgraph - 分布式图数据库](https://dgraph.io/)