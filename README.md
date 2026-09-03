# Book — C/C++ 算法工程实践仓库

[![CI](https://img.shields.io/badge/CI-passing-brightgreen)](.github/workflows/ci.yml)
[![Dual-Track](https://img.shields.io/badge/dual--track-active-blue)](engineering/CMakeLists.txt)
[![OpenSpec](https://img.shields.io/badge/OpenSpec-198归档-active-brightgreen)](openspec/changes/archive/)
[![Engineering Tests](https://img.shields.io/badge/eng--tests-104-brightgreen)](engineering/build/)
[![Learning Tests](https://img.shields.io/badge/learn--tests-158-brightgreen)](learning/code-solutions/c/test/c/)
[![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)

> 完整指标快照：[**docs/project-metrics.json**](docs/project-metrics.json)

---

## 项目定位

本仓库是 **C/C++ 算法与数据结构工程实践** 的双轨制项目：

| 轨道 | 路径 | 定位 | 说明 |
|------|------|------|------|
| **工程轨** | `engineering/` | α 工程作品集 | 生产级代码、存储引擎、向量数据库、RAG 系统 |
| **学习轨** | `learning/` | β 学习日志 | LeetCode 题解、教学代码、面试资料、Obsidian 笔记 |
| **参考轨** | `reference/` | 源码镜像 | 12 个开源项目 git submodule（**不参与构建**） |

### 技术栈

- **语言**：C11 + C++17
- **构建**：CMake 3.20+
- **测试**：GoogleTest（vendored）
- **协议**：PostgreSQL Wire、gRPC、REST
- **依赖**：零运行时外部依赖

---

## 目录结构

```
book/
├── engineering/                 # 工程轨道（默认构建）
│   ├── src/
│   │   ├── db/                 # 数据库存储引擎（核心）
│   │   │   ├── storage/       # 存储层：Buffer Pool、WAL、Page、Lock
│   │   │   ├── access/        # 访问方法：Heap、BTree、向量索引
│   │   │   ├── index/         # 索引子系统（20+ ANN 索引）
│   │   │   ├── sql/           # SQL 执行器：Parser/Optimizer/Executor
│   │   │   ├── distributed/   # 分布式：分片、事务、Raft、2PC
│   │   │   ├── graph/         # 图引擎：邻接表、CSR、BFS/DFS
│   │   │   └── core/           # 核心：GUC、Server、initdb、pg_ctl
│   │   ├── algo-prod/         # 生产算法库：K-Means、排序、二分、量化
│   │   ├── ds/                 # 数据结构：Trie 等
│   │   ├── graph/              # 图算法
│   │   ├── kbase/              # 知识库（Embedding/RAG）
│   │   ├── redis/              # Redis 核心移植（SDS/链表/跳表）
│   │   ├── cpp/                # C++ STL 手写实现（mystl）
│   │   └── sdk/                # 多语言 SDK
│   ├── include/                # 公共头文件
│   ├── test/                   # gtest 单元测试（605 个源文件）
│   ├── apps/                   # 独立应用（见下方应用列表）
│   ├── rag/                    # RAG 系统（Python SDK + 服务端）
│   ├── tools/                  # 工程专用工具
│   └── test_data/              # 测试数据集（SIFT/ANN 等）
│
├── learning/                    # 学习轨道（按需构建）
│   ├── notes/                  # Obsidian 笔记（132 篇）
│   ├── code-solutions/         # 题解
│   │   └── c/                  # LeetCode/面试题 + 单元测试
│   ├── ds-c/orig/              # 手撕数据结构教学版
│   ├── ds-cpp/                 # C++ 数据结构教学版
│   ├── algo-c/                 # 教学算法
│   ├── interview/              # 面试八股文
│   └── playground/             # 演示代码
│
├── reference/                   # 参考资料（git submodule）
│   └── open-source/            # faiss/redis/postgres/mysql/milvus/...
│
├── docs/                        # 架构文档（215+ 篇）
├── openspec/                    # OpenSpec 变更管理
├── third_part/                  # 第三方（googletest/cjson）
└── build/, test-results/       # 构建和测试产物
```

---

## 一、存储引擎（PostgreSQL 风格）

### 1.1 整体架构

```
┌─────────────────────────────────────────────────────────────┐
│                        SQL 执行层                            │
│         Parser → Planner → Optimizer → Executor            │
├─────────────────────────────────────────────────────────────┤
│                       Access Method 层                      │
│     ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐  │
│     │  Heap    │ │  BTree   │ │  向量 AM  │ │  其他 AM  │  │
│     │  堆表    │ │  B+Tree  │ │ HNSW/IVF │ │          │  │
│     └────┬─────┘ └────┬─────┘ └────┬─────┘ └────┬─────┘  │
├───────────┼────────────┼────────────┼─────────────────────┤
│           │      Buffer Pool       │                      │
│     ┌─────▼─────────────────────▼─────┐                   │
│     │  Hash 表（O(1)查找）             │                   │
│     │  Clock-Sweep 置换算法            │                   │
│     │  脏页管理 + Pin/Unpin 引用计数   │                   │
│     └───────────────────────────────┬─┘                   │
├─────────────────────────────────────┼─────────────────────┤
│                    Catalog 系统                             │
│     pg_class | pg_attribute | pg_index | OID 分配        │
├─────────────────────────────────────────────────────────────┤
│                    WAL 日志系统                             │
│     写前日志 | Redo | 检查点 | Buffer 协调                 │
├─────────────────────────────────────────────────────────────┤
│                    磁盘文件层                               │
│     Page 管理 | 空闲页面分配 | 顺序/随机 I/O               │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 核心模块

| 模块 | 文件 | 功能 |
|------|------|------|
| **Catalog** | `db/catalog.h/c` | 系统表管理、OID 分配、表/列/索引元数据 |
| **Buffer Pool** | `db/storage/bufmgr.c` | 内存缓存、Clock-Sweep 置换、Hash 表查找 |
| **Heap AM** | `db/storage/heapam.c` | 堆表存储、页面结构、元组 CRUD |
| **BTree AM** | `db/access/am_btree.c` | B+Tree 索引、键比较、范围扫描 |
| **WAL** | `db/storage/wal.c` | 写前日志、检查点、崩溃恢复 |

### 1.3 设计要点

**Buffer Pool 置换策略**：采用 Clock-Sweep 算法，每个 buffer 有一个 usage_count 位，每次扫描时：
1. 若 `usage_count == 0`，淘汰该页
2. 若 `usage_count > 0`，减一并继续扫描

**WAL 持久化**：采用追加写模式，每次事务提交前必须将日志刷盘，支持 Redo 恢复。

**Page 结构**：
```
┌────────────────────┬──────────────┐
│      Header        │   Line Pointer Array   │
│  (PageHeaderData)  │   (指向 heap 数组)     │
├────────────────────┴──────────────┤
│          Heap Tuples               │
│  (实际数据，按写入顺序排列)         │
└────────────────────────────────────┘
```

---

## 二、SQL 执行引擎

### 2.1 整体架构

```
                    SQL Query
                       ↓
                 ┌─────────┐
                 │ Parser  │  sql_parser.c
                 └────┬────┘
                      ↓
                 ┌─────────┐
                 │ Planner │  planner.c
                 └────┬────┘
                      ↓
                 ┌──────────┐
                 │Optimizer │  optimizer.c (基于代价)
                 └────┬────┘
                      ↓
  ┌──────────┬──────────┬──────────┬──────────┐
  ↓          ↓          ↓          ↓          ↓
SeqScan   IndexScan  HashJoin  HashAgg   Sort...
  ↓          ↓          ↓          ↓          ↓
                 ┌─────────┐
                 │Executor │  Volcano 迭代器模型
                 └────┬────┘
                      ↓
                 Tuple Result
```

### 2.2 核心算子

| 算子 | 文件 | 说明 |
|------|------|------|
| **SeqScan** | `nodeSeqscan.c` | 全表扫描，按块读取 |
| **IndexScan** | `nodeIndexscan.c` | 索引扫描，支持 BTree/Hash |
| **HashJoin** | `nodeHashjoin.c` | Hash 连接，两阶段（Build/Probe） |
| **NestLoop** | `nodeNestloop.c` | 嵌套循环连接 |
| **HashAgg** | `nodeHashagg.c` | Hash 聚合，支持 GROUP BY |
| **Sort** | `nodeSort.c` | 外排序，外部归并 |
| **Limit** | `nodeLimit.c` | 限制返回行数 |
| **Window** | `nodeWindow.c` | 窗口函数 |

### 2.3 内存管理

采用 MemoryContext 层级管理内存：

```
TopMemoryContext
    ├── ExecutorMemoryContext     # 执行器临时内存
    ├── planner::PlannerGlobal    # 规划器全局
    └── per-query context         # 每个查询独立
```

### 2.4 并行执行

```
Main Process
    ├── Worker 1 ──→ TupleQueue ←── SeqScan
    ├── Worker 2 ──→ TupleQueue ←── SeqScan
    └── Worker 3 ──→ TupleQueue ←── SeqScan
                       ↓
                  Gather
```

使用 `worker_pool.c` 线程池 + `tuple_queue.c` 队列通信。

### 2.5 触发器

支持 BEFORE/AFTER 触发器链：
```sql
CREATE TRIGGER before_insert
    BEFORE INSERT ON users
    FOR EACH ROW
    EXECUTE FUNCTION validate_user();
```

### 2.6 分区表

支持 RANGE/LIST/HASH 分区：
```sql
CREATE TABLE sales (
    id INT,
    date DATE,
    amount NUMERIC
) PARTITION BY RANGE (date);
```

### 2.7 物化视图

```sql
CREATE MATERIALIZED VIEW monthly_sales AS
SELECT DATE_TRUNC('month', date), SUM(amount)
FROM sales GROUP BY 1;

REFRESH MATERIALIZED VIEW monthly_sales;
```

### 2.8 协议支持

| 协议 | 文件 | 说明 |
|------|------|------|
| **PGWire** | `pgwire.c` | PostgreSQL Wire Protocol 服务端 |
| **REST API** | `rest_api.c` | HTTP JSON API |

---

## 三、向量索引（20+ ANN 实现）

### 3.1 索引全景图

```
┌─────────────────────────────────────────────────────────────┐
│                    向量索引体系                              │
├──────────────────┬──────────────────────────────────────────┤
│   图索引          │  HNSW, NSW, SSG, ScaNN                  │
├──────────────────┼──────────────────────────────────────────┤
│   倒排索引        │  IVF-Flat, IVF-PQ, IVF-HNSW, Faiss 兼容  │
├──────────────────┼──────────────────────────────────────────┤
│   树索引          │  KD-Tree, Ball-Tree                     │
├──────────────────┼──────────────────────────────────────────┤
│   哈希索引        │  LSH, Multi-Probe LSH, ITQ, Spectral Hash│
├──────────────────┼──────────────────────────────────────────┤
│   量化方法        │  PQ, SQ, OPQ, RQ, LVQ                   │
├──────────────────┼──────────────────────────────────────────┤
│   混合检索        │  BM25, 向量+标量, 混合过滤               │
├──────────────────┼──────────────────────────────────────────┤
│   磁盘索引        │  DiskANN                                 │
└──────────────────┴──────────────────────────────────────────┘
```

### 3.2 主要索引详解

#### HNSW（分层可导航小世界图）

```
Layer 2:  ────────●────────●         (稀疏层，最快搜索)
Layer 1:  ────●───●───●───●───●     (中层)
Layer 0:  ●●●●●●●●●●●●●●●●●●●●●●   (底层，所有点)

搜索：从顶层入口点开始，每层贪心搜索最近邻，最后在底层精排
```

**参数**：
- `M`：每个节点的最大连接数（推荐 16-64）
- `efConstruction`：构建时搜索范围（推荐 100-400）
- `efSearch`：搜索时搜索范围（推荐 50-1000）

#### IVF-PQ（倒排文件 + 产品量化）

```
┌─────────────────────────────────────────────┐
│  原始向量: [0.1, 0.3, 0.5, 0.7, ...]       │
│         ↓  Product Quantization             │
│  聚类中心码本: 256 个，聚类维度 4            │
│         ↓                                   │
│  压缩后: [42, 128, 97, 201] (16 bytes)      │
└─────────────────────────────────────────────┘

搜索：
1. PQ 压缩 query 向量
2. 在倒排列表中找最近的几个聚类中心
3. 在这些聚类的向量中暴力搜索
```

#### DiskANN（磁盘优化索引）

专为大规模向量磁盘存储设计：
- **PQ 预处理**：将向量压缩为 4-32 bytes
- **Vamana 图**：优化磁盘 I/O 的图结构
- **搜索缓存**：热点数据缓存加速

### 3.3 量化方法对比

| 方法 | 压缩率 | 精度损失 | 适用场景 |
|------|--------|----------|----------|
| PQ（产品量化） | 8-64x | 中 | 大规模高维 |
| SQ（标量量化） | 4x | 高 | 小规模高精度 |
| OPQ（优化 PQ） | 8-64x | 低 | 高精度需求 |
| RQ（残差量化） | 可调 | 低 | 极高精度 |
| LVQ（学习矢量量化） | 可调 | 低 | 监督学习 |

### 3.4 召回率基准（SIFT-10K 数据集）

| 索引 | R@1 | R@10 | R@100 |
|------|-----|------|-------|
| HNSW | 92% | 85% | 78% |
| IVF-Flat | 98% | 92% | 85% |
| NSW | 88% | 80% | 72% |
| ScaNN | 90% | 83% | 75% |

---

## 四、分布式架构

### 4.1 层次架构

```
┌─────────────────────────────────────────────────────────────┐
│                      应用层                                 │
│     SQL 查询 | 跨分片查询 | 分布式事务                      │
├─────────────────────────────────────────────────────────────┤
│                      协调层                                 │
│     节点注册发现 | 全局锁 | 领导者选举 | 配置管理            │
├─────────────────────────────────────────────────────────────┤
│                      高可用层                               │
│     Raft 共识 | 日志复制 | 成员变更 | 故障检测              │
├─────────────────────────────────────────────────────────────┤
│                      事务层                                 │
│     2PC 两阶段提交 | SAGA 补偿 | TSO 时间戳 | MVCC         │
├─────────────────────────────────────────────────────────────┤
│                      分片层                                 │
│     Hash 分片 | Range 分片 | 一致性 Hash | 动态扩缩容        │
├─────────────────────────────────────────────────────────────┤
│                      存储层                                 │
│     Buffer Pool | Heap/BTree | WAL                          │
└─────────────────────────────────────────────────────────────┘
```

### 4.2 分片策略

**Hash 分片**：
```sql
-- 按 user_id hash 分片
CREATE TABLE orders (
    id INT,
    user_id INT,
    ...
) PARTITION BY HASH (user_id) PARTITIONS 16;
```

**Range 分片**：
```sql
-- 按时间范围分片
CREATE TABLE events (
    id INT,
    event_time TIMESTAMP,
    ...
) PARTITION BY RANGE (event_time);
```

### 4.3 分布式事务

支持 **2PC（两阶段提交）** 和 **SAGA** 两种模式：

```
2PC 流程：
Phase 1 (Prepare):
    Coordinator → 所有节点：PREPARE
    各节点：写预提交日志，返回 YES/NO

Phase 2 (Commit):
    若全部 YES：Coordinator → 所有节点：COMMIT
    若任意 NO：Coordinator → 所有节点：ROLLBACK
```

### 4.4 Raft 共识

用于 Leader 选举和日志复制：
- **Leader 选举**：Term + Vote 机制
- **日志复制**：AppendEntries 一致性
- **成员变更**：Joint Consensus

---

## 五、图引擎

### 5.1 存储模型

支持两种存储模型：
- **邻接表**：每行存储顶点的所有边
- **CSR**（Compressed Sparse Row）：压缩稀疏行，适合静态图

### 5.2 图算法

| 算法 | 文件 | 说明 |
|------|------|------|
| BFS | `bfs.c` | 广度优先搜索 |
| DFS | `dfs.c` | 深度优先搜索 |
| Dijkstra | `dijkstra.c` | 单源最短路径 |
| PageRank | `pagerank.c` | 页面排名 |
| 三角形计数 | `triangle.c` | 子图模式 |

### 5.3 图查询语言

```sql
-- SQL 扩展查询
SELECT * FROM graph_match(
    'MATCH (a:Person)-[:KNOWS]->(b:Person) WHERE a.age > 30'
);
```

---

## 六、RAG 系统

### 6.1 系统架构

```
┌─────────────────────────────────────────────────────────────┐
│                      服务端层                                │
│     gRPC Server | REST API | WebSocket                     │
├─────────────────────────────────────────────────────────────┤
│                      管道层                                 │
│     Naive RAG | Advanced RAG | Modular RAG                  │
├─────────────────────────────────────────────────────────────┤
│                      检索层                                 │
│     向量检索 | BM25 | 知识图谱 | 多路召回 + RRF 融合        │
├─────────────────────────────────────────────────────────────┤
│                      索引层                                 │
│     HNSW | IVF-PQ | 持久化存储                             │
├─────────────────────────────────────────────────────────────┤
│                      数据层                                 │
│     文档解析 | 分块策略 | Embedding 生成                    │
└─────────────────────────────────────────────────────────────┘
```

### 6.2 检索流程

```
用户查询 → 查询分类（意图识别）
    ↓
查询改写 / 查询扩展 / 查询分解
    ↓
┌────────────────────────────────────────┐
│ 多路召回（并行）                        │
│   ① 向量检索（HNSW/IVF-PQ）            │
│   ② BM25 全文检索                       │
│   ③ 知识图谱（三元组匹配）              │
└────────────────────────────────────────┘
    ↓
RRF 融合（Reciprocal Rank Fusion）
    ↓
重排序（Cross-Encoder / Late Fusion）
    ↓
上下文构建（Context Building）
    ↓
LLM 生成（OpenAI / Claude / 本地模型）
    ↓
回答输出
```

### 6.3 Python SDK

```python
from rag_engine import RAGPipeline

pipeline = RAGPipeline(
    vector_index="hnsw",
    reranker="cross-encoder",
    llm="openai"
)

# 添加文档
pipeline.add_documents([
    {"content": "向量数据库是...", "metadata": {"source": "doc1"}},
])

# 问答
result = pipeline.query("什么是向量数据库？")
print(result.answer)
print(result.contexts)
```

### 6.4 子模块清单

| 模块 | 功能 |
|------|------|
| `index/` | 向量/BM25 索引管理 |
| `retrieval/` | 多路召回、RRF 融合 |
| `reranker/` | Cross-Encoder 重排序 |
| `embedding/` | 嵌入模型、批量编码 |
| `chunker/` | 文档分块（Sentence/Paragraph/Recursive） |
| `llm/` | 大模型集成（OpenAI/Claude/Ollama） |
| `knowledge_graph/` | 三元组存储、TransE/TransH |
| `query_processing/` | 查询分类/分解/扩展/改写 |
| `evaluator/` | RAGAs 评估指标 |
| `server/` | gRPC/HTTP 服务 |
| `persist/` | 索引持久化 |

---

## 七、独立应用

### 7.1 应用列表

| 应用 | 路径 | 功能 |
|------|------|------|
| **api-server** | `apps/api-server/` | PostgreSQL Wire 协议服务器（端口 5432） |
| **db_driver** | `apps/db_driver/` | 数据库客户端驱动 |
| **games** | `apps/games/` | 小游戏（贪吃蛇、2048、数独） |
| **kbase** | `apps/kbase/` | 知识库 CLI（Embedding + RAG） |
| **todo-app** | `apps/todo-app/` | Todo 应用（Vue3 + SQLite） |
| **vdb_cli** | `apps/vdb_cli/` | 向量数据库 CLI 客户端 |
| **web** | `apps/web/` | Web 服务端 |
| **workbench_server** | `apps/workbench_server/` | 数据库工作台服务端 |
| **tools** | `apps/tools/` | 工程专用工具 |

### 7.2 api-server 详解

PostgreSQL 兼容服务器：

```bash
# 启动服务器
./api-server -p 5432 -D ./data

# 连接
psql -h localhost -p 5432 -U postgres
```

支持的命令：
- StartupMessage（协议握手）
- Simple Query（`SELECT * FROM t`）
- Extended Query（Prepared Statement）

### 7.3 todo-app 详解

全栈 Todo 应用：

```
frontend/           # Vue3 + Taro H5/小程序
├── src/
│   ├── pages/     # 页面（index/todo/detail）
│   ├── components/# 组件（TodoItem/TagPicker）
│   └── utils/     # 工具函数
└── server/        # SQLite 后端

backend/           # C 语言服务
├── todo_handler.c # 请求处理
├── todo_model.c  # 数据模型
└── todo_migration.c # 迁移脚本
```

### 7.4 kbase 详解

知识库命令行工具：

```bash
# 创建索引
kbase index create --type hnsw --dim 768

# 添加文档
kbase doc add --file ./docs/*.md

# 搜索
kbase search "向量数据库原理" --top-k 5

# RAG 问答
kbase ask "解释 HNSW 的搜索过程"
```

---

## 八、算法库

### 8.1 algo-prod 模块

| 组件 | 路径 | 功能 |
|------|------|------|
| K-Means | `Kmeans/` | 聚类算法 |
| 二分查找 | `binary_search/` | 有序数组查找 |
| 排序 | `sort/` | 快排/归并/堆排 |
| 字典 | `dict/` | 哈希字典 |
| 距离计算 | `distance/` | 余弦/欧氏/汉明距离 |
| 量化 | `quantization/` | PQ/SQ 量化 |

### 8.2 距离计算

```c
// 余弦距离
float cosine_distance(const float *a, const float *b, int dim);

// 欧氏距离（L2）
float l2_distance(const float *a, const float *b, int dim);

// 内积（用于归一化向量）
float inner_product(const float *a, const float *b, int dim);
```

### 8.3 量化实现

**PQ（Product Quantization）**：
```c
pq_quantize(vec, codebook, dim, m, k, output_code);

pq_decode(output_code, codebook, dim, m, k, reconstructed_vec);
```

---

## 九、OpenSpec 变更管理

所有变更必须走 OpenSpec 流程：

```
讨论 → proposal.md → design.md → tasks.md → specs/*.md → 实现 → 归档
```

### 流程说明

1. **proposal.md**：变更提案（背景、目标、范围、影响）
2. **design.md**：技术设计（架构、接口、边界）
3. **tasks.md**：任务拆解（每个任务独立可交付）
4. **specs/*.md**：能力规格（API 契约、数据格式）

### 当前活跃变更

- `c9-1-housekeeping` —— 日常维护
- `c9-2-known-limitations` —— 已知限制
- `c9-3-subsystems` —— 子系统改进

### 历史归档

198 个变更已归档到 `openspec/changes/archive/`。

---

## 十、快速开始

### 10.1 构建工程轨

```bash
# 配置
cmake -B build/engineering -S engineering -DBUILD_TESTING=ON

# 编译
cmake --build build/engineering --parallel 4

# 测试
ctest --test-dir build/engineering --output-on-failure
```

### 10.2 构建学习轨

```bash
cmake -B build/learning -S learning -DBUILD_TESTING=ON
cmake --build build/learning --parallel 4
ctest --test-dir build/learning --output-on-failure
```

### 10.3 双轨构建

```bash
cmake -B build/root -S . -DENGINEERING_BUILD=ON -DLEARNING_BUILD=ON
cmake --build build/root --parallel 4
```

### 10.4 运行独立应用

```bash
# API Server
cd build/engineering/apps/api-server
./api-server -D ./test_data/db

# VDB CLI
cd build/engineering/apps/vdb_cli
./vdb_cli --host localhost --port 5432
```

### 10.5 运行测试

```bash
# 运行所有测试
ctest --test-dir build/engineering --output-on-failure

# 运行特定测试
ctest --test-dir build/engineering -R db_storage -V

# 查看测试覆盖
bash engineering/scripts/coverage/run.sh
```

---

## 十一、编译产物规范

| 类型 | 输出目录 |
|------|----------|
| 编译产物 | `build/<项目或轨道>/` |
| 测试数据库 | `test-results/<项目或轨道>/` |
| 覆盖率报告 | `test-results/<项目或轨道>/coverage/` |
| 日志文件 | `test-results/<项目或轨道>/logs/` |

---

## 十二、文档索引

| 类别 | 路径 | 说明 |
|------|------|------|
| 存储架构 | [docs/storage-architecture.md](docs/storage-architecture.md) | PG 风格存储引擎 |
| SQL 执行器 | [docs/sql-executor/](docs/sql-executor/) | Parser/Optimizer/Executor |
| 向量索引 | [docs/index/](docs/index/) | 索引理论和实现 |
| 分布式 | [docs/diagrams/level1-distributed/](docs/diagrams/level1-distributed/) | 分布式架构图 |
| RAG 系统 | [engineering/rag/docs/](engineering/rag/docs/) | RAG 架构 |
| 部署 | [docs/deployment/](docs/deployment/) | Docker/运维 |

---

## 十三、治理文档

- [AGENTS.md](AGENTS.md) —— AI 助手操作指南
- [CLAUDE.md](CLAUDE.md) —— Claude Code 指令（中文规范、OpenSpec 铁律）
- [docs/architecture/dual-track.md](docs/architecture/dual-track.md) —— 双轨架构
- [docs/project-metrics.json](docs/project-metrics.json) —— 项目指标快照
