# Vespa Wiki（Yahoo/NTT 出品的文档式向量平台）

> 参考版本：Vespa 8.x（Java + C++，Yahoo 起源、NTT 持续投入）。**企业级文档式搜索 + 向量检索 + ML 推理** 一站式平台。本 wiki 重点覆盖向量类型特有章节（嵌入向量 Tensor 字段、HNSW 与倒排融合、容器化部署、ML 模型推理）。

## 一、项目定位

Vespa 是由 **Yahoo!（2005 起源，Oath / Verizon Media 维护）** 启动、后由 **NTT（Verizon 2021 出售给 NTT 旗下 Vespa.ai）** 持续投入的 **大规模文档式搜索 + 向量检索 + ML 推理** 一站式平台。提供 **YQL（类似 SQL 的查询语言）**、**容器化应用、嵌入向量原生支持、机器学习模型在线推理**，是当前最强大的 **"搜索 + 推荐 + 推荐 RAG"** 一体化开源平台。

- **痛点**：
  - 传统搜索（Elasticsearch / Solr）向量检索是后加的，缺乏原生 Tensor 字段 + ML 推理；
  - 传统向量库（Milvus / Qdrant）只是检索引擎，**缺少 ML 模型部署与在线推理**；
  - RAG / 推荐场景需要 **文档检索 + 向量召回 + 排序模型 + 生成模型** 全链路；
  - 大规模（亿级文档）场景下需要 **计算-数据协同处理**；
  - 需要 **复杂业务逻辑**（多阶段排序、自定义评分、ML 推理）；
- **典型用户**：电商搜索与推荐、新闻媒体、企业搜索、广告平台、RAG 应用、AI Agent、生物制药、内容平台；
- **核心口号**：**"The engine for applications that use AI to make decisions in real-time"** + **"Search. Rank. Recommend. In big data."**。

## 二、发展历史与社区

- **起源**：1997 年开始（AlltheWeb / Overture / AltaVista 等搜索引擎），2005 年 Yahoo 重写为 Vespa；
- **里程碑**：
  - 2005：Vespa 在 Yahoo 内部诞生（基于 AlltheWeb 代码）；
  - 2017-09：**开源 v6**（Apache 2.0）；
  - 2018：v6.x 引入 **Tensor 字段 + 机器学习**；
  - 2019：v7.x 引入 **Container 容器化**、**Multi-Tenancy**；
  - 2020：v7.x 引入 **Streaming Search**（实时搜索）；
  - 2021-07：Verizon Media 将 Vespa 卖给 NTT，**Vespa.ai 公司成立**；
  - 2022：v8.x 引入 **ColBERT 多向量**、**PyTorch 模型集成**；
  - 2023：v8.x 持续强化 **向量 + ML 推理**；
  - 2024：v8.x 引入 **Approximate Nearest Neighbor（HNSW）** 完善、**GPU 推理**；
  - 2025-2026：v8.x 持续优化 **生成式 AI 集成**、**ColPali 多模态**；
- **维护状态**：活跃，NTT + Vespa.ai 团队投入，季度大版本；
- **社区**：Slack、Forum、GitHub 6k+ star，Vespa 官方年度峰会。

## 三、许可与商业限制

- **核心代码**：**Apache License 2.0**（最宽松）；
- **商业版本**：**Vespa Cloud**（全托管，多区域）、**Vespa Enterprise Support**；
- **生态关系**：
  - 与 **Yahoo 搜索** 起源紧密相关；
  - 与 **PyTorch / ONNX / TensorFlow** 深度整合（ML 推理）；
  - 与 **HuggingFace** 模型集成；
  - 与 **OpenSearch / Elasticsearch** 是搜索阵营竞品；
  - 与 **Milvus / Qdrant** 是向量库阵营竞品，但 Vespa 是 **平台**（含向量 + ML + 文档）。

## 四、业界主要使用方与案例

- **大型用户**（公开）：
  - **Yahoo**：邮件、新闻、搜索（历史）；
  - **Oath / Verizon Media**：媒体网站搜索；
  - **Spotify**：Discover Weekly 推荐探索（早期）；
  - **Bolt**：网约车搜索；
  - **Line**：日本即时通讯搜索；
  - **多家电商**（匿名）；
- **行业**：新闻媒体、电商搜索与推荐、企业搜索、广告 RAG、内容平台、生物制药；
- **典型场景**：新闻推荐、商品检索、文档搜索 RAG、ML 排序、A/B 测试、个性化推荐。

## 五、一句话对比定位

> Vespa 是 **"Java/C++ + YQL 查询 + Tensor 字段 + 倒排 + HNSW 向量 + ML 模型推理 + 容器化 + Streaming Search + 计算-数据协同"** 的一站式搜索/推荐/向量平台；与 **Elasticsearch（搜索 + 向量）**、**OpenSearch（AWS 分支）**、**Milvus（向量专用）**、**Qdrant（Rust 向量）**、**Weaviate（Go 模块化）**、**Pinecone（商业向量）** 构成搜索/向量阵营。Vespa 的差异化是 **"文档 + 向量 + ML 推理一体化 + 计算-数据协同 + Yahoo/NTT 工业验证 + 大规模生产级"**。

## 六、整体架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                          客户端层                                     │
│  - Vespa HTTP API（GET/POST /document/v1/, /search/v1/）            │
│  - Vespa Java/Python/Node.js SDK                                    │
│  - gRPC（部分）                                                     │
└─────────────────────┬───────────────────────────────────────────────┘
                      │ HTTP/JSON + HTTP/feed
┌─────────────────────▼───────────────────────────────────────────────┐
│                    Container 容器层（JDisc 框架）                     │
│  container-core/                                                     │
│  ├─ Application（应用包，包含 services.xml + components）           │
│  ├─ HTTP Filter Chain（JDisc DiscFilter）                          │
│  ├─ Searcher Chain（Document Processor / Searchers）               │
│  ├─ Component Injection（@Inject）                                        │
│  ├─ Handler / Processor / Searcher 接口                              │
│  └─ Config Server / Config Sentinel（动态配置）                    │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                  Document API 层                                     │
│  documentapi/                                                        │
│  ├─ DocumentOperations（put / update / get / remove）              │
│  ├─ Visitor（批量遍历）                                              │
│  ├─ Routing Service（route to Content Cluster）                    │
│  └─ Bucket Space（数据分桶）                                        │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                  SearchCore / Proton（搜索核心）                      │
│  searchcore/src/main/java/com/yahoo/search/core/proton/             │
│  ├─ ProtonEngine（嵌入式搜索节点）                                  │
│  ├─ Document DB（put / get / remove）                              │
│  ├─ Indexing Pipeline（文档解析 → 字段索引）                        │
│  ├─ Attribute Writer / Reader                                       │
│  ├─ Summary Manager（doc store）                                   │
│  └─ Document Meta Store                                             │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                  Match Engine / Vespa Search（C++ 检索）            │
│  searchlib/                                                          │
│  ├─ Matching（match-phase / rank-phase / rerank-phase）             │
│  ├─ Query Tree / YQL 解析                                          │
│  ├─ Query Planner / Optimizer                                      │
│  ├─ Ranking Framework（ONNX / XGBoost / 手写模型）                │
│  ├─ Tensor Fields（dense + sparse + mixed）                        │
│  ├─ HNSW Index（向量 ANN）                                          │
│  ├─ Posting Lists（倒排）                                          │
│  ├─ Attribute Vector（列存）                                        │
│  └─ Bitmap（过滤）                                                  │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                  存储层（Distributed File Store）                     │
│  - 文档存储（Summary + Attribute + Index 分层）                    │
│  - Tensor Block Files（向量原始数据 + 索引）                        │
│  - 分布式节点（Content Cluster）                                    │
│  - 文件系统 + 内存缓存（mmap）                                      │
└─────────────────────────────────────────────────────────────────────┘
```

### 关键设计取舍

- **计算-数据协同（Co-located Processing）**：搜索节点同时处理 query 和 data，无需单独的数据层；
- **三阶段匹配（Match / Rank / Rerank）**：粗召回 → 模型排序 → 精排，分层执行；
- **Tensor 字段原生**：向量作为一等公民，schema 即类型系统；
- **多阶段 Ranking**：可注入自定义 Java / Python / ONNX 模型；
- **Streaming Search**：实时数据流式索引（低延迟更新）；
- **HNSW 集成**：v8.x 引入 HNSW 作为 Tensor 字段索引类型；
- **容器化部署**：所有应用代码打包为 Application Package（Java JAR + 配置文件）；
- **可扩展性**：通过自定义 Searcher、Document Processor、RankProfile 注入业务逻辑。

## 七、存储引擎

### 7.1 Document Store（文档存储）

- **Summary Fields**：原始字段值，用于返回文档；
- **Attribute Vectors**：列存字段，用于过滤 / 排序 / 聚合；
- **Index（Posting List）**：倒排索引字段，用于全文检索；
- **Tensor Block Files**：向量字段，存原始 + HNSW 索引。

### 7.2 Summary Store

- `document::Document` 完整存储；
- 文件系统 + mmap；
- 用于 query 结果返回。

### 7.3 Attribute Vector（列存）

- 列式存储：每个字段独立文件；
- 支持 BTree / Hash / BitVector 索引；
- 用于 fast-search、sort、aggregation。

### 7.4 倒排索引

- **Posting List**：每 term 一条 posting list；
- 编码：**FST（Finite State Transducer）** 字典 + 压缩 posting；
- 源码：`searchlib/src/vespa/searchlib/index/`；
- 压缩：参考 Lucene 的 Packed Blocks + Skip List。

### 7.5 Tensor Block Files（向量存储）

- **类型系统**：`tensor<float>(x[16])`（稀疏/稠密/混合/绑定/索引）；
- 存储格式：自定义二进制 + mmap；
- 索引：HNSW（v8.x）；
- 源码：`searchlib/src/vespa/searchlib/tensor/`、`document/src/vespa/document/fieldvalue/tensorfieldvalue.cpp`。

## 八、索引机制（**向量类型核心**）

### 8.1 向量索引算法全景

| 索引 | 文件 | 备注 |
|------|------|------|
| **Flat** | `searchlib/tensor/` | 暴力精确 |
| **HNSW** | `searchlib/tensor/hnsw/` | v8.x 引入 |
| **Streaming Tensor** | `searchlib/streaming/` | 实时流式 |

### 8.2 Tensor 字段类型系统

Vespa 的向量字段是 **强类型 Tensor**，schema 定义：

```
schema article {
    document article {
        field title type string {
            indexing: summary | index
        }
        field embedding type tensor<float>(x[768]) {
            indexing: summary | attribute | index
            attribute: distance-metric: angular
        }
    }
}
```

**Tensor 类型**：
- **tensor<float>(x[N])**：稠密向量（dim=N）；
- **tensor<float>(x{}, y{})**：二维 tensor；
- **tensor<int8>(x[768])**：量化向量（v8.x）；
- **tensor<float>(x{}, x{})**：稀疏 tensor（稀疏 embedding）；
- **tensor<bfloat16>(x[768])**：bf16（v8.x）。

### 8.3 HNSW 详解

Vespa 在 v8.x 引入原生 HNSW 实现（基于 FAISS / hnswlib 思路）：

- **实现位置**：`searchlib/src/vespa/searchlib/tensor/hnsw_index/`；
- **距离度量**：angular（cosine）、euclidean、dot、hamming；
- **量化**：可选 int8 量化；
- **参数**：通过 schema 配置 `target-hnsw-max-links-per-node`、`target-hnsw-ef-construction`。

### 8.4 多阶段匹配（Match / Rank / Rerank）

```
┌──────────────────────────────────────────────────────────────┐
│  阶段 1：Match（粗召回）                                      │
│  - 倒排 + HNSW + 过滤                                       │
│  - 目标：高速筛选出 ~100-1000 候选                           │
└─────────────────────┬────────────────────────────────────────┘
                      │
┌─────────────────────▼────────────────────────────────────────┐
│  阶段 2：Rank（模型排序）                                    │
│  - 在候选上执行 ML 模型                                     │
│  - 目标：精准打分 ~100-200 候选                            │
│  - 支持：XGBoost / LightGBM / PyTorch / ONNX / 手写 Java   │
└─────────────────────┬────────────────────────────────────────┘
                      │
┌─────────────────────▼────────────────────────────────────────┐
│  阶段 3：Rerank（精排）                                      │
│  - 最复杂的模型（如 BERT / Cross-Encoder）                   │
│  - 目标：~10-50 顶级结果                                    │
└──────────────────────────────────────────────────────────────┘
```

**Ranking Framework**（`container-search/src/main/java/com/yahoo/search/ranking/`）：
- **RankProfile**：在 schema 中定义多阶段排序逻辑；
- **Expression Language**：自定义评分函数；
- **ML Integration**：ONNX / XGBoost / LightGBM / PyTorch 直接集成。

### 8.5 倒排 + 向量混合检索

Vespa 的混合检索通过 **YQL + RankProfile** 实现：

```sql
SELECT * FROM article 
WHERE 
  ({label:"title_embedding"} nearest(embedding, query_vector)) OR
  ({label:"title_text"} userQuery())
LIMIT 100
```

**实现位置**：`searchlib/src/vespa/searchlib/query/`、`searchlib/src/vespa/searchlib/aggregation/`。

### 8.6 ColBERT / ColPali 多向量

- **v8.x** 支持 **ColBERT 风格多向量**（每文档多个 token 向量）；
- **API**：`tensor<float>(t{}, x[128])`（t 是 token 维度）；
- **距离**：MaxSim（query token 与 doc token 的最大相似度求和）；
- **实现**：`searchlib/src/vespa/searchlib/tensor/colbert_distance.h`。

### 8.7 量化方案

| 量化 | 字段类型 | 压缩比 |
|------|----------|--------|
| **int8** | `tensor<int8>(x[N])` | 1/4 |
| **bfloat16** | `tensor<bfloat16>(x[N])` | 1/2 |
| **二值化** | 实验 | 1/32 |

## 九、查询/计算/接口层

### 9.1 YQL（Vespa Query Language）

Vespa 的查询语言，类 SQL：

```sql
SELECT title, body FROM article 
WHERE 
  title contains "vespa" AND
  embedding nearest(embedding, [0.1, 0.2, ...]) 
  AND rankfeatures(user) > 0.5
LIMIT 10
```

### 9.2 REST API

```
GET  /search/v1/?yql=...
POST /document/v1/{namespace}/{documenttype}/docid/{id}
POST /document/v1/{namespace}/{documenttype}/feed
GET  /document/v1/{namespace}/{documenttype}/docid/{id}
```

### 9.3 gRPC（部分）

- 文档操作：gRPC；
- 检索：HTTP（YQL over HTTP）。

### 9.4 SDK 生态

| 语言 | 包名 |
|------|------|
| Java | `ai.vespa:vespaclient` |
| Python | `pyvespa`（官方） |
| Node.js | `vespa-node`（社区）|
| C++ | Vespa 内部 + C API |

### 9.5 Application Package（应用打包）

```
app/
├── services.xml          # 容器配置
├── schemas/article.sd   # 集合 schema
├── rank-profiles/        # 排序模型
├── components/           # 自定义 Java 组件
├── models/               # ONNX 模型
└── searchdefinitions/    # YQL 查询模板
```

### 9.6 ML 模型集成

- **ONNX**：原生集成，v8.x 支持；
- **XGBoost / LightGBM**：原生；
- **PyTorch**：通过 TorchScript / ONNX 转换；
- **TensorFlow**：通过 SavedModel / ONNX；
- **LLM 集成**：v8.x 内置 Generative AI 集成（OpenAI、HuggingFace）；
- **GPU 推理**：v8.x 支持 ONNX GPU。

### 9.7 多租户

- **Application**：每个 Application 独立；
- **Schema** 内可定义多个 document types；
- **多租户**通过 `tenant` namespace 隔离。

## 十、事务与一致性

- **写入**：Document API → Bucket → Proton → Summary + Index；
- **Streaming Mode**：实时索引，低延迟可见（毫秒级）；
- **Indexed Mode**：批量索引，可见性延迟（秒级）；
- **一致性**：默认最终一致，**Write Set** 保证同一 bucket 内一致性；
- **崩溃恢复**：从 attribute + index 文件恢复。

## 十一、复制与高可用

- **Group 副本**：每个 bucket 多副本（默认 2）；
- **Distributor**：决定路由到哪个节点；
- **Redundancy**：可配置副本数；
- **Fleet Controller**：集群状态管理；
- **Config Server**：动态配置分发给节点；
- **Vespa Cloud**：跨区域 HA。

## 十二、扩展性

- **垂直扩展**：增加节点内存 + CPU；
- **水平扩展**：
  - **Cluster**：增加 Content Cluster 节点；
  - **Container Cluster**：独立扩展容器层；
  - **Streaming Index**：动态分片；
- **冷热分层**：
  - 倒排 + 量化向量常驻内存；
  - Summary + 原始向量可冷存到磁盘；
  - **Visit API**：批量遍历冷数据。

## 十三、性能与基准

- **Vespa Benchmark Suite**（`tests/performance/`）：包含多个标准 benchmark；
- **Streaming Search**：毫秒级索引可见；
- **HNSW 查询**：百万级向量约 **1-5ms / query**；
- **三阶段匹配**：支持亿级文档下的 sub-second 检索；
- **GPU 推理**：ONNX 模型推理可加速 10-100x；
- **多模型融合**：XGBoost + BERT 联合打分典型 50-200ms。

## 十四、运维与可观测性

- **Metrics**：Prometheus（`/metrics` 端点）；
- **Logs**：结构化日志；
- **Tracing**：OpenTelemetry；
- **Inspect API**：`/inspect/v1/` 提供集群状态；
- **Application API**：`/application/v2/` 管理应用包部署；
- **Config Server**：动态配置；
- **Vespa Cloud Console**：可视化运维。

## 十五、生态与集成

- **ML 框架**：PyTorch / TensorFlow / ONNX / XGBoost / LightGBM；
- **LLM 集成**：OpenAI、HuggingFace、Cohere（v8.x）；
- **Embedding**：Sentence-Transformers / OpenAI / Cohere；
- **可视化**：Vespa Cloud Console + Grafana；
- **部署**：Docker Compose、Kubernetes Operator、Helm；
- **监控**：Prometheus + Grafana；
- **CI/CD**：Vespa CLI 工具链。

## 十六、核心论文与算法原理

- **HNSW**：Malkov & Yashunin, 2016；
- **PQ**：Jégou et al., 2011；
- **ColBERT / late interaction**：Khattab & Zaharia, 2020；
- **Two-stage Retrieval**：Craswell et al., 2020；
- **Learning to Rank**：Liu et al., 2009；
- **Two Towers Model**：神经检索经典结构；
- **Tensor Computation**：参考 numpy / TensorFlow；
- **FST**：Lucene 风格索引。

## 十七、源码学习路径

| 优先级 | 模块 | 路径 | 说明 |
|--------|------|------|------|
| P0 | YQL 解析 | `searchlib/src/vespa/searchlib/query/` | 查询语言 |
| P0 | Match Engine | `searchlib/src/vespa/searchlib/matching/` | 检索核心 |
| P0 | HNSW | `searchlib/src/vespa/searchlib/tensor/hnsw_index/` | 向量索引 |
| P0 | Proton | `searchcore/src/main/java/com/yahoo/search/core/proton/` | 搜索节点 |
| P1 | Document API | `documentapi/` | 写入 |
| P1 | Container | `container-core/` | 容器层 |
| P1 | Ranking | `container-search/src/main/java/com/yahoo/search/ranking/` | 排序 |
| P1 | Tensor Fields | `document/src/vespa/document/fieldvalue/tensorfieldvalue.cpp` | Tensor 类型 |
| P2 | Streaming | `streaming-search/` | 实时搜索 |
| P2 | Distributor | `distributor/` | 分片路由 |
| P2 | Config | `configserver/` | 配置管理 |

## 十八、源码规模与开发语言

- **Java**：约 65% LOC（Container + Proton + Ranking）；
- **C++**：约 30% LOC（Search Core + 存储）；
- **Python**：约 3% LOC（pyvespa + 工具）；
- **Proto / XML**：约 2% LOC（接口 + 配置）；
- **构建系统**：Maven + CMake；
- **CI**：GitHub Actions；
- **LOC 总量**：约 150 万行（Vespa 是大型项目）。

## 十九、学习资源

- **官方文档**：https://docs.vespa.ai/
- **GitHub**：https://github.com/vespa-engine/vespa
- **博客**：https://blog.vespa.ai/
- **Slack**：https://slack.vespa.ai/
- **课程**：Vespa 官方教程；
- **应用样例**：https://github.com/vespa-engine/sample-apps；
- **书籍**：《Vespa Documentation》（在线文档极完善）；
- **YouTube**：Vespa 官方频道。

## 二十、可移植性与部署

| 平台 | 支持 |
|------|------|
| Linux x86_64 / ARM64 | 原生 |
| macOS | 开发模式（Docker 推荐）|
| Docker | 官方镜像（多组件）|
| K8s | 官方 Vespa Operator + Helm |
| Vespa Cloud | 全托管 |

## 二十一、变体与衍生项目

- **Vespa Cloud**：NTT 全托管；
- **Vespa Engine**：核心开源版本；
- **sample-apps**：官方应用样例库；
- **Vespa CLI**：命令行工具；
- **pyvespa**：Python SDK + 笔记本实验。

## 二十二、风险与陷阱

1. **部署复杂**：完整 Vespa 至少需要 Config Server + Container + Content Cluster 多角色；
2. **Application Package**：所有应用代码必须打包为 Application Package，调试不便；
3. **HNSW 仅 v8+**：老版本无原生 HNSW，需要手动集成；
4. **Schema 修改成本**：Schema 变更可能触发全量重建索引；
5. **Streaming Mode 资源**：实时索引消耗更多资源；
6. **ML 模型部署**：PyTorch 模型需转换 ONNX；
7. **YQL 学习曲线**：类 SQL 但有独特语法；
8. **多租户隔离**：通过 namespace 隔离，资源竞争仍可能；
9. **Streaming Search 与 Indexed Mode 选择**：场景匹配不当会导致延迟异常；
10. **Backup 策略**：需要自定义 Snapshot 工具。

## 二十三、与本项目的关联

- **`engineering/src/index/`** 已实现 HNSW，可参考 Vespa 的 Match/Rank/Rerank 三阶段设计；
- **`engineering/rag/`** 中 RAG 系统可对接 Vespa 作为端到端后端（向量 + ML 推理）；
- **学习价值**：Vespa 是 **"文档 + 向量 + ML 推理一体化"** 的工业级参考；
- **计算-数据协同**：Vespa 的设计思路对 MiniVecDB 的端到端优化有启发；
- **`learning/notes/`** 可沉淀 Vespa 应用打包 + ML 推理经验。

## 二十四、参考资料

- [Vespa GitHub](https://github.com/vespa-engine/vespa)
- [Vespa 官方文档](https://docs.vespa.ai/)
- [Vespa Cloud](https://cloud.vespa.ai/)
- [Vespa Blog](https://blog.vespa.ai/)
- [HNSW 论文](https://arxiv.org/abs/1603.09320)
- [ColBERT 论文](https://arxiv.org/abs/2004.12832)
- [Two-stage Retrieval 综述](https://arxiv.org/abs/2004.14552)
- [ONNX 格式](https://onnx.ai/)
- [ANN-Benchmarks](https://github.com/erikbern/ann-benchmarks)
- [Vespa Sample Apps](https://github.com/vespa-engine/sample-apps)
- [PyVespa](https://pyvespa.readthedocs.io/)