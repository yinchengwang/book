# ArangoDB Wiki（多模型原生图数据库）

> 参考版本：ArangoDB 3.11+（C++ 实现，存储引擎 RocksDB / MMFiles，AQL 多模型查询语言，VelocyPack 二进制 JSON）。本 wiki 重点覆盖图类型专项章节（AQL 多模型、CAPEL 全文索引、Pregel 图算法、Cluster 弹性伸缩）。

## 一、项目定位

ArangoDB 是由 **ArangoDB GmbH**（总部德国科隆）开源的 **多模型原生数据库（Multi-Model Native Database）**，以 **"One Core. Multiple Data Models."** 为口号，提供 **文档（Document）+ 图（Graph）+ 键值（KV）+ 全文检索（Search, IResearch）** 四合一数据模型、统一的 **AQL（ArangoDB Query Language）** 查询语言、**VelocyPack** 自描述二进制 JSON 协议，是数据模型融合场景的代表产品。

- **痛点**：
  - 单一图数据库（Neo4j）难以应对文档与 KV 场景；
  - 多数据库组合（PostgreSQL + Redis + Elasticsearch）运维成本高；
  - 不同模型之间数据流转（join / graph compute）需要自定义 ETL；
  - 中小企业需要"一套核心支持多种数据结构"；
  - 现代 GenAI / RAG / 知识图谱场景需要"文档 + 关系 + 向量融合"；
- **典型用户**：SaaS 平台、电商 O2O、知识图谱、欺诈检测、社交网络、IoT、推荐系统、GraphRAG；
- **核心口号**：**"One Core. Multiple Models."** + **"Gharial: the graph engine for multimodel."** + **"Open the Graph Power, with Document Native."**。

## 二、发展历史与社区

- **起源**：2008 年由 Claudius "Cla" Weinberger 在德国创立；
- **里程碑**：
  - 2009：AvocadoDB 1.0 起步；
  - 2011：改名 ArangoDB 1.0；
  - 2014：2.0 加入图模型 + AQL；
  - 2015：3.0 集成 VelocyPack + 集群模式；
  - 2017：3.3 引入 Pregel 图计算（类 Google Pregel）；
  - 2018：3.4 Foxx Microservices + SmartGraph / SatelliteGraphs；
  - 2019：3.5 RocksDB 存储引擎生产化；
  - 2020：3.6 K8s Operator 与 Active Failover；
  - 2021：3.8 进一步优化 RocksDB + 全文检索（IResearch）；
  - 2023：3.10 多 Raft 复制（Replication2 alpha）+ 向量索引；
  - 2024：3.11 LTS，**Embedded 模式**（C++ 进程内嵌）；
  - 2025：3.12，K8s Operator GA、并发索引构建；
  - 2026：ArangoGraph 商用云、Arango AI 集成；
- **维护状态**：活跃，每月 1+ 发布版本，每周 commit 数十次；
- **社区**：GitHub 13.6k+ star、Slack 8000+ 工程师、社区论坛、ArangoDB Summit 年度大会；
- **生态**：100+ 集成，伙伴关系：Kubernetes、Confluent、Kafka、StreamSets、Prometheus、Grafana。

## 三、许可与商业限制

- **核心代码**：**Business Source License (BSL)** 1.1 → 3 年后转 Apache 2.0（"BSL → Apache"开放模式）；
- **商业版本**：
  - **ArangoDB Enterprise**：闭源，含 SmartGraph 2.0、SatelliteGraphs、Hot Backup、C2K8s Operator GA；
  - **ArangoGraph Insights Platform**：全托管云（AWS / GCP / Azure）；
  - **ArangoDB OEM**：嵌入式 OEM 商业许可证；
- **生态关系**：
  - 与 **GKE / EKS / AKS** 通过 Operator 部署；
  - 与 **Spark / Kafka** 通过 connectors；
  - 与 **LangChain / LlamaIndex** 有 ArangoDB Retriever 集成；
  - 与 **DGL / PyG** 通过 Pregel 算法库集成。

## 四、业界主要使用方与案例

- **大型用户**（公开）：
  - **Cisco**：网络监控与配置；
  - **ING Bank（荷兰）**：支付风控；
  - **Telefónica**：电信计费；
  - **StickerMule**：电商个性化；
  - **DISH Network**：电视广告投放；
  - **Maersk**：航运 + 物流；
  - **Mercedes-Benz**：车辆数据；
  - **Shopify / Wayfair**：电商场景；
  - 国内：阿里、字节、腾讯、华为内部试点；
- **行业**：电信、银行保险、电商、社交网络、IoT、知识图谱、广告投放、物流；
- **典型场景**：欺诈检测 + 客户画像（文档 + 图）、推荐系统、商品-CDN 协同、命名实体识别 + 关系抽取 + GraphRAG。

## 五、一句话对比定位

> ArangoDB 是 **"多模型原生（Document + Graph + KV + Search）+ AQL 类 SQL 统一查询 + VelocyPack 二进制 JSON + RocksDB 存储 + Pregel 图计算 + 一站式运维（Operator / Active Failover）"**；与 **Neo4j（纯图）**、**Memgraph（纯图 + 兼容 Cypher）**、**Nebula（纯图 + 国产）**、**SurrealDB（多模型新兴）**、**OrientDB（多模型早期）** 构成图数据库阵营。ArangoDB 的差异化是 **"多模型 + BSL 开源策略 + Cluster Operator 完整"**。

## 六、整体架构

```
┌──────────────────────────────────────────────────────────────────┐
│                         客户端层                                  │
│  - arangosh（JavaScript REPL）                                   │
│  - Drivers: Go / Python / Java / Node.js / C# / PHP / Elixir    │
│  - HTTP REST API（自描述过 VelocyPack）                          │
│  - GraphQL Gateway（社区 / Enterprise）                          │
└─────────────────────┬────────────────────────────────────────────┘
                      │ HTTP/JSON or VelocyPack (binary)
┌─────────────────────▼────────────────────────────────────────────┐
│                       通用服务器（GeneralServer）                  │
│  arangod/GeneralServer/                                           │
│  ├─ HttpHandlerFactory    HTTP 路由                              │
│  ├─ V8Dispatcher          V8 JS 脚本（Foxx Microservices）       │
│  ├─ Auth / LDAP / OAuth                                          │
│  ├─ Endpoint（TCP / HTTP / VST）                                 │
│  └─ Scheduler（Fibers / std::thread pool）                      │
└─────────────────────┬────────────────────────────────────────────┘
                      │
┌─────────────────────▼────────────────────────────────────────────┐
│                        AQL 解析与执行                              │
│  arangod/Aql/                                                     │
│  ├─ Parser / AST / TypeChecker                                  │
│  ├─ Query / Plan / Optimizer                                    │
│  ├─ ExecutionEngine            Volcano 风格执行引擎               │
│  ├─ Executor                  各 ExecutionBlock                  │
│  └─ Indexes / Collections / Graph traversal nodes               │
└─────────────────────┬────────────────────────────────────────────┘
                      │
┌─────────────────────▼────────────────────────────────────────────┐
│                    图引擎（Gharial）                              │
│  arangod/Graph/                                                   │
│  ├─ Graph       通用图定义 + 遍历                                 │
│  ├─ ShortestPath / kShortestPaths                              │
│  ├─ TraversalBlock     遍历执行节点                              │
│  ├─ SmartGraphManager  SmartGraph 2.0 / Satellite               │
│  ├─ Pregel            Pregel 风格 BSP 图算法                    │
│  └─ GraphAlgorithmsModule（社区版简单算法，EE 提供更多）         │
└─────────────────────┬────────────────────────────────────────────┘
                      │
┌─────────────────────▼────────────────────────────────────────────┐
│                    引擎接口 / StorageEngine                       │
│  arangod/StorageEngine/                                           │
│  └─ StorageEngine API（同一接口，两个实现）                       │
│     ├─ RocksDBEngine（默认）                                     │
│  arangod/RocksDBEngine/                                          │
│  │   ├─ RocksDBColumnFamilyManager                             │
│  │   ├─ RocksDBTransactionState                                 │
│  │   └─ RocksDBIndex / RocksDBSettings                          │
│  │   其余：MMFiles（旧引擎，2026 年废弃）                       │
│  │     arangod/MMFiles/                                         │
│  │                                                              │
│  └─ ClusterEngine                                                 │
│     arangod/ClusterEngine/                                       │
│     ├─ ClusterInfo                                             │
│     ├─ Plan / Optimize / DistributeShards                       │
│     └─ ClusterTransaction / ClusterRepState                     │
└─────────────────────┬────────────────────────────────────────────┘
                      │
┌─────────────────────▼────────────────────────────────────────────┐
│                    索引层                                          │
│  arangod/Indexes/                                                 │
│  ├─ Index 抽象 + 各实现：Persistent / Unique / TTL                │
│  ├─ GeoIndex（arango/GeoIndex/）                                │
│  ├─ IResearch 倒排索引（全文 + 向量）                            │
│  └─ RocksDB Column Family 索引                                   │
└─────────────────────┬────────────────────────────────────────────┘
                      │
┌─────────────────────▼────────────────────────────────────────────┐
│                    复制与共识                                       │
│  arangod/Replication/                # RAFT 实现                  │
│  arangod/Replication2/               # Multi-Raft（3.10+）       │
└──────────────────────────────────────────────────────────────────┘
```

### 关键设计取舍

- **统一的 AQL**：图、文档、K-V 都走同一条 SQL-like 查询 → 跨模型 JOIN 自然；
- **VelocyPack**：二进制 JSON，自描述、零拷贝、可嵌套 → 网络高效；
- **存储引擎接口化**：RocksDB Engine 与 Cluster Engine 同实现 `StorageEngine` API，便于切换；
- **Fibers 调度 + std::thread**：`arangod/Scheduler` 使用协程（Fiber）作为执行上下文；
- **V8 Embedded JS**（Foxx）：让用户编写 MicroService 直接注入 arangod（V8 GC 集成）；
- **Replication2（C 23 引入）**：Multi-Raft 同一集群多 Raft Group，多 shard 副本独立一致。

## 七、存储引擎

### 7.1 RocksDB Engine（默认 / 生产）

- 后端 RocksDB（Facebook C++ KV / LSM-Tree），多个 ColumnFamily：
  - `default`（document 实际数据 + 索引）；
  - `graphs`（图的 collection / 边表）；
  - `analysis`（IResearch 全文）；
  - `vpack::Ptrs`（指针间接）；
  - `Write-Ahead Log RocksDB own`（与 Arango transaction log）；
- **checkpoint** 与 RocksDB own checkpoint 一致；写入走 RocksDB WAL + ArangoDB 自有的 transaction log；
- **compaction** 由 RocksDB 通用策略 + Arango 自定义；
- **Bloom filter / block cache** 在 RocksDB 与 Arango 层级共享。

### 7.2 MMFiles（旧引擎，2026 deprecated）

- **mmap** 主导，每 collection 一个 32 MB 文件；
- 记录 append-only / deletion marker；
- 写入可走 sync / async（async 默认）；
- Compaction 周期触发；
- 多文档在同一文件内紧凑；
- 2026 全部切 RocksDB Engine，仅作历史兼容。

### 7.3 Cluster 引擎

- **Sharding**：collection 默认分 36 块（hash-based 或 enterprise flexible sharding）；
- **Sharding 策略**：community 一致 hash、Enterprise `SmartGraph`（一致 hash 但路由表预热），适用于图遍历的 locality-aware。

### 7.4 Replicas / Replication

- 异步副本（active-passive）/ **Active Failover** 自动 promote；
- Enterprise 提供同步 + 异步混合（Synchroneous + Asynchronous replication）；
- **Replication2 / Multi-Raft**：3.10+ 每 Shard 独立 Raft，副本切换对前端透明。

## 八、索引机制

### 8.1 图存储模型

ArangoDB 图用 **Edge Collection + Vertex Collection** 实现：

- **Vertex Collection** = 普通文档 collection（`_id` 唯一）；
- **Edge Collection** = 特殊文档 collection（每个文档必须有 `_from`、`_to` → 引用两边 vertex id）；
- **遍历** = JOIN 两表（AQL 里走 `FOR v IN 1..ANY "col/key" edges …`）。

这与 Neo4j"节点-关系指针直连"不同：ArangoDB 走 **双表 JOIN**（vertex × edge），靠索引（edge hash）加速。

| 维度 | ArangoDB（双表） | Neo4j（指针直连） | 邻接表（Adjacency） |
|------|----------------|------------------|--------------------|
| 节点 | Document | 定长 slot | Vertex + Adj list |
| 关系 | Edge Collection | 双向链表指针 | 邻接数组 |
| 遍历 | JOIN | 指针 hop | 数组 next |
| 索引 | hash on `_from`/`_to` | 类型分桶 + B+Tree | 邻接数组 built-in |

### 8.2 默认与显式索引

- 每个 collection 自动建 `_id` 索引（Hash）；
- 显式索引（Persistent / Hash / Skiplist / TTL / Geo / Fulltext / IResearch / Vector）：
  - **Persistent**：基于 RocksDB；
  - **Hash**：HASH `field` 等值查询；
  - **Skiplist**：范围查询、排序；
  - **TTL**：过期文档自动清理；
  - **Geo**：GeoJSON / 经纬度范围 / 半径；
  - **Fulltext**：旧倒排（4.0 前是 default，3.10 起 IResearch 取代）；
  - **IResearch**（社区版内置 + EE）：倒排 + 全文 + 向量；
  - **Vector**：HNSW（Hnswlib 集成）3.10+。

### 8.3 全文与向量索引

- **IResearch 倒排**：
  - `arangod/IResearch/`；
  - 倒排 + BM25 + phrase + fuzzy；
  - 与图、文档组合查询：`SEARCH a.text == "..."` AQL 扩展；
- **向量索引**：
  - HNSW（hnswlib 在 arangod/IResearch/VectorIndex/）；
  - 3.10+ 提供 cosine / L2 / 内积；
  - 关联数据可走标识符过滤。

### 8.4 图遍历算法

- **Shortest Path**：`SHORTEST_PATH` 内置（支持加权 / 无加权 / 多终点 / 多起点）；
- **kShortest Paths**：Yen 算法（社区版）；
- **Patents**：EE 提供专利路径相关算法；
- **Pregel**（EE）：
  - Vertex-centric BSP 模型；
  - 自定义 algorithm：`ALGORITHM HITS / WCC / PageRank / SCC / LabelPropagation / SSSP`；
  - 调度器走 Pregel master + worker；
  - 编程模型：`arangod/Graph/Pregel` 与 `arangod/Pregel`。

### 8.5 图算法细节（BSP / Pregel 风格）

```
Step 0: Superstep 0 — vote of every vertex
       combiner: send message to neighbors
Step 1: Superstep 1 — compute, vote again
…
Step k: halt condition satisfied → finalize

ArangoDB Pregel 工作流：
  arangod/Pregel/Worker.cpp  注册算法；
  arangod/Pregel/Master.cpp  协调 superstep 与 barrier；
  arangod/Pregel/Algorithm/HITS/Pagerank.cpp 等具体实现
```

## 九、查询/计算/接口层

### 9.1 AQL（ArangoDB Query Language）

AQL 是类 SQL 的声明式语言，支持图与文档融合：

```aql
// 文档查询
FOR u IN users
  FILTER u.age >= 21 AND u.country == "CN"
  SORT u.age DESC
  LIMIT 100
  RETURN { id: u._id, name: u.name, age: u.age }

// 图遍历
FOR v IN 2..3 ANY "users/alice" knows
  OPTIONS { bfs: true, uniqueVertices: "global" }
  FILTER v.age > 25
  RETURN v.name

// 最短路径
LET path = SHORTEST_PATH(
  "users/alice" TO "users/bob" GRAPH "social"
  OPTIONS { weightAttribute: "affinity" }
)
RETURN path

// 子图匹配 / 模式查询
FOR v IN 1..ANY 'artists/the_jayhawks' wrote edge, written_by
RETURN v

// 全文检索
FOR doc IN view_name
  SEARCH ANALYZER(doc.text IN TOKENS("red fox", "text_en") ANY)
  RETURN doc

// 向量检索（IResearch View）
FOR d IN view
  LET q = APPROX_NEAREST_COSINE(d.embedding, @query_vec, 10)
  RETURN { doc: d, score: q.distance }
```

### 9.2 事务

- **BEGIN / COMMIT / ABORT**：单分片内原子；
- **Cluster 事务**：across-shard 需 Enterprise 同步事务，Community 仅 eventual；
- **Stream Transactions**：长事务逐步提交 (`db.transaction(...)` JavaScript)。

### 9.3 优化器

- AQL Optimizer 含规则：rule-based + cost-based（3.7+）；
- 优化规则示例：`use-index`、`avoid-sort`、`reduce-extraction`、`move-calculations-up`、`inline-subqueries`、`optimize-traversals`。

### 9.4 嵌入式 C++ 模式

- ArangoDB 3.11+ 提供 Embedded API（社区版可作为进程内嵌）；
- `ArangodServer::run()`：
  - 同进程启动 arangod 实例；
  - 适用：CI 测试、单进程 SaaS；
  - 区别于服务模式（端口暴露 HTTP）。

### 9.5 Foxx（MicroService）

- V8 + ArangoDB 集成的 MicroService 框架；
- 可在 arangod 内运行 Node-like JS；
- 路由：`Foxx.createRouter()`；
- 产物：Foxx App = JS + Manifest，部署到 `_apps/`;
- 现代推荐：Foxx 之外走独立服务 + HTTP。

### 9.6 GraphQL Gateway（社区版 + EE）

- 3.6+ 内置 GraphQL Service（独立 `arangod-graphql`）；
- 通过 schema 暴露 collection / graph 作为 types。

### 9.7 图计算引擎

- Pregel（BSP 模型，EE）：如前述；
- 社区版简单图算法（如 ShortestPath、WeightedTraversal）；
- 集成:**Cluster Pregel** 跨 shard 协调（master 调度 + worker 计算）。

## 十、事务与一致性

### 10.1 ACID

- ACID 全实现：Atomicity（ROCKSDB write batch）、Consistency（schema 强制）、Isolation（read uncommitted → repeatable read → snapshot）、Durability（WAL + RocksDB fsync）；
- 单实例 RocksDB write batch 提供乐观事务；
- 跨 collection / shard 事务（仅当所有涉及的 collection 落在同一 shard，或 Enterprise Synchronous replication 提供跨 shard）。

### 10.2 隔离级别

- 默认 Read-Committed；
- Stream Transactions 提供 TransactionLevel 可调：`exclusive`、`write`、`read`；
- Document-level MVCC：每个 collection 维护 version，事务可比对版本。

### 10.3 Cluster 事务

- Community：eventual consistency across shards（仅 local shard committed）；
- EE：Synchronous replication + Raft 跨 shard，提供 ACID；
- Default Transactions：community 中 EOL 提供 atomic within single shard。

### 10.4 乐观并发

- `@Rev` / `_rev` 字段在 document 上，事务 update 携带 prev revision；
- 与 Neo4j 比，文档式 OCC 比节点式 OCC 更易上手。

## 十一、复制与高可用

### 11.1 单实例模式

- 单 arangod 进程；
- 集群未启用时所有 collection 都在单分片内。

### 11.2 Active Failover（3.6+）

- 主从模式，自动化 failover；
- 1 主 + N 备，主故障自动 promote 备；
- 适合不需要水平扩展的中型部署。

### 11.3 Cluster Mode

```
┌─────────────────────────────────────────────────┐
│                     Coordinator                 │
│   - AQL 解析 + 优化 + 跨 shard 调度             │
│   - 客户端连入（轮询）                          │
└──────────┬──────────────────────────────────────┘
           │ 路由信息从 Agency 同步
┌──────────▼────────┐    ┌─────────────────┐
│   DB-Server 1     │    │   DB-Server 2    │ …
│   - Shard 主/副本 │
│   - 接受协调器请求 │
│   - 执行事务      │
└───────────────────┘    └──────────────────┘
           ▲
           │ Resign / replicate
┌────────────────────────────┐
│        Agency              │
│   - 集群元数据 (Raft)      │
│   - Plan / Current        │
│   - Leader / Followers    │
└────────────────────────────┘
```

- **Agency** (Raft) 存 Plan（disired state）+ Current（actual）；
- **Coordinator** 与 **DB-Servers** 通过 Agency 同步；
- **DB-Server** 自身又是 RocksDB Engine + Storage Engine API。

### 11.4 副本与一致性

- Community 异步副本 → failover；
- EE Synchronous Replication → durability + Raft；
- 3.10 起 **Replication2** 多 Raft 组 → shard 级别独立 Raft，加速收敛。

## 十二、扩展性

- **水平扩展**：shard 数量可调；hot shard 可通过 EE **Hot Shard Move**；
- **垂直扩展**：受内存（写 buffer）+ RocksDB bloom filter size 制约；
- **冷热分层**：社区版不支持原生；EE 提供 **satellite collections**（高频小 collection 全集群缓存）；
- **SmartGraph**：EE 智能图分片策略，按已知 key 局部化（少跨 shard）；
- **SatelliteGraph**：EE 全集群节点同步 collection 的低延迟图查询。

## 十三、性能与基准

- **Bench**：开源 `arangodb-bench` 提供 micro-benchmark（traversal、aggregation）；
- **第三方**：BenchANT、AWS 内部 benchmark；
- **典型数字**：
  - 点写：10w–50w QPS（社区版 RocksDB）；
  - 邻接遍历：百万节点 1-step 10–100 ms；
  - Shortest Path 4-hop 全图 100 ms–500 ms（受硬件影响）；
  - 集群模式 5-shard：5w QPS 点读；
- **和 Neo4j 对比**：单实例图遍历 Neo4j 占优（图原生），社区级 cluster 模式 ArangoDB 更通用。

## 十四、运维与可观测性

- **监控**：内置 `/_admin/metrics`、`/_admin/statistics` (Prometheus 格式)；
- **Slow Query**：AQL explain + profile + `query slow-log threshold`；
- **备份**：`arangodump` + `arangorestore`；EE Hot Backup；
- **升级**：`arangod_upgrade.js` 跨版本；
- **Tracing**：OpenTelemetry 实验性（3.10+）；
- **Operator**：`arangodb/k8s-operator`（Enterprise）：Helm + CRD + 自动 failover + rolling upgrade；
- **监控集成**：Grafana Dashboard 模板（社区提供）、Datadog / New Relic 集成。

## 十五、生态与集成

- **客户端**：
  - Go：`github.com/arangodb/go-driver`；
  - Python：`python-arango`（社区）+ LangChain 集成；
  - Java：`com.arangodb:arangodb-java-driver`；
  - Node：`arangojs`；
  - C#：`ArangoDB.Client`；
  - JS / JS-Web-Starter：`@arangodb/community-graphql`；
  - PHP / Elixir / R 均有官方或半官方驱动；
- **图可视化**：ArangoDB Web UI（内置）+ 第三方（Graphistry、yWorks）；
- **ORM / OGM**：Spring Data ArangoDB（有 Spring 集成）；
- **BI**：PowerBI / Tableau 自定义连接器；
- **消息队列**：Kafka Sink（CDC）；
- **容器化**：官方 `arangodb/arangodb` Docker 镜像、Kubernetes Helm chart、Operator（Enterprise）；
- **AI / GenAI**：ArangoDBAI（BigQuery 风格 AI Function）、LangChain Retriever、LlamaIndex Reader、与 Hugging Face 集成。

## 十六、核心论文与算法原理

- **Pregel 论文**：Malewicz et al. (Google)，"Pregel: A System for Large-Scale Graph Processing"（2010，SIGMOD）；
- **RocksDB**（FIFO + Bloom + LSM）：Dong et al., "Evolution of Development Priorities in Key-value Stores"（2017 VLDB）；
- **Raft 论文**：Ongaro & Ousterhout, "In Search of an Understandable Consensus Algorithm"（2014 USENIX ATC）；
- **VelocyPack**：内部格式（`arangod/VelocyPack/`，二进制 JSON 自描述）；
- **多模型原文**："One DBMS for all your data: an examination of multi-model databases"（arXiv 2019）；
- **Graph Analytics**：见 Pregel 后续工作，如 Giraph、GraphX、Spark GraphFrames。

## 十七、源码学习路径

| 阶段 | 模块 | 路径 | 要点 |
|------|------|------|------|
| 入门 | AQL 入口 | `arangod/Aql/` `Parser.cpp` / `Query.cpp` | 解析、优化、执行 |
| 入门 | GeneralServer | `arangod/GeneralServer/` `HttpHandler.cpp` | HTTP 入口、VelocyPack 解析 |
| 进阶 | 存储引擎 | `arangod/RocksDBEngine/` `RocksDBColumnFamilyManager.cpp` | RocksDB 集成 |
| 进阶 | Indexes | `arangod/Indexes/` 与 `arangod/IResearch/` | 索引、IResearch 全文 |
| 进阶 | Cluster | `arangod/Cluster/` `ClusterInfo.cpp`、`ClusterEngine.cpp` | Plan + Current |
| 进阶 | 复制 | `arangod/Replication/`、`arangod/Replication2/` | Raft 实现 |
| 高级 | Pregel | `arangod/Pregel/` `Master.cpp` / `Worker.cpp` | BSP 调度 |
| 高级 | 图遍历 | `arangod/Graph/` `TraversalBlock.cpp`、`ShortestPathBlock.cpp` | 遍历执行节点 |
| 高级 | Foxx | `arangod/V8Server/` `FoxxController.cpp` | V8 集成 |

## 十八、源码规模与开发语言

- **LOC**：约 80 万行 C++（arangod）、约 5 万行 JS（Foxx + utils）、约 10 万行 VelocyPack 头文件；
- **语言占比**：
  - C++ ≈ 90%；
  - JavaScript ≈ 5%（Foxx、UI、bench）；
  - Shell / CMake / Python ≈ 5%；
- **构建系统**：CMake（顶层 `CMakeLists.txt`）+ `CMakePresets.json`；
- **C++ 标准**：C++17 / C++20 渐进引入；
- **CI**：BuildKite（ArangoDB 内部）+ GitHub Actions（社区）；
- **代码风格**：`STYLEGUIDE.md`（clang-format 部分自定义规则）。

## 十九、学习资源

- **官方文档**：
  - `https://docs.arangodb.com/stable/` 主文档；
  - `https://docs.arangodb.com/stable/aql/` AQL；
  - `https://docs.arangodb.com/stable/graphs/` 图；
  - `https://docs.arangodb.com/stable/cluster/` 集群；
  - `https://docs.arangodb.com/stable/release-notes/` 发布说明；
  - `https://docs.arangodb.com/stable/foxx/` Foxx；
- **书籍**：
  - "ArangoDB Cookbook"（Packt）；
  - "Graph Data Modeling in NoSQL with Neo4j and ArangoDB"；
  - "Learning ArangoDB"（Packt, Rashid Bin Muhammad）；
- **视频 / 课程**：ArangoDB 官方 YouTube、Arango Summit Talk 集、Demo 应用（Dealership、Game of Thrones Social Network）；
- **社区**：Slack、Stack Overflow `[arangodb]`、Reddit r/arangodb、官方 Forum；
- **Demo Data**：ArangoDB Oasis 免费云 Demo、`github.com/arangodb/arangodb` 的 `tests/` 集成测试数据。

## 二十、可移植性与部署

- **支持平台**：Linux（x86_64 / ARM64）、macOS（x86_64 / M1/M2）、Windows（x86_64，WSL 推荐）、Docker（官方镜像）；
- **容器化**：`arangodb/arangodb:3.11` 镜像 + Helm chart + Enterprise K8s Operator；
- **Helm**：社区 chart（`arangodb/k8s-helm-chart`）；
- **Operator**：`arangodb/k8s-operator`（Enterprise）：自动扩容 / 升级 / backup；
- **嵌入式部署**：3.11+ C++ 进程内嵌（ArangodServer），用于 SaaS 单进程 + Arango Graph 复用。

## 二十一、变体与衍生项目

- **ArangoDB Enterprise**：商业闭源，含 SmartGraph 2.0、SatelliteGraph、Hot Backup、Synchronous replication、C2K8s Operator GA；
- **ArangoGraph Insights Platform**：商业全托管云（Free Tier 1 GB / Professional / Enterprise）；
- **ArangoDB-OEM**：嵌入式 OEM 商业许可证；
- **ODF / Adaptor**：ArangoDB → Spark、DGL、PyG Connector、SQL Adaptor（Parquet / S3）；
- **历史**：AvocadoDB（更名）、Mesos Marathon 支持；

## 二十二、风险与陷阱

### 22.1 BSL（Business Source License）风险

- BSL 1.1：3 年后该版本转 Apache，但当前版本不可直接用于商业产品；
- 重新分发（打包为商业 SaaS）受限；
- OEM 需联系 ArangoDB 直接协商；
- 规避：3 年后版本（已 Apache 2.0）适用于商业。

### 22.2 多模型代价

- 多模型 = 通用性 vs 单模型极致：AQL 跨模型 JOIN 性能不如单模型专用（如 Neo4j 指针遍历）；
- 大量图遍历需要 SmartGraph（EE）才能保 local locality，社区版有跨 shard overhead；
- 文档 schema 自由 → 迁移路径难追踪；
- 性能鱼与熊掌不可兼得。

### 22.3 Cluster 运维陷阱

- Shard 重映射必须停写；
- 异步副本早期版本导致数据丢失风险（无 Synchronous Replication）；
- Agency 节点选择至少 3 节点，防脑裂；
- Heavy join 跨 shard 容易超时 → 提前建模（denormalize）；
- Reshard 期间部分请求失败需客户端重试。

### 22.4 MMFiles 废弃陷阱

- 旧文档 / 客户端驱动默认期望 MMFiles；
- 3.12 起默认 RocksDB → 业务迁移需重新设计 DDL、CAP、备份；
- 索引 API 行为差异：MMFiles 用 `use-index-hint`，RocksDB 用 `useIndex`；
- 备份恢复格式不同（`arangodump --rocksdb` vs `--mmfiles`）。

### 22.5 Pregel 算法局限

- 单一算法需 governor hash 算法；
- 算法运行必须先建图（vertex collections + edge collections）；
- 大量图（>10 亿 vertex）单次 Pregel 运行会成单点；
- v3.10 起可重启（checkpoint），但 EE 限定。

### 22.6 Foxx GC 与版本陷阱

- V8 GC 抖动在大 collection 下明显；
- V8 version 固定（升级有 breaking change）；
- Foxx Service 与 arangod 版本绑定强，升级需重新部署；
- 大流量 Foxx 走独立 arangod 进程隔离。

### 22.7 安全

- 默认无加密（生产建议 TLS + Mutual Auth）；
- 内置 RBAC + LDAP 需 EE；
- Foxx Service 可读写 ROOT 权限 → 越权风险；
- 客户端驱动需注意 `ARANGO_NO_AUTH` 调试开关。

## 二十三、与本项目的关联

- **多模型融合**：本项目"多模态数据库"目标里 document + graph + KV 融合是 ArangoDB 的核心思想，可借鉴其统一查询语法（AQL）作为多模型语言的参考；
- **RocksDB Engine**：本项目 `engineering/src/db/` KV 模型可借鉴 RocksDB 存储选型；
- **Pregel BSP 图算法**：本项目未来图算法实现可参考 Pregel 思想；
- **AQL 优化器**：可借鉴其"`use-index`、`reduce-extraction`"等优化规则；
- **嵌入式模式**：借鉴 ArangoDB 3.11+ Embedded API，本项目可考虑让自有引擎也支持 in-process 模式（参考 Kuzu）；
- **Operator 路径**：本项目 K8s 化时 ArangoDB Operator 提供成熟模板。

## 二十四、RocksDB 存储层：列族划分与键值编码

> 以下三节（§二十四 ~ §二十六）的源码行号基于 `reference/graph/arangodb/` 的 **3.12.11-devel** 完整 clone（`ARANGO-VERSION` 文件）。该版本中存储引擎抽象层位于 `arangod/StorageEngine/`，具体实现只有 `arangod/RocksDBEngine/` 与 `arangod/ClusterEngine/` 两个目录；MMFiles 的目录与符号已被完全删除，因此本节起全部以 RocksDB 引擎为对象。

### 24.1 列族（Column Family）划分

一个 collection 的数据不落在同一个 RocksDB key 空间里，而是按「记录种类」拆进 **11 个列族**（`arangod/RocksDBEngine/RocksDBColumnFamilyManager.h:64`）：

| # | 内部名 | 对外名 | 内容 |
|---|--------|--------|------|
| 0 | default | definitions | 数据库 / collection 元数据 |
| 1 | Documents | documents | 文档正文（VelocyPack） |
| 2 | PrimaryIndex | primary | `_id` / `_key` 主索引 |
| 3 | EdgeIndex | edge | `_from` / `_to` 邻接索引 |
| 4 | VPackIndex | vpack | 持久化 secondary index（skiplist / hash） |
| 5 | GeoIndex | geo | 地理索引 |
| 6 | FulltextIndex | fulltext | 旧全文索引 |
| 7 | ReplicatedLogs | replicated-logs | 复制日志 |
| 8 | ZkdIndex | mdi | 多维索引（Zkd 为 unprefixed MDI 的旧称） |
| 9 | MdiPrefixed | mdi-prefixed | 带前缀的多维索引 |
| 10 | VectorIndex | vector | 向量索引 |

枚举定义在 `arangod/RocksDBEngine/RocksDBColumnFamilyManager.h:42`，内部名与对外名两套字符串数组分别硬编码于 `arangod/RocksDBEngine/RocksDBColumnFamilyManager.cpp:36` 与 `arangod/RocksDBEngine/RocksDBColumnFamilyManager.cpp:46`。注意第 8 个列族的内部名保留为 `ZkdIndex`，源码在该行专门留了向后兼容注释 —— 改名会作废已有数据目录。

**为什么按记录种类分列族**：LSM-Tree 的 compaction、block cache、bloom filter、prefix extractor 全部是 per-column-family 的。图查询的访问模式与文档正文截然不同 —— 遍历只读 `edge` 与 `primary` 两个列族，正文 `documents` 列族完全不被触碰。分开之后，一次边插入触发的 compaction 只在 `edge` 列族内合并，不与文档正文争抢 I/O 与内存带宽。代价是每个列族各自持有一份缓存与 bloom 数组，内存上界为 O(列族数 × 缓存预算)。

### 24.2 键编码：类型字节与定长 / 变长之分

键的**种类**由单字节枚举 `RocksDBEntryType` 标识（`arangod/RocksDBEngine/RocksDBTypes.h:39`），取值刻意避开可打印 ASCII：

```
Database='0'  Collection='1'  Document='3'  PrimaryIndexValue='4'
EdgeIndexValue='5'  VPackIndexValue='6'  View='>'  GeoIndexValue='?'
```

（`arangod/RocksDBEngine/RocksDBTypes.h:44` 起为完整取值，注释里还留了两条 pre-3.8 GA 的作废 revision tree 编码，标注「不得重用」。）

类型字节**是否写进键**取决于该键是否定长：

- **变长键**写入类型字节。`arangod/RocksDBEngine/RocksDBKey.cpp:220` 处数据库键为 `[type:1B][databaseId:8B]`，collection 键为 `[type:1B][databaseId:8B][collectionId:8B]`；
- **定长键**不写类型字节 —— 反正按列族就能区分，写了纯属浪费。文档键由 `arangod/RocksDBEngine/RocksDBKey.cpp:238` 构造：

```
Document 键 = [collectionObjectId : 8B little-endian][LocalDocumentId : 8B little-endian]
              └──────── 前缀（整 collection 共享） ────────┘└──── 行内唯一 ────┘
```

主索引键则是「变长尾部」形态（`arangod/RocksDBEngine/RocksDBKey.cpp:250`）：

```
PrimaryIndexValue 键 = [indexId : 8B][primaryKey : 0..n B]      // primaryKey 即 _key
```

`indexId` 前缀不可省 —— 主索引列族内混着 persistent / skiplist / hash 三类索引，去掉前缀会让不同索引的键互相穿插，直接破坏全局有序性，也就毁掉了范围扫描的前提。

### 24.3 前缀提取器与「一个顶点一个前缀」

边索引键是最微妙的一种（`arangod/RocksDBEngine/RocksDBKey.cpp:262`）：

```
EdgeIndexValue 键 = [indexId : 8B][vertexId : 0..n B]['\0'][LocalDocumentId : 8B]['0xFF']
                    └────── RocksDB 前缀提取器只取这一段 ──────┘└──── 定位单条边 ────┘
```

分隔符 `\0` 定义在 `arangod/RocksDBEngine/RocksDBKey.cpp:56`。为什么必须在字符串后补一个 `\0`？因为顶点 id 长度可变，提取器无法从尾部回推切点，必须用一个**不会出现在顶点 id 里**的字节当终止符。

提取逻辑在 `arangod/RocksDBEngine/RocksDBPrefixExtractor.h:41`：检查末字节，若非 `\0` 则断言它是 `0xFF`，再回退 `8 + 1` 字节取前缀。`arangod/RocksDBEngine/RocksDBPrefixExtractor.h:58` 的 `InDomain` 用同一判据区分「完整键」（末字节 `0xFF`）与「已是前缀」（末字节 `\0`）—— 这让 `Transform` 幂等，因为 RocksDB 偶尔会对已变换过的 key 再变换一次，源码里对此有明确注释。

**收益**：顶点 v 的全部出边在 RocksDB 的 key 序上恰好构成一段连续区间，且这整段**共享同一个 bloom 位图**。遍历查 v 的出边时：

```
朴素方案：每条边独立 seek  →  O(d · log N) 次比较，d = 出度
前缀 bloom：1 次 bloom 探测 →  O(log N + d) 次比较
```

误判概率取决于 bloom 的每 key 位数 k 与总位数 b：p ≈ 1 − e^(−k·n·d / 2^b)。扇出 d 越大收益越明显 —— 这是图存储相对通用 KV 最实质的一处优化，也是「邻接表天然按顶点聚簇」在 LSM 存储上的落地方式。

### 24.4 值编码：LocalDocumentId + RevisionId

键里放的是 `LocalDocumentId`（8 字节单调递增整数），值里跟着放行版本号。主索引值由 `arangod/RocksDBEngine/RocksDBValue.cpp:273` 的分支构造：

```
PrimaryIndexValue 值 = [LocalDocumentId : 8B]                        // 无版本：仅做唯一性校验
                    或 [LocalDocumentId : 8B][RevisionId : 8B]       // 带版本，见 arangod/RocksDBEngine/RocksDBValue.cpp:280
```

这样「按 `_key` 查到 `LocalDocumentId`」与「校验并发写的 `_rev` 是否过期」是**同一次 seek 返回的两段字节**，不需要第二次读。反方向按 `LocalDocumentId` 取正文走 `documents` 列族，那里的值就是裸 VelocyPack，不再带任何头部 —— 读取路径的零额外分配由此而来。

## 二十五、Edge Index 的邻接存取与遍历游标推进

### 25.1 边索引是覆盖索引

社区版每个 edge collection 有两个系统索引 `_from` 与 `_to`，由 `RocksDBEdgeIndex` 承载（构造见 `arangod/RocksDBEngine/RocksDBEdgeIndex.cpp:746`，`_directionAttr` 即被索引的那一侧，`_isFromIndex` 区分方向）。插入路径见 `arangod/RocksDBEngine/RocksDBEdgeIndex.cpp:820`：

```
insert(edgeDoc):
    v   = edgeDoc[_directionAttr]        // _from 索引取 _from，_to 索引取 _to
    key = [indexId][v]['\0'][docId]['0xFF']
    val = edgeDoc[对端属性]               // _from 索引存 _to，_to 索引存 _from，见 arangod/RocksDBEngine/RocksDBEdgeIndex.cpp:828
    Put(edge 列族)                        // arangod/RocksDBEngine/RocksDBEdgeIndex.cpp:835
```

关键在 `arangod/RocksDBEngine/RocksDBEdgeIndex.cpp:828`：**值里存的是对端属性的原样字符串**。这意味着边索引天然是覆盖索引 —— 从 v 的出边顺序扫出来时，每条记录已经带着终点，**不需要回 `documents` 列族取边文档**。这是 ArangoDB 遍历比「vertex × edge 双表 JOIN」更快的原因：实际发生的不是 JOIN，而是「索引顺序扫描 + 一次批量取顶点」。

值不做 `_id` ↔ `_key` 归一化：写入侧直接取 `stringView()`（`arangod/RocksDBEngine/RocksDBEdgeIndex.cpp:832`），查询侧 `handleValNode` 也是原样塞进查找键数组（`arangod/RocksDBEngine/RocksDBEdgeIndex.cpp:1267`）。所以边文档若把 `_from` / `_to` 写成短键（SmartGraph 风格的 `vertex/edge` 形式），索引体积可线性压下来，代价是用户自行承担 `_id` → `_key` 的翻译。

### 25.2 游标推进：rearm 而非重建

遍历每推进一个顶点都要问一次「v 的所有出边」。逐次重建索引迭代器，会让每一步都付出一次完整的 `indexScanForCondition`（AST 匹配 + 条件下推 + 迭代器分配）。ArangoDB 的做法是**原地改写 AST 条件常量并复用迭代器**。

`SingleServerEdgeCursor::LookupInfo::rearmVertex`（`arangod/Graph/Cursors/SingleServerEdgeCursor.cpp:96`）做两件事：

1. **改写条件常量**：找到形如 `e._from == "coll/a"` 的比较节点，把右侧字符串常量就地替换为新顶点 id（`arangod/Graph/Cursors/SingleServerEdgeCursor.cpp:116`）。若所用索引未覆盖 `_from` / `_to`，则改写到表达式的最后一个成员上（`arangod/Graph/Cursors/SingleServerEdgeCursor.cpp:126`）。注意其间的 `TEMPORARILY_UNLOCK_NODE` —— AQL AST 平时是不可变共享树，这里是受控的可变豁免。
2. **复用或重建游标**（`arangod/Graph/Cursors/SingleServerEdgeCursor.cpp:146`）：

```
if (cursor.canRearm()):
    cursor.rearm(astNode)              # 只换 seek 目标，迭代器对象保留
else:
    trx->indexScanForCondition(...)    # 全量重建，见 arangod/Graph/Cursors/SingleServerEdgeCursor.cpp:160
```

边索引迭代器明确声明支持 rearm（`arangod/RocksDBEngine/RocksDBEdgeIndex.cpp:303`），rearm 只做两件事：按新条件填查找键数组、按等值 / `IN` 分别走两条路径（`arangod/RocksDBEngine/RocksDBEdgeIndex.cpp:325` 与 `arangod/RocksDBEngine/RocksDBEdgeIndex.cpp:336`），其余状态原样保留。

复杂度（d 为出度，N 为该索引的条目数）：

```
重建方案：每步 O(条件匹配) + 一次 O(log N) seek + 一次迭代器分配，固定开销 ∝ 步数
rearm 方案：每步 一次 O(log N) seek + 一次常量替换，省掉与 d 无关的固定开销
```

两者分别计入 `incrCursorsRearmed()`（`arangod/Graph/Cursors/SingleServerEdgeCursor.cpp:148`）与 `incrCursorsCreated()`（`arangod/Graph/Cursors/SingleServerEdgeCursor.cpp:157`），可直接从 `/_admin/statistics` 观察命中率。

### 25.3 方向的表达：正反两个独立游标

方向枚举是每个游标各持一份的 `enum Direction { FORWARD, BACKWARD }`（`arangod/Graph/Cursors/SingleServerEdgeCursor.h:91`），而非全局枚举。两条方向边各自持有独立游标与独立索引：

- 正向（`_from` 索引）：从边文档取 `_from`、从索引值取 `_to` 作为下一跳顶点；
- 反向（`_to` 索引）：从边文档取 `_to`、从索引值取 `_from` 作为下一跳顶点。

`arangod/Graph/Cursors/SingleServerNeighbourCursor.cpp:78` 处的三元分支正是这个二选一。`ANY` 方向等价于同时持有两者并在结果层做去重。

最短路径用的**双侧枚举器** `TwoSidedEnumerator` 同时持有左右两侧（`arangod/Graph/Enumerators/TwoSidedEnumerator.cpp:288` 与 `arangod/Graph/Enumerators/TwoSidedEnumerator.cpp:290`）：

```
_left  { Direction::FORWARD,  forwardProvider,  ... }
_right { Direction::BACKWARD, backwardProvider, ... }
```

两侧交替推进，而不是双向 BFS 并行展开 —— 每步比较当前最优路径长度与两侧剩余深度，超出即剪枝（方向分支见 `arangod/Graph/Enumerators/TwoSidedEnumerator.cpp:145`）。理论上搜索空间从单侧 Dijkstra 的 O(b^d) 降到 O(b^(d/2))，b 为分支因子、d 为最短跳数。

### 25.4 从边文档里抠出对端 id：VelocyPack 就地扫描

边索引已经给了对端 id，为何还需要 `extractFromFromDocument`？因为**当所用索引不覆盖所需属性**（自定义过滤表达式、或 `_from` 索引不覆盖 `_to`）时仍需读边文档正文。此时 ArangoDB 不走 VelocyPack 的属性名哈希查找，而是**在字节流上线性扫属性名标记**。

系统属性在 VelocyPack 里用单字节标记（`lib/Basics/VelocyPackHelper.h:479`）：

```
_key=0x31   _rev=0x32   _id=0x33   _from=0x34   _to=0x35
```

且源码用 `static_assert` 强制它们**严格递增**（`lib/Basics/VelocyPackHelper.h:484` 起）—— 因为边文档正是按此序落盘。于是：

```
extractToFromDocument(slice):                          # arangod/Transaction/Helpers.cpp:281
    p     = slice.begin() + slice.findDataOffset(slice.head())
    count = 0
    while (*p <= ToAttribute && ++count <= 4):         # _from 版见 arangod/Transaction/Helpers.cpp:262
        if (*p == ToAttribute): return VPackSlice(p + 1)   # 命中即构造 slice，零拷贝
        ++p
        p += VPackSlice(p).byteSize()                  # 跳过 属性名 + 属性值
    return slice.get(StaticStrings::ToString)           # 扫描未命中 → 退回哈希查找
```

三处提前退出把最坏情况钉死：

1. `*p <= 标记` —— 属性名按字节序递增，一旦当前属性名大于目标，后面不可能再有目标（前提是落盘有序，由 `static_assert` 与写入端共同保证）；
2. `++count <= 4` —— 系统属性至多 5 个（`_key/_rev/_id/_from/_to`），扫过 4 个还没命中就必然没有；
3. 命中后直接以 `p + 1` 构造 `VPackSlice`，**不复制**。

正常路径是 O(≤4) 次指针跳转且零分配；只有属性乱序的自定义文档才付哈希查找的代价。函数头注释同时声明了前提：仅对含 `_key/_id/_from/_to/_rev` 且按此序的边文档有效。

### 25.5 遍历条件的可下推性判定

条件并非都能下推到索引。`arangod/Aql/TraversalConditionFinder.cpp:73` 定义了四分类：

```
OptimizationCase { PATH, EDGE, VERTEX, NON_OPTIMIZABLE }
```

判定逻辑见 `arangod/Aql/TraversalConditionFinder.cpp:675` 的 `identifyCase`：把条件引用的变量逐一比对 `vertexVar` / `edgeVar` / `pathVar`，**只允许命中一种**；一旦混用两种（例如同时出现 edge 与 vertex），或引用了遍历后才定义的变量，判为 `NON_OPTIMIZABLE`，该条件成员被从下推候选中剔除（`arangod/Aql/TraversalConditionFinder.cpp:716`）。判定通过的边条件最终喂给 §25.2 的 `rearmVertex` —— 也就是说，**优化器的判定结果直接决定遍历能否免掉每步的迭代器重建**。

## 二十六、AQL 集群执行：Scatter-Gather 与优化规则调度

Coordinator 上的执行计划不是「一个计划 + 运行时切分」，而是**编译期就把 Scatter / Gather 插进计划树**。§七与 §十一描述的是部署拓扑，本节补的是计划树里的两个算子。

### 26.1 Scatter：两种扇出粒度

`arangod/Aql/ExecutionNode/ScatterNode.h:48` 定义了两种扇出粒度：

```
enum ScatterType { SERVER = 0, SHARD = 1 };
```

- `SERVER`：一个 DBServer 一个执行单元，同一 server 上的多个 shard 由本地事务管理器串行调度；
- `SHARD`：一个 shard 一个执行单元，可真正并行。

选择权交给优化器规则 `scatter-in-cluster`。它先把可被 scatter 的节点类型圈成静态白名单（`arangod/Aql/Optimizer/Rule/ScatterInCluster.cpp:50`）：

```
ENUMERATE_COLLECTION, ENUMERATE_NEAR_VECTORS, INDEX, MATERIALIZE,
ENUMERATE_IRESEARCH_VIEW, INSERT, UPDATE, REPLACE, REMOVE, UPSERT
```

即**只有真正触碰 collection 数据的算子才需要扇出**；纯计算算子（FILTER / CALCULATION / SORT）留在 Coordinator 本地跑，避免无效 RPC。规则随后在每个命中节点前插入 `ScatterNode::ScatterType::SHARD`（`arangod/Aql/Optimizer/Rule/ScatterInCluster.cpp:270`）并重接依赖边。

代价模型（u 个扇出单元，第 i 个的数据量 s_i，网络往返 R）：

```
不扇出（u=1）    ：T ≈ O(s) + R
并行散列（u 个） ：T ≈ max_i O(s_i) + R        —— 与 u 几乎无关
协调器串行收割   ：T_merge ≈ O(Σ s_i)          —— 单线程收割时反而变差
```

所以只有 u 足够大、s_i 足够小时 SHARD 级散列才划算；单 shard collection 走 SERVER 级即可，这正是 §26.2 判据的来源。

### 26.2 Gather 的并行度判定

扇入端 `GatherNode` 的并行度是三态而非布尔（`arangod/Aql/ExecutionNode/GatherNode.h:52`）：

```
enum class Parallelism : uint8_t { Undefined = 0, Serial = 2, Parallel = 4 };
```

`Undefined` 是「留待后续规则决定」的中间态。初值直接由集合形态给出（`arangod/Aql/ExecutionNode/GatherNode.cpp:318`）：

```
evaluateParallelism(collection):
    return (collection.numberOfShards() == 1 || collection.isSatellite())
             ? Parallelism::Serial      # 单 shard 或卫星集合：并行无收益
             : Parallelism::Undefined;
```

真正的判定在规则 `parallelize-gather`（`arangod/Aql/Optimizer/Rule/ParallelizeGather.cpp:88`）。它先用一个图遍历 `ParallelizableFinder`（`arangod/Aql/Optimizer/Rule/ParallelizeGather.cpp:38`）收集两类否决理由：

- 若**已经**存在并行度 > 1 的图遍历节点（`arangod/Aql/Optimizer/Rule/ParallelizeGather.cpp:60`），再叠一层并行会超订线程池 → 否决；
- 若图遍历节点不是本地节点（`arangod/Aql/Optimizer/Rule/ParallelizeGather.cpp:61` 的 `isLocalGraphNode()`）→ 否决。

只有全部图节点都判定为本地（`arangod/Aql/Optimizer/Rule/ParallelizeGather.cpp:106` 的 `allSatellite`）才把 Gather 置为 `Parallel`（`arangod/Aql/Optimizer/Rule/ParallelizeGather.cpp:113`），否则显式置 `Serial`。这个「要么全并行、要么全串行」的粗粒度决策避免了部分并行带来的乱序合并开销 —— 因为并行扇入必须整体按序归并，混着来反而要付额外排序。

### 26.3 消除无谓的 Remote → Scatter 对

并非所有「Coordinator 上的远程算子」都需要先 scatter 再远端执行。规则 `remove-unnecessary-remote-scatter` 专门清理这一组合（`arangod/Aql/Optimizer/Rule/RemoveUnnecessaryRemoteScatter.cpp:41`）：

```
for each RemoteNode r（不进子查询）:
    if r 无依赖:              continue
    if r 的首个依赖不是 Scatter: continue
    node = r 的 scatter 依赖
    while node != nullptr:
        if node 的依赖数 != 1:  break          # 有分叉，不能删
        node = node 的唯一依赖
        if node 不是 SINGLETON / CALCULATION / FILTER
           且未被 shouldExcludeFromScatterGather 标记:   # 见 arangod/Aql/Optimizer/Rule/RemoveUnnecessaryRemoteScatter.cpp:72
            break
    # 走到底 → r 与它的 scatter 之间只有纯本地算子，删掉这一对
```

即「Scatter 与 Remote 之间若只隔着单例算子与本地计算 / 过滤，扇出再扇入纯属往返浪费，直接拉直」。这类改写在 AQL 文本里不可见，只能靠 `explain` 的节点树观察。

### 26.4 优化规则的注册与调度

规则不是硬编码的调用序列，而是**按 level 排序的数据表**。level 枚举定义在 `arangod/Aql/OptimizerRule.h:92`，注释写明「level 值小者先执行、且必须唯一」，并按 pass 分组（Pass 1 上移节点、Pass 2 消冗余、Pass 3 交换相邻 Enumerate、Pass 4 二次上移……）。其中约第 500 号 level 被特别标注为屏障：更高 level 新产生的计划不得回退到此 level 及以下。

每条规则带一组位标志（`arangod/Aql/OptimizerRule.h:45`）：

```
Default=0  Hidden=1  ClusterOnly=2  CanBeDisabled=4
CanCreateAdditionalPlans=8  DisabledByDefault=16  EnterpriseOnly=32
```

`CanCreateAdditionalPlans` 决定优化器是「就地改写」还是「派生候选计划 + 代价比较」—— 后者对应候选计划池 `arangod/Aql/Optimizer.h:181` 的 `PlanList _plans`，最终由 `stealBest()` 交出胜者。`EnterpriseOnly` 决定社区版构建下该规则是否注册；源码里成片的 `#ifdef USE_ENTERPRISE` 就是在守这条。

注册入口是 `OptimizerRulesFeature::addRules()`（`arangod/Aql/OptimizerRulesFeature.cpp:212`），逐条调用 `registerRule(名字, 函数指针, level, flags, 说明文本)`；当前源码共 82 处注册调用，最终落进按 level 排序的 `_rules` 向量（`arangod/Aql/OptimizerRulesFeature.cpp:120`）。名字到 level 的反查在 `ruleByLevel` 里用 `std::lower_bound` 二分完成（`arangod/Aql/OptimizerRulesFeature.cpp:152`）—— 这正是 `AQL EXPLAIN` 能按名字 `.using("rule-name")` 单独开关某条规则的基础。§9.3 列举的 `use-index` / `avoid-sort` 等属于这张表里的条目，本节补充的是它们**如何被调度**。

## 二十七、参考资料

1. ArangoDB 官方文档 — `https://docs.arangodb.com/`。
2. ArangoDB 源码 — `https://github.com/arangodb/arangodb`。
3. VelocyPack 规范 — `https://github.com/arangodb/velocypack`。
4. Apache RocksDB — `https://github.com/facebook/rocksdb`。
5. Pregel 论文 — `https://research.google/pubs/large-scale-graph-computing-at-google/`。
6. BSL License — `https://mariadb.com/bsl11/`。
7. ArangoDB 社区论坛 — `https://www.arangodb.com/community/`。
8. Foxx Microservice — `https://docs.arangodb.com/stable/foxx/`。
9. SmartGraph 文档 — `https://docs.arangodb.com/stable/graphs-smart-graphs.html`。
10. K8s Operator — `https://github.com/arangodb/k8s-operator`。
