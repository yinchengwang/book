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

## 二十四、参考资料

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
