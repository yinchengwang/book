# Memgraph Wiki（兼容 Cypher 的高性能 C++ 图数据库）

> 参考版本：Memgraph 2.x（C++20 实现，存储基于自研 LSM-Tree `Mage` + 分布式基于 Raft，Cypher 兼容，与 Neo4j 协议互通的 Bolt）。本 wiki 重点覆盖图类型专项章节（Cypher 兼容性、流式事务、内存图存储 MAGE、向量索引、LangChain 集成）。

## 一、项目定位

Memgraph 是由 **Memgraph Ltd.**（克罗地亚萨格勒布）开源的 **兼容 Cypher 的内存优先图数据库（In-Memory First Graph Database）**，以 **"real-time graph analytics at scale"** 为口号，提供 **Cypher 查询语言（兼容 Neo4j 5.x 部分）**、**Bolt 协议兼容**、**流式数据接入（Kafka / Pulsar）**、**图算法 MAGE 库**、**嵌入式 C++ 客户端**，是性能对标 Neo4j 但更现代的开源图数据库代表。

- **痛点**：
  - Neo4j 在分布式上 Commercial Only（Causal Cluster），开源版仅单实例；
  - 现有图数据库对实时流数据接入不友好（Kafka / Pulsar 需自己写 ETL）；
  - 想兼容 Cypher / Bolt 但不想被 Java 庞大运行时束缚；
  - 想做实时反欺诈（毫秒级 decisions）需要内存优先；
  - 与 LLM 集成需要向量索引 + Cypher；
- **典型用户**：金融反欺诈、网络安全、推荐系统、欺诈检测、社交网络、电信运营商、IT 网络运维、知识图谱；
- **核心口号**：**"Open source graph database, real-time analytics"** + **"the fastest graph database"** + **"real-time graph stream processing"**。

## 二、发展历史与社区

- **起源**：2017 年由 Croatian 团队创立，开发者起源 Memgraph Inc.；
- **里程碑**：
  - 2017：Memgraph 1.0 发布，Cypher / Bolt 实现；
  - 2018：1.5 加入 Storage Optimization + Distributed Coordinator 实验；
  - 2019：2.0 重写存储引擎为 MAGE，内存优先；
  - 2020：Distributed Memgraph 路线图公示；
  - 2021：2.0+ 加向量 + 全文 + Kafka Streams；
  - 2022：2.5 加 CDC + Stored Procedures + MAGE 算法；
  - 2023：2.10 / 2.14 高可用 / 数据持久化优化；
  - 2024：3.0 加入 Lakehouse + ONNX 集成 + 内存效率改进；
  - 2025：3.x LLM / 向量 / 内存 / 分布式 Raft GA；
- **维护状态**：活跃，每周数十 commit；
- **社区**：GitHub 8k+ star、Memgraph Community Forum、Discord / Slack、Memgraph Live 直播 + Community Calls；
- **生态**：与 **Kubernetes**（Helm chart）、**Kafka**、**Pulsar**、**Kubegres**、**LangChain**、**LlamaIndex**、**Jupyter**、**Streamlit**、**dbt** 等集成。

## 三、许可与商业限制

- **核心代码**：**Business Source License (BSL)** → 4 年后转 Apache 2.0；
- **商业版本**：**Memgraph Enterprise**：闭源，含 Multi-DC Replication、Centroid Algorithm、SAML/LDAP、C++ Memory Profiler；
- **云服务**：Memgraph Cloud（AWS / GCP / Azure / Self-hosted）；Memgraph Lab（Web GUI）。
- **第三方依赖**：
  - **Nlohmann Json**（MIT）：JSON 序列化；
  - **GTest**（BSD3）：测试；
  - **RocksDB**（Apache 2.0 / GPLv2 双协议，部分模块）：可选；
  - **libuv**：异步 IO；
- **生态关系**：
  - 与 **Neo4j Bolt 5** 协议兼容（驱动可直接使用，无需重写客户端）；
  - 与 **Kafka / Pulsar** 流处理集成；
  - 与 **Python（py2neo / pymgclient）** 通过 Bolt；
  - 与 **LangChain / LlamaIndex** 提供 GraphRAG / Vector Retriever；
  - 与 **dbt** 提供图模型变换。

## 四、业界主要使用方与案例

- **大型用户**（公开）：
  - **Cedars-Sinai**（医院）：医疗知识图谱；
  - **Bilt**（信用卡公司）：反欺诈；
  - **Telefónica**：电信拓扑；
  - **PlayStation (Sony)**：玩家行为图；
  - **Dataiku**：Data Science 平台集成；
  - **Salesloft / Outreach**：销售图谱；
  - **Gong.io**：销售 call 分析；
  - 国内：金融 / 电信公司部分试点；
- **行业**：金融反欺诈、电信运营商、IT 运维、推荐系统、医疗、电子游戏、Sales / Marketing；
- **典型场景**：实时反欺诈（毫秒级 decision）、客户旅程图、销售 call 图分析、网络安全（attack graph）、实时推荐、知识图谱、GraphRAG。

## 五、一句话对比定位

> Memgraph 是 **"兼容 Cypher + Bolt（Neo4j 客户端互通）+ 内存优先 + C++ 高性能 + 流式接入（Kafka / Pulsar）+ MAGE 图算法 + 向量索引 + BSL → Apache"**；与 **Neo4j（Java + 专有存储 + 商业版）**、**NebulaGraph（C++ + RocksDB 自带 + 国产）**、**ArangoDB（多模型 BSL）**、**Kuzu（嵌入式 C++）**、**DuckDB / SQLite for graph（轻量）**、**Dgraph（GraphQL 优先）** 构成图数据库阵营。Memgraph 的差异化是 **"兼容 Neo4j 接口 + 内存优先 + C++ 高性能 + 流处理"**。

## 六、整体架构

```
┌────────────────────────────────────────────────────────────────────┐
│                            客户端层                                  │
│  - Memgraph Lab（Web UI）                                          │
│  - Drivers: Bolt 5 兼容（Neo4j Python / Java / JS / Go / .NET）   │
│  - mgconsole（Cypher REPL，Python / C++）                          │
│  - Query API / Bolt / HTTP (Lab API)                              │
└─────────────────────┬──────────────────────────────────────────────┘
                      │ Bolt 5 自描述二进制协议（TCP/7687）
┌─────────────────────▼──────────────────────────────────────────────┐
│                       Frontend / Coordinator                        │
│  src/frontend/                                                     │
│  ├─ Session 管理（session.hpp）                                    │
│  ├─ BoltSession / BoltServer                                       │
│  │   ├─ Bolt（Bolt 5 协议实现）                                    │
│  │   └─ Handshake / Auth / Routing                                 │
│  ├─ Query Plan 入口（Plan.cpp / Planner.cpp）                      │
│  └─ Telemetry / Audit / Auth                                       │
└─────────────────────┬──────────────────────────────────────────────┘
                      │
┌─────────────────────▼──────────────────────────────────────────────┐
│                       Planner / Optimizer                           │
│  src/query/planner/                                                 │
│  ├─ Parser             nlohmann/json + flex/bison                    │
│  ├─ LogicalPlan       Cypher AST → Logical                         │
│  ├─ Optimizer          rule-based + cost-based                       │
│  ├─ PhysicalPlan      生成可执行的执行计划                            │
│  └─ Plan / Procedure / Trigger / Migration 模块                       │
└─────────────────────┬──────────────────────────────────────────────┘
                      │
┌─────────────────────▼──────────────────────────────────────────────┐
│                       执行器                                          │
│  src/query/                                                        │
│  ├─ Interpreter / ExecutionContext                                  │
│  ├─ Operators: Scan / Filter / Project / Expand / Join / Limit       │
│  ├─ Cartesian / Hash / Apply                                        │
│  ├─ Trigger / Procedure / Migration                                  │
│  └─ Functional / User-defined procedures                             │
└─────────────────────┬──────────────────────────────────────────────┘
                      │
┌─────────────────────▼──────────────────────────────────────────────┐
│                       存储引擎（Storage Engine v2 / MAGE）            │
│  src/storage/v2/                                                   │
│  ├─ Storage           顶层入口                                      │
│  ├─ InMemoryStorage    内存优先存储                                  │
│  │   ├─ Vertex / Edge storage（链表 + skiplist）                     │
│  │   ├─ Index（Label/Property 索引）                                │
│  │   ├─ Constraints（unique / existence）                          │
│  │   └─ Vector Index（HNSW / Faiss-style）                         │
│  │                                                                │
│  ├─ DurableStorage   RocksDB 持久化                                 │
│  │   ├─ WAL + snapshot                                              │
│  │   ├─ Snapshot（一致性快照）                                       │
│  │   └─ Recovery                                                   │
│  │                                                                │
│  ├─ DiskStorage    优化版（快照 + WAL）                                │
│  ├─ IsolationLevel  Snapshot / Read Committed                       │
│  └─ Replication    Raft / Main + Replica                            │
└─────────────────────┬──────────────────────────────────────────────┘
                      │
┌─────────────────────▼──────────────────────────────────────────────┐
│                       高可用 / 复制                                   │
│  src/replication/  src/coordination/                                │
│  ├─ Raft            自研 Raft 库（与 KV 集成）                         │
│  ├─ Coordinator     多节点协调（heartbeat、leader 选举）              │
│  ├─ Snapshot        Raft snapshot 上传到 followers                   │
│  └─ Cluster Main    主 / 副本状态机                                  │
└─────────────────────┬──────────────────────────────────────────────┘
                      │
┌─────────────────────▼──────────────────────────────────────────────┐
│                       工具与集成                                       │
│  src/integrations/   Kafka / Pulsar / Mongo / MySQL CDC            │
│  src/mage/           图算法 / UDF module                            │
│  src/dbms/           高层 DBMS 控制器                                │
│  src/audit/          审计日志                                        │
│  src/auth/           鉴权                                            │
│  src/http_handlers/  HTTP API / Admin / Lab                         │
└────────────────────────────────────────────────────────────────────┘
```

### 关键设计取舍

- **内存优先 + 持久化**：默认所有数据在内存（高性能），Durable Storage 提供 RocksDB 落盘；
- **兼容 Neo4j Bolt / Cypher**：客户端可以直接用 Neo4j 驱动；
- **C++20 与现代 C++**：模板化元编程 + 协程（实验性）+ Modules；
- **流式接入**：Kafka / Pulsar 客户端模块作为 transformation；
- **存储引擎版本演进**：Storage V1 → V2 → v2（Multi-version）+ RocksDB 选项；
- **可扩展图算法 MAGE**：用户可编写 C++ / Python / C 共享库的图算法 module；
- **LangChain / LlamaIndex 集成**：通过 Bolt / Cypher + Vector index。

## 七、存储引擎

### 7.1 InMemoryStorage（MAGE 系）

- **Vertex**：`vectors::IntStore<Vertex>` + 邻居指针；
- **Edge**：`vectors::IntStore<Edge>` + 双向链表；
- **Index**：
  - `skip_list<Label>`：Label → Vertex Set；
  - `skiplist<PropertyIndex>`：属性索引；
  - VectorIndex（HNSW）：与属性索引并存；
- **Snapshot**：周期 + 主动生成"时间点" view，提供可重复读与 exported stats；
- **Durability**：DurableStorage（RocksDB-based）和 InMemoryStorage 互为副本（enterprise）。

### 7.2 RocksDB DurableStorage

- RocksDB 列族：`vertices / edges / indices / metadata`；
- WAL + Snapshot 周期性落盘；
- 性能比纯内存慢 30%-50%，但数据可靠；
- 与 InMemoryStorage 一致性序列化（`is_wal` + `is_recovery`）。

### 7.3 Property 存储

- 不同类型（Int / Double / String / List / Map / Duration / Geographic）都有专门的 column / storage；
- ListProperty / MapProperty 内部表达为链表；
- Nested 类型：`Map<string, T>`、`List<T>` 等。

### 7.4 时间约束与索引

- **Unique Constraint**：`CREATE CONSTRAINT …` 唯一；
- **Existence Constraint**：必填属性；
- **TTL**（3.x 实验性）：自动过期。

### 7.5 备份与恢复

- 在线 SNAPSHOT 备份（实验性）；
- 离线 RocksDB 拷贝；
- `mgconsole` 配合 Cypher 导出 JSON。

## 八、索引机制

### 8.1 图存储模型

Memgraph 走"**内存优先 + 节点 → 边双向链表 + Label/Property 索引**"路线，与 Neo4j 类似但内存优先：

| 维度 | Memgraph（in-mem + 双向） | Neo4j（原图指针） | Nebula（KV+Raft） |
|------|--------------------------|------------------|------------------|
| 节点 | IntStore + 链表 | 定长 slot | RocksDB row |
| 边 | 双向链表 | 双向链表 | KV row 双方向 |
| 遍历 | 链表 + 索引 | 指针直连 | KV 跳 |
| 持久化 | snapshot / RocksDB | mmap / WAL | RocksDB native |
| 副本 | Raft（3.x GA） | 商业版 CC | Multi-Raft |

### 8.2 Label 索引

- 每个 Label 的 vertex 集合以 skiplist 维护；
- `MATCH (n:User) RETURN n` → O(log N) 索引扫；
- 不必扫全图。

### 8.3 属性索引

- 单属性 + 复合属性：B+Tree-like skiplist；
- `CREATE INDEX ON :User(email)`；
- `USING INDEX` hint。

### 8.4 全文索引

- 自带倒排索引，1.x 起内置；
- `CREATE TEXT INDEX … ON :Document(text)`；
- 查询 `CALL db.idx.fulltext.queryNodes(...)` 或 `WHERE text CONTAINS "…"`。

### 8.5 向量索引

- 2.10+ 内置 HNSW 向量索引（`CREATE VECTOR INDEX …`）；
- Backed by Hnswlib-style 实现；
- 查询：`MATCH (d:Doc) … RETURN vector_similarity(d.embedding, $v)`；
- 与 LangChain / LlamaIndex 集成。

### 8.6 边类型索引

- 隐式（边自然有 type 索引，因为存储结构）；
- 自定义：`CREATE EDGE INDEX …` 在 Enterprise 可优化特定属性。

### 8.7 索引代价

- 频繁写入大量 Label/Property 索引会写入放大（内存更新）；
- 向量索引重建时间较长；
- 索引过多降低写入吞吐。

## 九、查询/计算/接口层

### 9.1 Cypher 查询语言（部分兼容）

```cypher
// 节点 + 关系
MATCH (p:Person {name: 'Alice'})-[:KNOWS]->(friend)
WHERE friend.age > 18
RETURN friend.name, friend.age

// OPTIONAL MATCH
MATCH (p:Person)
OPTIONAL MATCH (p)-[:WORKS_ON]->(proj)
RETURN p.name, proj.name

// 可变长度路径
MATCH (a:Person)-[:KNOWS*1..5]->(b:Person)
WHERE a.name = 'Alice'
RETURN DISTINCT b.name

// 子查询 + 集合
MATCH (p:Person)-[r:KNOWS]->(friend)
WITH p, COLLECT(friend.name) AS friends
WHERE SIZE(friends) > 3
RETURN p.name, friends

// WITH 子句
MATCH (n:Person)
WITH n, COUNT { (n)-[:KNOWS]->() } AS num
WHERE num > 5
RETURN n.name, num

// 向量 + 全文本
CALL db.idx.vector.search('DocEmbedding', 5, $vector) YIELD node, similarity
RETURN node
```

### 9.2 计划与优化

- **Cypher → Plan**：
  - AST → 规则优化 → Plan 树；
  - 优化规则：谓词下推、动态过滤、RBO；
  - 物理执行：Volcano-style + 部分向量化；
- `EXPLAIN / PROFILE`；
- 缓存:`Plan cache` 减少重复解析。

### 9.3 Bolt 协议

- Bolt 5 自描述二进制协议，与 Neo4j 同协议层；
- 客户端驱动复用：
  - Java：`org.neo4j.driver:neo4j-java-driver`；
  - Python：`neo4j`；
  - JS：`neo4j-driver`；
- 路由：`+serverRouting` → 多实例 cluster 时路由。

### 9.4 图算法库 MAGE

Memgraph 暴露图算法库：

```cypher
CALL pagerank.get()
  YIELD node, rank
  SET node.rank = rank
RETURN node.name, rank
```

- 包含算法：
  - Centrality：PageRank、ArticleRank、Betweenness、Closeness、Eigenvector、Degree、HITS、WCC、SCC；
  - Path：BFS / DFS / Dijkstra / A*；
  - Community：Louvain、Label Propagation、Connected Components、Modularity、Clique / K-Core；
  - Node Embedding：FastRP、GraphSAGE、Node2Vec；
  - Similarity：Jaccard / Cosine / Overlap；
  - Link Prediction：Adamic-Adar、Common Neighbors、Preferential Attachment、Resource Allocation；
  - 节点 / 边权重自定义函数；
- **MAGE** = Memgraph Algorithm Graph Extensions → 用户可用 C++ / Python / Cython / shared lib 编写并加载（`load_module(...)`）。

### 9.5 流处理（Kafka / Pulsar）

- `src/integrations/kafka/` & `pulsar/`；
- Streams API：`CREATE STREAM source STREAM_TOPIC ...`；
- Transformer：`CALL mg.transform(...)`；
- 流式查询：`MATCH (n) WITH n, … RETURN n`；
- Apply on every event from Kafka topic。

### 9.6 触发器 / 存储过程

- `CREATE TRIGGER … EXECUTE …`；
- 触发器可在事务前后执行；
- Procedure：`CALL … RETURN …`；
- Memgraph 启用 CPython / Embedded JS（实验性）。

### 9.7 LLM / 向量

- Vector Index + 嵌入式 LLM 函数；
- `CALL ai.translate("Alice", "es")`；
- `CALL ai.summarize($text)`；
- 与 LangChain / LlamaIndex 集成：`MemgraphVectorStore` / `MemgraphGraphRetriever`。

## 十、事务与一致性

### 10.1 ACID 事务

- 默认 Snapshot Isolation（基于 snapshot + version）；
- 显式 `BEGIN / COMMIT / ROLLBACK`；
- Read 走 snapshot → 保证 repeatable read / repeatable read of data;
- Write 走 raft + WAL → atomic commit;

### 10.2 隔离级别

- 默认 Snapshot Isolation（SI）；
- `IsolationLevel::SNAPSHOT` 或 `READ_COMMITTED` 配置；
- 无 phantoms、无 dirty reads。

### 10.3 锁

- 乐观并发（OCC）实现：基于 version；
- 写时检查 version，无冲突 → commit；冲突 → 重试；
- Enterprise 提供 "Force single-threaded transactions" 用于特殊场景。

### 10.4 写后写（write-write）

- 用 `version` 字段 + commit-time check；
- 失败重试。

### 10.5 写后读（read-after-write）

- 默认弱一致；
- Session 内 enforce read-after-write by routing to leader。

## 十一、复制与高可用

### 11.1 单实例

- 单 `memgraph` 进程 + Bolt + HTTP；
- 适合 embedding、Cypher 开发、单元测试。

### 11.2 Main / Replica（Raft）

- 2.x / 3.x GA，Enterprise 提供 Raft replication；
- Main（写入）+ Replica（流式）模式；
- Heartbeat + election 触发 failover；
- Bolt `+serverRouting` 路由到 Main。

### 11.3 Multi-DC / GEO Replication

- Cluster Main + Cluster Replica（Enterprise）：异步复制多机房。

### 11.4 集群发现

- Cluster coordinator：`-cluster-name`；
- 启动 mode：`coordinator`、`data`、`coordinator+data`；
- 节点间 heartbeat（coordinator ↔ data）；
- 加入 / 离开集群通过 `cluster add / leave`。

### 11.5 备份

- `mgconsole`：dump to JSON；
- snapshot 实验性；
- RocksDB durable storage 离线拷贝；
- `online-backup` 实验性命令。

## 十二、扩展性

- **水平扩展**：通过 Enterprise Main + Replica 集群；
- **垂直扩展**：受内存限制；x86_64 16TB+，ARM64；
- **冷热分层**：实验性，partition 引擎到 RocksDB；
- **大型部署**：256GB+ 内存 + 多实例；
- **多租户**：Multi-database (Memgraph 2.x) Experimental。

## 十三、性能与基准

- **LDBC SNB**：Memgraph 在 SF=30 上 30k-60k QPS（内存优先，比 Neo4j Community 快 2-3 倍）；
- **官方 benchmark**：pagestarter benchmark；
- **典型数字**：
  - 单实例 1-hop 邻居查询：<1ms（内存）；
  - 3-hop：<10ms（10w vertices）；
  - PageRank 1 亿节点：秒级；
  - 向量 ANN（1.5k dim, 10w vec）：<5ms；
- **参考**：`https://memgraph.com/blog/memgraph-vs-neo4j-performance`（Memgraph 官方）显示 Memgraph 在某些 query 上快 4 倍以上。

## 十四、运维与可观测性

- **指标**：Prometheus（`/metrics`）、OpenTelemetry；
- **Slow log**：`EXPLAIN / PROFILE` + 数据库 `slow_log`；
- **Audit log**：Enterprise；
- **CLI**：`mgconsole` + `systemctl status memgraph`；
- **Memgraph Lab**：Web GUI（可视化、监控、Cypher 编辑）；
- **部署**：Docker、Kubernetes Helm chart、Operator（社区）；
- **备份**：JSON export + RocksDB snapshot；
- **升级**：rolling upgrade；
- **Memory tuning**：
  - 设置 `MGMAPSIZE` 等参数；
  - 内存监控使用 `--storage-properties-on-edges=true` 等。

## 十五、生态与集成

- **客户端**：
  - Python：`pymgclient`（官方封装 Bolt） + Neo4j `neo4j` 驱动；
  - Go：`mgclient-go`；
  - Java：`mgclient-java`（或 Neo4j 官方）；
  - C++：embedded；
  - Node.js：`mgclient-node`；
- **可视化**：Memgraph Lab（Web UI）+ Neo4j Browser（可连接）+ Bloom；
- **图算法库**：MAGE；
- **消息队列**：Kafka、Pulsar Streams（实验性 CDC）；
- **BI / ETL**：dbt-memgraph、CSV import；
- **容器化**：官方 `memgraph/memgraph` Docker + Helm chart；
- **AI / GenAI**：
  - LangChain Memgraph 集成 (`langchain_memgraph`)；
  - LlamaIndex Memgraph Store；
  - Vector Search / Hybrid Search（向量 + 属性过滤）；
  - LLM-as-a-Service（3.x）：与 OpenAI / Anthropic 一键集成。

## 十六、核心论文与算法原理

- **Cypher 规范**：参考 `https://opencypher.org/`（部分实现）；
- **Neo4j Bolt 协议**：`https://neo4j.com/docs/bolt/current/bolt-spec/`；
- **Raft 论文**：Ongaro & Ousterhout 2014；
- **HNSW 论文**：Malkov & Yashunin 2018；
- **OpenCypher TCK 兼容测试**：Neo4j 开源 + openCypher 标准化路径；
- **PageRank / Louvain / Label Propagation**：原始论文索引；
- **Bloom 过滤 + skiplist**：通用算法；
- **Bolt 5**：默认带 authentication + routing；
- **参考**：`https://memgraph.com/blog` 与 `https://docs.memgraph.com/`。

## 十七、源码学习路径

| 阶段 | 模块 | 路径 | 要点 |
|------|------|------|------|
| 入门 | Memgraph main | `src/memgraph.cpp` | 入口 |
| 入门 | Bolt Server | `src/communication/` `BoltServer.cpp` | Bolt 协议 |
| 入门 | Session | `src/frontend/` `session.cpp` | 会话 |
| 进阶 | Cypher Parser | `src/query/parser/` `parser.cpp` | 词法 + 语法 |
| 进阶 | Planner | `src/query/planner/` `planner.cpp` | 计划生成 |
| 进阶 | Optimizer | `src/query/optimizer/` | 优化 |
| 进阶 | Interpreter | `src/query/interpreter_context.cpp` | 解释执行 |
| 高级 | Storage V2 | `src/storage/v2/` `storage.cpp` | 存储 |
| 高级 | Indices | `src/storage/v2/indices/` | Label / Property / Vector |
| 高级 | Replication | `src/replication/` + `src/coordination/` | Raft |

## 十八、源码规模与开发语言

- **LOC**：约 50 万行 C++；
- **语言占比**：
  - C++ ≈ 90%；
  - Python（Cypher Procedure / Lab）≈ 6%；
  - Rust（少量工具）≈ 1%；
  - Shell / CMake / Lua ≈ 3%；
- **构建系统**：CMake（`CMakeLists.txt`）+ `cmake/neon_compiler_options.cmake` 等内部模块；
- **C++ 标准**：C++17（核心），部分模块 C++20；
- **依赖**：`nlohmann/json`、`RocksDB`（Enterprise 可选）、`gtest`、`Abseil`、`spdlog`；
- **CI**：GitHub Actions + Memgraph 内部 CI；
- **代码风格**：`docs/style-guide.md`（clang-format + 自定义）。

## 十九、学习资源

- **官方文档**：
  - `https://docs.memgraph.com/`；
  - `https://memgraph.com/docs/getting-started/`；
  - `https://memgraph.com/blog/`；
  - `https://memgraph.com/lab/`（在线 Lab sandbox）；
- **书籍**：
  - "Building Knowledge Graphs with Memgraph"（Memgraph 自家）；
  - "Real-Time Graph Analytics"（早期付费 guide）；
  - "Mastering Memgraph"（社区作者）；
- **视频 / 课程**：Memgraph Live 直播（YouTube/Zoom）、Memgraph 官方 YouTube channel、HackerEarth DBMS 课程、Coursera 演讲、Memgraph 认证工程师（Memgraph Certified）。
- **社区**：Memgraph Community Forum、Discord、Reddit r/Memgraph、Stack Overflow `[memgraph]`；
- **实战项目**：参考 `https://github.com/memgraph` 各子仓（`mage`、`lab`、`integrations/`）、Memgraph 官方 blog tutorials；
- **Playground**：`https://memgraph.com/product/lab` 提供在线 sandbox（无安装可玩）。

## 二十、可移植性与部署

- **支持平台**：Linux（x86_64 / ARM64）、macOS（x86_64 / ARM）、Docker 镜像；
- **K8s**：社区 Helm chart + 实验性 Operator；
- **容器化**：`memgraph/memgraph:latest`、Memgraph + MAGE；
- **嵌入式 API**：C++ 可链接 Memgraph lib（实验性）；
- **Python Embedded**：通过 `pymgclient` + 远程 Bolt；
- **Multi-DC**：Enterprise 跨地域部署。

## 二十一、变体与衍生项目

- **Memgraph Enterprise**：商业闭源（含 Multi-DC、Audit、Streams 等）；
- **Memgraph Cloud**：Vesoft / 商业云；
- **Memgraph Lab**：Web GUI 开源；
- **MAGE 开源 / Enterprise**：图算法库；
- **官方驱动**：Python `pymgclient`、Java mg-client（开源）。
- **上下游**：
  - 上游：Neo4j Cypher / Bolt；
  - 下游：Memgraph Lab、LangChain Memgraph、Memgraph-Streamz（Kafka/Pulsar 转换）。

## 二十二、风险与陷阱

### 22.1 内存优先 → 容量限制

- 默认所有 vertex / edge 在 RAM → 内存容量上限；
- 大图（> RAM）需用 Durable Storage（性能降低 30%-50%）；
- 监控内存使用：`VIRT` ≠ `RES`，关注实际 `RSS`；
- `vm.overcommit_memory`、swap 调优。

### 22.2 兼容 Cypher 但不全

- 非 100% 兼容（Neo4j 部分 Cypher 高级 pattern 不支持）；
- 部分 OPTIONAL MATCH 子查询、EXISTS、CALL IN TRANSACTIONS、gds 子集有差异；
- 建议从 Neo4j 迁回时跑测试套件（openCypher TCK）。

### 22.3 BSL 许可

- BSL 1.1：4 年内版本不能用于商业产品封装；
- 4 年后版本转 Apache 2.0 → 适合长期使用；
- 不能直接修改 / fork 商业化 → 需 Memgraph 协商。

### 22.4 Raft 集群限制

- 3.x GA Raft → Main / Replica；
- 主节点压力大（处理写入）；
- 一致性同步慢 → Multi-DC RTT 影响；
- Enterprise 配置需谨慎。

### 22.5 Storage V2 实验性

- 3.x 仍在演进，2.x V2 已 GA；
- 升级 V1→V2 需 dump；
- `mg.check-schema`、`mg.migrate` 等工具支持。

### 22.6 Kafka / Pulsar Streams

- Streams 与主查询共用资源；
- 高频 stream 易打爆 worker pool；
- offset / exactly-once：实验性；
- 升级 / 重启时需要小心 checkpoint。

### 22.7 向量索引重建

- HNSW 重建时间长（分钟级）；
- 调整 `efConstruction` / `M` 决定召回 vs 内存；
- `hnsw.resize` 实验性命令。

### 22.8 数据迁移

- 从 Neo4j 迁出：用 Neo4j dump CSV → Memgraph CSV import；
- 跨版本迁移：dump → upgrade → load；
- 用 `mg.export` 工具导 JSON。

### 22.9 客户端

- Neo4j 驱动虽可连接 Memgraph，但 routing-aware 不一定工作；
- 自己业务代码仍需指定 Bolt；
- Java driver 的 SSL/Auth 设置特殊。

## 二十三、与本项目的关联

- **Cypher / Bolt 实现**：本项目如未来扩展图，可参考 Memgraph 对 Bolt 协议的兼容思路；
- **内存优先**：本项目 KV + LSMT 方式与 Memgraph 内存版类似；
- **图算法库 MAGE**：可作为 "C++ 图算法库"的参考实现；
- **流式集成**：可参考 Memgraph Kafka 集成；
- **LangChain / LlamaIndex 集成**：本项目若做 GraphRAG，可复用其思路。

## 二十四、参考资料

1. Memgraph 官方文档 — `https://docs.memgraph.com/`。
2. Memgraph 源码 — `https://github.com/memgraph/memgraph`。
3. Memgraph Lab — `https://github.com/memgraph/memgraph-lab`。
4. MAGE 算法库 — `https://github.com/memgraph/mage`。
5. Bolt 5 协议规范 — `https://neo4j.com/docs/bolt/current/bolt-protocol/`。
6. openCypher TCK — `https://github.com/opencypher/opencypher-tck`。
7. Cypher 规范 — `https://opencypher.org/`。
8. LangChain Memgraph 集成 — `https://python.langchain.com/docs/integrations/graphs/memgraph/`。
9. Raft 论文 — `https://raft.github.io/raft.pdf`。
10. HNSW 论文 — `https://arxiv.org/abs/1603.09320`。
