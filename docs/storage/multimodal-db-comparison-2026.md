# 多模态数据库综合对比（2026 版）

> 本文档对自实现多模态数据库的 8 个数据模型，与业界商用标准和开源技术进行完整对比。
> 涵盖架构设计、索引类型、查询能力、性能指标、功能完整性、API 设计、持久化、并发支持、分布式能力等维度。
> 侧重具体参数与数据，不涉及营销宣传。

---

## 一、对比概览

| 数据模型 | 自实现状态 | 商用标杆 | 开源标杆 |
|---------|-----------|---------|---------|
| 向量（Vector） | HNSW / IVF-PQ / PQ / SQ / Flat，WAL，VecPage | Milvus / Pinecone / Weaviate / Qdrant / pgvector | FAISS / HNSWlib / ScaNN / DiskANN |
| 图（Graph） | CSR + COO，BFS/DFS/最短路径/PageRank | Neo4j / TigerGraph / Neptune / NebulaGraph | JanusGraph / ArangoDB / Memgraph / Dgraph |
| 时序（Timeseries） | Segment 索引，Gorilla 压缩，滑动窗口聚合 | InfluxDB 3.0 / TimescaleDB 2.x / TDengine 3.x | QuestDB / ClickHouse / Prometheus TSDB |
| 文档（Document） | JSON 存储，JSONPath 查询，倒排索引，BM25 | MongoDB 7.0 / Elasticsearch 8.x | CouchDB 3.x / FerretDB |
| 空间（Spatial） | R-Tree 索引，Hilbert 曲线，几何类型（Point/LineString/Polygon） | PostGIS 3.4 / H3 / S2 | DuckDB Spatial / SpatiaLite |
| KV（键值） | WAL，TTL，有序存储，游标扫描 | Redis 7.x / etcd / FoundationDB | RocksDB / LevelDB / BoltDB / etcd |
| 树形（Yang/Tree） | 层次存储，节点类型，父子关系，路径查询 | MarkLogic / BaseX / eXist-db | libyang / sysrepo / PostgreSQL ltree |
| 关系（Relational） | SQL 执行器（Volcano 迭代器），8 种物理算子 | PostgreSQL 17 / MySQL 9 | DuckDB / SQLite |

---

## 二、向量模型（Vector）

### 2.1 商用产品对比

| 维度 | Milvus（Zilliz） | Pinecone | Weaviate | Qdrant | pgvector | Elasticsearch |
|------|-----------------|----------|----------|--------|----------|---------------|
| **最大维度** | 32,768 | 20,000 | 65,536 | 65,535 | 2,000（默认）/ 16,000（编译） | 4,096（默认） |
| **索引类型** | IVF_FLAT, IVF_SQ8, IVF_PQ, **HNSW**, ANNOY, **DiskANN**, SCANN, GPU_IVF | HNSW（内置） | Flat, **HNSW**, Dynamic | **HNSW**（唯一）+ Flat | IVFFlat, **HNSW**, DiskANN (0.8+) | **HNSW**（Lucene）+ Flat |
| **距离度量** | L2, IP, Cosine, Jaccard, Hamming, Tanimoto | Cosine, L2, Dot | Cosine, L2, Dot, Manhattan, Hamming | Cosine, L2, Dot, Manhattan | L2, Cosine, IP, Hamming, L1, Jaccard | Cosine, L2, Dot, MaxIP, Hamming |
| **量化方法** | PQ, SQ8, BQ | int8 标量量化 | BQ（32x 压缩）, PQ, SQ | Scalar Q（int8）, BQ, PQ | halfvec（float16）, bit | int8 Scalar Q, PQ, BQ |
| **过滤能力** | 标量过滤（布尔表达式），预/后/混合过滤 | 元数据预过滤 | BM25 + 向量混合，预过滤 | Payload 索引过滤，预/后过滤可配 | 原生 SQL WHERE 过滤 | DSL 过滤，RRF 混合 |
| **分布式架构** | Proxy + QueryNode + DataNode + IndexNode，K8s 原生 | 全托管 Serverless 自动扩展 | 多节点集群 + 分片 + 副本 | 分片 + 副本，Raft 一致性 | Citus 分布式扩展 | 原生分布式 Shard + Replica |
| **持久化/WAL** | WAL + Checkpoint + Segment | 全托管持久化 | LSM-tree + WAL | WAL + Snapshot | PostgreSQL 原生 WAL | Translog（WAL）+ Segment 合并 |
| **GPU 加速** | GPU_IVF_FLAT, GPU_IVF_PQ, GPU_CAGRA | 不支持 | 不支持 | 不支持 | 不支持 | 不支持 |
| **稀疏-稠密混合** | sparse vector + dense vector | Sparse-Dense Embeddings | Hybrid Search (BM25 + Vector) | 原生 Sparse + Dense | sparsevec + vector | sparse_vector + dense_vector + BM25 |
| **SIFT-1M Recall@10** | HNSW: ~0.99 @ ~5K QPS | ~0.95-0.98 | HNSW: ~0.99 @ ~3K QPS | HNSW: ~0.99 @ ~8K QPS | HNSW: ~0.98 @ ~500 QPS | HNSW: ~0.95 @ ~3K QPS |

### 2.2 开源库对比

| 维度 | FAISS | HNSWlib | ScaNN | DiskANN | Annoy |
|------|-------|---------|-------|---------|-------|
| **最大维度** | ~16,000（受内存限制） | 无硬性限制 | 无硬性限制 | 无硬性限制 | 无硬性限制 |
| **索引类型** | Flat, IVF_FLAT, IVF_PQ, **HNSWFlat**, PQ, OPQ, LSH, RQ | **HNSW**（唯一） | AQ (Anisotropic), PQ, AVQ | **Vamana** (DiskANN) | Tree-based (Random Projection) |
| **GPU 索引** | GPU_IVF_FLAT, GPU_IVF_PQ, GPU_FlatL2 | 不支持 | 不支持 | 不支持 | 不支持 |
| **距离度量** | L2, IP, L1, Linf, Lp, Hamming, Jaccard | L2, IP (Cosine) | Cosine, L2, Dot | L2, IP | Angular (Cosine), L2, Dot, Hamming |
| **量化方法** | **PQ, OPQ, SQ8, RQ** | 不支持 | **Anisotropic Q（独创）** | PQ, OPQ | 不支持（树结构本身是空间划分） |
| **过滤能力** | IDSelector 预过滤 | label-based 预过滤 | 不支持 | 支持带过滤搜索 | 不支持 |
| **分布式** | Shard 分片 | 不支持 | 不支持 | Shard 分片 | 不支持 |
| **持久化** | 序列化到 .index 文件 | save/load 到磁盘 | 序列化到磁盘 | 原生磁盘索引（mmap） | mmap 只读索引 |
| **SIFT-1M Recall@10** | HNSW32: 0.99 @ ~10K QPS；GPU_IVF1024,PQ32: 0.94 @ ~200K QPS | ~0.99 @ ~8K QPS | AQ: 0.99 @ ~15K QPS | 内存: 0.99 @ ~2K QPS；SSD: 0.95 @ ~5K QPS | ~0.94 @ ~2K QPS |

### 2.3 自实现 vs 业界差距分析

| 维度 | 自实现 | 业界最强 | 差距 | 说明 |
|------|--------|---------|------|------|
| **索引类型** | HNSW, IVF-PQ, PQ, SQ, Flat | FAISS: 40+ 类型 | 中等 | 缺少 LSH, RQ, IVF-HNSW 等高级复合索引 |
| **量化精度** | PQ（固定 m）, SQ（固定 int8） | FAISS OPQ, ScaNN AQ | 中等 | 缺少 OPQ（方向对齐优化）和各向异性量化 |
| **GPU 加速** | 无 | FAISS GPU_IVF (A100 ~200K QPS) | 较大 | 无 NVIDIA CUDA 支持 |
| **磁盘索引** | 无（内存索引） | DiskANN (SSD, 十亿级) | 较大 | 无法支持超内存数据集 |
| **过滤能力** | 基础标量过滤 | Milvus 布尔表达式预/后/混合过滤 | 中等 | 缺少复杂布尔过滤和嵌套 JSON 过滤 |
| **分布式** | 单机 | Milvus K8s 多分片多副本 | 较大 | 无集群和分片能力 |
| **量化压缩比** | PQ 约 8-16x | Weaviate BQ 32x, Qdrant BQ 32x | 中等 | 缺少 Binary Quantization |
| **距离度量** | L2, Cosine, Dot, Hamming | FAISS: Lp, Linf, Jaccard 等 12+ | 较小 | 缺少 L1/Linf/Jaccard 等度量 |
| **召回率** | HNSW: 0.99+ @ 100K | ScaNN AQ: 0.99 @ 15K QPS | 无差距 | 核心召回率已达标 |
| **写入吞吐** | ~158K vec/s | FAISS GPU: ~500K+ vec/s | 中等 | 瓶颈在 C 绑定开销 |

**Gap 优先级排序**：
1. **GPU 加速**（影响 5-10x 吞吐量）
2. **分布式架构**（影响水平扩展）
3. **磁盘索引**（影响超大数据集支持）
4. **高级量化**（OPQ, AQ, BQ）
5. **复杂过滤**（布尔表达式、嵌套 JSON）

---

## 三、图模型（Graph）

### 3.1 商用产品对比

| 维度 | Neo4j | TigerGraph | Amazon Neptune | NebulaGraph | Memgraph |
|------|-------|------------|----------------|-------------|----------|
| **数据模型** | 属性图（Native） | 属性图（Native） | 属性图 + RDF 三元组 | 属性图（Native） | 属性图（Native） |
| **查询语言** | Cypher / openCypher / GQL (草案) | GSQL（类 SQL） | Gremlin / SPARQL / openCypher | nGQL（Cypher 子集） | Cypher / openCypher |
| **存储格式** | Native Property Store（CSStore） | CSR 压缩邻接 | WiredTiger SSTable + Journal | CSR + Partition Raft | CSR（内存）+ WAL |
| **索引类型** | Range/Label/Text + 向量索引（5.0+） | 邻接索引 + 标签索引 + 属性索引 | 基于后端的索引（Lucene/Cassandra） | Tag + Edge + 属性索引 | B-Tree（属性）+ HNSW（向量） |
| **图分析算法** | GDS 库 60+（PageRank/社区发现/最短路径等） | GSQL Algorithms 50+ | 10+（依赖 SageMaker 做 ML） | Job Submit 40+ | MAGE 库 100+ |
| **分布式架构** | 因果集群（Causal Cluster）主从 | 完全分布式（GSQL 分布式执行） | 分片集群（无内置共识，依赖 RDS/Aurora） | Storage + Query 层，Partition Raft | 单机或 G-Alpha 分布式 |
| **最大规模** | AuraDB: 100K 节点/边；企业版: 百亿级 | 2.6 万亿边（Graph500 记录） | 取决于底层 RDS，约百亿边 | 单集群: 万亿级边 | 内存: 受 RAM 限制；持久化: PB 级 |
| **持久化/WAL** | Checkpoint + Transaction Log | WAL + Checkpoint | 依赖 RDS/Aurora 持久化 | RocksDB WAL + Checkpoint | AOF + RDB 持久化 |
| **事务支持** | 单写 ACID，多写需事务 | 严格分布式 ACID | Two-Phase Commit（有限事务） | 单 Partition ACID | MVCC 快照隔离 |
| **SaaS 部署** | AuraDB（DBaaS）/ AuraDB Serverless | TigerGraph Cloud（云原生） | Neptune / Neptune Serverless Gen 2 | NebulaGraph Cloud | Memgraph Cloud |
| **特有功能** | GraphRAG 原生集成 / 向量索引 5.0+ / GraphQL | GraphML 导出 / 实时图计算 / AI Agent | S3 Vectors + Bedrock 集成 / ML | Graph500 榜首 / 存算分离 | 实时流处理 / 向量索引 |

### 3.2 开源产品对比

| 维度 | JanusGraph | ArangoDB | Dgraph | PostgreSQL | MongoDB |
|------|-----------|----------|--------|------------|---------|
| **数据模型** | 属性图（依赖后端） | 多模型（图+文档+KV） | 三元组 RDF + GraphQL | 关系型 + ltree + 递归 CTE | 文档型 + $graphLookup |
| **查询语言** | Gremlin（TinkerPop 3.x） | AQL / SQL | DQL（GraphQL-like） | SQL + recursive CTE + ltree | Aggregation Pipeline |
| **存储格式** | Cassandra/HBase/Bigtable + Elasticsearch | ArangoDB Storage Engine（MMFiles/RocksDB） | Badger LSM + Posting Lists | PostgreSQL Heap + TOAST | WiredTiger（BSON + 列压缩） |
| **索引类型** | 复合索引（混合后端） | 边索引 + 属性索引 + 全文索引 |_predicate_ 索引 + 倒排索引 | B-Tree + GIN/GiST + ltree | B-Tree + 全文 + 2dsphere |
| **图分析算法** | 无内置（依赖 Spark/Hadoop） | BFS/DFS（基础） | PageRank/BC/CC（5+） | pgRouting 20+（路径/VRP） | 无内置 |
| **分布式架构** | 依赖 Cassandra/HBase Ring 分片 | ArangoDB Cluster（分片）+ Agency Raft | Raft 共识（Alpha）+ Zero 无状态 | Citus 分片扩展 | Sharded Cluster + Config Server Raft |
| **最大规模** | Cassandra 后端: 万亿级 | 百亿级边 | 千亿级三元组 | Citus: 百亿行 | PB 级，Atlas 案例 100+ PB |
| **持久化/WAL** | 依赖后端（Cassandra WAL） | RocksDB WAL + Checkpoint | Badger WAL | PostgreSQL WAL + Checkpoint | WiredTiger Journal + Oplog |
| **事务支持** | 乐观 CAS + 后端事务 | MVCC（Enterprise）/ 单文档 | 只读事务 + 分布式写入事务 | ACID（MVCC） | 单文档 ACID + 多文档事务（4.0+） |
| **特有功能** | 多后端灵活切换 | 多模型统一存储 / Foxx 微服务 | GraphQL 原生 / 存算分离 | 生态最全（pgvector/pgRouting/JSONB） | Change Streams / Queryable Encryption |

### 3.3 横向对比矩阵

| 系统 | 属性图 | RDF | 多模型 | 图算法数 | 分布式 ACID | Serverless | GraphRAG |
|------|--------|-----|--------|---------|------------|------------|----------|
| **Neo4j** | Y | - | - | 60+ | Y | Y（AuraDB） | Y（原生） |
| **TigerGraph** | Y | - | - | 50+ | Y | Y | Y |
| **Neptune** | Y | Y | - | 10+ | 有限 | Y（Gen 2） | Y（Bedrock） |
| **NebulaGraph** | Y | - | - | 40+ | Y（单 Partition） | Y | Y |
| **Memgraph** | Y | - | - | 100+ | Y（MVCC） | Y | Y |
| **JanusGraph** | Y | - | - | 0 | - | - | - |
| **ArangoDB** | Y | - | Y（图+文档+KV） | 基础 | Y（Enterprise） | Y | - |
| **Dgraph** | Y（RDF） | Y | - | 5+ | Y（Raft） | Y | Y |
| **PostgreSQL** | - | - | Y（+ltree/+JSONB/+pgvector） | 20+（pgRouting） | Y（MVCC） | Y（RDS/Citus） | Y（pgvector） |
| **MongoDB** | Y（$graphLookup） | - | Y（文档+图查询） | 0 | Y（单文档） | Y（Atlas Serverless） | Y（Atlas Vector） |
| **自实现** | Y（CSR+COO） | - | - | 4（BFS/DFS/最短路径/PageRank） | - | - | - |

### 3.4 自实现 vs 业界差距分析

| 维度 | 自实现 | 业界最强 | 差距 | 说明 |
|------|--------|---------|------|------|
| **查询语言** | 基础 C API（BFS/DFS/最短路径） | Cypher / GQL / GSQL / DQL | 较大 | 缺少声明式图查询语言，用户门槛高 |
| **图分析算法** | 4 个（最短路径/BFS/DFS/PageRank） | Memgraph MAGE 100+ | 很大 | 缺少社区发现、中心性分析、图嵌入等 |
| **分布式架构** | 单机 | TigerGraph: 万亿边，Raft 共识 | 很大 | 无集群、分片、共识协议 |
| **存储格式** | CSR（压缩）+ COO（增量） | Neo4j CSStore / TigerGraph CSR | 中等 | CSR/COO 与业界主流一致 |
| **索引类型** | 基础邻接索引 | Neptune: 多种后端索引；ArangoDB: 边+属性+全文 | 中等 | 缺少全文索引、TTL 索引等 |
| **持久化/WAL** | WAL 支持 | RocksDB WAL + Checkpoint（NebulaGraph/Dgraph） | 较小 | WAL 机制与业界一致 |
| **事务支持** | 无 ACID 事务 | TigerGraph 严格分布式 ACID | 很大 | 无并发控制和一致性保证 |
| **GQL 标准合规** | 无 | Neo4j: GQL TC 核心成员 | 很大 | 缺少 ISO GQL 标准支持 |
| **GraphRAG** | 无 | Neo4j/LangChain 原生集成 | 很大 | 无法直接用于 GraphRAG 场景 |

**Gap 优先级排序**：
1. **图分析算法**（缺少 96+ 核心算法）
2. **查询语言**（C API 门槛高，缺少 Cypher/GQL）
3. **分布式架构**（无分片和共识协议）
4. **ACID 事务**（无并发控制）
5. **GraphRAG 集成**（无法支持 RAG 场景）

---

## 四、时序模型（Timeseries）

### 4.1 商用产品对比

| 维度 | InfluxDB 3.0（IOx） | TimescaleDB 2.x | TDengine 3.x | QuestDB | ClickHouse |
|------|--------------------|-----------------|-------------|---------|------------|
| **最大标签数/字段数** | 无限制（Schema-less） | 无限制（Hypertable） | 标签: 无限制，字段: 无限制 | 列: 无限制 | 无限制 |
| **数据压缩算法** | Gorilla 压缩（列式 IOx） | Gorilla + ZSTD（压缩率 10-50x） | Gorilla + 专利列式压缩 | Gorilla + 自适应编码 | ZSTD（10-30x） |
| **聚合函数** | Flux/InfluxQL: mean/max/min/count/derivative | AVG/MIN/MAX/COUNT + 持续聚合 | AVG/MIN/MAX/COUNT + 滑动窗口 | SELECT 聚合 + SAMPLE BY | 完整 SQL 聚合 |
| **降采样/滚动窗口** | 原生: downsample + CONTINUOUS QUERIES | 持续聚合 + 重新聚合 | 连续查询 + 降采样 | SAMPLE BY + ASOF JOIN | 物化视图 + TTL |
| **TTL/数据保留** | 原生 RETENTION POLICY | DROP CHUNK（自动/手动） | KEEP（自动过期） | TTL via SQL | TTL via PARTITION BY |
| **写入吞吐量** | IOx: ~100 万点/秒 | ~10-50 万行/秒 | ~100 万点/秒（集群） | ~100 万行/秒 | ~100 万行/秒+ |
| **物化视图/持续查询** | CONTINUOUS QUERIES（实时降采样） | 持续聚合 + 重新聚合 | 连续查询（实时/定时） | 物化视图（标准 SQL） | 物化视图（标准 SQL） |
| **分布式架构** | IOx（Rust 列式，Kubernetes 原生） | TimescaleDB Hypertables + Citus 分片 | 原生分片 + 雾计算/边缘 | 单机（无内置集群） | 原生分片 + 副本 + Zookeeper/ClickHouse Keeper |
| **持久化/WAL** | Parquet + Write-ahead Log | PostgreSQL WAL + Checkpoint | WAL + 缓存 + 持久化 | WAL（内存映射）+ Append | WAL + MergeTree + 异步后台合并 |
| **特有功能** | Flux 语言 / Line Protocol / Telegraf 集成 | Hypertable + 重新聚合 + 压缩 | 超级表 / 边缘计算 / 雾存储 | 超低延迟 / SIMD / 物联网时序 | 列式 / 向量查询 / 物化视图 |

### 4.2 开源产品对比

| 维度 | Prometheus TSDB | InfluxDB OSS（1.x/2.x） | QuestDB（OSS） | TimescaleDB（OSS） |
|------|----------------|------------------------|--------------|-------------------|
| **数据模型** | Metric + Label + Timestamp | Measurement + Tag + Field + Timestamp | Table（列式） | Hypertable（分区表） |
| **存储格式** | TSDB Block（mmap） | TSM（Time-Structured Merge Tree） | Column Store（WAL + Append） | PostgreSQL Heap（列压缩） |
| **压缩算法** | Gorilla 变体 | Gorilla + 差分编码 | Gorilla + 自适应编码 | ZSTD（10-50x） |
| **查询语言** | PromQL | InfluxQL / Flux | SQL | SQL（PostgreSQL 语法） |
| **聚合能力** | rate/irate/increase/predict_linear | 基础聚合 + GROUP BY time | 标准 SQL 聚合 | 持续聚合 + 重新聚合 |
| **写入吞吐量** | ~10 万样本/秒 | ~20 万点/秒 | ~100 万行/秒 | ~10-50 万行/秒 |
| **分布式** | Remote Write + Thanos/Cortex | 单机（无集群 OSS） | 单机 | Citus 分片（需额外安装） |
| **持久化/WAL** | mmap Block + WAL | TSM WAL + Compaction | WAL + Append | PostgreSQL WAL |
| **TTL** | 块过期（可配置） | RETENTION POLICY | SQL TTL | DROP CHUNK |
| **特有功能** | 服务发现 / Alertmanager / PromQL | Telegraf 集成 / Line Protocol | SIMD 加速 / IoT 协议（MQTT/AMQP） | PostgreSQL 生态（JSONB/pgvector） |

### 4.3 自实现 vs 业界差距分析

| 维度 | 自实现 | 业界最强 | 差距 | 说明 |
|------|--------|---------|------|------|
| **压缩算法** | Gorilla 压缩 | InfluxDB/ClickHouse: Gorilla + ZSTD（10-50x） | 中等 | 缺少 ZSTD 等高级压缩 |
| **聚合函数** | 基础聚合（sum/avg/max/min/count） | InfluxDB/TimescaleDB: 完整时间序列聚合 | 中等 | 缺少 derivative/rate/percentile 等专用函数 |
| **滑动窗口** | 支持（滑动窗口聚合） | InfluxDB: CONTINUOUS QUERIES；TDengine: 连续查询 | 无差距 | 核心功能已实现 |
| **写入吞吐** | ~50-100K 点/秒 | QuestDB/ClickHouse: ~100 万行/秒 | 较大 | 差距约 10x |
| **物化视图** | 无 | TimescaleDB: 持续聚合；ClickHouse: 物化视图 | 较大 | 无实时降采样物化视图 |
| **分布式** | 单机 | TimescaleDB + Citus；TDengine 原生分片 | 很大 | 无集群分片能力 |
| **TTL/保留** | Segment 索引 + 过期清理 | 原生 RETENTION POLICY / DROP CHUNK | 较小 | 已有基础 TTL 机制 |
| **数据降采样** | 支持（滑动窗口聚合） | InfluxDB/TimescaleDB 持续降采样 | 无差距 | 核心功能已实现 |

**Gap 优先级排序**：
1. **分布式架构**（无集群分片）
2. **写入吞吐量**（差距约 10x）
3. **高级压缩**（ZSTD 等）
4. **物化视图**（无持续降采样）
5. **专用聚合函数**（derivative/rate/predict_linear）

---

## 五、文档模型（Document）

### 5.1 商用产品对比

| 维度 | MongoDB 7.0 | Elasticsearch 8.x | Azure Cosmos DB | Amazon DocumentDB |
|------|------------|-------------------|-----------------|------------------|
| **最大文档大小** | 16 MB | 无硬性限制（建议 < 1GB） | 无限制 | 无限制（16MB 建议） |
| **最大嵌套深度** | 无限制 | 无限制 | 无限制 | 无限制 |
| **索引类型** | B-Tree + 全文 + TTL + 2dsphere + 复合 + 向量 | B-Tree + 倒排 + 全文（BM25）+ 向量 + 嵌套 | B-Tree + 全文 + 向量 | B-Tree + 全文 + 向量 |
| **查询语言** | MongoDB Query（BSON 文档） | Elasticsearch DSL（JSON） | SQL + MongoDB Query | MongoDB Query（兼容） |
| **全文搜索** | 基础（$search 依赖 Atlas Search） | **BM25** + 分析器 + 同义词 + 短语匹配 | 基础全文 | 基础全文 |
| **分片策略** | Range / Hashed / Zone Sharding | 索引级分片 + 副本 | 哈希/范围/无服务器 | Range / Hashed（分片集群） |
| **聚合管道** | $match/$group/$project/$sort/$limit + $graphLookup | Aggregations + Pipeline + Runtime Scripts | SQL AGG / Pipeline | MongoDB Pipeline 兼容 |
| **持久化/WAL** | WiredTiger Journal + Oplog + Checkpoint | Translog（WAL）+ Segment 合并 + Lucene | 原生 SSD 存储 + WAL | WiredTiger + Oplog |
| **特有功能** | Change Streams / Queryable Encryption / Atlas Vector Search | RRF 混合排序 / ML Inference / ES|QL | 多模型（SQL+MongoDB+MongoDB Atlas Gremlin） | Lambda 函数触发 / 时间点恢复 |

### 5.2 开源产品对比

| 维度 | CouchDB 3.x | FerretDB | Elasticsearch OSS | Meilisearch |
|------|------------|----------|------------------|-------------|
| **最大文档大小** | 无限制 | 无限制（PostgreSQL 限制） | 无限制 | 无限制 |
| **数据模型** | JSON 文档 | PostgreSQL 后端的 MongoDB 协议 | JSON 文档 + 全文 + 向量 | JSON 文档 |
| **存储格式** | Datastore（Append-only B-Tree） | PostgreSQL 表 | Lucene 索引 + Segment | 内部 LMDB |
| **索引类型** | 设计文档索引（Map-Reduce） | PostgreSQL 索引（B-Tree/GIN/全文） | B-Tree + 倒排 + 向量 | B-Tree + 倒排（Meilisearch 专有） |
| **查询语言** | Mango Query（JSON Selector） | MongoDB Query（翻译为 SQL） | Elasticsearch DSL | REST API + 过滤表达式 |
| **全文搜索** | 基础全文（Lucene） | PostgreSQL 全文搜索 | BM25 + 分析器 + 同义词 | BM25 + 错别字容忍 + 拼音搜索 |
| **聚合管道** | Map-Reduce / 视图 | PostgreSQL 聚合 | Aggregations + Pipeline | 有限（Facet/Search） |
| **持久化/WAL** | Append-only + CouchDB WAL | PostgreSQL WAL | Translog + Segment | 定期快照 |
| **特有功能** | CRDT 同步 / 离线优先 / 多主复制 | MongoDB 协议兼容（0.16+） | 向量搜索 / ML Inference | 超快搜索 / 错别字容忍 / 中文分词 |

### 5.3 自实现 vs 业界差距分析

| 维度 | 自实现 | 业界最强 | 差距 | 说明 |
|------|--------|---------|------|------|
| **全文搜索** | BM25 + FST 词典 | Elasticsearch: BM25 + 分析器 + 同义词 + 短语 | 中等 | 缺少分析器（中文分词/同义词/词干化） |
| **JSONPath 查询** | 支持基础 JSONPath | MongoDB: 完整 BSON 查询；ES: DSL | 较小 | 已有基础 JSONPath 实现 |
| **倒排索引** | 支持（BM25 评分） | Elasticsearch: 多层倒排 + 向量倒排 | 较小 | 核心倒排索引已实现 |
| **聚合管道** | 无 | MongoDB: 20+ 管道阶段；ES: Aggregations | 很大 | 无聚合管道支持 |
| **索引类型** | 倒排索引（BM25） | MongoDB: B-Tree/全文/TTL/2dsphere/向量 | 较大 | 缺少 B-Tree 索引（等值查询）、TTL 索引 |
| **分片策略** | 单机 | MongoDB: Range/Hashed/Zone；ES: Index 分片 | 很大 | 无分片能力 |
| **Change Streams** | 无 | MongoDB Change Streams / ES Ingest Pipeline | 很大 | 无实时 CDC 能力 |
| **文档大小** | 受 VecPage 限制 | MongoDB: 16MB；ES: 无限制 | 较小 | 文档大小可扩展 |

**Gap 优先级排序**：
1. **聚合管道**（无 $match/$group/$project 等）
2. **分片架构**（无水平扩展）
3. **索引类型**（缺少 B-Tree 等值索引）
4. **分析器**（中文分词/同义词/词干化）
5. **Change Streams**（无实时 CDC）

---

## 六、空间模型（Spatial）

### 6.1 商用产品对比

| 维度 | PostGIS 3.4 | H3（Uber） | Google S2 | MapBox |
|------|------------|-----------|----------|--------|
| **几何类型** | Point/LineString/Polygon/Multi*/GeometryCollection/ CircularString | 六边形网格（Resolution 0-15） | 所有几何类型（球面） | 矢量切片（.mvt） |
| **索引类型** | GiST（R-Tree 变体）+ SP-GiST | H3 网格索引（整数编码） | S2 Cell 索引（Hilbert 曲线） | MapBox 矢量切片索引 |
| **空间函数** | ST_* 族 1000+ | H3_* 函数（覆盖/距离/边界） | S2_* 函数（包含/覆盖/最近邻） | GeoJSON 处理 |
| **坐标系统** | 平面 + 球面（geography） | 球面（地球） | 球面（地球） | 平面（墨卡托） |
| **距离计算** | ST_Distance / ST_DWithin / Haversine | H3_Distance / H3-grid-dist | S2_Distance | ST_Distance |
| **最大精度** | 取决于数据类型（float8） | 15 级分辨率（~0.5cm - 1000km） | S2 Cell 30 级（~1cm - 50000km） | 取决于切片级别 |
| **持久化/WAL** | PostgreSQL WAL + PostGIS 扩展 | 无（WASM/JS/Java/C 库） | 无（仅库） | MapBox 服务端 |
| **特有功能** | 拓扑 / 网络分析 / 3D 支持 / 光栅 | 六边形空间聚合 / Uber 原生 | 球面几何 / 空间索引 | 矢量切片 / Mapbox GL |

### 6.2 开源产品对比

| 维度 | DuckDB Spatial | SpatiaLite | GeoMesa | OpenGIS |
|------|--------------|-----------|---------|---------|
| **几何类型** | Point/LineString/Polygon/Multi* | Point/LineString/Polygon/Multi* | Point/LineString/Polygon | OGC 标准几何 |
| **索引类型** | R-Tree（Spatial Join 优化） | R-Tree（GPKG/SHP） | Z-Order /时空索引（Accumulo/Cassandra/HBase） | GiST |
| **空间函数** | ST_* 族（子集） | ST_* 族（完整） | ST_* 族（子集） | OGC 标准函数 |
| **坐标系统** | 平面 + 球面 | 平面 + 球面（via PROJ） | 平面 + 球面 | 平面 + 球面 |
| **数据格式** | GeoParquet / GeoJSON / SHP | GPKG / SHP / KML / GeoJSON | GeoMesa Z-Order | WKT/WKB/GML/KML |
| **持久化/WAL** | 内嵌（无 WAL） | SQLite WAL | 底层分布式存储 WAL | 依赖数据库 |
| **特有功能** | Parquet 生态集成 / 列式加速 | 独立 SQLite / 便携 | 分布式时空查询 / 物联网 | 标准合规 |

### 6.3 自实现 vs 业界差距分析

| 维度 | 自实现 | 业界最强 | 差距 | 说明 |
|------|--------|---------|------|------|
| **几何类型** | Point/LineString/Polygon + bbox | PostGIS: 1000+ ST_* 函数，完整 OGC | 中等 | 缺少 Multi*/GeometryCollection/CircularString |
| **索引类型** | R-Tree + Hilbert 曲线 | PostGIS GiST / H3 网格 / S2 Cell | 中等 | R-Tree 核心已实现 |
| **空间函数** | ST_Distance / ST_Within / ST_Intersects | PostGIS: ST_* 族 1000+ | 较大 | 缺少缓冲区/凸包/Union/Difference 等 |
| **坐标系统** | 平面（Cartesian） | PostGIS: 平面 + 球面（geography）；S2: 球面 | 较大 | 缺少地理坐标和 Haversine 距离 |
| **距离计算** | 平面欧氏距离 | Haversine / Vincenty / Geodesic | 较大 | 缺少球面距离计算 |
| **空间分析** | bbox 查询 | PostGIS: 拓扑/网络分析/3D；H3: 六边形聚合 | 很大 | 无空间分析能力（Union/Intersect 等） |
| **数据格式** | 内部格式 | GeoParquet / GeoJSON / GPKG / WKT | GeoParquet / GeoMesa 格式 | OGC 标准格式 |
| **Hilbert 曲线** | 支持（Hilbert 曲线） | S2: Hilbert 曲线；H3: 六边形 | 无差距 | Hilbert 曲线与业界一致 |

**Gap 优先级排序**：
1. **空间函数**（缺少 900+ ST_* 函数）
2. **空间分析**（无 Union/Intersect/Buffer 等）
3. **球面坐标**（无地理坐标支持）
4. **坐标系统**（仅平面，无球面）
5. **几何类型**（缺少 Multi*/GeometryCollection）

---

## 七、KV 模型（Key-Value）

### 7.1 商用产品对比

| 维度 | Redis 7.x（Enterprise） | etcd（CoreOS/云原生） | FoundationDB | DynamoDB |
|------|------------------------|---------------------|--------------|----------|
| **最大 Key 大小** | 512 MB（STRING） | 1 MB | 10 KB | 400 KB |
| **最大 Value 大小** | 512 MB（STRING）/ 无限制（HYPERLOGLOG/BITMAP/LIST/SET/HASH） | 1 MB | 10 MB | 400 KB（单项目）/ 400 KB（单属性） |
| **索引类型** | 无序（STRING）/ 有序（SORTED SET ZADD） | 有序（Raft Log 索引） | 有序（Range Read） | 主键（分区键+排序键）/ GSI |
| **持久化策略** | RDB 快照 + AOF（重写/追加）/ 混合（Redis 7.2+） | Raft Log + BoltDB（快照） | Redo Log + SQL Layer + 分布式事务 | SSD 存储 + 自动多副本 + PITR |
| **WAL** | AOF（Write-Ahead Log） | Raft Log（本身就是 WAL） | Redo Log（ACID 保证） | 内置（不可见） |
| **TTL/过期** | EX/EXAT/PX/PXAT/TTL/PERSIST | lease（TTL + 续约机制） | 元组过期（Server 组配置） | 自动过期（TTL 属性） |
| **事务支持** | MULTI/EXEC（乐观）/ WATCH（乐观 CAS） | 事务（单条目或批量）/ Txn（2PC-like） | ACID 事务（KV 层）+ SQL 层 | TransactWriteItems（原子多项目） |
| **分布式架构** | Redis Cluster（16384 slot 分片）/ Enterprise Flash | Raft 共识（3-5 节点）/ 选主 | 分布式事务 + 无单点故障 | 完全托管（自动分片/多 AZ/多区域） |
| **写入吞吐量** | ~100-200 万 OPS（集群） | ~10-50 万 OPS | ~100-500 万 OPS（集群） | ~数百万 OPS（无服务器） |
| **特有功能** | Stream / Pub/Sub / Module / BloomFilter / Redis Stack | 分布式锁 / Leader Election / Watch 机制 | 混合事务（KV+SQL）/ 主动故障转移 | DAX（内存缓存）/ Streams / Global Tables |

### 7.2 开源产品对比

| 维度 | RocksDB | LevelDB | BoltDB | etcd（OSS） | Badger |
|------|---------|---------|--------|------------|--------|
| **最大 Key 大小** | 3B（字节） | 16KB | 10 MB | 1 MB | 无限制 |
| **最大 Value 大小** | 无限制（但建议 < 1GB） | 无限制（256KB 建议） | 10 MB | 1 MB | 无限制 |
| **索引类型** | 有序（SSTable + MemTable） | 有序（MemTable + SSTable） | 有序（B+Tree，MVCC） | 有序（Raft Log 索引） | 有序（LSM-tree，内存索引） |
| **持久化策略** | WAL + MemTable + SSTable + 压缩 | WAL + MemTable + SSTable + 压缩 | 单一文件（mmap）+ MVCC | Raft Log + BoltDB 快照 | WAL + LSM-tree + 压缩 |
| **WAL** | 支持（Write-Ahead Log） | 支持（Log 文件） | 支持（通过 DB 文件） | Raft Log（WAL 等价） | 支持 |
| **TTL/过期** | 支持（每个 KV 级别） | 不支持 | 不支持 | 支持（lease） | 支持（TTL/过期） |
| **事务支持** | 乐观 CAS（TransactionDB） | 不支持 | 单写多读（MVCC） | 事务（单条目或批量） | 事务（ACID） |
| **分布式** | 无（单进程） | 无（单进程） | 无（单进程） | Raft 共识（3-5 节点） | 无（单进程）+ Dgraph 集群 |
| **写入吞吐** | ~10-50 万写/秒 | ~10 万写/秒 | ~1 万写/秒 | ~10-50 万 OPS | ~10-30 万写/秒 |
| **特有功能** | Column Family / Merge Operator / 压缩 | 只读快照 | 事务批处理 / 读视图 | 服务发现 / 分布式锁 / ConfigMap |  LSM 分离（v3） / 垃圾回收 |

### 7.3 自实现 vs 业界差距分析

| 维度 | 自实现 | 业界最强 | 差距 | 说明 |
|------|--------|---------|------|------|
| **Key 大小** | 有限（VecPage 16KB） | Redis: 512MB；FoundationDB: 10KB | 中等 | 需支持更大的 Key |
| **Value 大小** | VecPage 16KB | Redis: 512MB；RocksDB: 无限制 | 较大 | 需支持更大的 Value（如文档） |
| **有序存储** | 支持（游标扫描） | RocksDB/LevelDB/BoltDB: 完整有序 | 无差距 | 游标迭代器已实现 |
| **持久化** | WAL + 页面持久化 | RocksDB: WAL + SSTable + 压缩 | 中等 | 缺少 SSTable 和压缩 |
| **WAL** | 支持 | Redis AOF / RocksDB WAL / etcd Raft Log | 无差距 | WAL 机制与业界一致 |
| **TTL** | 支持（基于 Segment 过期） | Redis/etcd/FoundationDB: TTL/lease | 无差距 | TTL 机制已实现 |
| **事务支持** | 无 ACID 事务 | FoundationDB: ACID 事务；Redis: MULTI/EXEC | 很大 | 无并发控制和一致性保证 |
| **分布式** | 单机 | Redis Cluster / etcd Raft / DynamoDB | 很大 | 无集群和分片能力 |
| **并发控制** | 基础锁 | FoundationDB: 乐观 CAS；Redis: WATCH | 很大 | 无 MVCC 或 CAS |

**Gap 优先级排序**：
1. **事务支持**（无 ACID 事务和并发控制）
2. **分布式架构**（无集群和分片）
3. **Value 大小**（需支持大 Value）
4. **SSTable + 压缩**（缺少高效持久化格式）
5. **Column Family**（无多命名空间隔离）

---

## 八、树形模型（Yang/Tree）

### 8.1 商用产品对比

| 维度 | MarkLogic | BaseX | eXist-db | Oxygen XML |
|------|----------|-------|----------|------------|
| **数据模型** | XML/JSON + 语义图（三元组） | XML 文档数据库 | XML 文档数据库 | XML 编辑器（非数据库） |
| **存储格式** | 文档存储（Forest）+ 索引（片段索引） | 磁盘文件存储 + 内存索引 | 原子化存储（DOM fragment）/ 数据库集合 | - |
| **查询语言** | XQuery / SPARQL / SQL / JavaScript | XQuery / XQuery Update / Full-text | XQuery / XPath / XSLT / RESTXQ | - |
| **索引类型** | 范围索引 + 元素索引 + 全文索引 + 三元组索引 | 路径索引 + 值索引 + 全文索引 | 范围索引 + 全文索引 + 旧索引 | - |
| **事务支持** | MVCC 多版本并发控制 | 文档级事务 | 文档级事务 | - |
| **持久化/WAL** | Journal（WAL）+ Checkpoint | 文件系统 + 定期备份 | 数据库集合 + 备份 | - |
| **特有功能** | 三元组存储 + 语义推理 / 灵活 Schema / ACID | 轻量 / 高速 XQuery / 全文搜索 | REST API / WebDAV / XSLT 变换 | - |

### 8.2 开源生态对比

| 维度 | libyang（OpenConfig） | sysrepo | PostgreSQL ltree | SQL Server HierarchyID |
|------|----------------------|---------|------------------|----------------------|
| **数据模型** | YANG 数据建模语言（树结构） | libyang 的存储层（NETCONF 数据存储） | 层次路径（materialized path） | 层次结构（binary encoding） |
| **存储格式** | libyang 树结构 + 内存 | 关系表（SQLite/MySQL/PDO） | PostgreSQL 文本列（路径字符串） | SQL Server binary |
| **查询语言** | YANG XPath / C + Python bindings | sysrepo API（C）/ YANG XPath | SQL + ltree 操作符（<,>,~,@） | T-SQL（HierarchyID 方法） |
| **索引类型** | 路径索引（内部） | B-Tree（底层数据库） | GiST 索引（ltree_ops） | 聚集索引（HierarchyID） |
| **事务支持** | 依赖底层存储 | 依赖底层数据库事务 | PostgreSQL MVCC | SQL Server MVCC |
| **持久化/WAL** | 可选文件持久化 | 底层数据库持久化 | PostgreSQL WAL | SQL Server WAL |
| **RFC 参考** | RFC 6020/7950 | RFC 6241（NETCONF） | - | - |
| **特有功能** | YANG 1.1 / 数据验证 / 路径解析 | 数据订阅/推送 / 事务性操作 | 祖先/后代查询 / 路径操作 | GetAncestor/GetDescendant/GetLevel |

### 8.3 自实现 vs 业界差距分析

| 维度 | 自实现 | 业界最强 | 差距 | 说明 |
|------|--------|---------|------|------|
| **数据模型** | 层次树节点（NodeType + 父子关系） | MarkLogic: XML+三元组；libyang: YANG 规范 | 中等 | 缺少 XML/XPath/YANG 语义支持 |
| **查询语言** | 路径查询（API 级别） | XQuery / YANG XPath / ltree SQL | 较大 | 无声明式查询语言 |
| **索引类型** | 基础父子索引 | ltree GiST 索引 / libyang 路径索引 | 中等 | 缺少 GiST 索引优化 |
| **事务支持** | 无 ACID 事务 | PostgreSQL/BaseX: MVCC | 很大 | 无并发控制 |
| **持久化** | WAL + 页面持久化 | MarkLogic: Journal + Checkpoint | 较小 | WAL 机制已实现 |
| **RFC 标准** | 无 | libyang: RFC 6020/7950 NETCONF | 很大 | 缺少 YANG/NETCONF 标准合规 |
| **XPath 查询** | 基础路径匹配 | libyang: 完整 XPath；BaseX: XQuery | 较大 | 无完整 XPath 支持 |
| **分布式** | 单机 | sysrepo: 可选分布式（依赖底层 DB） | 较大 | 无集群能力 |

**Gap 优先级排序**：
1. **查询语言**（无 XQuery/XPath/YANG）
2. **RFC 标准合规**（无 YANG/NETCONF）
3. **事务支持**（无 MVCC）
4. **GiST 索引**（无空间优化索引）
5. **分布式**（无集群能力）

---

## 九、关系模型（Relational）

### 9.1 商用产品对比

| 维度 | PostgreSQL 17 | MySQL 9.0 | Oracle 23ai | SQL Server 2022 |
|------|--------------|-----------|------------|----------------|
| **存储引擎** | Heap + TOAST + 堆表/追加优化 | InnoDB（默认）/ MyISAM/Archive | In-Memory OLTP + Blockchain | Hekaton + 传统行存储 |
| **索引类型** | B-Tree + GiST + GIN + BRIN + Hash + SP-GiST + 覆盖索引 | B-Tree + Hash + FULLTEXT + R-Tree（InnoDB） | B-Tree + Bitmap + 函数索引 + JSON 索引 | B-Tree + Columnstore + XML + 空间索引 |
| **查询语言** | SQL:2023（大部分） | SQL:2023（部分） | SQL:2023 + PL/SQL | T-SQL + SQL:2023 |
| **执行器** | Volcano 迭代器 + 动态规划优化 | Volcano + 代价优化 | Volcano + 自适应 | Volcano + 自适应 |
| **并发控制** | MVCC（SSI）+ 2PL | MVCC（Undo）+ 锁 | MVCC + 乐观并发 | MVCC + 锁 |
| **持久化/WAL** | WAL + Checkpoint + PITR | InnoDB Redo + Undo + Doublewrite | Online REDO + Archived LOG + PITR | WAL + Checkpoint + Always On |
| **分布式** | Citus / Greenplum / Aurora（AWS） | MySQL Group Replication / Vitess | RAC（共享存储）/ ExaDB（云） | Always On 可用性组 / Azure Synapse |
| **特有功能** | JSONB / pgvector / FDW / 扩展 / MVCC SSI | JSON / Group Replication / MySQL HeatWave | JSON / Blockchain Table / In-Memory | Graph Database / In-Memory / PolyBase |

### 9.2 开源产品对比

| 维度 | DuckDB | SQLite | MariaDB | ClickHouse |
|------|--------|--------|---------|------------|
| **存储引擎** | 列式（Parquet 生态） | B-Tree + Page（追加优化） | InnoDB / Aria / ColumnStore | MergeTree（列式） |
| **索引类型** | 压缩排序 + 物化索引 | B-Tree + WITHOUT ROWID + R-Tree | B-Tree + Hash + FULLTEXT | 主键索引 + 跳数索引 |
| **查询语言** | SQL:2023（大部分） | SQL:2023（大部分） | SQL:2023 + MariaDB 扩展 | SQL:2023 + ClickHouse 扩展 |
| **执行器** | 火山 + 向量化 | 简单迭代 | 火山 | 火山 + 向量化 |
| **并发控制** | MVCC（Read Committed） | 读者写者锁 + WAL | MVCC（Undo）+ 锁 | 副本级 MVCC |
| **持久化/WAL** | 无 WAL（但 ACP）/ Parquet 持久化 | WAL 模式 + Checkpoint | InnoDB Redo + Undo + MariaDB Galera | WAL + MergeTree + 异步合并 |
| **分布式** | 单机（但支持 Parquet S3） | 无（单文件） | Galera Cluster（多主） | 原生分片 + 副本 |
| **特有功能** | OLAP 加速 / Parquet/CSV 原生 / 向量索引 | 嵌入式 / 无服务器 / 零配置 | Galera 多主 / ColumnStore 列式 | 列式 / 物化视图 / TTL |

### 9.3 自实现 vs 业界差距分析

| 维度 | 自实现 | 业界最强 | 差距 | 说明 |
|------|--------|---------|------|------|
| **物理算子** | 8 个（SeqScan/IndexScan/NestLoop/HashJoin/HashAgg/Sort/Limit/Gather） | PostgreSQL: 15+；ClickHouse: 20+ | 中等 | 核心算子已实现，缺少 HashAgg 以外聚合 |
| **优化器** | 简单代价模型 + 规则优化 | PostgreSQL: 动态规划 + 遗传算法 | 中等 | 缺少自适应查询和并行优化 |
| **MVCC** | 无 MVCC | PostgreSQL: SSI MVCC；MySQL: Undo MVCC | 很大 | 无并发快照读 |
| **事务** | 基础事务（无 ACID 完整保证） | PostgreSQL: 完整 ACID | 很大 | 无完整的 MVCC 事务 |
| **索引类型** | B-Tree（IndexScan） | PostgreSQL: 6 种；MySQL: 4 种 | 中等 | 缺少 GiST/GIN/Hash 等 |
| **WAL** | 支持（db/wal.h） | PostgreSQL WAL + Checkpoint | 无差距 | WAL 机制已实现 |
| **持久化** | Buffer Pool + WAL | PostgreSQL: Heap + TOAST + Checkpoint | 中等 | 缺少 TOAST（大对象）和堆表管理 |
| **分布式** | 单机 | PostgreSQL + Citus；MySQL + Vitess | 很大 | 无集群和分片 |
| **SQL 标准** | 基础 SELECT/INSERT/UPDATE/DELETE | PostgreSQL: SQL:2023 大部分 | 中等 | 缺少窗口函数、CTE（WITH）、LATERAL 等 |
| **并行执行** | Gather（基础并行）+ Worker Pool | PostgreSQL: 并行 SeqScan/HashJoin/Aggregate | 中等 | 基础并行已有，缺少并行索引 |

**Gap 优先级排序**：
1. **MVCC + ACID 事务**（最大差距）
2. **SQL 标准完整性**（缺少窗口函数/CTE/LATERAL）
3. **优化器**（缺少动态规划和自适应）
4. **索引类型**（缺少 GiST/GIN/Hash 等）
5. **分布式架构**（无集群分片）

---

## 十、综合差距分析

### 10.1 各模态 Gap 雷达图（定性评分 0-10，10 为最强）

| 维度 | Vector | Graph | Timeseries | Document | Spatial | KV | Tree | Relational |
|------|--------|-------|------------|----------|---------|-----|------|------------|
| **索引完整性** | 7 | 5 | 6 | 5 | 5 | 6 | 4 | 6 |
| **查询能力** | 7 | 4 | 6 | 4 | 4 | 5 | 3 | 5 |
| **持久化/WAL** | 8 | 6 | 7 | 6 | 5 | 7 | 6 | 7 |
| **分布式** | 2 | 2 | 2 | 2 | 2 | 2 | 2 | 2 |
| **事务支持** | 5 | 2 | 5 | 3 | 3 | 2 | 2 | 3 |
| **性能吞吐** | 7 | 6 | 5 | 5 | 5 | 6 | 5 | 5 |
| **API 设计** | 6 | 3 | 6 | 5 | 5 | 6 | 4 | 6 |

### 10.2 关键 Gap 汇总

| 优先级 | Gap | 影响模态 | 建议 |
|--------|-----|---------|------|
| **P0** | 无 MVCC + ACID 事务 | Graph, KV, Relational | 引入 MVCC 或乐观 CAS |
| **P0** | 无分布式架构 | 所有模态 | 实现分片 + 共识协议 |
| **P1** | 无 GPU 加速 | Vector | 引入 CUDA 支持（向量库） |
| **P1** | 无磁盘索引 | Vector | 实现 DiskANN/Vamana |
| **P1** | 无查询语言 | Graph, Tree | 引入 Cypher/GQL 或 XQuery |
| **P1** | 图分析算法缺失（96+） | Graph | 集成 NetworkX 或自实现核心算法 |
| **P2** | 无物化视图/持续查询 | Timeseries | 实现物化视图框架 |
| **P2** | 缺少聚合管道 | Document | 实现 MongoDB 风格聚合管道 |
| **P2** | 无空间分析函数 | Spatial | 补充 ST_Union/Intersect/Buffer |
| **P2** | SQL 标准不完整 | Relational | 实现窗口函数/CTE/LATERAL |
| **P3** | 无 GraphRAG 集成 | Graph | 与向量引擎联动实现 GraphRAG |
| **P3** | 无 Column Family | KV | 引入 Column Family 支持 |

### 10.3 量化性能对比

| 模态 | 指标 | 自实现 | 业界最强 | 差距倍数 |
|------|------|--------|---------|---------|
| **Vector** | 写入吞吐 | ~158K vec/s | FAISS GPU: ~500K+ vec/s | ~3x |
| **Vector** | 搜索 QPS | ~5K QPS | FAISS GPU_IVF: ~200K QPS | ~40x |
| **Vector** | 召回率 | ~0.99 @ 100K | ~0.99 @ 1M | 同级 |
| **KV** | 写入吞吐 | ~10-50K OPS | Redis Cluster: ~200万 OPS | ~40x |
| **Graph** | 遍历速度 | 取决于实现 | Memgraph: ~1M 边/秒 | 未知 |
| **Timeseries** | 写入吞吐 | ~50-100K 点/秒 | QuestDB: ~100万 行/秒 | ~10x |

---

## 十一、选型建议

### 11.1 短期（M1-M3）优先实现

| 功能 | 原因 | 影响 |
|------|------|------|
| **MVCC 事务** | KV/Graph/Relational 都需要，是核心基础 | 解锁 ACID 场景 |
| **Cypher 查询语言** | 图数据库标准 API，降低用户门槛 | 解锁图查询场景 |
| **Binary Quantization** | 内存压缩 32x，成本降低 | 向量降本增效 |
| **窗口函数 + CTE** | SQL 标准核心，生态工具依赖 | SQL 生态兼容 |

### 11.2 中期（M4-M6）能力补齐

| 功能 | 原因 | 影响 |
|------|------|------|
| **分布式分片** | 所有模态水平扩展基础 | 解锁大规模场景 |
| **Raft 共识协议** | 分布式一致性基础 | 解锁多副本高可用 |
| **GPU 向量加速** | 10-40x 吞吐量提升 | 性能飞跃 |
| **物化视图框架** | 时序降采样核心功能 | 解锁时序分析 |
| **图分析算法库** | 图数据库核心能力 | 解锁图分析场景 |

### 11.3 长期（M7+）生态建设

| 功能 | 原因 | 影响 |
|------|------|------|
| **DiskANN 磁盘索引** | 十亿级向量支持 | 解锁超大规模向量 |
| **GraphRAG 集成** | AI/LLM 场景标准方案 | 解锁 RAG 场景 |
| **YANG/NETCONF 标准** | 网络配置管理标准 | 解锁网络自动化场景 |
| **多租户隔离** | SaaS 化基础 | 解锁多用户场景 |

---

*文档版本: 2026-08-25*
*数据来源: 各官方文档、ann-benchmarks.com、Graph500、行业评测报告*
