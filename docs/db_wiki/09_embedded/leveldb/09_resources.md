# LevelDB 学习资源

## 学习目标

- 掌握 LevelDB 源码目录结构
- 了解关键源码文件
- 建立源码阅读路径

## 源码路径

### 项目仓库

```
reference/open-source/leveldb/  # 源码镜像（非 CMake 构建）
```

### 目录结构

```
leveldb/
├── db/                      # 核心实现
│   ├── db_impl.cc          # DB 主实现
│   ├── db_impl.h
│   ├── memtable.cc         # MemTable (SkipList)
│   ├── memtable.h
│   ├── skip_list.h         # SkipList 实现
│   ├── table_cache.cc      # SSTable 缓存
│   ├── table_cache.h
│   ├── version_set.cc      # Version 管理
│   ├── version_set.h
│   ├── version_edit.cc     # Version 变更
│   ├── version_edit.h
│   ├── builder.cc          # SSTable 构建
│   ├── builder.h
│   ├── filename.cc         # 文件命名
│   ├── filename.h
│   ├── log_reader.cc       # WAL 日志读取
│   ├── log_reader.h
│   ├── log_writer.cc       # WAL 日志写入
│   ├── log_writer.h
│   ├── write_batch.cc      # WriteBatch 实现
│   └── write_batch.h
│
├── table/                  # SSTable 格式
│   ├── table.cc           # SSTable 读取
│   ├── table.h
│   ├── builder.cc         # SSTable 构建
│   ├── builder.h
│   ├── block.cc           # Data Block
│   ├── block.h
│   ├── block_builder.cc   # Block 构建
│   ├── block_builder.h
│   ├── filter_block.cc    # Bloom Filter Block
│   ├── filter_block.h
│   ├── two_level_iterator.cc  # 两层迭代器
│   └── two_level_iterator.h
│
├── util/                   # 工具库
│   ├── arena.cc           # 内存分配器
│   ├── arena.h
│   ├── bloom.cc           # Bloom Filter
│   ├── bloom.h
│   ├── cache.cc           # LRU Cache
│   ├── cache.h
│   ├── coding.cc          # 编码工具
│   ├── coding.h
│   ├── comparator.cc      # 比较器
│   ├── comparator.h
│   ├── crc32c.cc          # CRC32 校验
│   ├── crc32c.h
│   ├── env.cc             # 环境抽象
│   ├── env.h
│   ├── hash.cc            # 哈希函数
│   ├── hash.h
│   ├── mutexlock.h        # 锁工具
│   ├── options.cc         # 选项实现
│   ├── options.h
│   ├── status.cc          # 状态码
│   ├── status.h
│   ├── slice.h            # Slice 工具
│   └── testutil.h         # 测试工具
│
├── include/leveldb/        # 公共 API
│   ├── db.h              # DB 接口
│   ├── options.h         # 配置选项
│   ├── write_batch.h     # WriteBatch
│   ├── iterator.h        # 迭代器
│   ├── status.h          # 状态码
│   ├── comparator.h      # 比较器
│   ├── env.h             # 环境抽象
│   ├── filter_policy.h   # Filter 策略
│   ├── slice.h           # Slice
│   ├── table.h           # Table 接口
│   └── cache.h           # Cache 接口
│
├── port/                   # 平台适配
│   ├── port.h
│   ├── port_posix.cc
│   └── port_posix.h
│
└── doc/                    # 文档
    ├── index.md
    ├── impl.md
    └── table_format.md
```

## 关键源码文件

### 核心入口

| 文件 | 职责 | 关键函数 |
|------|------|---------|
| `db/db_impl.cc` | DB 主实现 | `Put()`, `Get()`, `Delete()`, `Write()` |
| `db/db_impl.h` | DB 结构体 | `DBImpl` 类定义 |

### LSM 管理

| 文件 | 职责 | 关键结构 |
|------|------|---------|
| `db/version_set.cc` | Version 管理 | `VersionSet`, `Recover()`, `PickCompaction()` |
| `db/version_edit.cc` | Version 变更 | `VersionEdit`, `EncodeTo()`, `DecodeFrom()` |
| `db/builder.cc` | SSTable 构建 | `BuildTable()` |

### Compaction

| 文件 | 职责 | 关键函数 |
|------|------|---------|
| `db/version_set.cc` | Compaction 选择 | `PickCompaction()`, `SetupOtherInputs()` |
| `db/db_impl.cc` | Compaction 执行 | `BackgroundCall()`, `BackgroundCompaction()` |

### MemTable

| 文件 | 职责 | 关键函数 |
|------|------|---------|
| `db/memtable.cc` | MemTable 实现 | `Add()`, `Get()`, `Ref()`, `Unref()` |
| `db/skip_list.h` | SkipList 实现 | `Insert()`, `Contains()`, `Iterator` |

### SSTable

| 文件 | 职责 | 关键结构 |
|------|------|---------|
| `table/table.cc` | SSTable 读取 | `Table::Open()`, `Table::NewIterator()` |
| `table/builder.cc` | SSTable 构建 | `TableBuilder::Add()`, `Finish()` |
| `table/block.cc` | Block 处理 | `Block::Iter` |
| `table/filter_block.cc` | Bloom Filter | `FilterBlockBuilder`, `FilterBlockReader` |

### WAL

| 文件 | 职责 | 关键结构 |
|------|------|---------|
| `db/log_writer.cc` | WAL 写入 | `Writer::AddRecord()` |
| `db/log_reader.cc` | WAL 读取 | `Reader::ReadRecord()` |

### WriteBatch

| 文件 | 职责 | 关键函数 |
|------|------|---------|
| `db/write_batch.cc` | 批量写入 | `Put()`, `Delete()`, `Iterate()` |

### 工具

| 文件 | 职责 | 关键结构 |
|------|------|---------|
| `util/arena.cc` | 内存分配 | `Arena`, `Allocate()` |
| `util/bloom.cc` | Bloom Filter | `BloomFilterPolicy` |
| `util/cache.cc` | LRU Cache | `LRUCache`, `ShardedLRUCache` |

## 源码阅读路径

### 写入路径

```
1. db_impl.cc: DBImpl::Put()           # 入口
2. db_impl.cc: DBImpl::Write()         # 写入调度
3. write_batch.cc: WriteBatch::Put()   # 批处理
4. log_writer.cc: Writer::AddRecord()  # WAL 写入
5. memtable.cc: MemTable::Add()        # MemTable 插入
6. skip_list.h: SkipList::Insert()     # SkipList 操作
```

### 读取路径

```
1. db_impl.cc: DBImpl::Get()           # 入口
2. memtable.cc: MemTable::Get()        # MemTable 查找
3. version_set.cc: Version::Get()      # SSTable 查找
4. table_cache.cc: TableCache::Get()   # 缓存命中/未命中
5. table/table.cc: Table::InternalGet() # SSTable 内部查找
6. table/block.cc: Block::Iter::Seek() # Block 二分查找
```

### Compaction 流程

```
1. db_impl.cc: BackgroundCompaction()  # 后台压缩
2. version_set.cc: PickCompaction()    # 选择压缩任务
3. version_set.cc: DoCompactionWork()  # 执行合并
4. builder.cc: BuildTable()            # 构建新 SSTable
5. version_set.cc: InstallCompactionResults()  # 更新 Version
```

### 恢复流程

```
1. db_impl.cc: DBImpl::Recover()       # 恢复入口
2. version_set.cc: VersionSet::Recover() # 读取 Manifest
3. log_reader.cc: Reader::ReadRecord() # 重放 WAL
4. builder.cc: BuildTable()            # 构建恢复后的 SSTable
```

## 设计文档

### 官方文档

- **实现细节**: https://github.com/google/leveldb/blob/main/doc/impl.md
- **SSTable 格式**: https://github.com/google/leveldb/blob/main/doc/table_format.md
- **Benchmark**: https://github.com/google/leveldb/blob/main/doc/benchmark.md

### 关键文章

1. **"LevelDB 实现解析"**
   - MemTable、Log、SSTable 实现
   - Compaction 策略分析

2. **"Bigtable: A Distributed Storage System for Structured Data"**
   - LevelDB 的设计思想来源
   - SSTable 和 MemTable 的起源

3. **"The Log-Structured Merge-Tree"** (O'Neil et al. 1996)
   - LSM-Tree 理论基础
   - 写入放大和读取放大分析

## 扩展学习

### 相关项目

| 项目 | 说明 |
|------|------|
| HyperLevelDB | LevelDB 的并发改进版本 |
| Basho LevelDB | Riak 数据库使用的分支 |
| RocksDB | Facebook 的 LevelDB 衍生版本 |

### 调试工具

```bash
# 使用 ldb 工具
./build/ldb --db=/tmp/testdb dump  # 转储所有 key
./build/ldb --db=/tmp/testdb get key  # 获取指定 key
./build/ldb --db=/tmp/testdb stats  # 数据库统计
```

## 要点总结

- **源码入口**：`db/db_impl.cc` 是核心
- **LSM 核心**：`version_set.cc` + `memtable.cc`
- **SSTable**：`table/` 目录完整实现
- **WAL 恢复**：`log_reader.cc` + `log_writer.cc`
- **工具链**：`util/` 目录包含 Bloom/Cache/Arena

## 思考题

1. LevelDB 的源码为什么只有 3 万行？其简洁性体现在哪些设计选择？
2. 为什么 LevelDB 选择 SkipList 而不是 B+ Tree 作为 MemTable？
3. 从 LevelDB 到 RocksDB，Facebook 做了哪些关键改进？