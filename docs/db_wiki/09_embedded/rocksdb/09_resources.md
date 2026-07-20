# RocksDB 学习资源

## 学习目标

- 掌握 RocksDB 源码目录结构
- 了解关键源码文件
- 建立源码阅读路径

## 源码路径

### 项目仓库

```
reference/open-source/rocksdb/  # 源码镜像（非 CMake 构建）
```

### 目录结构

```
rocksdb/
├── db/                       # 核心实现
│   ├── db_impl.cc           # DB 主实现
│   ├── db_impl.h
│   ├── column_family.cc     # 列族实现
│   ├── column_family.h
│   ├── memtable.cc          # MemTable
│   ├── memtable.h
│   ├── memtable_list.cc     # MemTable 列表
│   ├── memtable_list.h
│   ├── version_set.cc       # Version 管理
│   ├── version_set.h
│   ├── version_edit.cc      # Version 变更
│   ├── version_edit.h
│   ├── compaction_job.cc    # Compaction 执行
│   ├── compaction_job.h
│   ├── compaction_picker.cc # Compaction 选择
│   ├── compaction_picker.h
│   ├── flush_job.cc         # Flush 执行
│   ├── flush_job.h
│   ├── write_batch.cc       # WriteBatch
│   ├── write_batch.h
│   ├── write_thread.cc      # 写入线程
│   └── write_thread.h
│
├── table/                    # SSTable 格式
│   ├── block_based_table_builder.cc  # SSTable 构建
│   ├── block_based_table_builder.h
│   ├── block_based_table_reader.cc   # SSTable 读取
│   ├── block_based_table_reader.h
│   ├── block.cc              # Data Block
│   ├── block.h
│   ├── block_builder.cc      # Block 构建
│   ├── block_builder.h
│   ├── filter_block.cc       # Filter Block
│   ├── filter_block.h
│   ├── cuckoo_table_builder.cc  # Cuckoo Table
│   └── cuckoo_table_builder.h
│
├── util/                     # 工具库
│   ├── arena.cc             # 内存分配器
│   ├── arena.h
│   ├── bloom.cc             # Bloom Filter
│   ├── bloom.h
│   ├── cache.cc             # LRU Cache
│   ├── cache.h
│   ├── coding.cc            # 编码工具
│   ├── coding.h
│   ├── comparator.cc        # 比较器
│   ├── comparator.h
│   ├── crc32c.cc            # CRC32 校验
│   ├── crc32c.h
│   ├── env.cc               # 环境抽象
│   ├── env.h
│   ├── hash.cc              # 哈希函数
│   ├── hash.h
│   ├── rate_limiter.cc      # 速率限制
│   ├── rate_limiter.h
│   ├── statistics.cc        # 统计
│   ├── statistics.h
│   ├── threadpool.cc        # 线程池
│   └── threadpool.h
│
├── include/rocksdb/          # 公共 API
│   ├── db.h                # DB 接口
│   ├── options.h           # 配置选项
│   ├── write_batch.h       # WriteBatch
│   ├── iterator.h          # 迭代器
│   ├── status.h            # 状态码
│   ├── comparator.h        # 比较器
│   ├── env.h               # 环境抽象
│   ├── filter_policy.h     # Filter 策略
│   ├── slice.h             # Slice
│   ├── table.h             # Table 接口
│   ├── cache.h             # Cache 接口
│   ├── column_family.h     # 列族接口
│   ├── transaction_log.h   # 事务日志
│   ├── merge_operator.h    # Merge 操作符
│   ├── perf_level.h        # 性能级别
│   └── statistics.h        # 统计接口
│
├── utilities/                # 工具组件
│   ├── transactions/        # 事务支持
│   │   ├── transaction_db_impl.cc
│   │   ├── transaction_db_impl.h
│   │   ├── transactional_writer.cc
│   │   └── transactional_writer.h
│   ├── backup/              # 备份恢复
│   │   ├── backupable_db.cc
│   │   └── backupable_db.h
│   ├── blob_db/             # BlobDB
│   │   ├── blob_db.cc
│   │   └── blob_db.h
│   ├── memory/              # 内存数据库
│   │   └── memory_allocator.h
│   └── merge_operators/     # Merge 操作符
│       ├── string_append.cc
│       └── uint64add.cc
│
├── tools/                    # 工具
│   ├── db_bench.cc          # 基准测试工具
│   ├── ldb_tool.cc          # 管理工具
│   └── sst_dump.cc          # SSTable 转储工具
│
├── port/                     # 平台适配
│   ├── port.h
│   ├── port_posix.cc
│   └── port_posix.h
│
└── doc/                      # 文档
    ├── rocksdb_häagen-dazs.md
    └── rocksdb-design.md
```

## 关键源码文件

### 核心入口

| 文件 | 职责 | 关键函数 |
|------|------|---------|
| `db/db_impl.cc` | DB 主实现 | `Put()`, `Get()`, `Delete()`, `Write()` |
| `db/column_family.cc` | 列族管理 | `CreateColumnFamily()`, `DropColumnFamily()` |

### 写入路径

| 文件 | 职责 | 关键结构 |
|------|------|---------|
| `db/write_thread.cc` | 写入线程管理 | `WriteThread`, `JoinBatchGroup()` |
| `db/write_batch.cc` | 批量写入 | `WriteBatch`, `Put()`, `Delete()` |
| `db/memtable.cc` | MemTable | `MemTable`, `Add()`, `Get()` |

### Compaction

| 文件 | 职责 | 关键函数 |
|------|------|---------|
| `db/compaction_picker.cc` | Compaction 选择 | `PickCompaction()`, `PickL0Compaction()` |
| `db/compaction_job.cc` | Compaction 执行 | `Run()`, `ProcessSubcompaction()` |
| `db/flush_job.cc` | Flush 执行 | `Run()`, `WriteLevel0Table()` |

### Version 管理

| 文件 | 职责 | 关键结构 |
|------|------|---------|
| `db/version_set.cc` | Version 管理 | `VersionSet`, `Recover()`, `LogAndApply()` |
| `db/version_edit.cc` | Version 变更 | `VersionEdit`, `EncodeTo()`, `DecodeFrom()` |

### SSTable

| 文件 | 职责 | 关键结构 |
|------|------|---------|
| `table/block_based_table_builder.cc` | SSTable 构建 | `TableBuilder`, `Add()`, `Finish()` |
| `table/block_based_table_reader.cc` | SSTable 读取 | `Table::Open()`, `Table::NewIterator()` |
| `table/block.cc` | Block 处理 | `Block::Iter` |

### 事务

| 文件 | 职责 | 关键结构 |
|------|------|---------|
| `utilities/transactions/transaction_db_impl.cc` | 事务 DB | `TransactionDBImpl` |
| `utilities/transactions/transaction_impl.cc` | 事务实现 | `TransactionImpl` |

### 工具

| 文件 | 职责 | 用途 |
|------|------|------|
| `tools/db_bench.cc` | 基准测试 | 性能测试 |
| `tools/ldb_tool.cc` | 管理工具 | 数据库管理 |
| `tools/sst_dump.cc` | SSTable 转储 | SSTable 分析 |

## 源码阅读路径

### 写入路径

```
1. db_impl.cc: DBImpl::Put()           # 入口
2. write_thread.cc: WriteThread::JoinBatchGroup()  # 写入组
3. write_batch.cc: WriteBatch::Put()   # 批处理
4. db_impl.cc: DBImpl::WriteToWAL()   # WAL 写入
5. memtable.cc: MemTable::Add()        # MemTable 插入
6. flush_job.cc: FlushJob::Run()       # Flush 到 SSTable
```

### 读取路径

```
1. db_impl.cc: DBImpl::Get()           # 入口
2. column_family.cc: GetColumnFamily() # 获取列族
3. memtable.cc: MemTable::Get()        # MemTable 查找
4. version_set.cc: Version::Get()      # SSTable 查找
5. table/block_based_table_reader.cc: Get()  # SSTable 内部查找
6. table/block.cc: Block::Iter::Seek() # Block 二分查找
```

### Compaction 流程

```
1. compaction_picker.cc: PickCompaction()  # 选择任务
2. compaction_job.cc: Run()               # 执行合并
3. table/block_based_table_builder.cc: Add()  # 构建新 SSTable
4. version_set.cc: LogAndApply()          # 更新 Version
```

### 列族管理

```
1. column_family.cc: CreateColumnFamily()    # 创建
2. version_set.cc: LogAndApply()             # 持久化
3. column_family.cc: DropColumnFamily()      # 删除
4. version_set.cc: LogAndApply()             # 更新 Version
```

## 设计文档

### 官方文档

- **RocksDB 设计文档**: https://github.com/facebook/rocksdb/wiki/RocksDB-Basics
- **Wiki 目录**: https://github.com/facebook/rocksdb/wiki
- **性能调优**: https://github.com/facebook/rocksdb/wiki/Performance-Benchmarks

### 关键文章

1. **"RocksDB: A Persistent Key-Value Store for Flash and RAM Storage"**
   - 设计动机与架构
   - 与 LevelDB 的差异

2. **"The Evolution of RocksDB"**
   - 版本演进历史
   - 关键特性加入时间线

3. **"RocksDB Compaction Options"**
   - Compaction 策略详解
   - 参数调优指南

## 扩展学习

### 相关项目

| 项目 | 说明 |
|------|------|
| MyRocks | MySQL 的 RocksDB 存储引擎 |
| TiKV | 分布式 KV 数据库 |
| ArDB | AeroSpike 的 RocksDB 集成 |
| BlobDB | RocksDB 键值分离扩展 |

### 调试工具

```bash
# ldb 工具
./ldb --db=/tmp/testdb dump        # 转储所有 key
./ldb --db=/tmp/testdb get key     # 获取指定 key
./ldb --db=/tmp/testdb stats       # 数据库统计

# sst_dump 工具
./sst_dump --file=/path/to/sst --command=scan  # 扫描 SSTable
./sst_dump --file=/path/to/sst --command=verify # 验证 SSTable

# 性能分析
./db_bench --benchmarks=fillseq --num=1000000
```

## 要点总结

- **源码入口**：`db/db_impl.cc` 是核心
- **列族管理**：`column_family.cc` 是 RocksDB 特有
- **Compaction**：`compaction_job.cc` + `compaction_picker.cc`
- **事务**：`utilities/transactions/` 目录
- **工具**：`db_bench` 基准测试，`ldb` 管理工具

## 思考题

1. RocksDB 的源码结构比 LevelDB 复杂多少？新增了哪些核心模块？
2. 为什么 RocksDB 需要独立的 `compaction_picker.cc` 和 `compaction_job.cc`？
3. 从 LevelDB 到 RocksDB，Facebook 做了哪些关键改进？