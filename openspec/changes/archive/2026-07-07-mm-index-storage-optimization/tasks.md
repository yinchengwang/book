# 多模态数据库索引存储优化任务清单

## 1. 向量分页存储 (vector-page-storage)

- [x] 1.1 创建 `include/db/storage/vector/vec_page.h` 头文件，定义 VecPage API ✅
- [x] 1.2 创建 `src/db/storage/vector/vec_page.c` 实现文件 ✅
- [x] 1.3 实现 `vector_page_pool_create()` 创建页池 ✅
- [x] 1.4 实现 `vector_page_append()` 追加向量到页池 ✅
- [x] 1.5 实现 Clock-Sweep 页面置换算法 ✅
- [x] 1.6 实现页面刷盘 `vector_page_flush()` ✅
- [ ] 1.7 实现 mmap 内存映射支持（待增强）
- [ ] 1.8 实现页面完整性校验（magic + checksum）（待增强）
- [ ] 1.9 添加单元测试（待添加）
- [x] 1.10 更新 CMakeLists.txt 添加 vec_page.c ✅

## 2. 向量量化压缩 (vector-pq-compression)

- [x] 2.1 创建 `include/algo/quantization/pq.h` PQ 量化器头文件 ✅
- [x] 2.2 创建 `src/algo/quantization/pq.c` PQ 量化器实现 ✅
- [x] 2.3 实现 K-Means 码书训练算法 ✅
- [x] 2.4 实现向量 PQ 编码和解码 ✅
- [x] 2.5 实现 ADIS (Asymmetric Distance Computation) 距离计算 ✅
- [ ] 2.6 实现 OPQ (Optimized PQ) 旋转优化（待实现）
- [ ] 2.7 实现 IVF-PQ 复合索引（待实现）
- [ ] 2.8 实现 PQ 索引文件序列化/反序列化（待实现）
- [ ] 2.9 添加单元测试和性能基准（待添加）

## 3. 图 CSR 存储 (graph-csr-storage)

- [x] 3.1 创建 `include/db/storage/graph/graph_csr.h` 头文件 ✅
- [x] 3.2 创建 `src/db/storage/graph/graph_csr.c` 实现文件 ✅
- [x] 3.3 实现 CSR 顶点存储（vertices.bin 格式） ✅
- [x] 3.4 实现 CSR 边存储（edges.bin 格式） ✅
- [x] 3.5 实现 COO 增量缓冲区 ✅
- [x] 3.6 实现 CSR + COO 合并算法 ✅
- [x] 3.7 实现顶点标签索引（labels.bin） ✅
- [x] 3.8 实现 O(1) 出边查询 `graph_csr_get_out_edges()` ✅
- [ ] 3.9 实现反向边索引支持入边查询（待完善）
- [ ] 3.10 添加单元测试（待添加）

## 4. 图 Hilbert 曲线索引 (graph-hilbert-index)

- [x] 4.1 创建 `include/db/index/hilbert.h` Hilbert 曲线头文件 ✅
- [x] 4.2 创建 `src/db/index/hilbert.c` Hilbert 曲线实现 ✅
- [x] 4.3 实现 `hilbert_encode()` 2D 转 1D 编码 ✅
- [x] 4.4 实现 `hilbert_decode()` 1D 转 2D 解码 ✅
- [x] 4.5 实现动态 Hilbert 阶数选择 ✅
- [x] 4.6 实现 Hilbert 辅助索引构建 ✅
- [x] 4.7 实现空间范围查询优化 ✅
- [x] 4.8 实现空间 KNN 查询优化 ✅
- [x] 4.9 添加 Hilbert 索引文件序列化 ✅
- [x] 4.10 添加 Hilbert 索引单元测试 ✅

## 5. 时序分段索引 (ts-segmented-index)

- [x] 5.1 创建 `include/db/storage/ts/ts_segment.h` 头文件 ✅
- [x] 5.2 创建 `src/db/storage/ts/ts_segment.c` 实现文件 ✅
- [x] 5.3 实现分段索引 `ts_segment_index_create()` ✅
- [x] 5.4 实现 `ts_segment_append()` 追加数据点 ✅
- [x] 5.5 实现段自动切换逻辑 ✅
- [x] 5.6 实现段稀疏索引（二分查找） ✅
- [x] 5.7 实现时间范围查询 `ts_segment_query()` ✅
- [x] 5.8 实现段压缩集成（调用 ts_compress） ✅
- [x] 5.9 实现 TTL 过期数据清理 ✅
- [x] 5.10 添加时序分段索引单元测试 ✅

## 6. 时序物化视图 (ts-materialized-view)

- [x] 6.1 创建 `include/db/storage/ts/ts_mview.h` 头文件 ✅
- [x] 6.2 创建 `src/db/storage/ts/ts_mview.c` 实现文件 ✅
- [x] 6.3 实现 `ts_mview_create()` 创建物化视图 ✅
- [x] 6.4 实现 AVG/MIN/MAX/COUNT 聚合计算 ✅
- [x] 6.5 实现 `ts_mview_refresh()` 刷新物化视图 ✅
- [x] 6.6 实现 `ts_mview_query()` 查询物化数据 ✅
- [x] 6.7 实现百分位（P50/P95/P99）物化视图 ✅
- [x] 6.8 实现自动后台刷新机制 ✅
- [x] 6.9 实现物化视图持久化 ✅
- [x] 6.10 添加物化视图单元测试 ✅

## 7. R-Tree 持久化 (rtree-persistence)

- [x] 7.1 创建 `include/db/storage/spatial/rtree_file.h` 头文件 ✅
- [x] 7.2 创建 `src/db/storage/spatial/rtree_file.c` 实现文件 ✅
- [x] 7.3 实现 R-Tree 文件头格式（64 字节） ✅
- [x] 7.4 实现页面化节点存储（4KB 页面） ✅
- [x] 7.5 实现节点序列化（内部节点 + 叶子节点） ✅
- [x] 7.6 实现 `rtree_save()` 持久化 ✅
- [x] 7.7 实现 `rtree_load()` 延迟加载 ✅
- [x] 7.8 实现页面缓存和 LRU 淘汰 ✅
- [x] 7.9 实现文件损坏检测和恢复 ✅
- [x] 7.10 添加 R-Tree 持久化单元测试 ✅

## 8. R-Tree Hilbert 曲线优化 (rtree-hilbert-curve)

- [x] 8.1 复用 `include/db/index/hilbert.h` Hilbert 曲线 ✅
- [x] 8.2 实现 Hilbert 辅助索引构建 ✅
- [x] 8.3 实现 bbox Hilbert 码计算 ✅
- [x] 8.4 实现 Hilbert 排序插入优化 ✅
- [x] 8.5 实现 Hilbert 辅助索引构建 ✅
- [x] 8.6 实现基于 Hilbert 的 KNN 查询 ✅
- [x] 8.7 实现 Hilbert 螺旋搜索算法 ✅
- [x] 8.8 实现 Hilbert 索引持久化 ✅
- [x] 8.9 实现动态阶数选择 ✅
- [ ] 8.10 添加单元测试（待添加）

## 9. 文档倒排索引 (doc-inverted-index)

- [x] 9.1 创建 `include/db/storage/doc/doc_inverted.h` 头文件 ✅
- [x] 9.2 创建 `src/db/storage/doc/doc_inverted.c` 实现文件 ✅
- [x] 9.3 实现 FST 术语字典（简化版哈希字典） ✅
- [x] 9.4 实现术语查找 `dict_lookup()` ✅
- [x] 9.5 实现前缀查询 `dict_prefix_search()` ✅
- [x] 9.6 实现倒排列表存储 ✅
- [x] 9.7 实现增量编码压缩 ✅
- [x] 9.8 实现 `doc_inverted_index_add()` 索引文档 ✅
- [ ] 9.9 实现墓碑机制和合并清理（待完善）
- [ ] 9.10 添加单元测试（待添加）

## 10. 文档 BM25 打分 (doc-bm25-scorer)

- [x] 10.1 创建 `include/db/storage/doc/bm25.h` 头文件 ✅
- [x] 10.2 创建 `src/db/storage/doc/bm25.c` 实现文件 ✅
- [x] 10.3 实现 BM25 公式 `bm25_score()` ✅
- [x] 10.4 实现 IDF 计算（改进公式） ✅
- [x] 10.5 实现文档长度正规化 ✅
- [x] 10.6 实现 `bm25_batch_score()` 批量打分 ✅
- [x] 10.7 实现 BM25F 字段加权打分 ✅
- [x] 10.8 实现 BM25+ 变体 ✅
- [x] 10.9 实现 IDF 缓存机制 ✅
- [ ] 10.10 添加单元测试和性能基准（待添加）

## 11. 集成与测试

- [x] 11.1 更新 `vector_engine.c` 集成 VecPage 存储 ✅
- [x] 11.2 更新 `vector_engine.c` 集成 PQ 压缩 ✅
- [x] 11.3 更新 `graph_engine.c` 集成 CSR 存储 ✅
- [x] 11.4 更新 `ts_engine.c` 集成分段索引 ✅
- [x] 11.5 更新 `spatial_engine.c` 集成 R-Tree 持久化和 Hilbert 索引 ✅
- [x] 11.6 更新 `doc_engine.c` 集成倒排索引 ✅
- [x] 11.7 创建 Hilbert 和空间引擎集成测试用例 ✅
- [x] 11.8 更新 CMakeLists.txt 添加 Hilbert 模块依赖 ✅
- [x] 11.9 性能基准测试 ✅
- [x] 11.10 文档更新 ✅

## 12. 文档与示例

- [x] 12.1 更新 docs/storage-architecture.md 描述新存储格式 ✅
- [x] 12.2 创建 docs/vector-storage.md 向量存储使用指南 ✅
- [x] 12.3 创建 docs/graph-storage.md 图存储使用指南 ✅
- [x] 12.4 创建 docs/timeseries-storage.md 时序存储使用指南 ✅
- [x] 12.5 创建 docs/spatial-storage.md 空间存储使用指南 ✅
- [x] 12.6 创建 docs/doc-storage.md 文档存储使用指南 ✅
