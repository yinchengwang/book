# 变更提案：mmdb-htap-evolution

## Why

当前多模态数据库虽然已实现 8 种数据模型（关系、KV、向量、时序、图、文档、空间、树）的存储引擎，但缺乏关键的 HTAP（混合事务/分析处理）能力：

1. **缺乏并发事务控制**：单写者模型限制了并发性能，缺少 MVCC 导致读写互相阻塞
2. **缺乏统一 SQL 层**：各引擎独立 API，无法用统一查询语言跨模型查询
3. **缺乏跨模型联合查询**：无法实现"向量+关系"、"图+时序"等异构模型关联查询
4. **各模型能力不均衡**：向量、时序、图等模型的查询能力相比商业产品（如 Milvus、TimescaleDB、Neo4j）仍有差距
5. **缺乏标准网络协议**：无法直接使用业界生态工具（psql、JDBC、BI 工具）访问数据库

本变更是将多模态数据库演进为完整 HTAP 数据库的关键步骤，通过自研深入学习业界最佳实践。

## What Changes

### Phase 1: 核心基础设施增强（4-6 周）
- 实现 MVCC 多版本并发控制，支持 RC/SI/SSI 可配置隔离级别
- 升级锁管理器，实现行级锁和死锁检测
- 增强数据完整性，页面 checksum 校验和自修复机制
- 完善物化视图，支持增量刷新和多视图链式依赖

### Phase 2: SQL 层 + 网络协议（6-8 周）
- 实现完整 SQL 解析器（词法分析、语法分析、语义分析）
- 实现查询计划器（逻辑计划、物理计划、代价优化）
- 实现 Volcano 迭代器模型的执行引擎
- 实现 PostgreSQL Wire Protocol（pgwire），支持标准 PostgreSQL 客户端
- 实现 REST API（管理、监控接口）
- 开发 Python SDK 和 JDBC Driver

### Phase 3: 向量模型增强（4-6 周）
- 实现 DiskANN 磁盘友好向量索引（NSG 图）
- 实现混合检索（向量 + 属性过滤，pre-filter/post-filter）
- 增强量化能力（SQ、OPQ、自适应量化参数）
- 实现分层索引（热：HNSW/内存，温：IVF-PQ/SSD，冷：IVF/对象存储）
- 新增 `VECTOR_SEARCH()` SQL 扩展函数

### Phase 4: 时序模型增强（4-6 周）
- 实现时序专用列式存储引擎（Delta + RLE + Gorilla 压缩）
- 实现时序专用索引（时间分区、Tag 倒排索引、降采样索引）
- 实现时序 SQL 函数库（`TIME_SERIES_AGG`、`FIRST/LAST`、`RATE` 等）
- 实现连续聚合（Continuous Aggregate），支持实时增量聚合
- 增强数据保留策略（分层保留 hot/cold/archive）

### Phase 5: 图模型增强（4-6 周）
- 实现属性图支持（顶点/边属性、Schema 定义）
- 实现图查询语言（openCypher 兼容子集 + `GRAPH_TRAVERSE()` SQL 扩展）
- 实现图索引（标签索引、属性索引、向量属性索引）
- 实现图算法库（BFS/DFS、Shortest Path、PageRank、Louvain 等）
- 实现动态图支持（增量更新、版本快照、时间旅行查询）

### Phase 6: 文档模型增强（4-6 周）
- 实现嵌套文档支持（嵌套对象/数组、JSONPath 增强）
- 实现文档向量检索（语义搜索、BM25 + 向量混合检索）
- 实现文档聚合框架（桶聚合、指标聚合、管道聚合）
- 增强全文检索（可插拔分词器、短语搜索、同义词）

### Phase 7: 空间 + KV + 树模型增强（3-4 周）
- 空间模型：地理坐标类型、空间函数（ST_Distance、ST_Contains）、R-Tree + QuadTree
- KV 模型：有序集合（Sorted Set）、范围查询、TTL 增强
- 树模型：层级查询（ancestors/descendants）、XML/JSONPath

### Phase 8: 跨模型联合查询 + OLAP 增强（6-8 周）
- 实现跨模型查询优化器（异构模型 JOIN、跨模型执行计划）
- 实现向量化执行引擎（SIMD 加速、列批处理）
- 实现通用列式存储引擎（分析场景专用）
- SQL 扩展函数统一化（所有模型函数注册到统一系统）

### Phase 9: 分布式演进（8-12 周）
- 实现数据分片和请求路由（Hash 分片、Range 分片）
- 实现分布式事务（2PC、SAGA、分布式 MVCC）
- 实现 Raft 高可用（Leader 选举、日志复制、故障切换）
- 实现多节点协调服务（集群管理、全局锁、配置管理）

## Capabilities

### New Capabilities

- `sql-parser`: SQL 解析引擎，支持 DDL/DML/聚合/JOIN/子查询等完整语法
- `sql-planner`: 查询计划器，生成逻辑和物理执行计划，支持代价优化
- `sql-executor`: Volcano 迭代器模型的查询执行引擎
- `pgwire-protocol`: PostgreSQL Wire Protocol 协议实现，兼容标准客户端
- `rest-api`: REST API 接口，提供管理和监控能力
- `python-sdk`: Python 语言 SDK，支持连接管理、游标、事务
- `jdbc-driver`: JDBC Driver，实现 java.sql.Driver 接口
- `mvcc`: MVCC 多版本并发控制，支持 RC/SI/SSI 隔离级别
- `row-level-lock`: 行级锁和死锁检测机制
- `vector-diskann`: DiskANN 磁盘友好向量索引
- `vector-hybrid-search`: 向量混合检索（支持属性过滤）
- `ts-columnar`: 时序列式存储引擎
- `ts-functions`: 时序 SQL 函数库（聚合、降采样等）
- `ts-continuous-agg`: 时序连续聚合
- `graph-property`: 属性图支持
- `graph-query`: 图查询语言（Cypher 兼容）
- `graph-algorithms`: 图算法库
- `doc-nested`: 嵌套文档支持
- `doc-vector-search`: 文档向量检索
- `doc-aggregations`: 文档聚合框架
- `spatial-functions`: 空间函数库
- `kv-sorted-set`: 有序集合 KV
- `tree-hierarchy`: 层级树查询
- `cross-model-query`: 跨模型联合查询
- `vectorized-execution`: 向量化执行引擎
- `columnar-storage`: 通用列式存储
- `sharding`: 数据分片和路由
- `distributed-transaction`: 分布式事务
- `raft-ha`: Raft 高可用
- `cluster-coordinator`: 集群协调服务

### Modified Capabilities

- `物化视图`: 从当前基础实现增强为支持增量刷新和多视图链式依赖
- `向量引擎`: 增强为支持 DiskANN、分层索引、混合检索
- `时序引擎`: 增强为支持列式存储、时序专用索引、连续聚合
- `图引擎`: 增强为支持属性图、图查询语言、图算法
- `文档引擎`: 增强为支持嵌套文档、向量检索、聚合框架
- `空间引擎`: 增强为支持地理坐标类型和空间函数
- `KV 引擎`: 增强为支持有序集合和范围查询
- `树引擎`: 增强为支持层级查询

## Impact

### 新增目录

```
src/db/
├── sql/
│   ├── parser/          # SQL 解析器
│   ├── planner/         # 查询计划器
│   └── executor/        # 执行引擎
├── server/
│   ├── pgwire/          # PostgreSQL 协议
│   └── rest/            # REST API
├── sdk/
│   ├── python/          # Python SDK
│   └── java/            # JDBC Driver
├── txn/
│   └── mvcc/            # MVCC 实现
├── storage/
│   ├── lock/            # 行级锁
│   ├── columnar/        # 列式存储
│   └── vector/
│       └── diskann/     # DiskANN 索引
├── cluster/
│   ├── shard/           # 分片管理
│   ├── txn/             # 分布式事务
│   ├── raft/            # Raft 共识
│   └── coordinator/     # 协调服务
```

### 修改目录

```
src/db/storage/
├── vector/              # 向量引擎增强
├── ts/                  # 时序引擎增强
├── graph/               # 图引擎增强
├── doc/                 # 文档引擎增强
├── spatial/             # 空间引擎增强
├── kv/                  # KV 引擎增强
├── yang/                # 树引擎增强
└── mm/                  # 物化视图增强
```

### 依赖关系

- SQL 层依赖现有存储引擎，通过统一适配层调用
- pgwire 协议解析结果直接送入 SQL 层
- MVCC 需要与 WAL、Buffer Pool 深度集成
- 跨模型查询需要所有模型引擎支持统一接口

### 部署模式影响

- 新增 `libmmdb-server` 静态库（服务模式）
- 新增 `libmmdb-embed` 静态库（嵌入式模式，与服务模式共享核心）
- 配置文件新增 `postgresql.conf` 兼容格式
