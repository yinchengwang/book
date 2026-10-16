# NebulaGraph Wiki（国产分布式原生图数据库）

> 参考版本：NebulaGraph 3.x（C++ 实现，核心 KV 引擎 RocksDB，meta 服务基于 Raft，存储引擎基于 Nebula KV，查询语言 nGQL）。本 wiki 重点覆盖图类型专项章节（共享无状态架构、RocksDB + Raft、nGQL 双语言融合、向量索引 + 大模型集成）。

## 一、项目定位

NebulaGraph（曾用名 NebulaGraph，由 **杭州悦数科技有限公司 / Vesoft Inc.**）开源的 **分布式原生图数据库（Distributed Native Graph Database）**，以 **"杭州图数据库"** 与 **"make graph powerful with the connected data"** 为口号，提供 **存算分离（Storage + Meta + Graph 三层）架构**、**nGQL（Graph SQL/Compat）查询语言 + 向 Cypher 兼容**、**集群规模 100+ 节点**、**向量索引**（RAG 场景），是国内图数据库生态的代表实现。

- **痛点**：
  - 单实例图数据库（Neo4j）难以承载 PB 级别图数据；
  - 国产化趋势下需要自控图数据库；
  - 反欺诈 / 推荐系统 / 知识图谱需要"高吞吐 OLTP + 大图遍历"；
  - GraphRAG 等场景要求"图 + 向量"双索引；
  - 多机房 + 跨区域容灾需要分布式；
  - 运维门槛低（无侵入式部署 + 成熟 Operator）；
- **典型用户**：金融反欺诈、知识图谱、推荐系统、社交网络、电信运营商、券商、电力能源、智慧城市、GraphRAG、知识中台；
- **核心口号**：**"Connected data, made easy."** + **"Open Source, Distributed, Performant Graph Database"** + **"AS A Native Graph Database"**。

## 二、发展历史与社区

- **起源**：2018 年底 Vesoft Inc. 创立，2019 年发布 1.0 alpha，2020 年初 1.0 GA；
- **里程碑**：
  - 2018-12：NebulaGraph 1.0 alpha，存算分离架构：Graph / Meta / Storage 三角色；
  - 2019-06：1.0 预发布版发布；
  - 2019-12：1.0 GA（RocksDB-based 存储 + RAFT 一致性）；
  - 2020：1.0.x 系列稳定，Cassandra 后端支持被移除（仅 RocksDB）；
  - 2021：2.0 GA，加入 `MATCH` （兼容 Cypher 部分）、`LOOKUP` 索引、Spark Connector、GraphX 集成；
  - 2022：2.5+，JDBC、Go driver、Java driver 完善、Dashboard（Studio）；
  - 2022-2023：2.6/3.0 加强运维：备份工具 nebula-br、Helm Chart、Operator 成熟；
  - 2023：3.0 GA，向量索引（ANN）、全文索引；
  - 2024：3.4/3.6 LLM 集成（GP-Tree、LLM-Pathway）、Cluster Manager 工具、AI Suite；
  - 2025：3.7/3.8 大规模优化（DISTINCT、subgraph、视图）；
- **维护状态**：活跃，每月发布版本；
- **社区**：GitHub 10k+ star（含所有子项目），NebulaGraph 中文社区论坛、微信群、知识星图；
- **生态**：与 **大模型生态（OpenAI / Hugging Face / LangChain / LlamaIndex）** 集成；与 **Apache Spark、Flink、Kafka、Maxwell** 集成；与 **Spring Data Graphx、Element-UI Nebula Studio** 集成。

## 三、许可与商业限制

- **核心代码**：**Apache License 2.0**；
- **第三方组件**：
  - RocksDB（Facebook）：Apache 2.0 / GPLv2 双协议（Meta 项目）；
  - gRPC / Protobuf：Apache 2.0；
  - Folly / libevent：Apache 2.0；
  - brpc：Apache 2.0（百度）
- **商业版本**：
  - **Vesoft NebulaGraph Enterprise**：闭源商业版，含 Navigator 图可视化、数据导入工具、商业支持、Studio 增强版；
  - **NebulaGraph Cloud**：全托管云服务（Vesoft 提供）；
- **生态关系**：
  - 与 **LangChain / LlamaIndex** 提供 GraphRAG Retriever；
  - 与 **Apache Spark / Flink / Kafka** 提供 ETL / CDC；
  - 与 **Apache HBase / Cassandra** 提供外部 KV 后端的对比样例（Nebula 用 RocksDB 自带）；
  - 与 **Element-UI / Streamlit / vesoft-studio** 提供可视化。

## 四、业界主要使用方与案例

- **大型用户**（公开）：
  - **快手**：实时推荐、社交网络；
  - **美团**：餐饮知识图谱与配送网络；
  - **小红书**：内容反作弊与推荐；
  - **网易**：游戏反作弊、知识图谱；
  - **360**：安全图谱；
  - **平安集团**：金融反欺诈 + 知识图谱；
  - **中国移动**：运营商网络拓扑；
  - **国家电网**：电力知识图谱；
  - **同济大学 / 清华**：知识图谱研究；
- **行业**：社交电商、金融反欺诈、电信运营商、电力、智慧城市、知识图谱、推荐系统；
- **典型场景**：风控反欺诈（多跳借贷关系图）、知识图谱（实体关系抽取）、GraphRAG（RAG + 知识图谱）、推荐系统（用户-物品图）、IT 资产图谱、电信网络拓扑。

## 五、一句话对比定位

> NebulaGraph 是 **"国产 / 分布式原生图数据库（RocksDB 自带 KV + Raft）+ 存算分离 + nGQL 兼容部分 Cypher + 向量索引 + LangChain GraphRAG 集成 + 中文社区活跃"**；与 **Neo4j（专有存储 + 商业化最深）**、**ArangoDB（多模型 BSL）**、**JanusGraph（适配器型）**、**Memgraph（兼容 Cypher + C++）**、**Dgraph（GraphQL 优先）**、**HugeGraph（蚂蚁/百度开源）**、**TuGraph（蚂蚁商业）** 构成图数据库阵营。NebulaGraph 的差异化是 **"国产 + 分布式 + RocksDB + 中文社区 + 存算分离 + LangChain 集成"**。

## 六、整体架构

```
┌────────────────────────────────────────────────────────────────────┐
│                            客户端层                                  │
│  - nebula-console（C++ REPL，与 broker / tool 同包）                │
│  - Drivers: Go / Python / Java / C++ / Rust / Node.js              │
│  - HTTP API（Graph 服务的 HTTP 接口）                                │
│  - nebula-graph-studio（Web UI）                                   │
└─────────────────────┬──────────────────────────────────────────────┘
                      │ Thrift / gRPC（Nebula 二进制协议）
┌─────────────────────▼──────────────────────────────────────────────┐
│                       Graph 服务（无状态）                          │
│  src/graph/                                                       │
│  ├─ Service Layer（GraphService.cpp）                              │
│  ├─ Session 管理（SessionManager.cpp）                              │
│  ├─ Planner / Optimizer / Validator                                │
│  │    ├─ parser/        nGQL 词法 + 语法                          │
│  │    ├─ planner/       Logical → Physical plan                   │
│  │    └─ optimizer/     rule-based + cost-based                   │
│  ├─ Executor（executor/）                                          │
│  │    ├─ executor/      Volcano-style 节点                        │
│  │    └─ visitor/       表达式求值 AST visitor                    │
│  ├─ Scheduler       调度到 Meta/Storage                            │
│  └─ Storage Client  与 Meta/Storage 通信                          │
└─────────────────────┬──────────────────────────────────────────────┘
                      │
┌─────────────────────▼──────────────────────────────────────────────┐
│                       Meta 服务（强一致）                           │
│  src/meta/                                                        │
│  ├─ MetaServiceImpl     (Thrift handler)                          │
│  ├─ MetaClient          同步 metad 到 Graph                       │
│  ├─ ClusterManager      添加/删除 Space/Partition                 │
│  ├─ 强一致：kvstore::MetaClient → Raft 协议                       │
│  └─ 备份 / Snapshot（基于 Nebula 自带 raft snapshot）            │
└─────────────────────┬──────────────────────────────────────────────┘
                      │
┌─────────────────────▼──────────────────────────────────────────────┐
│                       Storage 服务（多副本 Raft）                    │
│  src/storage/                                                     │
│  ├─ StorageService   处理 AddVertices/Edges/GetNeighbors 等        │
│  ├─ Exec / Executor                                             │
│  │   ├─ AddVerticesExecutor  Edge / Vertex 持久化                │
│  │   ├─ GetNeighborsExecutor 图遍历（关键性能点）                 │
│  │   ├─ ScanVertex / ScanEdge 范围扫描                          │
│  │   └─ IndexByScanExecutor    索引扫描                          │
│  ├─ KVStore 抽象层（src/kvstore/）：                               │
│  │    ├─ nebula::KVStore       RocksDB / Memory                  │
│  │    ├─ RocksDBEngine         生产实现                          │
│  │    └─ RAFT                  Multi-Raft 协议（自研）            │
│  ├─ Index（含向量 / 全文 / Lookup）                               │
│  └─ Compaction / GC (rocksdb + Nebula 自定义)                     │
└─────────────────────┬──────────────────────────────────────────────┘
                      │
┌─────────────────────▼──────────────────────────────────────────────┐
│                       工具 / 客户端                                  │
│  src/console/   REPL 工具                                          │
│  src/clients/   Python/Java/Go/C++ 客户端库                          │
│  src/tools/     import / backup / parse / replay                   │
│  src/interface/ Thrift IDL                                         │
└────────────────────────────────────────────────────────────────────┘
```

### 关键设计取舍

- **存算分离三层架构**：Graph（无状态）+ Meta（Raft） + Storage（多副本 Raft）→ 各角色独立扩展；
- **自家 raft**：核心自研 KV + RAFT 引擎（src/kvstore/raft/），不必依赖外部 Raft 实现；
- **RocksDB 作为底层 KV**：每个 partition 一个 RocksDB 实例，WAL + raft log + 主键索引；
- **Schema 优先**：space / tag / edge type / index 等在 Meta 中强一致；
- **查分阶段**：parser → planner → optimizer → executor → 与 storage 通信 → 结果聚合；
- **Lookup / Match**：nGQL 同时有"路径语法 + 类 Cypher MATCH"两条路径，对应不同查询场景；
- **向量索引**（3.x）：使用 HNSW（`src/storage/index/`）+ 与图属性混合查询。

## 七、存储引擎

### 7.1 整体存储格式

- **Space** = 租户（namespace）；
- 每个 Space 切若干 **Partition**（默认 100）；
- 每个 Partition 内部 RocksDB 存：
  - **Vertex**：`vid → {tags + props}`；
  - **Edge**：`srcVid + edgeType + ranking → {dstVid + props}`；
  - **Index**：在 vid-keyed 列族中存储索引 entries。

### 7.2 Vertex ID（vid）

- 自定义类型：`int64 / string / fixed_string`；
- 自带 **hash** 切片，避免字符串比较 → hash to partition。

### 7.3 边存储

- 按 srcVid 划分 → 同源多边按 edgeType + ranking 排布；
- 一个 edge 行 = `key : srcVid + typeId + ranking`，`value : dstVid + props`；
- 双方向存储：额外存反向索引 `outEdge ↔ inEdge`（内嵌 reverse 索引）。

### 7.4 索引

- 标签索引（Tag Index）：`vid → property value`；
- 边类型索引（Edge Index）：相似；
- Lookup 索引：标量值索引；
- 向量索引：`src/storage/index/` 基于 HNSW（用 hnswlib 的变体）；
- 全文索引：基于 ES 的 puli（Nebula 自带）或 Elasticsearch External。

### 7.5 压缩与保留

- RocksDB 提供 ZSTD / Snappy 压缩；
- Nebula 提供 partition TTL（`TTL_DURATION`）；
- 部分 large properties 可独立存到 KV（off-row，避免 bloated rows）。

### 7.6 备份与恢复

- 在线备份：`nebula-br`（Nebula Backup & Restore）工具；
- 全局 snapshot + 各 partition RocksDB backup；
- 数据格式：meta + storage 三层一致性点；
- 支持**`leader + follower`**两阶段：先把 meta snapshot 存，再串 backup storage partitions。

## 八、索引机制

### 8.1 图存储模型

NebulaGraph 走 **"按 srcVid 分片 KV + Edge 双方向"** 路线，是现代工业图数据库的代表：

| 维度 | NebulaGraph（KV + 双方向 Edge） | Neo4j（指针直连） | ArangoDB（双表） |
|------|--------------------------------|------------------|----------------|
| 节点 | RocksDB row | 定长 slot | Document |
| 边 | srcVid+typeId+ranking 双方向 | 双向链表 | Edge Collection |
| 遍历 | 同 partition 内 O(1) KV 跳 | O(1) | JOIN |
| 跨 partition | 跨 raft group | N/A | 跨 shard |
| 副本 | Multi-Raft | 单实例 / 多主 Raft | Sync/Async |

### 8.2 标签索引（Tag Index）

```
CREATE TAG INDEX IF NOT EXISTS player_index_0 ON player(name(20))
SHOW TAG INDEXES
REBUILD TAG INDEX
```

- 自动产生 B+ Tree-like KV entries（在 index 列族中）；
- 倒排链：`indexValue + vid → tag_table`；
- 查询：`LOOKUP ON player WHERE player.name == "Tim Duncan"`。

### 8.3 边类型索引（Edge Type Index）

```
CREATE EDGE INDEX IF NOT EXISTS follow_index_0 ON follow(degree)
```

- 用于反向边（in-edges）上的过滤。

### 8.4 Lookup 索引

- 非主索引、可建在 edge / tag 多列；
- 查询：`LOOKUP ON tag WHERE …`、`LOOKUP ON edge WHERE …`；
- 写入放大可控。

### 8.5 向量索引（3.x）

- `CREATE VECTOR INDEX <name> ON TAG <tag>(<prop>) DIMENSION <n> TYPE hnsw`;
- 后端为基于 hnswlib 的变体（src/storage/index/vector/）；
- 查询：`LOOKUP ON TAG WHERE vec.c1 NEAR([...] ORDER BY similarity) LIMIT k`；
- 与 RAG 结合：`nGQL + AI 函数 = GraphRAG`。

### 8.6 全文索引

- 基于 Elasticsearch 外部 + Community 全文；
- 或集成 **Puli** 嵌入式全文；
- 查询：`MATCH … WHERE … YIELD doc.fulltext("…")`。

### 8.7 索引代价

- 索引越多写放大越严重；
- 索引 rebuild 可在线，`REBUILD TAG INDEX …` 不阻塞查询；
- 向量索引 rebuild 时间长（建议离线）；
- 索引 slot（partition 一致 hash）对齐分析：稀疏 partition 容易产生 hot partition。

## 九、查询/计算/接口层

### 9.1 nGQL 查询语言

nGQL 是 NebulaGraph 的类 SQL 图查询语言，融合自 SQL、Cypher、Property Graph 风格：

```sql
-- Match（类 Cypher）
MATCH (v:player)-[e:follow]->(w:player)
WHERE v.name == "Tim Duncan"
RETURN v, e, w

-- Lookup（标量）
LOOKUP ON player WHERE player.age > 30 YIELD id(vertex) AS id, player.name AS name

-- Go（路径）
GO FROM "player100" OVER follow YIELD dst(edge) AS dst

-- Fetch（属性）
FETCH PROP ON player "player100" YIELD properties(vertex).name

-- 路径函数
FIND SHORTEST PATH FROM "player100" TO "player200" OVER follow

-- SubGraph
GET SUBGRAPH WITH PROP FROM "player100" BOTH EDGE follow YIELD VERTICES AS nodes, EDGES AS relations

-- 二选一
MATCH p = SHORTESTPATH(("a"})-[:follow*1..5]->("b")) RETURN p

-- 向量查询
LOOKUP ON doc WHERE doc.embedding NEAR([0.1, 0.2, 0.3, ...]) LIMIT 5
```

### 9.2 nGQL 解析

- `src/parser/`：基于 flex/bison 的解析器；
- AST → 抽象语法树（`graph::QueryContext`）；
- 输出 plan。

### 9.3 Plan / Optimizer

- **Logical Plan** → **Physical Plan** → **Executor**（Volcano）；
- Optimizer 规则示例：
  - `PushFilterDown`：条件推 below join；
  - `PushLimitDown`：limit 下推；
  - `PruneColumns`：列裁剪；
  - `MergeAdjacentProjects`：合并同类 project；
  - `RewriteIndexScan`：将 LOOKUP 改写为 index scan。

### 9.4 Executor

- `src/graph/executor/` 各 ExecutionNode；
- 主要 node：
  - `GetNeighborsNode` → 拉邻居（**性能关键点**）；
  - `IndexScanNode` → 标量/向量索引扫；
  - `FilterNode` → 谓词下推；
  - `LoopNode` → 多次循环；
  - `DedupNode` → 去重；
  - `TopNode` / `LimitNode` → 截断；
  - `ProjectNode` → 投影；
  - `AggregateNode` → 聚合；
  - `JoinNode` / `HashLeftJoinNode` → 各类 join。

### 9.5 跨 Partition 处理

- Executor 通过 **`partitionId mapping`** 路由请求；
- 跨 partition 时 fetch / gather 数据 → 聚合节点；
- "SubGraph with cap" 等查询需 bring all partition 数据。

### 9.6 接口与协议

- **Thrift**：Nebula 内置协议（`src/interface/`）；
- **HTTP**（GraphService HTTP）：REST-like 端口；
- **gRPC**（实验性）：部分服务支持 gRPC；
- **Drivers**：Go / Python / Java / C++ / Rust / Node.js 全部官方驱动；
- **Studio**（Web UI）：可视化 + nGQL 编辑器。

### 9.7 图算法库

- `nebula-algorithm`：基于 Spark / Flink 的常用算法包；
  - PageRank、LPA（Label Propagation）、Louvain、Connected Components、Shortest Paths、Node2Vec、GraphSAGE；
  - 集群 BSP 实现；
- `Hubble` / `Explorer`：可视化路径与子图；
- **GraphX Spark 集成**：用 Spark GraphX 跑图算法 → 全量 OLAP。

### 9.8 GraphRAG / AI 集成

- `nGQL` + AI Function：`MATCH … YIELD AI.SUMMARY(text)`、`AI.EMBEDDING(text)`；
- 与 LangChain 集成：NebulaGraphQAChain 检索器（基于图的 Hybrid Search）；
- 与 LlamaIndex 集成：NebulaGraphStore / NebulaGraphReader；
- 与 HuggingFace / OpenAI 提供 `Embedding Generator` 一键生成向量；
- LLM-Pathway：在图内做"组合路径"作为上下文。

## 十、事务与一致性

### 10.1 事务模型

- NebulaGraph OLTP 仅提供 **Session 级事务**：
  - `BEGIN / COMMIT / ROLLBACK` (阿里 API 风格?)；
  - ACID 仅限同一个 partition / leader；
  - 跨 partition 不支持 distributed transaction（不提供 2PC）；
- 部分用例可走 optimistic concurrency：CAS version。

### 10.2 一致性

- 默认 Raft majority commit；
- Read 可以走 leader（linearizable read），或 weak follower（best-effort）；
- 一致性由 `storage.read_consistency` 参数控制。

### 10.3 锁与冲突

- Nebula 不依赖外部 Lock Manager；
- 写入时基于 Raft linearizability 自然序列化；
- Tag / Edge 修改使用 `GetNeighbors` 类似 CAS（dow_id + vid）。

### 10.4 隔离级别

- 默认 Read-Committed；
- 部分模式支持 Snapshot Isolation（lock-time 同步读）。

## 十一、复制与高可用

### 11.1 集群角色

- **Graph 服务**：无状态，多副本水平扩展；
- **Meta 服务**：Raft 集群（默认 3 节点）；
- **Storage 服务**：每个 partition 一组 Raft（默认 3 副本）。

### 11.2 多 Raft

- 每个 partition 内一个 Raft Group；
- Leader 上写、Followers 上读；
- Partition 迁移可重切分（rebalance）。

### 11.3 跨数据中心

- 通过 Storage 多 AZ 部署；
- Meta Service Raft 集群在同 AZ（保持 quorum）；
- HDFS / Object Storage 存放 snapshot；
- 跨数据中心副本受限于延迟。

### 11.4 集群脑裂避免

- Meta + Storage 都以 Raft 避免脑裂；
- 节点间心跳 + 选举；
- 同时多个 partition 之间 leader 解耦。

### 11.5 客户端发现

- Nebula Meta 服务回传路由表（partitions + leaders）；
- 客户端缓存 → 失败时主动刷新；
- 集群扩容时自动重路由。

## 十二、扩展性

- **水平扩展**：增删 Storage 节点 → 自动 rebalance partition；
- **垂直扩展**：单机内存、heap size 受 RocksDB 配置限制；
- **冷热分层**：实验性，大体量 partition 可拆为 hot/warm archive；
- **集群规模**：生产验证 100+ 节点、亿级边、十亿级向量；
- **动态扩缩容**：在线 add/remove partition，client 自动重路由。

## 十三、性能与基准

- **LDBC SNB**：NebulaGraph 在 SF=100 上 5w+ QPS（混合 OLTP/OLAP）；
- **内部 Bench**：Nebula 仓库 `tests/bench/` 提供 micro-benchmark（traversal / match / lookup）；
- **典型数字**：
  - 邻居查询（1-hop，O(1)）：<1ms（batch）；
  - 多跳 5-hop：10-100ms（千万级）；
  - 向量 ANN（10w vec）：<10ms；
  - 写入：5w+ insert/s。
- **对比 Neo4j**：Nebula 分布式吞吐 > Neo4j 单点；图遍历时间相近（千万级）；向量 ANN Nebula 自带。

## 十四、运维与可观测性

- **监控**：`metaserve: 19559, graph: 19669, storage: 19779` Prometheus / StatsD；
- **慢查询**：`graph_slow_log` 配置文件 → 阈值；
- **Dashboard**：`nebula-studio` Web UI；
- **部署**：`nebula-up`（Docker Compose 一键）、`nebula-operator`（K8s Operator）、Helm；
- **备份**：`nebula-br` 全量 + 增量；
- **升级**：集群 rolling upgrade；
- **Tracking**：内部 trace 服务（`src/common/trace`）；
- **资源**：`docs.nebula-graph.com.cn` 中文文档、FAQ / troubleshoot / performance tuning。

## 十五、生态与集成

- **客户端**：
  - Python：`nebula3-python`；
  - Go：`nebula-go`；
  - Java：`client-java`；
  - C++：同源；
  - Rust：`nebula-rs`；
  - Node.js：`nebula-nodejs-driver`；
- **数据导入**：
  - `nebula-importer`：CSV/JSON bulk load；
  - `exchange`：Spark Source → Nebula；
  - `exchange-1.x`：支持 Kafka / Hive / MySQL → Nebula；
- **CDC**：`exchange-debezium-cdc`；
- **BI / 可视化**：`nebula-studio` + `hubble` + `GraphScope`；
- **容器化**：
  - `vesoft/nebula-graphd`、`vesoft/nebula-metad`、`vesoft/nebula-storaged` 官方 Docker；
  - `vesoft/nebula-operator`（K8s Operator）；
  - Helm chart；
- **AI / GenAI**：
  - `nebula-ai-services`：AI 函数包装；
  - `nebula-langchain`（社区 / Vesoft）：LangChain Retriever；
  - `nebula-llm`：`llm-pathway` 多路径推理；
  - 与 HuggingFace、OpenAI、Google Vertex 提供 embedding；
- **第三方**：
  - 与 Apache Spark / Flink 集成；
  - 与 Apache HBase / Cassandra / MySQL ETL；
  - 与 Tableau / Power BI（自定 connector）。

## 十六、核心论文与算法原理

- **Raft 论文**：Ongaro & Ousterhout, "In Search of an Understandable Consensus Algorithm"（USENIX ATC 2014）；
- **HNSW 论文**：Malkov & Yashunin, "Efficient and robust approximate nearest neighbor search using Hierarchical Navigable Small World graphs"（IEEE TPAMI 2018）；
- **Multi-Raft / Partitioned Raft**：王选 + Nebula 团队博客描述；
- **ROCKSDB 论文**：Dong et al., "Evolution of Development Priorities in Key-value Stores"（VLDB 2017）；
- **LDBC Social Network Benchmark 规范**：`https://ldbc.github.io/ldbc_snb_docs/`；
- **存算分离**：与 CockroachDB / Spanner / TiKV 思路类似，但专为图设计；
- **nGQL + 兼容 Cypher**：参考 Cypher 规范与 GQL 草案。

## 十七、源码学习路径

| 阶段 | 模块 | 路径 | 要点 |
|------|------|------|------|
| 入门 | GraphService 入口 | `src/graph/service/` `GraphService.cpp` | 服务入口 |
| 入门 | Console | `src/console/` `NebulaConsole.cpp` | REPL |
| 进阶 | Parser / AST | `src/parser/` `Parser.yy` `Lexer.l` | nGQL 语法 |
| 进阶 | Planner / Optimizer | `src/graph/planner/` `src/graph/optimizer/` | 规则 + 优化 |
| 进阶 | Executor | `src/graph/executor/` | 执行节点 |
| 进阶 | Session Manager | `src/graph/session/` `SessionManager.cpp` | 会话 |
| 高级 | Meta Service | `src/meta/` `MetaServiceImpl.cpp` | Raft 一致集群 |
| 高级 | Storage Service | `src/storage/` `StorageService.cpp` | 持久化 |
| 高级 | KV Store / Raft | `src/kvstore/` `Raft.cpp` | 自研 Raft |
| 高级 | Index | `src/storage/index/` | HNSW、Lookup |

## 十八、源码规模与开发语言

- **LOC**：约 100 万行 C++（核心）；
- **语言占比**：
  - C++ ≈ 92%；
  - Python（驱动 / bench / tools）≈ 3%；
  - Go / Java / Rust（驱动 / 集成）≈ 4%；
  - Shell / YAML / 文档 ≈ 1%；
- **构建系统**：CMake（顶层 `CMakeLists.txt`、`src/CMakeLists.txt`）；
- **C++ 标准**：C++14 / C++17；
- **编译依赖**：Thrift / RocksDB / gRPC / Protobuf / brpc / Folly / glog；
- **CI**：GitHub Actions + NebulaCloud 自研流水线；
- **代码风格**：`docs/coding_style.md`（clang-format 配 + 自定义规则）。

## 十九、学习资源

- **官方文档**：
  - `https://docs.nebula-graph.com.cn/` 中文（强烈推荐）；
  - `https://docs.nebula-graph.io/` 英文；
  - `https://docs.nebula-graph.io/3.x/getting-started/`；
  - `https://docs.nebula-graph.io/3.x/nGQL/`；
- **书籍**：
  - "图数据库：理论与实践"（机械工业出版社，王浩）；
  - "NebulaGraph 实战"（电子工业出版社）；
  - "图数据库：从入门到实战"；
- **视频 / 课程**：NebulaGraph 官方 Bilibili（China）、NebulaGraph YouTube、网易云课堂"NebulaGraph 入门与实战"；
- **社区**：NebulaGraph 中文论坛、知乎 NebulaGraph 标签、GitHub Discussions、钉钉 / 微信群；
- **实战项目**：`example` 子项目、各类 hackathon 案例（`github.com/vesoft-inc/nebula-spark-connector`）、知乎 NebulaGraph 标签；
- **在线 sandbox**：`playground.nebula-graph.io`（交互式 Demo）；

## 二十、可移植性与部署

- **支持平台**：Linux（x86_64 / ARM64）、macOS（x86_64 / ARM）、Docker、CentOS / Ubuntu / Debian；
- **K8s**：`vesoft/nebula-operator` v1alpha1，CRD 实现扩缩容 / 升级 / 备份；
- **Helm chart**：社区 vesoft/nebula-helm；
- **Docker Compose**：`vesoft/nebula-docker-compose`：3 角色 launcher；
- **ARM 平台**：Nebula 3.0+ 支持 ARM（飞腾、华为鲲鹏等）；
- **嵌入式**：官方不提供嵌入式模式 → 如需嵌入式图，参考 **Kuzu / Memgraph**。

## 二十一、变体与衍生项目

- **NebulaGraph Enterprise**：商业闭源 + Navigator；
- **NebulaGraph Cloud**：Vesoft 商业云；
- **NebulaGraph Studio / Explorer / Dashboard**：可视化；
- **Nebula-Hubble**：跨集群巡检工具；
- **Nebula-LLM**：与 LLM 集成链；
- **Nebula GraphX / Spark Connector**：Spark Source；
- **历史**：与 Oldboy、蚂蚁 HugeGraph、华为/招商 TuGraph 形成国产图数据库族群；
- **关联项目**：Weaviate（向量库）与 Nebula 可联合使用（Hybrid Search）。

## 二十二、风险与陷阱

### 22.1 存算分离的运维痛点

- 三类服务（Graph / Meta / Storage）独立部署 → 部署 + 监控复杂；
- 扩容 / 缩容 / 滚动升级多角色协调；
- 小项目维护成本反而较高（推荐 ≥3 节点）；
- 网络抖动易触发 leader 选举，THP 抖动需优化 OS。

### 22.2 RocksDB 参数调优

- RocksDB 默认参数不适配 NVMe/SSD 大盘 → 可能踩 IO 抖动；
- 一个 partition 一个 RocksDB 实例 → 大量 partition 内存占用高；
- 调优点：`rocksdb_db_options.write_buffer_size`、`max_total_wal_size`、列族压缩；
- Compaction 内存压力（`write_buffer_size * max_write_buffer_number`）；
- Nebula 调优文档 `https://docs.nebula-graph.io/3.x/5.appendix/rdb-config/`。

### 22.3 跨 Partition 查询

- 跨 partition 涉及 gather → 没有 local locality；
- `MATCH` 大范围遍历或 `FIND SHORTEST PATH` 易踩热点 partition；
- 建议：图数据按 `vid` 顺序设计，让 zone-aware 切片更优。

### 22.4 Schema 修改慢

- Schema 走 meta service → Raft 一致；
- 大量 ADD / ALTER TAG 改动阻塞写入；
- 建议：批量化 + 灰度。

### 22.5 nGQL 与 Cypher 兼容性

- nGQL ≠ Cypher，部分 Cypher 高级模式不支持（如更深 path 表达式、EXISTS subqueries、OPTIONAL MATCH 部分语义）；
- 迁移老 Cypher 需调整语法；
- 推荐"按 nGQL 特性"。

### 22.6 向量索引容量与构建

- 向量索引独立列族 → 大内存；
- 1 亿向量 HNSW 约 200-500 GB 内存；
- 重建 HNSW 时间长 → 离线、滚动；
- 与 Neo4j 5.x 一样需要 GPU/大量内存支持大规模向量检索。

### 22.7 索引代价

- Tag Index / Edge Index 默认全量 → 写放大；
- 写入延迟上升；
- 建议：prod-env 创建 incremental 索引、按需 alter。

### 22.8 数据迁移与备份

- `nebula-importer` 适合空库 → 在群组数据迁移必须谨慎；
- `nebula-br` 一致性快照基于 RocksDB Snapshot + Meta 的 snapshot；
- 跨版本兼容：`format_version` 检查；
- 推荐使用 `nebula-exchange` + Spark 把老图数据导入 NebulaGraph。

### 22.9 LLM / GraphRAG 落地

- AI 函数增加 query latency → 优化调用（批量化 + 缓存）；
- Embedding 模型需与向量维度一致；
- GraphRAG 性能由图遍历 + 向量索引共同决定；
- 适合场景：知识抽取、混合检索、客服问答；
- 不适合场景：超大规模全文 + LLM 实时生成。

## 二十三、与本项目的关联

- **存算分离架构**：本项目 `engineering/` 中 KV 与 WAL 解耦的设计思路与 Nebula 一致；
- **自研 KV + Raft**：本项目若实现 KV 集群，可借鉴 Nebula 的多 Raft Group 模式；
- **多角色调度**：类似本项目"主二进制 + 库"的多模块拆分；
- **nGQL 风格**：本项目如未来扩展图，可参考 nGQL 设计（兼容 SQL / Cypher 双语法）；
- **向量索引**：本项目多模态中"HNSW + 图"组合 → Nebula 是工程样板；
- **Operator**：`vesoft/nebula-operator` 作为 K8s Operator 模板。

## 二十四、参考资料

1. NebulaGraph 官方文档 — `https://docs.nebula-graph.io/` 与 `https://docs.nebula-graph.com.cn/`。
2. NebulaGraph 源码 — `https://github.com/vesoft-inc/nebula`。
3. NebulaGraph 3.x Release Notes — `https://github.com/vesoft-inc/nebula/releases`。
4. RocksDB Wiki — `https://github.com/facebook/rocksdb/wiki`。
5. Raft 论文 — `https://raft.github.io/raft.pdf`。
6. HNSW 论文（IEEE TPAMI） — `https://arxiv.org/abs/1603.09320`。
7. Apache Arrow 文档（Nebula Studio 集成） — `https://arrow.apache.org/`。
8. LangChain NebulaGraph 集成 — `https://python.langchain.com/docs/integrations/graphs/nebula_graph/`。
9. Nebula Performance Tuning — `https://docs.nebula-graph.io/3.x/5.appendix/rdb-config/`。
10. LDBC SNB Benchmark — `https://ldbc.github.io/ldbc_snb_docs/`。
