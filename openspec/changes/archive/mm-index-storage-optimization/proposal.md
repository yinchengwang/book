# 多模态数据库索引存储优化

## Why

当前多模态数据库的各个索引（向量、图、时序、空间、文档）的存储方式与业界领先产品（如 Milvus、Neo4j、TimescaleDB、PostGIS、Elasticsearch）存在显著差距。主要问题包括：

1. **向量索引**：缺少分页管理、增量持久化、量化压缩、内存映射等业界标准特性
2. **图索引**：缺乏关系类型索引、CSR 存储格式、空间填充曲线等优化
3. **时序索引**：时间范围索引缺失、物化聚合视图不完整
4. **空间索引**：R-Tree 无持久化、不支持 OGC 几何标准、无 Hilbert 曲线
5. **文档索引**：使用线性扫描而非倒排索引、缺少全文搜索能力

这些问题导致当前实现在大规模数据场景下性能低下，无法满足生产环境需求。

## What Changes

### 向量索引存储优化
- 实现向量数据的分页管理（基于 4KB/8KB/16KB 页面的 VecPage 模块）
- 支持 HNSW 索引增量持久化（自动增量保存，而非全量重建）
- 添加 PQ/OPQ 量化压缩支持（10-50x 压缩比）
- 实现 mmap 内存映射支持（减少内存拷贝）
- 添加 IVF 分区索引支持（支持 DiskANN 风格分区）

### 图索引存储优化
- 实现 CSR（Compressed Sparse Row）边存储格式
- 添加关系类型索引（边类型 + 属性索引）
- 实现空间填充曲线（Hilbert/Z-Order）加速范围查询
- 添加顶点标签索引（Label Index）

### 时序索引存储优化
- 实现时间范围分段索引（Segmented Time Index）
- 添加物化聚合视图支持（预计算聚合结果）
- 优化 Gorilla 压缩算法（Delta-of-Delta 时间戳编码）
- 实现 TTL 保留策略的自动清理

### 空间索引存储优化
- 实现 R-Tree 持久化存储格式（对齐 PostgreSQL GiST）
- 添加 OGC 几何类型支持（LineString、Polygon、MultiPoint）
- 实现 Hilbert 曲线索引（加速 KNN 查询）
- 添加空间查询优化器（使用 R-Tree 剪枝）

### 文档索引存储优化
- 实现倒排索引存储（替代线性扫描）
- 添加 BM25 全文打分算法
- 实现 IK/ANSJ 分词器集成接口
- 添加文档更新增量索引

### 通用存储优化
- 统一分页管理层（Page Cache）
- 实现 WAL（Write-Ahead Logging）持久化
- 添加统计信息收集（INDEX_STATS）

## Capabilities

### New Capabilities

- `vector-page-storage`: 向量分页存储管理，支持内存池和 mmap
- `vector-pq-compression`: 向量量化压缩，支持 PQ/OPQ 算法
- `graph-csr-storage`: 图 CSR 存储格式，优化边遍历性能
- `graph-hilbert-index`: 图 Hilbert 曲线索引，加速空间查询
- `ts-segmented-index`: 时序分段索引，按时间范围组织数据
- `ts-materialized-view`: 时序物化视图，支持预计算聚合
- `rtree-persistence`: R-Tree 持久化存储格式
- `rtree-hilbert-curve`: R-Tree Hilbert 曲线优化
- `doc-inverted-index`: 文档倒排索引存储
- `doc-bm25-scorer`: 文档 BM25 打分算法

### Modified Capabilities

- `vector-hnsw-index`: 扩展现有 HNSW 索引，支持增量持久化和量化
- `ts-compression`: 优化 Gorilla 压缩，添加 Delta-of-Delta 编码
- `spatial-engine`: 扩展空间引擎，支持 OGC 几何标准

## Impact

### 受影响模块

| 模块 | 影响说明 |
|------|---------|
| `src/db/storage/vector/` | 新增 vec_page.c/h、量化压缩模块 |
| `src/db/storage/graph/` | 新增 graph_csr.c/h、hilbert.c/h |
| `src/db/storage/ts/` | 新增 ts_segment.c/h、ts_mview.c/h |
| `src/db/storage/spatial/` | 新增 rtree_persist.c/h、hilbert.c/h |
| `src/db/storage/doc/` | 新增 doc_inverted.c/h、bm25.c/h |
| `include/db/storage/` | 相应头文件更新 |

### 依赖关系

- 向量量化需要 `algo/quantization/` 模块支持
- 图 Hilbert 曲线需要数学库支持
- 文档分词需要 `algo/tokenizer/` 模块接口

### 测试影响

- 新增模块需要完整的单元测试
- 现有向量/图/时序测试需要适配新的存储格式
