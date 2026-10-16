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

## 二十四、参考资料

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
