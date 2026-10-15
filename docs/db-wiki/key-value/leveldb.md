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

## 二十五、参考资料

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