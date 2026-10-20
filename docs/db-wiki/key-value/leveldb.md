# LevelDB Wiki（嵌入式 LSM 键值数据库）

> 参考版本：LevelDB 仓库当前 commit `7ee830d02b623e8ffe0b95d59a74db1e58da04c5`（v1.23 系列；LevelDB 自 2017 年起稳定维护，每月小版本更新）。

## 一、项目定位

LevelDB 是 Google 开源的一款**轻量级、嵌入式、持久化的 LSM-Tree 键值数据库**，由 Bigtable 团队核心工程师 Jeffrey Dean、Sanjay Ghemawat 编写。它是 RocksDB 的"祖父项目"，提供了简洁、优雅、可阅读的 LSM 实现，作为学术研究、教学与轻量级持久化场景的首选引擎。

- **痛点**：
  - SQLite 等关系引擎对 KV 场景过重；
  - RocksDB 复杂度过高、配置项过多、依赖较重；
  - 嵌入式设备（如 Chrome 浏览器、Android）需要小巧、零依赖的 KV 引擎；
  - 教学场景需要清晰的 LSM 实现参考；
- **典型用户**：Chrome 浏览器（IndexedDB 后端、cookie、token、本地存储）、Android 系统（部分本地数据库）、Riak KV（早期）；社区派生如 Riak 1.x、HyperDex、Accumulo 部分子项目。

## 二、发展历史与社区

- **起源**：2011 年 Google 发布 LevelDB 1.0，开源（BSD 3-Clause）；设计灵感来自 Bigtable 内部的 Tablet Server 存储格式；
- **里程碑**：
  - 2011 年开源初始版本，作者 Jeffrey Dean / Sanjay Ghemawat；
  - 2013-2014 年社区维护（Google 团队投入有限）；
  - 2014 年起 Facebook fork 出 RocksDB（详见 rocksdb.md）；
  - 2017 年 1.18 引入 WriteBatch 内部并发安全改进；
  - 2019-2020 年 1.22 完善 compaction / iterator；
  - 2023 年 1.23（持续小幅改进）；
- **维护状态**：维护频率较低（每月小幅 PR），但代码稳定；社区 fork 活跃（RocksDB、Pebble 等）；
- **社区**：GitHub Issues、LevelDB Google Group（lleveldb@googlegroups.com）。

## 三、许可与商业限制

- **许可证**：**BSD 3-Clause**（宽松、几乎无限制）；
  - 来源：`leveldb/LICENSE`；
  - 关键约束：保留版权、禁止用贡献者名义背书衍生品；
  - 商业使用、修改、分发、闭源衍生均可；
- **商业版本**：无；LevelDB 是纯社区项目；
- **Google 内部使用**：Bigtable 早期、Spanner 内部部分组件、F1；现在部分场景已切换至 RocksDB。

## 四、业界主要使用方与案例

- **Chrome 浏览器**：IndexedDB、LocalStorage、网络缓存；
- **Android**：早期系统级数据（被 Room/SQLite 取代大部分场景）；
- **Riak KV**：早期 Riak 1.x 后端；
- **HyperLevelDB**：Riak 团队的 fork，加入并发压缩；
- **Accumulo / Cassandra** 部分子项目；
- **教学**：CMU 15-445、Stanford CS346、MIT 6.824 等课程的标准参考实现。

## 五、一句话对比定位

> LevelDB 是**"轻量级、纯 C++11、单线程 compaction 的 LSM 引擎"**；与 **RocksDB（功能增强、SSD 优化、多线程）**、**Pebble（CockroachDB fork、Go 重写）**、**Badger（Go 实现、WiscKey）** 形成**LSM 嵌入式阵营**。LevelDB 因代码简洁常作为教学参考。

## 六、整体架构

LevelDB 沿用 LSM-Tree 经典三层：**MemTable → WAL → SST（多层）**。架构极简，仅约 15 个核心类。

### 6.1 总体分层

```
┌──────────────────────────────────────────────────────────────────────┐
│                   用户 API (include/leveldb/db.h)                     │
│   - class DB  : Open / Put / Get / Delete / Write / NewIterator     │
│   - class Iterator : Seek / Next / Prev / Key / Value / Valid       │
│   - class Snapshot / WriteBatch / ReadOptions / WriteOptions        │
└─────────────────────┬────────────────────────────────────────────────┘
                      │
┌─────────────────────▼────────────────────────────────────────────────┐
│              DBImpl  (db/db_impl.h)                                  │
│   - 主要类，负责协调读写、compaction、recovery                      │
│   - 持有：mem_, imm_, versions_, logfile_, table_cache_             │
│   - 同步锁：BGL 互斥锁保护内部状态                                  │
│   - 同步后台线程：BGThread 调度 compaction                           │
└─────────────────────┬────────────────────────────────────────────────┘
                      │
┌─────────────────────▼────────────────────────────────────────────────┐
│              MemTable (db/memtable.h + skiplist)                      │
│   - 基于跳表 (db/skiplist.h)                                        │
│   - 写满阈值（默认 4MB，可配）→ 冻结为 immut                         │
└─────────────────────┬────────────────────────────────────────────────┘
                      │
┌─────────────────────▼────────────────────────────────────────────────┐
│              Write-Ahead Log (db/log_writer.h + log_reader.h)        │
│   - 32KB block；CRC32 校验；物理 + logical 记录类型                   │
└─────────────────────┬────────────────────────────────────────────────┘
                      │
┌─────────────────────▼────────────────────────────────────────────────┐
│              SST (table/table.cc + format.h + block_builder.h)        │
│   - Block-based Table (data block + meta index + index + footer)     │
│   - Block 压缩（Snappy 默认开启，可关）                              │
│   - Filter Block (可选 Bloom Filter via util/bloom.cc)              │
└─────────────────────┬────────────────────────────────────────────────┘
                      │
┌─────────────────────▼────────────────────────────────────────────────┐
│              VersionSet (db/version_set.h)                            │
│   - 维护所有 level 的 SST 文件集合                                   │
│   - Version + VersionEdit 抽象                                       │
│   - 通过 current_ 原子指针 + ref counting 保证读一致性               │
└─────────────────────┬────────────────────────────────────────────────┘
                      │
┌─────────────────────▼────────────────────────────────────────────────┐
│              Compaction (db/compaction.h + compaction.cc)            │
│   - 单线程 compaction (BGThread)                                    │
│   - Level compaction (Score-based picker)                            │
│   - 不支持 Universal / FIFO 等其他策略                               │
└─────────────────────┬────────────────────────────────────────────────┘
                      │
┌─────────────────────▼────────────────────────────────────────────────┐
│              TableCache (db/table_cache.h)                            │
│   - LRU 缓存 SST 文件的 Table 对象（避免重复打开）                  │
│   - 单线程互斥保护                                                   │
└──────────────────────────────────────────────────────────────────────┘
```

源码入口：
- DB 入口：`db/db_impl.h` `db/db_impl.cc:DBImpl`
- 写路径：`db/db_impl.cc:DBImpl::Write()`
- 读路径：`db/db_impl.cc:DBImpl::Get()`
- Iterator：`db/db_iter.cc`

### 6.2 进程模型

LevelDB 是**嵌入式库**，不创建独立线程（除了 compaction 后台线程）：

- **应用线程**：调用 Put/Get/Delete；
- **BGThread 后台线程**：负责 compaction 与 flush；
- **锁**：单个全局 mutex（`mutex_`）保护大部分内部状态；这是 LevelDB 性能限制的关键（也是 RocksDB 引入 sharding / lock-free 优化的起点）；
- **TableCache**：单线程访问，LRU 淘汰；
- **线程不安全**：单 DB 实例不支持多线程并发 Put（必须外部加锁或使用 Snapshot 隔离读）。

### 6.3 一条 PUT 命令的端到端路径

1. **应用调用**：`DB::Put(WriteOptions&, const Slice&, const Slice&)`；
2. **构造 WriteBatch**：单条 KV 也封装为 WriteBatch（`db/write_batch.cc`）；
3. **加锁**：`mutex_.Lock()`（避免与 compaction 冲突）；
4. **写入 MemTable**：插入跳表（`mem_->Add()`）；
5. **写入 WAL**：`log_->AddRecord()` 追加物理日志；
6. **检查是否满**：`mem_->ApproximateMemoryUsage() > write_buffer_size`；
7. **解锁**：`mutex_.Unlock()`；
8. **可能触发 flush**：若 mem 满，调用 `MaybeScheduleFlush()` 通知 BGThread；
9. **响应完成**：默认 `sync=false`（异步），`sync=true` 时调 `logfile_->Sync()` 强制刷盘。

### 6.4 一条 GET 命令的端到端路径

1. **应用调用**：`DB::Get(ReadOptions&, const Slice&, std::string*)`；
2. **加锁读版本**：`versions_->current()` 获取当前 Version（ref++）；
3. **查询 MemTable**：`mem_->Get()` + 遍历 `imm_` 链表（已冻结 mem）；
4. **查询 SST**（`version_->Get()`）：
   - L0：因 key range 重叠，需查所有 L0 文件（按 seqnum 倒序）；
   - L1~L6：二分查找 LevelFilesByKey 确定唯一文件；
5. **Bloom Filter**（可选）：`filter->KeyMayMatch()` 快速过滤；
6. **Index Block 定位**：二分查找 index block 找到 data block；
7. **Data Block 二分**：每 16 entries 一个 restart point；
8. **合并多版本**：返回最新 seqnum 对应的 value；
9. **记录统计**：命中/未命中、Bloom 误判等。

## 七、存储引擎（KV 类型核心库深度展开）

LevelDB 是 LSM-Tree 的**经典教科书实现**。

### 7.1 LSM-Tree 三层结构

```
                       Write Path
┌────────────────────────────────────────────────────────────────┐
│  ┌─────────────────┐      ┌──────────────────────────────┐    │
│  │  Active         │      │  WAL (db/log_writer.h)       │    │
│  │  MemTable       │ ───> │  32KB block, CRC32, sync     │    │
│  │  (skiplist)     │      └──────────────────────────────┘    │
│  └────────┬────────┘                                            │
│           │ (full)                                              │
│           ▼                                                     │
│  ┌─────────────────┐      ┌──────────────────────────────┐    │
│  │  Immutable      │ ───> │ Flush to L0                   │    │
│  │  MemTable       │      │ (table/table_builder.cc)      │    │
│  └─────────────────┘      └──────────────────────────────┘    │
└────────────────────────────────────────────────────────────────┘

                       Read Path
┌────────────────────────────────────────────────────────────────┐
│  mem (active) → imm 链 → L0 (overlap) → L1 (sorted) → ... → Ln │
└────────────────────────────────────────────────────────────────┘

                       Compaction
┌────────────────────────────────────────────────────────────────┐
│  L_n Score > 1 → 选 L_n 一个文件 + L_n+1 重叠文件集            │
│  → 多路归并 → 新 L_n+1 文件                                     │
│  → 复用 L_n 文件空间                                            │
└────────────────────────────────────────────────────────────────┘
```

### 7.2 MemTable（跳表）

`db/skiplist.h`：

```cpp
// db/skiplist.h:65
template<typename Key, class Comparator>
class SkipList {
 private:
  struct Node {
    Node const* next_[1];  // 实际为 kMaxHeight=12
    Key const key_;
    
    Node(Key key, int height) : key_(key) {
      for (int i = 0; i < height; i++) next_[i] = nullptr;
    }
  };
  // ... Insert / Contains / Iterator ...
};
```

要点：
- **跳表高度随机**：P=0.25，每节点期望高度 1.33；
- **内存布局优化**：`next_[1]` 占位符 + Node[] 紧邻分配节省指针；
- **无锁读**：跳表只追加不删除（删除仅标记），无锁读；
- **键编码**：`MemTable` 内部 key = `<user_key + seq + type>`（sequence number 高位编码 type）；
- **写满阈值**：`write_buffer_size=4MB` 默认。

### 7.3 SST 文件（Block-Based Table）

`table/format.h`：

```
┌────────────────────────────────────────────────────────────────┐
│                       SST 文件布局                              │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │                  Data Block 1                             │  │
│  │  [restart point 每 16 entries]                             │  │
│  │  [Snappy compressed]                                      │  │
│  ├──────────────────────────────────────────────────────────┤  │
│  │                  Data Block 2 ...                         │  │
│  │                  ...                                      │  │
│  ├──────────────────────────────────────────────────────────┤  │
│  │              Meta Index Block                              │  │
│  │  (filter block offset / index block offset / properties)  │  │
│  ├──────────────────────────────────────────────────────────┤  │
│  │              Index Block                                  │  │
│  │  (data block offsets + first keys + restart intervals)    │  │
│  ├──────────────────────────────────────────────────────────┤  │
│  │              Filter Block (optional)                       │  │
│  ├──────────────────────────────────────────────────────────┤  │
│  │              Properties Block (optional)                   │  │
│  ├──────────────────────────────────────────────────────────┤  │
│  │              Footer (固定 48B)                             │  │
│  │  [meta_index_offset][index_offset][magic][padding]        │  │
│  └──────────────────────────────────────────────────────────┘  │
└────────────────────────────────────────────────────────────────┘
```

要点：
- **Block 大小**：默认 4KB（`block_size=4096`）；
- **Restart Interval**：默认 16（每 16 个 entry 一个全 key，作为二分起点）；
- **压缩**：默认 Snappy；
- **Filter**：可选 Bloom Filter（`util/bloom.cc`，全键）；
- **Checksum**：每个 block 带 CRC32。

### 7.4 WAL（Write-Ahead Log）

`db/log_writer.h`：

```cpp
// db/log_writer.h:30
class Writer {
 private:
  WritableFile* dest_;
  size_t block_offset_;       // 当前 block 已写偏移
  uint32_t type_crc_[kMaxType+1];  // CRC table
  
  Status EmitPhysicalRecord(RecordType type, const char* ptr, size_t length);
};
```

要点：
- **32KB Block**：跨多个 KV 拼接为物理 record；
- **Record 类型**：kFullType（单 block 完整）/ kFirstType / kMiddleType / kLastType（跨 block）；
- **CRC32 校验**：每个 record 头部含 CRC32（包含 type 字节，避免错位）；
- **Sync 策略**：`WriteOptions.sync=true` 时强制 fsync；
- **多 WAL**：LevelDB 单实例单 WAL，不支持多 WAL（RocksDB 支持）。

### 7.5 Compaction（Level Compaction）

`db/compaction.h` / `db/compaction.cc`：

```cpp
// db/compaction.h:13
class Compaction {
 public:
  Compaction(const Options* options, int level);
  
  int level() const { return level_; }       // 0 = L0→L1, n>0 = L_n→L_n+1
  VersionEdit* edit() { return &edit_; }
  bool IsTrivialMove() const;
  // ...
};
```

要点：
- **Score 计算**：`score = total_size(L_n) / max_bytes_for_level(L_n)`，score > 1 触发；
- **文件选择**：选择 L_n 中得分最高的文件 + L_n+1 中与它 key range 重叠的所有文件；
- **多路归并**：K-way merge（K = 1 + L_n+1 重叠数）；
- **Trivial Move**：L_n 文件与 L_n+1 无重叠（仅 boundary 相邻），直接 move；
- **Grandfather 文件**：压实时检查 L_n+2 的 overlap，避免压完又触发更多压；
- **手动压实**：`DB::CompactRange()` 接口手动触发。

### 7.6 Bloom Filter

`util/bloom.cc`：

```cpp
// util/bloom.cc:23
class BloomFilterPolicy : public FilterPolicy {
 private:
  size_t bits_per_key_;
  size_t k_;  // hash 函数数
  
 public:
  BloomFilterPolicy(int bits_per_key) : bits_per_key_(bits_per_key) {
    k_ = static_cast<size_t>(bits_per_key * 0.69);  // ≈ ln(2)
  }
  
  const char* Name() const override;
  void CreateFilter(const Slice* keys, int n, std::string* dst) override;
  bool KeyMayMatch(const Slice& key, const Slice& bloom_filter) override;
};
```

要点：
- **bits_per_key 默认 10**：1% 假阳率；
- **K hash 函数**：基于 `BloomHash` 派生，避免多 hash 函数的额外开销；
- **Block-based Filter**：每个 data block 一个 bloom（精度高、内存大）；
- **可关闭**：`Options::filter_policy=nullptr` 关掉 bloom。

### 7.7 TableCache（LRU 缓存）

`db/table_cache.h`：

```cpp
// db/table_cache.h:25
class TableCache {
 public:
  Iterator* NewIterator(const ReadOptions& options, const FileMetaData& file_meta, ...);
  Status Get(const ReadOptions& options, const FileMetaData& file_meta, ...);
  
 private:
  Cache* cache_;  // 内部 LRU Cache（util/cache.cc）
};
```

要点：
- **LRUCache**：`util/cache.cc` 单线程 LRU；
- **缓存 key**：`<filename> + file_size`；
- **value**：`TableAndFile` 对象（含 file handle + Table 对象）；
- **容量控制**：`Options::max_open_files`（默认 1000）；
- **淘汰策略**：LRU（Least Recently Used）。

### 7.8 VersionSet（版本管理）

`db/version_set.h`：

```cpp
// db/version_set.h:74
class Version {
 public:
  std::vector<FileMetaData*> files_[config::kNumLevels];  // 各层 SST 文件元数据
  // ...
};

class VersionSet {
 public:
  Version* current();   // 当前 Version（原子指针）
  void AppendVersion(Version* v);  // 添加新 Version
  void LogAndApply(VersionEdit* edit);  // 应用版本变更（持久化到 MANIFEST）
};
```

要点：
- **Version**：某一时间点的快照（所有层的 SST 文件集合）；
- **VersionEdit**：版本变更操作（AddFile / RemoveFile / SetNextFileNumber / SetLastSequence）；
- **原子指针**：`current_` 是原子指针，多读者并发读；
- **MANIFEST**：每次 version edit 持久化到 MANIFEST 文件（启动 replay）；
- **垃圾文件清理**：旧 Version 引用计数为 0 时，其 SST 文件被删除。

## 八、索引机制

- **主索引**：SST 的 Index Block（指向 Data Block 的 offsets + first key）；
- **Bloom Filter**：加速不存在的 key 查询；
- **二分查找**：Data Block 内每 16 entry 一个 restart point；
- **L0 范围扫描**：L0 文件 key range 重叠，LevelIterator 多路归并；
- **没有二级索引**：LevelDB 是纯 KV，不支持 range/file/priority；上层应用自行实现。

## 九、查询/计算/接口层

- **API 风格**：C++ 嵌入式 API（`DB::Put/Get/Delete/Write` + Iterator）；
- **语言绑定**：Go（`goleveldb` / `syndtr/goleveldb`）、Java（`leveldbjni`）、Python（`plyvel`）、Rust（`rusty-leveldb`）、Ruby（`leveldb-ruby`）；
- **执行模型**：直接方法调用，无 SQL；
- **批量写**：`WriteBatch`（atomic write 多条记录）；
- **Iterator**：前缀扫描、范围扫描、反向迭代（注意：反向迭代器只支持单层合并的 L0 → L_n）；
- **Snapshot**：`DB::GetSnapshot()` 提供一致性读视图（基于 sequence number）；
- **CompactRange**：手动触发范围压缩。

## 十、事务与一致性（KV 类型简化版）

LevelDB 事务模型**非常弱**：

- **WriteBatch 原子性**：单 `Write()` 调用内的多条 Put/Delete 原子应用，崩溃后能完整恢复（基于 WAL seqnum）；
- **Snapshot 隔离**：读可获取某个 sequence number 的快照视图，与其他写入并发无影响；
- **没有跨 key 事务**：单 key 操作（Put/Get/Delete）天然原子；
- **没有 OCC / 2PC**：单实例无多 key ACID 事务；
- **持久化一致性**：`WriteOptions::sync=true` + WAL 强同步（fsync）；`sync=false` 异步（默认）；
- **崩溃恢复**：启动时 replay MANIFEST + WAL，重建内存中的 MemTable 状态。

## 十一、复制与高可用

LevelDB **本身不内置复制**，由上层应用实现：

- **单实例**：最常见用法；
- **Replication**：上层（如 Riak）做 Raft / Paxos 复制 + LSM 引擎；
- **Backup**：手动复制文件目录（注意一致性快照时间点）；
- **Import / Export**：`WriteBatch` 序列化协议可导出导入（`db/write_batch.cc:WriteBatchInternal::SetContents`）。

## 十二、扩展性

- **多列族**：不支持（LevelDB 是单命名空间；RocksDB 引入 Column Family）；
- **横向扩展**：上层（Riak / HyperLevelDB）按 shard 分片；
- **垂直扩展**：单实例可配数百 MB 内存（MemTable + TableCache）；
- **动态参数调整**：不支持运行时改 Options；
- **TTL**：不支持（用户需自己实现）；
- **冷热分层**：不支持。

## 十三、性能与基准

- **官方基准**（`leveldb/db/leveldb_main.cc`，已弃用）：
  - micro-bench（fillrandom、readrandom、readrandomwriterandom）；
- **db_bench**：标准测试工具（`util/bench.cc`）；
- **YCSB**（Yahoo! Cloud Serving Benchmark）：LevelDB 表现良好（read-heavy + write-heavy）；
- **关键指标**：
  - **写放大**：~10-30 倍（取决于 compaction 频率）；
  - **读放大**：~每层 1 次 IO；
  - **空间放大**：~5-10%（压缩 + 待压缩文件）；
- **典型数字**：随机写 ~ 30K op/s（HDD），点查 ~ 50K op/s（Block Cache 命中）；
- **性能瓶颈**：单 mutex 锁、RocksDB fork 主因即解决此瓶颈。

## 十四、运维与可观测性

- **监控指标**：`GetStats()`、`DB::GetProperty()` 提供 stats；
- **Histogram**：`HistogramType` 跟踪操作延迟；
- **日志**：`Options::info_log` 自定义 logger（默认写入文件）；
- **Backup**：手动 `cp -r` 数据库目录（建议停写后操作）；
- **Recovery**：启动自动 replay MANIFEST + WAL；
- **tracing**：无内置支持，需外部 hook。

## 十五、生态与集成

- **客户端 SDK**：C（`leveldb/c.h`）、C++、Go（`goleveldb`）、Java（`leveldbjni`）、Python（`plyvel`）、Rust（`rusty-leveldb`）；
- **数据库**：Riak KV（早期）、HyperLevelDB（Riak 团队 fork，并发压缩）、Accumulo 部分子项目；
- **消息队列**：BuntDB、Tile38；
- **容器化**：无官方 Docker（嵌入式库），上层应用自带；
- **BI 工具**：无（嵌入式引擎，无 SQL 接口）；
- **存储服务**：Chrome IndexedDB、Android 系统数据库（早期）。

## 十六、变体与衍生项目

- **RocksDB**：Facebook fork，加入 SSD 优化、多线程、Column Family（详见 rocksdb.md）；
- **Pebble**：CockroachDB fork RocksDB 后独立，Go 重写，模块化；
- **HyperLevelDB**：Riak 团队 fork，加入多线程 compaction；
- **Bedrock**：Basho 出品，基于 LevelDB 的 Berkeley DB 风格扩展；
- **Tectonic**：GitHub 上半维护的 fork；
- **LevelDB.NET / leveldb-sharp**：.NET 包装。

## 十七、核心论文与算法原理

- **The Log-Structured Merge-Tree (LSM-Tree)**（O'Neil et al., 1996）：LSM 原始论文；
- **Bigtable: A Distributed Storage System for Structured Data**（Google, 2006）：Bigtable 论文，详细描述 LSM 设计；
- **Skip Lists: A Probabilistic Alternative to Balanced Trees**（Pugh, 1990）：跳表原始论文；
- **LevelDB Implementation Notes**（Sanjay Ghemawat & Jeff Dean）：LevelDB 官方实现笔记（http://leveldb.google.com/static/docs/impl.html）；
- **RocksDB: Evolution of LevelDB**（Dhruba Borthakur, Facebook Engineering）：fork 的演化分析。

## 十八、源码学习路径

- **入门（1 周）**：
  - 阅读 `include/leveldb/db.h`：所有公共接口；
  - 阅读 `db/db_impl.h` 与 `db/db_impl.cc`：核心实现；
  - 阅读 `db/skiplist.h`：跳表实现；
  - 阅读 `db/memtable.h`：MemTable 实现；
  - 阅读 `db/version_set.h` + `db/version_set.cc`：版本管理。
- **进阶（2-3 周）**：
  - 阅读 `table/format.h` + `table/builder.cc` + `table/table.cc`：SST 构造与读取；
  - 阅读 `db/log_writer.h` + `db/log_reader.h`：WAL；
  - 阅读 `db/compaction.h` + `db/compaction.cc`：压缩核心；
  - 阅读 `util/bloom.cc`：Bloom Filter；
  - 阅读 `util/cache.cc`：LRU Cache。
- **高级（持续）**：
  - 阅读 `db/db_iter.cc`：Iterator 实现（多路归并）；
  - 阅读 `db/builder.cc` + `db/table_builder.cc`：SST Builder；
  - 阅读 `db/repair.cc`：数据库修复工具；
  - 阅读 `db/c.cc`：C API 包装。

### 18.1 关键源码路径速查

| 模块 | 路径 |
|------|------|
| DB 主入口 | `db/db_impl.h` + `db/db_impl.cc` |
| 写路径 | `db/db_impl.cc:DBImpl::Write()` |
| 读路径 | `db/db_impl.cc:DBImpl::Get()` |
| MemTable | `db/memtable.h` + `db/skiplist.h` |
| SST Builder | `table/builder.cc` |
| SST Reader | `table/table.cc` |
| SST 格式 | `table/format.h` |
| Compaction | `db/compaction.h` + `db/compaction.cc` |
| WAL Writer | `db/log_writer.h` |
| WAL Reader | `db/log_reader.h` |
| Bloom | `util/bloom.cc` |
| Cache | `util/cache.cc` |
| VersionSet | `db/version_set.h` |
| WriteBatch | `db/write_batch.h` |
| Iterator | `db/db_iter.h` |
| C API | `db/c.cc` |
| 基准工具 | `util/bench.cc` |
| repair 工具 | `db/repair.cc` |

## 二十、源码规模与开发语言

- **LOC 数量**：约 2.5 万行 C++（核心 `db/` + `table/` + `util/` + `include/`）；
- **主要语言**：C++11（核心）、C（`c.h`）、Bash（基准）；
- **依赖**：`libpthread`、`libc`、`libsnappy`（可选压缩）；**默认零依赖**；
- **构建系统**：CMake（`CMakeLists.txt`）+ Makefile（`Makefile`）；
- **测试**：C++ Google Test + 自有单元测试；
- **CI**：Travis CI（基础）；GitHub Actions（PR 触发）。

## 二十一、学习资源

- **官方文档**：[https://github.com/google/leveldb/blob/main/doc/index.md](https://github.com/google/leveldb/blob/main/doc/index.md)；
- **实现笔记**：[http://leveldb.google.com/static/docs/impl.html](http://leveldb.google.com/static/docs/impl.html)；
- **官方博客**：Google Research Blog（Jeff Dean / Sanjay Ghemawat）；
- **论文参考**：LSM 原始论文、Bigtable 论文、跳表论文；
- **书籍**：《LevelDB 源码解析》（社区）、《Designing Data-Intensive Applications》（Martin Kleppmann，含 LSM 章节）；
- **YouTube**：CMU Database Group 演讲（LevelDB 实现讲解）；
- **课程**：CMU 15-445（Andy Pavlo）、Stanford CS346、MIT 6.824；
- **Reddit / HN**：r/databases、news.ycombinator.com。

## 二十二、可移植性与部署

- **支持平台**：Linux（全架构）、macOS、Windows（实验性）、BSD、Android、iOS；
- **编译器**：GCC 4.8+、Clang 3.0+、MSVC 2015+；
- **依赖**：默认零依赖（仅 pthread）；可选 Snappy（压缩）；
- **容器化**：嵌入式库（无独立服务），上层应用自带 Docker；
- **嵌入式部署**：直接静态链接到 C/C++ 应用；Chrome / Android 即典型；
- **云原生**：无（无独立服务）。

## 二十三、风险与陷阱

- **单 mutex 锁**：多线程写入性能受限；高并发建议 fork RocksDB；
- **单后台线程 compaction**：压缩跟不上写入时前端 stall；监控 `stats[Compaction]`；
- **没有 Column Family**：多业务隔离需用多实例；
- **没有 Universal Compaction**：写密集场景写放大大；
- **没有 Partitioned Index**：大 SST 索引块读延迟较高；
- **大 value 不友好**：单 value > 100KB 性能下降；用 WiscKey 风格需 fork；
- **没有内置 TTL**：需外部 TTL 或 fork；
- **没有多线程 compaction**：HyperLevelDB 解决；
- **崩溃恢复丢数据**：默认 `sync=false` 异步 WAL；强一致需 `sync=true`；
- **Iterator 持有**：长时间持有 Iterator 阻止 SST 文件回收；
- **慢查询**：点查延迟通常 < 1ms，但冷 SST 范围扫描可能百毫秒级。

## 二十四、与本项目的关联

- **`engineering/` 中可能的对应实现**：
  - `engineering/src/db/` 中如需嵌入 LSM 引擎，LevelDB 是极简参考；
  - 跳表（skiplist）实现可作为 `learning/ds-c/orig/skiplist.c` 教学代码；
  - WAL + MemTable 的写路径设计（log-then-memtable）可作为 WAL 学习材料；
  - VersionSet 的版本管理可参考为 MVCC 教学案例；
- **学习复用价值**：
  - SST 格式设计（Block-based + Bloom + Index）可移植到 `learning/ds-c/`；
  - Level compaction 算法（Score-based picker）作为 LSM 入门教学；
  - 跳表 + Restart Interval + 二分查找组合是教学经典；
  - 整个代码库 LOC 仅 2.5 万行，是 C++ LSM 入门首选。

## 二十六、WriteBatch 合并算法与 Version 快照读取

> 本节源码路径：`db/dbformat.h`、`db/dbformat.cc`、`db/write_batch.cc`、`db/version_set.cc`、`db/memtable.cc`（canonical；模块未 init，行号需校对）。§7.2 / §7.8 只给了类骨架与要点清单，本节补齐「键怎么打包 → 批次怎么重放 → 读时怎么裁决版本」三段算法。

### 26.1 Internal Key 的打包与排序

跳表、SST 内部存的都不是用户键，而是 **InternalKey** = `user_key || 8 字节 trailer`，trailer 按小端打包：

```
trailer = (sequence << 8) | type
```

`InternalKeyComparator`（`db/dbformat.h` + `db/dbformat.cc`）的语义是「用户键升序 + 序号降序」：

```
Compare(a, b):
    r = user_comparator.Compare(a.user_key, b.user_key)
    if r != 0: return r
    # trailer 以小端字节序存放，字节序比较 == 数值比较
    if a.trailer > b.trailer: return -1      # 大 trailer 排在前 = 新记录排在前
    if a.trailer < b.trailer: return +1
    return 0
```

两个推论：

- **同一 user_key 的最新版本一定排最前**，Seek 后取到的第一条即最新版本，不必遍历比较时间戳；
- 排序键是 `(user_key ASC, sequence DESC)`，所以一个 key 在 SST 内是一条「倒序时间线」，旧版本物理上排在后面，只能靠 compaction 回收。

> 复杂度：单次比较 O(键长)；跳表查找期望 O(log n) 次比较 ⇒ 期望 O(log n · L)。注意因 `kMaxHeight` 上界存在，极端情况退化到 O(n)。

### 26.2 WriteBatch 的物理编码

`WriteBatch` 是「一次原子提交多条记录」的载体，其编码（`db/write_batch.cc`）= 12 字节头 + N 条变长记录：

```
[8B sequence (fixed64, 小端)][4B count (fixed32, 小端)] [record] × count
```

每条 record 是「1 字节 ValueType 标签 + 若干 LengthPrefixedSlice 字段」。符号名取自 `db/dbformat.h` 的 `ValueType` 枚举；枚举数值随版本演进（RangeDeletion 等新类型曾插入），故此处只列语义、不列数值：

| ValueType | 负载字段 | 语义 |
|---|---|---|
| `kTypeValue` | key, value | 写入 |
| `kTypeDeletion` | key | 写 tombstone |
| `kTypeSingleDeletion` | key | 写「一次性删除」标记 |
| `kTypeMerge` | key, value | 追加一个 merge operand |
| `kTypeColumnFamilyValue` | cf, key, value | 多列族写入（LevelDB 主干未启用列族，见 §十二） |
| `kTypeColumnFamilyDeletion` | cf, key | 多列族删除 |
| `kTypeLogData` | blob | 只进 WAL、不进 MemTable 的旁路载荷 |

**SingleDeletion 与普通 tombstone 的区别**：读路径遇到 `kTypeSingleDeletion` 即可判定「该 key 的全部旧版本作废」，不再往下层找；而 `kTypeDeletion` 只挡住比它更新的版本，比它更旧的仍可能被读到。二者都最终要靠 compaction 真正下沉清理，但 SingleDeletion 允许压实时更激进地丢弃整条 key 的历史链。

### 26.3 InsertInto：把 batch 重放到目标

`WriteBatchInternal::InsertInto`（`db/write_batch.cc`）用一个内部接口 `Handler` 把「类型分支」从遍历逻辑中抽离，MemTable 与 Version 实现同一套 Handler，于是同一份 batch 既能重放到内存表、也能在恢复时重放到 SST 层：

```
InsertInto(mem):
    writer = MemTableInserter{ snapshot = batch.sequence, mem }
    if batch.count == 0: return
    if batch.sequence + batch.count > mem.latest_sequence:   # 跨 batch 推进时回调 DBImpl
        return HandleNotInBatch(batch.sequence, batch.count)
    Iterate(batch.contents[12 ..], &writer)                   # 触发 writer.Put / Delete / SingleDelete / Merge
```

类型分支伪代码（Handler 视角，注意 **seq 在遍历中自增**）：

```
Handler::Put(cf, key, value):
    mem.Insert(EncodeInternalKey(key, ++seq, kTypeValue), value)
Handler::Delete(cf, key):
    mem.Insert(EncodeInternalKey(key, ++seq, kTypeDeletion), "")
Handler::SingleDelete(cf, key):
    mem.Insert(EncodeInternalKey(key, ++seq, kTypeSingleDeletion), "")
Handler::Merge(cf, key, value):
    mem.Insert(EncodeInternalKey(key, ++seq, kTypeMerge), value)
```

一次 batch 内的 N 条记录因此天然占据连续 N 个序号 `[S, S+N)`。这正是 §十 中「WriteBatch 原子性」的存储层基础：崩溃恢复时只要序号连续即可整批重放。原子可见性由读路径的 snapshot 序号裁决 —— `LookupKey` 以 batch 首序号 `S` 为可见上界，落在 `(S, S+N)` 区间内的记录对尚未推进的读者一律不可见。

### 26.4 Version::Get：自底向上的版本裁决

读路径最终落到 `Version::Get`（`db/version_set.cc`），顺序固定为「L0 → L1 → … → 最大非空层」（MemTable 由 `DBImpl::Get` 在调用本函数前先行查询），遇到**第一个可裁决的记录**即停止：

```
Version::Get(options, lkey, value, stats):
    state = fresh
    # 1) L0：文件间 key range 重叠，必须全查；Version 保持 files_[0] 按序号倒序
    GetOverlappingInputs(level = 0, &lkey, /*null largest*/ &tmp)
    for f in tmp:                          # 最新的文件先试
        TableCache::Get(f) -> Table::InternalGet(...)
        if state.found: return true
    # 2) L1..N：range 互不重叠，二分定位唯一文件即可
    for level in 1 .. kNumLevels-1:
        f = FindFile(level, lkey)         # 二分：smallest <= target <= largest
        if f == nullptr: continue
        TableCache::Get(f) -> Table::InternalGet(...)
        SaveValue(lkey, value_type, value, state)
        if state.found: return true
    return state.found
```

`SaveValue` 是**「能不能停」的裁决核心**（`db/version_set.cc` 中的独立成员函数）：

```
SaveValue(lkey, value_type, value, state) -> stop:
    if value_type ∉ {kTypeValue, kTypeMerge}:
        return true        # kTypeDeletion / kTypeSingleDeletion：
                            # 已有更新版本判定「不存在」，下层同 key 必然更旧，无意义，直接停
    if value_type == kTypeMerge:
        state.merge_operands.push(value)     # merge 可累加，继续下探找基底
        return false
    # kTypeValue：这就是该 key 的「基底值」
    state.base_value = value
    state.found = true
    return true                              # 停止向下查找
```

注意职责边界：`Version::Get` 只负责**找齐 base + operands 并置位状态**，operand 的实际归约（`MergeHelper::TimedFullMerge`）发生在 `DBImpl::Get` 层完成。

> 复杂度与放大：`FindFile` 每次 O(log n_level)；磁盘代价上界 = 每层 1 次 index seek + 1 次 data block seek ⇒ 读放大 R = O(kNumLevels)。若 Bloom 全部判定「不存在」（查询的 key 真不存在），R 退化为 O(kNumLevels) 次常数级内存判定、**0 次磁盘 IO** —— 这是 Bloom filter 的主要收益场景（详见 §二十八）。

### 26.5 Snapshot：序号即一致性视图

`DBImpl::Get` 未显式指定 `options.snapshot` 时取 `versions_->LastSequence()` 作上界；显式指定则取 `SnapshotImpl::sequence_`（`db/db_impl.h`）。可见性不靠事后过滤，而是靠**比较器的截断**：`LookupKey(key, snapshot)` 把 seek 键的 trailer 固定为 `(snapshot, kValueTypeForSeek)`；由于排序是 seq 降序，Seek 会正好停在第一条「序号 ≤ snapshot」的记录上。

```
seek_key = Encode(user_key, trailer = (snapshot << 8) | kValueTypeForSeek)
# 排序 (user_key ASC, seq DESC) ⇒ seek 之后的第一个有效节点即 snapshot 可见的最新版本
```

复杂度 O(1)（无额外扫描），代价只是 seek 键多带 8 字节。

## 二十七、Compaction 触发、选择与 TrivialMove 优化

> 本节源码路径：`db/version_set.cc`（`Version::PickCompaction` / `PickLevelCompaction` / `GetOverlappingInputs`）、`db/compaction.cc`（`Compaction::IsTrivialMove`）、`db/version_edit.cc`（canonical；模块未 init，行号需校对）。§7.5 给了类骨架与一句式 score 公式，本节补齐触发判据、两套文件选择策略、免 IO 优化条件与写放大量级推导。

### 27.1 触发判据：score，自低层向上取首个超限者

```
score(L0)   = |files_[L0]| / kL0_CompactionTrigger
score(L>0)  = TotalFileSize(L) / MaxBytesForLevel(L)

MaxBytesForLevel(L) = 10MiB × max_bytes_for_level_multiplier^(L-1)      # L ≥ 1

PickCompaction():
    for level in 0 .. kNumLevels-2:
        if score(level) >= 1.0: return PickLevelCompaction(level)        # 低层优先
    return PickFileToCompact()                                          # 兜底：按 seek 计数挑最冷文件
```

要点：

- **L0 用文件数而非字节数**：新 flush 的文件大小未知（可能远小于 2MiB 目标值），按字节算 score 会长期为 0 而永不触发。`kL0_CompactionTrigger` 的具体默认值随版本演进，此处不写死数字，其量级为「个位数」。另有独立的写停顿阈值 `kL0_StopWritesTrigger` 控制反压。
- **`MaxBytesForLevel` 是等比数列**（公比 = `max_bytes_for_level_multiplier`，默认 10）。每层容量差 10 倍是 LevelDB 写放大可控的直接原因（推导见 27.4）。
- **低层优先**：一旦 L0 超限就压 L0，即使 L3 也超限也不会先压 L3 —— 因为 L0 是读路径必经层，收益最大。
- **兜底路径**保证压实永不饿死：所有层 score 都 < 1 时按 seek 计数挑文件，摊平长期冷数据。

> 复杂度：逐层求 `TotalFileSize` 为 O(总文件数)；单次 Pick O(总文件数)，全部发生在后台线程，不进入读路径。

### 27.2 PickLevelCompaction 的两套策略

```
PickLevelCompaction(level):
    if level == 0:
        return SetupOtherL0Files(files_[0] sorted by seq DESC)   # 文件可能互相重叠，逐个向后取
    else:
        f = FindSmallestFileWithLeastOverlap(output_level = level + 1)   # 重叠字节最少 = 最省 IO
        return SetupOtherInputs(f)
```

- **L0 路径**：`SetupOtherL0Files` 从当前文件往后**连续**取文件，直到下一个文件与已选集合的 range 不再重叠 —— 因为 L0 单个文件可能同时压在多个 L0 文件上，不连续取会漏数据。
- **L>0 路径**：`FindSmallestFileWithLeastOverlap` 在输出层逐文件累加重叠字节数，取最小者。这是经典的 **min-overlap 选择**：输入文件选得越「干净」，需要归并的输出层文件越少，IO 越省。
- **Grandfather 限制**：输入集合展开后，还要检查它与 **L+2 层**的重叠字节总量；超阈值就扩展输入集合「多吞几个同层文件」一并压掉，避免压完立刻触发下一次级联压实（parent 变成 grandparent 的数据量暴涨）。
- 两种策略的展开都由 `GetOverlappingInputs` 完成 —— 它就是 §26.4 读路径 L0 检索用的同一个函数，一套 range overlap 判定服务读写两侧。

### 27.3 IsTrivialMove：LevelDB 最具特色的免 IO 优化

若一次压实的输出与某个已存在文件「逐字节等价」，压实就退化成一次文件重命名。`Compaction::IsTrivialMove`（`db/compaction.cc`）给出三个必须**同时**成立的判据：

```
IsTrivialMove(files) -> bool:
    # 判据 1：总大小不超过输出层容量上限（防止免 IO 搬运造出巨型文件）
    if TotalFileSize(files) > MaxBytesForLevel(output_level): return false
    # 判据 2：只能是单文件输入（多文件必然要归并，无法复用）
    if files.size() != 1: return false
    # 判据 3：输入文件的 key range 不得与祖父层（output_level + 1）重叠
    if SomeFileOverlapsRange(output_level + 1, f.smallest, f.largest): return false
    return true
```

为什么判据 3（祖父层检查）必要：免 IO 搬运把文件「就地升级」到输出层，若它与祖父层大量重叠，祖父层本应被压掉的数据就要在下一轮做一次昂贵的全量归并 —— 等于把成本推迟并放大。这正是 LevelDB 官方实现笔记反复强调的「不要制造一个需要昂贵 merge 的 parent file」。

TrivialMove 生效时的 `VersionEdit` 只做两件事：把该文件标记删除、以新 level 号重新加入 —— **不 merge、不写新数据、只有一次 rename**，`edit` 里不含任何 SST 内容。走这条路径时压缩的写 IO 摊销为 0，是 LevelDB 在「刚 flush、下一层恰好有同区间空位」场景下的关键性能来源。

> 注：部分后续版本在判据 1 之前追加了更严格的祖父层字节预算检查（按 grand-grandparent 重叠量设阈值）。本文按上游主干的三判据表述，具体阈值以所用版本源码为准。

### 27.4 写放大的数量级推导

LevelDB 采用 level compaction（不支持 universal，见 §7.5），一条记录从 flush 进 L0 到沉到最底层，需跨越 `kNumLevels - 1` 次层间压实，**每下沉一层就被完整重写一次**：

```
WA ≈ 1 (flush 写) + (kNumLevels - 1) (逐层重写) ≈ kNumLevels      # kNumLevels = 7 ⇒ 量级 ~7
# 实测量级 ~10-30 倍（见 §十三）：差值来自 merge 放大、边界文件非满量归并、块压缩失败重写等
```

两条可调的权衡：

- 层数越多 / 每层容量倍增比越激进 ⇒ 写放大越低，但读放大线性上升（每层一次 seek）；
- LevelDB 固定 `kNumLevels = 7`、不提供运行时动态调层 —— 这是它与 RocksDB（可配置层数、多 Column Family、universal/tiered 策略）的分水岭。

## 二十八、SST 文件结构、BlockBuilder 前缀压缩与 FilterBlock 查询

> 本节源码路径：`db/table/builder.cc`、`db/table/format.cc`、`db/table/format.h`、`db/table/two_level_iterator.cc`、`table/filter_block.cc`、`table/block_based_filter_policy.h`、`util/bloom.cc`（canonical；模块未 init，行号需校对）。§7.3 画了块级布局、§7.6 给了 Bloom 类骨架，本节补齐「块内怎么排 → 块间怎么定位 → 缺失 key 怎么跳过」三层算法。

### 28.1 BlockBuilder：前缀压缩 + restart 数组

`BlockBuilder::Add`（`db/table/builder.cc`）对相邻键做公共前缀压缩：

```
Add(key, value):
    shared     = common_prefix_length(key, last_key)
    non_shared = key.size() - shared
    EncodeVarint32(shared); EncodeVarint32(non_shared)
    append(key[shared ..]); append(value)
    restarts.push_back(current_offset)
    if num_entries % block_restart_interval == 0:      # 默认 16
        restarts.push_back(restarts.back())           # 追加一个全量键作为新锚点
        last_key.clear()
```

`Finish` 收尾时把 restart 偏移数组追加到数据之后，每个偏移变长编码，末尾再写 4 字节 `num_restarts`：

```
[entries:  varint32 shared | varint32 non_shared | key_tail | value ] × N
[restart 数组: varint32 偏移 × num_restarts ]
[4B num_restarts]
[block trailer: 1B block type + 4B crc32c]                  # kBlockTrailerSize = 5
```

两个参数的取值与含义：

| 参数 | 默认 | 作用 |
|---|---|---|
| `Options::block_size` | 4096 字节 | 压缩单元大小，也是一次磁盘 IO 的粒度 |
| `Options::block_restart_interval` | 16 | 前缀压缩的「重启周期」，也是线性扫描上界 |
| `kBlockTrailerSize` | 5 字节 | 每块尾部 1 字节类型 + 4 字节校验和 |

（校验算法上游为 **crc32c** Castagnoli，`table/format.cc` 的 crc mask 为 `0xa282ead8`；此处与 §7.4 提到的 WAL 侧 CRC32 属不同实现。）

**空间复杂度**：设块内 N 条键、平均键长 L、公共前缀占比 p、value 平均长 V，则

```
块大小 ≈ N·(1-p)·L + N·V + O(N/16)·varint(L) + O(N/16)
```

**点查复杂度**：先在 restart 数组上二分（O(log(N/16)) 次键比较）落入区间，再顺序线性扫描至多 `block_restart_interval` 条 ⇒

```
单块点查 = O(log(N/16) + block_restart_interval) 次键比较，与块大小无关
```

这正是 restart 机制的核心价值：没有 restart 点就得对全块做前缀逐字节二分（O(N) 次比较且必须从头解压键），有了它就把「键前缀依赖」限制在 16 条的窗口内。

### 28.2 BlockHandle 与两级定位（TwoLevelIterator）

块在文件中的位置用 `BlockHandle{offset, size}` 表示，`db/table/format.cc` 用变长编码落盘（`util/coding.h` 的 `EncodeVarint64` / `DecodeVarint64`，单值最多 10 字节）：

```
Encode:  varint64(offset) || varint64(size)
Decode:  读 varint64 得 offset，再读 varint64 得 size，
         有效负载长度 = size - kBlockTrailerSize      # 扣掉尾部 type + crc
```

Footer 是文件末尾的定长尾记录，存放两个 `BlockHandle`（meta index 与 index），后跟对齐填充与 8 字节 magic number（`table/format.h` 的 `kTableMagicNumber = 0xdb4775248b80fb57`，用于快速判定「这是不是一个 LevelDB table」）。关键设计点：meta index 自身以 `filter.<policy name> → BlockHandle` 记录 filter 块位置，因此**换 filter 策略只需改 meta index，不必重排任何 data block**。

两级定位由 `db/table/two_level_iterator.cc` 实现：

```
Seek(target):
    index_iter_.Seek(target)                     # index block 内二分：entry = (块首键 → BlockHandle)
    if !index_iter_.Valid(): return INVALID
    data_iter_ = block_cache.Get(index_iter_.value())    # BlockHandle → 该 data block 的迭代器
    data_iter_.Seek(target)                     # 块内 restart 数组二分 + 窗口内线性扫描
    SaveDataIterator(data_iter_, &data_iter_)  # 缓存，Next() 时省去再次取块
```

点查代价：**1 次 index block 读 + 1 次 data block 读**（两次顺序 IO；命中 OS page cache 时后者常为 0 次物理 IO），CPU 代价为两次块内二分，即 O(log(块内条目数)) 次键比较。

### 28.3 FilterBlock：每块一份 Bloom + 稀疏 offset 数组

Bloom 数据不是「一个文件一份」，而是**每个 data block 独立一份**，索引用一个稀疏 offset 数组（`table/filter_block.cc`）：

```
FilterBlockBuilder::AddFilter(filter):            # 每个 data block 调一次
    filter_offsets.push(data_.size())             # 记录该 filter 在数据区的起始偏移
    data_.append(1 字节长度前缀)                    # 让 reader 知道这个 filter 有多长
    data_.append(filter)

FullFilterBlockReader::Filter(block_offset):      # reader 侧：filter 数可远多于查询数
    index = block_offset >> base_lg_              # base_lg_ 由 filter 平均大小推出，直接算候选下标
    start = offsets[index]; limit = offsets[index + 1]
    if block_offset == last_block_offset_ && !last_filter.empty():
        return last_filter                        # 命中上一次缓存，零 IO
    return ReadFilter(filter_data, start, limit)  # 未命中才解引用
```

`BlockBasedFilterPolicy::KeyMayMatch`（`table/block_based_filter_policy.h`）的判定只有两步：

```
KeyMayMatch(block_offset, key):
    f = reader.Filter(block_offset)
    if f.empty(): return true                     # 关键：该块没有 filter 时必须放行，
                                                  #       不能误判为「不存在」
    return BloomFilterPolicy::KeyMayMatch(key, f) # 见 §7.6
```

「缺 filter 即放行」是**正确性要点**：Bloom 只能产生假阳（把不存在说成可能存在），绝不能产生假阴；缺失 filter 的唯一安全行为是退化为全扫。

### 28.4 Bloom 位寻址：单个哈希派生的等差序列

`util/bloom.cc` 不做 k 次独立哈希，而是用**一个 64 位哈希派生的等差序列**逼近 k 个独立哈希：

```
构建:  h    = BloomHash(key)                    # util/hash.cc 的 64 位哈希
       delta = (h >> 17) | (h << 47)            # 循环左移 47 位，取另一组近似独立位模式
       bits  = n × bits_per_key                 # n = 本次 Add 的键数
       对 j = 0 .. k-1:
           bitpos = h % bits
           result[bitpos >> 3] |= 1 << (bitpos & 7)
           h += delta                           # 模 2^64 自然回绕

查询:  h    = BloomHash(key)
       delta = (h >> 17) | (h << 47)
       bits  = (filter_len - 1) × 8             # 减去 1 字节长度前缀
       对 j = 0 .. k-1:
           bitpos = h % bits
           if (filter[bitpos >> 3] & (1 << (bitpos & 7))) == 0: return false   # 任一未置位即排除
           h += delta
       return true
```

假阳率（独立哈希近似）：

```
p ≈ (1 - e^(-k·n/m))^k,   取 k = bits_per_key · ln 2 使 p 关于 bits_per_key 单调最小
# §7.6 的 k_ = bits_per_key × 0.69 即 ln 2 的工程近似
# bits_per_key = 10 ⇒ k = 6, p ≈ 1%
```

Bloom 带来的复杂度收益：本来要 O(log(N/16) + 16) 次键比较 + 2 次块读才能确认「不存在」；命中 filter 后只需 O(k) 次位判定即可返回 false，**磁盘 IO 降为 0**。收益随「查询中不存在的 key 占比」近似线性增长 —— 这正是读多写少、且查询以点查为主的负载下 Bloom 最划算的原因。反之，写入密集型负载要为每条记录额外付出 k 次位置 1 的写放大，性价比下降。

> 变体说明：`delta` 的位移量、BloomHash 的具体实现（`util/hash.cc`）在派生 fork（Pebble、以及移植进 RocksDB 的版本）中存在差异，本节按上游 LevelDB 表述。

## 二十九、参考资料

- [LevelDB GitHub](https://github.com/google/leveldb)
- [LevelDB Implementation Notes](http://leveldb.google.com/static/docs/impl.html)
- [LevelDB 实现笔记（中文版）](https://github.com/google/leveldb/blob/main/doc/impl.md)
- [LSM-Tree 原始论文（O'Neil 1996）](https://www.cs.umb.edu/~poneil/lsmtree.pdf)
- [Bigtable 论文（Google 2006）](https://research.google/pubs/large-scale-data-management-using-bigtable/)
- [Skip Lists 论文（Pugh 1990）](https://15721.codetables.org/slides/2023/07_pugh_1990.pdf)
- [Designing Data-Intensive Applications（Martin Kleppmann）](https://dataintensive.net/)
- [LevelDB 源码阅读笔记（社区）](https://github.com/google/leveldb/wiki)
- [RocksDB Wiki - LevelDB 对比](https://github.com/facebook/rocksdb/wiki/LevelDB)
- [Riak KV 文档 - LevelDB 后端](https://docs.riak.com/riak/kv/latest/using/cluster-operations/leveldb/)
- [HyperLevelDB GitHub](https://github.com/rescrv/HyperLevelDB)