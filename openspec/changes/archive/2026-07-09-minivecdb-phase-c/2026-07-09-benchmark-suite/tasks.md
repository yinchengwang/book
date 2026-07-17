# C1: 基准测试 - 任务清单

## 任务概述

实现基准测试套件，验证 MiniVecDB 性能指标。

## 任务清单

### A. 向量操作基准测试

- [x] **A.1** 向量插入基准测试 (BulkInsert)
- [x] **A.2** 向量搜索基准测试 (Search)
- [x] **A.3** 吞吐量测试 (Throughput)

### B. 代价模型基准测试

- [x] **B.1** HNSW 代价估算测试
- [x] **B.2** IVF 代价估算测试
- [x] **B.3** DiskANN 代价估算测试
- [x] **B.4** 最优索引选择测试

### C. 分片基准测试

- [x] **C.1** 一致性哈希性能测试
- [x] **C.2** 跨分片结果合并测试

## 基准测试结果

### 代价模型测试
```
num_vectors=1000000, top_k=10
HNSW cost:   1382.55
IVF cost:     39073.50
DiskANN cost: 401.00
Best index:   DiskANN
```

### 一致性哈希测试
```
vectors=10000, time=0.00 ms
Hash mean: 9.11e+18
```

## 文件清单

```
test/db/benchmark/
├── CMakeLists.txt
└── vector_benchmark.cpp
```

## 状态

- 创建时间: 2026-07-09
- 状态: 已完成
- 所属大变更: 2026-07-09-minivecdb-end-to-end
