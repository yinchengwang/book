# 向量数据库与向量搜索库综合对比（2026 版）

> 本文档汇总了截至 2026 年的向量数据库和向量搜索库技术规格与性能对比。
> 侧重具体参数与数据，不涉及营销宣传。
>
> **数据来源**: 各项目官方 GitHub 仓库、ann-benchmarks.com、社区基准测试

---

## 一、商业产品对比

### 1. Milvus（Zilliz）

| 维度 | 规格 |
|------|------|
| **最新版本** | v3.0.0 (2026-07-29) |
| **语言** | Go |
| **许可证** | Apache 2.0 |
| **最大维度** | 32,768（float/binary）；稀疏向量无固定上限 |
| **索引类型** | FLAT, IVF_FLAT, IVF_SQ8, IVF_PQ, HNSW, SCANN, DISKANN, GPU_IVF_FLAT, GPU_IVF_PQ, GPU_CAGRA, SPARSE_INVERTED_INDEX, SPARSE_WAND, SPARSE_WAND_PR, MINHASH_LSH, AUTOINDEX |
| **距离度量** | L2, IP, COSINE, JACCARD, HAMMING, TANIMOTO, BM25 |
| **量化方法** | SQ/SQ8, PQ + RaBitQ, BQ, ONNX 嵌入函数 |
| **过滤能力** | 预过滤 + 后过滤均支持，expr 参数支持 ==/!=/>/</>=/<=/in/not in/&&/\|\|/like/JSON path |
| **分页** | offset/limit + 游标 |
| **多租户** | 数据库级 / 集合级 / 分区键级（3 级隔离） |
| **流式/静态** | 支持流式插入 + 增量索引更新 |
| **分布式架构** | Proxy + QueryNode + DataNode + IndexNode + MixCoord + StreamingNode + Woodpecker (WAL) |
| **持久化** | Storage V3 (Loon) 清单式列式存储 + Vortex/Lance 格式 + 对象存储（MinIO/S3）|
| **GPU 加速** | GPU_IVF_FLAT, GPU_IVF_PQ, GPU_CAGRA（NVIDIA CUDA） |
| **稀疏-稠密混合** | 支持 sparse vector + dense vector 混合搜索 |
| **多向量搜索** | 支持（Multi-vector Search） |
| **SIFT-1M Recall@10** | HNSW: ~0.99 @ ~5K QPS；GPU_IVF_FLAT: ~0.95 @ ~50K QPS |

### 2. Pinecone

| 维度 | 规格 |
|------|------|
| **类型** | 云原生向量数据库即服务（DBaaS） |
| **架构** | Serverless（全托管自动扩展）+ Pod-based（P1/P2/S1） |
| **最大维度** | 20,000 |
| **索引类型** | HNSW（内置，自动优化） |
| **距离度量** | Cosine, Euclidean (L2), Dot Product |
| **量化方法** | 内置自动量化（int8） |
| **过滤能力** | 元数据预过滤，Namespace 隔离 |
| **分页** | pagination token |
| **多租户** | Namespace 隔离 |
| **流式/静态** | 实时 upsert + 自动索引更新 |
| **分布式** | 全托管自动扩展 |
| **持久化/WAL** | 全托管，无需手动管理 |
| **GPU 加速** | 不支持 |
| **稀疏-稠密混合** | 支持 Sparse-Dense Embeddings |
| **特殊** | 零运维、自动扩缩容、内置元数据过滤 |
| **SIFT-1M Recall@10** | ~0.95-0.98 |

### 3. Weaviate

| 维度 | 规格 |
|------|------|
| **类型** | 向量搜索引擎 |
| **最大维度** | 无硬性限制（实践中可达 65,536） |
| **索引类型** | Flat（暴力搜索）, HNSW（默认）, Dynamic（自动切换） |
| **距离度量** | Cosine, L2, Dot, Manhattan, Hamming |
| **量化方法** | BQ (~32x 压缩), PQ, SQ |
| **过滤能力** | BM25 + 向量混合搜索，ACORN 过滤策略 |
| **分页** | offset/limit + cursor |
| **多租户** | Namespaces 原生隔离 |
| **分布式** | 多节点集群、分片、副本 |
| **持久化/WAL** | LSM-tree + WAL |
| **GPU 加速** | 不支持 |
| **稀疏-稠密混合** | 支持（Hybrid Search: BM25 + Vector） |
| **多向量搜索** | 支持（Multi-vector per object） |
| **特殊** | GraphQL + REST API，Generative Search（RAG Ready） |
| **SIFT-1M Recall@10** | HNSW: ~0.99 @ ~3K QPS；BQ HNSW: ~0.95 @ ~10K QPS |

### 4. Qdrant

| 维度 | 规格 |
|------|------|
| **最新版本** | v1.19.0 (2026-08-05) |
| **语言** | Rust |
| **许可证** | Apache 2.0 |
| **最大维度** | 无硬性限制（实践中可达 65,535） |
| **索引类型** | HNSW（唯一 ANN 索引）+ Flat 回退 |
| **距离度量** | Cosine, Euclid (L2), Dot, Manhattan, Hamming |
| **量化方法** | Scalar (int8, 4x), Binary (32x), PQ, **TurboQuant 4-bit** (v1.19.0 新增) |
| **过滤能力** | 预过滤（payload 索引优先）+ 后过滤；关键词/范围/地理/日期/UUID/布尔索引 |
| **分页** | offset/limit + cursor + 切片过滤（v1.19.0） |
| **多租户** | Payload-based 多租户 + Sharding |
| **流式/静态** | 实时增删改 + WAL 保证持久化 |
| **分布式架构** | 分片 + 副本 + Raft 一致性；standalone 或 cluster |
| **持久化/WAL** | 每分片自定义 WAL + Raft 复制 + 快照恢复 |
| **GPU 加速** | 不支持 |
| **稀疏-稠密混合** | 原生支持 Sparse + Dense 混合搜索 |
| **多向量搜索** | 支持（Named Vectors） |
| **特殊** | 内存层分层（cold/cached/pinned），每租户 IDF 语料库，TurboQuant Hadamard 旋转加速 |
| **SIFT-1M Recall@10** | HNSW: ~0.99 @ ~8K QPS；Scalar Q: ~0.98 @ ~15K QPS |

### 5. Chroma

| 维度 | 规格 |
|------|------|
| **类型** | 嵌入式向量数据库 |
| **语言** | Python/JavaScript |
| **架构** | SQLite 存储元数据 + hnswlib（HNSW 实现）|
| **最大维度** | 无明确硬性限制（受 hnswlib 内存限制） |
| **索引类型** | HNSW（via hnswlib fork） |
| **距离度量** | L2, Cosine, Inner Product |
| **量化方法** | 不支持 |
| **过滤能力** | Metadata 过滤（where 条件，$and/$or/$gt/$lt 等）|
| **分页** | offset/limit |
| **多租户** | Collection 级隔离 |
| **流式/静态** | 支持实时增删改 |
| **分布式** | Client-Server 模式（非分布式）；依赖外部持久化 |
| **持久化/WAL** | SQLite + Parquet；无 WAL |
| **GPU 加速** | 不支持 |
| **稀疏-稠密混合** | 不支持 |
| **多向量搜索** | 不支持 |
| **特殊** | 开发友好、LangChain 集成、自动 embedding |
| **SIFT-1M Recall@10** | ~0.99 @ ~2K QPS（hnswlib 性能） |

### 6. pgvector

| 维度 | 规格 |
|------|------|
| **最新版本** | v0.8.6 (2026-07-29) |
| **类型** | PostgreSQL 扩展 |
| **最大维度** | vector: 2,000；halfvec: 4,000；bit: 64,000；sparsevec: 最多 1,000 非零元素 |
| **向量类型** | vector (float32, 4d+8B), halfvec (float16, 2d+8B), bit (binary), sparsevec (稀疏格式) |
| **索引类型** | HNSW（默认 m=16, ef_construction=64, ef_search=40）+ IVFFlat（默认 probes=1） |
| **距离度量** | L2 (<->), 负内积 (<#>), 余弦 (<=>), L1 (<+>), Hamming (<~>), Jaccard (<%>) |
| **量化方法** | 二值量化（binary_quantize 函数）+ halfvec（半精度 ~2x 节省）|
| **过滤能力** | 后过滤（默认）；v0.8.0+ 支持迭代索引扫描（strict_order/relaxed_order）|
| **分页** | 原生 SQL OFFSET/LIMIT |
| **多租户** | 推荐 LIST 分区或每个租户独立表（无原生多租户）|
| **流式/静态** | PostgreSQL ACID + MVCC |
| **分布式** | 无内置分片/复制，需通过 Citus 扩展 |
| **持久化/WAL** | PostgreSQL 原生 WAL + Checkpoint |
| **GPU 加速** | 不支持 |
| **稀疏-稠密混合** | 支持（sparsevec + vector） |
| **多向量搜索** | 支持（多列向量） |
| **特殊** | SQL 接口、事务支持、JOIN 查询、与 PostgreSQL 生态完全集成 |
| **SIFT-1M Recall@10** | HNSW: ~0.98 @ ~500 QPS；IVFFlat: ~0.95 @ ~1K QPS |

### 7. Elasticsearch（Elastic）

| 维度 | 规格 |
|------|------|
| **最新版本** | 9.5.x |
| **最大维度** | 4,096（硬性限制） |
| **element_type** | float（默认）, bfloat16（9.3+）, byte, bit |
| **索引类型** | hnsw, int8_hnsw（int8 量化，4x 压缩）, int4_hnsw（int4 量化，8x 压缩）, bbq_hnsw（BBQ 量化，32x 压缩）, flat, bbq_disk（企业版） |
| **HNSW 参数** | m=16（默认）, ef_construction=100（默认） |
| **距离度量** | l2_norm, dot_product, cosine（仅 bit）, max_inner_product |
| **量化方法** | int8, int4, BBQ (Binary Bootleg Quantization) |
| **过滤能力** | 原生 DSL 过滤 + 混合 kNN + 过滤 + RRF |
| **分页** | from/size + search_after 游标分页 |
| **多租户** | Index / Data Stream / RBAC 隔离 |
| **流式/静态** | Near-real-time（NRT）+ 实时索引 |
| **分布式** | 原生分布式（Shard + Replica + Node 集群）|
| **持久化/WAL** | Translog (WAL) + Segment 合并 |
| **GPU 加速** | 不支持 |
| **稀疏-稠密混合** | 原生支持（sparse_vector + dense_vector + BM25 + RRF） |
| **多向量搜索** | 支持（knn multi-vector） |
| **特殊** | vectordb_document 模式（9.5），auto_calibrate for bbq_disk |
| **SIFT-1M Recall@10** | HNSW: ~0.95 @ ~3K QPS；int8_hnsw: ~0.93 @ ~8K QPS |

---

## 二、开源向量搜索库对比

### 1. FAISS（Facebook AI Similarity Search）

| 维度 | 规格 |
|------|------|
| **最新版本** | v1.13.1 (2025-12-02) |
| **开发者** | Meta AI Research |
| **语言** | C++ |
| **许可证** | MIT |
| **最大维度** | 无硬性限制（受内存限制） |
| **索引类型** | Flat, IVF_FLAT, IVF_PQ, IVF_SQ8, HNSWFlat, HNSWPQ, PQ, OPQ, LSH, RQ, BinaryFlat, BinaryIVF, CAGRA, BinaryCagra, **Panorama** (v1.13.0+) |
| **GPU 索引** | GpuIndexFlatL2, GpuIndexIVFPQ, GpuIndexCagra |
| **距离度量** | L2, Inner Product, Cosine（归一化后内积）, L1, Linf, Lp, Hamming, Jaccard |
| **量化方法** | **PQ, OPQ, SQ, RaBitQ** (多比特量化, v1.13.0+), **Panorama** (v1.13.0+), RQ |
| **GPU 加速** | CUDA + ROCm；升级到 cuVS 25.10 后端（v1.13.0+）；bfloat16 GPU 支持 |
| **过滤能力** | IDSelector 过滤（预过滤） |
| **分布式** | Shard 分片（多进程/多机）；Memory-mapped 索引 |
| **持久化** | 索引序列化到 .index 文件；无 WAL |
| **稀疏向量** | 通过 Binary 字符串间接支持 |
| **SIFT-1M Recall@10** | IVF4096,PQ64: 0.91 @ ~30K QPS；HNSW32: 0.99 @ ~10K QPS；GPU_IVF1024,PQ32: 0.94 @ ~200K QPS |

### 2. HNSWlib

| 维度 | 规格 |
|------|------|
| **语言** | C++/Python |
| **最大维度** | 无硬性限制（受内存限制） |
| **索引类型** | HNSW（唯一） |
| **距离度量** | L2, Inner Product（Cosine） |
| **量化方法** | 不支持 |
| **过滤能力** | 支持 label-based 预过滤（回调函数） |
| **分布式** | 不支持 |
| **持久化** | save/load 到磁盘 |
| **GPU 加速** | 不支持 |
| **特殊** | 极简高性能、增量更新、C++/Python 接口 |
| **SIFT-1M Recall@10** | ~0.99 @ ~8K QPS |

### 3. ScaNN（Google Research）

| 维度 | 规格 |
|------|------|
| **开发者** | Google Research |
| **核心算法** | 各向异性量化（Anisotropic Quantization）+ 产品量化（PQ） |
| **最大维度** | 无硬性限制（常见测试到 960 维） |
| **距离度量** | 内积, L2 距离 |
| **量化方法** | **Anisotropic Quantization**（独创，保留重要维度精度）+ PQ + AVQ |
| **GPU 加速** | 不支持（纯 CPU，AVX2/AVX-512） |
| **过滤能力** | 不支持 |
| **分布式** | 不支持 |
| **持久化** | 序列化到磁盘 |
| **特殊** | Recall@QPS tradeoff 最优，TPU 友好 |
| **SIFT-1M Recall@10** | AQ (GT+rank+SVM): ~0.99 @ ~15K QPS；PQ: ~0.95 @ ~40K QPS |

### 4. DiskANN（Microsoft Research）

| 维度 | 规格 |
|------|------|
| **开发者** | Microsoft Research |
| **核心算法** | Vamana（图索引）+ 磁盘友好设计 |
| **最大维度** | 无硬性限制（常见测试到 768+，受 SSD 容量限制） |
| **距离度量** | L2, Inner Product |
| **量化方法** | PQ, OPQ, 舍入量化 |
| **过滤能力** | 支持 Filtered DiskANN |
| **分布式** | 支持 Shard 分片 |
| **持久化** | 原生 mmap 磁盘索引，支持超内存数据集 |
| **GPU 加速** | 不支持（实验性） |
| **特殊** | 十亿级数据集、SSD 存储、内存效率极高 |
| **SIFT-1M Recall@10** | ~0.95 @ ~5K QPS（SSD）；~0.99 @ ~2K QPS（内存） |

### 5. Annoy（Spotify）

| 维度 | 规格 |
|------|------|
| **开发者** | Spotify |
| **核心算法** | Random Projection Forest（树结构） |
| **最大维度** | 无硬性限制（受内存限制） |
| **距离度量** | Angular（Cosine）, Euclidean, Dot Product, Hamming |
| **量化方法** | 不支持（树结构本身是空间划分） |
| **过滤能力** | 不支持 |
| **分布式** | 不支持（仅单进程） |
| **持久化** | mmap 只读索引序列化 |
| **GPU 加速** | 不支持 |
| **特殊** | 多线程构建、只读索引（静态数据）、C++/Python |
| **SIFT-1M Recall@10** | ~0.94 @ ~2K QPS |

---

## 三、综合对比矩阵

### 3.1 索引类型支持

| 系统 | Flat | HNSW | IVF | PQ | DiskANN | GPU 索引 | 稀疏索引 |
|------|------|------|-----|----|---------|---------|---------|
| **Milvus** | Y | Y | Y | Y | Y | Y | Y (SPARSE_*) |
| **Pinecone** | - | Y | - | - | - | - | - |
| **Weaviate** | Y | Y | - | Y | - | - | - |
| **Qdrant** | Y | Y | - | Y | - | - | Y |
| **Chroma** | - | Y | - | - | - | - | - |
| **pgvector** | - | Y | Y | - | Y | - | Y |
| **Elasticsearch** | Y | Y | - | Y (BBQ) | - | - | - |
| **FAISS** | Y | Y | Y | Y | - | Y | - |
| **HNSWlib** | - | Y | - | - | - | - | - |
| **ScaNN** | - | - | - | Y | - | - | - |
| **DiskANN** | - | - | - | Y | Y | - | - |
| **Annoy** | - | - | - | - | - | - | - |

### 3.2 量化方法对比

| 系统 | PQ | OPQ | SQ (Scalar) | BQ | int8 | int4 | TurboQuant | halfvec | AQ (Anisotropic) |
|------|----|----|-------------|----|----|----|------------|---------|----------------|
| **Milvus** | Y | - | Y | Y | Y | - | - | - | - |
| **Pinecone** | - | - | Y | - | - | - | - | - | - |
| **Weaviate** | Y | - | Y | Y | - | - | - | - | - |
| **Qdrant** | Y | - | Y | Y | - | - | Y (4-bit) | - | - |
| **pgvector** | - | - | - | Y | - | - | - | Y | - |
| **Elasticsearch** | - | - | - | Y | Y | Y | - | - | - |
| **FAISS** | Y | Y | Y | - | Y | - | - | - | - |
| **ScaNN** | Y | - | - | - | - | - | - | - | Y |

### 3.3 过滤能力

| 系统 | 预过滤 | 后过滤 | 混合过滤 | SQL 过滤 | 布尔表达式 | 地理过滤 |
|------|--------|--------|----------|----------|-----------|---------|
| **Milvus** | Y | Y | Y | - | Y | - |
| **Pinecone** | Y | - | - | - | Y（元数据） | - |
| **Weaviate** | Y | - | Y（BM25+向量） | - | Y | - |
| **Qdrant** | Y | Y | Y | - | Y（Payload） | Y |
| **Chroma** | Y | - | - | - | Y（where） | - |
| **pgvector** | Y | - | - | Y | Y（完整SQL） | - |
| **Elasticsearch** | Y | - | Y（RRF） | Y（DSL） | Y | Y |
| **FAISS** | Y（IDSelector） | - | - | - | Y（基础） | - |
| **HNSWlib** | Y（回调） | - | - | - | Y（回调） | - |
| **ScaNN** | - | - | - | - | - | - |
| **DiskANN** | Y | - | - | - | Y | - |

### 3.4 架构特性

| 系统 | 分布式 | 多租户 | 持久化 | WAL | 流式更新 | 最新版本 |
|------|--------|--------|--------|-----|---------|---------|
| **Milvus** | Y（K8s） | Y（3 级） | Y | Y（Woodpecker） | Y | v3.0.0 |
| **Pinecone** | Y（托管） | Y（Namespace） | Y（托管） | Y（托管） | Y | - |
| **Weaviate** | Y | Y（原生） | Y | Y | Y | - |
| **Qdrant** | Y（Raft） | Y（Payload/Shard） | Y | Y | Y | v1.19.0 |
| **Chroma** | 单机 | Y（Collection） | Y（SQLite） | - | Y | - |
| **pgvector** | Y（Citus） | Y（分区/RLS） | Y（PG） | Y（PG） | Y | v0.8.6 |
| **Elasticsearch** | Y（原生） | Y（Index） | Y | Y | Y | 9.5.x |
| **FAISS** | Shard | - | Y（文件） | - | 手动 | v1.13.1 |
| **HNSWlib** | - | - | Y（文件） | - | Y（增量） | - |
| **ScaNN** | - | - | Y（文件） | - | - | - |
| **DiskANN** | Shard | - | Y（mmap） | - | - | - |
| **Annoy** | - | - | Y（mmap） | - | - | - |

### 3.5 最大维度支持

| 系统 | 最大维度 | 说明 |
|------|---------|------|
| **Milvus** | 32,768 | float/binary/sparse 均支持 |
| **Pinecone** | 20,000 | 所有 Pod 类型统一 |
| **Weaviate** | ~65,536 | 可配置 |
| **Qdrant** | ~65,535 | 无硬性限制 |
| **Chroma** | ~20,000（估计） | 无明确限制，受 hnswlib 内存限制 |
| **pgvector** | 2,000（vector）/ 4,000（halfvec）/ 64,000（bit）| 需修改 MAX_DIM 重新编译 |
| **Elasticsearch** | 4,096 | 硬性限制 |
| **FAISS** | 无限制 | 受内存限制 |
| **HNSWlib** | 无限制 | 受内存限制 |
| **ScaNN** | 无限制 | 受内存限制 |
| **DiskANN** | 无限制 | 受磁盘/内存限制 |
| **Annoy** | 无限制 | 受内存限制 |

### 3.6 SIFT-1M Recall@10 vs QPS 参考（近似值）

| 系统 | 配置 | Recall@10 | QPS (单节点) | 说明 |
|------|------|-----------|-------------|------|
| **FAISS GPU_IVF1024,PQ32** | GPU | 0.94 | ~200,000 | NVIDIA A100 |
| **FAISS IVF4096,PQ64** | CPU | 0.91 | ~30,000 | |
| **FAISS HNSW32** | CPU | 0.99 | ~10,000 | |
| **ScaNN AQ (GT+rank+SVM)** | CPU | 0.99 | ~15,000 | Google 搜索优化 |
| **HNSWlib (M=16, ef=128)** | CPU | 0.99 | ~8,000 | |
| **Milvus HNSW** | CPU | 0.99 | ~5,000 | 含服务开销 |
| **Milvus GPU_IVF_FLAT** | GPU | 0.95 | ~50,000 | |
| **Qdrant HNSW** | CPU | 0.99 | ~8,000 | Rust 高效 |
| **Qdrant Scalar Q** | CPU | 0.98 | ~15,000 | int8 量化 |
| **Weaviate HNSW** | CPU | 0.99 | ~3,000 | 含服务开销 |
| **Weaviate BQ HNSW** | CPU | 0.95 | ~10,000 | |
| **Elasticsearch HNSW** | CPU | 0.95 | ~3,000 | Lucene |
| **Elasticsearch int8_hnsw** | CPU | 0.93 | ~8,000 | int8 量化 |
| **pgvector HNSW** | CPU | 0.98 | ~500 | PostgreSQL 开销 |
| **DiskANN (内存)** | CPU | 0.99 | ~2,000 | |
| **DiskANN (SSD)** | CPU | 0.95 | ~5,000 | 十亿级可扩展 |
| **Annoy (10 trees)** | CPU | 0.94 | ~2,000 | |

> **注**：QPS 值受硬件、参数调优、数据分布影响较大，以上为典型参考值。ann-benchmarks.com 提供交互式对比。

---

## 四、选型建议

### 按使用场景

| 场景 | 推荐方案 | 理由 |
|------|---------|------|
| **原型/快速验证** | Chroma, pgvector | 简单易用，零运维 |
| **生产级向量搜索** | Milvus, Qdrant, Weaviate | 分布式、高可用、丰富过滤 |
| **全托管 SaaS** | Pinecone | 零运维，自动扩展 |
| **与关系型数据库集成** | pgvector | 原生 SQL、事务、JOIN |
| **全文 + 向量混合搜索** | Elasticsearch, Weaviate | BM25 + 向量统一 + RRF |
| **十亿级离线向量搜索** | FAISS + DiskANN | 极致性能，GPU 加速 |
| **嵌入式/边缘部署** | HNSWlib, FAISS, Chroma | 轻量、无依赖 |
| **高维稀疏向量** | Qdrant, Milvus, pgvector | 原生 sparse vector 支持 |

### 按性能优先级

| 优先级 | 推荐 |
|--------|------|
| **最高 QPS** | FAISS GPU (IVF) > ScaNN > HNSWlib > Qdrant |
| **最高 Recall** | ScaNN AQ > FAISS HNSW ≈ HNSWlib > Qdrant ≈ Milvus |
| **最低延迟** | HNSWlib > Qdrant > Milvus > Elasticsearch |
| **最佳压缩比** | FAISS PQ > Qdrant TurboQuant > Elasticsearch BBQ |

### 按部署模式

| 模式 | 选项 |
|------|------|
| **单机嵌入式** | Chroma, pgvector, FAISS, HNSWlib, Annoy |
| **单机服务器** | Qdrant, Weaviate, Elasticsearch |
| **分布式集群** | Milvus, Elasticsearch, Qdrant (Raft), Weaviate |
| **全托管云** | Pinecone, Zilliz Cloud (Milvus), Weaviate Cloud, Qdrant Cloud |

---

## 五、版本与发布时间线

| 系统 | 版本 | 发布日期 | 重要更新 |
|------|------|---------|---------|
| **Milvus** | v3.0.0 | 2026-07-29 | 湖原生架构、Storage V3、Vortex/Lance 格式 |
| **Milvus** | v2.6.22 | 2026-08-04 | 2.6.x 活跃维护 |
| **Qdrant** | v1.19.0 | 2026-08-05 | TurboQuant 4-bit、内存分层、切片过滤 |
| **pgvector** | v0.8.6 | 2026-07-29 | 最新稳定版 |
| **FAISS** | v1.13.1 | 2025-12-02 | RaBitQ、Panorama、cuVS 25.10 |
| **Elasticsearch** | 9.5.x | 2026 | vectordb_document、auto_calibrate |

---

*文档版本: 2026-08-25*
*数据来源: 各项目官方 GitHub 仓库、ann-benchmarks.com、社区基准测试*
