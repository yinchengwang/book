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

## 二十五、参考资料

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