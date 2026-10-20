# RocksDB Wiki（嵌入式 LSM 键值数据库）

> 参考版本：RocksDB 仓库当前 commit `8f492b2f47f2fa45efe7f05ae4e6e233997e5e49`（v8.x 系列；v6.x 引入 BlobDB，v7.x 引入 secondary cache 与改进的 compaction，v8.x 进一步优化内存管理）。

## 一、项目定位

RocksDB 是 Facebook（现 Meta）开源的一款**嵌入式、持久化、高性能 LSM-Tree 键值数据库**，由 Google LevelDB 演化而来，专为 SSD 与多核服务器优化。它通过库形态（library）嵌入到上层应用中，提供丰富的可调参数与组合式架构，被广泛用作 OLTP、消息存储、搜索索引、监控时序等场景的底层引擎。

- **痛点**：
  - LevelDB 功能简洁、性能受限，无法利用 SSD 高 IO 与多核 CPU；
  - 业务（MySQL、TiKV、ClickHouse、Flink、Cassandra 等）需要可嵌入的存储引擎而非独立数据库服务；
  - 高写入吞吐场景需要 LSM 写优化的天然优势，但又要兼顾读延迟与可调参数；
  - SSD 普及后，需要一个"为 SSD 量身定制"的 KV 引擎（减少写放大、利用并行压缩）。
- **典型用户**：Facebook/Meta（数百套服务使用）、LinkedIn、Uber、Pinterest、Netflix；上层产品 MyRocks（MySQL 的 RocksDB 引擎）、TiKV（TiDB 存储层）、CockroachDB 早期版本、Flink RocksDB State Backend、Apache Flink 流处理状态。

## 二、发展历史与社区

- **起源**：2012 年 Facebook 的 Dhruba Borthakur 基于 Google LevelDB 1.5 fork 出 RocksDB；初始目标是替换 HBase 为某些场景下更轻量的方案。
- **里程碑**：
  - 2013 年开源（BSD + GPLv2 双许可）；
  - 2014-2015 年加入 Column Family、Compaction Filter、TransactionDB（乐观事务）；
  - 2016 年 Universal Compaction 完善、Backup API；
  - 2017 年 BlobDB（分离大 value 到 blob 文件）；
  - 2018 年 SST 压缩编码改进（`ZSTD`、`LZ4`）、`CompactionJob` 重构；
  - 2019 年基于 C++20 计划、SecondaryCache 雏形；
  - 2020-2021 年 6.x 系列（Adaptive Bloom Filter、Per-key placement in blob）；
  - 2022 年 7.x（Tiered Storage、改进的 Rate Limiter、Checksum 增强）；
  - 2023-2024 年 8.x（更紧凑 SST、改进的 Iterator、内存优化）；
  - 2025 年 9.x 计划（持续优化）。
- **维护状态**：活跃；每月发版；约 300+ 贡献者。
- **社区**：Facebook Engineering Blog、GitHub Discussions、rocksdb-developers@ mailing list、RocksDB 峰会（每年与 TiDB/PingCAP 联合）。

## 三、许可与商业限制

- **许可证**：**GPLv2**（链接应用时）与 **BSD 3-Clause**（部分头文件）双协议。
  - 来源：`rocksdb/LICENSE`；
  - 关键约束：GPLv2 影响动态链接场景；多数生产系统（如 MyRocks、TiKV）以静态链接 + GPL 兼容许可发布；
  - 商业用户可购买 RocksDB 商业许可（联系 Meta/Facebook 商务）；
- **企业版差异**：Meta 自家使用 RocksDB 作为 MyRocks（MySQL 引擎）、LogDevice、ZippyDB 等系统的存储层；社区版功能与商业版几乎一致（不像 Redis Enterprise 那样区分明显）；
- **第三方商业支持**：Percona（MyRocks 优化）、PingCAP（TiKV）、Uber / Netflix 等均有内部维护 fork。

## 四、业界主要使用方与案例

- **Meta 自家**：MyRocks（Facebook 的 MySQL 后端替代 InnoDB）、LogDevice（分布式日志）、ZippyDB（KV 存储）；
- **数据库厂商**：TiKV（TiDB 存储）、CockroachDB（早期，2021 年切换为 Pebble）、YugabyteDB 早期、Ceramic（ClickHouse 第三方引擎）；
- **流处理**：Apache Flink（State Backend）、Apache Samza、Apache Beam；
- **消息队列**：Apache Pulsar（BookKeeper 层之上可选 RocksDB 索引）；
- **监控时序**：Prometheus TSDB 部分场景；
- **云服务**：Uber（Michelangelo ML 特征存储）、LinkedIn（Kafka 索引层）、Netflix（EVCache 缓存后端）。

## 五、一句话对比定位

> RocksDB 是**"嵌入式 LSM + SSD 优化 + 可组合模块"**的 KV 引擎标杆；与 **LevelDB（轻量级 LSM、同步线程、纯 C++11）**、**Pebble（CockroachDB fork、模块化、Go 重写）**、**Badger（Go 实现、WiscKey 风格分离 KV）** 共同构成**LSM 嵌入式 KV 阵营**。

## 六、整体架构

RocksDB 采用**模块化分层架构**，每个组件都是可替换的"pluggable"模块；用户可通过 `DBOptions` / `ColumnFamilyOptions` 调整。

### 6.1 总体分层

```
┌──────────────────────────────────────────────────────────────────────┐
│                       用户 API (include/rocksdb/db.h)                 │
│   - DB / DBImpl / TransactionDB                                      │
│   - ColumnFamilyHandle（多列族）                                     │
│   - Iterator / ReadOptions / WriteOptions                            │
└─────────────────────┬────────────────────────────────────────────────┘
                      │
┌─────────────────────▼────────────────────────────────────────────────┐
│                 前端抽象层 (db/column_family.h)                       │
│   - VersionSet  ：所有版本（SST 文件集 + 元数据）                     │
│   - Version     ：一个时间点的视图                                    │
│   - VersionEdit ：版本变更操作                                       │
│   - ColumnFamilyData ：单个列族的所有可变状态                         │
└─────────────────────┬────────────────────────────────────────────────┘
                      │
┌─────────────────────▼────────────────────────────────────────────────┐
│        MemTable (db/memtable.h + memtable/)                            │
│   - 默认跳表 (skiplist) 可选 prefix bloom / hash                      │
│   - MemTableList ：immem 链表（flush 前冻结）                         │
│   - 写满阈值 → 变为 immut memtable → flush 到 L0                     │
└─────────────────────┬────────────────────────────────────────────────┘
                      │
┌─────────────────────▼────────────────────────────────────────────────┐
│        Write-Ahead Log (db/wal_manager.h)                            │
│   - WAL writer (db/log_writer.h)                                     │
│   - WAL reader (db/log_reader.h)                                     │
│   - 32KB record（kBlockSize），CRC32 校验                             │
│   - 支持 Multi-Log（ColumnFamily 隔离）                               │
└─────────────────────┬────────────────────────────────────────────────┘
                      │
┌─────────────────────▼────────────────────────────────────────────────┐
│        SST 文件层 (table/)                                            │
│   - SST 文件格式 (table/format.h + table/block_based_table_builder)  │
│   - Block-based Table + Plain Table + Cuckoo Table                    │
│   - Block cache (cache/lru_cache.h)                                  │
│   - Bloom Filter (util/bloom.cc)                                     │
│   - Index/Filter blocks (partitioned index)                           │
└─────────────────────┬────────────────────────────────────────────────┘
                      │
┌─────────────────────▼────────────────────────────────────────────────┐
│        Compaction (db/compaction/)                                    │
│   - Level compaction（通用）/ Universal compaction（写密集）         │
│   - FIFO compaction（纯缓存）/ Manual compaction                     │
│   - Compaction Job + Subcompaction（并行）                            │
└─────────────────────┬────────────────────────────────────────────────┘
                      │
┌─────────────────────▼────────────────────────────────────────────────┐
│        底层 IO (env/io_posix.h + util/)                              │
│   - Env 抽象（POSIX / HDFS / S3 / Memory）                            │
│   - RateLimiter（IO 流控，避免压垮 SSD）                             │
│   - FileSystem 接口                                                   │
│   - 多线程：FlushThread / CompactionThreadPool / BGThreads          │
└──────────────────────────────────────────────────────────────────────┘
```

源码入口：
- DB 入口：`db/db_impl/db_impl.h`（`class DBImpl : public DB`）
- 列族管理：`db/column_family.h`
- 写路径：`db/db_impl/db_impl_write.cc`
- 读路径：`db/db_impl/db_impl_readonly.cc`
- 压缩：`db/compaction/compaction_job.cc`

### 6.2 进程模型

RocksDB 是**嵌入式库**，线程由上层应用控制，但内部有多个**后台线程**：

- **Flush 线程**（1+）：将 memtable flush 到 L0；
- **Compaction 线程池**（可配置 N 个）：执行 L0→L1、L1→L2... 或 Universal Compaction；
- **BG 线程池**：垃圾文件清理（WAL/SST）、统计信息采样、统计信息 flush；
- **用户线程**：负责 Put / Get / Delete，前端无锁原子操作；
- **Sync/Callback 线程**：WriteCallback / CbOnComplete。

线程模型特点：
- 前端 **无锁读**（通过 `VersionSet::current()` 原子指针 + Reference Counting）；
- 写线程互斥（写者之间竞争 `write_thread_` mutex，但写入 memtable 是 batched）；
- Compaction 后台运行，**不阻塞前端**（但会竞争 IO）。

### 6.3 一条 PUT 命令的端到端路径

1. **应用调用**：`DB::Put(WriteOptions&, ColumnFamilyHandle*, const Slice&, const Slice&)`；
2. **写入 WAL**：`db/log_writer.h` 写入预写日志（默认 32KB record，多个 KV 拼一个 record）；
3. **写入 MemTable**：`db/memtable.h` 默认 Skiplist 实现；
4. **写完成通知**：`WriteCallback::OnWriteCompleted`；
5. **MemTable 满**（默认 64MB）→ 转 immut → 后台 Flush：
   - 创建 SST Writer；
   - 遍历 memtable（迭代器）；
   - 写 data block、index block、meta index block、footer；
   - 写入 L0（无序，与之前的 L0 文件 key range 重叠）；
   - 删除 WAL（`wal_manager` 清理）；
6. **Compaction 后台调度**：根据 `level_compaction_dynamic_level_bytes` 或 `num_levels` 决定何时触发压缩；
7. **响应前端**：默认 `WriteOptions::sync=false` 不 fsync，强一致需求 `sync=true`。

### 6.4 一条 GET 命令的端到端路径

1. **应用调用**：`DB::Get(ReadOptions&, ColumnFamilyHandle*, const Slice&, std::string*)`；
2. **检查 memtable**（active + immut 链）：`MemTable::Get()` 跳表查询；
3. **检查 Block Cache**：缓存的 SST 数据块（data block）；
4. **查 L0 SST**：因 L0 文件 key range 可能重叠，需要检查全部 L0 文件（带 bloom 加速）；
5. **逐层下推**：L1~L7 各层只有一个 SST 与查询 key range 相交（level compaction 保证）；
6. **Bloom Filter**：`util/bloom.cc` 按 `BlockBasedTableReader::Get()` 判断 key 可能存在；
8. **Index Block**：定位到具体 data block；
9. **Data Block**：二分查找（默认 16 entries / restart interval）；
10. **返回 value 或 NotFound**。

## 七、存储引擎（KV 类型核心库深度展开）

RocksDB 的核心是基于 **LSM-Tree（Log-Structured Merge Tree）** 的多层结构，配以 Bloom Filter、Block Cache 等读优化。

### 7.1 LSM-Tree 总体架构

```
                       Write Path (Hot)
┌──────────────────────────────────────────────────────────────────┐
│                                                                  │
│  ┌────────────────┐      ┌──────────────────────────┐            │
│  │  Active        │      │  WAL                     │            │
│  │  MemTable      │ ───> │  (db/log_writer.h)       │            │
│  │  (skiplist)    │      │  32KB records, CRC32     │            │
│  └────────┬───────┘      └──────────────────────────┘            │
│           │ (full)                                              │
│           ▼                                                     │
│  ┌────────────────┐      ┌──────────────────────────┐            │
│  │  Immutable     │ ───> │ Flush to L0               │            │
│  │  MemTable(s)   │      │ (table/block_based_*)    │            │
│  └────────────────┘      └──────────────────────────┘            │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘

                       Read Path (Cold)
┌──────────────────────────────────────────────────────────────────┐
│  Check Active MemTable → Immutable → Block Cache → L0 → L1 → L2 │
│                                                                  │
│  L0: 重叠 SST（level compaction 触发后 sort）                     │
│  L1: 非重叠 SST，按 key 排序                                     │
│  L2~L7: 同 L1，但单文件更大（默认 10^N 倍）                       │
│                                                                  │
│  Level Compaction: 选中 Ln 的某个文件 + Ln+1 与之重叠的所有文件  │
│                   → 多路归并排序 → 新 Ln+1 文件                   │
└──────────────────────────────────────────────────────────────────┘
```

### 7.2 MemTable（基于跳表）

`db/memtable.h`：

```cpp
// db/memtable.h:31
class MemTable {
 public:
  // 跳表实现（默认）或 prefix hash
  MemTable(const InternalKeyComparator& comparator,
           const ImmutableMemTableOptions* memtable_options,
           const SequenceNumber latest_seq, ...);
  
  void Add(SequenceNumber s, ValueType type, const Slice& key,
           const Slice& value, ...);
  
  bool Get(const LookupKey& key, std::string* value, Status* s, ...);
};

// 跳表节点（memtable/skiplist.h）
struct Node {
  Node* next_[kMaxHeight];  // kMaxHeight=12
  uint32_t height_;
  // 紧跟 next_ 数组之后是 key_data
};
```

要点：
- **跳表（skiplist）**：O(log N) 插入/查找，无需重平衡，比红黑树更适合多线程并发写；
- **高度随机**：每节点高度按 P=1/4 几何分布（期望高度 1.33）；
- **Prefix Bloom**：可选对 key 前缀建 bloom（针对 `Get(Seek)` 范围查询的优化）；
- **MemTable Allocator**：默认 `Arena`（内存池），减少 malloc/free 开销；
- **Flush 触发**：`write_buffer_size=64MB` 满后冻结，转 immut，下一个 memtable 接管。

### 7.3 SST 文件结构（Block-Based Table）

`table/block_based_table_builder.h`：

```
┌───────────────────────────────────────────────────────────────┐
│                       SST 文件结构                              │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────────────┐ │
│  │ Data     │ │ Data     │ │ ...      │ │  Meta Index Block │ │
│  │ Block 1  │ │ Block 2  │ │          │ │  (offsets/filter)│ │
│  └──────────┘ └──────────┘ └──────────┘ └──────────────────┘ │
│  ┌──────────────────────────────────────────────────────────┐ │
│  │                Index Block                                │ │
│  │  (data block 的 offset + first key + restart interval)    │ │
│  └──────────────────────────────────────────────────────────┘ │
│  ┌─────────────┐ ┌────────────────────────────┐                │
│  │ Filter Block│ │ Range Del Block            │                │
│  └─────────────┘ └────────────────────────────┘                │
│  ┌──────────────────────────────────────────────────────────┐ │
│  │                Properties Block + Footer                   │ │
│  └──────────────────────────────────────────────────────────┘ │
└───────────────────────────────────────────────────────────────┘
```

要点：
- **Data Block**：默认 4KB 压缩（Snappy / LZ4 / ZSTD），按 key 排序，每 16 个 entry 一个 restart point（二分查找起点）；
- **Index Block**：两段式（partitioned index）— 顶层 index block 指向二级 index blocks，二级 block 再指向 data blocks；
- **Filter Block**：每个 data block 对应的 bloom filter（默认 10 bits/key，1% 假阳性）；
- **Compression**：默认 Snappy；`compression_per_level` 可按层选不同算法；
- **Checksum**：每个 block 带 CRC32；footer 含 `kBlockBasedTableMagicNumber`。

### 7.4 Compaction（LSM 核心后台）

`db/compaction/compaction_job.h`：

```cpp
// db/compaction/compaction.h:39
class Compaction {
 public:
  // compaction 输入：cfd_, inputs_[0..num_input_levels-1], output_level_
  // level compaction: inputs[0] 是 Ln 一个文件，inputs[1] 是 Ln+1 重叠文件集
  // universal compaction: inputs[0] 是整个 sorted run 数组
  
  Compaction(Version* input_version, ...);
  bool IsTrivialMove() const;  // L0→L1 无 overlap → 直接 move
  // ...
};
```

要点（**Level Compaction**）：
- **Score**：`score = total_size(L_n) / max_bytes_for_level(L_n)`；score > 1 触发压缩；
- **文件选择**：L_n 中得分最高的文件 + L_n+1 与之 key range 重叠的所有文件；
- **多路归并**：k 路归并（k = 1 + 1 = L_n+1 重叠文件数）；
- **Subcompaction**：当输出文件 > `max_subcompaction_bytes` 时分裂成 N 个并行任务（提升 SSD 并发）；
- **Trivial Move**：L_n 文件与 L_n+1 无重叠（仅边界相邻），直接 move 避免归并代价；

要点（**Universal Compaction**）：
- 适合写密集 / 时序场景；
- 按 `size_ratio` 阈值判断何时合并相邻 sorted run；
- 整体读放大小（按文件数级别）但写放大大。

### 7.5 Bloom Filter

`util/bloom.cc`：

```cpp
// util/bloom.cc:55
class BloomFilterPolicy : public FilterPolicy {
 public:
  // bits_per_key_, k_（hash 函数数 = bits_per_key * ln(2) ≈ bits/0.69）
  void CreateFilter(const Slice* keys, int n, std::string* dst) override;
  bool KeyMayMatch(const Slice& key, const Slice& bloom_filter) override;
};
```

要点：
- **Block-based Bloom**：每个 data block 一个 bloom（精度最高）；
- **Ribbon Filter**（v7+）：比 bloom 节省 30% 空间，CPU 类似；
- **Partitioned Filter**：与 partitioned index 一一对应，filter block 拆分；
- **Whole Key vs Prefix**：默认 whole key；可配 prefix bloom for `prefix_extractor`。

### 7.6 Block Cache

`cache/lru_cache.h`：

```cpp
// cache/lru_cache.h:46
class LRUCache : public Cache {
  // ShardedLRUCache（默认 16 shard，按 hash(key) 分发降低锁竞争）
  // LRU 链表 + 哈希表
  // 容量控制（usage / capacity），逐出策略
};
```

要点：
- **Sharded LRU**：默认 16 shard，每个 shard 一个 LRU 链表 + 哈希表；
- **Secondary Cache**：v7+ 支持 `compressed_secondary_cache`（NVMe / blob）；自动 promotion 到 primary；
- **三种类型**：Data Block Cache（默认开）、Index Block Cache（默认开）、Filter Block Cache（默认开）；
- **统计**：`GetStats()` 提供 hit/miss 计数；
- **自定义缓存**：可通过 `NewCache()` 注入自定义实现（如 CCache、Caffeine）。

### 7.7 WAL（Write-Ahead Log）

`db/log_writer.h` / `db/log_reader.h`：

```
┌──────────────────────────────────────────────────────────────┐
│  WAL 文件结构（32KB record，跨多个 KV）                        │
│  ┌────────────────────────────────────────────────────────┐  │
│  │  Record 1: [CRC32(4B)][Length(2B)][Type(1B)][Data(N B)] │  │
│  ├────────────────────────────────────────────────────────┤  │
│  │  Record 2 ...                                            │  │
│  │  ...                                                     │  │
│  │  Record N (last_kv_block_size 标记)                      │  │
│  └────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────┘
```

要点：
- **顺序写**：append-only，fsync 一次整 32KB（或更大 batch）；
- **多 WAL**：Column Family 可选独立 WAL 文件（避免跨列族 sync 阻塞）；
- **Recovery**：启动时 replay WAL，重建 memtable；通过 seqnum 检测已持久化的 memtable；
- **WAL 回收**：超过 `WAL_ttl_seconds` 或 `WAL_size_limit_mb` 自动删除。

### 7.8 BlobDB（分离大 Value）

`db/blob/`：

要点：
- **WiscKey 风格**：大 value (> `min_blob_size` 默认 4KB) 单独存到 blob 文件，SST 只存 blob handle；
- **Blob File**：单独 append-only 文件，按 blob_handle 顺序写入；
- **GC**：通过 `GetBlobFilesForGC()` 选择冷 blob 文件 + SST 引用情况；
- **Tiered Storage**：blob file 可放 HDD（节省 SSD）。

## 八、索引机制

- **主索引**：SST 的 Block-based Index（`table/block_based_table_reader.h`）；
- **分区索引**（Partitioned Index）：v6.6+ 默认开启，将 index block 二级切分，加速点查；
- **Bloom Filter**：加速"key 不存在"的快速判断；
- **前缀哈希（prefix hash）**：针对固定前缀扫描优化；
- **二级缓存**（Secondary Cache）：压缩块的冷数据缓存；
- **没有原生二级索引**：由上层（TiKV / MyRocks）实现二级索引。

## 九、查询/计算/接口层

- **API 风格**：C++ 嵌入式 API（`DB::Put/Get/Delete` + Iterator）；
- **语言绑定**：C（`rocksdb/c.h`）、Java（`org.rocksdb`）、Go（`gorocksdb`）、Python（`python-rocksdb`）、Rust（`rust-rocksdb`）；
- **执行模型**：直接方法调用，无 SQL；事务通过 `TransactionDB`（乐观 + pessimistic）；
- **批量写**：`WriteBatch`（atomic write 多条记录）；
- **迭代器**：`DB::NewIterator()` 支持前缀扫描、范围扫描、reverse 迭代；
- **Backup**：API 层提供 `BackupEngine`（增量 + 全量 + 校验）。

## 十、事务与一致性（KV 类型简化版）

- **WriteBatch 原子性**：单 `Write()` 调用内的多条 Put/Delete 原子应用，崩溃后能完整恢复；
- **乐观事务**（`TransactionDB` + `OptimisticTransactionDB`）：
  - 基于 seqnum 的 MVCC；
  - 提交时检查冲突（commit-time conflict check）；
  - 适合冲突低的场景（绝大多数）；
- **悲观事务**（`TransactionDB` + `PessimisticTransactionDB`，v5+）：
  - 基于 lock manager（`db/lock_manager`）；
  - 死锁检测（`transaction_lock_timeout` + `DeadlockDetector`）；
  - `lock_manager` 与 lock column family 可分离；
- **隔离级别**：Snapshot Isolation（SI）默认；可配 `ReadCommitted`（更宽松）；
- **持久化一致性**：`WriteOptions::sync=true` + WAL 强同步；`disableWAL=true` 关 WAL（仅缓存用）；
- **WriteConflict**：返回 `Status::Busy` 表示冲突，需上层重试。

## 十一、复制与高可用

RocksDB **本身不内置复制**，但提供**插件式 API**：

- **Single instance**：单库运行（最常见）；
- **Replication Layer**：上层（MyRocks、TiKV、CockroachDB）实现 Raft / Paxos 复制；
- **Backup & Restore**：`BackupEngine` 提供增量/全量/校验；
- **Import / Export**：`IngestExternalFile()` 用于 SST 文件导入（数据迁移）；
- **跨实例复制**：由上层实现（如 MySQL binlog 复制 RocksDB change stream）。

## 十二、扩展性

- **多列族**（Column Family）：单 DB 多命名空间，独立 compaction / memtable 配置；
- **横向扩展**：上层（TiKV）按 Region 分片；RocksDB 实例本身不分片；
- **垂直扩展**：单实例可配数百 GB 内存（Block Cache + MemTable + MemTable Arena）；
- **动态参数调整**：`SetOptions()` 运行时改 compaction / cache / memtable 配置；
- **TTL**：列族级 `ttl` 自动过期清理；
- **冷热分层**：BlobDB 热数据走 SSD，冷 blob 走 HDD。

## 十三、性能与基准

- **官方基准**（`rocksdb/benchmarks/`）：micro-bench（fillrandom、readrandom、readrandomwriterandom）；
- **db_bench**：标准测试工具，支持 `fillseq` / `fillrandom` / `readseq` / `readrandom` / `overwrite` / `merge` / `delete` 等；
- **YCSB**（Yahoo! Cloud Serving Benchmark）：在 RocksDB 上表现良好（read-heavy + write-heavy）；
- **关键指标**：
  - **写放大（Write Amplification）**：默认 ~4-10 倍（取决于压缩策略）；
  - **读放大（Read Amplification）**：~每层 1 次 IO（L1~L7），加 Bloom ~1 次；
  - **空间放大（Space Amplification）**：~10-30%（压缩 + 待压缩文件）；
- **典型数字**：随机写 ~ 100K op/s（SSD 单盘），点查 ~ 200K op/s（Block Cache 命中）。

## 十四、运维与可观测性

- **监控指标**：`GetStats()`、`DB::GetProperty()` 提供丰富指标；
- **Histogram**：`rocksdb::HistogramType` 跟踪操作延迟分位数；
- **日志**：`Options::info_log` 可注入自定义 logger（`InfoLogLevel` 控制）；
- **Slow Log**：通过 `lldb_stats` 或上层收集；
- **Backup 验证**：`BackupEngine::VerifyBackup()` 校验 checksum；
- **Prometheus 集成**：第三方 Exporter（如 `prometheus-rocksdb-exporter`）；
- **tracing**：`PerfContext` + `IOTrace` 提供 IO 路径追踪。

## 十五、生态与集成

- **客户端 SDK**：C（c.h）、C++、Java（`org.rocksdb`）、Go（`gorocksdb`）、Python（`python-rocksdb`）、Rust（`rust-rocksdb`）；
- **ORM**：通过上层 SQL 引擎（TiDB、MySQL）；
- **消息队列**：Apache Pulsar（BookKeeper 索引）、Kafka Stream State Store；
- **容器化**：官方 Docker 镜像（`embedded-rocksdb`、`rocksdb-tools`）；
- **BI 工具**：无（嵌入式引擎，无 SQL 接口）；
- **存储服务**：TiKV、MyRocks、YugabyteDB（早期）、Ceramic、Flink State Backend。

## 十六、变体与衍生项目

- **LevelDB**：RocksDB 的父项目（轻量、纯 C++11、单线程 compaction）；
- **Pebble**：CockroachDB fork 后独立发展，模块化、Go 重写；
- **Badger**：dgraph 出品的 Go 实现，WiscKey 风格分离 KV；
- **TerarkDB**：商业 fork，专门优化压缩与迭代（已被字节跳动收购）；
- **Speedb**：开源 fork，针对 SSD + KV 优化；
- **librocksdb-sys**：Rust 绑定；
- **TiKV**：PingCAP 出品，以 RocksDB 为底层 + Raft 共识；
- **MyRocks**：Facebook 出品的 MySQL 引擎插件（替代 InnoDB）。

## 十七、核心论文与算法原理

- **The Log-Structured Merge-Tree (LSM-Tree)**（O'Neil et al., 1996）：LSM 原始论文；
- **Bigtable: A Distributed Storage System for Structured Data**（Google, 2006）：LSM 在工业界的代表；
- **WiscKey: Separating Keys from Values in SSD-conscious Storage**（Wuhan Univ. + Facebook, 2017）：WiscKey 风格 KV 分离；
- **Ribbon Filter**（Google: 2020 v7+ 替代 bloom）；
- **Dostoevsky: LSM 的改进**（Facebook 工程博客，2018）；
- **RocksDB 架构演进**（Dhruba Borthakur, Facebook Engineering Blog 2013-2024 持续更新）。

## 十八、源码学习路径

- **入门（1 周）**：
  - 阅读 `include/rocksdb/options.h` 与 `db/db_options.h`：所有可配参数；
  - 阅读 `db/memtable.h` + `memtable/skiplist.h`：跳表实现；
  - 阅读 `db/version_set.h`：版本管理与多层 SST 元数据；
  - 阅读 `db/db_impl/db_impl_write.cc`：写路径（含 WAL）；
  - 阅读 `db/db_impl/db_impl_readonly.cc`：读路径。
- **进阶（2-3 周）**：
  - 阅读 `table/block_based_table_builder.h` 与 `block_based_table_reader.cc`：SST 构造与查询；
  - 阅读 `db/compaction/compaction_job.cc`：压缩核心；
  - 阅读 `db/log_writer.h` + `db/log_reader.h`：WAL 格式；
  - 阅读 `util/bloom.cc`：Bloom Filter；
  - 阅读 `cache/lru_cache.h`：Block Cache 实现。
- **高级（持续）**：
  - 阅读 `db/blob/`：BlobDB 分离 KV；
  - 阅读 `db/transaction/`：事务实现（乐观 + 悲观）；
  - 阅读 `db/range_tombstone_*.cc`：区间删除；
  - 阅读 `db/secondary_cache_adapter.h`：二级缓存；
  - 阅读 `db/wal_manager.cc` + `db/recovery.cc`：WAL 与崩溃恢复。

### 18.1 关键源码路径速查

| 模块 | 路径 |
|------|------|
| DB 主入口 | `db/db_impl/db_impl.h` |
| 写路径 | `db/db_impl/db_impl_write.cc:Put()` |
| 读路径 | `db/db_impl/db_impl_readonly.cc:Get()` |
| MemTable | `db/memtable.cc` + `memtable/skiplist.h` |
| 跳表 | `memtable/skiplist.h` |
| SST Builder | `table/block_based_table_builder.h` |
| SST Reader | `table/block_based_table_reader.cc` |
| Compaction | `db/compaction/compaction_job.cc` |
| 压缩挑选 | `db/compaction/compaction_picker.cc` |
| WAL Writer | `db/log_writer.h` |
| WAL Reader | `db/log_reader.h` |
| Bloom | `util/bloom.cc` |
| LRU Cache | `cache/lru_cache.cc` |
| Column Family | `db/column_family.h` |
| VersionSet | `db/version_set.h` |
| BlobDB | `db/blob/` |
| 事务 | `db/transaction/` |

## 二十、源码规模与开发语言

- **LOC 数量**：约 35 万行 C++（核心 `db/` + `table/` + `memtable/` + `util/` + `tools/`）+ 大量测试 C++；
- **主要语言**：C++17（核心，部分 C++20 计划）、C（`c.h`）、Bash（基准脚本）；
- **构建系统**：CMake（`CMakeLists.txt`）+ 内部 `build_tools/`；
- **测试**：C++ Google Test + 自有集成测试框架；
- **CI**：Facebook 内部 CI + GitHub Actions（PR 触发）；多架构编译（Linux x86_64/aarch64/ppc64le、macOS）。

## 二十一、学习资源

- **官方文档**：[https://github.com/facebook/rocksdb/wiki](https://github.com/facebook/rocksdb/wiki)；
- **官方博客**：Facebook Engineering Blog（搜索 "RocksDB"）；
- **论文参考**：LSM 原始论文、WiscKey 论文、Dostoevsky 论文（搜索 arXiv）；
- **书籍**：《RocksDB 调优实战》（社区）、RocksDB Tuning Guide（官方 Wiki）；
- **YouTube**：RocksDB Summit 历届演讲、CMU Database Group 演讲；
- **会议**：SIGMOD、VLDB、OSDI（LSM 相关论文）、RocksDB Summit；
- **Reddit / HN**：r/databases、Hacker News "RocksDB" 话题。

## 二十二、可移植性与部署

- **支持平台**：Linux（x86_64 / aarch64 / ppc64le）、macOS、Windows（实验性）、BSD；
- **编译器**：GCC 7+、Clang 6+、MSVC 2019+；
- **依赖**：`libpthread`、`libz`、`libsnappy`（默认压缩）、`liblz4`、`libzstd`（可选）；
- **容器化**：官方 Docker（`rocksdb/rocksdb`）、上层（TiKV）Helm Chart；
- **嵌入式部署**：直接静态/动态链接到 C++ 应用；不需独立服务进程；
- **云原生**：通过上层 TiKV / CockroachDB Operator 部署。

## 二十三、风险与陷阱

- **写放大（Write Amplification）**：LSM 天然劣势；写密集场景考虑 Universal Compaction 或 WiscKey 风格；
- **读放大（Read Amplification）**：点查需要遍历多层（Bloom + Index + Data），Compaction 不足时加剧；
- **空间放大（Space Amplification）**：未压缩文件占额外空间；`disable_auto_compactions` 错误开启会无限增长；
- **Stall**：Compaction 速度跟不上写入时，前端 `write_stall` 触发；监控 `rocksdb_write_stall`；
- **大 value**：单 value > 100KB 时建议用 BlobDB；否则压缩与 IO 不友好；
- **Bloom Filter 误判**：默认 10 bits/key，假阳率 ~1%，但 Cold Storage 或大 key space 可能更高；
- **WAL 丢失**：未 `sync=true` 的写入在崩溃后丢失；事务一致性需 `sync=true`；
- **Range Tombstone**：区间删除实现较复杂，可能导致 point lookup miss；
- **Iterator 持有**：Iterator 引用 SST 文件版本，持有时间过长会阻止文件回收（`max_manifest_file_size`）；
- **IO 抖动**：`rate_limiter` 不合理配置可能造成压垮 SSD 或利用率不足；
- **慢查询**：Range Scan（特别是 cold SST）可能秒级；monitor 没有 SQL 入口，纯 API 调试需自建。

## 二十四、与本项目的关联

- **`engineering/` 中可能的对应实现**：
  - `engineering/src/db/` 中如需嵌入 LSM 引擎，RocksDB 是首选参考；
  - `engineering/src/algo/` 中可借鉴跳表（skiplist）实现；
  - `engineering/include/db/` 中 KV 引擎可参考 MemTable + SST 设计；
- **学习复用价值**：
  - SST 格式设计（Block-based + Bloom + Index）可移植到 `learning/ds-c/`；
  - Compaction 算法（Level + Universal）作为教学案例；
  - WAL + MemTable 的写路径（log-then-memtable）可作为 WAL 学习材料；
  - 多列族抽象（独立 compaction / memtable）设计值得借鉴。

## 二十六、Compaction 调度算法：score 判定、输入选取与写放大控制

§7.4 只给了 Level / Universal 的定性结论和一行 score 公式，本节补上 score 的精确计算位置、动态层容量、picker 分派、overlap 扩张伪代码、trivial move 与 subcompaction 的完整判定式。

源码：`db/version_set.cc`（approx，模块未 init，行号需校对）`VersionStorageInfo::ComputeCompactionScore`、`VersionSet::PickCompaction`、`VersionSet::AppendVersion`；`db/compaction/compaction_picker.cc`（approx）`CompactionPicker::PickCompaction`、`CompactionPicker::GetOverlappingInputs`、`CompactionPicker::CalculateBaseBytes`；`db/compaction/compaction_picker_level.cc`（approx）`LevelCompactionPicker::PickCompaction`、`LevelCompactionBuilder::PickFileToCompact`、`LevelCompactionBuilder::GetCompaction`；`db/column_family.cc`（approx）`ColumnFamilyData::NeedsCompaction`。

### 26.1 每层字节预算：10 倍几何级数与动态层基数

```cpp
// 伪代码（Compaction::MaxBytesForLevel 的语义；写入路径见
//        db/mutable_cf_options.cc（approx）MutableCFOptions::RefreshMaxBytesForLevel）
uint64_t MaxBytesForLevel(int level) {
    if (level == 0) return max_bytes_for_level_base_;      // 动态模式下为 base_bytes × 10^base_level
    return static_cast<uint64_t>(multiplier_ * MaxBytesForLevel(level - 1));
}
```

`multiplier_ = max_bytes_for_level_multiplier`，默认 **10**；`max_bytes_for_level_base` 默认 **256MB**。等比数列：

```
L1   256 MB
L2    2.5 GB     = 256 MB × 10
L3     25 GB
L4    250 GB
L5    2.5 TB
L6     25 TB
L7    250 TB     （top level，实际装不满）
```

**动态层基数**（`level_compaction_dynamic_level_bytes`，默认 true）：基数不是写死的 1，而是随库大小前移。`RefreshMaxBytesForLevel` 的等价简化为：

```
base_level = clamp(1 + floor(log10(total_size / max_bytes_for_level_base)), 1, num_levels - 1)

base_bytes = max( total_size / 10^(num_levels - 1 - base_level),
                  max_bytes_for_level_base, 1MB )

L0                        = max(write_buffer_size, base_bytes)
L_k (1 ≤ k < base_level)  = max_bytes_for_level_base
L_(base_level+i)          = base_bytes × 10^i
```

设计意图：**数据量小时层数少**（每层容量大、compaction 少、IOPS 省），**数据量大时层数自动铺开**（读路径只需下探少数几层，读放大省）。所以"层容量倍率 10×"不是硬编码的等比数列，而是**乘在一个会移动的基数上的等比数列**。

**复杂度**：每次 `AppendVersion` 重算一次，代价 `O(num_levels)`（≤ 7），可忽略；真正贵的是随后的 overlap 扩张。

### 26.2 Score 判定式与 picker 分派

```cpp
// 伪代码（VersionStorageInfo::ComputeCompactionScore）
for (int level = 0; level < num_levels; ++level) {
    if (level == 0) {
        // L0 用「文件数」而不是字节数打分 —— 因为 L0 文件是重叠的，字节数会失真
        score_[0] = num_sorted_runs(0) / level0_file_num_compaction_trigger;      // 默认 4
        if (compaction_style == kLevel) {
            // IO 密集场景下 L0 可能瞬时冲破 L1 的字节预算，补一刀字节维度
            score_[0] = max(score_[0], total_size(0) / max_bytes_for_level_base);
        }
    } else {
        score_[level] = total_size(level) / max_bytes_for_level(level);
    }
}
```

`score >= 1.0` 即"这一层欠债"。完整调用链：

```
VersionSet::AppendVersion
  └─ ComputeCompactionScore            → 写入 compaction_score_[level]
      └─ ColumnFamilyData::NeedsCompaction()  → 为真才调度
          └─ DBImpl::BackgroundCompaction / MaybeScheduleFlushOrCompaction
              └─ cfh->compaction_picker_->PickCompaction(cfh, mutable_cf_options, log_context)
```

三套 picker 由 `CompactionStyle` 决定（`ColumnFamilyData::compaction_picker_`）：

| style | picker | 源码（canonical） |
|-------|--------|------------------|
| `kLevel` / `kLocalLevel` | `LevelCompactionPicker` | `db/compaction/compaction_picker_level.cc` |
| `kUniversal` | `UniversalCompactionPicker` | `db/compaction/compaction_picker_universal.cc` |
| `kFIFO` | `FIFOCompactionPicker` | `db/compaction/compaction_picker_fifo.cc` |

`LevelCompactionBuilder::GetCompaction` 的扫描顺序是**先扫 base level 之上所有 score ≥ 1 的层，再扫 base level 本身**，因此数据量小时 compaction 自然发生在浅层。`CompactAtLevel(level)` / `CompactWithoutInstallation` 是 manual compaction 与 L0 特例走的旁路入口。

**三个 L0 数字别搞混**（这是最常见的调参误解）：

```
level0_file_num_compaction_trigger = 4    → "开始排队 L0→L1"，压缩侧
level0_slowdown_writes_trigger     = 20   → "写降速"，写侧（§27）
level0_stop_writes_trigger         = 36   → "写停止"，写侧（§27）
```

三者都是 `MutableCFOptions` 字段，可用 `SetOptions()` 在线调整。

### 26.3 输入文件选取：score 选种子 + overlap 扩张

```cpp
// 伪代码（CompactionPicker::GetOverlappingInputs 的核心扩张逻辑）
level = inputs[0].level;                              // 种子文件所在层
seed  = level_files[level] 中 score 最高的那个          // LevelCompactionBuilder::PickFileToCompact
inputs[0].files.push(seed);

// L0 特例：种子可以是「一个」文件，也可以是所有 score 最高的一批文件
if (level == 0 && num_l0_files >= trigger) {
    // kMaxExpandedBytesPerFile / expanded_compaction_byte_size_limit 决定能吞多少
}

if (allow_trivial_move) {                              // 默认 true
    if (下层与 seed 的 key range 重叠字节 < max_grand_parent_overlap_bytes) {
        trivial_move = true;                           // 直接 move，不做归并
        return;
    }
}

// 向下逐层扩张：吸收所有与「已选 key range 之并」相交的文件
range = seed 的 [smallest_key, largest_key]
for (lv = level + 1; lv < num_levels; ++lv) {
    overlaps = { f ∈ level_files[lv] : f 的 range 与 range 相交 }
    if (overlaps.empty()) break;                       // 无重叠则该层不参与
    inputs.push(lv, overlaps)
    range = range ⊕ overlaps 的包围区间                  // 下一层的扩张基准
    if (本层扩张总量 > expanded_compaction_byte_size_limit) break;   // 防止一次吞掉半个 DB
}
```

**两条上限约束**：

1. **扩张上限（`expanded_compaction_byte_size_limit`）**：即使下层重叠文件很巨大，也最多吸收有限量级的输入。单次 compaction 过大 → W 放大与 stall 同时恶化，且失去「小步快跑」的并行度收益。
2. **祖父层重叠上限（`max_grand_parent_overlap_bytes`，默认 = `10 × target_file_size_base` = 640MB）**：种子文件与**祖父层**（level+2）的重叠字节上限。这是 LevelDB 时代的老规矩，作用是限制"输出文件被祖父层切碎"导致的读放大，RocksDB 保留它作为折中。

**Trivial move 的完整判定**（`Compaction::IsTrivialMove`）：

```
IsTrivialMove :=
      output_level_ > 0
  ∧   inputs_.size() == num_input_levels_        // 每层都"只挑到 1 个文件"
  ∧   ( level == 0 ?  seed 与 output 层完全不重叠
                  :  seed 在 output 层无重叠
                  ∧  文件体积仍在 target_file_size_base 量级 )

收益：0 次读、0 次写、0 次解压，只改 manifest → 复杂度 O(log F) + 一次 rename 系统调用
代价：只是推迟膨胀，并不能阻止；所以老 key 会持续向右扩张，最终仍需一次真归并
```

**复杂度小结**：

| 操作 | 复杂度 | 说明 |
|------|--------|------|
| `ComputeCompactionScore` | `O(num_levels)` | 每次 AppendVersion |
| `RefreshMaxBytesForLevel` | `O(num_levels)` | 动态层基数重算 |
| overlap 扩张（单层 F 个文件） | `O(F · log F)` | `InternalKeyComparator` 二分 + 区间扫描 |
| trivial move | `O(log F) + rename` | 只改元数据 |
| 真实 k 路归并 | `O(N · log k)` | N = 输入总条目数，k = 归并路数 |
| Universal 的 score | `O(S)` | S = sorted run 数，见 26.5 |

### 26.4 Subcompaction：把一次大归并拆成并行任务

```cpp
// 伪代码（db/compaction/compaction_job.cc（approx）CompactionJob 的 subcompaction 切分）
// 单个输出文件的大小上限（MaxFileSizeForLevel）：
target = kTargetFileSizeBase × kTargetFileSizeMultiplier^output_level    // 默认 64 MB × 1^n
// 切分预算：
budget = max(max_subcompaction_bytes, target × subcompaction_max_output_file_size)

// 判定式：只有输出能切成足够多份，才值得付并行开销
num_subcompactions = ceil(estimated_output_bytes / budget)
if (num_subcompactions <= 1 || max_subcompactions == 1)
    return 单线程归并;

// 否则：先按 key range 切成 num_subcompactions 个**互不重叠**的区间 [start, end)
for (i = 0; i < num_subcompactions; i++)
    SubcompactionState{ start = range[i], end = range[i+1] }.Schedule();
```

**为什么必须先切区间、再并行**：归并指针（多路归并的堆顶）无法分头推进，多个线程若共享同一段输入流只能串行。切成互不重叠的 `[start_key, end_key)` 后，每个 `SubcompactionState` 只处理自己的区间，**输出文件按 key range 天然有序且互不重叠**，可以直接拼进 `Version`。

`max_subcompactions` 缺省为 1（关闭）。开启后的收益上限是存储设备的并发 IO 度数（NVMe 队列深度），代价是每路都要独立的迭代器、compaction filter 与 merge 状态机，CPU 与内存上升。**SSD 上 `max_subcompactions` 与 `max_background_jobs` 是补偿写延迟的一对主要旋钮**，§二十三 里"IO 抖动"的常见解法就是这两项。

### 26.5 Universal Compaction：按 sorted run 比值打分

Universal 模式下不存在"L(n) → L(n+1)"的层级关系，整库被组织成若干**有序 run（sorted run）**，`UniversalCompactionPicker` 的打分改为**相邻 run 的体积比**：

```
Universal score = max over i in [1, S) of ( size(run_i) / size(run_(i-1)) )
再除以 CompactionOptionsUniversal::size_ratio   （默认 1，即比值 > 1 就要压）
```

其中 `S` 是非空 sorted run 数。语义是：**某个 run 比它的前驱大 size_ratio 倍以上，说明后继被反复覆盖写入、已严重碎片化，该合并了**。

配套参数（`CompactionOptionsUniversal`，源码 `include/rocksdb/options.h`）：

```
size_ratio                    默认 1     体积比阈值
min_merge_width               默认 2     一轮最少合并几个 run（太小则写放大失控）
max_merge_width               默认 32    一轮最多合并几个 run（太大则读放大失控）
max_size_amplification_percent 默认 100   相对总数据量的最大空间放大上限
```

`min_merge_width` / `max_merge_width` 一起解释了 Universal 的核心权衡：

```
空间放大 ≈ T / W_max   （T = 需要合并的 run 数，W = 合并宽度）
读放大   ≈ S
约束：T ≤ min_merge_width 时压不住 → 空间放大抬升
约束：T ≥ max_merge_width 时 → 必须整批压 → 读放大 = S

默认 2 / 32 → 空间放大 ≈ 2-4×，读放大 ≈ S（无层级收敛）
```

§7.4 说 Universal"整体读放大小（按文件数级别）但写放大大"——**准确的说法是写放大小、读放大小**，因为 run 被压成 1 个，读反而更便宜；它牺牲的是**空间放大**（叠加容忍窗口 `W_max` 个 run 的旧版本）。因此 Universal 适合**写入覆盖率高、有明确 flush 边界**的负载（时序、指标），不适合读多写少、key 空间无限增长的通用负载。

---

## 二十七、Write Stall 写阻塞：三级降级判定与延迟写

§二十三 只把 Stall 列为"风险"，§7.4 也没写触发之后的行为。本节补上五个阈值、两条判定轴、`ShouldDelayWrite` 的判定顺序与两阶段延迟写循环。

源码：`db/db_impl/db_impl_write.cc`（approx，模块未 init，行号需校对）`DBImpl::WriteImpl`（含 `max_pending_delay` 短轮询循环与 `max_write_delay` 长等待）；`db/db_impl/db_impl.cc`（approx）`DBImpl::ShouldDelayWrite`、`DBImpl::BackgroundCompaction`、`DBImpl::BackgroundCallFlush`、`DBImpl::BackgroundCallCompaction`、`DBImpl::MaybeScheduleFlushOrCompaction`、`DBImpl::WriteBufferManager`（内存水位）；参数定义 `include/rocksdb/advanced_options.h`（approx）`level0_slowdown_writes_trigger`、`level0_stop_writes_trigger`、`soft_pending_compaction_bytes_limit`、`hard_pending_compaction_bytes_limit`、`delayed_write_rate`。

### 27.1 三条判定轴与五个阈值

```
轴 1 —— L0 文件数（N₀）
    N₀ <  level0_file_num_compaction_trigger (=4)    → 正常
    N₀ ≥  level0_slowdown_writes_trigger     (=20)   → 降速（注意：不是阻塞）
    N₀ ≥  level0_stop_writes_trigger         (=36)   → 停止写入

轴 2 —— 待压缩字节（P，待参与 compaction 的输入总字节）
    P >  soft_pending_compaction_bytes_limit (=64 GB)   → 降速
    P >  hard_pending_compaction_bytes_limit (=256 GB)  → 停止写入

轴 3 —— memtable 堆积
    #immem ≥ max_write_buffer_number (=2)              → 停止写入
    总内存 > max_total_in_memory_size                  → 停止写入
```

三条轴**取最严**，任一轴触顶就阻塞。值得注意的一个事实：默认参数下 `max_bytes_for_level_base = 256 MB`、`target_file_size_base = 64 MB`，单次 L0→L1 的输入量通常在 1-2 GB 量级，因此

```
轴 1 需要 N₀ ≈ 20 个 L0 文件 ≈ 数 GB 未压缩数据才降速
轴 2 需要 P  > 64 GB 才降速

⇒ 实践中 level0_slowdown_writes_trigger (=20) 是先咬人的那个；
  轴 2 / 轴 3 是针对"单文件极大"或"压缩带宽崩溃"的安全网。
```

调参时的经验法则：**调 L0 阈值是治标（用读放大换写延迟），调 `max_background_jobs` / `max_subcompactions` 是治本**（把压缩吞吐提上来，让 N₀ 根本涨不上去）。

### 27.2 降速 vs 停止：两个动作、两套代价

| | 降速（slowdown） | 停止（stop / stall） |
|---|---|---|
| 动作 | 写线程以 1 ms 粒度轮询 `ShouldDelayWrite()`，满足则 sleep | 写线程进入两阶段延迟循环并最终返回 `Status::TimedOut()` |
| 速率控制 | 受 `delayed_write_rate` 约束（默认 16 MB/s 令牌桶） | 无速率，纯挂起 |
| 观测指标 | `RATE_LIMITER` / `STALL_MICROS` 计数上升 | `Stalls(count)` 中对应项上升 |
| 解除条件 | `ShouldDelayWrite()` 返回 false（压缩追平） | 同左；硬限优先级高于软限 |

### 27.3 `ShouldDelayWrite()` 的判定顺序

判定顺序即源码顺序，**先低成本后高成本、软限让位于硬限**：

```cpp
// 伪代码（DBImpl::ShouldDelayWrite 的判定结构）
bool DBImpl::ShouldDelayWrite() {
    const uint64_t prewritten_bytes = GetWALPreallocatedBlocksBytes();
    if (prewritten_bytes + write_queue_size_ > 写队列字节上限) return true;   // 队列爆了

    // 剩余可写预算 = 允许的 WAL 预写字节 − 已预写字节
    size_t allowed_delay = write_delay_max_ - prewritten_bytes;

    // ---- 轴 3：memtable ----
    if (immem_mems_.size() >= mutable_cf_options.max_write_buffer_number) return true;

    // ---- 轴 2：待压缩字节 ----
    P = 估算待压缩输入总字节;
    if (P > hard_pending_compaction_bytes_limit)   allowed_delay = 0;      // 硬限：直接停
    else if (P > soft_pending_compaction_bytes_limit) allowed_delay = min(allowed_delay, delayed_write_rate);

    // ---- 轴 1：L0 文件数 ----
    N0 = GetNumFilesAtLevel0();
    if (N0 >= level0_stop_writes_trigger)            allowed_delay = 0;      // 硬限：直接停
    else if (N0 >= level0_slowdown_writes_trigger)    allowed_delay = min(allowed_delay, delayed_write_rate);

    // 累计的"欠账"超过剩余预算 → 阻塞
    if (allowed_delay < prewritten_bytes) return true;
    write_delay_active_ = false;
    return false;
}
```

**关键不变量**：`allowed_delay` 是三轴的**取最小值**结果——任一轴进入硬限就把它压到 0（等价于"不许再写"），任一轴进入软限就把它压到 `delayed_write_rate`（等价于"按令牌桶限速"）。这也解释了为什么把三个 soft 阈值调得再高，也不会绕过另一个轴的硬限。

### 27.4 `WriteImpl` 的两阶段延迟写循环

```cpp
// 伪代码（DBImpl::WriteImpl 中 stall 处理的主循环结构）
// 阶段一：短轮询，给后台 compaction 一个「快速解除」的机会
for (int64_t i = 0; i < max_pending_delay && write_queue_ == WRITE_QUEUE_NORMAL; ++i) {
    if (ShouldDelayWrite()) {
        记录 stall 微秒数;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));    // 1 ms 粒度
    } else {
        break;                                                        // 压缩追平 → 立刻返回
    }
}

if (i == max_pending_delay) {
    // 阶段二：关闭写队列，唤醒 flush / compaction 后台线程重新调度
    MaybeScheduleFlushOrCompaction();

    // 阶段三：长等待，等后台真正把 L0 压下去
    while (i++ < max_write_delay && write_queue_ == WRITE_QUEUE_NORMAL && ShouldDelayWrite()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    if (i >= max_write_delay) {
        // 真的 stall：写请求失败而不是无限挂起
        return Status::TimedOut("Stalling writes because ...");
    }
}
```

**为什么必须分两阶段**：若只用一次长 sleep，压缩在 sleep 中途追平了，写线程仍要等到 sleep 结束才恢复 —— 写延迟被**拉平成固定值**，P99 完全由 sleep 粒度决定。短轮询阶段让"压缩一追平、写立即恢复"成为可能，长等待阶段则保证后台线程在被 stall 堵死的关键窗口里仍能被唤醒并调度 compaction。两者的折中由 `max_pending_delay` 与 `max_write_delay`（毫秒级）控制。

### 27.5 观测与定位

`GetProperty("rocksdb.stats")` 的 `Stalls(count)` 字段是定位的唯一可靠入口，它按轴归因：

```
Stalls(count)
  ├ level0_numfiles            ← 命中 level0_slowdown_writes_trigger
  ├ pending_compaction_bytes   ← 命中 soft/hard_pending_compaction_bytes_limit
  ├ memtable_stop              ← 命中 max_write_buffer_number / 内存水位
  └ write_buffer / write_queue ← WAL 队列或队列字节上限
```

定位到具体轴后，解法完全不同：

- `level0_numfiles` 占主导 → L0 fan-in 问题，提 `max_background_jobs` + `max_subcompactions` + `max_bytes_for_level_base`
- `pending_compaction_bytes` 占主导 → 压缩带宽问题，提 IO 能力 / 降 `compression` 等级 / 改用 Universal
- `memtable_stop` 占主导 → 写入带宽远超 flush 能力，提 `max_background_jobs` / 降 `write_buffer_size`

### 27.6 FIFO Compaction：用「直接丢文件」绕开归并

当负载是"只写 + 定期过期"（时序保留窗口），归并本身就是浪费。`CompactionStyle::kFIFO` 的 picker（`db/compaction/compaction_picker_fifo.cc`，approx）`FIFOCompactionPicker::PickCompaction` 不做任何归并：

```cpp
// 伪代码（FIFOCompactionPicker::PickCompaction 的判定）
if (total_size < mutable_cf_options.fifo_compaction_trigger) return nullptr;   // 默认 1 GB
if (head 文件仍被活跃 Iterator / Snapshot 引用)             return nullptr;   // 不可回收
// FIFO_delete_policy: kOldest（默认）/ kNewest / kTime
return 丢弃队首文件;      // 整个文件连带其中的所有 key 一起消失
```

**适用边界**（这一条常被误用）：

- FIFO 的语义是**丢弃最旧文件里的全部数据**，它不是通用删除机制；对同一 key 的乱序覆盖写会产生"旧文件已删、新文件里是历史版本"之外的不一致语义，故**要求写入近似单调**。
- `FIFO_delete_policy = kTime` 时按 mtime 删，同样要求写入严格时间单调。
- 好处是写放大约为 1（没有归并）、无需任何 merge 状态机；代价是读放大随保留 run 数线性增长。

---

## 二十八、InternalKey 编码、MVCC 可见性与 compaction 丢键判定

§7.2 只说了"`Add(SequenceNumber, ValueType, key, value)` 四参数"，§十 只说了"基于 seqnum 的 MVCC"。本节补上 InternalKey 字节布局、排序不变量、快照可见性判定、compaction 丢键的三条规则，以及 WriteBatch 的 seq 分配与原子性。

源码：`db/dbformat.h` / `db/dbformat.cc`（approx，模块未 init，行号需校对）`InternalKey`、`ParsedInternalKey`、`InternalKey::EncodeTo`、`InternalKeyComparator::Compare`、`ExtractUserKey`、`PackSequenceAndType`、`UnpackSequenceAndType`、`kTypeDeletion`、`kTypeValue`、`kTypeMerge`、`kTypeSingleDeletion`、`kTypeRangeDeletion`、`kTypeColumnFamilyValue`、`kTypeBlobIndex`、`kMaxSequence`；`db/snapshot_impl.h`（approx）`SnapshotImpl::IsVisibleInSnapshot`；`db/memtable.cc`（approx）`MemTable::Add`、`MemTable::Get`；`db/compaction/compaction_iterator.cc`（approx）`CompactionIterator::NextFromInput`、`CompactionIterator::findEarliestVisibleSnapshot`、`CompactionIterator::ikeyNotNeededForIncrementalSnapshot`、`CompactionProxy::KeyNotExistsBeyondOutputLevel`、`MergeHelper::MergeUntil`；`util/coding.h`（approx）`EncodeFixed64` / `DecodeFixed64`。

### 28.1 InternalKey 字节布局与排序不变量

```
磁盘 / memtable 中的一条记录：

  ┌──────────────────────┬────────────────────────────┐
  │  user_key (变长)      │  packed_seq_and_type (8B)  │
  └──────────────────────┴────────────────────────────┘

  packed = (seq << 8) | type        （低 8 位放 type，高 56 位放 seq）
  以 64 位**小端**写入（util/coding.h 的 EncodeFixed64）

  type 取值（部分，db/dbformat.h）：
    kTypeDeletion          = 0x0    0x1 kTypeValue
    kTypeMerge             = 0x2    0x4 kTypeColumnFamilyDeletion
    kTypeColumnFamilyValue = 0x5    0x6 kTypeColumnFamilyMerge
    kTypeSingleDeletion    = 0x7    0xF kTypeRangeDeletion
    kTypeBlobIndex         = 0x11   0x16 kTypeWideColumnEntity
```

由于 type 占掉低 8 位，`db/dbformat.h` 里 seq 的上限被定义为 `kMaxSequence = (1ull << 56) - 1`——**这不是巧合，而是编码布局的直接后果**。

比较器 `InternalKeyComparator::Compare` 的**排序不变量**：

```
若 user_key(a) ≠ user_key(b)  →  只比较 user_key（走 user_comparator_：字典序 / 数值序 / 反序皆可）
若 user_key(a) =  user_key(b)  →  比较 packed_seq_and_type 的**逆序**（seq 大者排在前面）

⇒ 同一 user_key 的所有版本在 SST / memtable 里是「最新在最前」连续排列
```

这个"seq 逆序"约定是**读路径与 compaction 丢键判定的共同前提**：读只需 `Seek(user_key)` 一次就命中最新版本；compaction 才能用"扫到同 user_key 簇的第一条即最新"来做整簇丢弃。

### 28.2 快照可见性：一个 O(1) 判定

```cpp
// 伪代码（SnapshotImpl::IsVisibleInSnapshot 的判定结构）
bool IsVisibleInSnapshot(const InternalKey* key) {
    const SequenceNumber seq = key->sequence;
    if (seq == 0) return true;        // seq 0 是 LevelDB 沿用的全局初始版本哨兵，对一切快照可见
    if (seq > number_) return false;  // 快照拍下之后才写入 → 该快照看不见
    return visible_at_tip_ || (current_->number_ <= number_);
}
```

三条分支的语义：

- `seq == 0`：初始空版本，任何快照都要看得见（否则空库会出现"读不到"的情况）。
- `seq > number_`：写在快照之后。
- `visible_at_tip_`：`GetSnapshot()` 拿到的"裸快照"（其 `number_` 等于取快照瞬间的最大 seq），对任何 key 都可见，不需要再比对 Version。
- 否则要求快照所属的 `Version` 尚未被后续 compaction 淘汰——这是**唯一会让快照失效的情形**，也是 `VersionRef` 引用计数的存在理由。

**为什么是 O(1) 而不是回滚日志式的读时过滤**：每个 `Get` 只做一次常量判定 + 一次 `Seek`（`O(log n)`），无需在读路径上遍历多版本。反过来，**代价在写/压缩侧**：compaction 必须为所有活着的快照保留对应版本（下一节规则 3）。

### 28.3 Compaction 丢键的三条规则

```cpp
// 伪代码（CompactionIterator::NextFromInput 的核心分支）
// 前提：输入按 InternalKeyComparator 排序 ⇒ 同 user_key 的版本按 seq 递减排列

// 规则 1：整簇「无任何快照可见」→ 整个 user_key 丢弃
if (last_snapshot == current_user_key_snapshot_) {
    ++iter_stats_.num_record_drop_hidden;          // 该 key 对所有活跃快照都不可见
    input_->Next();
    continue;
}

// 规则 2：tombstone 彻底无用 → 丢弃（写放大削减的主力）
if (compaction_ != nullptr && ikey_.type == kTypeDeletion &&
    IN_EARLIEST_SNAPSHOT(ikey_.sequence) &&       // 低于/等于最早快照 ⇒ 无快照能看见它
    ikeyNotNeededForIncrementalSnapshot() &&      // 增量备份/快照导出也不需要它
    compaction_->KeyNotExistsBeyondOutputLevel(ikey_.user_key, &level_ptrs_)) {
    ++iter_stats_.num_record_drop_obsolete;
    if (!bottommost_level_) ++iter_stats_.num_optimized_del_drop_obsolete;
    input_->Next();                               // 不输出
    continue;
}

// 规则 3：merge operand 累加到快照边界为止
//   MergeHelper::MergeUntil(iter, range_del_agg, stop_before, at_bottom)
//   stop_before = findEarliestVisibleSnapshot() 返回的 prev_snapshot
//   遇到 seq <= stop_before 即停手，保证每个快照仍能看到自己那一版

valid_ = true;                                    // 输出本条
```

三条规则的意图：

- **规则 1**：同 seq 的 value + tombstone 配对（全零 key 的初始写入对）会产生"整簇都不可见"的 key，直接丢掉。
- **规则 2（写放大削减的核心）**：删除标记若已对所有快照不可见、且下层也不会再出现这个 key，它就完全没有作用——写进 SST 只是浪费字节，并让后续每次点查多一次无效 IO。丢弃它直接降低 W 放大的分子。
- **规则 3**：在满足所有快照语义的前提下尽量合并 merge operand，减少输出条目数。

**复杂度**：每条输入的判定是 `O(1)`~`O(快照数)`，整次 compaction 落在 `O(N · log k)`（k = 归并路数），而不是"不丢键"时的 `O(N · 快照数)`。这也是 RocksDB 写放大能压在 4-10× 而非更高的原因之一（呼应 §十三）。

### 28.4 Range Tombstone：区间删除如何影响点查

`kTypeRangeDeletion` 记录一条 `[start_key, end_key)` 的区间删除：

- **写入侧**：`MemTable::Add` 收到该类型时不写普通记录，而是累加到 memtable 内部的区间删除结构；compaction 时通过 range del aggregator（`CompactionIterator` 的 `range_del_agg_`）把来自多层的 tombstone 合并。
- **读侧**：点查拿到一个普通 key 后，除了比 seq 还要确认"是否存在 seq ≥ 该 key.seq 的区间 tombstone 覆盖它"。覆盖判定最终物化为**输出文件内的 fragmented tombstone**（按 `RangeTombstoneFragmenter` 的块大小切片），所以点查多了一次区间判定，iterator 也要在遇到 tombstone 时做 `SetKey`。
- **合并规则**：compaction 输出时，**同 seq 的 range tombstone 必须合并且不允许被切分点劈开**（除非切分点落在相同 key 上），否则 tombstone 数量会随层数翻倍爆炸——这是 §二十三 里"Range Tombstone 实现复杂"的根因。

**使用建议**：range tombstone 适用于**批量清除**（TTL 整段过期、`DeleteRange` 清历史）；少量单点删除应走 `Delete()`。因为 range tombstone 会阻止相关文件的复用与 trivial move，滥用会显著抬高 W 放大。

### 28.5 WriteBatch：原子性在内存中的表示与 seq 分配

```cpp
// 伪代码（include/rocksdb/write_batch.h（approx）WriteBatch 的内部表示）
struct WriteBatch {
    std::string rep_;              // 所有操作记录连续存放于同一块 buffer
    uint32_t    count_;            // 本批条目数
    SequenceNumber last_sequence_; // 本批最后分配的 seq

    Put(k, v)          rep_ += [kTypeValue,          k, v]
    Delete(k)          rep_ += [kTypeDeletion,       k]
    SingleDelete(k)    rep_ += [kTypeSingleDeletion, k]
    DeleteRange(s, e)  rep_ += [kTypeRangeDeletion,  s, e]
    Merge(k, v)        rep_ += [kTypeMerge,          k, v]
    // 事务用的两阶段标记
    MarkBeginPrepareXID(xid) rep_ += [kTypeBeginPrepareXID, xid]
    MarkEndPrepareXID(xid)   rep_ += [kTypeEndPrepareXID,   xid]
    MarkCommit(xid)          rep_ += [kTypeCommitXID,        xid]
    // 每次操作 count_++；整批共用一个 buffer → 一次 Write() 只需一次 WAL 写 + 一次 memcpy
};
```

**seq 的连续分配**：

```
seq_last = version_set.LastSequence()
for (i = 0; i < count_; i++)
    internal_key = [ batch[i].key, seq_last + i, batch[i].type ]
version_set.SetLastSequence(seq_last + count_)      // 一次性推进
```

三条推论：

1. **原子性**：落在 `[seq_last, seq_last + count_)` 这一段连续 seq 的记录，要么全部可见、要么全部不可见，**不存在"半个 WriteBatch 可见"的中间态**——崩溃恢复正是靠这个连续区间判断 memtable 是否已完整落盘（§7.7 的 "通过 seqnum 检测已持久化的 memtable"）。
2. **多列族写入**（`WriteBatchInternal::InsertInto`）会把不同列族的操作分组，按列族各自跳过不相干的 seq 段，但仍共享同一 seq 空间。
3. **seq 是 64 位单调递增**，`kMaxSequence = (1ull << 56) - 1` 作为"无快照约束"的哨兵值；达到上限即写满，几乎不可能触及。

---

## 二十九、参考资料

- [RocksDB 官方 Wiki](https://github.com/facebook/rocksdb/wiki)
- [RocksDB Tuning Guide](https://github.com/facebook/rocksdb/wiki/RocksDB-Tuning-Guide)
- [LSM-Tree 原始论文（O'Neil 1996）](https://www.cs.umb.edu/~poneil/lsmtree.pdf)
- [WiscKey 论文（Xia et al. 2017）](https://www.usenix.org/system/files/conference/fast17/fast17-xia.pdf)
- [Facebook Engineering Blog - RocksDB 系列](https://engineering.fb.com/category/data-infrastructure/)
- [Dostoevsky: LSM-Tree 的改进](https://www.cs.cmu.edu/~xiaohanwu/Slides/Dostoevsky.pdf)
- [RocksDB source code（GitHub）](https://github.com/facebook/rocksdb)
- [TiKV 文档 - RocksDB 集成](https://tikv.org/docs/deep-dive/key-value-engine/)
- [CockroachDB Pebble fork 起源](https://www.cockroachlabs.com/blog/pebble-rocksdb-kv-store/)
- [MyRocks 官方文档](https://github.com/facebook/mysql-8.0)
- [RocksDB Summit 历年演讲](https://www.youtube.com/results?search_query=rocksdb+summit)