# JanusGraph Wiki（可插拔存储的分布式图数据库）

> 参考版本：JanusGraph 1.x（Java 实现，存储后端：Apache Cassandra / Apache HBase / ScyllaDB / Google Bigtable / Oracle BerkeleyDB；索引后端：Elasticsearch / Apache Solr / Apache Lucene）。本 wiki 重点覆盖图类型专项章节（适配器架构、CQL/Gremlin 双查询语言、Fauna 风格的 AdaptiveGraph）。

## 一、项目定位

JanusGraph 是 **Linux Foundation** 旗下的开源 **可插拔存储（Pluggable Backend）** 分布式图数据库，由 **Titan**（原 Aurelius 公司，2012 创立）的延续项目演化而来，提供 **适配器架构**（任意后端 KV + 任意全文索引）、**TinkerPop / Gremlin** 遍历语言、OLTP OLAP 双工，是 Hadoop / Cassandra 体系下"图数据库上承接 OLTP"的代表实现。

- **痛点**：
  - 单一图数据库（Neo4j、Memgraph）依赖专有存储，难以水平扩展至 PB 级；
  - 当组织已在 HBase / Cassandra / Scylla 上有数据沉淀时，希望**就近**部署图数据库；
  - OLTP 与 OLAP 工作负载共用同一后端（Spark + GraphX）；
  - 需要"社交图 / 知识图谱 / 推荐系统"等深度遍历能力，又能对接 ES / Solr 全文检索；
  - 老牌 Titan 用户需要平滑迁移；
- **典型用户**：Hadoop 生态企业、Cassandra / HBase 重度用户、知识图谱平台、社交网络、欺诈检测、推荐系统；
- **核心口号**：**"A scalable graph database optimized for storing and querying highly-connected models"** + **"pluggable storage backends + pluggable indexing"** + **"part of The Linux Foundation"**。

## 二、发展历史与社区

- **起源**：
  - 2012：ThinkAurelius 发布 **Titan 1.0**，基于 Cassandra + Elasticsearch；
  - 2015：Titan 1.0+ 商业公司被 DataStax 收购，DataStax 商业版（DataStax Enterprise Graph）继续支持；
  - 2016-Titan 1.x 维护停滞，DataStax 商业化同步开源切割；
  - 2017-01：The Linux Foundation 接收，孵化项目改名为 **JanusGraph**；
  - 2017：JanusGraph 0.2.0（首个稳定版本），加入 TinkerPop 3.x；
- **里程碑**：
  - 2017：JanusGraph 0.2 - 0.3 支持 HBase + Cassandra + BerkeleyDB + Bigtable；
  - 2018：JanusGraph 0.3.x 加入 Schema Maker、Index Maker、Spark Connector（graph-computer OLAP）；
  - 2019-2020：JanusGraph 0.5 - 0.6（进入 Apache 阶段管理）大规模性能优化；
  - 2020：JanusGraph 0.5.2 OLAP SparkOLAP 集成（"Graph Computer"）；
  - 2021-2023：JanusGraph 1.0 路线图推进；
  - 2024：1.0 GA、TinkerPop 3.6/3.7 完全适配、JanusGraph Server 重启；
  - 2025：1.1.x 优化 Geoshape、JanusGraph Bigtable GA；
- **维护状态**：活跃，每季度小版本，每月 commit 数十次；
- **社区**：GitHub 5k+ star、JanusGraph 邮件组、Slack、社区 PMC（PMC 含 IBM、Google、Huawei、Expero、Systap）；
- **生态**：TinkerPop/Apache、Apache Foundation 关系紧密（IP 守护者来自 Asf）、多个厂商（IBM Compose、Datastax、Huawei 等）发布商业支持。

## 三、许可与商业限制

- **核心代码**：**Apache License 2.0**；
- **存储后端**：
  - Apache Cassandra：**Apache 2.0**；
  - Apache HBase：**Apache 2.0**；
  - ScyllaDB：**AGPLv3**（仅驱动）→ JanusGraph 代码本身是 Apache；
  - Google Bigtable：商业闭源（Google 提供客户端）；
  - Oracle BerkeleyDB Java Edition（JE）：Sleepycat License（商业兼容）；
- **索引后端**：
  - Elasticsearch / OpenSearch：SSPL / Apache 2.0（取决于版本）；
  - Apache Solr：**Apache 2.0**；
  - Apache Lucene：**Apache 2.0**；
- **商业支持**：IBM Compose for JanusGraph（IBM 提供商业支持与扩展）；
- **变体**：
  - **IBM Compose for JanusGraph**：商业全托管（IBM Cloud）+ 企业级监控；
  - **Huawei GraphBase**：HBase 集成的企业级图数据库；
  - **Expero JanusGraph EE**：增强版（含 OLAP、监控）；
- **生态关系**：与 Apache TinkerPop、Apache Cassandra、Apache HBase、Elasticsearch、Spark GraphX、Apache Flink 紧密集成。

## 四、业界主要使用方与案例

- **大型用户**（公开）：
  - **Expero**：咨询与解决方案；
  - **Airbnb**：早期（DataStax Enterprise Graph）；
  - **Walmart**：商品图与推荐；
  - **AT&T**：网络拓扑与监控；
  - **Aeris**：IoT 设备管理；
  - **Hitachi Vantara**：数据建模；
  - **Bell Canada**：电信网络；
  - **Telenor**：电信；
  - 国内：华为、阿里、移动运营商、中信银行；
- **行业**：电信、网络运营商、Hadoop 生态、社交网络、知识图谱；
- **典型场景**：社交网络分析、反欺诈（实时）、推荐系统、知识图谱（实体关系）、网络与设备拓扑、运营商 CDR。

## 五、一句话对比定位

> JanusGraph 是 **"可插拔存储（任意 KV + 任意索引）+ OLTP + OLAP 双工 + TinkerPop / Gremlin + 无限扩展（依赖底层 KV）"**；与 **Neo4j（专有存储 + 原生图）**、**ArangoDB（多模型专有存储）**、**Nebula（原生分布式国产图）**、**Memgraph（兼容 Cypher + C++）**、**Dgraph（GraphQL 优先）**、**DataStax Enterprise Graph** 构成图数据库阵营。JanusGraph 的差异化是 **"广度接 Hadoop 生态 + 可插拔 + 工业实操"**。

## 六、整体架构

```
┌────────────────────────────────────────────────────────────────────┐
│                            客户端层                                  │
│  - Gremlin Console / Gremlin Server                                │
│  - Drivers: Java / Python / .NET / Node.js / Clojure                │
│  - TinkerPop 协议栈（HTTP/JSON 或 WebSocket）                       │
└─────────────────────┬──────────────────────────────────────────────┘
                      │
┌─────────────────────▼──────────────────────────────────────────────┐
│                  JanusGraph Server / Embedded                       │
│  janusgraph-server/                                                 │
│  ├─ JanusGraphServer         TinkerPop Server WAR（war 部署）      │
│  ├─ ServerOptions / JanusGraphChannelizer                         │
│  └─ Configuration（BerkeleyDB JE / Cassandra / HBase backend 选定）│
└─────────────────────┬──────────────────────────────────────────────┘
                      │
┌─────────────────────▼──────────────────────────────────────────────┐
│                    JanusGraph Core API                               │
│  janusgraph-core/src/main/java/org/janusgraph/core/                │
│  ├─ JanusGraphFactory            工厂打开/配置 graph              │
│  ├─ JanusGraph                   实例与事务                       │
│  ├─ JanusGraphTransaction        TX & 锁                          │
│  ├─ JanusGraphSchemaMaker        schema 元数据                    │
│  ├─ JanusGraphIndex              索引抽象（graph + vertex-centric）│
│  └─ Traversal / GremlinProcessor                                  │
└─────────────────────┬──────────────────────────────────────────────┘
                      │
┌─────────────────────▼──────────────────────────────────────────────┐
│                   优化器 / 查询重写                                   │
│  janusgraph-core/src/main/java/org/janusgraph/graphdb/             │
│  ├─ query/                     JanusGraphPredicate / Condition   │
│  ├─ query/lucene/              Lucene 倒排查询                    │
│  ├─ query/geoshape/            GeoSpatial                       │
│  ├─ query/graph/               Graph / vertex-centric 索引        │
│  ├─ query/cache/               结果缓存                           │
│  └─ olap/                      OLAP Spark OLAP（GraphComputer） │
└─────────────────────┬──────────────────────────────────────────────┘
                      │
┌─────────────────────▼──────────────────────────────────────────────┐
│                   后端适配器层（Storage Backend）                      │
│  janusgraph-core/src/main/java/org/janusgraph/diskstorage/         │
│  ├─ Backend          接口：get / put / delete / scanner           │
│  ├─ KeyColumnValueStore  NoSQL 接口抽象                           │
│  ├─ BaseBackend / BaseTransaction                                │
│  ├─ common / config 公共配置                                       │
│  ├─ cassandra/        Cassandra 适配                            │
│  ├─ hbase/            HBase 适配                                │
│  ├─ scylla/           ScyllaDB 适配                             │
│  ├─ bigtable/         Google Bigtable 适配                      │
│  ├─ berkeleydb/       BerkeleyDB JE 适配                        │
│  └─ inmemory/         单机测试用                                  │
└─────────────────────┬──────────────────────────────────────────────┘
                      │
┌─────────────────────▼──────────────────────────────────────────────┐
│                   索引后端（Index Backend）                            │
│  janusgraph-core/src/main/java/org/janusgraph/diskstorage/indexing/│
│  ├─ IndexProvider        接口                                     │
│  ├─ lucene/              Lucene（单机/嵌入式）                     │
│  ├─ es/                  Elasticsearch 适配                      │
│  ├─ solr/                Solr 适配                               │
│  ├─ cql/                 Cassandra CQL（部分索引）               │
│  └─ IndexUpdate / IndexQuery                                     │
└────────────────────────────────────────────────────────────────────┘
```

### 关键设计取舍

- **可插拔**：存储后端、索引后端均可替换；
- **TinkerPop**：图遍历语言 Gremlin（一种 Groovy DSL，可嵌入 Java / Python）作为标准接口；
- **OLTP + OLAP 双工**：OLAP 走 Spark GraphX（OLAP API GraphComputer），与 OLTP 共享 schema 抽象；
- **混合索引（Mixed Index）**：标量、地理、全文、组合索引都建在外部 ES/Solr/Lucene；
- **Vertex-centric Index**：每个 vertex label 内的"出边类型 + 排序"索引（如"好友中按年龄排序"）；
- **基于 schema**：写入前需要 schema（节点 label、边 label、属性键），schema 缓存于内存；
- **AdaptiveGraph（实验性）**：根据查询模式自动调优（schema migration）。

## 七、存储引擎

### 7.1 数据模型与存储格式

- **节点 + 边 + 属性**：Property Graph；
- 物理存储：后端 KV，每个 JanusGraph 元素（vertex、edge、property、schema）映射到 KV row；
- **Edge Cut**：边被切成"两个 directed edge"——一条出边（out-edge）、一条反边（in-edge） → 两份独立 KV row；
- **Properties 链**：每条边或节点的属性集合以"chain"方式存入 → 第 N 个属性 = `vertex(<id>) + (<key>, <id>) → value`；
- **Schema 一致性 hash**：所有元素 id 在 schema 内 + Cassandra partitioner 之间映射。

### 7.2 Cassandra Backend（最常用）

- **Keyspace** = 一个图（如 `janusgraph`）；
- **Tables**：`edgestore`、`graphindex`、`systemlog`、`systempropertieslock_` 等；
- **Replication Factor** 与一致性水平由 JanusGraph → Cassandra 配置映射；
- **CQL Token Partitioning** 与 hash 切片保持一致性；
- 与 C* 3.11+ 兼容性测试。

### 7.3 HBase Backend

- **表族**：类似 Cassandra，每类对象一表（如 `janusedgestore`）；
- **RegionServer** 切片；
- 与 HBase 2.x 兼容；
- 适合与 Hadoop 集成。

### 7.4 BerkeleyDB Backend

- 单机嵌入式（与图遍历应用同进程）；
- 适用于中小规模 + 桌面；
- 原生事务 + 高效；
- 学习与 CI 测试首选。

### 7.5 ScyllaDB / Bigtable

- ScyllaDB：类 Cassandra + C++ 实现（write 优化）；
- Google Bigtable：Google Cloud 服务，闭源服务端，需要 Google Cloud 凭证。

### 7.6 InMemory Backend

- `janusgraph-inmemory/` 子模块，仅用于测试与小型 demo；
- 启动即跑，无持久化。

### 7.7 事务与锁

- **Schema Transaction**：使用 **BerkeleyDB / Cassandra Paxos / Zookeeper** 做事务协调；
  - 默认 `LOCAL_LOCK_MECHANISM` = 行锁；
  - Schema 修改（加 label、加索引）走全局一致性协议；
- **Data Transaction**：依赖后端事务语义（如 C* LWT、Cassandra Paxos、HBase row lock）；
- **Multi-Version** + **SerializableIsolation**（默认）：后端未支持时回退到 Read-Committed。

### 7.8 WAL / Commit

- `systemlog` 列族存 WAL；
- 配置 `txLog` + `commit time` 与 `tx id`；
- **Time UUID**：每个 commit 一个 timeUUID 作为 read 锚点。

## 八、索引机制

### 8.1 图存储模型

JanusGraph 走"边切分（edge cut）+ 后端 KV"的路线，介于"原生图"与"关系型映射"之间：

| 维度 | JanusGraph | Neo4j（原生图） | ArangoDB（多模型） |
|------|------------|---------------|--------------------|
| 节点 | KV row | 定长 slot | Document |
| 边 | 双 directed edge row | 双向链表 | Edge Collection |
| 索引 | 外部 ES/Solr | 内置索引 | 内置索引 |
| 邻接遍历 | KV 双行扫 | 指针直连 | 双表 JOIN |

### 8.2 Graph Index（图级索引）

- **Composite Index**：复合索引 `(key1, key2, …) → vertexId`；
- **Mixed Index**：标量 + 地理 + 全文 + 数字 + 自定义 predicate；
  - 后端走 ES / Solr / Lucene；
  - 注册 `JanusGraphIndex`、`IndexField`；
  - 多字段可“加权组合”；

### 8.3 Vertex-Centric Index（每个节点内的边索引）

- 仅在一个 vertex 内做"按某属性找邻居"加速：
  ```
  mgmt.buildEdgeIndex(myEdgeLabel, "byWeight", Direction.OUT, Order.DESC, "weight")
  ```
- 在 edge 之外增加一组 KV 行：`（vertexId, edgeType, "byWeight", weight, otherVertexId）`；
- 查询：
  ```groovy
  g.V('name','alice').outE('knows').order().by('weight', desc)
  ```

### 8.4 全文检索（Mixed Index with Text mapping）

- 在 ES/Solr 上对 property 建文本映射；
- 配合 Geoshape mapping、数值 mapping 组成 MixedIndex；

### 8.5 地理空间

- `geo = Geo.search().within(Geometry.buffer(geoPoint, 0.5))`；
- 后端 ES 的 geo_point / geo_shape mapping；

### 8.6 向量索引

- 1.0 起实验性 HNSW Mixed-Index（外部插件）；
- 与 OLTP 整合仍处早期。

### 8.7 索引代价

- Mixed Index 越多，写放大越严重（每写一行 → 三向修改）；
- Vertex-centric index → KV 行数翻倍；
- 索引建立走 Bulk Processor 异步，不阻塞写入。

## 九、查询/计算/接口层

### 9.1 Gremlin 查询语言

Gremlin 是 Apache TinkerPop 标准的图遍历语言（Groovy DSL），可通过 Java/Python/.NET 驱动嵌入执行。

```groovy
// 1. 简单遍历
g.V().has('name', 'Alice').out('knows').values('name')

// 2. 多跳
g.V().has('name', 'Alice')
  .repeat(out('knows'))
  .until(has('name', 'Bob'))
  .path()

// 3. 最短路径
g.V().has('name','Alice').repeat(bothE().otherV().simplePath())
  .until(has('name','Bob').and().loops().is(0))
  .path().limit(1)

// 4. 子图模式
g.V().hasLabel('person').out('knows').in('knows')
  .where(neq('name'))
  .group().by('name').by(count())

// 5. PageRank（OLAP）
g.V().pageRank().by('rank').values('rank')

// 6. Cypher-style（部分客户端支持）— 用 GraphTraversal 难以表达；Gremlin 更通用

// 7. 索引搜索
g.V().has('name', textContains('vortex'))
```

### 9.2 OLAP：GraphComputer / Spark

- OLAP 通过 **Spark GraphComputer**（基于 Spark + GraphX）执行 BSP：
  - Spark context 内启动 worker 节点；
  - BSP superstep + barrier；
  - 持久化 schema：JanusGraph 与 Spark GraphX 双向读 vertex / edge；
- `JanusGraphSparkGraph` 类：
  ```
  SparkGraph sparkGraph = JanusGraphSparkGraph(graph)
  sparkGraph.pageRank().rounds(10).write().csv(...)
  ```
- **OLTP vs OLAP**：默认 OLTP（traversal / Gremlin）、OLAP（PageRank、社区检测）走 Spark。

### 9.3 接口与服务器

- **Embedded**：Java 代码同进程打开 `JanusGraphFactory.open(conf)`；
- **JanusGraph Server**：TinkerPop Server 包装（WAR + script-engine + channelizer），独立 JVM 暴露 HTTP/JSON 与 WebSocket；
- **Gephi / Cytoscape**：通过 TinkerPop 插件集成。

### 9.4 客户端驱动

- Java：原汁原味；
- Python：`gremlinpython`（Apache TinkerPop 官方）；
- Node.js：`gremlin-js`；
- .NET：`Gremlin.Net`；
- 跨语言基于 GraphSON 序列化协议。

### 9.5 表达式（Math / 聚合）

- Gremlin 内提供 `sum / count / mean / math`；
- JanusGraph 扩展一些时间 / 距离函数。

## 十、事务与一致性

### 10.1 事务分类

- **OLTP**：Cassandra LWT / HBase row lock + schema lock；
- **OLAP**：图算法任务，不在事务约束内；
- 同一实例同一时刻串行多个事务（schema lock 串行 schema 写）。

### 10.2 一致性配置

- 写入一致性 = `storage.backend = cassandra` + `storage.cassandra.write-consistency-level = QUORUM`；
- 读取一致性类似；
- 强一致 vs eventual 一致，依数据库需求调整；
- Schema 修改需要全局锁（`schema.default = None` 则关闭 lock）。

### 10.3 读后写（Read-after-write）

- 默认非严格会话一致，需在 Gremlin 加 `consistency()` 后置；
- 可用 `tx.commit()` 强制同步 + 读最新版本。

### 10.4 隔离

- `IsolationLevel` = `READ_UNCOMMITTED` / `READ_COMMITTED` / `REPEATABLE_READ` / `SERIALIZABLE`；
- 后端支持时尽量高，否则提示降级。

## 十一、复制与高可用

### 11.1 存储层复制（依赖后端）

- **Cassandra / Scylla** = 全分布式 + Replication Factor；
- **HBase** = RegionServer 集群 + HDFS 三副本；
- **Bigtable** = Google Cloud 内置；
- **BerkeleyDB** = 单机（高可用靠 OS 级复制）。

### 11.2 JanusGraph HA

- 节点本身无状态（数据在后端），多个 JanusGraph Server 进程互为热备；
- 多 server 节点都连同一 KV 集群，由客户端驱动做负载均衡；
- `JanusGraphFactory.Open(conf)` 在 zookeeper / etcd 注册 → 多 server 同步 + 客户端发现；

### 11.3 跨区域

- Cassandra DC-aware Rack；
- HBase 跨集群复制；
- Bigtable 跨区域复制；
- 大多数场景依靠底层 KV，JanusGraph 本身无内置跨区域复制。

## 十二、扩展性

- **水平扩展**：随 KV 后端线性扩展（Cassandra 节点数）；
- **垂直扩展**：每个 JanusGraph Server 是单进程，heap 大小可配（`storage.buffer-size`）；
- **数据分片**：自动（hash 切片），仅 OLAP 切 vertex cut；
- **冷热分层**：外部 ES/Solr 索引天然支持 tiering（hot/warm/cold）；
- **图切片限制**：跨 partition 遍历 hit 多 partition → 慢查询。

## 十三、性能与基准

- **LDBC SNB**：JanusGraph on Cassandra / HBase 在 SF=30 上数十~数百 QPS；与 Neo4j 比，明显慢（社区版 + Cassandra 行扫描）；
- **Graph500 / WikiData**：可扩展集群下数百亿边；
- **典型数字**：
  - 单实例 JanusGraph 点写 1w QPS；
  - 集群 OLTP（SF=30，60 节点）读 ~30k QPS；
  - OLAP：随 Spark Worker 线性扩展；
- **基准**：JanusGraph 官方 `tests/benchmark/` 提供 LDBC + 简单 micro-benchmark。

## 十四、运维与可观测性

- **指标**：内置 Metrics（codahale 或 Micrometer），Prometheus、JMX；
- **慢查询日志**：Gremlin Query 解析 → queryPlan → 执行时间；
- **GC** + Heap 调优：每 backend 不同；
- **备份**：依赖后端（Cassandra snapshot、HBase snapshot、BerkeleyDB 离线复制）；
- **迁移**：JanusGraphUtil / Schema → 数据迁移工具；
- **监控集成**：JMX → Prometheus（Micrometer）、Grafana 模板、Datadog；
- **Tracing**：OpenTelemetry 实验性集成（社区插件）。

## 十五、生态与集成

- **客户端**：
  - Java：原生；
  - Python：`gremlinpython`（Apache TinkerPop）；
  - JavaScript：`gremlin-js`；
  - .NET：`Gremlin.Net`；
  - Go：`gremlingo`（Apache TinkerPop official）；
- **可视化**：TinkerPop / Gephi / Cytoscape / Linkurious / DataStax Studio；
- **图算法**：Apache Spark GraphX / GraphFrames（OLAP）；
- **消息队列**：Kafka CDC（通过 `janusgraph-cdc`）+ Debezium；
- **容器化**：官方 Docker image + Helm chart（社区）；
- **AI / GenAI**：与 LangChain 集成（GraphDatabase retriever）+ Neo4j GenAI plugin 风格（思路参考）；
- **BI / 报表**：PowerBI、Tableau 适配器均通过 GraphQL / Gremlin HTTP 接口。

## 十六、核心论文与算法原理

- **TinkerPop 白皮书**：Pinkel et al., "The TinkerPop Graph Framework";
- **HBase 论文**："Bigtable: A Distributed Storage System for Structured Data"（Google 2006）+ HBase Architecture Docs;
- **Cassandra 论文**：Lakshman & Malik 2010 "Cassandra — A Decentralized Structured Storage System";
- **Spark GraphX**：Gonzalez et al., 2014 "GraphX: Graph Processing in a Distributed Dataflow Framework";
- **Gremlin 论文**：Rodriguez, "The Gremlin Graph Traversal Machine and Language"（ACM HPGC 2015）；
- **OLTP/OLAP 分工**：Vicknair et al., "RDF Query and Reasoning in BerkelyDB";
- **参考**：`docs/janusgraph-arch.png` 与 `docs/janusgraph-paper.pdf`（在 JanusGraph 官网）。

## 十七、源码学习路径

| 阶段 | 模块 | 路径 | 要点 |
|------|------|------|------|
| 入门 | JanusGraphFactory | `janusgraph-core/src/main/java/org/janusgraph/core/JanusGraphFactory.java` | 工厂模式入口 |
| 入门 | JanusGraph API | `JanusGraph.java` `JanusGraphTransaction.java` | 基础 API |
| 进阶 | Schema Maker | `JanusGraphSchemaMaker.java` + `SchemaContainer.java` | 模式管理 |
| 进阶 | Index Provider | `janusgraph-core/src/main/java/org/janusgraph/diskstorage/indexing/` | 索引抽象 |
| 进阶 | Cassandra Backend | `janusgraph-cassandra/src/main/java/org/janusgraph/diskstorage/cassandra/` | CQL 适配 |
| 高级 | Backend Transaction | `janusgraph-core/src/main/java/org/janusgraph/diskstorage/BackendTransaction.java` | 事务处理 |
| 高级 | OLAP GraphComputer | `janusgraph-core/src/main/java/org/janusgraph/graphdb/olap/` + `janusgraph-hadoop/` | Spark BSP |
| 高级 | OLTP Optimizer | `janusgraph-core/src/main/java/org/janusgraph/graphdb/query/lucene/` | query rewrite |

## 十八、源码规模与开发语言

- **LOC**：约 30 万行 Java；
- **语言占比**：
  - Java ≈ 95%；
  - Groovy（脚本示例 / Gremlin 解析）≈ 3%；
  - Maven/Shell/SQL ≈ 2%；
- **构建系统**：Apache Maven（多模块 `pom.xml` 顶 `janusgraph/` 含子模块：`janusgraph-core/-cassandra/-hbase/-scylla/-berkeleydb/-bigtable/-es/-solr/-lucene/-inmemory/-hadoop/-test/-server`）；
- **JDK**：JDK 11+（1.0）与 JDK 17+ 推荐；
- **TinkerPop**：内置 TinkerPop 版本；
- **CI**：GitHub Actions + Apache CI；
- **代码风格**：Apache Java 代码风格 + Checkstyle。

## 十九、学习资源

- **官方文档**：
  - `https://docs.janusgraph.org/` 主文档；
  - `https://docs.janusgraph.org/basics/` 基础；
  - `https://docs.janusgraph.org/advanced-topics/` 进阶；
  - `https://docs.janusgraph.org/configuration/` 配置；
  - `https://docs.janusgraph.org/recipes/` 示例；
- **书籍**：
  - "Practical Graph Analytics with Apache TinkerPop, Gremlin, and JanusGraph"（Apress）；
  - "Graph Databases for the Architect"（IBM Redbooks）；
  - "Apache TinkerPop 实战"；
- **视频 / 课程**：JanusGraph Summit 演讲归档、IBM Compose 培训视频、YouTube TinkerPop 官方、JanusGraph 工程师 talk；
- **社区**：JanusGraph Mailing List（Google Groups）、Slack `#janusgraph`、JanusGraph GitHub Discussions、Stack Overflow `[janusgraph]`；
- **Demo 项目**：JanusGraph Examples (`reference/graph/janusgraph/janusgraph-examples/`) + `reference/graph/janusgraph/README.md`；
- **在线 sandbox**：IBM Compose 免费沙盒（受限于试用）。

## 二十、可移植性与部署

- **支持平台**：Linux（x86_64 / ARM64）、macOS（x86_64 / Apple Silicon）、Windows（WSL 推荐）+ Docker；
- **容器化**：官方 Docker `janusgraph/janusgraph:latest`、`janusgraph/janusgraph-full`（含 ES）；
- **Kubernetes**：社区 Helm chart + 自定义配置；
- **嵌入式部署**：`JanusGraphFactory.open("berkeleyje:...")` 即嵌入式持久化（适合中小规模）；
- **CI 推荐**：Cassandra / HBase docker-compose + JanusGraph image。

## 二十一、变体与衍生项目

- **IBM Compose for JanusGraph**：商业全托管（IBM Cloud）+ 企业级监控 + SLA；
- **Huawei GraphBase**：HBase 集成的企业级图数据库（国产化）；
- **Expero JanusGraph EE**：增强版（包含 OLAP、监控、专家支持）；
- **Aurelius Titan（已废弃）**：原 fork 来源，2017 停止维护；
- **DataStax Enterprise Graph**：DataStax 商业版（DSE 6.x 包含），后续部分功能兼容 JanusGraph；
- **历史版本链**：Titan 1.x → JanusGraph 0.2.x → 0.3 → 0.5 → 0.6 → 1.0 GA。

## 二十二、风险与陷阱

### 22.1 性能陷阱

- **跨 partition 遍历**：依赖 Cassandra / HBase 的 hash 切片，跨节点遍历慢；
- **大规模 OLTP 时延**：Cassandra LWT 模式性能下降明显；
- **图查询不等值**：无充分索引 → 全 KV scan → 频繁调优 schema。

### 22.2 Schema 锁

- Schema 修改需要全局锁；
- Schema 锁竞争激烈时阻塞 OLTP 写入；
- Schema Maker 建议灰度部署。

### 22.3 Mixed Index 写放大

- 每个属性索引 ES/Solr → 写三向放大；
- 高频写场景需关掉 irrelevant 索引；
- **ES 写入 QPS** 限制：ES 默认 refresh interval 1s → 大批量频繁 rebuild。

### 22.4 分布式死锁

- Cassandra LWT / HBase 行锁在并发高时容易超时；
- 需合理估算事务超时 + 重试策略；
- 切 back-end 时性能差异大（建议预先 benchmark）。

### 22.5 一致性 vs 可用性

- 默认 eventual consistency；
- 强一致应用需 `QUORUM` 一致性 + LWT 写；
- 弱一致应用早期版本无 replica 保护，客户端需手动处理 stale read。

### 22.6 OLAP / OLTP 干扰

- OLAP Spark Job 产生大量 scanner → 干扰 OLTP 时延；
- 建议隔离 OLAP 集群 / 时间窗口。

### 22.7 升级路线

- Cassandra / HBase / ES / Solr 主版本升级可能 breaking；
- JanusGraph 主版本变更需要 review Storage Adapter 行为变化；
- 建议跑 `janusgraph-inmemory-test` 验证。

### 22.8 索引维护

- 重建 Mixed Index 时阻塞读取；
- 重建 Vertex-Centric Index 阻塞对应边类型遍历；
- 大量 Vertex-Index 重建需 batch 处理。

## 二十三、与本项目的关联

- **可插拔后端架构**：本项目若希望"任意 KV + 任意索引"的设计，可参考 JanusGraph 的 `Backend` 接口；
- **Gremlin 图遍历**：本项目可借鉴其遍历 API 设计（即便本项目最终走 SQL/Cypher 路线）；
- **Mixed Index 概念**：本项目若实现"标量 + 全文 + 向量"组合索引，可学习 ES/Solr mixed 模式；
- **OLAP/OLTP 分离架构**：本项目可参考 JanusGraph 在 Spark 上跑 OLAP 的设计；
- **schema Maker / Index Maker DSL**：本项目若走 schema-first 模式可借鉴；
- **不直接关联**：本项目 `engineering/src/db/` 暂无图引擎，但多模态 wiki 直接对齐 JanusGraph 已能落地于未来扩展。

## 二十四、后端存储抽象：五个 Store 与遍历短路点

### 24.1 真实的接口继承链（Titan 时代命名已废弃）

JanusGraph 1.2.0 的后端抽象是一条**很薄**的接口链，比 Titan 0.5 时代简单得多：

```
StoreManager                        janusgraph-core/src/main/java/org/janusgraph/diskstorage/keycolumnvalue/StoreManager.java
  └─ KeyValueStoreManager           janusgraph-core/src/main/java/org/janusgraph/diskstorage/keycolumnvalue/keyvalue/KeyValueStoreManager.java:25
       └─ OrderedKeyValueStoreManager  janusgraph-core/src/main/java/org/janusgraph/diskstorage/keycolumnvalue/keyvalue/OrderedKeyValueStoreManager.java:27

KeyValueStore                       janusgraph-core/src/main/java/org/janusgraph/diskstorage/keycolumnvalue/keyvalue/KeyValueStore.java:27
  └─ OrderedKeyValueStore           janusgraph-core/src/main/java/org/janusgraph/diskstorage/keycolumnvalue/keyvalue/OrderedKeyValueStore.java:30
```

`KeyValueStore` 全部只有 5 个方法：`delete`（`janusgraph-core/src/main/java/org/janusgraph/diskstorage/keycolumnvalue/keyvalue/KeyValueStore.java:36`）、`get`（`janusgraph-core/src/main/java/org/janusgraph/diskstorage/keycolumnvalue/keyvalue/KeyValueStore.java:46`）、`containsKey`（`janusgraph-core/src/main/java/org/janusgraph/diskstorage/keycolumnvalue/keyvalue/KeyValueStore.java:56`）、`acquireLock`（`janusgraph-core/src/main/java/org/janusgraph/diskstorage/keycolumnvalue/keyvalue/KeyValueStore.java:67`）、`close`。**没有 put / 没有 scanner** —— 写入与范围扫描都在子类 `OrderedKeyValueStore` 上：`insert`（`janusgraph-core/src/main/java/org/janusgraph/diskstorage/keycolumnvalue/keyvalue/OrderedKeyValueStore.java:41`）、`getSlice`（`janusgraph-core/src/main/java/org/janusgraph/diskstorage/keycolumnvalue/keyvalue/OrderedKeyValueStore.java:55`）、`getSlices`（`janusgraph-core/src/main/java/org/janusgraph/diskstorage/keycolumnvalue/keyvalue/OrderedKeyValueStore.java:71`）。

范围查询的三元组 `[start, end)` + `Predicate<StaticBuffer> keyFilter` + `limit` 封装在 `KVQuery`（`janusgraph-core/src/main/java/org/janusgraph/diskstorage/keycolumnvalue/keyvalue/KVQuery.java:28`），`getKeySelector()` 把它压成「过滤谓词 + 剩余条数」的可执行游标控制器（`janusgraph-core/src/main/java/org/janusgraph/diskstorage/keycolumnvalue/keyvalue/KVQuery.java:57`）。

⚠️ **重要命名纠正**：`KeyColumnValueStore`（`janusgraph-core/src/main/java/org/janusgraph/diskstorage/keycolumnvalue/KeyColumnValueStore.java`）在 1.2.0 里是与 `KeyValueStore` **平级的遗留接口**，不是它的父类；Titan 时代的 `KeyColumnarBackend` / `KeyRangeBackend` / `KeySlicedBackend` 在本仓库**全仓零命中**。同理 `ConfiguredBackend` 也已被 `BackendTransaction` + `BaseTransactionConfig` 取代（零命中）。

### 24.2 五个固定 Store

`Backend`（`janusgraph-core/src/main/java/org/janusgraph/diskstorage/Backend.java:133`）启动时固定打开这几类 store，名字**永不可改**（注释见 `janusgraph-core/src/main/java/org/janusgraph/diskstorage/Backend.java:137`）：

| Store | 常量与位置 | 职责 |
|-------|-----------|------|
| `edgestore` | `janusgraph-core/src/main/java/org/janusgraph/diskstorage/Backend.java:148` | 全部边 + 属性（按 vertex id 分行） |
| `graphindex` | `janusgraph-core/src/main/java/org/janusgraph/diskstorage/Backend.java:149` | 图内倒排索引：属性值 → vertex |
| `txlog` | `janusgraph-core/src/main/java/org/janusgraph/diskstorage/Backend.java:158` | 事务 WAL |
| `systemlog` | `janusgraph-core/src/main/java/org/janusgraph/diskstorage/Backend.java:159` | schema 变更日志 |
| `<store>_lock_` | 后缀常量 `janusgraph-core/src/main/java/org/janusgraph/diskstorage/Backend.java:156` | 分布式锁（独立 store） |

内存缓存按固定比例切：`EDGESTORE_CACHE_PERCENT = 0.8` / `INDEXSTORE_CACHE_PERCENT = 0.2`（`janusgraph-core/src/main/java/org/janusgraph/diskstorage/Backend.java:161`）。热数据天然集中在 `edgestore`，因为邻接遍历是热点。

### 24.3 遍历短路点：BackendTransaction

`BackendTransaction`（`janusgraph-core/src/main/java/org/janusgraph/diskstorage/BackendTransaction.java:62`）把所有读操作收敛成 6 个入口，每一个都对应一次图遍历的**短路点**：

| 入口 | 位置 | 语义 |
|------|------|------|
| `edgeStoreQuery(KeySliceQuery)` | `janusgraph-core/src/main/java/org/janusgraph/diskstorage/BackendTransaction.java:287` | 已知 vertexId，取某一 relation 类别下的 slice —— 邻接遍历主路径 |
| `edgeStoreMultiQuery(List<key>, SliceQuery)` | `janusgraph-core/src/main/java/org/janusgraph/diskstorage/BackendTransaction.java:302` | N 个 vertex 同构 slice 批取 |
| `edgeStoreKeys(SliceQuery)` | `janusgraph-core/src/main/java/org/janusgraph/diskstorage/BackendTransaction.java:394` | 全局 key 扫描（`g.V()` / OLAP） |
| `indexQuery(KeySliceQuery)` | `janusgraph-core/src/main/java/org/janusgraph/diskstorage/BackendTransaction.java:434` | 图内索引查询 |
| `indexQuery(String, IndexQuery)` / `rawQuery` / `totals` | `janusgraph-core/src/main/java/org/janusgraph/diskstorage/BackendTransaction.java:451` / `janusgraph-core/src/main/java/org/janusgraph/diskstorage/BackendTransaction.java:482` / `janusgraph-core/src/main/java/org/janusgraph/diskstorage/BackendTransaction.java:517` | 外部索引后端（ES / Solr / Lucene） |

全局扫描的键边界是硬编码的 8 字节全零 / 全一（`EDGESTORE_MIN_KEY` / `EDGESTORE_MAX_KEY`，`janusgraph-core/src/main/java/org/janusgraph/diskstorage/BackendTransaction.java:70`）—— 因为 JanusGraph 约定 vertexId 是 64 位。

**能力降级分支**：若后端不支持 `hasMultiQuery()`，`edgeStoreMultiQuery`（`janusgraph-core/src/main/java/org/janusgraph/diskstorage/BackendTransaction.java:303` 处判断）退化为 `multiThreadedEdgeStoreMultiQuery`（`janusgraph-core/src/main/java/org/janusgraph/diskstorage/BackendTransaction.java:355`）—— 用 `CountDownLatch` 给每个 key 起一个线程。但源码注释明确写着：即使退化成多线程，也**优先**把一批 key 直接发给支持 multiQuery 的后端，因为分布式 KV（Cassandra / HBase）能做分组异步下发，比「一 key 一线程」更省连接（注释见 `janusgraph-core/src/main/java/org/janusgraph/diskstorage/BackendTransaction.java:350`）。低于 `MIN_TASKS_TO_PARALLELIZE = 2`（`janusgraph-core/src/main/java/org/janusgraph/diskstorage/BackendTransaction.java:67`，判断在 `janusgraph-core/src/main/java/org/janusgraph/diskstorage/BackendTransaction.java:357`）时线程池开销大于收益，直接串行。

### 24.4 读写分离的提交与重试

提交是**两阶段**的：`commitStorage()` 只提交主存储（`janusgraph-core/src/main/java/org/janusgraph/diskstorage/BackendTransaction.java:148`），`commitIndexes()` 逐个提交外部索引并**收集异常而不是首个即抛**（`janusgraph-core/src/main/java/org/janusgraph/diskstorage/BackendTransaction.java:152`）—— 这样一个索引挂掉不会连累其他索引的提交，调用方拿到 `Map<String,Throwable>` 自行决定降级。

重试在 `BackendOperation`（`janusgraph-core/src/main/java/org/janusgraph/diskstorage/util/BackendOperation.java:35`）：

```
executeDirect(exe, totalWaitTime):                      # :59
    maxTime  = now + totalWaitTime
    waitTime = perturb(50ms)                           # BASE_REATTEMPT_TIME :41
    loop:
        try: return exe.call()
        catch e:
            找到最内层 BackendException                 # :71-73
            若它是 TemporaryBackendException -> 记录待重试  # :74
            否则若是 BackendException -> 立即抛出        # :76-77
            否则 -> 包成 PermanentBackendException 抛出  # :79
        if now + waitTime < maxTime: sleep(waitTime)   # :84-87
        else: break
        waitTime = perturb(waitTime * 2)               # :96
    throw TemporaryBackendException(超时)               # :98
```

抖动扰动 ±20%（`PERTURBATION_PERCENTAGE`，`janusgraph-core/src/main/java/org/janusgraph/diskstorage/util/BackendOperation.java:42`，扰动函数 `janusgraph-core/src/main/java/org/janusgraph/diskstorage/util/BackendOperation.java:44`）是为了打散多客户端同时重试造成的惊群。所有读路径都经过它（`executeRead`，`janusgraph-core/src/main/java/org/janusgraph/diskstorage/BackendTransaction.java:523`），并在那里把线程中断转成 TinkerPop 的 `TraversalInterruptedException`（`janusgraph-core/src/main/java/org/janusgraph/diskstorage/BackendTransaction.java:529`）。

**异常契约三态**：`TemporaryBackendException` = 可重试（超时 / 限流 / 节点不可用）；`PermanentBackendException` = 不可重试（表不存在 / 序列化失败）；`ResourceUnavailableException` = 后端明确表示资源打满。三态划分让上面的退避循环能精确区分「再试一次」和「立刻放弃」。

### 24.5 后端能力矩阵 StoreFeatures

`StoreFeatures`（`janusgraph-core/src/main/java/org/janusgraph/diskstorage/keycolumnvalue/StoreFeatures.java:31`）用 8 个布尔位描述后端能力：`hasScan`（`janusgraph-core/src/main/java/org/janusgraph/diskstorage/keycolumnvalue/StoreFeatures.java:37`）、`hasUnorderedScan`（`janusgraph-core/src/main/java/org/janusgraph/diskstorage/keycolumnvalue/StoreFeatures.java:43`）、`hasConsistentScan`（`janusgraph-core/src/main/java/org/janusgraph/diskstorage/keycolumnvalue/StoreFeatures.java:51`）、`hasOrderedScan`（`janusgraph-core/src/main/java/org/janusgraph/diskstorage/keycolumnvalue/StoreFeatures.java:57`）、`hasMultiQuery`（`janusgraph-core/src/main/java/org/janusgraph/diskstorage/keycolumnvalue/StoreFeatures.java:64`）、`hasLocking`（`janusgraph-core/src/main/java/org/janusgraph/diskstorage/keycolumnvalue/StoreFeatures.java:71`）、`hasBatchMutation`（`janusgraph-core/src/main/java/org/janusgraph/diskstorage/keycolumnvalue/StoreFeatures.java:78`）、`isKeyOrdered`（`janusgraph-core/src/main/java/org/janusgraph/diskstorage/keycolumnvalue/StoreFeatures.java:85`）。`StandardStoreFeatures`（`janusgraph-core/src/main/java/org/janusgraph/diskstorage/keycolumnvalue/StandardStoreFeatures.java:26`）是不可变的 Builder 实现。

两个后端的声明对比：

| 能力位 | BerkeleyJE | HBase |
|--------|-----------|-------|
| 声明位置 | `janusgraph-berkeleyje/src/main/java/org/janusgraph/diskstorage/berkeleyje/BerkeleyJEStoreManager.java:126` | `janusgraph-hbase/src/main/java/org/janusgraph/diskstorage/hbase/HBaseStoreManager.java:380` |
| `orderedScan` / `keyOrdered` | true / true | true / true |
| `multiQuery` | **未声明（false）** | true |
| `unorderedScan` | false | true |
| `locking` | true | 未声明（false） |
| `batchMutation` | 未声明（false） | true |
| `distributed` | 未声明（false） | true |
| `optimisticLocking` | 显式 false（`janusgraph-berkeleyje/src/main/java/org/janusgraph/diskstorage/berkeleyje/BerkeleyJEStoreManager.java:138`） | true |

能力位直接决定执行路径：

- **BerkeleyJE 的 slice 走 Cursor 两段式**：首次 `Get.SEARCH_GTE` 定位到 `start`（`janusgraph-berkeleyje/src/main/java/org/janusgraph/diskstorage/berkeleyje/BerkeleyJEKeyValueStore.java:185`），之后每步 `Get.NEXT` 顺序迭代（`janusgraph-berkeleyje/src/main/java/org/janusgraph/diskstorage/berkeleyje/BerkeleyJEKeyValueStore.java:187`），并受 `selector.reachedLimit()` 约束（`janusgraph-berkeleyje/src/main/java/org/janusgraph/diskstorage/berkeleyje/BerkeleyJEKeyValueStore.java:183`）；单点查则是 `Get.SEARCH`（`janusgraph-berkeleyje/src/main/java/org/janusgraph/diskstorage/berkeleyje/BerkeleyJEKeyValueStore.java:123`）。
- **HBase 的 slice 全部下推到 RegionServer**：构造 `ColumnRangeFilter` 做列区间裁剪（`janusgraph-hbase/src/main/java/org/janusgraph/diskstorage/hbase/HBaseKeyColumnValueStore.java:159`），有 limit 时再叠一层 `ColumnPaginationFilter`（`janusgraph-hbase/src/main/java/org/janusgraph/diskstorage/hbase/HBaseKeyColumnValueStore.java:168`）—— limit 因此是**服务端**限制而非客户端截断，避免拉回海量数据。
- **`acquireLock` 两端都不可用**：BerkeleyJE 直接是 no-op，依赖 JE 自身的 MVCC 隔离（`janusgraph-berkeleyje/src/main/java/org/janusgraph/diskstorage/berkeleyje/BerkeleyJEKeyValueStore.java:141`）；HBase 直接抛 `UnsupportedOperationException`（`janusgraph-hbase/src/main/java/org/janusgraph/diskstorage/hbase/HBaseKeyColumnValueStore.java:129`）。真正的分布式锁由 `consistentkey` locker 在独立的 `_lock_` store 上实现（注册见 `janusgraph-core/src/main/java/org/janusgraph/diskstorage/Backend.java:197`）。

### 24.6 复杂度

| 操作 | 后端复杂度 | 上层语义 |
|------|-----------|---------|
| `get(单 key)` | O(1) | 按 id 取节点 |
| `getSlice(KVQuery)` | O(log N + K)（B 树定位 + K 条结果） | 邻接遍历 / vertex-centric 索引 |
| `getSlices(N key)` | JE：O(N log N + K)；HBase：O(命中 Region 数 + K) | multiQuery 合批 |
| `getKeys(SliceQuery)` | O(N) | 全图扫描（OLAP） |

其中 `K` 是本次扫描命中的记录数。`limit` 通过 `KVQuery` 构造器传给父类 `BaseQuery`（`janusgraph-core/src/main/java/org/janusgraph/diskstorage/keycolumnvalue/keyvalue/KVQuery.java:43`）能把 O(N) 压成 O(log N + limit) —— 这是 `g.V().limit(10)` 不扫全图的原因。

## 二十五、键编码与 ID 分配：3 位关系类型前缀与 Base36 元素 ID

### 25.1 关系类型前缀：3 bit 划分

`IDHandler.DirectionID` 只有 3 个取值，且**故意跳过了 `01b`**：

```
PROPERTY_DIR(0)   // 00b  属性
EDGE_OUT_DIR(2)   // 10b  出边
EDGE_IN_DIR(3)    // 11b  入边
```

（`janusgraph-core/src/main/java/org/janusgraph/graphdb/database/idhandling/IDHandler.java:51`，枚举项在 `janusgraph-core/src/main/java/org/janusgraph/graphdb/database/idhandling/IDHandler.java:53`）

列（column）部分的整体位布局（源码注释，`janusgraph-core/src/main/java/org/janusgraph/graphdb/database/idhandling/IDHandler.java:122`）：

```
[ Invisible & System (2 bit) | Relation-Type-ID (1 bit) | Relation-Type-Count (变长) | Direction-ID (1 bit) ]
```

前缀的计算（`janusgraph-core/src/main/java/org/janusgraph/graphdb/database/idhandling/IDHandler.java:95`）：

```
prefix = ((systemType ? 0 : invisible ? 2 : 1) << 1) + getRelationType()
其中 getRelationType()  = id >>> 1        # :64  0=属性, 1=边
     getDirectionInt()   = id & 1         # :68  0=OUT, 1=IN
     PREFIX_BIT_LEN      = 3               # :114
```

**这是 JanusGraph 遍历短路点的物理基础**。`getBounds(RelationCategory, boolean)`（`janusgraph-core/src/main/java/org/janusgraph/graphdb/database/idhandling/IDHandler.java:185`）把「只要出边」「只要属性」这类语义编译成一段**连续字节区间**：

```
getBounds(type, systemTypes):
    PROPERTY: start = end = prefix(PROPERTY_DIR, systemTypes, systemTypes)
    EDGE:     start = end = prefix(EDGE_OUT_DIR,  systemTypes, systemTypes)
    RELATION: start = prefix(PROPERTY_DIR, ...)
              end   = prefix(EDGE_OUT_DIR, ...)
    return [ byte(start << 5), byte((end + 1) << 5) ]     # getPrefixed :178，前缀左移到字节高 3 位
```

因为 `PROPERTY_DIR(0) < EDGE_OUT_DIR(2) < EDGE_IN_DIR(3)`，所以 `[PROPERTY, EDGE_OUT]` 恰好覆盖「属性 ∪ 出边」的连续前缀，`EDGE_IN` 天然被排除在外。于是一次范围扫描在**存储层**就不可能读到入边行 —— 不需要任何额外过滤。这正是 §7.1 说的「Edge Cut」在字节层面如何变成「只扫一半」。

### 25.2 变长整数 VariableLong

前缀占高 3 位，剩下的位要装 relation type 的计数。`VariableLong.writePositiveWithPrefix`（`janusgraph-core/src/main/java/org/janusgraph/graphdb/database/idhandling/VariableLong.java:160`）把前缀写进首字节高位，count 用块状变长编码跟在后面；`positiveWithPrefixLength`（`janusgraph-core/src/main/java/org/janusgraph/graphdb/database/idhandling/VariableLong.java:188`）**先算长度再分配**，供 `getRelationType` 预分配缓冲（`janusgraph-core/src/main/java/org/janusgraph/graphdb/database/idhandling/IDHandler.java:138`）。

解码侧 `readRelationType`（`janusgraph-core/src/main/java/org/janusgraph/graphdb/database/idhandling/IDHandler.java:143`）：

```
[c0, c1] = readPositiveWithPrefix(in, 3)          # :144  {count, prefix}
dirID        = DirectionID.getDirectionID(c1 & 1, c0 & 1)   # :145
typeId       = c0 >>> 1                                     # :146
isSystemType = (c1 >> 1) == 0                               # :147
typeId = IDManager.getSchemaId(isSystemType ? SystemPropertyKey : UserPropertyKey, typeId)
```

代价：绝大多数关系类型（count 小）整个 column **1 字节**搞定；count 每翻一倍多占 1 字节。写入放大几乎为零。

顶点 id 则支持两种编码并存：long 用 `VariableLong.writePositive`（`janusgraph-core/src/main/java/org/janusgraph/graphdb/database/idhandling/IDHandler.java:213`），字符串用 `VariableString.write`（`janusgraph-core/src/main/java/org/janusgraph/graphdb/database/idhandling/IDHandler.java:211`）；解码时靠 `STRING_ID_MARKER = -128`（`janusgraph-core/src/main/java/org/janusgraph/graphdb/database/idhandling/IDHandler.java:49`，最高位为 1，而正 long 的最高位恒为 0）判别类型（`janusgraph-core/src/main/java/org/janusgraph/graphdb/database/idhandling/IDHandler.java:226`）。

### 25.3 VertexIDType：用位后缀而非前缀字段打标签

`IDManager.VertexIDType`（`janusgraph-core/src/main/java/org/janusgraph/graphdb/idmanagement/IDManager.java:63`）不靠固定字段，而是让不同类型占用 id 的**不同位宽后缀**。源码注释里那张完整掩码表在 `janusgraph-core/src/main/java/org/janusgraph/graphdb/idmanagement/IDManager.java:36`：

```
        0 - + User created Vertex
      000 -     * Normal vertices
      010 -     * Partitioned vertices
      100 -     * Unmodifiable (e.g. TTL'ed) vertices
      110 -     + Reserved for additional vertex type
        1 - + Invisible
       01 -     + Schema related vertices
      101 -         + Schema Type vertices
   0101 -             + Relation Type vertices
  00101 -                 + Property Key
 000101 -                     * User Property Key
 100101 -                     * System Property Key
   10101 -                 + Edge Label
 010101 -                     * User Edge Label
 110101 -                     * System Edge Label
    1101 -             Other Type vertices
   01101 -                 * Vertex Label
```

每个枚举项的 `offset()` 就是它的位宽：`NormalVertex` = 3（`janusgraph-core/src/main/java/org/janusgraph/graphdb/idmanagement/IDManager.java:83`）、`PartitionedVertex` = 3（`janusgraph-core/src/main/java/org/janusgraph/graphdb/idmanagement/IDManager.java:99`）、`UnmodifiableVertex` = 3（`janusgraph-core/src/main/java/org/janusgraph/graphdb/idmanagement/IDManager.java:115`）。全局参数：`TYPE_LEN_RESERVE = 3`（`janusgraph-core/src/main/java/org/janusgraph/graphdb/idmanagement/IDManager.java:380`）、`MAX_PARTITION_BITS = 16`（`janusgraph-core/src/main/java/org/janusgraph/graphdb/idmanagement/IDManager.java:392`）。

**分区号内嵌在 id 高位**。`getPartitionId`（`janusgraph-core/src/main/java/org/janusgraph/graphdb/idmanagement/IDManager.java:481`）：

```
partition = (vertexId >>> USERVERTEX_PADDING_BITWIDTH) & (partitionIDBound - 1)
```

`USERVERTEX_PADDING_BITWIDTH = NormalVertex.offset()`（`janusgraph-core/src/main/java/org/janusgraph/graphdb/idmanagement/IDManager.java:400`）。这意味着**一次点查不需要查任何映射表**就能算出该打到哪个分区 —— 分布式 KV 的路由决策是 O(1) 且零 I/O 的。

`getKey(Object vertexId)`（`janusgraph-core/src/main/java/org/janusgraph/graphdb/idmanagement/IDManager.java:489`）三条分支：schema 节点直接 8 字节 long buffer；long 用户节点重组为 `(partition << partitionOffset) | type.addPadding(count)`；String 节点走 `BufferUtil.getStringIdBuffer`（`janusgraph-core/src/main/java/org/janusgraph/graphdb/idmanagement/IDManager.java:505`）。

### 25.4 「长度即类型」的反向约定

`getKeyID(StaticBuffer)`（`janusgraph-core/src/main/java/org/janusgraph/graphdb/idmanagement/IDManager.java:534`）用了一个巧妙的不变量：

```
if b.length() != 8:  return VariableString.read(b)    # :535-537 长度非 8 ⇒ 字符串 id
value = b.getLong(0)                                   # :538     长度恰为 8 ⇒ long id
```

源码注释（`janusgraph-core/src/main/java/org/janusgraph/graphdb/idmanagement/IDManager.java:512`）解释了为什么这里能这么判：因为整个 buffer 就是 id 本身，长度必然精确；而 §25.2 里 column 内的 id 只是 buffer 的一部分，长度不定，那里就只能靠 `STRING_ID_MARKER` 位。还特别注明 long 型 id 的首字节最高位**不保证为 0**，所以位标记方案在这里行不通（`janusgraph-core/src/main/java/org/janusgraph/graphdb/idmanagement/IDManager.java:523`）。为了防止歧义，marker + 长度恰好等于 8 时会补一个哑字节（注释见 `janusgraph-core/src/main/java/org/janusgraph/graphdb/idmanagement/IDManager.java:520`）。

### 25.5 PartitionedVertex 的规范分区：XOR 折叠

Partitioned Vertex 要在每个分区各存一份副本，但**必须算出不同的分区号**。`getPartitionHashForId`（`janusgraph-core/src/main/java/org/janusgraph/graphdb/idmanagement/IDManager.java:566`）用逐块 XOR 折叠：

```
result = 0; offset = 0
while offset < 64:
    result ^= (id >>> offset) & (partitionIDBound - 1)   # :572
    offset += partitionBits                              # :573
return result
```

把 64 位按 `partitionBits` 切成 ⌈64 / partitionBits⌉ 块，每块取低 partitionBits 位做 XOR，结果落在 `[0, partitionIDBound)`。XOR 的好处是无进位、各块互不干扰 —— 同样 count 在不同副本上只要起始 offset 不同就能得到不同分区。`getCanonicalVertexIdFromCount`（`janusgraph-core/src/main/java/org/janusgraph/graphdb/idmanagement/IDManager.java:579`）用它构造规范副本，`isCanonicalVertexId`（`janusgraph-core/src/main/java/org/janusgraph/graphdb/idmanagement/IDManager.java:590`）用「算出来 == 原值」判定当前副本是不是主副本，读取时据此去重。

复杂度 O(64 / partitionBits) 次整数运算。

### 25.6 元素 ID 的字符串编码：Base36 + 可配分隔符

`RelationIdentifier`（`janusgraph-driver/src/main/java/org/janusgraph/graphdb/relations/RelationIdentifier.java:31`）既是指向 relation 的引用，也是外部索引后端的**文档主键**，还是 TinkerPop `element.id` 的载体。

`toString()`（`janusgraph-driver/src/main/java/org/janusgraph/graphdb/relations/RelationIdentifier.java:127`）格式：

```
<LongEncoding.encode(relationId)>-<outVertexId>-<LongEncoding.encode(typeId)>[-<inVertexId>]
```

- **分隔符默认 `-`**（`janusgraph-driver/src/main/java/org/janusgraph/graphdb/relations/RelationIdentifier.java:58`），可用系统属性或环境变量 `JANUSGRAPH_RELATION_DELIMITER` 覆盖（常量在 `janusgraph-driver/src/main/java/org/janusgraph/graphdb/relations/RelationIdentifier.java:34`，校验逻辑要求单个 ASCII 可打印字符，见 `janusgraph-driver/src/main/java/org/janusgraph/graphdb/relations/RelationIdentifier.java:48`）
- String 型端点 id 前置 `STRING_ENCODING_MARKER`（`janusgraph-driver/src/main/java/org/janusgraph/graphdb/relations/RelationIdentifier.java:134`）
- 属性 relation 没有 `inVertexId`，故长度为 3；边 relation 长度为 4。`parse`（`janusgraph-driver/src/main/java/org/janusgraph/graphdb/relations/RelationIdentifier.java:149`）按 3/4 两态还原

⚠️ **命名纠正**：本仓库用的是**单个 `-` 分隔符**（默认），不是 `#` / `:` 双分隔符 —— 后者是 Titan 0.x 与早期 JanusGraph 的旧格式。

`LongEncoding`（`janusgraph-driver/src/main/java/org/janusgraph/util/encoding/LongEncoding.java:26`）是标准 base36：

```
BASE_SYMBOLS = "0123456789abcdefghijklmnopqrstuvwxyz"    # :28   B = 36

encode(num):                      # :54
    while num != 0:
        sb.append(symbols[num % B]); num /= B
    return reverse(sb)

decode(s):                        # :42
    num = 0
    for ch in s: num = num * B + symbols.indexOf(ch)
    return num
```

`STRING_ENCODING_MARKER = 'S'`（`janusgraph-driver/src/main/java/org/janusgraph/util/encoding/LongEncoding.java:32`）**故意不在符号表内**，所以解码时看到首字符是 `S` 就知道这不是 base36 数字（源码注释见 `janusgraph-driver/src/main/java/org/janusgraph/util/encoding/LongEncoding.java:30`）。

长度：64 位正整数 → `⌈64 / log₂36⌉ = ⌈64 / 5.1699⌉ = 13` 字符。属性 relation 的 id 形如 13 + 1 + 13 + 1 + 13 = 41 字符。

**哈希只看 (relationId, typeId)**：`hashCode` 返回 `Long.hashCode(relationId)`（`janusgraph-driver/src/main/java/org/janusgraph/graphdb/relations/RelationIdentifier.java:115`），`equals` 也只比这两个字段（`janusgraph-driver/src/main/java/org/janusgraph/graphdb/relations/RelationIdentifier.java:123`）—— 因为 relationId 全局唯一，端点 id 是冗余的，这让 relation 可以被 O(1) 去重而不必解字符串。

顺带：TinkerPop 的内部键 `~adjacent`（§26.5 谓词下推要用）由 `Token.makeSystemName`（`janusgraph-core/src/main/java/org/janusgraph/graphdb/internal/Token.java:51`）→ `Graph.Hidden.hide(name)` 生成。

## 二十六、索引策略与查询重写：从自动配置策略到显式声明

### 26.1 事实纠正：自动配置索引策略已被整体移除

JanusGraph 0.x 定义了 `GraphIndexConfigurationStrategy` 接口与 `default` / `base` / `classic` / `simple` 四个内置实现，在 `buildIndex()` 时**隐式推断**该建哪些键索引、用哪种类型、是否唯一。本仓库（`pom.xml` 版本 `1.2.0-SNAPSHOT`）中实测：

- `GraphIndexConfigurationStrategy`、`JanusGraphDefaultIndexStrategy`、`JanusGraphBaseIndexStrategy`、`JanusGraphClassicIndexStrategy`、`JanusGraphSimpleIndexStrategy` —— **全仓零命中**
- `index-strategy` 配置项 —— `janusgraph-core/src/main/java` 与 `docs/` 均**零命中**

1.x 已改为**纯 schema-first**，索引必须显式声明：

```groovy
mgmt.buildIndex('byName', Vertex.class)                              // JanusGraphManagement.java:183
    .addKey(nameKey)                                                  // 无参重载
    .addKey(ageKey, Parameter.of('mapping', Mapping.TEXT))            // 带 Parameter 重载
    .buildUnique()          // 或 buildComposite() / buildRelation()
```

`IndexBuilder` 接口定义在 `janusgraph-core/src/main/java/org/janusgraph/core/schema/JanusGraphManagement.java:193`，两个 `addKey` 重载分别在 `janusgraph-core/src/main/java/org/janusgraph/core/schema/JanusGraphManagement.java:201` 与 `janusgraph-core/src/main/java/org/janusgraph/core/schema/JanusGraphManagement.java:217`，实现类为 `ManagementSystem.IndexBuilder`（`janusgraph-core/src/main/java/org/janusgraph/graphdb/database/management/ManagementSystem.java:866`）。

**实际影响**：0.x 的经典姿势「`propertyKey('name').cardinality(SINGLE).indexed()` + `automatic()` 自动建索引」在 1.x 里不再自动发生 —— 必须显式 `buildIndex().addKey()`。反过来好处是索引行为完全可预测，没有隐式兜底。

### 26.2 索引后端只剩三种

`StandardIndexProvider`（`janusgraph-core/src/main/java/org/janusgraph/diskstorage/StandardIndexProvider.java:29`）：

| 枚举 | shorthand | 实现类 |
|------|-----------|--------|
| `LUCENE` | `lucene` | `org.janusgraph.diskstorage.lucene.LuceneIndex` |
| `ELASTICSEARCH` | `elasticsearch` / `es` | `org.janusgraph.diskstorage.es.ElasticSearchIndex` |
| `SOLR` | `solr` | `org.janusgraph.diskstorage.solr.SolrIndex` |

（分别见 `janusgraph-core/src/main/java/org/janusgraph/diskstorage/StandardIndexProvider.java:30`、`janusgraph-core/src/main/java/org/janusgraph/diskstorage/StandardIndexProvider.java:31`、`janusgraph-core/src/main/java/org/janusgraph/diskstorage/StandardIndexProvider.java:32`）

⚠️ **`hadoop` 索引后端（`HadoopIndex`）在 1.2.0 已删除**（全仓零命中）。`janusgraph-hadoop/` 模块现在只剩 OLAP Spark 作业配置（`org.janusgraph.hadoop.config.*`），不再提供索引 provider。

⚠️ 同理，**存储后端注册表只剩 5 个**：`StandardStoreManager`（`janusgraph-core/src/main/java/org/janusgraph/diskstorage/StandardStoreManager.java:28`）枚举项为 `BDB_JE` / `CQL` / `HBASE` / `IN_MEMORY` / `SCYLLA`（`janusgraph-core/src/main/java/org/janusgraph/diskstorage/StandardStoreManager.java:29` 至 `janusgraph-core/src/main/java/org/janusgraph/diskstorage/StandardStoreManager.java:33`）。**Google Bigtable 已不在标准存储后端注册表中**（`janusgraph-bigtable/` 目录仍在，但未注册为 `StandardStoreManager`）。

### 26.3 索引字段的 Mapping 与双字段策略

索引字段的数据类型来自 `PropertyKey.dataType()` + 显式 `Mapping` 参数。`Mapping` 枚举（`janusgraph-core/src/main/java/org/janusgraph/core/schema/Mapping.java:30`）共 6 档：`DEFAULT` / `TEXT` / `STRING` / `TEXTSTRING` / `PREFIX_TREE` / `BKD`（`janusgraph-core/src/main/java/org/janusgraph/core/schema/Mapping.java:32` 至 `janusgraph-core/src/main/java/org/janusgraph/core/schema/Mapping.java:40`），解析入口 `getMapping`（`janusgraph-core/src/main/java/org/janusgraph/core/schema/Mapping.java:52`）从 `KeyInformation.getParameters()` 里取，缺省为 `DEFAULT`。

| Mapping | 索引形态 | 能力 | 代价 |
|---------|---------|------|------|
| `TEXT` | 分词后建倒排 | 支持 `textContains` / 分词匹配 | **不能**精确匹配整个字符串，不能高效排序 |
| `STRING` | 整串不分词 | 精确等值 + 可排序 | 不支持全文 |
| `TEXTSTRING` | **双字段** | 两者兼得 | 索引体积 ≈ 两者之和 |
| `PREFIX_TREE` | 前缀树 | 地理范围 | ES 7 弃用 / ES 8 移除 |
| `BKD` | BKD 树 | 地理范围（ES 默认） | 仅 ES |

ES 侧的地理策略迁移写在源码注释里：`PREFIX_TREE` 在 ElasticSearch 7 被弃用、8 被移除，ES 改用 `BKD`（`janusgraph-core/src/main/java/org/janusgraph/core/schema/Mapping.java:37`）；`PREFIX_TREE` 在 Solr 8 与 Lucene 8 仍可用（`janusgraph-core/src/main/java/org/janusgraph/core/schema/Mapping.java:39`）。

**双字段机制**（`TEXTSTRING`）在 `LuceneIndex` 里落地：

```
getDualFieldName(fieldKey, ki):                       # LuceneIndex.java:1150
    if ki.getDataType() 是 String 且 Mapping == TEXTSTRING:
        return fieldKey + "_____s"                    # STRING_SUFFIX :145
    return empty

getDualMapping(ki):                                   # LuceneIndex.java:1157
    满足上条件时返回 Mapping.STRING，否则 DEFAULT
```

写入时对同一条 `IndexRecordEntry` **连续 add 两个 field**（`janusgraph-lucene/src/main/java/org/janusgraph/diskstorage/lucene/LuceneIndex.java:413`）：主 field 用原 mapping，影子 field 用 dual mapping（`janusgraph-lucene/src/main/java/org/janusgraph/diskstorage/lucene/LuceneIndex.java:415`）。查询时按 predicate 自动选分支（`janusgraph-lucene/src/main/java/org/janusgraph/diskstorage/lucene/LuceneIndex.java:790`），并在分词类查询时避免误用 STRING 字段（`janusgraph-lucene/src/main/java/org/janusgraph/diskstorage/lucene/LuceneIndex.java:794`）。排序路径同样走 dual field 名（`janusgraph-lucene/src/main/java/org/janusgraph/diskstorage/lucene/LuceneIndex.java:580`）。

复杂度：设文档数 N、平均字段值长度 L，`TEXTSTRING` 的索引体积约 `Θ(N · L · 2)`，倒排链合并常数翻倍；而精确匹配查询的代价与 `STRING` 相同。这是用**写放大 + 存放大**买查询灵活性。

### 26.4 索引文档的字段布局

`LuceneIndex` 文档主键是 §25.6 的 relation 字符串，字段名带固定前缀以避免与用户属性冲突：

| 常量 | 值 | 用途 | 位置 |
|------|-----|------|------|
| `DOCID` | `_____elementid` | 文档主键，存 `RelationIdentifier.toString()` | `janusgraph-lucene/src/main/java/org/janusgraph/diskstorage/lucene/LuceneIndex.java:144` |
| `STRING_SUFFIX` | `_____s` | TEXTSTRING 的影子字段后缀 | `janusgraph-lucene/src/main/java/org/janusgraph/diskstorage/lucene/LuceneIndex.java:145` |
| `GEOID` | `_____geo` | 几何字段 | `janusgraph-lucene/src/main/java/org/janusgraph/diskstorage/lucene/LuceneIndex.java:146` |

`DOCID` 用 `new StringField(DOCID, docID, Field.Store.YES)` 写入（`janusgraph-lucene/src/main/java/org/janusgraph/diskstorage/lucene/LuceneIndex.java:389`）—— **StringField 不分词且 Store.YES**，因为查询结果必须回读原始 relation 串才能定位 KV 行；用 TextField 会被分词器切碎导致回读失败。查询时只 `FIELDS_TO_LOAD = {DOCID}` 一个字段（`janusgraph-lucene/src/main/java/org/janusgraph/diskstorage/lucene/LuceneIndex.java:147`），避免把全字段拉回内存（取值见 `janusgraph-lucene/src/main/java/org/janusgraph/diskstorage/lucene/LuceneIndex.java:618`）。

### 26.5 IndexRecords：一条索引记录的形状

`IndexRecords extends ArrayList<IndexRecordEntry[]>`（`janusgraph-core/src/main/java/org/janusgraph/graphdb/database/index/IndexRecords.java:24`）。`add` 时做**防御性深拷贝一层数组**（`janusgraph-core/src/main/java/org/janusgraph/graphdb/database/index/IndexRecords.java:27`）—— 因为 CDC 变更流会把同一条记录数组分发给多个下游处理器（写索引、触发器等），共享引用会导致某个处理器就地改写污染其他处理器。`getRecordValues()`（`janusgraph-core/src/main/java/org/janusgraph/graphdb/database/index/IndexRecords.java:31`）把每条的字段值数组摊平成 `Object[]`，交给外部索引后端批量写。

### 26.6 查询重写：谓词下推与合批

`JanusGraphLocalQueryOptimizerStrategy`（`janusgraph-core/src/main/java/org/janusgraph/graphdb/tinkerpop/optimize/strategy/JanusGraphLocalQueryOptimizerStrategy.java:53`）是本地查询优化的总入口，`apply`（`janusgraph-core/src/main/java/org/janusgraph/graphdb/tinkerpop/optimize/strategy/JanusGraphLocalQueryOptimizerStrategy.java:63`）依次做五件事：

```
applyJanusGraphVertexSteps(traversal)                             # :99   VertexStep → JanusGraphVertexStep，折叠 HasStep，合并 limit
applyJanusGraphPropertiesSteps(traversal, cacheSize, mode)         # :118  Properties/PropertyMap/ElementMap → 属性预取
applyJanusGraphLabelSteps(traversal, mode)                         # label() 下推
applyJanusGraphDropSteps(traversal, mode)                          # drop() 下推
inspectLocalTraversals(...)                                        # 多查询合批（JanusGraphMultiQueryStrategy）
```

其中 `applyJanusGraphVertexSteps`（`janusgraph-core/src/main/java/org/janusgraph/graphdb/tinkerpop/optimize/strategy/JanusGraphLocalQueryOptimizerStrategy.java:99`）把标准 `VertexStep` 换成 `JanusGraphVertexStep`（`janusgraph-core/src/main/java/org/janusgraph/graphdb/tinkerpop/optimize/strategy/JanusGraphLocalQueryOptimizerStrategy.java:101`），并在返回边时把后续 `HasStep` 折进边标签过滤（`janusgraph-core/src/main/java/org/janusgraph/graphdb/tinkerpop/optimize/strategy/JanusGraphLocalQueryOptimizerStrategy.java:105`）—— 源码注释明确说 order/range 不能折因为它们不是 local 的（`janusgraph-core/src/main/java/org/janusgraph/graphdb/tinkerpop/optimize/strategy/JanusGraphLocalQueryOptimizerStrategy.java:106`）；紧随的 `RangeGlobalStep` 会把 limit 下推成 `setLimit`（`janusgraph-core/src/main/java/org/janusgraph/graphdb/tinkerpop/optimize/strategy/JanusGraphLocalQueryOptimizerStrategy.java:113`）。

**谓词下推**是其中最有价值的一条规则。`AdjacentVertexOptimizerStrategy`（`janusgraph-core/src/main/java/org/janusgraph/graphdb/tinkerpop/optimize/strategy/AdjacentVertexOptimizerStrategy.java:41`）识别三种位置：

```
NONE
V2V_ID       // VertexStep(返回 vertex) → HasStep(T.id == v)
V2E_E2V_ID   // VertexStep(返回 edge) → EdgeVertexStep/EdgeOtherVertexStep → HasStep(T.id == v)
```

`getOptimizablePosition`（`janusgraph-core/src/main/java/org/janusgraph/graphdb/tinkerpop/optimize/strategy/AdjacentVertexOptimizerStrategy.java:64`）向前回溯两步，跳过中间的 `NoOpBarrierStep`，逐步校验前驱步的类型。

改写规则（`replaceSequenceV2VthenID`，`janusgraph-core/src/main/java/org/janusgraph/graphdb/tinkerpop/optimize/strategy/AdjacentVertexOptimizerStrategy.java:139`）：

```
g.V(x).out('knows').has(T.id, y)
        ↓  改写为
g.V(x).outE('knows').has('~adjacent', y).inV()
```

先把「返回顶点」的 `VertexStep` 拆成一个返回边的 `VertexStep`（`janusgraph-core/src/main/java/org/janusgraph/graphdb/tinkerpop/optimize/strategy/AdjacentVertexOptimizerStrategy.java:151`）加一个反向的 `EdgeVertexStep`（`janusgraph-core/src/main/java/org/janusgraph/graphdb/tinkerpop/optimize/strategy/AdjacentVertexOptimizerStrategy.java:158`，`BOTH` 方向则用 `EdgeOtherVertexStep`，见 `janusgraph-core/src/main/java/org/janusgraph/graphdb/tinkerpop/optimize/strategy/AdjacentVertexOptimizerStrategy.java:156`），再在中点插入 `has('~adjacent', y)`（`janusgraph-core/src/main/java/org/janusgraph/graphdb/tinkerpop/optimize/strategy/AdjacentVertexOptimizerStrategy.java:173`）。已存在 `V2E_E2V_ID` 形态时走更轻量的 `replaceSequenceV2EthenE2VthenID`（`janusgraph-core/src/main/java/org/janusgraph/graphdb/tinkerpop/optimize/strategy/AdjacentVertexOptimizerStrategy.java:110`）。

`~adjacent` 是内置隐式属性 `ImplicitKey.ADJACENT_ID`（`janusgraph-core/src/main/java/org/janusgraph/graphdb/types/system/ImplicitKey.java:57`，`Token.makeSystemName("adjacent")` → `~adjacent`），类型 `Long.class`。合法性由 `AdjacentVertexHasIdOptimizerStrategy.isValidStep`（`janusgraph-core/src/main/java/org/janusgraph/graphdb/tinkerpop/optimize/strategy/AdjacentVertexHasIdOptimizerStrategy.java:70`）把关：必须恰好 1 个 `HasContainer`、key 是 `T.id`、predicate 是 `Compare.eq`、值是 `Vertex` 或 `Long`（`janusgraph-core/src/main/java/org/janusgraph/graphdb/tinkerpop/optimize/strategy/AdjacentVertexHasIdOptimizerStrategy.java:76` 至 `janusgraph-core/src/main/java/org/janusgraph/graphdb/tinkerpop/optimize/strategy/AdjacentVertexHasIdOptimizerStrategy.java:81`）。

**为什么值得**：设顶点 x 的出度为 D。原始计划要物化全部 D 条边（D 次 KV slice 读），再逐条比对端点 id；改写后在**边层**用隐式属性过滤，未命中的边不会被取出，期望代价 O(1)。对「粉丝数十万」的节点是数量级差异。多条件版本 `AdjacentVertexHasUniquePropertyOptimizerStrategy` / `AdjacentVertexIsOptimizerStrategy` 同理，把 `has('uniqueProp', v)` 也下推到边层。

改写结果最终汇聚到 `JanusGraphStep`（`janusgraph-core/src/main/java/org/janusgraph/graphdb/tinkerpop/optimize/step/JanusGraphStep.java:62`）：`buildGlobalGraphCentricQuery`（`janusgraph-core/src/main/java/org/janusgraph/graphdb/tinkerpop/optimize/step/JanusGraphStep.java:116`）汇总所有 `hasContainer` 交给查询规划层（单条约束由 `addConstraint` 追加，`janusgraph-core/src/main/java/org/janusgraph/graphdb/tinkerpop/optimize/step/JanusGraphStep.java:166`）。`toString`（`janusgraph-core/src/main/java/org/janusgraph/graphdb/tinkerpop/optimize/step/JanusGraphStep.java:213`）会把最终查询形状打印出来，含 `Or(...)` 形式的复合分支（`janusgraph-core/src/main/java/org/janusgraph/graphdb/tinkerpop/optimize/step/JanusGraphStep.java:236`）—— 调优时可直接对照 Gremlin Server 日志确认改写是否生效。

## 二十七、参考资料

1. JanusGraph 官方文档 — `https://docs.janusgraph.org/`。
2. JanusGraph 源码 — `https://github.com/JanusGraph/janusgraph`。
3. Apache TinkerPop 文档 — `https://tinkerpop.apache.org/docs/current/`。
4. Apache Cassandra 文档 — `https://cassandra.apache.org/doc/latest/`。
5. Apache HBase 文档 — `https://hbase.apache.org/`。
6. Elasticsearch 文档 — `https://www.elastic.co/guide/`。
7. Gremlin 语言手册 — `http://tinkerpop.apache.org/docs/current/recipes/`。
8. IBM Compose for JanusGraph — `https://www.ibm.com/cloud/compose/janusgraph`。
9. "The TinkerPop Graph Framework" — Rodriguez & Taube（HPGC 2015）。
10. Spark GraphX 论文 — Gonzalez et al.（OSDI 2014）。
