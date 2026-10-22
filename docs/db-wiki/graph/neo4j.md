# Neo4j Wiki（原生图数据库的开源标杆）

> 参考版本：Neo4j 5.x 社区版（Java + Scala，JDK 17+，Cypher 5 语言规范，Apache Arrow Flight + Bolt 5 协议）。本 wiki 重点覆盖图类型专项章节（原生图存储、Cypher 查询语言、图遍历算法、向量索引）。

## 一、项目定位

Neo4j 是由 **Neo4j Inc.**（瑞典创业，现总部位于美国旧金山）开源的 **原生图数据库（Native Graph Database）**，以 **"the graph database leader"** 为定位，提供 **节点-关系-属性（Node-Relationship-Property）** 的 Property Graph 数据模型、**Cypher** 类 SQL 的声明式图查询语言、**Bolt 二进制协议 + Browser 可视化 + Desktop 桌面 IDE** 完整工具链，是 OLTP 图数据库的事实标准。

- **痛点**：
  - 关系型数据库在多层 JOIN（如社交网络六度关系）下性能指数级下降；
  - 传统图实现（如 JanusGraph 等外挂索引）依赖外部存储，难以保证图遍历（graph traversal）的恒定时间；
  - 缺乏声明式图查询语言，需要开发者手写 BFS/DFS；
  - 知识图谱 / 欺诈检测 / 推荐系统等场景需要"任意深度"的关系查询；
  - 现代 RAG / Agent 应用需要图 + 向量混合检索（Neo4j 5.x 内置向量索引）；
- **典型用户**：社交网络、欺诈检测、推荐系统、知识图谱、网络安全、IT 运维（CMDB）、药物研发、地理空间、电信网络；
- **核心口号**：**"The world's leading graph database"** + **"Make sense of connections"** + **"Cypher: the SQL for graphs"**。

## 二、发展历史与社区

- **起源**：2000 年代初由 Emil Eifrem（创始人、CEO） 等在瑞典创立，2007 年 Neo4j 1.0 发布，2010 年开源（GPLv3）；
- **里程碑**：
  - 2007：Neo4j 1.0，首个生产级图数据库；
  - 2010：开源（GPLv3）；
  - 2012：Cypher 1.0 发布；
  - 2015：Neo4j 2.3 引入"复合类型 + 标签 + 索引"，奠定现代数据模型；
  - 2016：Neo4j 3.0，Cypher 升级为独立规范；
  - 2018：Causal Clustering（Raft 实现）+ 多数据库；
  - 2019：Cypher 10 标准工作启动（OGC / openCypher）；
  - 2020：Neo4j 4.0，多数据库、Fabric 联邦、ACID 多图；
  - 2022：Neo4j 5.0，**向量索引**（Vector Index）、**全文索引**（Full-text）、**Cypher 25**、**复合日志结构**（Composite Logging）；
  - 2023-2024：Neo4j 5.10+，GenAI 集成（LLM 嵌入直接写入）、Arrow Flight 加速 ETL；
  - 2025-2026：Neo4j 5.20+，分布式 AuraDS 与自管的 Aura Enterprise 协同、AI Workflow / Copilot；
- **维护状态**：活跃，每周 30+ commit；
- **社区**：GitHub 13k+ star、Neo4j 社区版（GPLv3）、Neo4j Online Community、Neo4j 认证工程师（Neo4j Certified Professional）、GraphSummit 全球大会、Discord/Slack；
- **生态**：超过 100 个 Neo4j 官方/社区集成、官方与 LangChain / LlamaIndex / Haystack 提供 GraphRAG / Vector 集成。

## 三、许可与商业限制

- **核心代码**：**GPLv3**（社区版）+ **AGPLv3**（自托管远程使用）；
- **商业版本**：
  - **Neo4j Enterprise**：闭源商业版，含 Causal Clustering（多主）、在线备份、RBAC、LDAP、SAML、审计、监控（Prometheus）、Fabric 多图联邦；
  - **Neo4j Aura**：
    - **AuraDB Free**：永久免费云实例（一个数据库 + 50 万节点 + 325 万关系）；
    - **AuraDB Professional / Enterprise / Virtual Dedicated Cloud**：商业托管；
    - **AuraDS**（Data Science）：全文与图算法工作台（Causal Clustering 兼容）；
- **生态关系**：
  - 与 **Graph Data Science Library（GDS）** 紧密集成（图算法库）；
  - 与 **Apache Arrow Flight**、**Apache Spark**、**Apache Kafka**（CDC、Change Data Capture）双向同步；
  - 与 **LangChain / LlamaIndex** 提供 GraphQA / Vector 检索集成；
  - 与 **GenAI 插件（OpenAI / Anthropic / Vertex / Bedrock）** 内置 LLM 工作流。

## 四、业界主要使用方与案例

- **大型用户**（公开）：
  - **Walmart / eBay**：商品推荐与欺诈检测；
  - **Airbnb**：房源关联推荐；
  - **Adobe**：营销知识图谱；
  - **NASA / Lockheed Martin**：航天器工程元数据；
  - **BMW / Volkswagen**：智能制造与供应链；
  - **Cisco / Verizon**：网络拓扑；
  - **CERN**：粒子物理实验元数据；
  - **Swiss Life / UBS / Deutsche Bank**：金融反欺诈与 KYC；
  - **Merck**：药物研发知识图谱；
  - 国内：腾讯、阿里、字节、平安、招行、华为等；
- **行业**：金融反欺诈、推荐系统、知识图谱、医疗、网络安全、IT 运维、社交网络；
- **典型场景**：欺诈环检测（`MATCH (a)-[:TRANSFER]->(b)-[:TRANSFER]->(a)`）、推荐（朋友的朋友、协同过滤）、知识图谱（实体-关系-属性）、IT 拓扑（CMDB）、药物相互作用（Drug-Drug Interaction）、GraphRAG（实体关系抽取后存图）。

## 五、一句话对比定位

> Neo4j 是 **"原生图数据库（Property Graph）+ Cypher 查询语言 + Bolt 协议 + 节点指针直连 + 向量索引（5.x）+ GDS 图算法库 + Fabric 多图联邦 + 完整工具链（Browser/Desktop/Aura）"**；与 **JanusGraph（外接存储）**、**Nebula（国产分布式）**、**Memgraph（兼容 Cypher + C++）**、**ArangoDB（多模型）**、**Kuzu（嵌入式）** 构成图数据库阵营。Neo4j 的差异化是 **"最早商业化 + 工具链完整 + 厂商主导 + AI 集成"**。

## 六、整体架构

```
┌──────────────────────────────────────────────────────────────────┐
│                            客户端层                                │
│  - Cypher Shell / Neo4j Browser / Neo4j Desktop                  │
│  - Drivers: bolt://  → Java / Python / JavaScript / Go / .NET   │
│  - HTTP API: REST 端点（已弃用，建议 Bolt）                       │
│  - Apache Arrow Flight: 高吞吐 ETL/分析                          │
└─────────────────────┬────────────────────────────────────────────┘
                      │ Bolt (TCP, 自描述二进制协议)
┌─────────────────────▼────────────────────────────────────────────┐
│                       服务器层（Neo4j Server）                      │
│  community/server/                                                │
│  ├─ EnterpriseEntryPoint（EE）/ CommunityEntryPoint（CE）       │
│  ├─ DBMS 生命周期管理：org.neo4j.dbms                            │
│  ├─ DiscoveryService（集群发现）                                   │
│  └─ ConfigLoader / CertManager                                  │
└─────────────────────┬────────────────────────────────────────────┘
                      │
┌─────────────────────▼────────────────────────────────────────────┐
│                       数据库内核（Kernel）                          │
│  community/kernel/src/main/java/org/neo4j/kernel/                │
│  ├─ GraphDatabaseAPI（顶层 API）                                  │
│  ├─ TransactionalKernel / StatementProcessor                     │
│  ├─ DatabaseManager / GraphDatabaseQueryService                  │
│  ├─ LockManager / DeadlockDetector（Reentrant locking）          │
│  ├─ AvailabilityGuard / HealthCheck                              │
│  └─ Recovery / Backup / Diagnostics                              │
└─────────────────────┬────────────────────────────────────────────┘
                      │
┌─────────────────────▼────────────────────────────────────────────┐
│                       Cypher 执行层                                │
│  community/cypher/                                                │
│  ├─ cypher-planner                Logical + Physical plan         │
│  ├─ cypher-runtime                Volcano / slotted / pipelined  │
│  ├─ cypher-compiler               cartesian / parallel runtime   │
│  ├─ cypher-ast                    抽象语法树、语义分析               │
│  ├─ cypher-feather               矢量执行（实验性）               │
│  ├─ cypher-shell                  REPL                             │
│  └─ cypher-planner-spi            适配点                           │
└─────────────────────┬────────────────────────────────────────────┘
                      │
┌─────────────────────▼────────────────────────────────────────────┐
│                       存储引擎（Store / StorageEngine）             │
│  community/kernel/impl/store/                                     │
│  ├─ RecordStorageEngine（Neostore，节点/关系记录）                │
│  ├─ ReadStore / WriteStore                                       │
│  ├─ PropertyStore                                                │
│  ├─ TokenStore（labels / relTypes / propKeys）                   │
│  ├─ SchemaIndexProvider（社区 JBump、Caffeine Cache）             │
│  ├─ SchemaRule（索引/约束规则）                                     │
│  └─ BatchedTransactionAppender/WAL Appender                       │
└─────────────────────┬────────────────────────────────────────────┘
                      │
┌─────────────────────▼────────────────────────────────────────────┐
│                       索引层                                       │
│  community/index/                                                 │
│  ├─ lucene-index             全文 + 向量（5.x 一体索引）           │
│  ├─ native-index             B+Tree + GB+Tree                     │
│  │   └─ GBPTree: Generalized B+ Tree（社区）                     │
│  └─ vector-index             HNSW（Lucene HNSW 适配或自定义）      │
└─────────────────────┬────────────────────────────────────────────┘
                      │
┌─────────────────────▼────────────────────────────────────────────┐
│                       图存储格式（Native Graph Storage）            │
│  community/kernel/impl/stereotype/                                │
│  ├─ NodeStore：固定大小节点记录（id + labelIds + nextRelId +      │
│  │            nextPropId），形成定长 slot                         │
│  ├─ RelationshipStore：关系为"双向链表"，                       │
│  │            firstNode, secondNode, type, prev/next              │
│  │            双向指针 → "节点指针直连"的核心                    │
│  ├─ PropertyStore：动态属性 KV 链                                │
│  └─ 组合方式：节点像"心轴"，关系是双向指针的"辐条"               │
└─────────────────────┬────────────────────────────────────────────┘
                      │
┌─────────────────────▼────────────────────────────────────────────┐
│                       磁盘文件层                                     │
│  - neostore.nodestore.db（节点文件）                              │
│  - neostore.relationshipstore.db（关系文件）                      │
│  - neostore.propertystore.db.*（属性文件，链式）                  │
│  - neostore.schemastore.db（schema：label/relType/约束/索引）      │
│  - neostore.transaction.db（transaction log，含 WAL）            │
│  - neostore.counts.db（节点/关系计数槽）                           │
└──────────────────────────────────────────────────────────────────┘
```

### 关键设计取舍

- **节点指针直连（Native Pointer Chasing）**：关系记录里存 `firstNodeId` / `secondNodeId` 和 `prevRelId` / `nextRelId` 双向链表。两步一跳即"任意深度遍历"，是 Neo4j 与"关系型映射图数据库"的关键区别。
- **定长节点 + 变长属性**：节点记录固定（KB 级槽位），属性动态链式 → 写入路径简单、读取路径极快。
- **GB+Tree（社区版 B+Tree）**：相比 Tiger / Elasticsearch，用更宽扇出（fan-out）减少树高。
- **Cypher 编译器层**：logical plan → cost-based optimizer → physical plan（pipelined / slotted / cartesian / parallel）。
- **fabric 多图联邦**（Enterprise）：跨多个 Neo4j 实例统一查询。
- **多语言驱动统一 Bolt**：所有官方客户端都走 Bolt 5 协议（TCP 二进制、自描述、参数化）。

## 七、存储引擎

Neo4j 的存储设计以"图遍历"为最高优先级。

### 7.1 节点与关系存储

- **节点记录**定长（默认 ≈41 字节），含 `id`、`labelIds[]`（多个 label 枚举）、`nextRelId`（第一条关系）、`nextPropId`（第一条属性）。
- **关系记录**含 `id`、`typeId`、`firstNodeId`、`secondNodeId`、`prevRelId`、`nextRelId`、`firstPrevRelId`、`firstNextRelId`（来源/目标节点的双向链表头），约 47 字节。
- **属性记录**链式 KV，每个属性键值对独立存储，长度变长。
- **遍历原理**：`MATCH (a)-[r]-(b)` 编译为 `(a.nextRelId)` 一路 `nextRelId` 链拉取，或按类型 `relType` 索引过滤。

### 7.2 节点-关系-属性

```
(a) --[KNOWS]--> (b) --[WORKS_ON]--> (c)

Neo4j 物理存储视角：
  NodeStore.id_a    → record { labelIds:[Person], nextRelId:r1 }
  RelStore.id_r1    → { type:KNOWS, first:a, second:b, prevRelId:?, nextRelId:r3 }
  NodeStore.id_b    → record { labelIds:[Person], nextRelId:r1, nextRelId_tail:r3 }
  RelStore.id_r3    → { type:WORKS_ON, first:b, second:c, prevRelId:r1, ... }
```

### 7.3 事务日志（WAL）

- `neostore.transaction.db` 为事务日志（logical log），每个事务 commit 时追加。
- **Composite Log Channels**（Neo4j 5.x）：将日志按需分流到不同 channel，便于备份与 CDC。
- 检查点（check-point）周期由 `dbms.checkpoint.interval.tx` 控制。

### 7.4 Counters

- `neostore.counts.db` 维护"节点 X label 计数"和"关系 X type 计数"，批量直方图更新。
- `dbms.record_format` 配置（默认 `standard`，实验性 `block` 块存储格式）。

### 7.5 压缩与刷盘

- 节点/关系记录不压缩（定长高效）；
- 属性字符串可配置 `CompressedPropertyRecordExtractor`（实验性）；
- 刷盘策略：`dbms.tx_log.rotation.retention_policy` + page-cache（mmap）。

### 7.6 备份与恢复

- 离线备份直接拷贝 `data/databases/<db>/`；
- 在线备份：Enterprise `neo4j-admin backup`（协议导出 + 重放）；
- CDC：Streams 行为 Kafka 主题（Enterprise）。

## 八、索引机制

Neo4j 5.x 的索引体系是"索引即一等公民"。8.0 后计划整合为复合索引（Composite Type-Aware Index）。

### 8.1 图存储模型（与关系型图对比）

| 维度 | 原生图（Neo4j） | 关系型映射 | 邻接表（Adjacency） |
|------|----------------|-----------|-------------------|
| 节点 | 定长 slot | 表的行 | 顶点数组 |
| 关系 | 双向链表 + 跨指针 | 表的行 + JOIN | 邻接 list（如 B-tree 子表） |
| 遍历 | 指针直连 O(1)-O(k) | 多 JOIN | 数组跳转 |
| 多 label | 多 label id 一并存 | 单独表 / EAV | 单 type |

Neo4j 走 **原生（原生图）+ CSR/CSC** 之外的"双向链表 + 类型内分组指针"路线：相邻节点按 `relTypeId` 分桶，每桶内部保持链表，跨类型遍历走"双跳（double jump）"——给定类型的关系数 ≤ hop 数内可直跳，跨类型靠 predicate 过滤。

### 8.2 节点 label 索引

- 每个 label 隐式提供 B+Tree 索引（node label index）存储 `labelId → nodeId`；
- 显式索引可选：`CREATE INDEX FOR (n:User) ON (n.email)`；
- schema index provider：`lucene-index` 或 `native-index（GBPTree）`。

### 8.3 关系类型索引

- `MATCH (:Person)-[:KNOWS]->(:Person)` 全扫描分类桶；
- 大量类型查询可通过 `@-prop` 复合索引：`CREATE INDEX FOR ()-[r:KNOWS]-() ON (r.since)`。

### 8.4 属性索引（B+Tree）

- 实现位于 `community/index/native-index/`；
- **GBPTree**（Generalized B+ Tree，社区版）支持宽 fan-out（≥ 32 KB 节点）；
- 点查：`n.email = "alice"` → 索引命中 → nodeId；
- 范围查询：`n.age > 20 AND n.age < 35`。

### 8.5 全文索引（5.x）

- `CREATE FULLTEXT INDEX …` 走 Lucene（HNSW + 倒排混合索引）；
- 查询：`CALL db.index.fulltext.queryNodes("idx", "Alice")`；
- 与向量索引底层共享一个 Lucene engine：`_fulltext_lucene_<n>`。

### 8.6 向量索引（5.x）

- `CREATE VECTOR INDEX … FOR (n:Doc) ON (n.embed) OPTIONS { indexConfig: { `vector.dimensions`: 1536, `vector.similarity_function`: 'cosine' } }`；
- 后端走 **Apache Lucene HNSW**（最近从自定义实现迁移到 Lucene-native）；
- 与全文 + 标量索引组合为"Hybrid Search"（label 过滤 + 向量距离）；
- 用于 GraphRAG / RAG 中的实体属性向量化。

### 8.7 索引代价与约束

- 索引越多写放大越严重（每次写入对应索引 B+Tree 都需更新）；
- 唯一约束 `CREATE CONSTRAINT … REQUIRE node.prop IS UNIQUE` 必配索引；
- 复合（Composite）索引与节点 key 约束：`nodeKey` 业务主键。

## 九、查询/计算/接口层

### 9.1 Cypher 查询语言

Cypher 是 Neo4j 的类 SQL 声明式图查询语言。关键子句：

```cypher
// 模式匹配：模式可视化
MATCH (p:Person {name:"Alice"})-[:KNOWS]->(friend)
WHERE friend.age > 18
RETURN friend.name, friend.age
ORDER BY friend.age DESC
LIMIT 10

// OPTIONAL MATCH
MATCH (p:Person)
OPTIONAL MATCH (p)-[:WORKS_ON]->(proj)
RETURN p.name, proj.title

// 可变长度路径
MATCH (a:Person)-[:KNOWS*1..5]->(b:Person)
WHERE a.name = "Alice"
RETURN DISTINCT b.name

// CALL 子程序（图算法 / GDS）
CALL gds.shortestPath.stream({
  sourceNode: id(a), targetNode: id(b), relationshipTypes: ['KNOWS']
})
YIELD path

// WITH / COLLECT 聚合
MATCH (p:Person)-[:KNOWS]->(f)
WITH p, COLLECT(f.name) AS friends
WHERE SIZE(friends) >= 3
RETURN p.name, friends
```

### 9.2 执行模型

- **Plan**：Logic Plan → Cost-Based Rewrite → Physical Plan（runtime: pipelined / slotted / cartesian）；
- **Slotted Runtime**：经典 Volcano-style，但用 slot 复用降低分配；
- **Pipelined Runtime**：5.0+ 默认引入，操作符间管道化、in-place 复用；
- **Parallel Runtime**（Eager / Morsel）：实验性，分片并行执行，适合分析查询；
- **Cartesian Runtime**（实验性）：cartesian product 局部向量化。

### 9.3 Bolt 协议

- TCP 二进制协议，自描述（版本协商、auth、route、begin/run/pull/discard 等 21 条消息）；
- 多语言驱动统一走 Bolt 5.x（声明路由、连接池、事务 pipelining）；
- 实验性 Arrow Flight：快速 ETL/分析，避免序列化开销。

### 9.4 图算法（图数据科学 GDS）

Neo4j 图算法库 GDS（独立插件，Apache 2.0）：

- **路径算法**：最短路径（BFS / Dijkstra / A* / Yen's K-shortest）、Steiner Tree、Random Walk；
- **中心性**：PageRank、ArticleRank、Betweenness、Closeness、HITS、Eigenvector、Degree；
- **社区检测**：Louvain、Label Propagation、Connected Components、Strongly Connected、K1 Coloring、Modularity；
- **节点嵌入**：FastRP（FAST RP 随机投影）、GraphSAGE（深度学习）、Node2Vec；
- **相似度**：Jaccard、Cosine、Euclidean、Overlap；
- **链路预测**：Adamic-Adar、Common Neighbors、Preferential Attachment、Resource Allocation；
- **拓扑链**：图遍历 API（Traversal API / BoltDFS / BreadthFirst）。

引擎：**Pregel 范式 + BSP 同步屏障（point-in-time execution, 6.x 实验性）**。

### 9.5 Browser / Bloom / Desktop

- **Neo4j Browser**：浏览器中 Cypher + 可视化；
- **Neo4j Bloom**：商业可视化（节点-关系图大幅可定制）；
- **Neo4j Desktop**：本地多数据库管理（用户态 daemon，组合 AuraDB / 本地）。

## 十、事务与一致性

### 10.1 ACID

- **原子性**：事务 commit 时先写 WAL（tx log），成功后释放锁；
- **一致性**：通过 constraint（uniqueness / existence / node key）；
- **隔离性**：默认 `READ_COMMITTED`，不支持非 repeatable read（点查强，范围查无 REPEATABLE READ）；
- **持久性**：事务 log 落盘后才返回 commit。

### 10.2 锁模型

- **Reentrant Lock**：节点 / 关系记录级共享/排他锁；
- **Deadlock Detection**：wraph + wait/timeout → 检测到后回滚最小代价事务；
- **Lock Manager**：`community/lock/` 实现的锁服务，READ_COMMITTED 下锁时长较短（点查锁即释）。

### 10.3 乐观并发与因果集群

- **CC（Causal Clustering）**（Enterprise）：Core + Read Replica，Raft 协议同步事务；
- 单数据库内乐观并发：`dbms.transaction.monitor.check.klass`、监控事务冲突。

### 10.4 CDC 与 Streams

- Streams CDC（Enterprise）将变更发到 Kafka topic；
- CDC source of truth = transaction log；
- `dbms.change_data_capture` configurable topic。

## 十一、复制与高可用

### 11.1 单机模式

- 单实例 `neo4j-server` + 1 个 Bolt port（7687）+ 1 个 HTTP port（7474）；
- 兼容嵌入式：`org.neo4j.graphdb.GraphDatabaseService`。

### 11.2 Causal Clustering（Enterprise）

```
                 ┌─────────────────┐
                 │   Write / Read  │
                 └─────────┬───────┘
                           │
            ┌──────────────┼──────────────┐
            ▼              ▼              ▼
        Core (Raft) ──── Core ──── Core
            ▲              ▲              ▲
            │              │              │
        Replica  ──── Replica  ──── Replica
```

- **Core**：3-5 节点，Raft 同步事务日志（每 Core 都能写入，多 Raft leader）；
- **Read Replica**：异步拉取 tx-log，可水平扩展，只提供只读；
- 客户端通过 Bolt **+ 路由表** 自动写发到 Core、读发到 Replica。

### 11.3 Fabric 多图联邦（Enterprise）

- 一个 Cypher 子句跨多个数据库实例：`USE graph.byName('db1')`；
- 跨实例 JOIN 通过 **Fabric** 服务统一调度。

### 11.4 跨区域容灾

- 异步副本支持跨数据中心；
- 5.x 提供 **Composite DB**（不同 database 之间可联邦）；
- 建议多区域部署间延时 ≤ 50 ms（否则 raft 性能下降明显）。

## 十二、扩展性

- **垂直扩展**：节点-关系记录定长，热点数据库可达数十亿节点（受 JVM heap 影响）；
- **水平扩展**：通过 Fabric 多图、Causal Cluster 多 Raft、Sharding-by-prefix（实验性）；
- **冷热分层**：社区版无原生 tiering，企业版提供 **Neo4j Enterprise + PageCache**；
- **向量索引**：HNSW 单数据集数十亿向量级别（受 JVM heap 影响）。

## 十三、性能与基准

- **LDBC SNB**（Linked Data Benchmark Council Social Network Benchmark）：Neo4j 在 LDBC-SNB-Interactive SF=30 上 700 QPS（4.0），5.x 后有显著优化；
- **内部 Bench**：仓库内 `community/neo4j/src/test/bench/` 提供 micro-benchmark（HNSW-Bench、Traversal-Bench）；
- **第三方**：Neo4j 官方 benchmark 报告 4.0 在 SF=1000 单实例可达 ~50 万节点-关系（受硬件影响）；
- **典型数字**：
  - 单实例写入 1w~10w QPS（点查），1k~1w QPS（深度遍历）；
  - 图遍历 5-hop 含过滤器 0.5~10ms（千万级图）；
  - 向量 HNSW（1.5k dim，10w 向量）单核 ANN ~1ms。

## 十四、运维与可观测性

- **指标**：`server.metrics.enabled=true` + Prometheus 集成（Enterprise）；
- **慢查询日志**：`dbms.logs.query.threshold=5s` + `dbms.logs.query.parameter_logging`；
- **Page Cache**：`server.memory.heap.initial_size` / `server.memory.heap.max_size` 调优 + `server.memory.pagecache.size`；
- **导入工具**：`neo4j-admin import`（CSV bulkload）+ `neo4j-admin database import` + `migrate-into-neo4j-via-explain-cypher`；
- **备份恢复**：`neo4j-admin backup`（Enterprise）+ 离线拷贝；
- **Tracing**：OpenTelemetry 实验性集成（5.x）；
- **CDC**：Enterprise Streams 发到 Kafka。

## 十五、生态与集成

- **客户端**：
  - Java：`org.neo4j.driver:neo4j-java-driver`；
  - Python：`neo4j`（同步 + async，自带 Bolt 5）；
  - JS/TS：`neo4j-driver`（浏览器、Node、React Native）；
  - Go：`github.com/neo4j/neo4j-go-driver/v5`；
  - .NET：`Neo4j.Driver`；
  - 全部走 Bolt 5。
- **图算法**：GDS 库（Apache 2.0，独立仓库 `neo4j/graph-data-science`）；
- **ORM / OGM**：Spring Data Neo4j、Neo4j-OGM、Quarkus Neo4j；
- **BI**：Tableau Graph Connector、PowerBI Neo4j connector、Apache Superset；
- **消息队列**：Kafka（CDC）、Confluent Connect Neo4j Sink/Source；
- **容器化**：`neo4j:5.x` 官方镜像（Docker Hub）+ Helm chart（社区）；
- **AI / GenAI**：LangChain Neo4j integration、LlamaIndex Neo4j Reader/Writer、Haystack Neo4j Retriever、Neo4j GenAI plugin（OpenAI / Anthropic / Vertex / Bedrock）。

## 十六、核心论文与算法原理

- **Property Graph 数据模型**：`https://neo4j.com/docs/graphql/current/data-modeling/property-graph/`；
- **Cypher 规范**：`https://opencypher.org/specification/` 与 Cypher 5 implementation guidelines；
- **Apache Lucene HNSW**：Malkov & Yashunin 2018（"Efficient and robust approximate nearest neighbor search using Hierarchical Navigable Small World graphs"，IEEE TPAMI）；
- **GBPTree**：广义 B+ Tree（`github.com/neo4j/neo4j` `community/index/gbptree`）；
- **Causal Consistency**：`https://jepsen.io/consistency/models/causal` + Neo4j 集群白皮书；
- **Raft 论文**：Diego Ongaro 2014（Stanford PhD thesis，CORE 内部用其变体）；
- **GDS 图算法**：
  - PageRank / Louvain / Label Propagation 等算法论文参见 GDS 文档 `https://neo4j.com/docs/graph-data-science/current/algorithms/`。

## 十七、源码学习路径

| 阶段 | 模块 | 路径 | 要点 |
|------|------|------|------|
| 入门 | 顶层 API | `community/kernel-api/src/main/java/org/neo4j/kernel/api/` | 理解 GraphDatabaseService、Transaction、KPI |
| 入门 | 客户端驱动 | Bolt messages（`bolt-messages/`） | 理解 Bolt 协议 |
| 进阶 | Cypher parser | `community/cypher/cypher-ast/` | 抽象语法树 |
| 进阶 | Cypher planner | `community/cypher/cypher-planner/` | Logical → Physical 计划 |
| 进阶 | Cypher runtime | `community/cypher/cypher-runtime/` | Volcano → Pipelined |
| 高级 | 存储引擎 | `community/kernel/impl/store/` | NodeStore / RelStore / Property |
| 高级 | 原生索引 | `community/index/native-index/` + `community/index/gbptree/` | GBPTree 实现 |
| 高级 | 向量索引 | `community/index/lucene-index/` | Lucene HNSW 适配 |
| 高级 | 复制（EE） | `enterprise/causal-clustering/` | Raft + 路由（企业版） |
| 高级 | 图算法 | 外部仓库 `neo4j/graph-data-science` | Pregel-style 算法 |

## 十八、源码规模与开发语言

- **LOC**：超过 200 万行 Java + Scala（社区版 + 企业版合一）；
- **语言占比**：
  - Java ≈ 80%（Kernel、Server、Cypher 部分）；
  - Scala ≈ 15%（部分 Cypher、工具）；
  - Python / Cypher DSL / JS / YACC & ANTLR ≈ 5%；
- **构建系统**：Maven 多模块（`community/pom.xml`、`enterprise/pom.xml`）+ 部分子模块 Gradle；
- **JDK**：JDK 17+（5.x）；
- **CI**：Neo4j 内部 Buildkite，对外 GitHub Actions 部分工作流；
- **代码风格**：Neo4j Style Guide 与 Checkstyle 规则；`mvn -pl <module> compile` 即可增量编译。

## 十九、学习资源

- **官方文档**：
  - `https://neo4j.com/docs/developer-manual/current/` 主开发者手册；
  - `https://neo4j.com/docs/cypher-manual/current/` Cypher 手册；
  - `https://neo4j.com/docs/operations-manual/current/` 运维手册；
  - `https://neo4j.com/docs/graph-data-science/current/` GDS；
  - `https://neo4j.com/docs/browser-manual/` Browser 使用；
- **书籍**：
  - "Graph Databases"（Ian Robinson / Jim Webber / Emil Eifrem，O'Reilly）；
  - "Knowledge Graphs"（Mayo Aitken、Lloyds Bank）；
  - "Learning Neo4j 4.x"（Chris Skardon，Apress）；
  - "Building Knowledge Graphs with Neo4j"（Sumit Pal）；
- **视频 / 课程**：Neo4j 官方 YouTube channel、GraphAcademy（Neo4j 在线认证课，免费）、GraphSummit 大会回放；
- **社区**：Neo4j Community Forum、Reddit r/Neo4j、Stack Overflow `[neo4j]`、Neo4j Discord；
- **Reddit / HN**：搜索 "Neo4j 5" "GenAI" "GraphRAG"；
- **实战项目**：Neo4j Movies 示例、Spatial Demo、Sandbox of the Day。

## 二十、可移植性与部署

- **支持平台**：Linux（x86_64 / ARM64）、macOS（x86_64 / M1/M2 ARM）、Windows（x86_64，2024+ 兼容）；
- **容器化**：官方 Docker 镜像（`neo4j:5.x`、`neo4j:5.x-enterprise`），提供 `NEO4J_AUTH` / `NEO4J_SERVER_MEMORY` 等环境变量；
- **Helm Chart**：社区 chart（`helm/charts/neo4j`、`helm/charts/secrets`）；
- **Kubernetes Operator**：Neo4j 官方 Operator（`neo4j/neo4j-kubernetes-operator`）5.0+，DeclarativeSpec；
- **嵌入式部署**：
  - **嵌入式模式**：Java 进程内 `org.neo4j.driver.Driver` 不算嵌入；真正嵌入式需用 `neo4j-embedded`（已弃用）或新生态用 **Memgraph / Kuzu** 替代；
  - 官方推荐单机 `neo4j-server` + Bolt 客户端。

## 二十一、变体与衍生项目

- **Neo4j Enterprise**（商业闭源）：含 Causal Clustering、Fabric、Streams、RBAC、SAML/LDAP、Backup；
- **Neo4j AuraDB**：全托管云服务（Free / Professional / Enterprise / Virtual Dedicated）；
- **Neo4j AuraDS**：数据科学管理工具（图算法 + 向量 ETL）；
- **Neo4j OPenCypher TCK**：测试套件让其他 Cypher 兼容实现（如 Memgraph、openCypher for Apache Spark）；
- **衍生**：Redis Graph（已弃用）、AgensGraph（PostgreSQL 内置图，改写 SQL）、Crafting Software Graph + AI 系列；
- **OGC / openCypher**：`opencypher.org` 致力于将 Cypher 标准化为 ISO GQL（Graph Query Language）。

## 二十二、风险与陷阱

### 22.1 GPLv3 / AGPL 影响

- **GPLv3** 强 copyleft：派生 / 修改 Neo4j 必须开源；
- **AGPLv3**：通过网络使用 Neo4j 服务，外部客户端访问也算传播链路 → 必须开源 SaaS 整体代码；
- 自托管若走 **Enterprise 许可证（商业）**，可规避 AGPL；
- 云原生集成（如自托管 + 公网 Bolt）若违反 AGPL 法律风险大。

### 22.2 容量与 JVM 调优

- 大节点（数十亿）容易踩 Page Cache / Heap 调优；
- 大量遍经历 stack overflow（深度过深、Iterable 走错类型）：用 `LIMIT`/`shortestPath` 兜底；
- 大集合（`COLLECT`）超过万条需分批；
- 事务过长（>50ms）容易 lock-wait 与 deadlock，需拆分短事务。

### 22.3 Cypher 性能陷阱

- **大量关系类型**：`MATCH (a)-[:T1|T2|...|TN]->(b)` 是全类型 union，慢查询头号；
- **深度优先 + OPTIONAL MATCH**：编译器的形状不佳时多次重扫；
- **未命中索引**：过滤不命中 label/属性索引 → 全图扫描；
- **Cartesian Product**：多 OPTIONAL MATCH 拼连后未缩边；
- 规避：`EXPLAIN` + `PROFILE` + 索引 + `LIMIT` 早停。

### 22.4 数据迁移

- `neo4j-admin import` 仅支持 **空库**导入；
- 在线迁移走 `apoc.periodic.iterate`（APOC 库）或自定义 ETL；
- CSV 列顺序与 ID 类型有限制，需预处理；
- 旧数据迁移（4.x → 5.x）需 `neo4j-admin migrations`。

### 22.5 向量索引使用

- 向量索引不能与 label filter 共用同一属性；
- 维度与 metric 必须在创建时指定不可热改；
- Lucene HNSW 的 parameter（如 `vector.quantization.enabled`）需测试；
- GraphRAG 经常混入自定义 embedding 函数，需先 sanity check cosine 距离结果。

### 22.6 事务与隔离限制

- 仅 `READ_COMMITTED`，无 REPEATABLE READ；
- 跨事务一致性需借助 **Causal Cluster + leader write + bookmark**；
- 大量 range read + write 容易出现 read skew（应用层需自行控制）；
- 写事务无 optimistic retry，需手写 retry 循环。

### 22.7 资源泄露

- 长事务阻塞 GC / Page cache；
- 关闭 cursor、iterator 必须 try-finally；
- 客户端驱动连接池饱和需调高 `max_connection_pool_size`；
- Bolt session 复用而非每次新建。

## 二十三、与本项目的关联

- **设计参考**：本项目 `engineering/src/db/` 中存储引擎设计借鉴 Neo4j 的"定长节点记录 + 双链表关系指针"思想；
- **索引机制**：本项目 `engineering/include/db/index/` 中 B+Tree 概念源自 GB+Tree；
- **Cypher 解析**：未实现；学习曲线对应本项目扩展到 SQL 的 path expression；
- **生态整合**：未来可考虑用 Bolt 协议暴露本项目引擎以便 Python / JS 客户端连接；
- **向量索引**：本项目"多模态"框架里 graph + vector 检索的混合，与 Neo4j 5.x 同步演进。

## 二十四、记录格式与页内槽位分配（2026.07.0 实测）

> **版本基准**：本节及以下两节的所有行号均以本仓库 `reference/graph/neo4j/` 的**完整 clone** 为准，其 `pom.xml:7` 声明版本为 `2026.07.0`（**日历版本号，已不是 5.x**）。因此下面给出的字节数、位布局、算法与前文 §六~§八 按 5.x 描述的部分内容存在出入，本节只陈述代码事实，不改写既有章节。

### 24.1 三个记录格式族

`StoreVersion` 把「记录格式」建模为 **格式族（FormatFamily）× 存储大版本** 的二维枚举，每族每版本一个不可变单例，实现在 `community/record-storage-engine/src/main/java/org/neo4j/kernel/impl/store/format/StoreVersion.java:26`：

```java
STANDARD_V4_3(FormatFamily.STANDARD,  0, 1, "4.3.0", true),
STANDARD_V5_0(FormatFamily.STANDARD,  1, 1, "5.0.0", false),
ALIGNED_V4_3 (FormatFamily.ALIGNED,   0, 1, "4.3.0", true),
ALIGNED_V5_0 (FormatFamily.ALIGNED,   1, 1, "5.0.0", false),
HIGH_LIMIT_V4_3(FormatFamily.HIGH_LIMIT, 0, 1, "4.3.0", true),
HIGH_LIMIT_V5_0(FormatFamily.HIGH_LIMIT, 1, 1, "5.0.0", false);
```

（逐条行号：STANDARD 在 `community/record-storage-engine/src/main/java/org/neo4j/kernel/impl/store/format/StoreVersion.java:26` 与 `:27`，ALIGNED 在 `:29` 与 `:30`，HIGH_LIMIT 在 `:32` 与 `:33`。）

三个族的取舍：

| 族 | 核心差异 | 代价 |
|----|---------|------|
| `STANDARD` | 传统定长记录；文件页大小向下截断到 `recordSize` 的整数倍，记录**绝不跨页** | 页尾留下 0~`recordSize-1` 字节空洞；OS 无法按整页读写 |
| `ALIGNED` | 页尾补齐，记录可跨页；文件页大小 == page cache 页大小 | 补齐字节浪费；`PageAlignedV5_0` 类注释直言「唯一没有直接演化的理由是迁移代价高」 |
| `HIGH_LIMIT` | 用存储「高水位页」把稀疏 id 段的物理页密度做高 | 结构更复杂 |

ALIGNED 族的设计意图写在 `community/record-storage-engine/src/main/java/org/neo4j/kernel/impl/store/format/aligned/PageAlignedV5_0.java:52`：

> `Pages are padded at the end instead of letting record span 2 pages.` —— 让 OS 每次能取/写**完整的 8K 页**，比「8K 减 5 字节」友好得多。

三族对「文件页大小」的处理是同一个三元表达式的两分支，见 `community/record-storage-engine/src/main/java/org/neo4j/kernel/impl/store/format/BaseRecordFormat.java:108`：

```java
public int getFilePageSize(int pageSize, int recordSize) {
    return pageAligned ? pageSize : Math.min(pageSize, pageSize - pageSize % recordSize);
}
```

即 **ALIGNED 直接用整页；其余族把页大小向下取整到记录大小的整数倍**，从而保证一条记录永远落在单页内。

### 24.2 各 store 的记录字节布局

所有定长记录的 `RECORD_SIZE` 常量都集中在 `community/record-storage-engine/src/main/java/org/neo4j/kernel/impl/store/format/standard/` 下的各个 `*RecordFormat` 类里：

| 记录 | 字节 | 常量出处 | 布局（源码注释原文） |
|------|------|---------|--------------------|
| `NodeRecord` | **15** | `community/record-storage-engine/src/main/java/org/neo4j/kernel/impl/store/format/standard/NodeRecordFormat.java:32` | `in_use(byte)+next_rel_id(int)+next_prop_id(int)+labels(5)+extra(byte)`（注释在同文件 `:31`） |
| `RelationshipRecord` | **34** | `community/record-storage-engine/src/main/java/org/neo4j/kernel/impl/store/format/standard/RelationshipRecordFormat.java:35` | `directed\|in_use(byte)+first_node(int)+second_node(int)+rel_type(int)+first_prev_rel_id(int)+first_next_rel_id+second_prev_rel_id(int)+second_next_rel_id+next_prop_id(int)+first-in-chain-markers(1)` |
| `RelationshipGroupRecord` | **25** | `community/record-storage-engine/src/main/java/org/neo4j/kernel/impl/store/format/standard/RelationshipGroupRecordFormat.java:38` | `[type+inUse+highbits,next,firstOut,firstIn,firstLoop,owningNode] = 25B`（注释在 `:33`） |
| `PropertyRecord` | **41** | `community/record-storage-engine/src/main/java/org/neo4j/kernel/impl/store/format/standard/PropertyRecordFormat.java:37` | `1 /*next and prev high bits*/ + 4 /*next*/ + 4 /*prev*/ + 32 /*property blocks*/` |
| `TokenRecord`（label / propKey / relType 共用基类） | **5** + 名串 | `community/record-storage-engine/src/main/java/org/neo4j/kernel/impl/store/format/standard/TokenRecordFormat.java:30` | `1 /*inUse*/ + 4 /*nameId*/` |

**这里必须纠正前文 §7.1 的两个数字**：节点记录不是「≈41 字节」，关系记录不是「约 47 字节」。实测分别是 **15** 与 **34** 字节；41 字节是 **属性记录** 的大小（前文把属性记录的数值安到了节点头上）。

节点的 15 字节还能再压：header 字节的 8 个 bit 被**全部分配**掉，写路径的注释在 `community/record-storage-engine/src/main/java/org/neo4j/kernel/impl/store/format/standard/NodeRecordFormat.java:95`：

```java
// [    ,   x] in use bit
// [    ,xxx ] higher bits for rel id
// [xxxx,    ] higher bits for prop id
```

对应读路径的两行掩码移位在同文件 `:62` 与 `:63`：

```java
long relModifier  = (headerByte & 0xEL) << 31;   // header 的 bit1..bit3 -> nextRel 的 bit32..bit34
long propModifier = (headerByte & 0xF0L) << 28;  // header 的 bit4..bit7 -> nextProp 的 bit32..bit35
```

所以一个节点记录的**有效载荷只有 14 字节**（1 header + 4 nextRel + 4 nextProp + 5 labels），而 `nextRel` 与 `nextProp` 各有 3 / 4 个高位藏在 header 里。id 位宽上限由 `community/record-storage-engine/src/main/java/org/neo4j/kernel/impl/store/format/standard/StandardFormatSettings.java:29` 的 `NODE_MAXIMUM_ID_BITS = 35` 封顶——这个数字直接由「header 里能藏几位」推导出来：4 字节的 `int` 只给 32 位，再加 header 里的 3 位，正好 35。label token 上限 31 位（`StandardFormatSettings.java:37`）则是被 JVM 的 `HashMap` key 用 `int[]` 装载这一实现细节倒逼的，源码注释直接写了原因。

### 24.3 页内槽位寻址：O(1) 的 id → (page, offset)

定长 store 的寻址是**纯除法**，没有任何哈希或查找表，实现在 `community/record-storage-engine/src/main/java/org/neo4j/kernel/impl/store/RecordPageLocationCalculator.java:36` 与 `:49`：

```java
public static long pageIdForRecord(long id, int recordsPerPage) {
    return id / recordsPerPage;
}
public static int offsetForId(long id, int recordSize, int recordsPerPage) {
    return (int) (id % recordsPerPage) * recordSize;
}
```

每页能放多少条记录由 `community/record-storage-engine/src/main/java/org/neo4j/kernel/impl/store/CommonAbstractStore.java:391` 决定：

```java
recordsPerPage = (filePageSize - pagedFile.pageReservedBytes()) / recordSize;
recordsEndOffset = recordsPerPage * recordSize; // Truncated file page size to whole multiples of record size.
```

**复杂度**：`pageId`/`offset` 两条整除各 O(1)，`recordsPerPage` 在 store 打开时算一次并缓存（字段声明在 `CommonAbstractStore.java:89`）。这就是「定长记录」相对「变长槽位表」的全部性能意义：**指针直连 = 一次整除 + 一次页缓存取页，没有间接层**。

以硬编码的 8K 页（`community/io/src/main/java/org/neo4j/io/pagecache/PageCache.java:49` 的 `int PAGE_SIZE = 8192`；该版本**没有**用户可配置的 `page_size` 选项，且会被校验为 2 的幂）+ STANDARD 族 + `NodeRecordFormat`（15B）为例：`recordsPerPage = 8192 / 15 = 546`，`recordsEndOffset = 546 × 15 = 8190`，每页尾部浪费 2 字节。同样的 8K 页放 `RelationshipRecordFormat`（34B）：`8192 / 34 = 240`，尾部浪费 32 字节——**记录越大，STANDARD 族的页尾空洞越明显**，这正是 ALIGNED 族存在的直接动因。

### 24.4 Store 文件清单（2026.07.0）

`RecordDatabaseFile` 枚举枚举了**全部**数据库文件，共 16 个（`community/layout/src/main/java/org/neo4j/io/layout/recordstorage/RecordDatabaseFile.java:40` 起），文件名常量集中在 `community/layout/src/main/java/org/neo4j/io/layout/recordstorage/RecordDatabaseFileNames.java:26` 起：

```java
COUNTS_STORE                    = "neostore.counts.db";                              // :26
NODE_STORE                      = "neostore.nodestore.db";                           // :28
NODE_LABELS_STORE               = "neostore.nodestore.db.labels";                    // :29
RELATIONSHIP_STORE              = "neostore.relationshipstore.db";                   // :31
RELATIONSHIP_GROUP_STORE        = "neostore.relationshipgroupstore.db";              // :32
RELATIONSHIP_GROUP_DEGREES_STORE= "neostore.relationshipgroupstore.degrees.db";      // :33
PROPERTY_STORE                  = "neostore.propertystore.db";                       // :37
PROPERTY_ARRAY_STORE            = "neostore.propertystore.db.arrays";               // :38
PROPERTY_STRING_STORE           = "neostore.propertystore.db.strings";              // :39
SCHEMA_STORE                    = "neostore.schemastore.db";                         // :46
INDEX_STATISTICS_STORE          = "neostore.indexstats.db";                          // :48
METADATA_STORE                  = "neostore";                                        // :50
```

对比前文 §六「磁盘文件层」列的 6 个文件，有三处需要修正：

1. **`neostore.transaction.db` 已不在文件清单里** —— 事务日志早已迁到独立目录，由 `community/kernel/src/main/java/org/neo4j/kernel/impl/transaction/log/files/TransactionLogFiles.java` 管理，不再与数据文件混放；
2. **多出 `neostore.nodestore.db.labels`** —— 标签溢出（一个节点的 label 装不进 5 字节时）落在这个独立的 dynamic store 里，见 §二十五；
3. **多出 `neostore.relationshipgroupstore.db` 及其 `.degrees.db`** —— 前者存关系组记录，后者是关系组度数的 **GBPTree**（§二十五详述），前文完全没有提到这两个文件。

每个 store 文件的**头 1 条记录被保留**放 store header，格式是一个 `int`（记录格式的版本号），见 `community/record-storage-engine/src/main/java/org/neo4j/kernel/impl/store/IntStoreHeaderFormat.java:33` 的 `numberOfReservedRecords()` 返回 `1`，写入是同文件 `:38` 的一行 `cursor.putInt(header)`。

## 二十五、密集节点、关系组与遍历状态机

### 25.1 稀疏节点 / 密集节点的分界

Neo4j 的节点按关系数分成两种物理形态，切换阈值可配置，默认 **50**，见 `community/configuration/src/main/java/org/neo4j/configuration/GraphDatabaseSettings.java:797`：

```java
public static final Setting<Integer> dense_node_threshold = newBuilder(
                "db.relationship_grouping_threshold", INT, 50)
```

判定发生在每次建关系时、真正写入之前，`community/record-storage-engine/src/main/java/org/neo4j/internal/recordstorage/RelationshipCreator.java:152`：

```java
if (relCount(node.getId(), relProxy.forReadingData()) >= denseNodeThreshold) {
    convertNodeToDenseNode(nodeChange, relProxy.forChangingLinkage(), relRecords, ...);
}
```

**关键设计（很反直觉但很省）**：稀疏节点根本没有独立的「度数」字段——**度数就内联在关系链首条关系的 `prevRel` 字段里**。`RelationshipCreator.java:135` 的两行注释和实现把这件事说得很直白：

```java
static int relCount(long nodeId, RelationshipRecord rel) {
    // Degrees are stored in the backward pointer of the first in chain!
    return (int) rel.getPrevRel(nodeId);
}
```

于是「这个节点有多少条出边」是 **O(1)** 的：读一次 `nextRel` 指向的链首关系，再取它的 `prevRel`。省掉了一个 store 的随机读，但代价是**链首关系是热 key**。

### 25.2 稀疏 → 密集的转换算法

`convertNodeToDenseNode`（`RelationshipCreator.java:253`）做的是一次**链表重排**，复杂度 O(k)（k = 该节点当前关系数）：

```
convertNodeToDenseNode(node, firstRel):
  1  node.setDense(true);  node.setNextRel(NULL)      // nextRel 语义整体切换
  2  for rel in chain(firstRel):                     // 边走边改写
  3      next = rel.getNextRel(node.id)              // 先读出后继（将被覆盖）
  4      rel.setPrevRel(NULL, node.id)
  5      rel.setNextRel(NULL, node.id)
  6      rel.setDense(node.id)                       // 标记该端由组记录管理
  7      connectRelationshipToDenseNode(node, rel, ...)
  8  goto 2
```

每条关系被摘出旧的双向链表、再挂进对应「类型 × 方向」的关系组链表。因为 group 记录按 `type` 排序存储，转换后遍历同一个节点时**按类型跳变快得多**，而稀疏节点遍历一条关系就要在关系 store 里跨页跳。

### 25.3 关系组记录与遍历状态机

关系组记录只有 25 字节，却承载了「一个节点 × 一种类型 × 三个方向」的全部入口，字段定义见 `community/record-storage-engine/src/main/java/org/neo4j/kernel/impl/store/record/RelationshipGroupRecord.java:30` 起：`type`、`next`、`firstOut`、`firstIn`、`firstLoop`、`owningNode`。

遍历一个密集节点，由 `community/record-storage-engine/src/main/java/org/neo4j/internal/recordstorage/RecordRelationshipTraversalCursor.java:205` 的注释给出了完整状态机（外加 `:184` 的 ASCII 示意图）：

```
1) 取下一个 group；没有更多 group 就停
2) next = group.incomingReference；状态 -> OUTGOING
3) 沿关系链迭代到末尾
4) next = group.outgoingReference；状态 -> LOOP
5) 沿关系链迭代到末尾
6) next = group.loop；状态 -> INCOMING
7) 沿关系链迭代到末尾
8) GOTO 1
```

对应的实现是一个三状态 `switch`，在 `RecordRelationshipTraversalCursor.java:215` 起。状态判定入口是**同一个 `nextRel` 字段的两义性**——`init` 方法在 `:90` 处根据 `isDense` 二选一（`:97` 走 `groups()`，`:102` 走 `chain()`）。

状态机的两个精妙剪枝：

1. **按类型早停**：group 按 `type` 有序，一旦 `group.getType() > selection.highestType()` 就直接返回（`:225`），因为后面的组类型只会更大；
2. **按类型整组跳过**：类型不在 selection 里就 `continue`，整条组链表**一次都不读**（`:229`），这正是 `MATCH (a)-[:KNOWS]->(b)` 能避开读全部关系的机制——从「O(度数)」降到「O(度数/K)」。

**遍历复杂度**：设节点度数为 d、类型数为 T，则稀疏节点全类型遍历 O(d) 次关系读；密集节点遍历为 O(T + d)（T 次组记录读 + d 次关系读），但 `MATCH (a)-[:T_i]->(b)` 单类型查询退化为 O(T + d/K)，K 是该类型的平均关系数。

### 25.4 引用编码：让「组引用」伪装成「关系引用」

`nextRel` 这一个 8 字节字段，既可能指向关系记录，也可能指向关系组记录。`community/record-storage-engine/src/main/java/org/neo4j/internal/recordstorage/RelationshipReferenceEncoding.java:57` 用最高 4 个 bit 编码这个区别：

```java
static final long FLAG_MARKER = 0x8000_0000_0000_0000L;   // :57  最高位
static final long FLAG_MASK   = 0x7000_0000_0000_0000L;   // :58  flag 载荷占 bit60..62
static final long FLAGS       = 0xF000_0000_0000_0000L;   // :59  清除用
```

`encodeDense` 在 `:84` 打标记位：

```java
static long encodeDense(long reference) {
    return reference | DENSE.bits | FLAG_MARKER;
}
```

类注释（`:45` 起）说明了 `FLAG_MARKER` 存在的唯一理由：**编码后的引用在数值上必然为负**，这样任何「忘记用 `clearEncoding` 就直接当 id 用」的错误都会立刻暴露成负数下标，而不是静默读到错误记录。这是一个用值域换断言的设计。

还有一个防御性设计在遍历器里：`RecordRelationshipTraversalCursor.java:163` 的注释说明，如果遍历**稀疏**节点的过程中该节点被另一个事务转成了密集节点，读到的边可能既漏又重，于是直接抛 `TransientTransactionFailureException.outdatedRead()` 要求应用重试——**宁可让事务失败，也不返回不一致的遍历结果**。

### 25.5 关系组度数：GBPTree 而非定长 store

前文 §7.4 只提到「counts.db 维护计数槽」。实测中**关系组度数已经搬进 GBPTree**：`community/record-storage-engine/src/main/java/org/neo4j/internal/counts/GBPTreeRelationshipGroupDegreesStore.java:46` 的类声明直接写明 `extends GBPTreeGenericCountsStore implements RelationshipGroupDegreesStore`。

它的 key 是一个手工压位的 `CountsKey`，注释画出了位图（`GBPTreeRelationshipGroupDegreesStore.java:136`）：

```
first:  [gggg,gggg][gggg,gggg][gggg,gggg][gggg,gggg] [gggg,gggg][gggg,gggg][gggg,gggg][gggg,ggdd]
        g: relationship group id,   d: RelationshipDirection#id()
second: 0
```

压位实现就一行，在同文件 `:147`：

```java
static CountsKey degreeKey(long groupId, RelationshipDirection direction) {
    return new CountsKey(TYPE_DEGREE, groupId << 2 | direction.id(), 0);
}
```

即 **35 位 groupId + 2 位方向 = 37 位**，塞进 key 的第一个 8 字节；`CountsKey` 总长 13 字节（1 字节 type + 8 字节 first + 4 字节 second），见 `community/storage-engine-util/src/main/java/org/neo4j/internal/counts/CountsKey.java:31`。

度数的更新路径因此变成 **GBPTree 的 key-value 增量写**（`CountUpdater.increment`，被 `GBPTreeRelationshipGroupDegreesStore.java:122` 调用），复杂度 O(log N)、N 为树节点数。而读路径 `degree(groupId, direction, ...)`（`:100`）是一次点查，同样 O(log N)。对比旧的定长度数槽 O(1)：**用 log 因子换掉了「度数与 group 记录 1:1 稠密」带来的空间浪费**——只有实际存在的关系组才有度数条目。

### 25.6 GBPTree 内核：分裂、合并与 latch coupling

标签索引（§26）与度数（§25.5）共用同一个 `GBPTree` 内核，其算法决策集中在三处：

**① 分裂比例**。唯一的分裂常数在 `community/index/src/main/java/org/neo4j/index/internal/gbptree/InternalTreeLogic.java:94`：`DEFAULT_SPLIT_RATIO = 0.5`，注释说明语义是「0=左节点全空，0.5=对半，1=左节点全留」。真正算出分裂位置的是 `community/index/src/main/java/org/neo4j/index/internal/gbptree/TreeNodeUtil.java:96`：

```java
static int splitPos(int keyCount, double ratioToKeepInLeftOnSplit) {
    int minSplitPos = 1;          // 左节点至少留 1 个 key（不能留空）
    int maxSplitPos = keyCount - 1;
    return Math.max(minSplitPos, Math.min(maxSplitPos, (int) (ratioToKeepInLeftOnSplit * keyCount)));
}
```

两次 clamp 保证分裂后**左、右节点都非空**，这是 B+Tree 不变式。

**② 合并判定用容量而非阈值**。定长叶子节点的容量是**推导出来**的（`community/index/src/main/java/org/neo4j/index/internal/gbptree/LeafNodeFixedSize.java:83` 的 `maxKeyCount = floorDiv(payloadSize - headerSize, keySize + valueSize)`），合并/再平衡判据直接写成容量的函数，同文件 `:294` 与 `:304`：

```java
public int canRebalance(PageCursor left, int leftKeyCount, PageCursor right, int rightKeyCount) {
    if (leftKeyCount + rightKeyCount >= maxKeyCount) {
        int moveFromPosition = (rightKeyCount + leftKeyCount) / 2;   // 目标：两兄弟各占一半
        return leftKeyCount - moveFromPosition;
    }
    return -1;                      // -1 哨兵 = 不再平衡，直接合并
}
public boolean canMerge(PageCursor left, int leftKeyCount, PageCursor right, int rightKeyCount) {
    return leftKeyCount + rightKeyCount <= maxKeyCount;
}
```

即：两兄弟**能装进一个节点就合并**（省一页），装不下但总和超过容量就**各搬一半**（平衡扇出）。

**③ 节点内二分查找**。`community/index/src/main/java/org/neo4j/index/internal/gbptree/KeySearch.java:92` 起是教科书式的二分，节点内查找 O(log k)、k 为节点内 key 数；树高 h 层 → 点查 O(h·log k)，B+Tree 下 h 通常 3~4 层。

**④ 为什么读者不会被写者阻塞**：`community/index/src/main/java/org/neo4j/index/internal/gbptree/LatchCrabbingCoordination.java:34` 的类注释直接给出「Better Latch Crabbing」的两种模式——

> *Optimistic mode uses read latches all the way down to leaf. Down at the leaf the latch is upgraded to write... If operation is unsafe (split/merge) then first an optimistic latch upgrade on parent is attempted... Otherwise... will result in releasing of the latches and flip to pessimistic mode.*

即 **无冲突时全路径只持读闩，只有到叶子才升级为写闩**；并发读者同样只持读闩，因此**读-读不互斥、读不阻塞写**。一旦发现分裂/合并这类结构性变更，放不下就整条路径释放并切到悲观模式（全程写闩）重做。这是把「无冲突快路径 O(h) 且无互斥」与「有冲突时保证正确」组合起来的标准手法。

## 二十六、标签索引：位图化 Token Scan 与事务状态归并

### 26.1 先纠正一个架构事实：`LabelScanStore` 已不存在

用 `find` 在整个 clone 的主源码里搜索，`LabelScanStore` / `LabelIndexStore` / `BTreeIndex` / `BTreeSampler` / `BTreeUpdater` / `BTreePopulator` / `SchemaIndexProvider` **零命中**。唯一的残留引用在 `community/record-storage-engine/src/main/java/org/neo4j/kernel/impl/storemigration/legacy/SchemaStore44Reader.java`——那是从 Neo4j 4.4 迁移数据时用的 legacy 读取器，读的是老版本的 `labeltokenstore` 布局。（`NativeIndex` 这个名字仍然存在，但已是 `kernel/impl/index/schema/` 下另一个类，与老版本被删除的 `org.neo4j.kernel.api.impl.index.NativeIndex` 无关。）`community/record-storage-engine/src/main/java/org/neo4j/kernel/impl/store/SchemaStore.java:104` 唯一的字段 `private final PropertyStore propertyStore;`，**不再持有任何标签索引结构**；schema 抽象也已迁到独立的 `community/schema` 模块（`IndexType` 枚举把索引分为 `TOKEN / LOOKUP / TEXT / RANGE / POINT / VECTOR / FULLTEXT` 七类）。

前文 §8.2 说「每个 label 隐式提供 B+Tree 索引（node label index）存储 `labelId → nodeId`」——方向对（确实是 GBPTree），但**键值模型说错了**。实际形态是**位图化的 token 索引**。

### 26.2 位图模型：一个 64 节点窗口 = 一个 tree 条目

索引是一棵 `GBPTree<TokenScanKey, TokenScanValue>`，字段声明在 `community/kernel/src/main/java/org/neo4j/kernel/impl/index/schema/DefaultTokenIndexReader.java:41`。

- **Key** `TokenScanKey{int tokenId; long idRange;}` —— 类注释（`community/kernel/src/main/java/org/neo4j/kernel/impl/index/schema/TokenScanKey.java:22` 起）写明：`each key consists of tokenId and entityIdRange, i.e. entityId/rangeSize`；
- **Value** `TokenScanValue{long bits;}` —— 类注释（`community/kernel/src/main/java/org/neo4j/kernel/impl/index/schema/TokenScanValue.java:23`）写明 `A small bit set of maximum 64 bits`，窗口大小由同文件 `:26` 的 `RANGE_SIZE = Long.SIZE` 定为 **64**。

```
索引条目 (tokenId, idRange)  →  1 个 long（64 bit）
                                  bit k = 1  ⇔  节点 (idRange*64 + k) 带此 label
```

**空间收益**：朴素方案「一个 (labelId, nodeId) 键」要 8 字节 key；位图方案把 64 个节点的成员关系压进 8 字节 value，一个条目覆盖 64 个节点。对「label 命中率 1/64」量级的标签，索引体积近似压缩 **64 倍**，且一次 cache line（8 字节）读能同时判 64 个节点。

### 26.3 遍历算法：跳窗 + 抽位

`TokenScanValueIndexProgressor` 把 GBPTree 的顺序扫描包装成「实体 id 递增游标」。核心在 `community/kernel/src/main/java/org/neo4j/kernel/impl/index/schema/TokenScanValueIndexProgressor.java:132`：

```java
private long extractNextId(int relevantBitPos) {
    long bitToFlip = 1L << relevantBitPos;
    assert (bits & bitToFlip) != 0;
    bits -= bitToFlip;                 // 清位，保证 next() 不会重复吐出同一 bit
    return baseEntityId + relevantBitPos;
}
```

主循环 `next()`（`:115`）是标准的「清空当前窗口 → 取下一窗口」：

```
next():
  while bits != 0:
      id = extractNextId(trailingZeros(bits))     // 最低置位
      if id ∈ range and client.acceptEntity(id):  return true
  if !nextRange(): return false                   // 游标推进到下一个 idRange 窗口
  bits = cursor.value().bits; baseEntityId = idLayout.firstIdOfRange(key.idRange)
```

`nextRange()` 在 `:139`，取窗口的两行是 `:150` 与 `:151`。**每吐出一个实体 id 的代价 = O(1)**（一次最低位提取 + 一次安全过滤），**每跳一个窗口的代价 = O(log N)**（GBPTree `Seeker.next()`）。所以一次标签扫描的总体代价是：

```
T(labelScan) = O(⌈命中节点数 / 64⌉ × log N_tree + 命中节点数)
```

即「**窗口内 O(1)、跨窗口 O(log N)**」的混合复杂度——比逐节点查表 O(k log N) 好了 `log N / 64` 倍。

### 26.4 skipUntil：长短腿策略

多标签交集（§26.6）需要「把落后的游标快进到目标 id」，靠 `skipUntil(id)`。这里的优化在 `TokenScanValueIndexProgressor.java:164`：

```java
if (id - baseEntityId > RANGE_SIZE * 10) {
    // if we need to take a long stride in tree
    cursor.reinitializeToNewRange(new TokenScanKey(tokenId, idLayout.rangeOf(id)), ...);
```

**阈值 = 10 个窗口 = 640 个 id**。短跳（<640）只清当前窗口的位；长跳直接 `reinitializeToNewRange` 重定位 GBPTree 游标，避免「逐位 scan 过几百个空窗口」。这是典型的**长短腿双算法**。

### 26.5 并行化：partitionedSeek 按 id 区间切分

标签扫描要喂给并行运行时，于是扫描被切成互不相交的 **id 区间分区**。`DefaultTokenIndexReader.java:136` 一次性把整棵树的键空间切成 `desiredNumberOfPartitions` 段：

```java
partitionEdges = index.partitionedSeek(fromInclusive, toExclusive, desiredNumberOfPartitions, cursorContext);
```

返回的是一串分界 `TokenScanKey`，分区数 = `edges.size() - 1`（`:150`）。每个工作线程通过 `reservePartition`（`:154`）用 `AtomicInteger.getAndIncrement()` 原子地领走一段，串行地做 `index.seek(from, to)`。

**这就是 Cypher 并行执行里「morsel」的实体层实现**：分片单位是 **节点 id 区间**而非固定行数，因此各分片的负载取决于标签在该 id 区间的密度——对 id 局部聚集的图数据，分区天然不均衡，这是该设计的已知代价。

### 26.6 事务状态归并：read-committed 的正确性核心

`MATCH (n:Person)` 的结果集 = **store 里的位图索引 ∪ 本事务内新增的带该 label 的节点 − 本事务内删除的**。归并逻辑集中在 `community/kernel/src/main/java/org/neo4j/kernel/impl/newapi/DefaultEntityTokenIndexCursor.java`。

**准备阶段**（`:98` 起）：只有事务确有改动时才建归并结构——

```java
if (txStateHolder.hasTxStateWithChanges()) {
    added   = peekable(createAddedInTxState(txStateHolder.txState(), token, order));
    removed = createDeletedInTxState(txStateHolder.txState(), token);
    useMergeSort = order != IndexOrder.NONE;
}
```

两个集合的构造在子类 `DefaultNodeLabelIndexCursor.java:59` 与 `:64`：`added` 取 `txState.nodesWithLabelChanged(token).getAdded()`；`removed` 是 `addedAndRemovedNodes().getRemoved()` 与 `nodesWithLabelChanged(token).getRemoved()` 的**并集**（两类删除都要屏蔽）。

**有序扫描走归并，无序扫描走拼接**（`:225` 的 `nextWithOrdering()` 与 `:201` 的 `nextWithoutOrder()`）。归并版每轮各从两条流各取一个元素，交给 `PrimitiveSortedMergeJoin`（`:247`）产出下一个；任一条流先耗尽就落到另一条。无序版更简单：先吐 `added`，再吐 store 里的。

**删除过滤是 O(1) 的哈希判断**，见 `:256`：

```java
private boolean isRemoved(long reference) {
    return removed != null && removed.contains(reference);
}
```

`removed` 是个 `LongSet`，且子类里用 `.asUnmodifiable()` 冻结（`DefaultNodeLabelIndexCursor.java:68`），所以热路径无拷贝、无并发写。

**排序是免费的**：`sortTxState`（`:260`）在 `ASCENDING/DESCENDING` 时把 `LongSet` 冻结成 `long[]` 再正向/反向迭代；`NONE` 时直接用 `longIterator()`。也就是说**事务内新增节点的有序扫描靠一次数组化排序完成**，而不是插入排序。

### 26.7 多标签：排序归并而非求交

`(n:A&B)` / `(n:A|B)` / `(n:A&!B)` 三个谓词分别对应三个游标类，实现在 `community/cypher/runtime-util/src/main/java/org/neo4j/internal/kernel/api/helpers/`。交集游标的归并主循环在 `community/cypher/runtime-util/src/main/java/org/neo4j/internal/kernel/api/helpers/IntersectionNodeLabelIndexCursor.java:113`：

```java
for (int i = 0; ; ) {
    compare(firstReference, secondReference);
    if (compare == 0)        { i++; if (i == length - 1) return true; }   // 全游标对齐 = 命中
    else if (compare < 0)    { advanceAllUpTo(i, secondReference); i = 0; } // 落后的全快进
    else                     { second.skipUntil(firstReference); second.next(); }
}
```

因为每个游标都是 **id 递增** 的，交集退化成经典的 **k 路有序归并 + galloping 快进**（`skipUntil` 即 §26.4 的长短腿）。升降序只需换 `compare`（`:200` 与 `:211` 两个子类分别用 `Long.compare` 和其相反数）。总代价 O(Σ 标签的扫描代价)，**没有任何一步会去读节点记录**——谓词筛选完全发生在 id 层。

## 二十七、参考资料

1. Neo4j 官方文档 — `https://neo4j.com/docs/`（含 Developer / Cypher / Operations / GDS Manual）。
2. Neo4j 源码 — `https://github.com/neo4j/neo4j`（Java + Scala，构建系统 Maven）。
3. openCypher 项目与 Cypher 规范 — `https://opencypher.org/` 与 `https://github.com/opencypher/cypher-spec`。
4. Neo4j Graph Data Science 库 — `https://github.com/neo4j/graph-data-science`（图算法实现，PIP Apache 2.0）。
5. Apache Lucene HNSW（基础） — `https://lucene.apache.org/core/`（Neo4j 向量索引底层）。
6. LDBC SNB 基准 — `https://ldbc.github.io/ldbc_snb_docs/`（Linked Data Benchmark Council）。
7. "Graph Databases"（O'Reilly）— `https://www.oreilly.com/library/view/graph-databases/9781449356282/`。
8. Raft 论文 — `https://raft.github.io/raft.pdf`（Ongaro & Ousterhout）。
9. Neo4j Operations Manual — `https://neo4j.com/docs/operations-manual/current/`。
10. Neo4j GenAI 集成 — `https://neo4j.com/labs/genai-ecosystem/`（LLM & RAG 集成参考）。
