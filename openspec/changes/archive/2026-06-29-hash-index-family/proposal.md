# Hash 索引族实现

## Why

C/C++ 算法与数据结构练习项目目前已实现 Linear Hash（pg_hash）和 Extendible Hash（cceh）两种经典哈希索引，但缺少现代数据库广泛使用的 Filter 类索引（Bloom、Cuckoo、Xor）和分布式场景必备的 Consistent Hash。作为向量数据库学习项目的重要组成部分，这些索引不仅是面试高频考点，更是理解 RocksDB、LevelDB、MySQL InnoDB 等工业级存储系统的基础。

## What Changes

本次变更将实现完整的哈希索引族，包括：

1. **Bloom Filter** — 标准布隆过滤器，支持添加和查询操作
2. **Cuckoo Filter** — 支持删除的布隆过滤器替代方案
3. **Xor Filter** — 比 Bloom 更紧凑的过滤器
4. **Consistent Hash** — 将现有的 demo 实现升级为完整的生产级一致性哈希

## Capabilities

### New Capabilities

- `bloom-filter`: 标准布隆过滤器，支持 add/query，存在性判断可能有假阳性但无假阴性
- `cuckoo-filter`: Cuckoo 哈希过滤器，支持 add/query/delete，空间效率与 Bloom 相当但支持删除
- `xor-filter`: Xor Filter，比 Bloom 节省约 20-30% 空间，基于 XOR 操作构造
- `consistent-hash`: 一致性哈希环，支持虚拟节点（vnode）、节点增删、负载均衡

### Modified Capabilities

本次变更为纯新增，无修改现有能力规格。

## Impact

### 目录结构

```
src/index/hash/
├── hash/           # Linear Hash（已有）
├── cceh/           # Extendible Hash（已有）
├── bloom/          # 新增：Bloom Filter
└── cuckoo/         # 新增：Cuckoo Filter

src/algo/ds/
├── distributed_index.c  # 升级为完整实现
└── distributed_index.h  # 添加新 API
```

### 头文件

```
include/index/hash/bloom/bloom.h
include/index/hash/cuckoo/cuckoo.h
include/algo/ds/distributed_index.h  # 扩展 API
```

### 测试

```
test/vector_index/hash/
├── CMakeLists.txt  # 添加新测试
└── bloom_test.cpp
└── cuckoo_test.cpp
```