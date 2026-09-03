# 模态追赶进度表

## Phase 0: 基础设施修复

| Task | 描述 | 状态 | 提交 |
|------|------|------|------|
| T1 | 统一错误码 storage_result_t | ✅ | 4ad7e32 |
| T2 | Lock Manager 条件变量 | ✅ | e754965 |
| T3 | Page CRC32 校验 | ✅ | 919cf47 |
| T4 | Buffer Pool 真实磁盘IO | ✅ | 6447a73 |
| T5 | WAL 变长LSN + redo/undo | ✅ | 3dde69a |
| T6 | Catalog 持久化 + hash查找 | ✅ | f7b2bca |
| T7 | MVCC 快照隔离 | ✅ | 9b66646 |

## Phase 1: 数据破坏性 Bug 修复

| Task | 描述 | 状态 | 提交 |
|------|------|------|------|
| T8 | Spatial 数据破坏修复 | ✅ | 4319c2b |
| T9 | Graph CSR 指针失效 + 反向索引 | ✅ | b02bd9f |
| T10 | Yang NULL 处理 | ✅ | 2ad044b |
| T11 | Timeseries Partition + Tag Index | ✅ | - |

## Phase 2A: 存储引擎补完

| Task | 描述 | 状态 | 提交 |
|------|------|------|------|
| T12 | KV LSM flush + skip list | ✅ | fb87c7e |
| T13 | Columnar 持久化 + 压缩 | ✅ | e684968 |
| T14 | Blob 代码去重 + multipart修复 | ✅ | 384d5de |
| T15 | Document get + inverted index | ✅ | 65c02c9 |
| T16 | Vector top-k heap + persist | ✅ | 9374bd9 |

## Phase 2B: 索引/查询补完

| Task | 描述 | 状态 | 提交 |
|------|------|------|------|
| T17 | RDF 索引激活 + 安全操作 | ✅ | dd3cc8f |
| T18 | Relational create + crash recovery | ✅ | 38c6286 |
| T19 | Log aggregate 真实计算 | ✅ | 7a725c6 |
| T20 | Stream 持久化修复 | ✅ | 918a916 |

## Phase 2C: 持久化修复

| Task | 描述 | 状态 | 提交 |
|------|------|------|------|
| T21 | CF stats + iter 优化 | ✅ | 1f01382 |
| T22 | ST R-Tree 索引集成 | ✅ | 7f61ebc |
| T23 | Sparse 线程安全 + hash map | ✅ | 6a63e77 |
| T24 | MMView refresh + cycle detection | ✅ | c770c7a |
| T25 | Multimodal 联合搜索 RRF | ✅ | ad15025 |
| T26 | Integrity 真实校验 | ✅ | 3ccac2d |

## Phase 3: 性能优化

| Task | 描述 | 状态 | 提交 |
|------|------|------|------|
| T27 | KV skip list | ✅ 已在T12完成 | fb87c7e |
| T28 | BM25 hash map | ✅ 已在T23完成 | 6a63e77 |
| T29 | Sparse top-k heap | ✅ | 470aa42 |
| T30 | ST kNN + R-Tree | ✅ 已在T22完成 | 7f61ebc |
| T31 | CF stats + iter | ✅ 已在T21完成 | 1f01382 |
| T32 | Catalog hash table | ✅ 已在T6完成 | f7b2bca |
| T33 | BM25 thread safety | ✅ 已在T23完成 | 6a63e77 |
| T34 | Graph CSR rwlock | ✅ | 9b66646 |
| T35 | Columnar compression | ✅ 已在T13完成 | e684968 |
| T36 | Timeseries Gorilla encoding | ✅ | f49b699 |

## Phase 4: 新增特性

| Task | 描述 | 状态 | 提交 |
|------|------|------|------|
| T37 | 跨模态事务 (2PC) | ✅ | 93d9ed4 |
| T38 | 物化视图完整实现 | ✅ | afb47c9 |
| T39 | RDF SPARQL 增强 | ✅ | 17a0716 |

## 测试文件

- `tests/test_lock.c` - Lock Manager
- `tests/test_page.c` - Page
- `tests/test_bufmgr.c` - Buffer Pool
- `tests/test_wal.c` - WAL
- `tests/test_catalog.c` - Catalog
- `tests/test_spatial.c` - Spatial
- `tests/test_graph.c` - Graph
- `tests/test_tree_yang.c` - Yang Tree
- `tests/test_ts.c` - Timeseries
- `tests/test_kv.c` - KV
- `tests/test_vector.c` - Vector
- `tests/test_document.c` - Document
- `tests/test_relational.c` - Relational
- `tests/test_sparse.c` - Sparse (BM25 + hybrid retrieval)

## 编译说明

所有测试使用 gcc/g++ 手动编译（CMake 有预存构建错误）。

```bash
# 编译 C 源文件
gcc -c src/db/storage/<modality>/<source>.c -I include -I include/db -o <output>.o

# 编译 C++ 测试文件
g++ -std=c++14 -c tests/test_<name>.c -I include -I include/db -I third_part/googletest/googletest/include -o tests/test_<name>.o

# 编译 GTest (只需一次)
g++ -std=c++14 -c third_part/googletest/googletest/src/gtest-all.cc -I third_part/googletest/googletest/include -I third_part/googletest/googletest -o gtest-all.o
g++ -std=c++14 -c third_part/googletest/googlemock/src/gmock-all.cc -I third_part/googletest/googletest/include -I third_part/googletest/googletest -I third_part/googletest/googlemock/include -I third_part/googletest/googlemock -o gmock-all.o

# 链接
g++ -std=c++14 -o tests/test_<name>.exe tests/test_<name>.o <sources> gtest-all.o gmock-all.o -lpthread
```
