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

## 二十五、参考资料

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