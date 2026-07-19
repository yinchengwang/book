# 数据库学习 Wiki

> 为 `reference/open-source/` 下 53 个开源数据库编写的学习笔记，采用分层混合模板结构。

## 目录结构

```
docs/db_wiki/
├── README.md                    # 本文件：总览 + 模板说明
├── 01_relational/               # 关系型数据库
│   ├── postgres/                # 每个数据库一个子目录
│   ├── mysql/
│   ├── tidb/
│   ├── sqlite3/
│   ├── duckdb/
│   ├── cockroach/
│   ├── oceanbase/
│   ├── opengauss/
│   ├── stonedb/
│   └── README.md               # 该类别横向对比
├── 02_key_value/                # KV 存储/内存数据库
│   ├── redis/
│   ├── dragonfly/
│   ├── etcd/
│   ├── garnet/
│   ├── nats/
│   └── README.md
├── 03_vector/                   # 向量数据库/索引库
│   ├── faiss/
│   ├── milvus/
│   ├── qdrant/
│   ├── chroma/
│   ├── weaviate/
│   ├── pgvector/
│   ├── vald/
│   ├── lancedb/
│   └── README.md
├── 04_graph/                     # 图数据库
│   ├── neo4j/
│   ├── nebula/
│   ├── arangodb/
│   ├── dgraph/
│   ├── janusgraph/
│   └── README.md
├── 05_time_series/              # 时序数据库
│   ├── timescaledb/
│   ├── questdb/
│   ├── greptimedb/
│   ├── victoria_metrics/
│   └── README.md
├── 06_search_engine/            # 搜索引擎
│   ├── elasticsearch/
│   ├── meilisearch/
│   ├── tantivy/
│   ├── zincsearch/
│   ├── paradedb/
│   └── README.md
├── 07_analytical/               # 分析型数据库
│   ├── clickhouse/
│   ├── druid/
│   ├── starrocks/
│   └── README.md
├── 08_streaming/                # 流式数据库
│   ├── redpanda/
│   ├── risingwave/
│   └── README.md
├── 09_embedded/                 # 嵌入式数据库
│   ├── leveldb/
│   ├── rocksdb/
│   ├── badger/
│   └── README.md
├── 10_document/                 # 文档数据库
│   ├── ferretdb/
│   ├── appwrite/
│   ├── nocodb/
│   └── README.md
├── 11_multi_model/              # 多模型数据库
│   ├── surrealdb/
│   └── README.md
├── 12_versioned/                # 版本化数据库
│   ├── dolt/
│   └── README.md
├── 13_edge/                     # 边缘数据库
│   ├── turso/
│   └── README.md
├── 14_benchmark/                # 基准测试
│   ├── ann_benchmarks/
│   └── README.md
├── 15_devops/                   # DevOps 工具
│   ├── bytebase/
│   └── README.md
└── 00_template/                 # 模板文件（供参考，不直接使用）
    ├── 01_relational/
    │   └── sample_db/
    ├── 02_key_value/
    │   └── sample_db/
    ├── 03_vector/
    │   └── sample_db/
    ├── 05_time_series/
    │   └── sample_db/
    └── 04_graph/
        └── sample_db/
```

## 模板族设计

按数据模型类别设计 5 套模板族，每套模板族定义了该类别数据库的子目录结构和文件命名约定。

### 模板族 1：关系型数据库

适用于：postgres, mysql, tidb, cockroach, sqlite3, duckdb, oceanbase, stonedb, opengauss

```
01_relational/<db_name>/
├── 00_overview.md          # 项目概览 + 学习路线图
├── 01_architecture.md      # 整体架构（含架构图）
├── 02_storage/             # 存储引擎层
│   ├── buffer_pool.md      # Buffer Pool 实现
│   ├── heap_table.md       # 堆表存储
│   ├── page_layout.md      # 页面结构
│   └── wal.md              # WAL 日志
├── 03_transaction/         # 事务系统
│   ├── mvcc.md             # MVCC 实现
│   ├── locking.md          # 锁机制
│   └── isolation.md        # 隔离级别
├── 04_query/               # 查询引擎
│   ├── parser.md           # SQL 解析
│   ├── planner.md          # 查询规划/优化器
│   └── executor.md         # 执行器
├── 05_index/               # 索引算法
│   ├── btree.md            # B+Tree / BTree
│   ├── hash.md             # Hash 索引
│   └── other_indexes.md    # 其他索引（GIN/GIST/BRIN等）
├── 06_features.md          # 关键特性（分区、并行查询、扩展机制等）
├── 07_use_cases.md         # 使用场景与选型对比
├── 08_experiments.md       # 动手实验（部署、CRUD、benchmark）
├── 09_resources.md         # 学习资源（文档、论文、博客、视频）
└── 10_project_connection.md # 与我项目的关联与启发
```

### 模板族 2：向量数据库/索引库

适用于：faiss, milvus, qdrant, chroma, weaviate, pgvector, vald, lancedb

```
03_vector/<db_name>/
├── 00_overview.md          # 项目概览 + 学习路线图
├── 01_architecture.md      # 整体架构（含架构图）
├── 02_index/               # 索引算法
│   ├── flat.md             # 暴力搜索
│   ├── ivf.md              # IVF（倒排文件索引）
│   ├── hnsw.md             # HNSW（分层可导航小世界图）
│   ├── pq.md               # 量化类（PQ/LVQ/OPQ）
│   └── diskann.md          # DiskANN（基于磁盘的 ANN）
├── 03_search.md            # 搜索执行流程
├── 04_features.md          # 关键特性（GPU加速、分布式、过滤、混合搜索）
├── 05_use_cases.md         # 使用场景与选型对比
├── 06_experiments.md       # 动手实验（部署、索引构建、召回率测试）
├── 07_resources.md         # 学习资源
└── 08_project_connection.md # 与我项目的关联与启发
```

### 模板族 3：KV 存储/内存数据库

适用于：redis, dragonfly, garnet, etcd, nats

```
02_key_value/<db_name>/
├── 00_overview.md          # 项目概览 + 学习路线图
├── 01_architecture.md      # 整体架构（含架构图）
├── 02_data_structures/     # 核心数据结构
│   ├── sds.md              # SDS（简单动态字符串）
│   ├── dict.md             # 哈希表
│   ├── list.md             # 链表/压缩列表
│   ├── skiplist.md         # 跳表
│   └── object_system.md    # 对象系统
├── 03_persistence/         # 持久化
│   ├── rdb.md              # RDB（快照）
│   └── aof.md              # AOF（追加日志）
├── 04_features.md          # 关键特性（复制、哨兵、集群、管道、事务）
├── 05_use_cases.md         # 使用场景与选型对比
├── 06_experiments.md       # 动手实验
├── 07_resources.md         # 学习资源
└── 08_project_connection.md # 与我项目的关联与启发
```

### 模板族 4：时序数据库

适用于：timescaledb, questdb, greptimedb, victoria_metrics

```
05_time_series/<db_name>/
├── 00_overview.md          # 项目概览 + 学习路线图
├── 01_architecture.md      # 整体架构（含架构图）
├── 02_storage/             # 存储引擎
│   ├── columnar_format.md  # 列式存储格式
│   ├── compression.md      # 压缩算法
│   └── partitioning.md     # 分区/分片策略
├── 03_query/               # 查询引擎
│   ├── time_aggregation.md # 时间聚合
│   └── downsampling.md     # 降采样
├── 04_features.md          # 关键特性
├── 05_use_cases.md         # 使用场景与选型对比
├── 06_experiments.md       # 动手实验
├── 07_resources.md         # 学习资源
└── 08_project_connection.md # 与我项目的关联与启发
```

### 模板族 5：图数据库

适用于：neo4j, nebula, arangodb, dgraph, janusgraph

```
04_graph/<db_name>/
├── 00_overview.md          # 项目概览 + 学习路线图
├── 01_architecture.md      # 整体架构（含架构图）
├── 02_storage/             # 存储引擎
│   ├── graph_storage.md    # 图存储结构（邻接表/邻接矩阵/属性图）
│   └── indexing.md         # 索引机制
├── 03_traversal/           # 遍历引擎
│   ├── query_language.md   # 查询语言（Cypher/Gremlin/SPARQL）
│   └── query_engine.md     # 查询执行流程
├── 04_algorithms.md        # 图算法（最短路径、PageRank、社区发现等）
├── 05_features.md          # 关键特性
├── 06_use_cases.md         # 使用场景与选型对比
├── 07_experiments.md       # 动手实验
├── 08_resources.md         # 学习资源
└── 09_project_connection.md # 与我项目的关联与启发
```

### 其他类别适配说明

其余类别（搜索引擎、分析型、流式、嵌入式、文档、多模型、版本化、边缘、基准测试、DevOps）从以上模板族派生，核心原则：

- **搜索引擎**：参考模板族 2（向量索引） + 模板族 1（查询引擎）
- **分析型（ClickHouse/Druid/StarRocks）**：参考模板族 4（时序） + 模板族 1（关系型查询）
- **流式**：参考模板族 1（存储） + 增加流处理/物化视图模块
- **嵌入式（LevelDB/RocksDB/Badger）**：参考模板族 3（KV 存储），简化为 LSM-Tree 结构
- **文档数据库**：参考模板族 1，增加 JSON/文档模型模块
- **多模型/版本化/边缘**：按实际特性从相近模板族裁剪

## 每个 Wiki 文件的必写内容

每个 `.md` 文件必须包含以下内容：

1. **学习目标** — 这个文件要搞清楚什么
2. **核心概念** — 关键术语和概念解释
3. **主体内容** — 多级标题组织，配 Mermaid 图
4. **要点总结** — 核心要点回顾
5. **思考题** — 2-3 个引发思考的问题

### Mermaid 图使用规范

```
graph TD / flow chart / sequenceDiagram / classDiagram
```

- 每个重要概念都必须配图
- 不要只画高层的方框图，要画出数据流、控制流或状态转换
- 图要自解释（有中文标注），不要只靠正文来理解图

## 模板示例文件

详见 `docs/db_wiki/_template/` 目录下的示例文件，展示了每个模板族各文件的预期内容密度和 Mermaid 图密度。