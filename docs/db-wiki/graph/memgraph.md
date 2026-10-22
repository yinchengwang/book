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

## 二十四、Bolt v2 存储引擎：Delta Chain 与两阶段垃圾回收

Memgraph 3.x 的存储实现只有一棵代码树 `src/storage/v2/`，运行模式由三值枚举 `StorageMode` 决定（`src/storage/v2/storage_mode.hpp:23`）：`IN_MEMORY_TRANSACTIONAL`（内存基表 + WAL / snapshot）、`IN_MEMORY_ANALYTICAL`（只读分析，不产生版本）、`ON_DISK_TRANSACTIONAL`（RocksDB 承载）。**Delta Chain 只在事务型内存模式下存在**——`CreateDeleteObjectDelta` 与 `CreateAndLinkDelta` 在分析型模式下直接提前返回，连 delta 都不生成（`src/storage/v2/mvcc.hpp:242`、`src/storage/v2/mvcc.hpp:315`）。本节拆解这套 Bolt 风格的多版本结构。

### 24.1 Delta 的定长布局与标签指针压缩

顶点本体极薄（`src/storage/v2/vertex.hpp:32`）：`gid` 常量 + `labels` 小向量（`src/storage/v2/vertex.hpp:41`）+ `in_edges` / `out_edges` 两条边三元组小向量（`src/storage/v2/vertex.hpp:43`、`src/storage/v2/vertex.hpp:44`）+ `PropertyStore properties`（`src/storage/v2/vertex.hpp:46`）。**所有历史版本都不在顶点上**，而是外挂在一条 delta 链上。

`Delta` 是定长 POD：`CommitInfo *commit_info` + `uint64_t command_id` + `prev` + 原子 `next` + 一个按 `action` 区分的 union 载荷（`src/storage/v2/delta.hpp:244`）。编译器必须满足 `sizeof(Delta) <= 56`（`src/storage/v2/delta.hpp:408`），由此可以算出内存密度上界：

```
每页可容纳 delta 数 ≤ ⌊4096 / 56⌋ = 73
每 slab 可容纳 delta 数 = ⌊(4×4096 − 8) / 56⌋ = 292      // 见 src/storage/v2/delta_container.hpp:59
```

后者正是源码里的硬断言 `static_assert(292 == delta_slab::capacity())`（`src/storage/v2/delta_container.hpp:62`）。

两个标签指针是这里最关键的省字节技巧：

- **`PreviousPtr`**（`src/storage/v2/delta.hpp:59`）把 `NULL_PTR / DELTA / VERTEX / EDGE` 四种指针类型编码进指针**低 2 位**。之所以可行，是因为被指向的三种结构都含 `uint64_t` 字段、要求 8 字节对齐，指针低 3 位必然为 0。取用时先 `value & kMask` 解类型，再 `value & ~kMask` 还原地址（`src/storage/v2/delta.hpp:96`）。
- **`TaggedVertexPtr`**（`src/storage/v2/delta.hpp:199`）把 `DeltaChainState` 三态编码进顶点指针**低 2 位**，使 delta 能携带链状态而不增加 `sizeof(Delta)`。

结果是：**一次属性更新只追加一条记录「被改动的那一个字段」的 delta，写放大与对象大小无关，仅与改动量成正比**——改一个属性不必重写顶点本体。

### 24.2 delta_container：页对齐 slab 分配器

delta 不逐个 `new`，而是走 slab。`delta_container`（`src/storage/v2/delta_container.hpp:239`）持有一个 `PageAlignedList<delta_slab>`，每次 `emplace` 先看队首 slab 是否满，满了就在**队首**插一个新 slab（`src/storage/v2/delta_container.hpp:295`）。slab 用 `kRemainingPageSpace = 4 页 − sizeof(void*)` 字节（`src/storage/v2/delta_container.hpp:59`）静态定容，选 4 页（16 KiB）的理由写在注释里：这是 jemalloc 的最小 large size class。

```
emplace(args):
  if deltas_.empty() || deltas_.front().is_full():
      deltas_.emplace_front()          # 新 slab 插到队首，保持 LIFO
  delta = deltas_.front().emplace_back(args...)
  if 需要额外内存（属性值 / 字符串）:
      lazily 创建 PageSlabMemoryResource（上游 = DB arena）
```

一个对象在自己的生命周期内产生的全部 delta 都落在**同一块 slab** 上，清空时整体归还，避免逐个析构。

### 24.3 Delta 链接的四步定序

`CreateAndLinkDelta`（`src/storage/v2/mvcc.hpp:315`）把新 delta 挂到对象链头，顺序被严格固定，因为 GC 与其他执行线程会并发遍历这条链：

```
1. delta->next.store(object->delta(), release);   // 先接后继
2. delta->prev.Set(object);                       // 再指回宿主对象
3. if object->delta(): object->delta()->prev.Set(delta);  // 此刻 GC 可见新 delta
4. object->SetDelta(delta);                       // 最后换头，读侧此时持对象锁
```

第 3 步与第 4 步之间的窗口里，新 delta 已可见但链头未换，所以源码在第 4 步注释里明确说明：读者必须持对象 `RWSpinLock` 读 `delta()` 才能安全。

### 24.4 非顺序 Delta 与 DeltaVertexCache

常态下 delta 按提交序串成一条链，读到别人事务的未提交 delta 就停——这是标准 MVCC。但**边导入场景**（百万级 bulk load）里，若严格串行化，同一个超级节点上的并发建边会退化成串行。Memgraph 的做法是允许「非顺序 delta」被前插到别人的未提交 delta 之前（`src/storage/v2/delta.hpp:186` 的 `DeltaSequencing`），代价是遍历与 GC 变重，收益是导入吞吐（`src/storage/v2/delta.hpp:180` 的注释即如此论述）。

代价是**定位宿主顶点不能再只看 `prev`**：非顺序 delta 的 `prev` 可能指向另一个 delta。`DeltaVertexCache::GetVertexFromDelta`（`src/storage/v2/inmemory/storage.cpp:258`）负责这件事：

```
GetVertexFromDelta(delta):
  if delta->prev.type == VERTEX: return delta->prev.vertex   // 常见路径 O(1)
  if delta in cache: return cache[delta]
  从 delta 沿 prev 向上游走，直到遇到 prev.type == VERTEX
  把本轮走过的所有「同事务子链头」一并写入 cache
  遇到跨事务边界时，先查 cache 再决定是否继续走
```

复杂度：单次查询最坏 `O(L)`（`L` = 链长），但因为缓存了途经的所有子链头，**一个事务内访问 N 个 delta 的总成本摊还到 `O(L + N)`**，而不是 `O(N·L)`。

### 24.5 两阶段垃圾回收

`CollectGarbage`（`src/storage/v2/inmemory/storage.cpp:3045`）的设计核心写在 `src/storage/v2/inmemory/storage.cpp:3108` 的注释里：**GC 必须分两阶段**。

先定义回收视界（horizon）：

```
oldest_active_start_timestamp = commit_log_->OldestActive()   // src/storage/v2/inmemory/storage.cpp:3116
// 再与未处理的 schema 更新起点取 min（src/storage/v2/inmemory/storage.cpp:3122）
```

只有 `commit_ts < oldest_active_start_timestamp` 的 delta 才可能没有任何活跃事务再读到。

```
阶段 A：摘链（unlink），不释放
  for 每个 committed_transactions_ 条目 g:
      if g.unlinkable_timestamp_ >= oldest_active_start_timestamp: continue  // src/storage/v2/inmemory/storage.cpp:3257
      持对象锁 → 复查该 delta 仍在链头 → UnlinkAndRemoveDeltas(...)        // src/storage/v2/inmemory/storage.cpp:205
      把整批 delta 移入 garbage_undo_buffers_（带 mark_timestamp_）

阶段 B：释放（free）
  若持有排他主锁（即无活跃事务）: garbage_undo_buffers_.clear()
  否则按 mark_timestamp_ 有序弹出 front，条件同上                  // src/storage/v2/inmemory/storage.cpp:3214
```

摘链与释放分离的原因：一个刚被摘链的 delta，可能仍被某个已存在的事务持有引用（它正靠这条 delta 来终止版本链遍历），因此不能立即 `delete`，只能标记延迟。

**并发安全的关键细节**写在 `src/storage/v2/inmemory/storage.cpp:3262`——当被摘的 delta 恰是链头时存在竞态：

```
[Vertex] → [Delta A]                      GC 线程认定 A 是链头
                                          TX 线程插入 B，A 成为 B 的 next
[Vertex] → [Delta B] ⇄ [Delta A]
                                          GC 线程把 A 从 Vertex 上摘掉
[Vertex] → (nullptr)                      ← A 的 next 仍指着 B，B 成孤儿
```

解法是「先拿对象锁 → 再复查链头是否仍是本 delta → 是才摘」，把无锁竞态转成加锁复查。

**快速丢弃路径**：若本次 GC 处于空闲（`try_lock` 拿到 `gc_lock_`，`src/storage/v2/inmemory/storage.cpp:1023`），事务提交时不必等 GC，而是自己调 `FastDiscardOfDeltas`（`src/storage/v2/inmemory/storage.cpp:1381`）→ `GCRapidDeltaCleanup`（`src/storage/v2/inmemory/storage.cpp:1346`）完成同样的摘链，把延后释放的部分交还给 GC。源码注释明确了这个 `try_lock` 不能换成阻塞锁，否则会与事务侧的 `engine_lock_` 构成死锁环。

**非顺序 delta 的贡献者协议**（`src/storage/v2/inmemory/storage.cpp:3144`）：带非顺序 delta 的已提交事务先进 `waiting_gc_deltas_` 候场室，只有当链上所有「贡献者」事务都提交或回滚后，才取贡献者中的最高提交时间戳作为 `unlinkable_timestamp_` 并转入正常 GC 队列。

**GC 的成本优化——IndexArming**：全量扫描每个索引太贵，所以 `IndexArming`（`src/storage/v2/index_arming.hpp:38`）在写路径上按位记录「本次写可能弄脏哪些索引」——`note_label` / `note_vertex_property` / `note_edge_property`（`src/storage/v2/index_arming.hpp:105`）。GC 阶段只扫被 arm 的索引，其余跳过（`src/storage/v2/inmemory/storage.cpp:501` 一连串 `RunGC()` 调用；单索引扫描入口见 `src/storage/v2/inmemory/label_index.hpp:237`）。

GC 总体复杂度（设 `D` = 本轮待摘 delta 数、`I` = 被 arm 的索引总条目数）：

```
摘链阶段  O(D · 平均对象锁竞争)
索引清理  O(I_arm)  而非 O(I_all)
释放阶段  O(D)
```

## 二十五、查询计划：算子树、七级重写流水线与代价模型

本节是对 §9.2 的下探：§9.2 只列了「AST → 规则优化 → Plan 树」四个要点，这里给出 Memgraph 实际执行规划的具体结构、每个重写器的职责与代价函数的真实参数。

### 25.1 逻辑算子族与计划树

Memgraph 采用**火山模型（Volcano / iterator）**：每个算子都是 `LogicalOperator` 子类，对外只暴露 `Pull()` 拉取一行与 `Frame`（符号表 + 位置映射）。全量算子定义在 `src/query/plan/operator.hpp`，家族包括：

- **扫描**：`ScanAll`（`src/query/plan/operator.hpp:595`）及其按索引特化的子类 `ScanAllByLabel` / `ScanAllByLabelProperties` / `ScanAllByVertexProperty` / `ScanAllByPointDistance` 等；
- **连接与过滤**：`Expand`（`src/query/plan/operator.hpp:1089`）、`ExpandVariable`（变长路径）、`Filter`（`src/query/plan/operator.hpp:1323`）、`EvaluatePatternFilter`；
- **改写类**：`Produce` / `CreateNode` / `CreateExpand` / `SetProperty` / `SetLabels` / `Delete`；
- **流控类**：`Unwind`（`src/query/plan/operator.hpp:2723`）、`Limit` / `Skip` / `Distinct` / `OrderBy` / `Aggregate` / `Accumulate`；
- **Enterprise 专属**：`AggregateParallel` / `OrderByParallel` / `ScanParallel*`。

规划入口 `MakeLogicalPlan`（`src/query/plan/planner.hpp:149`）的流程是：`CollectQueryParts` 把 AST 拆成 `QueryParts` → 用 `RuleBasedPlanner`（规则驱动，产出单一计划）或 `VariableStartPlanner`（变量起点，产出**多个候选计划**）生成计划树 → 每个候选依次经过 `Rewrite` → `ValidatePlan`（`src/query/plan/rewrite/plan_validator.hpp`）→ `EstimatePlanCost`，最后择优。

多候选择优不是纯比代价，而是**索引提示优先**：注释写明「一旦有计划用到了 index hint，就拒绝所有没用到的」，因为用户显式写了 `USING INDEX HINT` 就是要强制该计划（`src/query/plan/planner.hpp:191`）。排序键实际是二元组 `(use_index_hints, cost)` 的字典序。

### 25.2 七级重写流水线

`PostProcessor::Rewrite`（`src/query/plan/planner.hpp:74`）用自定义的管道算子把计划依次喂给七个重写器，顺序固定：

| 级 | 重写器 | 源码 | 职责 |
|----|--------|------|------|
| 1 | `RewriteEnumAccess` | `src/query/plan/rewrite/enum.hpp:246` | 把枚举属性过滤下推成范围扫描 |
| 2 | `RewriteWithIndexLookup` | `src/query/plan/rewrite/index_lookup.hpp:2194` | `ScanAll` + `Filter` → `ScanAllBy<Index>` |
| 3 | `RewriteWithJoinRewriter` | `src/query/plan/rewrite/join.hpp:768` | 合流型改写（多分支归一为 join） |
| 4 | `RewriteWithEdgeIndexRewriter` | `src/query/plan/rewrite/edge_index_lookup.hpp:1371` | `ScanAll` / `Expand` → `ScanAllByEdgeType*` |
| 5 | `RewritePeriodicDelete` | `src/query/plan/rewrite/periodic_delete.hpp:72` | 为 `DELETE` 注入分批缓冲，配合 `USING PERIODIC COMMIT` |
| 6 | `RewriteWithPruningBFS` | `src/query/plan/rewrite/pruning_bfs.hpp:31` | 标记可剪枝的变长路径展开 |
| 7 | `RewriteParallelExecution` | `src/query/plan/rewrite/parallel_rewrite.hpp:1022` | 拆成并行算子（`MG_ENTERPRISE` 条件编译，源码注释标注 "Keep at the end"） |

第 2 级是核心。它的决策函数 `FindBestIndexGroup`（`src/query/plan/rewrite/index_lookup.hpp:1443`）对每个候选 label 组合打分：

```
best_group.vertex_count = INT64_MAX
for each label_group in or_labels:                    // or_labels = OR 语义下的 label 分组
    for each label in group:
        for each LabelPropertiesIndexCandidate:
            vertex_count = db_->VerticesCount(label_id, properties)
            has_hint    = index_hints_.HasLabelPropertiesIndex(...)
            if vertex_count < best_vertex_count || (has_hint && !best_has_hint):
                记录为该 label 的最佳属性索引
        若无属性索引可用，退化到该 label 的纯 LabelIndex
    取 vertex_count 最小者作为整组的选择
```

即「**优先用选择性最高的索引**（条目数最少者胜出），其次才是 label 索引」；索引提示是一票否决式的优先级。

第 4 级与第 2 级对称，复用同一个 `OrderByEliminator`（`src/query/plan/rewrite/order_by_elimination.hpp:41`）处理边属性索引与 ORDER BY 的配合。

### 25.3 CompactFilters：把成对比较折叠成 RangeOperator

`CompactFilters`（`src/query/plan/rewrite/range.cpp:89`）是典型的谓词规范化。它先把 `AND` 连接的合取式拆成 `filters` 数组，再两两配对：

```
can_range(f):
  归一化 PropertyComparisonType：识别 n.prop OP value 与 value OP n.prop 两种写法，
  后者反转运算符（value > n.prop ≡ n.prop < value）
  → {LESS | GREATER | UNKNOWN}

Compatible(a, b):  a.type != b.type（一个上界一个下界）
                   且 property 相同
                   且 ident 的 symbol_pos 相同

for i in 1..len(filters):
    for j in 0..i-1:
        if infos[i].Compatible(infos[j]):
            用 RangeOperator 替换 filter[j] 与 filter[i]，两者置为 UNKNOWN 防止重复配对
```

关键收益在配对被折叠掉之后，剩下两个「半开区间」约束变成**一个可下推给索引的区间条件**，索引层不必逐条比较。源码 `src/query/plan/rewrite/range.cpp:128` 的注释就是 "Combine and switch to range"。

复杂度：朴素两两配对 `O(n²)`，`n` 为单个 `AND` 的合取项数量；头部注释承认这是当前实现，并指出若引入编译期字面量常量，可以把多条约束进一步压成一个区间，甚至提前判定区间为空。

### 25.4 平衡 Union：把 O(N) 深度压到 O(log N)

当一个节点同时命中多个 label 时（`MATCH (n:A|B|C|…)`），索引改写会生成 N 个独立扫描。若朴素左深串接，算子树深度是 `O(N)`，N 很大时会压爆执行线程栈。`BalancedDisjunctionUnion`（`src/query/plan/rewrite/balanced_union.hpp:58`）改为折叠成平衡二叉 Union 树，并在顶部挂**单个** `Distinct` 去重。

其注释（`src/query/plan/rewrite/balanced_union.hpp:31` 起「Preconditions」段）明确列出成立前提，也正是这些限定说明它不能推广到别处：

1. 每个扫描产出同一个输出符号 `node_symbol`，故各层符号映射一致；
2. 分支是纯集合并（`Union` 是拼接），故一次全局 `Distinct` 等价于逐层去重；
3. `Union(A,B)` 与 `Union(B,A)` 代价相同，总基数恒为分支和，配对与嵌套方式可自由优化深度——**分支不必等代价**（一个 label 可能命中几个点，另一个命中数百万），按索引顺序而非代价配对。

明确禁止复用于查询级 `UNION`（符号不同、`UNION ALL` 语义）与 `Cartesian`（左右顺序即代价决策）。

单扫描时原样返回，不加 `Union` 也不加 `Distinct`。

### 25.5 代价模型

代价估算器是一次对计划树的访问（`src/query/plan/cost_estimator.hpp`），维护 `(cost_, cardinality_)` 两个累加量。核心递推式（`src/query/plan/cost_estimator.hpp:602`）：

```
IncrementCost(param):
    cost_ += max(kMinimumCost, param × cardinality_) / num_threads_
```

即**代价 ∝ 到该算子时的中间结果行数**（这就是所谓「行数驱动代价」）。`kMinimumCost = 0.001` 给每个算子兜底，避免空表上的排序等操作被判为零代价。参数表在 `src/query/plan/cost_constants.hpp:18`：

| 算子 | `CostParam` | 基数乘子 `CardParam` |
|------|------------|----------------------|
| `ScanAll` | 1.0 | — |
| `ScanAllByLabel` 及各索引扫描 | 1.1 | — |
| `Expand` | 2.0 | 3.0（若有统计则用真实平均度 `stats->degree`） |
| `ExpandVariable` | 3.0 | 9.0 |
| `Filter` | 1.5 | 0.25 |
| `EdgeUniquenessFilter` | 1.5 | 0.95 |
| `Unwind` | 1.3 | 无字面量时 10.0 |

`Filter` 的 0.25 是假设选择率 25%；0.95 的边去重因子近似「随机图里已访问邻居占比很小」。二分支算子（`Union` / `RollUpApply` / `HashJoin`）直接把两支代价与基数**相加**，而不是相乘——因为两个分支是并行物化而非嵌套。

排序代价单独处理（`src/query/plan/cost_estimator.hpp:608`）：

```
IncrementOrderByCost():
    IncrementCost(kOrderBy × log2(max(kOrderByMinCardinality, cardinality_)))
```

`kOrderByMinCardinality = 2.0`（`src/query/plan/cost_constants.hpp:42`）是刻意设的下限，注释直言「防止排序在空表上看起来免费」——否则 `log2(1) = 0` 会让 `ORDER BY` 在代价上凭空消失。

### 25.6 计划缓存

缓存类型是两级（`src/query/cypher_query_interpreter.hpp:162`）：

```
PlanCache_t   = utils::LRUCache<frontend::HashedString, std::shared_ptr<PlanWrapper>>
PlanCacheLRU  = utils::Synchronized<PlanCache_t, utils::RWSpinLock>
```

每个 `Database` 自带一个实例（`src/dbms/database.hpp:263`），容量由 `--query-plan_cache_max_size` 控制（默认 1000，`src/query/cypher_query_interpreter.cpp:42`）。

`CypherQueryToPlan`（`src/query/cypher_query_interpreter.cpp:305`）的命中判据有**三道闸**，任一不过就整条作废并重规划：

1. **模式开关**：启用实验性 `PLANNER_V2` 时**完全绕过缓存**，注释写明「计划可能随 v2 演进而变」；
2. **索引就绪**：缓存计划记着它依赖的索引集合，用 `CheckIndicesAreReady` 复查；若此刻拿不到 accessor，则退化为「该计划不需要任何索引」才可复用（`src/query/cypher_query_interpreter.cpp:315`）；
3. **模块代龄**：`IsFresh(ast_storage, module_generation, module_generation)`，确保用户加载的 UDF 模块版本没变（`src/query/cypher_query_interpreter.cpp:317`）。

键是**去字面量化的查询文本**（`StrippedQuery`），所以 `WHERE age > 30` 与 `WHERE age > 40` 共享同一份计划。写入侧还要过 `is_cacheable_plan`；失效侧由 `PlanInvalidatorForDatabase`（`src/dbms/database.cpp:34`）在 schema 变更时按时间戳逐条 `invalidate`。

## 二十六、索引结构内部实现：SkipList MVCC、usearch 向量检索与 RocksDB 落盘

§八 列出了 Memgraph 提供哪些索引能力，本节下探到实现层：节点布局、MVCC 条目格式、索引快照切换、向量检索的第三方实现，以及落盘模式的键空间。

### 26.1 SkipList：可变长节点与二项式层高

Memgraph 自己手写了并发跳表 `utils::SkipList`（`src/utils/skip_list.hpp:650`），而非 B 树 / 红黑树。节点 `SkipListNode`（`src/utils/skip_list.hpp:164`）用了 **C 风格的柔性数组成员** `std::atomic<SkipListNode *> nexts[0]`——数组成员长度为 0 时不占空间，实际内存按 `sizeof(node) + height × 8` 一次性分配，布局注释画在 `src/utils/skip_list.hpp:147` 附近。

层高分布用一个 O(1) 的二项分布技巧（`src/utils/skip_list.hpp:114`，即 `SkipListNode_base::gen_height`）：

```
gen_height():
    value = thread_local_mt19937()()                 # 每线程一个 mt19937，避免锁
    if value < 1 << (32 − kSkipListMaxHeight): return kSkipListMaxHeight
    value >>= (32 − kSkipListMaxHeight)              # 保留高 32 位
    return __builtin_ffs(value)                      # 找最低位 1 的位置 = 层高
```

`kSkipListMaxHeight = 32`（`src/utils/skip_list.hpp:62`）。源码注释指出用的是 ticki 那篇 "Skip Lists Done Right" 的 "O(1) level generation"，唯一区别是本实现正确处理了 0：`ffs(0)` 返回 0 是非法层高，所以要先做阈值判断再映射。特殊地，当随机数小于 `1 << (32−32) = 1`（即恰好取到 0）时直接顶格给最大层高。

查找是标准的自顶向下：`find_node`（`src/utils/skip_list.hpp:1316`）从第 31 层降到第 0 层，同时填 `preds` / `succs` 两个长度 32 的数组。期望查找/插入复杂度均为 `O(log N)`；层高期望 `E[h] = 2`，故空间期望 `O(N)`。

### 26.2 Label 索引的 MVCC：条目即版本

索引条目不是裸 `Vertex *`，而是一个二元组（`src/storage/v2/inmemory/label_index.hpp:37`）：

```
struct Entry { Vertex *vertex; uint64_t timestamp; }   // 比较键 = (vertex, timestamp)
```

排序键是 `std::tie(vertex, timestamp)`（`src/storage/v2/inmemory/label_index.hpp:41`）。这意味着：**同一个顶点可以在跳表里同时存在多条记录，分别对应不同提交时刻的标签身份**，读事务按自己的可见性时点挑选 timestamp 不大于自己的那条。跳表实例本身是 `utils::SkipListDb<Entry>`（`src/storage/v2/inmemory/label_index.hpp:56`），并被包在 `shared_ptr<IndividualIndex>` 里——这样 schema 变更（drop / 重建索引）时可以把整个索引对象原子换出，而不影响正在跑的事务所持有的旧快照。

每个 label 对应一个 `IndividualIndex`，全部装在 `std::map<LabelId, shared_ptr<IndividualIndex>>` 里。

### 26.3 ActiveIndices：写时复制的索引快照

所有索引被聚合成一个不可变的 `ActiveIndices` 快照（`src/storage/v2/indices/active_indices.hpp:45`），持有 12 个子索引快照指针：label / label-property / edge-type / edge-type-property / edge-property / vertex-property / text / text-edge / point / vector / vector-edge。

构造时用 `DMG_ASSERT` 断言**没有任何一个为空**（`src/storage/v2/indices/active_indices.hpp:67`），杜绝了半初始化快照。替换单个字段用指针成员模板：

```
template <auto Member, class X>
std::shared_ptr<ActiveIndices const> With(X x) const;   // src/storage/v2/indices/active_indices.hpp:78
```

注释说明这样设计的原因是把「字段与位置」的映射知识集中在一处，避开 10 参数构造函数的错位隐患。事务启动时取一次快照（`GetActiveIndices()`，`src/storage/v2/indices/vector_index.hpp:272`），之后整个事务看到的索引集合不变——这就是 DDL 与 DML 之间的隔离手段，比全局锁廉价得多。

### 26.4 向量索引：底层是 usearch，不是 hnswlib

Memgraph 3.x 的向量索引由第三方库 **usearch**（unum-cloud）承载，conan 锁定版本 2.21.4（`conan.lock:7`），通过 `find_package` 引入（`src/storage/v2/CMakeLists.txt:181`）。

具体类型（`src/storage/v2/indices/vector_index_utils.hpp:47`）：

```
mg_vector_index_t = unum::usearch::index_dense_gt<
    Vertex *,                      // key 直接就是顶点裸指针
    unum::usearch::uint40_t,       // 内部槽位 key 占 40 bit
    TrackedVectorAllocator<64>,    // 向量内存分配器（容量 64 的量化档）
    TrackedVectorAllocator<8>>     // 标量分配器
```

外层再包一层 `synchronized_mg_vector_index_t`，加一把 `utils::ResourceLock`（`src/storage/v2/indices/vector_index_utils.hpp:50`），因为 usearch 本身不是全并发安全的。

检索路径 `VectorIndex::SearchNodes`（`src/storage/v2/indices/vector_index.cpp:447`）持**读锁**后调用 `index.search(...)`，再把距离换算成相似度：

```
sim(d) = 1 − d                 对 ip / cos / pearson / hamming / tanimoto
                                 / sorensen / jaccard            （内积类）
sim(d) = 1 / (1 + d)           对 l2sq / haversine / divergence   （距离类）
```

换算函数 `SimilarityFromDistance`（`src/storage/v2/indices/vector_index_utils.hpp:270`）最终还会取 `abs` 并夹到 `(0,1)`。**这个区分很关键**：内积型度量下小 distance 表示「更相似」，所以用 `1−d`；而 L2 距离下 `d=0` 表示完全相同，`1−d` 会得到 1 但 `d` 大时会变负数，故必须用 `1/(1+d)` 这种单调有界的衰减。

写入侧 `UpdateVectorIndex`（`src/storage/v2/indices/vector_index_utils.hpp:409`）的典型路径是「存在则先 remove 再 add」；容量按 `resize_coefficient` 倍增后 `try_reserve`（`src/storage/v2/indices/vector_index_utils.hpp:456`）——注意 `try_reserve` **失败也不抛异常**，只是维持原容量继续 add，说明扩容是尽力而为而非硬保证。

索引规格 `VectorIndexSpec`（`src/storage/v2/indices/vector_index.hpp:121`）固化了：索引名、`label_filter`、`property`、`metric_kind`、`dimension`、`resize_coefficient`、`capacity`、`scalar_kind`。`scalar_kind` 支持 `b1x8 / u40 / uuid / bf16 / f64 / f32 / f16 / f8` 等多种量化档位，解析失败时抛 `VectorSearchException` 并在消息里列出全部支持项（`src/storage/v2/indices/vector_index_utils.hpp:259`）。

### 26.5 ON_DISK_TRANSACTIONAL：RocksDB 列族与带时间戳的比较器

落盘模式下不再有内存基表，`DiskStorage`（`src/storage/v2/disk/storage.hpp:39`）把 RocksDB 包在 `RocksDBStorage` 结构里（`src/storage/v2/disk/rocksdb_storage.hpp:34`）：

```
vertex_chandle       // 顶点
edge_chandle         // 边
out_edges_chandle    // 出边
in_edges_chandle     // 入边
default_chandle      // 默认族（元数据等）
```

注释坦承这个结构「从软件工程角度不理想，应更彻底地封装进 Storage 类」。它的作用也仅是持有 `rocksdb::Options` 与五个列族句柄。

版本可见性不靠 delta 链，而靠**比较器把提交时间戳编进 key**——`ComparatorWithU64TsImpl`（`src/storage/v2/disk/rocksdb_storage.hpp:60`）继承 `rocksdb::Comparator`，按 `(user_key, ts)` 排序。同理，加载历史数据时用的 `DELETE_DESERIALIZED_OBJECT` delta 会自己造时间戳：时间戳取自 RocksDB key 中携带的值，由 `CreateDeleteDeserializedObjectDelta` 解析（`src/storage/v2/mvcc.hpp:254`），而不是取当前事务的时间。

快照侧由 `CreateSnapshot`（`src/storage/v2/durability/snapshot.hpp:105`）与 `LoadSnapshot`（`src/storage/v2/durability/snapshot.hpp:83`）成对提供。

## 二十七、参考资料

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
