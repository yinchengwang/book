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

## 二十四、NebulaKey 与属性值的字节级编码

> 本节源码基于 `reference/graph/nebula/` 的实际 clone（HEAD = `cdef57e5f`，issue #6150 修复后的主干）。这一版的目录结构与 3.x 已明显不同：`NebulaKeyUtils` 位于 `src/common/utils/`（**不是** `src/common/codec/`，后者只放行级编解码器 `RowWriterV2` / `RowReaderV2` / `NebulaCodecImpl`）。

### 24.1 4 字节前缀：type(1) + partId(3)

Nebula 的所有数据键都以一个 4 字节的 `int32_t` 打头。key 类型枚举定义在 `reference/graph/nebula/src/common/utils/Types.h:14`，共 9 种：

```
kTag_=1  kEdge=2  kIndex=3  kSystem=4  kOperation=5
kKeyValue=6  kVertex=7  kPrime=8  kDoublePrime=9
```

其中 `kPrime` / `kDoublePrime` 是 TOSS 两阶段提交协议专用的影子键 —— 写锁成功记 `kPrime`，收到对端 RPC 回包后再记 `kDoublePrime`（注释见 `reference/graph/nebula/src/common/utils/Types.h:22`）。

打包方式是把分区号左移 8 位再与类型或运算，位移量定义在 `reference/graph/nebula/src/common/utils/Types.h:57`：

```cpp
static constexpr uint8_t kPartitionOffset = 8;
static constexpr uint32_t kTypeMask = 0x000000FF;   // Types.h:61
```

以 tag key 为例，`reference/graph/nebula/src/common/utils/NebulaKeyUtils.cpp:40`：

```cpp
int32_t item = (partId << kPartitionOffset) | static_cast<uint32_t>(NebulaKeyType::kTag_);
key.append(reinterpret_cast<const char*>(&item), sizeof(int32_t)) ...
```

头文件里的布局注释写作 `type(1) + partId(3) + vertexId(*) + tagId(4)`（`reference/graph/nebula/src/common/utils/NebulaKeyUtils.h:15`）。这个「1 字节 type + 3 字节 partId」的描述只在小端序视角下成立：代码是 `memcpy` 一个宿主 `int32_t`，小端机器上字节 0 恰好是低 8 位（即 type），字节 1~3 是分区号的高 24 位。

**由此可推出一条硬约束**：`reference/graph/nebula/src/common/utils/NebulaKeyUtils.h:117` 的反解把 4 字节读回后右移 8 位取分区号 ——

```cpp
static PartitionID getPart(const folly::StringPiece& rawKey) {
  return readInt<PartitionID>(rawKey.data(), sizeof(PartitionID)) >> 8;
}
```

也就是说分区号只有 24 bit 有效，**单个 space 的分区数上限为 2^24 − 1 = 16 777 215**。默认 100 个分区（见 §7.1）离这个上限极远，但这条边界是编码本身决定的，调整 `partition_num` 时值得记住。

前缀扫描的上下界由两个常量函数给出：`reference/graph/nebula/src/common/utils/NebulaKeyUtils.cpp:21` 的 `firstKey` 用 `'\0'` 填充，`reference/graph/nebula/src/common/utils/NebulaKeyUtils.cpp:29` 的 `lastKey` 用 `'\377'` 填充 —— 后者必须用最大字节值，才能覆盖「前缀相同、剩余部分最大」的所有键。

### 24.2 三类实体 Key 的拼接

| 实体 | 布局 | 构造入口 | 除 vid 外固定长度 |
|------|------|---------|-------------------|
| Vertex | `partId\|kVertex(4)` + vid 定长填充 | `reference/graph/nebula/src/common/utils/NebulaKeyUtils.cpp:77` | 0 |
| Tag 行 | `partId\|kTag_(4)` + vid + tagId(4) | `reference/graph/nebula/src/common/utils/NebulaKeyUtils.cpp:37` | `kTagLen = 4+4` |
| Edge | `partId\|kEdge(4)` + srcId + type(4) + rank(8) + dstId + 版本位(1) | `reference/graph/nebula/src/common/utils/NebulaKeyUtils.cpp:52` | `kEdgeLen = 4+4+8+1` |

`kTagLen` / `kEdgeLen` / `kSystemLen` 三个常量定义在 `reference/graph/nebula/src/common/utils/Types.h:48` 起，注释写得很直白：「size of tag key except vertexId」。

两个关键细节：

1. **vid 补齐是定长的**，不足 `vIdLen` 的用 `pad`（默认 `'\0'`）右侧填充。这让前缀扫描能按字节序工作，同时 key 长度可预测。
2. **边 key 尾部有 1 字节版本位**，常量在 `reference/graph/nebula/src/common/utils/NebulaKeyUtils.h:314`：`kLockVersion = 0`、`kEdgeVersion = 1`。锁键与边键因此在同一命名空间内共存而不冲突 —— 加边时先写「锁键」（版本位 0），提交后再转为「边键」（版本位 1），转换函数是 `reference/graph/nebula/src/common/utils/NebulaKeyUtils.h:271` 的 `toEdgeKey`。

排序键（ranking）的编码是这一节最值得学的一处，见 24.3。

### 24.3 顺序保持编码：让 RocksDB 的字节序等于数值序

边 key 需要按 ranking 排序，索引 key 需要按属性值做范围查询 —— 但 RocksDB 只按字节序（memcmp）排序，不会理解 int64 或 double。Nebula 的做法是**把数值无损映射成保序的字节串**，这样一次 KV 前缀扫描就等价于一次有序区间查询，无需外挂任何排序结构。

**符号翻转 + 大端**（`reference/graph/nebula/src/common/utils/IndexKeyUtils.h:173`）：

```cpp
static std::string encodeInt64(int64_t v) {
  v ^= folly::to<int64_t>(1) << 63;   // 取反最高位（符号位）
  auto val = folly::Endian::big(v);   // 转为大端
  ...append(reinterpret_cast<const char*>(&val), sizeof(int64_t));
}
```

头文件 `reference/graph/nebula/src/common/utils/IndexKeyUtils.h:165` 起给了对照表：`0 → "\200\000\000\000\000\000\000\000"`、`1 → "\200\000…\001"`、`-1 → "\177\377…\377"`、`INT64_MAX → "\377\377…\377"`。取反符号位把有符号全序映射到无符号全序，大端则保证「高位字节先比」，两者合起来才得到正确的全序。

边的 ranking 走的是完全相同的编码，只是复用了 `NebulaKeyUtils` 自己那份：`reference/graph/nebula/src/common/utils/NebulaKeyUtils.h:245` 的 `encodeRank`。

**double 的保序变换**（`reference/graph/nebula/src/common/utils/IndexKeyUtils.h:219`）三分支：

```cpp
if (std::isnan(v))      return std::string(sizeof(double), '\xFF');  // NaN 排在最前
else if (v >= 0)        { auto val = big(v); c[0] |= 0x80; ... }    // 正数：置最高位为 1
else                    { auto x = (int64_t*)&v; *x = ~(*x); ... }  // 负数：按位取反
```

这是 IEEE-754 的标准 order-preserving trick：正数的最高位恒为 0，置 1 后与负数（取反后最高位恒为 1）落在互不重叠的区间，而各自区间内部的位模式顺序恰好等于数值顺序。`encodeDouble` 与 `NebulaKeyUtils::encodeRank` 只差一个最高位操作，因为浮点的符号位本来就在最高位。

**NULL 值也必须保序**：`reference/graph/nebula/src/common/utils/IndexKeyUtils.h:71` 的 `encodeNullValue` 按类型的固定宽度填满 `0xFF`。因为 `0xFF…` 是各类型编码的最大值，NULL 天然排在该列的最末尾，语义上是 `NULL` 排序在最后（与 SQL 默认一致）。

**索引键的完整拼接**（`reference/graph/nebula/src/common/utils/IndexKeyUtils.cpp:87`）：

```
[partId|kIndex:4][indexId:4][encodeValue(属性值)][vid 定长填充]
                                                 ↑ 同样的 key 可对应多个 vid → 天然支持非唯一索引
```

`reference/graph/nebula/src/common/utils/IndexKeyUtils.cpp:109` 的边索引在中间多插一段 `encodeRank(rank)`，布局变为 `[索引值][srcId][rank][dstId]`。两个函数都返回一个 `vector<string>` 而不是单个字符串 —— 因为**一对多索引列**（如地理位置索引经 `encodeGeography` 展开成多个 S2CellId）会为同一行生成多条键（`reference/graph/nebula/src/common/utils/IndexKeyUtils.h:322`）。

**写放大的定量结论**：插入 1 条边，若该边类型上有 M 个索引列且都是非唯一索引，则产生 **1 + M** 条 KV 写入；索引列若参与多值展开（如 GEOGRAPHY 的 S2 覆盖格子数 C），则为 `1 + M×C`。这解释了 §8.7 / §22.7 所述的写放大，也解释了为何 `CREATE INDEX` 默认是异步任务。

### 24.4 属性值行格式：RowWriterV2

tag 行 / 边行的 value 是一段自描述的属性行，由 `RowWriterV2` 产出（`reference/graph/nebula/src/codec/RowWriterV2.h`）。它相对 V1 的核心改进是**每个属性定长**。

**头字节**（`reference/graph/nebula/src/codec/RowWriterV2.h:41`）：

```
Version 2:   0 0 0 0 1 v v v
             ^^^^^^^ 保留  ^ 版本=1   ^^^ schemaVersion 占用的字节数(0~7)
```

**定长带来的复杂度收益**（`reference/graph/nebula/src/codec/RowWriterV2.h:47`）：给定属性序号，减去前面所有属性的定长宽度即可在 **O(1)** 得到该属性的起始偏移 —— V1 需要解析「块偏移表」才能定位，是 O(属性数)。各类型宽度同文件列出：BOOL/INT8=1B、INT16=2B、INT32/FLOAT/DATE=4B、INT64/DOUBLE/TIMESTAMP=8B、DATETIME=15B、GEOGRAPHY=8B。

**NULL 用位图而非分支**（`reference/graph/nebula/src/codec/RowWriterV2.h:72`）：一 bit 表示一个可空属性是否为 NULL，字节数公式为

```
numNullBytes = ((可空属性数 - 1) >> 3) + 1
```

对应成员变量 `numNullBytes_` 在 `reference/graph/nebula/src/codec/RowWriterV2.h:221`，位操作是 `setNullBit` / `clearNullBit` / `checkNullBit`（`reference/graph/nebula/src/codec/RowWriterV2.h:239`）。

**整体字节序**（`reference/graph/nebula/src/codec/RowWriterV2.h:81`）：

```
<header:1B> <schemaVersion:0~7B> <NULL flags:0+ B> <定长属性区:N B> <变长字符串内容:M B>
```

只有 STRING 是变长的：前 4 字节存内容偏移、后 4 字节存长度，内容统一追加到末尾。改写变长字符串时若原地放不下，靠 `outOfSpaceStr_` 标记把旧字符串挪进 `strList_` 间接层（`reference/graph/nebula/src/codec/RowWriterV2.h:231`），保证其余定长属性的偏移不变。

## 二十五、Meta 侧的分区 Leader 均衡算法

> ⚠️ 目录更正：这一版**没有** `src/meta/heartbeat/` 目录，也**没有** `partSchedule` / `partitionScheduleV2` 这两个符号。均衡逻辑在 `src/meta/processors/job/` 下，被拆成三个执行器：`LeaderBalanceJobExecutor`（leader 分布）、`DataBalanceJobExecutor`（副本分布）、`ZoneBalanceJobExecutor`（zone 感知）。

### 25.1 Meta 自身的键空间

Meta 服务把集群元数据放在自己的 KVStore 里，键构造集中在 `reference/graph/nebula/src/common/utils/MetaKeyUtils.h`：

| 键 | 入口 | 说明 |
|----|------|------|
| space | `reference/graph/nebula/src/common/utils/MetaKeyUtils.h:75` | `spaceKey(spaceId)`，前缀扫描用 `spacePrefix()`（`:81`） |
| 分区副本分配 | `reference/graph/nebula/src/common/utils/MetaKeyUtils.h:87` | `partKey(spaceId, partId)`，前缀 `partPrefix(spaceId)`（`:101`） |
| 分区→主机列表 value | `reference/graph/nebula/src/common/utils/MetaKeyUtils.h:107` | `parsePartVal(val, parNum)`，有 V1/V2 两代编码 |
| leader 记录 | `reference/graph/nebula/src/common/utils/MetaKeyUtils.h:157` | `leaderKey(spaceId, partId)`，前缀 `leaderPrefix(spaceId)`（`:163`） |

均��� executor 正是靠「前缀 + 值里编码的 peer 列表」重建全局拓扑。`reference/graph/nebula/src/meta/processors/job/LeaderBalanceJobExecutor.cpp:58` 取 `partPrefix(spaceId)`，`reference/graph/nebula/src/meta/processors/job/LeaderBalanceJobExecutor.cpp:60` 发起一次前缀迭代，把每个分区解析出的 `HostAddr` 列表反向散列成 `hostParts[host] → [partId...]`。

### 25.2 触发条件：偏差带而非精确均衡

最朴素的做法是「谁多给谁」，但这会让 leader 在每次触发时反复搬家。Nebula 用一个**偏差带**把容忍度参数化。阈值定义在 `reference/graph/nebula/src/meta/processors/job/LeaderBalanceJobExecutor.cpp:13`：

```cpp
DEFINE_double(leader_balance_deviation, 0.05,
              "after leader balance, leader count should in range "
              "[avg * (1 - deviation), avg * (1 + deviation)]");
```

非 zone 感知模式下，容忍区间按下式计算（`reference/graph/nebula/src/meta/processors/job/LeaderBalanceJobExecutor.cpp:377`、`:384`）：

```
activeSize = 有效主机数
globalAvg  = leaderParts / activeSize          // 整除，向下取整
globalMin  = ceil(leaderParts / activeSize × (1 − δ))
globalMax  = floor(leaderParts / activeSize × (1 + δ))
```

注意 `ceil` 用在下界、`floor` 用在上界 —— **宁可让每台机器多担一点，也不要低于下界**，因为欠载比超载更难在下一轮补回来。若 `localMinTmp > localMaxTmp`（主机数很少、δ 相对太大时会发生），代码回退到 `[avg, avg]`（`reference/graph/nebula/src/meta/processors/job/LeaderBalanceJobExecutor.cpp:365`）。

zone 感知模式改为逐主机计算，分母换成该主机持有的**副本数** `localParts` 而非 leader 数（`reference/graph/nebula/src/meta/processors/job/LeaderBalanceJobExecutor.cpp:353`）：

```
localAvg = localParts / replicaFactor
localMin = ceil(localParts / replicaFactor × (1 − δ))
localMax = floor(localParts / replicaFactor × (1 + δ))
```

### 25.3 候选分区筛选：差集而非全量

欠载主机要从别处「抢」一个 leader 过来，但它只能抢**自己持有副本、当前却不是 leader** 的分区 —— 否则那个分区会落到一台没有副本的机器上。`reference/graph/nebula/src/meta/processors/job/LeaderBalanceJobExecutor.cpp:448` 正是集合差：

```cpp
std::set_difference(allHostParts[target].begin(), allHostParts[target].end(),
                    leaderHostParts[target].begin(), leaderHostParts[target].end(),
                    std::back_inserter(diff));
```

前提是两个集合都已按 partId 有序（KV 前缀迭代天然产出有序序列）。差集把候选集从「本机所有副本分区」缩小到「本机 follower 分区」，是这一步的主要复杂度来源。

### 25.4 转移方向与配额限制

拿到候选分区后还要决定**从哪里抢**。`reference/graph/nebula/src/meta/processors/job/LeaderBalanceJobExecutor.cpp:459` 起遍历该分区的 peer 列表，逐个检查：跳过自己和非活跃主机；源必须是该分区当前 leader；且源的负载高于下界 —— 最后一条实现在 `reference/graph/nebula/src/meta/processors/job/LeaderBalanceJobExecutor.cpp:470`：

```cpp
auto it = std::find(sourceLeaders.begin(), sourceLeaders.end(), partId);
if (it != sourceLeaders.end() && minLoad < sourceLeaders.size()) {
  sourceLeaders.erase(it);
  targetLeaders.emplace_back(partId);
  plan.emplace_back(spaceId, partId, source, target);
```

`minLoad < sourceLeaders.size()` 是防止连锁反应的**配额闸门**：源机一旦降到下界就不再往外让，避免 A 让给 B、B 又让给 A 的震荡。抢到即 `break`，且外层在 `reference/graph/nebula/src/meta/processors/job/LeaderBalanceJobExecutor.cpp:483` 复查 `targetLeaders.size() == minLoad` 就收手 —— 补到下界就停，不贪心。

超载方向对称：让出时选**负载最低的 peer** 作为接手方，`reference/graph/nebula/src/meta/processors/job/LeaderBalanceJobExecutor.cpp:510` 用 `std::min_element` 在 peer 列表上求最小。

### 25.5 收敛性与任务编排

外层是一个 `while (true)` 反复扫描所有主机（`reference/graph/nebula/src/meta/processors/job/LeaderBalanceJobExecutor.cpp:398`），直到「全部在带内」或「本轮一个任务都没产出」为止（`reference/graph/nebula/src/meta/processors/job/LeaderBalanceJobExecutor.cpp:429`）。轮次上界是分区数 P，每轮 O(主机数 + 本轮迁移数)，最坏 O(P²)，实践中因偏差带的存在通常一两轮就收敛。

产出的任务装进 `BalancePlan`（`reference/graph/nebula/src/meta/processors/job/BalancePlan.h:22`）。`reference/graph/nebula/src/meta/processors/job/BalancePlan.h:51` 逐个 `addTask`，`reference/graph/nebula/src/meta/processors/job/BalancePlan.h:110` 的 `dispatchTasks()` 按目标主机分桶，从而把多个分区的 leader 转移**并行**下发给不同 storaged。

## 二十六、存储读路径的游标扫描与 Raft 快照

### 26.1 三类读请求的入口

`src/storage/query/` 下按请求类型各一个 processor：`ScanVertexProcessor`、`ScanEdgeProcessor`、`GetPropProcessor`、`GetNeighborsProcessor`、`GetDstBySrcProcessor`；`src/storage/kv/` 另有面向裸 KV 的 `GetProcessor` / `PutProcessor` / `RemoveProcessor`；`src/storage/index/LookupProcessor.cpp` 承担 `LOOKUP ON` 的索引路径。

`limit` 的「无上限」约定在 `reference/graph/nebula/src/storage/query/ScanVertexProcessor.cpp:32`：负数代表不限流，直接取 `int64_t` 最大值。分区数较多时展开为多线程并行，`reference/graph/nebula/src/storage/query/ScanVertexProcessor.cpp:177` 为每个分区准备独立的 `DataSet` 与游标语料：

```cpp
cursorsOfPart_.resize(req.get_parts().size());
for (size_t i = 0; i < req.get_parts().size(); i++) {
  nebula::DataSet result = resultDataSet_;
  results_.emplace_back(std::move(result));
```

**每个分区一个游标**是这套设计的关键：分区之间无共享状态，因此既可并行，又能在某分区失败时只重试该分区（游标本身就是重试凭证）。

### 26.2 顶点扫描：前缀扫描 + 顶点级聚合 + 游标推进

核心执行节点是 `ScanVertexPropNode`（`reference/graph/nebula/src/storage/exec/ScanNode.h:20`），它的 `doExecute`（`reference/graph/nebula/src/storage/exec/ScanNode.h:58`）是全 wiki 里最值得逐行读的一段。逻辑骨架：

```cpp
std::string prefix = NebulaKeyUtils::tagPrefix(partId);          // ScanNode.h:65
std::string start  = cursor.empty() ? prefix : cursor;           // 游标为空 → 从前缀起扫
auto kvRet = context_->env()->kvstore_->rangeWithPrefix(
    spaceId_, partId, start, prefix, &iter, enableReadFollower_); // ScanNode.h:73

for (; iter->valid() && resultDataSet_->rowSize() < rowLimit; iter->next()) {
  auto tagId   = NebulaKeyUtils::getTagId(vIdLen, key);           // ScanNode.h:86
  auto vertexId= NebulaKeyUtils::getVertexId(vIdLen, key);        // ScanNode.h:91
  if (vertexId != currentVertexId && !currentVertexId.empty()) {
    collectOneRow(isIntId, vIdLen, currentVertexId);             // vid 变了 → 上一顶点收尾
  }
  currentVertexId = vertexId;
  tagNodes_[tagIdIndex->second]->doExecute(key.toString(), value.toString());
}
if (resultDataSet_->rowSize() < rowLimit) collectOneRow(..., currentVertexId); // 收尾

cpp2::ScanCursor c;
if (iter->valid()) c.next_cursor_ref() = iter->key().str();        // ScanNode.h:112
cursors_->emplace(partId, std::move(c));
```

三个设计点：

1. **为什么按 `kTag_` 前缀扫而不是 `kVertex` 前缀**：顶点自身的 tag 列表行（`kTag_`）与顶点行（`kVertex`，`reference/graph/nebula/src/common/utils/NebulaKeyUtils.cpp:77`）是分开的两套键。扫 `kTag_` 前缀能在**一次有序扫描**内拿到一个顶点的全部 tag 属性，无需二次点查。
2. **顶点级聚合**：同一顶点的多个 tag 行在 RocksDB 里是连续的（key 以 vid 排序），所以循环里按「vid 是否变化」切分，一行结果需要多个 `TagNode` 依次填列（`reference/graph/nebula/src/storage/exec/ScanNode.h:132` 起的 `collectTagPropsIfValid`）。这也意味着**只扫 tag 前缀会漏掉「没有任何 tag 数据」的顶点** —— 与 §7.1 的存储模型一致。
3. **游标语义**：`iter->valid()` 为真时游标 = 当前 key（**尚未消费**的那个），否则不设游标表示扫完了。配合 `reference/graph/nebula/src/storage/exec/ScanNode.h:99` 的提前 `break`，断点续扫恰好从断点所在顶点重新开始，**不丢行也不重复行**。

### 26.3 filter 下推在存储层执行

谓词不是拉回 graphd 再过滤，而是在 storaged 上边扫边判。`reference/graph/nebula/src/storage/exec/ScanNode.h:175` 是分叉点：

```cpp
if (filter_ == nullptr) {
  resultDataSet_->rows.emplace_back(std::move(row));
} else {
  auto result = QueryUtils::vTrue(filter_->eval(*expCtx_));
  if (result.ok() && result.value()) resultDataSet_->rows.emplace_back(std::move(row));
}
```

`expCtx_` 是逐行构造的表达式上下文，用完立即 `expCtx_->clear()` 并清空所有 `TagNode`（`reference/graph/nebula/src/storage/exec/ScanNode.h:184`），保证下一行不串味。

注意 `limit` 与 `filter` 的**先后顺序**：`rowLimit` 比较的是 `resultDataSet_->rowSize()`，而 `rows` 只在 filter 通过后才追加（`reference/graph/nebula/src/storage/exec/ScanNode.h:83` 的循环条件）。所以 `limit` 约束的是**过滤后的结果行数**，过滤掉的行不消耗配额 —— 这正是 §9.3 中 `PushFilterDown` 规则想要的效果，且不会因为高选择率谓词而提前截断。

### 26.4 Raft 快照：按前缀分层 + 分区限速

follower 落后太多时 leader 不再逐条补日志，而是发全量快照。快照不是「整个 RocksDB 目录打包」，而是**按 key 前缀分层遍历**，前缀清单在 `reference/graph/nebula/src/common/utils/NebulaKeyUtils.cpp:250`：

```cpp
std::vector<std::string> NebulaKeyUtils::snapshotPrefix(PartitionID partId) {
  if (partId == 0) {                       // meta 自身的快照
    result.emplace_back("");                // 全量
  } else {                                 // 数据分区的快照
    result.emplace_back(vertexPrefix(partId));
    result.emplace_back(tagPrefix(partId));
    result.emplace_back(edgePrefix(partId));
    result.emplace_back(IndexKeyUtils::indexPrefix(partId));
    result.emplace_back(kvPrefix(partId));
    // kSystem will be written when balance data
    // kOperation will be blocked by jobmanager later
  }
```

**partId == 0 是 meta 的特例**，全量扫描；数据分区只取五类前缀，刻意排除 `kSystem`（分区搬迁时才写）和 `kOperation`（由 JobManager 管控、属于集群状态而非图数据）。这样得到的快照可以被新节点直接接管，同时把运维态键与数据态键隔离开。

一致性与限速分三层：

1. **视图一致性**：`reference/graph/nebula/src/kvstore/NebulaSnapshotManager.cpp:45` 先取 RocksDB 原生快照句柄并全程持有，后续多次回调都基于同一视图，不会把并发写入撕裂进去。
2. **日志水位**：`reference/graph/nebula/src/kvstore/NebulaSnapshotManager.cpp:54` 从 `systemCommitKey(partId)` 读出 `commitLogId` / `commitLogTerm`，随快照一起发给 follower。follower 据此确定从哪个日志位置接着补 —— 这是快照与增量日志的**衔接点**。
3. **速率控制**：`reference/graph/nebula/src/kvstore/NebulaSnapshotManager.cpp:15` 定批大小 `snapshot_batch_size = 512 KB`；累计到批大小后，`reference/graph/nebula/src/kvstore/NebulaSnapshotManager.cpp:122` 向限速器 `consume` 掉这批字节：

```cpp
rateLimiter->consume(batchSize,
                     FLAGS_snapshot_part_rate_limit,   // rate
                     FLAGS_snapshot_part_rate_limit);  // burstSize
```

单分区每秒上限由 `snapshot_part_rate_limit` 控制（`reference/graph/nebula/src/kvstore/NebulaSnapshotManager.cpp:12`）。集群总速率因此被限定为 `worker 线程数 × 单分区上限`（同文件 `:23` 的注释）。这个设计很重要 —— 若不限速，一个落后严重的 follower 触发的全量快照会瞬间打满 leader 的网络与磁盘，反过来拖慢正常业务读写。

### 26.5 心跳、选举与超时

心跳与选举在 `src/kvstore/raftex/RaftPart.cpp` 里，与 §11.2「每个 partition 一个 Raft Group」一一对应 —— 每个 `RaftPart` 有自己独立的心跳定时器与选举状态机。

心跳间隔 `reference/graph/nebula/src/kvstore/raftex/RaftPart.cpp:30`：

```cpp
DEFINE_uint32(raft_heartbeat_interval_secs, 5, "Seconds between each heartbeat");
```

leader 侧的发心跳条件极简（`reference/graph/nebula/src/kvstore/raftex/RaftPart.cpp:1140`）：必须 `status_ == RUNNING && role_ == LEADER` —— 即**状态不健康时连心跳都不发**，让 follower 超时更快触发选举，加速故障切换。

follower 侧触发选举的条件是「距上次收到消息 ≥ 5 秒」或「自己是 blind follower」（`reference/graph/nebula/src/kvstore/raftex/RaftPart.cpp:1146`）：

```cpp
return status_ == Status::RUNNING && role_ == Role::FOLLOWER &&
       (lastMsgRecvDur_.elapsedInMSec() >= FLAGS_raft_heartbeat_interval_secs * 1000 ||
        isBlindFollower_);
```

定时器间隔本身带随机抖动以防惊群（`reference/graph/nebula/src/kvstore/raftex/RaftPart.cpp:1411`）：

```cpp
size_t delay = FLAGS_raft_heartbeat_interval_secs * 1000 / 3 + folly::Random::rand32(500);
```

即基础 1.67s + `[0, 500)ms` 随机量 —— 远小于 5s 的超时阈值，保证正常心跳不会误触发选举。选举失败后进入 `[500, 2000)ms` 的随机退避（`reference/graph/nebula/src/kvstore/raftex/RaftPart.cpp:1419`），并连续尝试两轮选主（`reference/graph/nebula/src/kvstore/raftex/RaftPart.cpp:1413`）。

快照相关超时是 `reference/graph/nebula/src/kvstore/raftex/RaftPart.cpp:32` 的 `raft_snapshot_timeout = 300s`（默认 5 分钟），用于清理过期的待发送快照。

## 二十七、参考资料

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
