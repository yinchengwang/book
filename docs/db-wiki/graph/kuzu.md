# Kuzu Wiki（嵌入式 OLAP 图数据库）

> 参考版本：Kuzu 0.x / master（C++17 实现，进程内嵌模式，自研列存 + 邻接表存储，Cypher 兼容查询，向量化执行）。本 wiki 重点覆盖图类型专项章节（嵌入式图数据库、自研存储、邻接表 + 列存融合、Cypher 兼容性、向量索引、跨语言绑定）。

## 一、项目定位

Kuzu 是由 **Kùzu Inc.**（多伦多 / 大湾区）开源的 **嵌入式 OLAP 图数据库（Embedded Graph OLAP Database）**，以 **"the fastest in-process graph database"** 与 **"built for analytical graph workloads"** 为口号，提供 **嵌入式进程链接**（`libkuzu.so` / `kuzu.ios`）、**Cypher 兼容查询（Neo4j TCK 通过率高）**、**向量化执行 + 自研列存 + 邻接表（Adjacency Lists）** 三合一存储，是 DuckDB / SQLite 风格"嵌入式 + 高性能 OLAP"的图数据库代表。

- **痛点**：
  - 客户端 - 服务器图数据库（Neo4j、Memgraph、Aura）部署复杂；
  - 嵌入式 KV / SQL（如 DuckDB、SQLite）一般不支持图遍历；
  - 关系型映射（vertex 表 + edge 表）在大图遍历（5+ hop）退化为指数级 JOIN；
  - 图 + OLAP 分析场景需要"向量执行 + 列存"以提升 throughput；
  - 数据科学 / 笔记本工作流（Jupyter）需要本地嵌入式数据库；
  - 与 Python 数据栈（pandas / numpy）配合的图工具缺失；
- **典型用户**：数据科学家、研发 / 数据科学平台、知识图谱研究、本地应用嵌入、GraphRAG 嵌入式后端、推荐系统、欺诈检测 / 调查；
- **核心口号**：**"embedded graph database, performant on OLAP"** + **"the most complete Cypher implementation outside Neo4j"** + **"as performant as DuckDB for graphs"**。

## 二、发展历史与社区

- **起源**：2022 年由多伦多大学的 Kùzu Inc. 创始团队（CTO Andy Hsu, CEO Jin Lim 等）创立。团队前身有 DuckDB 的开发经验 → 工程上借鉴 DuckDB 嵌入式架构；
- **里程碑**：
  - 2022：Kùzu 0.0.1 — 初始发布；
  - 2023：0.0.5 系列 — 多个 Cypher 子句 + 嵌入式 Node.js / Python；
  - 2023-09：0.1.x — Java 绑定 + 性能提升；
  - 2023-12：0.2.x — C API 稳定 + CPU 矢量化（实验性）；
  - 2024：0.3.x — Node / Python / C++ SDK + 向量化执行（部分 Op）+ 性能优化；
  - 2024-10：0.4 — 大量 Cypher 进阶子句 + 矢量化节点（HashJoin、Scan、Filter 等）；
  - 2024-12：0.5.x — Recursive CTE 加强、Property Index、性能、Python 绑定更新（`kuzu`）；
  - 2025：0.6+ — Rust / Go / C# 绑定、向量索引（HNSW 计划）、持久化模式增强；
- **维护状态**：活跃，每周数十 commit；
- **社区**：GitHub 8k+ star、Discord 1.5k+ 用户、Slack、GitHub Discussions、Daily Office Hours、Twitter / X；
- **生态**：Jupyter / PyPI / npm / crates.io 跨语言绑定；与 Pandas / Polars / DuckDB / LangChain 集成。

## 三、许可与商业限制

- **核心代码**：**MIT License**（亲切的开放协议）；
- **官方绑定**：
  - Python：`kuzu`（PyPI）；
  - Node.js：`kuzu`（npm / `npm i kuzu`）；
  - C++ / C：原生头文件与 C API；
  - Rust / Go / C#：社区贡献 + 官方 SDK；
- **第三方依赖**：
  - **ANTLR4**（BSD-3）：Cypher 解析器；
  - **Arrow / Parquet**（Apache 2.0）：数据导入 / 导出；
  - **spdlog / fmt / nlohmann/json**（MIT）：日志 / 序列化；
- **商业版本**：**Kùzu Cloud**（未推出 / 实验性）；
- **生态关系**：
  - 与 **Neo4j Cypher 规范**对齐（TCK 跑分高，通过率超 90%）；
  - 与 **DuckDB / Pandas / Polars** 走"嵌入式 + OLAP"风格一致；
  - 与 **LangChain / LlamaIndex** 提供 GraphRAG / Cypher adapter。

## 四、业界主要使用方与案例

- **大型用户**（公开 / 社区）：
  - **LayerZero**：跨链协议（chain graph）；
  - **Streamlit**：docs 教程 + 案例；
  - **FaunaDB**：图查询参考；
  - **Mapbox / 微软 Bing Maps**：地理网络（社区案例）；
  - **MIT 计算机系 / 哈佛 / CMU**：教学与科研；
  - 国内：多个 AI / 数据科学创业团队；
- **行业**：研究 / 教学、数据科学、跨链协议、网络安全、AI/ML、推荐系统、GraphRAG；
- **典型场景**：本地 GraphRAG、知识图谱研究、推荐系统 mockup、网络 / 路径分析、数据科学笔记本、生物医学图谱嵌入、嵌入式图查询 SDK。

## 五、一句话对比定位

> Kuzu 是 **"嵌入式 OLAP 图数据库（Cypher 兼容 + 自研列存 + 邻接表 + 矢量化执行 + MIT + C++17）"**；与 **DuckDB（嵌入式列存 OLAP）**、**SQLite（嵌入式关系）**、**Memgraph（兼容 Cypher + 服务化）**、**Neo4j（专有存储 + 服务化）**、**NebulaGraph（服务化 + KV）**、**GraphDB（Apache 风格服务端）** 构成图生态。Kuzu 的差异化是 **"嵌入式 + 向量化 + Cypher 完整 + MIT + 列存融合"**。

## 六、整体架构

```
┌────────────────────────────────────────────────────────────────────┐
│                          API / 绑定层                                │
│  src/main/                                                         │
│  ├─ C++ API（直接包含 header）                                     │
│  ├─ C API（extern "C"）                                            │
│  ├─ Python / Node.js / Java bindings（基于 C API）                 │
│  ├─ Rust / Go / C# bindings（社区 + 官方）                         │
│  └─ kuzu::Database / kuzu::Connection                            │
└─────────────────────┬──────────────────────────────────────────────┘
                      │
┌─────────────────────▼──────────────────────────────────────────────┐
│                       解析 & 绑定（Binder）                          │
│  src/parser/                                                       │
│  ├─ antlr4/                Cypher ANTLR 文法                        │
│  ├─ parser.cpp             Cypher → AST                            │
│  └─ expression/            表达式 AST                              │
│  src/binder/                                                        │
│  ├─ StatementBinder       AST → Bound Statement                    │
│  ├─ ExpressionBinder       col-to-table mapping                    │
│  └─ QueryGraph / CypherClause                                │
└─────────────────────┬──────────────────────────────────────────────┘
                      │
┌─────────────────────▼──────────────────────────────────────────────┐
│                       Planner / Optimizer                           │
│  src/planner/                                                     │
│  ├─ Plan / LogicalPlan / LogicalOperator                          │
│  └─ operator/scan / extend / join / recursive-cte / intersect       │
│  src/optimizer/                                                   │
│  ├─ Rewriter / ExprRewrite                                       │
│  ├─ Push-Down Filter / Limit / Projection                        │
│  └─ Statistics-Based 优化                                        │
└─────────────────────┬──────────────────────────────────────────────┘
                      │
┌─────────────────────▼──────────────────────────────────────────────┐
│                       物理执行（Physical Operator）                  │
│  src/processor/                                                    │
│  ├─ operator/             Physical Operator tree                   │
│  │   ├─ persistent/      持久表操作                                │
│  │   ├─ scan/            扫描                                      │
│  │   ├─ extend/          邻居扩展                                  │
│  │   ├─ recursive_extend/递归扩展开                                │
│  │   ├─ hash_join/       Hash Join                                │
│  │   ├─ intersect/       Intersect                                │
│  │   └─ result/          Result collector                          │
│  ├─ Map/Vector-Execution 矢量化执行                                  │
│  └─ pipeline/             多 pipeline 调度                         │
└─────────────────────┬──────────────────────────────────────────────┘
                      │
┌─────────────────────▼──────────────────────────────────────────────┐
│                       表达式求值                                       │
│  src/expression_evaluator/                                          │
│  ├─ VectorEvaluator        矢量化求值                                │
│  ├─ Function               内置函数（aggregate / scalar / structural）│
│  └─ src/function/          函数目录（catalog）                       │
└─────────────────────┬──────────────────────────────────────────────┘
                      │
┌─────────────────────▼──────────────────────────────────────────────┐
│                       存储（Storage）                                │
│  src/storage/                                                      │
│  ├─ storage_manager        DB 物理 → 多个 StorageUnit / WAL         │
│  ├─ buffer_manager         mmap-based page cache                  │
│  ├─ page_manager           Page 级管理                             │
│  ├─ overflow_file          大对象溢出文件                          │
│  ├─ shadow_file            写时复制（MVCC shadow）                 │
│  ├─ free_space_manager     空间回收                                │
│  ├─ stats/                 表 / 列统计信息                          │
│  ├─ compression/           列存压缩                                │
│  │                                                                │
│  ├─ 列存（Columnar）：每个 node / rel table 列族                    │
│  │   └─ 同一表可"按节点 + 邻接表"列存                              │
│  │                                                                │
│  ├─ index/                B+Tree / HNSW / ART / Schema 索引        │
│  │   ├─ B+Tree / ART       标量属性索引                            │
│  │   └─ HNSW               向量索引（部分）                        │
│  ├─ local_storage/        临时本地存储                              │
│  ├─ disk_array            列存 disk array                          │
│  ├─ checkpointer           Snapshot + WAL checkpoint              │
│  └─ transaction/          MVCC 读写事务（WAL 协调）                │
└─────────────────────┬──────────────────────────────────────────────┘
                      │
┌─────────────────────▼──────────────────────────────────────────────┐
│                       文件 & 元数据                                   │
│  src/catalog/             catalog: tables / columns / indices     │
│  src/common/              common types, typesystem                 │
│  src/include/             public headers                          │
└────────────────────────────────────────────────────────────────────┘
```

### 关键设计取舍

- **嵌入式**：`libkuzu` + C API，所有进程内启动，不需要 server；
- **Cypher 兼容**：ANTLR 解析为目标，Neo4j TCK 通过率高；
- **存储融合**：列存（"现代 OLAP"）+ 邻接表（"图遍历"）混合；
- **向量化执行**：scan / filter / join 矢量化；
- **MVCC**：WAL + shadow file + version；
- **多语言 SDK**：通过 C API 暴露，Python / Node / Java / Rust / Go / C# 全支持；
- **MIT 许可**：对学术、商业友好。

## 七、存储引擎

### 7.1 物理存储概览

- 数据库目录 = 多个 WAL + 一个 shadow store + 一组 column files；
- 表 = "Node / Rel table"；
- 每个 Node / Rel table 内由列族组成：主键列（id）+ attribute columns（compressed）+ adjacency list（Node / Rel 之间）+ index 文件；
- 列存与邻接表分离存储。

### 7.2 节点表

- Node table = 一张节点表，列结构：
  - **nodeID**：int64 主键；
  - **属性列**：每个属性一个列族（compressed）；
  - **邻接列（AdjList）**：指向 Rel table 的指针数组；
- 按 nodeID 顺序 append-only 写入；
- B+Tree / Adaptive Radix Tree 索引在 nodeID 上。

### 7.3 边表（Rel Table）

- 边表也采用"主键列 + 属性列 + 列存"；
- 邻接表（Adjacency List）按源节点 / 目标节点分别拆分：
  - FWD adj list（按 source 排序的 target list）；
  - BWD adj list（按 target 排序的 source list）；
- 邻接表用 disk_array + 压缩；
- 双跳（fwd / bwd）用于恒定时间查询"邻居 N"。
- 邻接列族 CSR/CSC 风格的连续存储 → 缓存友好。

### 7.4 列存压缩

- 列存压缩：`min-max`、`dictionary`、`run-length`、`FSST（string）` 等；
- 字符串列存压缩 FSST（Facebook String Sorting Transformation）→ 4x 压缩比；
- 写时压缩 + 读时 lazy decompression。

### 7.5 Buffer Manager & Page

- `BufferManager`：mmap-based page cache；
- Page = 8 KB ~ 32 KB；
- LRU 或 ClockSweep 替换；
- mmap 加快读、冷数据 swap out。

### 7.6 WAL + Checkpoint

- `checkpointer.cpp` 周期创建 checkpoint；
- `overflow_file.cpp` 存大对象；
- `shadow_file.cpp`：shadow file + MVCC 版本；
- WAL 写 → Recovery 重新应用。

### 7.7 事务与版本

- MVCC：read snapshot + version check；
- 写走 commit 时挂起冲突事务（通常是 write-write）；
- 默认 Snapshot Isolation（部分场景 READ_COMMITTED）。
- `src/transaction/` 包括 Begin / Commit / Rollback 抽象。

### 7.8 备份与恢复

- 单文件 + WAL 重放；
- 嵌入式 → 备份直接拷 DB 目录；
- 实验性：在线 snapshot 接口。

## 八、索引机制

### 8.1 图存储模型（邻接表 + 列存）

| 维度 | Kuzu（列存 + 邻接表） | Neo4j（指针直连） | DuckDB（嵌入式列存） |
|------|---------------------|------------------|--------------------|
| 节点 | 列族 + nodeID 索引 | 定长 slot | 表行 |
| 边 | 列族 + Adj List | 双向链表 | 表行（外键） |
| 遍历 | 邻接表 + 列存二次过滤 | 指针直连 | JOIN |
| 索引 | B+Tree / ART / HNSW | 内置 | ART 索引 |
| 持久化 | WAL + checkpoint | mmap + WAL | WAL |

Kuzu 走 **"列存 + Adj List"**：列存压缩降低 I/O，Adj List 让遍历为 O(out-degree) + 列存压缩过滤。

### 8.2 标量索引（ART / B+Tree）

- **B+Tree / ART**（Adaptive Radix Tree）在每列属性上选建索引；
- 唯一索引 vs 非唯一索引；
- 复合索引（composite index）`index_on(nodeId, prop)` 实验性；
- ART 在 `src/storage/index/art_radix.cpp`：自研实现；
- 创建语法：`CREATE INDEX ON MyTable(prop)`。

### 8.3 邻接表索引

- 默认节点 → 邻接表是一组列；
- 二级索引：可建基于 destination 的 B+Tree → 加速 `(B)-[:R]->(A)` 反向遍历。

### 8.4 向量索引

- 0.6+ 实验性 HNSW；
- 后端是 kuzu 自研（在 `src/storage/index/`）；
- Cypher：`CALL db.index.vector.queryNodes("idx", 5, $vec)`（规划中）。

### 8.5 全文索引

- 实验性（基于 Tantivy / Lucene），主要靠 DuckDB / 外部；
- 推荐 Lucene 外部 + 用户端合并。

### 8.6 索引代价

- 索引越多写放大越严重；
- 列存 + 邻接表本身密集 → 索引写时不友好；
- 优化：批量压缩重建。

## 九、查询/计算/接口层

### 9.1 Cypher 查询语言

```cypher
// 节点 + 关系
MATCH (p:Person {name: 'Alice'})-[:KNOWS]->(friend)
WHERE friend.age > 18
RETURN friend.name, friend.age

// OPTIONAL MATCH
MATCH (p:Person)
OPTIONAL MATCH (p)-[:WORKS_ON]->(proj)
RETURN p.name, proj.title

// 可变长度路径
MATCH (a:Person)-[:KNOWS*1..5]->(b:Person)
WHERE a.name = 'Alice'
RETURN DISTINCT b.name

// WITH 子句
MATCH (p:Person)-[:KNOWS]->(friend)
WITH p, COLLECT(friend.name) AS friends
WHERE SIZE(friends) >= 3
RETURN p.name, friends

// 最短路径
MATCH p = shortestPath( (a:Person {name:'Alice'})-[:KNOWS*]->(b:Person {name:'Bob'}) )
RETURN p

// 子查询 + UNION
MATCH (p:Person)
RETURN p
UNION ALL
MATCH (c:Company) RETURN c

// CALL 子程序（图算法）（部分）
CALL db.index.fulltext.queryNodes("idx", "Alice")
```

### 9.2 ANTLR 解析器

- `src/antlr4/` 内嵌 ANTLR4；
- Cypher 文法 → AST；
- 各方言：3.x → 5.x 持续兼容 Neo4j 语法树。

### 9.3 Binder / Planner / Optimizer

- **Binder**：AST → Bound Statement，变量绑定（col-to-table mapping）；
- **Logical Plan**：`src/planner/`：
  - 节点算子：`LogicalScan`、`LogicalExtend`、`LogicalHashJoin`、`LogicalIntersect`、`RecursiveCTE`；
- **Optimizer**：
  - 规则：`PushDownFilter`、`PushDownLimit`、`RewriteOrdinality`；
  - 统计信息 / Cardinality 优化。

### 9.4 Physical Operator / 矢量化

- `src/processor/operator/`：物理节点；
- Vectorized execution：
  - `VectorEvaluator` + Arrow-style `ValueVector`；
  - 矢量化 scan / filter / join / aggregation / intersect；
  - 多个内核并行；
- Pipeline 调度：多算子 pipeline。

### 9.5 图算法

- Cypher 内置：最短路径、Dijkstra、All Pairs shortest；
- 内置算法集合：`dbms.kuzu.shortest_path` 等；
- 用户可在 Cypher 中用 WITH 子句写 BFS / DFS；
- 高级算法：PageRank / Louvain 通过 user-defined procedure（UDF / UDD）；
- 与 NetworkX（Python）互操作（Python 端用 `kuzu` + NetworkX）。

### 9.6 嵌入式客户端

- **C++**：直接用 `kuzu::Database / kuzu::Connection`；
- **C API**：其他绑定的基础；外部嵌入式调用；
- **Python**（pip install kuzu）：同步 API + Jupyter-friendly；
- **Node.js**：npm install；
- **Java**：Maven / Gradle 依赖；
- **Rust**：Cargo crate；
- **Go / C#**：社区贡献。

### 9.7 持久化与事务 API

- 嵌入式：多 connection 可共用同一 Database；
- 事务：`Connection.begin_transaction()` / `commit` / `rollback`；
- 备份：直接拷贝 DB 文件目录；
- 升级：跨小版本兼容；
- 大查询：默认 auto-commit，可显式 transaction。

## 十、事务与一致性

### 10.1 ACID

- 默认 Snapshot Isolation；
- ACID 全实现：atomic write batch + WAL + shadow + MVCC；
- 嵌入式单进程，无需分布一致；
- 跨连接：每个 connection 一个 transaction → 各自 snapshot。

### 10.2 隔离级别

- 默认 `READ_COMMITTED` 显式 / `SNAPSHOT_ISOLATION` 默认；
- 显式：`SET isolation_level=read_committed`；

### 10.3 乐观并发

- OCC → commit-time conflict check；
- `begin_immediate_transaction` 自动重试。

### 10.4 MVCC

- `shadow_file.cpp` 维护历史 row version；
- read snapshot 看到 commit-time version；
- write 进入 buffer → commit 时 apply。

### 10.5 崩溃恢复

- WAL 重放 + checkpoint fallback；
- 启动恢复：先 snapshot 再 replay WAL。

## 十一、复制与高可用

### 11.1 单实例模式

- 嵌入式本地单文件数据库；
- 不支持集群（不算 server mode）。

### 11.2 Multi-process / 文件级

- 每个进程打开同一目录 → MVCC 处理；
- 但同时挂写 → 可能冲突（默认 Snapshot Isolate 隔离）；
- WAL + Shadow 文件 + 文件锁 fsync 协调。

### 11.3 Raft / Replication

- 暂未实现 → 嵌入式模式下不包含；
- 路径：可与外部 coordination（etcd / consul）整合实现 HA；
- 实际场景：读副本 / WAL 推送 + 自分布；
- Kuzu 暂无商业 Enterprise。

## 十二、扩展性

- **垂直扩展**：受主机内存 / disk IO；
- **水平扩展**：暂无内置；
- **典型数据规模**：单实例能装入 100GB-1TB 数据（取决于 colstore 压缩比）；
- **冷热分层**：实验性 `Kuzu.warm_data()`；
- **跨多进程**：通过文件共享；
- **多数据库 Database**：单 Database 内多 schema → 简化的 namespace 隔离。

## 十三、性能与基准

- **Cypher TCK**：当前通过率超 90%，Neo4j 兼容度高；
- **LDBC SNB**：0.5.x 在嵌入式模式下 SF=1 / SF=3 内单核单实例可接近 Neo4j；
- **官方 Bench**：与 NetworkX / igraph 对比 5-100x 加速；
- **典型数字**：
  - 嵌入式单核插入：5w+ insert/s（小批）；
  - 3-hop 遍历：<10ms（10w 节点）；
  - 列存扫描：与 DuckDB 同类比肩（小子数据集 scan 优势大）；
  - 内存：列存压缩比 5-10x。
- **参考**：`https://kuzudb.com/blog/` 与 `https://docs.kuzudb.com/`。

## 十四、运维与可观测性

- **监控**：可通过回调函数自定义 metrics；
- **慢查询**：query profile（实验性）；
- **CLI**：Jupyter-notebook-friendly（Python `kuzu` 包内置 query）；
- **CLI for SQL/Cypher**：实验性 `ku` shell；
- **Docker**：官方 `kuzu/kuzu`，但更多面向 SDK 集成；
- **部署**：嵌入式无 server，部署简单；
- **备份**：CP / MV 备份工具可直接；
- **Tracing / Logs**：spdlog → stdout / file。

## 十五、生态与集成

- **客户端 SDK**：
  - C++：原汁原味；
  - C：API 给其他语言；
  - Python：`kuzu`（PyPI）；
  - Node.js：`kuzu`（npm）；
  - Java：Maven Central `com.kuzudb:kuzu`；
  - Rust：crates.io；
  - Go：实验性；
  - C#：实验性。
- **数据集成**：Apache Arrow（in-memory data exchange）、Pandas、Polars、DuckDB；
- **图算法**：NetworkX（Python）、igraph；
- **可视化**：Neo4j Browser（部分连接能力）、Streamlit、Plotly Dash、yWorks；
- **AI / LLM**：
  - LangChain Kuzu 集成：`kuzu_chain`；
  - LlamaIndex GraphStore / Reader 实验性；
  - GraphRAG：本地 LLMs + Cypher query 嵌入 Kuzu。

## 十六、核心论文与算法原理

- **Cypher 规范**：openCypher / Neo4j 5.x；
- **DuckDB 论文**：Raman et al., "DuckDB: an Embeddable Analytical Database"（SIGMOD 2019）；
- **CWI 邻接表 + 列存**：参考 HyPer / Velox / DuckDB；
- **FSST 字符串压缩**：Boncz & Friedman, "FSST: Fast Random Access String Compressor"（VLDB 2020）；
- **ART 索引**：Leis et al., "The Adaptive Radix Tree: ARTful Indexing for Main-Memory Databases"（ICDE 2013）；
- **MVCC（Snapshot Isolation）**：参考 PostgreSQL、Memgraph 论文。
- **参考链接**：`https://docs.kuzudb.com/` 与 `https://kuzudb.com/blog/`。

## 十七、源码学习路径

| 阶段 | 模块 | 路径 | 要点 |
|------|------|------|------|
| 入门 | Database / Connection | `src/main/` `database.cpp` `connection.cpp` | 入口 |
| 入门 | C API | `src/c_api/` | C ABI |
| 进阶 | Parser / ANTLR | `src/antlr4/` + `src/parser/` | Cypher 文法 |
| 进阶 | Binder | `src/binder/` | 变量绑定 |
| 进阶 | Logical Plan | `src/planner/` | 逻辑计划 |
| 进阶 | Physical Op / Vector | `src/processor/operator/` | 矢量化执行 |
| 高级 | Storage Manager | `src/storage/storage_manager.cpp` | 物理存储 |
| 高级 | Columnar / AdjList | `src/storage/copy/` + 邻接表 | 列存与邻接 |
| 高级 | WAL / Checkpoint | `src/storage/checkpointer.cpp` | 事务持久化 |
| 高级 | Index | `src/storage/index/` | ART / HNSW |

## 十八、源码规模与开发语言

- **LOC**：约 30 万行 C++；
- **语言占比**：
  - C++ ≈ 92%；
  - Python（驱动 / 测试）≈ 4%；
  - Shell / CMake / Java / Rust（绑定）≈ 4%；
- **构建系统**：CMake（顶层 `CMakeLists.txt`、`src/CMakeLists.txt`、`Makefile`）；
- **C++ 标准**：C++17（核心），部分 C++20；
- **依赖**：
  - ANTLR4（parser），目标集成 ANTLR generated；
  - spdlog / fmt / nlohmann / gtest；
  - 无服务端依赖。
- **CI**：GitHub Actions；
- **代码风格**：`https://github.com/kuzudb/kuzu` `.clang-format`。

## 十九、学习资源

- **官方文档**：
  - `https://docs.kuzudb.com/`；
  - `https://docs.kuzudb.com/cypher/` Cypher 兼容；
  - `https://docs.kuzudb.com/developer-guide/` C++ / Python API；
  - `https://docs.kuzudb.com/extensions/` 数据导入 / 导出；
  - `https://docs.kuzudb.com/getting-started/`；
- **书籍**：
  - "Embedded Graph Database with Kuzu"（社区作者）；
  - "Graph Databases: The Definitive Guide"（Neo4j 团队，虽针对 Neo4j 但有助于理解 Cypher）；
- **视频 / 课程**：
  - Kuzu 官方 YouTube（`KuzuEmbeddedGraph`）；
  - Office Hours 系列（每周 Discord Office Hours）；
  - DuckDB 生态会议演讲（与 Kuzu 类似模式）；
- **社区**：
  - Discord（Kuzu 社区）；
  - GitHub Discussions；
  - Twitter / X @kuzudb；
- **实战项目**：`https://github.com/kuzudb`，`example/` 与 `test/` 包含大量 demo（GraphRAG、MovieLens、StackOverflow、Bitcoin tx graph、社交网络 demo 等）。

## 二十、可移植性与部署

- **支持平台**：Linux（x86_64 / ARM64）、macOS（x86_64 / ARM）、Windows（x86_64）；
- **嵌入式模式**：直接 `pip install kuzu`，`npm install kuzu`，`#include <kuzu.hpp>`；
- **服务端模式**：无内置 server，但可借助 REST wrapper（社区贡献）；
- **容器化**：官方 `kuzu/kuzu` Docker 镜像（嵌入 demo）；
- **浏览器 / Web**：通过 Node.js 包可用于 Deno / Bun；
- **Python 部署**：Jupyter Notebook + Streamlit + FastAPI 中嵌入使用。

## 二十一、变体与衍生项目

- **Kuzu Core**：MIT，与原版同步；
- **Kuzu Java / Rust / Go / C#**：通过 C API 暴露；
- **Kuzu Server**（社区贡献）：小型 HTTP wrapper（实验性）；
- **商业版本**：Kuzu Inc. 暂无明显商业版；
- **关联项目**：与 **DuckDB、Polars、Apache Arrow、Numpy/Pandas** 形成数据科学生态。

## 二十二、风险与陷阱

### 22.1 嵌入式不等于"易安装"

- pip install / npm install 可能因编译失败；
- 多个 Python 版本（CPython 3.8-3.13）需对应 wheel；
- 树莓派 / 特殊 CPU 架构需要自行 cmake；
- macOS ARM 兼容性依赖 Xcode 工具链版本。

### 22.2 单进程限制

- 文件级多进程支持但不友好；
- 高并发写场景需自行协调；
- 类似 SQLite 锁升级为文件锁 → 容易争用。

### 22.3 兼容性快照

- 存储格式版本号 `storage_version`；
- 跨小版本（0.4 → 0.5）一般兼容；
- 大版本（0.x → 1.0）可能不兼容 → 注意备份与 dump 后 reload；
- export CSV 是稳妥的迁移通道。

### 22.4 Cypher 兼容覆盖率

- 对 Neo4j TCK 通过率高但"100% 兼容"不等于"100% 行为一致"；
- 部分 User-defined procedures 与 Neo4j GDS 不兼容；
- 部分 `OPTIONAL MATCH` 嵌套 / `UNION` 全场景可能差异；
- 建议在迁移之前跑自家测试集。

### 22.5 向量化覆盖

- 矢量化主要在 scan / filter / join；
- 部分操作仍是 row-at-a-time；
- 部分复杂 OLAP 优化空间大；
- 性能并非 OLAP 全打。

### 22.6 内存占用

- 列存压缩后内存占用较高；
- 大列族（百万行）查询时 Vector 分配可能慢；
- 调整 `vector_size` 与 buffer pool size。

### 22.7 嵌入式数据库的并发

- 并发写与 MVCC 配合 OK；
- 跨 connection 写时 optimistic retry 需要客户端处理；
- 长事务可能占 shadow pages。

### 22.8 升级 / 数据迁移

- `INSTALL <package>` / `LOAD <package>` Cypher 语法实验性；
- 备份 / restore 一定在落盘后完成；
- 部分数据类型在 Python / C++ 之间映射需谨慎。

### 22.9 图算法不完整

- 内置算法少（最短路径系列）；
- 复杂算法（如 Louvain）需用户自实现或调用 NetworkX；
- 用户贡献算法通过 `create_macro` 自包装。

## 二十三、与本项目的关联

- **嵌入式架构**：本项目 `learning/scaffold/cpp/stl` 已手写 24 组件，正适合参考 Kuzu 嵌入式库；
- **Cypher 兼容**：本项目如未来加图，可学 Kuzu 路径（ANTLR + Binder + Vector）；
- **列存 + 邻接表融合**：本项目学习 Kuzu 在 DuckDB 风格"OLAP + 图"混合模式；
- **矢量化执行**：与 `engineering/src/algo/` 中的 SIMD 优化一致（已用 AVX2 / AVX512）；
- **多语言绑定**：Kuzu 的"纯 C + 多语言绑定"模式适合本项目 Qt / Lua / Python 拓展。
- **本项目潜在应用**：在 Snippet Lab / Sandbox 中嵌入 Kuzu 做 graph-based 演示。

## 二十四、列式存储的层次结构与 Zone Map 剪枝

### 24.1 与 DuckDB 的同源关系（源码级证据）

Kuzu 的作者团队出自 DuckDB（DuckDB 论文作者群体，同属 CMU Database Group 血脉），因此 Kuzu 的存储与执行层大量沿用 DuckDB 的结构与函数命名。这一点在源码里有直接可查的证据，而不只是宣传口径：

- `src/common/checksum.cpp:2` 的文件头注释写明 checksum 实现 "taken from DuckDB"，并给出 `v1.3-ossivalis` 分支的源码链接与 LICENSE 链接；
- `src/include/storage/stats/hyperloglog.h:1` 的首行注释是 "This HyperLogLog implementation is taken from duckdb."，并直接给出 DuckDB 仓库对应头文件的链接；
- `src/include/common/types/date_t.h:52` 注明日期类型实现 "copied from DuckDB's source code"，`src/common/types/date_t.cpp:264` 进一步记录了 Kuzu 相对 DuckDB **删掉**了哪些特性（不解析负年份、不解析以 "BC" 结尾的日期）；
- `src/catalog/catalog_entry/sequence_catalog_entry.cpp:69` 的序列取值实现标注 "referenced from DuckDB"；
- `src/binder/bind/bind_file_scan.cpp:51` 直接说明 "we use duckdb to read from iceberg/delta/azure" —— 外部表（Iceberg / Delta / Azure）走 DuckDB 的文件扫描层。

理解这条同源链有一个实用价值：**DuckDB 执行层的结论（因子化表、链式哈希 HashJoin、Filter/Projection 下推、矢量化）在 Kuzu 里几乎一一对应地存在**，读 Kuzu 源码时可以把 DuckDB 的结论放心迁移过来；而**图特有的部分（邻接表即 CSR、遍历 Extend）才是 Kuzu 的自有设计**。

### 24.2 存储层次：Vector → Node Group → Column Chunk → Segment

Kuzu 的列存粒度由 CMake 生成的 `system_config.h` 决定（模板见 `cmake/templates/system_config.h.in`，默认值在 `CMakeLists.txt`）：

| 级别 | 常量 | 默认值 | 源码位置 |
|------|------|--------|----------|
| 向量（每次求值单位） | `VECTOR_CAPACITY_LOG_2` | 11 → 2048 值 | `CMakeLists.txt:119` |
| 文件页 | `PAGE_SIZE_LOG2` | 12 → 4 KB | `CMakeLists.txt:114` |
| 临时文件页（大块溢写） | `TEMP_PAGE_SIZE_LOG2` | 18 → 256 KB | `cmake/templates/system_config.h.in:34` |
| 节点组（存储单元） | `NODE_GROUP_SIZE_LOG2` | 17 → 131072 行 | `CMakeLists.txt:126` |
| 列块段大小上限 | `MAX_SEGMENT_SIZE_LOG2` | 18 → 256 KB | `CMakeLists.txt:132` |

`NodeGroup` 是列存的基本分配单位（`src/include/storage/table/node_group.h:85`），容量默认取 `StorageConfig::NODE_GROUP_SIZE`（`src/include/storage/table/node_group.h:87`）。节点 ID 到节点组的映射是**纯位运算**，没有除法：

```
startOffset(nodeGroupIdx) = nodeGroupIdx << NODE_GROUP_SIZE_LOG2    # src/include/storage/storage_utils.h:56
nodeGroupIdx(nodeOffset)  = nodeOffset  >> NODE_GROUP_SIZE_LOG2    # src/include/storage/storage_utils.h:59
```

`ColumnChunk` 内部是**一个 `vector<unique_ptr<ColumnChunkData>>`**——即"列块 = 若干不可变段（segment）"，构造函数在 `src/storage/table/column_chunk.cpp:59`。段的切分目的是把超过页大小的列拆成可独立读写的块，`MAX_SEGMENT_SIZE` 是其上界。更新不走原地改，而是追加到 chunk 附带的 `updateInfo` 链上（`src/storage/table/update_info.cpp:17`），因此 **segment 本身始终不可变** —— 这正是 zone map 统计得以成立的前提。

### 24.3 Zone Map：块级 min/max 与剪枝判据

每个列块维护一对 `[min, max]`。`ColumnChunkStats::update` 只对"可存储值类型"或内部 ID 采集统计（`src/storage/table/column_chunk_stats.cpp:11`），合并进已有 min/max 的逻辑在 `src/storage/table/column_chunk_stats.cpp:31`；字符串、列表等嵌套类型的统计始终为空，因此**永不剪枝**（保守正确）。

剪枝判据在 `checkZoneMapSwitch`（`src/storage/predicate/constant_predicate.cpp:22`）里是一组**否定式**区间判定。记谓词为 `col OP c`，块统计为 `[min, max]`：

| 谓词 | 剪枝（SKIP_SCAN）条件 | 直觉 |
|------|----------------------|------|
| `col = c` | `c ∉ [min, max]` | 常量落在值域外 |
| `col <> c` | `c = min 且 c = max` | 整块都等于 c |
| `col > c` | `c ≥ max` | 常量不小于块内最大值 |
| `col ≥ c` | `c > max` | 常量严格大于块内最大值 |
| `col < c` | `c ≤ min` | 常量不大于块内最小值 |
| `col ≤ c` | `c < min` | 常量严格小于块内最小值 |

注意每一条都"贴边才剪"，例如 `col > c` 是在 `c ≥ max` 时才剪（`src/storage/predicate/constant_predicate.cpp:42`）——只要块里可能存在大于等于 `max` 的值就必须扫。这保证了**漏剪率为 0**（false-positive 剪枝不存在），代价是剪枝率保守：一块 131072 行只要有 1 行落在值域里就整块读入。

一组谓词用 AND 串联（`ColumnPredicateSet::checkZoneMap`，`src/storage/predicate/column_predicate.cpp:14`）：任一谓词返回 SKIP 即整块跳过。

```
function zoneMapScan(chunks, columnIDs, predicateSets):
    for i in 0..len(columnIDs):
        cid = columnIDs[i]
        if cid 是 INVALID 或 ROW_IDX: continue
        if chunks[cid] 的 min 或 max 无值:            # 嵌套类型 / 全 NULL
            return SCAN
        for p in predicateSets[i]:
            if p 命中上表的剪枝条件:
                return SKIP
    return SCAN
```

**剪枝的实际位置**在扫描的最内层。`ChunkedNodeGroup::scan` 先算 zone map 结果，若返回 SKIP 就把输出选择向量置空并立即返回（`src/storage/table/chunked_node_group.cpp:293`、`src/storage/table/chunked_node_group.cpp:298`）：

```
getZoneMapResult(scanState, chunks):                 # src/storage/table/chunked_node_group.cpp:268
    for i in 0..len(scanState.columnIDs):
        cid = scanState.columnIDs[i]
        if cid 是 INVALID_COLUMN_ID 或 ROW_IDX: continue
        if chunks[cid]->hasUpdates():                # src/storage/table/chunked_node_group.cpp:278
            return ALWAYS_SCAN                        # 保守：有更新的块统计不准，整体放弃剪枝
        if chunks[cid]->getMergedColumnChunkStats() 被 predicateSets[i] 任一谓词剪掉:
            return SKIP_SCAN
    return ALWAYS_SCAN
```

`hasUpdates` 的放弃分支（`src/storage/table/chunked_node_group.cpp:278`）是这段代码里最重要的工程折衷，源码注释也明说了原因：把增量更新合并进统计"可能很慢"，于是直接放行。**这解释了为什么高频 UPDATE 的表 zone map 收益会塌掉**——剪枝粒度是 131072 行的列块，块内只要有一行被更新过，整块 131072 行都失去剪枝资格。

复杂度：剪枝发生在节点组粒度上，代价是每组 O(#谓词) 次整型比较。设选择率为 s、谓词数为 p，扫描代价从 O(N) 降到 O(s·N)，剪枝开销为 O((N / 131072) · p)；当 s·131072 ≫ p 时净收益为正。

### 24.4 选择率估计：HyperLogLog 精确基数

`TableStats` 维护表基数与每列 `ColumnStats`（`src/storage/stats/table_stats.cpp:9`）。`ColumnStats` 内嵌一个 HyperLogLog（构造时 `if (!isNested) hll.emplace()`，`src/storage/stats/column_stats.cpp:8`），更新时先对整列做向量化 hash，再把 hash 逐个喂给 HLL（`src/storage/stats/column_stats.cpp:14`），哈希分布用"取低 P 位定桶、剩余高位前导零数定寄存器高度"的经典算法（`src/include/storage/stats/hyperloglog.h:25`），参数 `P = 6`、`M = 2^6 = 64`、`ALPHA = 1/(2 ln 2)`（`src/include/storage/stats/hyperloglog.h:16`、`src/include/storage/stats/hyperloglog.h:18`、`src/include/storage/stats/hyperloglog.h:19`）。

基数读出走标准 HLL 修正公式（`src/storage/stats/hyperloglog.cpp:63`，入口 `src/storage/stats/hyperloglog.cpp:11`）：

```
估计基数 = round(ALPHA · M² / z)，其中
  z = M·τ((M − c[Q])/M) + Σ_{k=1..Q} c[k]·2^(−k) + M·σ(c[0]/M)
```

实现中 `Σ c[k]·2^(−k)` 用等价递推（`z += c[k]; z *= 0.5` 逐项累加）写出。相对误差约 `1.04/√m`，代价是每列 64 字节。

**HLL 的用途不是剪枝而是基数估计**：`getNumDistinctValues`（`src/include/storage/stats/column_stats.h:19`）直接喂给规划器做 `card / NDV` 的等值选择率计算（详见 §二十六.4）。

## 二十五、关系存储：CSR 节点组、双向分裂与校准树重分布

### 25.1 关系表的方向分裂

**这一版的 Kuzu 不再提供"CSR / CSRStar / FA"三种可选关系表示**。核心枚举只剩两种，且 CSR 是关系表的唯一格式（`src/include/storage/table/chunked_node_group.h:37`）：

```
enum class NodeGroupDataFormat : uint8_t { REGULAR = 0, CSR = 1 };
```

- `REGULAR` 用于节点表（`NodeGroup` 构造函数的 format 默认参数即 `REGULAR`，`src/include/storage/table/node_group.h:90`），节点表本质就是列存，不需要邻接结构；
- `CSR` 用于关系表，`NodeGroupCollection::getOrCreateNodeGroup` 按 format 二选一实例化（`src/storage/table/node_group_collection.cpp:168`），且 `RelTableData::getOrCreateNodeGroup` 无条件请求 CSR 格式（`src/include/storage/table/rel_table_data.h:92`）。

方向处理不是"一份数据两种访问顺序"，而是**按方向拆成两套物理表数据**。`RelTable` 持有按 `RelDataDirection`（FWD / BWD）索引的 `RelTableData`，取列时先按方向选表数据再取列（`src/include/storage/table/rel_table.h:177`）：

```
getCSROffsetColumn(direction) = getDirectedTableData(direction)->getCSROffsetColumn()
```

扫描状态初始化时方向决定读哪一侧的 offset/length 列（`src/storage/table/rel_table.cpp:45`）：

```
RelTableScanState::setToTable(..., direction):
    columns[i]       = RelTable::getColumn(columnIDs[i], direction)     # 按方向选列
    csrOffsetColumn  = RelTable::getCSROffsetColumn(direction)          # src/storage/table/rel_table.cpp:45
    csrLengthColumn  = RelTable::getCSRLengthColumn(direction)
```

方向是否双向存储是**编译期**开关：`KUZU_DEFAULT_REL_STORAGE_DIRECTION` 默认 `BOTH`（`CMakeLists.txt:109`），生成头里对应 `DEFAULT_REL_STORAGE_DIRECTION` 与 `DEFAULT_EXTEND_DIRECTION`（`cmake/templates/system_config.h.in:37`）。**只存单向能把关系表列存体积砍到约一半，代价是反向遍历要现场转置** —— 这是图数据库最典型的空间/查询权衡，Kuzu 把它做成了构建期选项而非运行期语法。

关系表的列序是固定的：邻居 ID 列恒为 0 号列、关系 ID 列恒为 1 号列（`src/include/storage/table/csr_node_group.h:162`），属性列从 2 号起。

### 25.2 CSRNodeGroup：持久段 + 内存索引的两层结构

`CSRNodeGroup`（`src/include/storage/table/csr_node_group.h:172`）的注释直接写明了它的两层组织（`src/include/storage/table/csr_node_group.h:165`）：

- **持久段** `persistentChunkGroup`：已 checkpoint 或已 flush 的数据，按 CSR 排列，是真正的列存；
- **瞬时段** `chunkedGroups`：正在提交但尚未 checkpoint 的数据，按普通节点组追加，并额外挂一个 `csrIndex` 记录每个绑定节点对应的行号向量。

两个成员声明在 `src/include/storage/table/csr_node_group.h:286`。`CSRIndex` 是一个**定长数组**，长度等于节点组大小（`src/include/storage/table/csr_node_group.h:82`）：

```
std::array<NodeCSRIndex, StorageConfig::NODE_GROUP_SIZE> indices;   // 131072 项
```

每个 `NodeCSRIndex`（`src/include/storage/table/csr_node_group.h:30`）带一个**顺序性压缩开关** `isSequential`：若该节点的邻居行号在瞬时段里连续，只存 `(startRow, length)` 两个数；一旦被更新打散就 `turnToNonSequential()` 展开成显式下标数组。这让"刚 append 未打散"的常见情形下每节点只花 2 个整数。

### 25.3 遍历扫描算法：连续区间 + 前缀 offset/length

CSR 头 `InMemChunkedCSRHeader`（`src/include/storage/table/csr_chunked_node_group.h:59`）就是两条列：`offset` 与 `length`。邻居定位是 O(1)：

```
start = header->getStartCSROffset(nodeOffset)   # 邻居行在 CSR 数组中的起点
deg   = header->getCSRLength(nodeOffset)        # 邻居数量
neighbors = persistentChunkGroup[nbrIDColumn][start .. start + deg - 1]
```

对应实现见 `src/storage/table/csr_node_group.cpp:26` 与 `src/storage/table/csr_node_group.cpp:27`。

物理算子侧的入口是 `ScanRelTable::getNextTuplesInternal`（`src/processor/operator/scan/scan_rel_table.cpp:74`）：先在已提交数据上反复 `scan`，返回空才向下游要下一批绑定节点并重置扫描状态（`src/processor/operator/scan/scan_rel_table.cpp:77`）。这实现了 Cypher 的"惰性展开"语义 —— 绑定节点不足一个向量（2048）时不会把整张关系表拉出来。

`tryScanCachedTuples`（`src/storage/table/csr_node_group.cpp:18`）是这段扫描里最精巧的一环：**一次 I/O 服务多个绑定节点**。若绑定节点按 offset 递增、且它们的 CSR 区间在缓存窗口内相邻，就不必为每个节点单独发一次列读：

```
tryScanCachedTuples(scanState):
    off    = nodeOffset(boundNode) % NODE_GROUP_SIZE
    start  = header->getStartCSROffset(off)          # src/storage/table/csr_node_group.cpp:26
    len    = header->getCSRLength(off)               # src/storage/table/csr_node_group.cpp:27
    nextCachedRowToScan = max(nextCachedRowToScan, start)
    if 起点落在已缓存窗口之外: return false            # 缓存失效，退回逐节点读
    n = min(nextRowToScan, start + len) - nextCachedRowToScan
    if 有「已扫描向量位图」: 用位图把尚未产出过的行挑出来组成选择向量
    else: 输出选择向量直接设为连续区间 [startCachedRow, +n)
    nextCachedRowToScan += n
    if start + len == nextCachedRowToScan: 切到下一个绑定节点并复位游标
```

缓存命中判定用的 `cachedScannedVectorsSelBitset` 是**按输出向量位（2048 位）标记哪些行已被产出过**，从而做到"多绑定节点 → 一次读盘 → 位图去重"。这正是 Kuzu"遍历 + 列存"融合的收益点：传统邻接表实现里 N 个绑定节点要 N 次指针追逐，这里退化成近似一次顺序读 + 位图过滤。

复杂度：单次 hop 展开 O(1) 定位 + O(deg) 连续读；随机访问单个绑定节点时走 `randomLookup` 分支，只读 1 个 offset 和 1 个 length（`src/storage/table/csr_node_group.cpp:86` 起的 `initScanForCommittedPersistent`），避免整组 header 扫描。

### 25.4 校准树：checkpoint 时的 CSR 区域重分布

高扇出图上会出现极端不均衡：少数 hub 节点度数上百万，多数节点度数个位数。若按节点组线性排 CSR，hub 所在组的存储密度会塌到极低，一页只装得下几条边。Kuzu 的解法是一棵**校准树（calibrator tree）**。`PackedCSRInfo`（`src/include/storage/table/csr_node_group.h:100`）定义树高与密度步长：

```
calibratorTreeHeight = NODE_GROUP_SIZE_LOG2 − CSR_LEAF_REGION_SIZE_LOG2 = 17 − 10 = 7
                                                                        # src/include/storage/table/csr_node_group.h:103
highDensityStep = (LEAF_HIGH_CSR_DENSITY − PACKED_CSR_DENSITY) / calibratorTreeHeight
                = (1.0 − 0.8) / 7 ≈ 0.02857
```

两个端点常量定义在 `src/include/common/constants.h:78`（`PACKED_CSR_DENSITY = 0.8`）与 `src/include/common/constants.h:79`（`LEAF_HIGH_CSR_DENSITY = 1.0`）。每个层级 `level` 的允许密度线性插值（`src/storage/table/csr_node_group.cpp:1166`）：

```
highDensity(level) = 1.0                                     若 level = 0（叶区）
                   = 0.8 + highDensityStep · (7 − level)     否则
```

即 level=7（顶层）要求 80% 填充，level=1 要求约 97.1%，叶区放宽到 100%。checkpoint 时合并相邻 region 的密度判定（`src/storage/table/csr_node_group.cpp:1177`）：

```
ratio = (旧 region 内各节点 CSR 长度之和 + 待合并 region 的 sizeChange 之和)
        / (region 右端 CSR 结束偏移 − region 左端 CSR 起始偏移)
若 ratio ≤ highDensity(level) 则允许合并                     # src/storage/table/csr_node_group.cpp:1193
```

叶区粒度由 `CSR_LEAF_REGION_SIZE = 2^10 = 1024` 决定（`cmake/templates/system_config.h.in:50`）。这套"自底向上按密度合并"的策略在效果上等价于一次 **best-fit 装箱**：稀疏区被并入高密度区，树的每一层对应一轮装箱，把 hub 的边集中到少数几个节点组，从而让 zone map 与列压缩都工作在高密度数据上。

代价写在源码的 TODO 里：`CSRIndex` 仍是每节点一个定长槽的平铺数组，作者注明应拆成"每叶区一层 + 每节点一层"两级以省空间（`src/include/storage/table/csr_node_group.h:79`）。

## 二十六、执行引擎：优化器规则序列、图专用基数估计与哈希表链式探测

### 26.1 优化器的规则序列（顺序即语义）

`Optimizer::optimize`（`src/optimizer/optimizer.cpp:22`）是一条**线性、可读的规则流水线**；`enablePlanOptimizer` 关闭时（`src/optimizer/optimizer.cpp:24`）整条流水线跳过，只跑 `SchemaPopulator` 补 schema（`src/optimizer/optimizer.cpp:75`）：

| # | 规则 | 源码行 | 作用 |
|---|------|--------|------|
| 1 | `RemoveFactorizationRewriter` | `src/optimizer/optimizer.cpp:26` | 先拆平因子化结构，否则后续规则无法处理 |
| 2 | `CorrelatedSubqueryUnnestSolver` | `src/optimizer/optimizer.cpp:29` | 相关子查询去相关 |
| 3 | `RemoveUnnecessaryJoinOptimizer` | `src/optimizer/optimizer.cpp:32` | 去掉恒等连接 |
| 4 | `FilterPushDownOptimizer` | `src/optimizer/optimizer.cpp:35` | 谓词下推 |
| 5 | `ProjectionPushDownOptimizer` | `src/optimizer/optimizer.cpp:39` | 列裁剪 |
| 6 | `LimitPushDownOptimizer` | `src/optimizer/optimizer.cpp:42` | LIMIT 下推 |
| 7 | `HashJoinSIPOptimizer` | `src/optimizer/optimizer.cpp:47` | Semi Masker 半连接剪枝（在 build 侧节点 ID 上建 Roaring 位图，probe 侧先过筛；可配置开关 `enableSemiMask`） |
| 8 | `TopKOptimizer` | `src/optimizer/optimizer.cpp:51` | `ORDER BY … LIMIT` → TopK |
| 9 | `FactorizationRewriter` | `src/optimizer/optimizer.cpp:54` | 重新插入因子化结构 |
| 10 | `AggKeyDependencyOptimizer` | `src/optimizer/optimizer.cpp:59` | 聚合键依赖消除（不改因子化结构，故置于 9 之后） |

第 1 条与第 9 条是一对：规则 1–8 都在"扁平行式"表示上工作，规则 9 再统一做因子化（factorization）—— 把若干列打包进一个 data chunk，遍历时只解包真正被扩展的那一列。`FactorizationRewriter::visitHashJoin`（`src/optimizer/factorization_rewriter.cpp:43`）演示了这一点：在 join 的 probe / build 两侧按需插 `Flatten`。图上 `(a)-[e]->(b)` 展开后，同一批行的 `b.id / b.name / e.weight` 会被打包在一起，下一跳只解 `b.id` 一列 —— **这是"只解包被访问列"而非"拆成独立元组"的关键**。

### 26.2 谓词下推：两个性质完全不同的下推目标

`FilterPushDownOptimizer` 对不同算子做**两种语义完全不同**的下推：

**（a）下推成属性谓词集（只给 zone map 当提示，不改变语义）**。`visitScanNodeTableReplace`（`src/optimizer/filter_push_down_optimizer.cpp:180`）在 `enableZoneMap` 打开时把谓词转成 `ColumnPredicateSet` 挂到扫描算子上（`src/optimizer/filter_push_down_optimizer.cpp:185`）。这些谓词**不改变结果**，纯粹是给存储层剪枝的提示 —— 即使所有谓词都没命中块统计，数据仍会被扫描后过滤。

**（b）改写成主键索引扫描（改变执行方式）**。若谓词是"节点 ID = 常量"，就把它从谓词集中弹出，并把扫描类型改成 `PRIMARY_KEY_SCAN`（`src/optimizer/filter_push_down_optimizer.cpp:199`）；若右侧不是常量（LITERAL / PARAMETER / `CAST(常量)`），则**把谓词原样塞回去**（`src/optimizer/filter_push_down_optimizer.cpp:204`），保证语义不回退。识别逻辑在 `isNodePrimaryKey`（`src/optimizer/filter_push_down_optimizer.cpp:285`）与 `popNodePKEqualityComparison`（`src/optimizer/filter_push_down_optimizer.cpp:298`）。

对 Extend 同理：非递归 extend 的属性谓词也可下推成 zone map 提示，但**递归 extend 被明确排除**（`src/optimizer/filter_push_down_optimizer.cpp:219`）。

### 26.3 TopK 改写

`ORDER BY → (PROJECTION →) MULTIPLICITY_REDUCER → LIMIT` 这条固定模式被折叠为单个 TopK 算子（`src/optimizer/top_k_optimizer.cpp:32`）。语义等价性来自两点：**排序键与 limit / skip 合并**（`orderBy->setLimitNum(...)` 与 `setSkipNum(...)`），以及**去掉了 multiplicity reducer** —— 去重可以推迟到 TopK 之后，因为 TopK 只保留前 K 行，对前 K 行去重不会改变前 K 行的集合。

复杂度收益是 `O(M log M) → O(M log K)`（M 为输入行数，K 为 LIMIT）。Kuzu 还设了"输入太小就不排序"的阈值 `OrderByConfig::MIN_SIZE_TO_REDUCE`（`cmake/templates/system_config.h.in:62`）。

### 26.4 图专用基数估计：扩展率

图查询的基数估计与关系型不同 —— **遍历不改变因子化基数，而是改变组的乘数（multiplier）**。`appendNonRecursiveExtend` 里写得很清楚（`src/planner/plan/append_extend.cpp:87`）：

```
extend->setCardinality(plan.getLastOperator()->getCardinality())   # 基数不变
group->setMultiplier(extensionRate)                                # src/planner/plan/append_extend.cpp:91
```

扩展率本身由**平均出度**导出（`src/planner/join_order/cardinality_estimator.cpp:219`）：

```
oneHopExtensionRate = numRels / max(1, numBoundNodes)
```

其中 `numRels` 取自关系表统计（`src/planner/join_order/cardinality_estimator.cpp:204`）、`numBoundNodes` 取自节点表统计。再按关系类型放大（`src/planner/join_order/cardinality_estimator.cpp:214`）：

| 关系类型 | 扩展率 |
|---------|-------|
| `NON_RECURSIVE` | `oneHop` |
| `VARIABLE_LENGTH_WALK` / `TRAIL` / `ACYCLIC` | `oneHop × max(upperBound, 1) × scaleFactor` |
| `SHORTEST` / `ALL_SHORTEST` / `WEIGHTED_SHORTEST` / `ALL_WEIGHTED_SHORTEST` | `min(oneHop × max(upperBound,1), numRels) × scaleFactor` |

最短路类用 `min(…, numRels)` 封顶是对的：最短路径的解空间不会超过关系总数。变长模式则再乘一个 `scaleFactor`（`src/planner/join_order/cardinality_estimator.cpp:214`），它来自客户端配置 `recursivePatternCardinalityScaleFactor`（`src/include/main/client_config.h:52`），默认值 `RECURSIVE_PATTERN_FACTOR = 100`（`src/include/main/client_config.h:21`）。注意方向：这个系数是**放大**估计值而不是调低 —— `oneHop × upperBound` 对变长模式是极端高估的朴素上界，但真实的路径数量远低于上界（同一批边被反复复用），放大 100 倍是在补偿这种系统性低估，代价是让规划器对变长模式更保守、更倾向于选 HashJoin 而非嵌套循环。

对照普通算子的估计：等值谓词默认选择率 0.01、非等值 0.1（`src/include/common/constants.h:159` 与 `src/include/common/constants.h:160` 的 `PlannerKnobs`）；若谓词命中单标签属性，则改用 HLL 的 NDV 做 `card / NDV`（`src/planner/join_order/cardinality_estimator.cpp:174`）；非 ID 连接的哈希连接退化为 `|probe| × |build| × 0.01^#cond`（`src/planner/join_order/cardinality_estimator.cpp:85`）。**图模式的连接键几乎总是节点 ID**，因此走的是除法分支 `(|probe| × build 侧键基数) / Σ NDV`，这正是图查询的 join 基数估计比通用 SQL 准得多的原因。

### 26.5 哈希表：分块槽数组 + 元组内分离链

Join / Aggregate 共用 `BaseHashTable`（`src/include/processor/result/base_hash_table.h:39`）。槽数组是**分块**的，每块大小固定为一个 `HASH_BLOCK_SIZE`（等于 `TEMP_PAGE_SIZE` = 256 KB）：

```
HASH_BLOCK_SIZE  = TEMP_PAGE_SIZE                                    # src/include/processor/result/base_hash_table.h:39
numSlotsPerBlock = HASH_BLOCK_SIZE / sizeof(uint8_t*) = 262144 / 8 = 32768
                                                                  # src/processor/operator/hash_join/join_hash_table.cpp:17
slotIdx          = hash % maxNumHashSlots                            # src/include/processor/result/base_hash_table.h:41
槽地址 = hashSlotsBlocks[slotIdx >> numSlotsPerBlockLog2] + (slotIdx & slotIdxInBlockMask)
                                                                  # src/processor/operator/hash_join/join_hash_table.cpp:189
```

分块的好处是槽数组按需增长（`allocateHashSlots` 按需追加新的 `DataBlock`，`src/processor/operator/hash_join/join_hash_table.cpp:107`），不预先分配整个数组；且槽地址计算是两次位运算、无分支。

**冲突处理用分离链（chaining），且链表指针直接内嵌在元组里**：`prevPtrColOffset` 被固定为表 schema 的最后一列（`src/processor/operator/hash_join/join_hash_table.cpp:20`）。建表阶段把每个元组头插进桶，并把旧桶头写回新元组的 prev 字段（`src/processor/operator/hash_join/join_hash_table.cpp:116`）。探测阶段先算 hash 拿到候选头指针，再沿 `prev` 链逐个比对（`src/processor/operator/hash_join/join_hash_table.cpp:154`）：

```
for i in 0..hashSelVec.getSelSize():
    probedTuples[i] = getTupleForHash(hashVector[hashSelVec[i]])   # src/processor/operator/hash_join/join_hash_table.cpp:150
while probedTuples[0] != null:
    matched += matchFlatVecWithEntry(keyVectors, probedTuples[0])
    probedTuples[0] = *getPrevTuple(probedTuples[0])      # src/processor/operator/hash_join/join_hash_table.cpp:164
    if matched == DEFAULT_VECTOR_CAPACITY: break           # 输出向量满即停
```

`matchFlatKeys` 的 `DEFAULT_VECTOR_CAPACITY` 截断（`src/processor/operator/hash_join/join_hash_table.cpp:154`）保证单个 probe 向量的输出不超过 2048 行，剩余匹配留给下一批 —— **"链可以很长，但一批只吐 2048 个"的背压**。

槽位数按 `nextPowerOfTwo(numTuples × 2)` 分配（`src/processor/operator/hash_join/join_hash_table.cpp:108`），即**负载因子 α 恒定 0.5**；因为槽数是 2 的幂，取模退化为掩码。分离链在这个负载下链表平均长度约 α = 0.5，成功查找的期望指针追逐次数约 `1 + α/2 = 1.25`，未命中约 `α = 0.5` —— 也就是说**绝大多数探测在一次指针解引用后就判定不匹配**，真正的成本落在链上的键字段比对上。

⚠️ **这一版没有外排溢写的 HashJoin**：`src/processor/operator/hash_join/` 下只有 build / probe / hash table 三个实现文件，全库检索 `spill` 在 `src/processor/` 下零命中。溢写能力存在于基础设施层（`Spiller`，`src/include/storage/buffer_manager/spiller.h:17`），供其他算子使用。**建侧过大时 Kuzu 的选择是让整条计划在小向量上跑完，而不是写临时文件** —— 这是与 Spark / Flink 显著不同的设计取向。

## 二十七、参考资料

1. Kuzu 官方文档 — `https://docs.kuzudb.com/`。
2. Kuzu 源码 — `https://github.com/kuzudb/kuzu`。
3. Cypher 规范（Neo4j） — `https://neo4j.com/docs/cypher-manual/current/`。
4. Cypher TCK — `https://github.com/opencypher/opencypher-tck`。
5. DuckDB 论文 — "DuckDB: an Embeddable Analytical Database"（SIGMOD 2019）。
6. FSST 论文 — Boncz & Friedman, "FSST: Fast Random Access String Compressor"（VLDB 2020）。
7. ART 论文 — Leis et al., "The Adaptive Radix Tree"（ICDE 2013）。
8. LangChain Kuzu 集成 — `https://python.langchain.com/docs/integrations/graphs/kuzu_db/`。
9. Kuzu 官方 YouTube — `https://www.youtube.com/@kuzuembeddedgraph`。
10. Kuzu 中文社区 / 中文 blog — `https://kuzudb.com/blog/`。
