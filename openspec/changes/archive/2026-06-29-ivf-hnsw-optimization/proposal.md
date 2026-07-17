# IVF-HNSW 优化提案

## Why

当前 IVF 索引的粗排阶段（找最近的 nprobe 个一级簇）使用暴力扫描，复杂度为 O(nlist × dims)。当 nlist 较大（如 10000+）时，这一开销成为搜索瓶颈。同时，IVF 的桶边缘效应导致部分向量在搜索时被遗漏，影响召回率。

HNSW-IVF 通过在聚类中心上构建 HNSW 图实现 O(log nlist) 的粗排，配合 Multi-assignment 多桶分配策略，可显著提升搜索精度和召回率。

## What Changes

### 1. 新增 HNSW-IVF 索引类型

- 在一级聚类中心上构建 HNSW 图，替代暴力扫描进行粗排
- 将粗排复杂度从 O(nlist × dims) 降低到 O(log nlist × M)
- 精度不降：HNSW 搜索能精确找到最近的 nprobe 个中心

### 2. 新增 Multi-assignment 支持

- 每个向量可分配到最近的 k 个桶（k 由用户配置，默认 1）
- 有效缓解桶边缘效应，提升召回率
- 内存开销：每向量额外存储 k-1 个 (bucket_id, offset) 对

### 3. 统一的参数配置接口

- 所有参数（nlist、nprobe、k_assignments、M、ef_search 等）均可配置
- 不在代码中硬编码任何魔法值
- 提供完整的 getter/setter 接口

## Capabilities

### New Capabilities

- `ivf-hnsw-index`: HNSW-IVF 复合索引实现
  - 基于现有 IVF 架构，在一级中心上构建 HNSW 图
  - 搜索时用 HNSW 替代暴力粗排
  - 支持与 PQ/SQ/LVQ/RQ 量化组合使用

- `ivf-multi-assignment`: IVF Multi-assignment 支持
  - 每个向量记录分配到的 k 个桶
  - 搜索时展开所有分配的桶
  - 可与 HNSW-IVF 或纯 IVF 组合使用

- `ivf-hnsw-benchmark`: HNSW-IVF 性能基准测试
  - 精度测试：recall@k 对比
  - 性能测试：QPS、延迟对比
  - 内存测试：索引大小对比

### Modified Capabilities

- `vdb-core-index-algorithms`: 扩展覆盖 HNSW-IVF 复合索引内容
  - 新增 `db-hnsw-ivf` 知识点

## Impact

### 受影响代码

- **新增目录**: `include/index/vector_index/ivf_hnsw/`、`src/index/vector_index/ivf_hnsw/`
- **新增文件**: IndexIVFHNSW.h、faiss_ivf_hnsw_*.c 等
- **测试文件**: `test/vector_index/ivf_hnsw/ivf_hnsw_test.cpp`

### 依赖关系

```
ivf_hnsw ──→ ivf ──→ algo (distance, quantization, kmeans)
          └───→ project_includes (头文件)
```

### API 兼容性

- 新增 API，不影响现有 IVF/HNSW 接口
- 提供统一的索引工厂模式创建不同类型索引
