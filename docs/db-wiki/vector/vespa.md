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

## 二十四、向量索引核心：Vespa 自研 HNSW 的构建与搜索

Vespa 的 HNSW 是 **C++ 原生实现**（不是对 hnswlib / FAISS 的包装），主体位于 `searchlib/src/vespa/searchlib/tensor/` 下的 `hnsw_index.{h,cpp}`、`hnsw_graph.{h,cpp}`、`hnsw_multi_best_neighbors.h`、`inv_log_level_generator.h`。类继承 `NearestNeighborIndex`，按 `HnswIndexType` 模板参数分化为 `SINGLE`（一文档一向量）与 `MULTI`（一文档多个 subspace 节点）两套。

### 并发模型：1 写线程 + N 读线程，不用互斥锁

类注释直接写明（`searchlib/src/vespa/searchlib/tensor/hnsw_index.h:34`）：

> The implementation supports 1 write thread and multiple search threads **without the use of mutexes**.

实现手段是**带 generation tracking 的数据存储**：节点、层数组、边数组都存在 `vespalib::datastore::ArrayStore` 里，句柄 `EntryRef` 携带代际号；读者走 `acquire_*`（acquire 序），写者整体替换数组并推进 generation，旧 generation 未回收前老读者仍安全。判定边是否已失效用 `still_valid(nodeid, levels_ref)`（`searchlib/src/vespa/searchlib/tensor/hnsw_graph.h:82`）。入口点则把 nodeid 与 level 打包进一个 `uint64_t` 原子量 `entry_nodeid_and_level`（`searchlib/src/vespa/searchlib/tensor/hnsw_graph.h:58`）。

### 层级生成：几何分布

`searchlib/src/vespa/searchlib/tensor/inv_log_level_generator.h:12` 给出分布：

```
Pr(level = k) = (1/M)^k · (1 − 1/M)
level = floor( −ln(1 − u) / ln M ),   u ~ U(0,1)
```

实现即 `double r = -log(1.0 - unif) * _levelMultiplier;`（`searchlib/src/vespa/searchlib/tensor/inv_log_level_generator.h:36`），其中 `_levelMultiplier = 1/ln(M)`（`searchlib/src/vespa/searchlib/tensor/inv_log_level_generator.h:32`）。两点值得注意：

- **固定随机种子** `0x1234deadbeef5678uLL`（同文件 `:32`）—— 层级分配完全可复现，重建索引得到同样的图，便于回归对比；
- 理论无上界，Vespa 硬钳到 `max_max_level = 29`（`searchlib/src/vespa/searchlib/tensor/hnsw_index.h:169`），正好卡在 uint64 打包的位宽内。

### 分层的 max links：schema 一个参数，落成两个值

schema 侧只暴露 `maxLinksPerNode`（默认 16，`config-model/src/main/java/com/yahoo/schema/document/HnswIndexParams.java:13`）和 `neighborsToExploreAtInsert`（默认 200，即 `efConstruction`，同文件 `:14`），但在 C++ 侧落成五个字段（`searchlib/src/vespa/searchlib/tensor/default_nearest_neighbor_index_factory.cpp:34`）：

```
max_links_at_level_0                = 2m        (m = 16 → 32)
max_links_on_inserts                = m         (新节点每层最多 m 条出边)
neighbors_to_explore_at_construction= 200
min_size_before_two_phase           = 10000
heuristic_select_neighbors          = true
```

取用规则只有一行（`searchlib/src/vespa/searchlib/tensor/hnsw_index.cpp:151`）：

```
max_links(level) = (level == 0) ? max_links_at_level_0 : max_links_on_inserts
```

即 **level-0 的容量是上层的 2 倍** —— 这是对 Malkov 论文 `M` 与 `M0 = 2M` 约定的直接落地，上层稀疏、下层稠密以保证导航精度。

### 搜索：贪心降层 + 自适应 beam

```
top_k_candidates(q, k):                            # searchlib/src/vespa/searchlib/tensor/hnsw_index.cpp:1077
    e = entry_node();  level = e.level
    while level > 0:                               # :1093
        e = find_nearest_in_layer(e, level)        # 纯贪心，扫完整条邻居链表
        level -= 1
    best ← [e]
    search_layer(best, k, level=0)                 # :1102
    return best

search_layer(best, k, level, filter):              # :364
    limit ← +inf
    while cand = candidates.pop_min():
        if cand.d > (1 + exploration_slack) * limit:  break       # :397 自适应 beam 剪枝
        for nb in links(cand, level):
            if visited(nb): continue
            d = dist(q, vec(nb))                                  # :430
            if d < (1 + exploration_slack) * limit:
                push candidates
                if d < limit and filter.check(docid(nb)):         # :435
                    best.push(nb)
                    while |best| > k: pop_furthest; limit = best.top.d
```

降层阶段 `find_nearest_in_layer`（`searchlib/src/vespa/searchlib/tensor/hnsw_index.cpp:339`）是最朴素的贪心：反复扫当前点邻居，只要发现更近的就跳过去，直到一轮无改进。

与 hnswlib 固定 `ef_search` 不同，Vespa 把 beam 的放行条件从 `cand.d > limit` 放宽成 `cand.d > (1 + exploration_slack) * limit`（`:397` 与 `:432`）。`exploration_slack = 0` 时退化为标准行为；调大则召回上升、延迟上升。

**复杂度**：降层 `O(L · M)` 次距离计算（L ≤ 29，实际 `L ≈ log_M N`）；layer-0 beam 宽 k，每访问一个节点扫 ≤ M 条边，距离计算 `O(V · M)`，V 是实际访问节点数（与 ef、图的稠密程度相关）。由于上层 M、上层节点数按 `(1/M)^k` 衰减，整条降层链的成本被压在 `O(M · log_M N)` 量级。

**VisitedTracker 二选一**：搜索前先用 `estimate_visited_nodes` 估算要访问多少节点（`searchlib/src/vespa/searchlib/tensor/hnsw_index.cpp:316`），估算值 ≥ `nodeid_limit / 128` 就用位图，否则用哈希表（`searchlib/src/vespa/searchlib/tensor/hnsw_index.cpp:624`）—— 小图用哈希省内存，大图用位图省哈希开销。

### 邻居选择：heuristic vs simple

```
select_neighbors(cands, max_links):               # :223
    heuristic_select_neighbors ? heuristic : simple

select_neighbors_heuristic:                      # :195
    nearest ← 全部候选的最近优先队列
    while c = nearest.pop_nearest():
        if have_closer_distance(c, result.used):  # :205
            result.unused.push(c); continue
        result.used.push(c)
        if |result.used| == max_links: 把 nearest 里剩余全部归入 unused; break
```

`have_closer_distance`（`searchlib/src/vespa/searchlib/tensor/hnsw_index.cpp:155`）逐个比较已选集合，只要存在一个点 `dist < candidate.distance` 就判定该候选冗余并丢弃 —— 这就是 HNSW 论文的 *keep-diverse* 剪枝：宁可少连一个近邻，也不制造聚团的边。

**复杂度对比**：`select_neighbors_simple`（`searchlib/src/vespa/searchlib/tensor/hnsw_index.cpp:178`）只是 `std::sort` 后取前 max_links 个（`:181`），`O(ef·log ef)`；heuristic 版每个候选要扫一遍已选集合，`O(ef · max_links)`，换来的是显著更好的图连通性与召回。

### 插入：两阶段 put（无锁并发索引）

**Prepare 阶段（只读，可并发）** —— `internal_prepare_add_node`（`searchlib/src/vespa/searchlib/tensor/hnsw_index.cpp:704`）：

```
node_max_level = min(level_generator.max_level(), 29)          # :707
while search_level > node_max_level:                             # 降到本次节点的最高层
    e = find_nearest_in_layer(e, search_level); search_level -= 1
for level = min(node_max_level, search_level) … 0:               # :729
    search_layer(best, neighbors_to_explore_at_construction, level)   # :730  efConstruction = 200
    connections[level] = select_neighbors(best, max_links_on_inserts) # :732
```

**Complete 阶段（单写线程）** —— `internal_complete_add_node`（`searchlib/src/vespa/searchlib/tensor/hnsw_index.cpp:779`）：

```
levels_ref = make_node(nodeid, docid, subspace, |connections|)
for level in 0 … n-1:
    neighbors = filter_valid_nodeids(level, connections[level], nodeid)   # :750 / :757
    connect_new_node(nodeid, neighbors, level)                            # :785
if n-1 > entry_level: set_entry_node({nodeid, levels_ref, n-1})           # :787
```

`filter_valid_nodeids` 用 prepare 阶段随边一起记下的 `EntryRef` 做 `still_valid()` 校验（`searchlib/src/vespa/searchlib/tensor/hnsw_index.cpp:757`）—— prepare 与 complete 之间别的线程可能已删掉或覆盖该节点，失效的边直接丢弃。**这是 lock-free 并发插入的正确性来源**：prepare 阶段的搜索结果允许丢失、允许过时，complete 阶段只保证不写入悬垂边，且新节点仍会被接到当时的图上。

`connect_new_node`（`:256`）负责双向化：先给新节点写边 → 给每个邻居补反向边 → 邻居超限时跑 `shrink_if_needed`（`:231`，重跑一次 `select_neighbors`，并对被淘汰者调 `remove_link_to` 清理反向边）。被淘汰的边必须清理，否则图会出现单向边，导航质量断崖式下降。

开关：schema 的 `hnsw.multithreadedindexing`，**默认 true**（`config-model/src/main/java/com/yahoo/schema/document/HnswIndexParams.java:74`），落到 C++ 的 `HnswIndexParams::multi_threaded_indexing()`（`searchlib/src/vespa/searchcommon/attribute/hnsw_index_params.h:19`），Proton 写入侧据此决定是否走 two-phase put（`searchcore/src/vespa/searchcore/proton/attribute/attribute_writer.cpp:47`）。

**边界条件**：`active_nodes < min_size_before_two_phase`（= 10000）时退回单线程全量插入（`searchlib/src/vespa/searchlib/tensor/hnsw_index.cpp:796`）—— 图太小时并发插入的新节点互相看不见（竞态窗口内都搜不到对方），会把图撕成几个互不相通的小片，破坏 HNSW 的可导航性。

### 删除：HNSW 论文不覆盖的部分

Vespa 补了 `mutual_reconnect`（`searchlib/src/vespa/searchlib/tensor/hnsw_index.cpp:821`）：把被删节点的所有邻居**两两配对**算距离、排序（`:840`），在双方容量允许的前提下补回边，且用 `already_added` 集合保证每个节点最多被补 1 条边（`:843`），避免删除引发的边爆炸。若删掉的正是入口点，则顺位挑一个邻居提升为新入口（`:872`）。

### 多向量索引：k 的语义是「文档数」而非「节点数」

`HnswIndexType::MULTI` 时一篇文档对应 N 个 subspace 节点（ColBERT 风格）。`HnswMultiBestNeighbors` 在候选优先队列之外额外维护一张 `_docids` 引用计数表（`searchlib/src/vespa/searchlib/tensor/hnsw_multi_best_neighbors.h:18`），`pop()` 时递减计数、归零才真正移除（`searchlib/src/vespa/searchlib/tensor/hnsw_multi_best_neighbors.h:30`），于是 `size()` 返回**去重后的文档数**。这直接决定了 ColBERT 场景下 `targetHits` 的单位是「篇文档」，而不是「个 token 向量」—— 少一层去重，结果里就会挤满同一篇文档的多个 token 向量。

## 二十五、近似 ↔ 精确的切换：过滤与 HNSW 的联合算法

### 决策点不在 HNSW 内部，而在 blueprint

`NearestNeighborBlueprint`（`searchlib/src/vespa/searchlib/queryeval/nearest_neighbor_blueprint.h:32`）的 `Algorithm` 枚举只有 4 个值，可读字符串见 `searchlib/src/vespa/searchlib/queryeval/nearest_neighbor_blueprint.cpp:29`：

| Algorithm | 含义 |
|-----------|------|
| `EXACT` | 暴力精确搜索 |
| `EXACT_FALLBACK` | 过滤后候选太少，放弃 HNSW 退回暴力 |
| `INDEX_TOP_K` | 走 HNSW，无过滤 |
| `INDEX_TOP_K_WITH_FILTER` | 走 HNSW，带 global filter |

### 第一步：要不要建 global filter（三段式）

`Query::handle_global_filter` 的注释把策略写死了（`searchcore/src/vespa/searchcore/proton/matching/query.h:123`）：

```
1) estimated_hit_ratio <  global_filter_lower_limit :  不做任何事（最终落 EXACT_FALLBACK）
2) estimated_hit_ratio <= global_filter_upper_limit :  计算 global filter 并装到 blueprint
3) estimated_hit_ratio >  global_filter_upper_limit :  装 "match all" filter
```

只有「存在 NN 索引」且「允许近似」时才需要这个 filter（`searchlib/src/vespa/searchlib/queryeval/nearest_neighbor_blueprint.cpp:119`）。

落到 `set_global_filter`（`searchlib/src/vespa/searchlib/queryeval/nearest_neighbor_blueprint.cpp:129`）分两种情况：

- **pre-filter**（global filter 已确定）：`hit_ratio = filter.count() / num_docs`；若 < `global_filter_lower_limit` 就置 `EXACT_FALLBACK`（`:141`）—— 图上能命中的点太少，扫图的成本不如直接暴力。
- **post-filter**（filter 还没定）：按预估命中率反向放大 k，以补上「先召回再过滤」的损耗：

```
adjusted_target_hits = min( target_hits / estimated_hit_ratio,
                           target_hits · target_hits_max_adjustment_factor )
```

（`searchlib/src/vespa/searchlib/queryeval/nearest_neighbor_blueprint.cpp:152`）默认 `target_hits_max_adjustment_factor = 20`（`searchlib/src/vespa/searchlib/fef/ranksetup.cpp:77`），即最多为过滤造成的不足额外多搜 20 倍 k，硬顶住延迟上限。

### 第二步：命中比低 → 切 filter-first（ACORN-1）

`perform_top_k`（`searchlib/src/vespa/searchlib/queryeval/nearest_neighbor_blueprint.cpp:196`）三条分支：

```
if lazy_filter && active:                                    # :201
    use_filter = (global_filter && lazy_filter)
                  ? FallbackFilter(global, lazy)             # :204
                  : lazy_filter
    low_hit_ratio = use_filter.count() / num_docs < filter_first_upper_limit   # :208
    find_top_k_with_filter(..., low_hit_ratio, ...)
elif global_filter.active:                                   # :214
    low_hit_ratio = global_filter_hit_ratio < filter_first_upper_limit          # :215
    find_top_k_with_filter(...)
else:                                                        # :221
    find_top_k(...)                                          # 无过滤纯 ANN
```

`low_hit_ratio` 为真时，HNSW 改走 `search_layer_filter_first`（`searchlib/src/vespa/searchlib/tensor/hnsw_index.cpp:1098`）。这是 Vespa 最有特色的检索路径，YQL 侧常量注释直接把它标注为 **'acorn-1'**（`container-search/src/main/java/com/yahoo/search/yql/YqlParser.java:191`）。

**为什么必须切**：普通 `search_layer_helper`（`searchlib/src/vespa/searchlib/tensor/hnsw_index.cpp:364`）里 filter 只作用在「是否把候选放进 best」这一步（`:435`）—— **不通过过滤的点仍然被 push 进 candidates 队列继续参与图遍历**。这是典型 post-filter 语义，当过滤选择率 r 很低时（比如 1%），为了凑够 k 个合规结果几乎要遍历全图，收益为负。

filter-first 版本（`search_layer_filter_first_helper`，`searchlib/src/vespa/searchlib/tensor/hnsw_index.cpp:453`）改成 **filter 先行 + 多跳扩邻**：

```
exploreNeighborhood(cand, level):                      # :543
    todo ← [cand]
    exploreNeighborhoodByOneHop(...)   # 1-hop          # :554
    exploreNeighborhoodByOneHop(...)   # 2-hop          # :558
    if |todo| < exploration · max_links²:              # :564
        exploreNeighborhoodByOneHop(...)   # 3-hop（仅稀疏区触发）

exploreNeighborhoodByOneHop:                           # :572
    for 每个本轮快照中的节点（不展开新推入的，见 :577）:
        for nb in links(node, level):
            todo.push(nb)                              # 新发现的下一跳无条件下探
            if !visited(nb) and filter.check(docid(nb)):   # :604
                found.push(nb)
                if |found| ≥ max_links_for_level(level): return   # :608 收满即止
```

关键差异：**只有通过过滤的节点才会被算距离**，被过滤掉的节点连距离都不算（普通版本是算完距离才判断）。跳数按 `|todo| < exploration · max_links²` 自适应收敛 —— 1 跳能凑够就只做 1 跳，只有在图稀疏区才升到 2 跳 / 3 跳。

**复杂度**：命中比 r 时，1 跳期望找到 `r · M` 个合规点，2 跳 `r · M²`，所以 3 跳必须被条件卡住。默认 `filter_first_exploration = 0.01`（`searchlib/src/vespa/searchlib/fef/ranksetup.cpp:75`），只有 `|todo|` 小于 M² 的百分之一才触发。代价从「遍历全图」降到近似 `O(M²)`，但要求 r 不能小到连 2 跳都填不满 —— 这也正是上一层 `filter_first_upper_limit`（默认 0.2）存在的意义。

### 第三步：搜索前的预算估计

`estimate_visited_nodes`（`searchlib/src/vespa/searchlib/tensor/hnsw_index.cpp:316`）在真正开搜前估一把要访问多少节点，用来选 VisitedTracker 的实现：

```
base    = max_links(level) · neighbors_to_find + 100
若 filter 生效:
    true_bits = filter.count()
    若 true_bits == 0 → 返回 nodeid_limit（直接判「要全扫」）
    estimate = base · filter.size() / true_bits      # :330 按选择率 1/r 放大
```

估算值 ≥ `nodeid_limit/128` 就切位图（`:624`）。这一段同时也是「过滤后会不会无解」的早期信号。

### 参数速查：YQL 注解 → rank property → 引擎默认值

| YQL 注解 | rank property | 引擎默认 | 作用 |
|---|---|---|---|
| `approximate` | — | `true` | 置 false 强制 EXACT（`container-search/src/main/java/com/yahoo/prelude/query/NearestNeighborItem.java:28`） |
| `hnsw.approximateThreshold` | `vespa.matching.global_filter.lower_limit` | 0.05 | filter 命中比低于它 → 强制暴力 |
| `hnsw.postFilterThreshold` | `vespa.matching.global_filter.upper_limit` | 1.0 | 命中比高于它 → 走 post-filter |
| `hnsw.filterFirstThreshold` | `vespa.matching.nns.filter_first_upper_limit` | 0.2 | 命中比低于它 → 启用 filter-first |
| `hnsw.filterFirstExploration` | `vespa.matching.nns.filter_first_exploration` | 0.01 | 3 跳扩邻的激进程度 |
| `hnsw.explorationSlack` | `vespa.matching.nns.exploration_slack` | 0.0 | 自适应 beam 宽度 |
| `hnsw.targetHitsMaxAdjustmentFactor` | `vespa.matching.nns.target_hits_max_adjustment_factor` | 20.0 | post-filter 时 k 的最大放大倍数 |

注解名与默认值的权威声明在 `container-search/src/main/java/com/yahoo/search/yql/YqlParser.java:187`（`approximateThreshold`）、`container-search/src/main/java/com/yahoo/search/yql/YqlParser.java:191`（`filterFirstThreshold`）、`container-search/src/main/java/com/yahoo/search/yql/YqlParser.java:192`（`postFilterThreshold`），到 rank property 的落点在 `container-search/src/main/java/com/yahoo/search/query/ranking/Matching.java:165`。⚠️ 两处默认值不一致：`filterFirstExploration` 的 YQL 常量注释写 `0.3`，而 C++ 引擎侧 `RankSetup` 的初值是 `0.01`（`searchlib/src/vespa/searchlib/fef/ranksetup.cpp:75`）—— 实际生效值取决于是否显式下发 rank property，排障时以 query profile 的 `matching` 段为准。

### 易混淆：两个同名不同义的 filter threshold

1. **查询规划器级** `filter-threshold`（rank property `vespa.matching.filter_threshold`，见 `searchlib/src/vespa/searchlib/fef/indexproperties.cpp:396`）：决定布尔查询里的某个 term 算不算「过滤器」。判定式是相对文档频率比较（`searchlib/src/vespa/searchlib/fef/filter_threshold.h:30`）：
   ```
   is_filter(rel_doc_freq) = rel_doc_freq > threshold
   ```
   即罕见 term（相对 DF 小）当过滤器展开成 bitmap，常见 term 才展开成候选集参与打分。
2. **HNSW 级** `hnsw.filterFirstThreshold` / `hnsw.approximateThreshold`：管的是走哪条检索路径（见上表）。

命名几乎一样但作用层次完全不同，排查召回问题时要先确认改的是哪个。

### MULTI 向量下 filter 语义的差异

`GlobalFilterWrapper` 有 SINGLE / MULTI 两个模板特化：

- **SINGLE**：`check(docid)` 直接查 bitmap，且 `clamp_nodeid_limit` 把 nodeid 上限压到 `filter.size()`（`searchlib/src/vespa/searchlib/tensor/hnsw_index.h:98`）—— nodeid 与 docid 一一对应，可以直接当数组下标；
- **MULTI**：nodeid 与 docid 不再一一对应，额外记一份 `_docid_limit`，`check` 里先做 `docid < _docid_limit` 的范围检查再查 bitmap（`searchlib/src/vespa/searchlib/tensor/hnsw_index.h:113`），且 `clamp_nodeid_limit` 是空实现。

## 二十六、距离度量家族与向量量化

### 7 种距离度量

schema 层枚举（`config-model/src/main/java/com/yahoo/schema/document/Attribute.java:44`）：

```
EUCLIDEAN, ANGULAR, GEODEGREES, INNERPRODUCT, HAMMING, PRENORMALIZED_ANGULAR, DOTPRODUCT
```

默认 `EUCLIDEAN`（`config-model/src/main/java/com/yahoo/schema/document/Attribute.java:210`），C++ 侧一一对应（`searchlib/src/vespa/searchcommon/attribute/distance_metric.h:9`）。工厂按 `metric × cell_type × 是否量化` 三重分派（`searchlib/src/vespa/searchlib/tensor/distance_function_factory.cpp:15`），每个度量都有 double / float / bfloat16 / int8 四个特化。

一个容易漏的映射：`INNERPRODUCT` 与 `PRENORMALIZED_ANGULAR` 落到**同一个** `PrenormalizedAngularDistanceFunctionFactory`（`searchlib/src/vespa/searchlib/tensor/distance_function_factory.cpp:44`）—— 因为在向量已 L2 归一化的前提下内积与余弦恒等，两个 schema 关键字是同义词；而 `ANGULAR` 会自己算归一化。

### `dotproduct` 不是普通内积，而是 MIPS 变换

`DistanceMetric::Dotproduct` 分派到 `MipsDistanceFunctionFactory`（`searchlib/src/vespa/searchlib/tensor/distance_function_factory.cpp:57`）。设计动机写在头注释里（`searchlib/src/vespa/searchlib/tensor/mips_distance_transform.h:55`）：

> Factory for distance functions which can apply a transformation **mapping Maximum Inner Product Search to a nearest neighbor problem**. When inserting vectors, **an extra dimension is added** ensuring behavior "as if" all vectors had length equal to the longest vector inserted so far, or at least length 1.

记 `L²` 为「到目前为止见过的最大平方范数」，补维值 `e_v = sqrt(max(0, L² − ‖v‖²))`。两个实现细节必须分清：

- **插入 / 建图侧**（`extra_dim = true`，`searchlib/src/vespa/searchlib/tensor/mips_distance_transform.cpp:69`）：距离 = `−(⟨a,b⟩ + e_a·e_b)`（`searchlib/src/vespa/searchlib/tensor/mips_distance_transform.cpp:52` 与 `:54`）。补维后 `[a, e_a]` 与 `[b, e_b]` 的长度都恰好是 L，因为 `‖a‖² + e_a² = L²`，所以该式 = `−L²·cos(â, b̂)` —— 一个**良定义的距离**，供图构建使用。最大内积问题之所以不能直接建图，就是因为内积不是度量（可为负、破坏三角不等式），补维等长正是把这个缺陷抹平。
- **查询侧**（`extra_dim = false`，`searchlib/src/vespa/searchlib/tensor/mips_distance_transform.cpp:65`）：距离 = `−⟨q, b⟩`，不补维，等价于把 query 的补维填 0，即**纯最大内积排序**。

配套的一致性要求：`L²` 是**在线递增**的（`MaximumSquaredNormStore::get_max`，mutex 保护，`searchlib/src/vespa/searchlib/tensor/mips_distance_transform.h:36`），所以索引保存 / 加载时必须把当时的 max 一并写进 header（`searchlib/src/vespa/searchlib/tensor/hnsw_index.cpp:1020` 的 `save_mips_max_distance`）—— 否则重建索引后的图结构与线上不一致，召回会静默漂移。初值固定为 `1.0`（`searchlib/src/vespa/searchlib/tensor/mips_distance_transform.h:28`），保证即使所有向量都极短或为零，根号内也不会为负。

### 向量量化：EDEN，1~4 bit + Hadamard 旋转

**类型层**（`searchlib/src/vespa/searchlib/tensor/tensor_quantization.h:14`）：对每个 dense subspace **单独**量化，把该 subspace 当作一个 `d` 维向量处理，从而可以跨 indexed 维度做最大密度的 bit packing；量化后的 tensor 类型与原类型形状不同 —— mapped 维度原样保留，所有 indexed 维度被替换成一个聚合的 int8 维度（名字复用字典序最后一个 indexed 维度）。完全稀疏的 tensor 不支持量化。

**量化器**是 EDEN（*Efficient Distributed Mean Estimation for diverse Networks*，`vespalib/src/vespa/vespalib/quant/eden.h:27`），支持 **1~4 bit/坐标**，配 **Hadamard 旋转**（`vespalib/src/vespa/vespalib/quant/eden.h:66` 的 `_rotator`）先做正交变换消除坐标间相关性，让非均匀码本的能量分配更均衡。存储布局（`vespalib/src/vespa/vespalib/quant/eden.h:46`）：`d·b` bit 向上取整到整字节 **+ 一个 4 字节 f32 scale**。

**算法**（`vespalib/src/vespa/vespalib/quant/eden.cpp`）：

```
y ← Rotator(x)                              # Hadamard 正向旋转
对 i in 1..d:
    idx_i = argmin_c |y_i · sqrt(d) − c|     # :147  在码本上取最近质心，预先按 sqrt(d) 缩放
    q_i   = codebook[idx_i]
yq   = ⟨y, q⟩                               # q 虚拟保存，不落盘

InnerProduct 模式:  scale = ‖y‖² / yq       # :152   EDEN-unbiased
MSE 模式:          scale = yq / ‖q‖²       # :162   EDEN-biased，最小二乘最优

落盘: [4B scale][idx 位打包]
反量化: x̂_i = codebook[idx_i] · scale，再 Rotator.inverse        # :176
```

**两种模式的差别是这个量化器最有价值的部分**（`vespalib/src/vespa/vespalib/quant/eden.h:15`）：MSE 模式最小化重建误差，但内积对重建误差的敏感度是非对称的，用它算 MIPS 会引入**系统性偏差**；`QuantMode::InnerProduct` 的 scale 取法（用 ‖y‖² 而非 ‖q‖² 作分子）就是专门消掉这个偏差的。**选 `dotproduct` 度量必须配 InnerProduct 量化模式**，否则 top-k 会系统性偏向某一类向量长度。

量化路径同样有度量特化：`QuantizedMipsDistanceFunctionFactory`（`searchlib/src/vespa/searchlib/tensor/distance_function_factory.cpp:109`）。注意 query 向量**始终**提升到 float32 再算，即使字段类型是 int8（`searchlib/src/vespa/searchlib/tensor/mips_distance_transform.h:90`）—— 即 query 的 int8 会被当作真正的整数而不是量化表示，避免二次量化误差。

另外 `HAMMING` / `GEODEGREES` 与量化不兼容，工厂的 `default` 分支直接 `abort()`（`searchlib/src/vespa/searchlib/tensor/distance_function_factory.cpp:113`）—— 这类配置在部署前就会被拒，不会拖到运行时。

## 二十七、参考资料

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