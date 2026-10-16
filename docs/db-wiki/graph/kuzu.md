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

## 二十四、参考资料

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
