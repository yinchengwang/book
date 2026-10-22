# Quickwit Wiki（Rust 云原生对象存储搜索）

> 参考版本：Quickwit v0.7.x（commit `main`），**Rust + Tantivy + 分布式 Actor 框架 Actix/Actor** 实现的 **云原生对象存储搜索引擎**，主打 **"S3 / Azure Blob / GCS 上的日志与文档索引"**、**水平无限扩展（无 shard）**、**多租户 Index + Tenant**。本 wiki 覆盖倒排、Tantivy 集成、对象存储集成、Janitor、Searcher 调度。

## 一、项目定位

Quickwit 是 **Quickwit SAS（Paul Masurel + François Boutier）** 开源的 **Rust 云原生搜索与分析引擎**，**底层完全基于 Tantivy**（Quickwit 团队是 Tantivy 的核心维护者）。它把 **"分布式无 shard 集群 + 对象存储（S3 / GCS / Azure Blob / 本地 FS）+ 多租户索引（index + tenant-id）+ 索引拆分（split）+ 异步 ingest + Searcher 调度"** 整合为云原生时代搜索服务。

- **痛点**：
  - Elasticsearch / OpenSearch 需要持久化本地磁盘 + 复杂集群配置 + 集群规模有限；
  - 大规模日志 / 文档数据存储在 S3 / GCS 但搜索困难（Athena / Presto 慢 / 不适合高频搜索）；
  - 想要 **"split-by-time / 多租户 / 多热存储 + 冷对象存储"** 的搜索架构；
  - SaaS 多租户运营成本高、希望做 **"按 index-tenant 拆分 + 按 split 持久化 + Searcher 缓存"**；
- **典型用户**：日志管理（Splunk / Datadog 替代）、SaaS 多租户搜索、对象存储归档查询、Compliance Archive、Security；
- **核心口号**：**"Search your petabytes on S3"** + **"Cloud-native search engine"** + **"Built on Tantivy"** + **"AGPL or commercial"**。

## 二、发展历史与社区

- **起源**：2017-11 **Quickwit SAS** 在巴黎成立，最初由 **Paul Masurel（同时维护 Tantivy）** + **François Boutier + Massimiliano Murrazzu** 等创立；
- **里程碑**：
  - 2019-Q1：v0.1，Tantivy 单进程；
  - 2020-Q4：v0.2，引入 **Indexer + Searcher 分层**；
  - 2021-05：v0.3，**Janitor（Index cleanup）+ Metastore（PostgreSQL/MySQL/SQLite）+ S3/GCS/Azure 集成**；
  - 2021-12：v0.4，**多租户 Index + Tenant**；
  - 2022-04：v0.5，**Streaming Ingest 改进**；
  - 2022-11：v0.6，**Tracing** + **TLS + RBAC**；
  - 2023-04：v0.7，**Aggregations（bucket + metric + pipeline）**；
  - 2023-11：v0.8，**Vector Search** + **Auto-bundled Indexing**；
  - 2024-2026：v0.9+ 持续强化 **Multi-Tenancy、Object Storage、Distributed Cache、Efficiency**；
- **维护状态**：极活跃，**GitHub ~6k stars**；
- **社区**：Quickwit Forum、Discord、Quickwit 官方博客、Tantivy 论坛共用、GitHub Discussions 活跃。

## 三、许可与商业限制

- **许可证**：**AGPL-3.0（开源版）** 自托管免费；
- **商业版本**：
  - **Quickwit Cloud**：官方 SaaS（2024+ 推出预览）；
  - **Quickwit Enterprise**（之前叫 _Quickwit Enterprise_）：on-prem 商业合同，含 SAML / SSO / Cross-Tenant 治理；
- **生态关系**：
  - **Tantivy**：底座；
  - **Apache Kafka / Redpanda**：Streaming source；
  - **OpenTelemetry**（OTel）：traces + logs；
  - **AWS Kinesis / SQS**：queue 集成；
  - **Prometheus / Grafana**：metrics；

## 四、业界主要使用方与案例

- **公开**：
  - **Quickwit 官方 demo**：完整 osef 开源数据集 + GitHub 爬虫索引；
  - **多家 SaaS / 电商公司** 内部日志聚合（中小规模）；**Ledger** / **Dataiku** 等；
- **典型场景**：
  - 50TB ~ PB 级日志查询（Splunk 替代）；
  - **Indexing on S3**（Athena 替代）；
  - 多租户 SaaS search。

## 五、一句话对比定位

> Quickwit 是 **"Rust + AGPL + Tantivy 底座 + 对象存储（S3/GCS/Azure）持久化 + Indexer/Searcher 分层 + 多租户 Index + Janitor 自动清理"** 的云原生搜索；与 **Elasticsearch / OpenSearch**、**Meilisearch / Typesense**、**Tantivy（库）**、**Athena / Presto + S3** 构成搜索阵营。Quickwit 的差异化是 **"云原生对象存储 + 无 shard + 多租户 + Tantivy 同源"**。

## 六、整体架构

```
┌────────────────────────────────────────────────────────────────────────┐
│                  客户端                                                │
│  - REST API（7280，search）/ 7281（ingest）                            │
│  - 官方 SDK：quickwit-jvm / quickwit-cli / curl                       │
│  - JS / Python 包（HTTP 兼容）                                         │
└─────────────────────┬──────────────────────────────────────────────────┘
                      │ HTTP/JSON
┌─────────────────────▼──────────────────────────────────────────────────┐
│                  Node Service（单 binary：indexer / searcher /       │
│                                       metastore / janitor）           │
│  quickwit-core/                                                        │
│  ├─ HTTP Service（Httprouter + REST handlers）                       │
│  ├─ Indexing Service（pipeline + sources）                            │
│  ├─ Search Service（聚合 + 多 split 搜索）                            │
│  ├─ Janitor Service（split cleanup / retention）                      │
│  ├─ Metastore（Postgres / MySQL / SQLite）                            │
│  └─ Storage（Memory / File / S3 / GCS / Azure Blob）                  │
└─────────────┬────────────────────────────────────────────────────────┘
              │
┌─────────────▼────────────────────────────────────────────────────────┐
│                  分层服务（每个 Node 可运行）                          │
│  ├─ Indexer（写入服务）                                               │
│  │   ├─ 来源：Kafka / Pulsar / Kinesis / NATS / File / S3 source   │
│  │   ├─ Pipeline：parse → tokenizer → doc → publish               │
│  │   └─ 写在本地 staging，按 split 切分再上传对象存储               │
│  ├─ Searcher（搜索服务）                                              │
│  │   ├─ 通过 Metastore 拉取 split 列表                               │
│  │   ├─ 通过 Storage 拉取 .split                                    │
│  │   ├─ 用 Tantivy 加载 + 搜索                                       │
│  │   └─ 聚合 -> 返回结果                                            │
│  ├─ Janitor（清理服务）                                                │
│  │   ├─ 删除过期 split                                              │
│  │   ├─ 应用 retention                                              │
│  │   └─ 切分 metadata                                              │
│  └─ Metastore                                                       │
│      ├─ Postgres / MySQL / SQLite                                    │
│      ├─ 存储 index / split / source / index_config 元数据          │
│      └─ polling / watch                                            │
└────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│                  Persistent Layer                                    │
│  ├─ Object Storage（S3 / Azure Blob / GCS / File）：.split（元数据 │
│  │   + 索引文件 tar 包，hot index 仍存本地）                       │
│  ├─ Hot Cache：本地 fs / 内存 holding 池                            │
│  └─ Metastore：Postgres / MySQL / SQLite（集群元数据）             │
└─────────────────────────────────────────────────────────────────────┘
```

### 关键设计取舍

- **Tantivy（同仓）+ 扩展 = Quickwit**：核心搜索引擎全部用 tantivy，在此之上加 **indexing pipeline + split + 多租户 + storage**；
- **Split = 不可变索引文件**：每个 split 是 Tantivy 索引 + 元数据 tar 包 + 顺序递增版本号；
- **对象存储优先**：split 默认放 S3 / GCS / Azure Blob，本地 cache 是热层；
- **Indexing 与 Searching 解耦**：Indexer 节点负责 ingest；Searcher 节点负责 query；
- **多租户原生**：每个 tenant-id 隔离，独立 index 配置 + 限速；
- **Actor 模型**：使用 tokio + Actor（`quickwit-actors`）做并发调度；
- **Metastore 抽象**：Postgres/MySQL/SQLite 都支持；
- **CLI / Config（YAML）**：通过 node-config.yaml 一致化部署。

## 七、存储引擎（Split + Tantivy）

### 7.1 Split 文件结构

```
<Tenant>-<Index>/split-{UUID}-{commit}/{...}
├─ split.json          # split 元数据（time_range, doc_count, num_docs, ...）
├─ _meta.json          # tantivy index meta
├─ .fast-file          # fast fields
├─ .fieldnorm          # field norms
├─ .idx / .pos         # inverted index posting
├─ .term / .posidx     # term dictionary + position index
├─ .columnstore        # columnar storage
├─ .del                # deleted docs
└─ .store              # stored fields
```

源码：
- `quickwit-indexing/src/source/split.rs`；
- `quickwit-search/src/split.rs`。

### 7.2 索引流（从 raw 数据到 searchable）

```
[Source]
  ├─ Kafka Ingest（Kafka Source config）
  ├─ Pulsar Source
  ├─ Kinesis Source
  ├─ NATS Source
  ├─ File / S3 Source
  └─ stdin source
  ↓
[Pipeline]（quickwit-indexing/src/pipeline/）
  ├─ raw bytes input
  ├─ parser: JSON / Nginx / OTLP / proprietary → DocBatch
  ├─ schema validation / mapping
  ├─ connector: tantivy index writer
  └─ commit（写入 hot staging）
  ↓
[Splits publish]
  ├─ 拆分为 split_id(=commit_timestamp)
  ├─ 二进制文件打包
  ├─ 推送对象存储（S3 / GCS / Azure）
  └─ Metastore 写入 split metadata
```

源码：
- `quickwit-indexing/src/pipeline/ingest_pipeline.rs`；
- `quickwit-storage/src/`，`quickwit-indexing/src/source/`。

### 7.3 索引 / Search 热/冷分层

- **Hot Stage**：本地文件系统 + 缓存（`QuickwitCache`）；
- **Cold / Object Storage**：默认 S3 / Azure Blob / GCS。
- **Source Cache**：`partial_cache` 在 searcher 端，访问模式 + LRU；
- 源码：`quickwit-storage/src/cache/`、`quickwit-search/src/cache.rs`。

### 7.4 段合并 / Segment

- **每 split = 1+ tantivy segments**：split 由多个内部段组成；
- **段合并策略**：在 Indexer 中用 tantivy 的 `MergePolicy`；
- **partial merge**：可选择在 search 时 merge 多个 splits（Tantivy MultiIndex + FastFields）；
- 源码：`quickwit-search/src/search_pipeline.rs`。

## 八、索引机制（**搜索类型重点展开**）

### 8.1 倒排索引（基于 Tantivy）

Quickwit 倒排完全继承自 **Tantivy**（`quickwit-search/src/extract_split_features.rs`），按 split 列出倒排 + 评分：

```
  Quickwit Split (uploaded on S3)
   ┌───────────────────────────────────────────────────────────────┐
   │  TANTIVY-based Segment                       │
   │  ┌────────────────────────────────────────────────────────┐ │
   │  │  Term Dictionary (FST)                                 │ │
   │  │  ┌──────────────────────────────────────┐             │ │
   │  │  │  Posting List (block + skip)          │             │ │
   │  │  │  ┌──────────────────────────────────┐ │             │ │
   │  │  │  │ Positions / Payloads             │ │             │ │
   │  │  │  └──────────────────────────────────┘ │             │ │
   │  │  │                                       │             │ │
   │  │  │  DocID (split-local) + global ID     │             │ │
   │  │  │                                       │             │ │
   │  │  │  FieldNorm + FastField                │             │ │
   │  │  └──────────────────────────────────────┘             │ │
   │  └────────────────────────────────────────────────────────┘ │
   └───────────────────────────────────────────────────────────────┘
```

源码：
- `quickwit-search/src/regex_optimization.rs`、`quickwit-search/src/collector.rs`；
- 直接调用 tantivy 模块。

### 8.2 BM25 默认相关性

- 继承自 Tantivy，默认 BM25（同 Apache Lucene 公式）；
- Quickwit 提供 `bm25` 参数（query 时可覆盖）：`quickwit-search/src/scoring.rs`。

### 8.3 高级相关性 / Block-Max WAND

- 来自 Tantivy `columnar/`：在大 sort_by 场景下减少 scoring；
- **Block-Max WAND**：参考 academic 论文 *Optimized Top-K Document Retrieval Using Heaps* 与 *Block-Max WAND*；
- 源码：`quickwit-search/src/collector/wand.rs`。

### 8.4 过滤器与后置过滤

- **Pre-Filter**：fast field 列存 + tantivy docId set 过滤；
- **Post-Filter**：collector 后期收缩；

源码：
- `quickwit-search/src/filter.rs`；
- `quickwit-search/src/collector.rs`。

### 8.5 倒排索引的可视化

```json
POST /api/v1/<index>/search
{
  "query": "title:quickwit AND description:rust",
  "aggs": {
    "publish_year": {
      "terms": { "field": "year", "size": 5 }
    }
  },
  "max_hits": 10,
  "start_timestamp": "2023-01-01T00:00:00Z",
  "end_timestamp": "2023-12-31T23:59:59Z"
}
```

- `start_timestamp` / `end_timestamp` 直接面向 split pruning：直接跳过 split 元数据不命中的范围。

源码：`quickwit-search/src/search_pipeline.rs`、`quickwit-search/src/leaf.rs`。

### 8.6 Vector Search（v0.8+）

- 字段 `cardinality` + `fast vector`；
- Quickwit 通过 **opensearch-hnsw / faiss-rs** 模块集成；
- `vector_search`：在 `quickwit-search/src/collector/` 提供 ANN 类 collector；
- 源码：`quickwit-search/src/vector/`。

## 九、查询 / 计算 / 接口层（**搜索类型重点展开**）

### 9.1 DSL 设计（Quickwit-Query = Lucene-like）

Quickwit 提供 **Lucene-like 查询 DSL**（`quickwit-query/src/query_ast/`）：

- **字段 query**：`title:foo`（匹配 title 中含 foo 的 docs）；
- **短语**：`"hello world"`（`title:"hello world"`）；
- **AND/OR/NOT**：`title:foo AND body:bar OR !label:test`；
- **wildcard**：`title:fo*`（前缀）；
- **range**：`year:[2020 TO 2023]`、`age:{18 TO 30}`；
- **regex**：`name:/joh?n/`；
- **boost**：`title:hello^2.0`；
- **group**：`(title:a OR title:b) AND active:true`；
- **default field**：没有字段限定 → 在 search_time 默认字段上查询。

源码：
- `quickwit-query/src/query_ast/user_input_query.rs`；
- `quickwit-query/src/query_ast/phrase_query.rs`、`bool_query.rs`、`range_query.rs`、`term_query.rs` 等。

### 9.2 聚合（Bucket + Metric + Pipeline）

Quickwit 0.7+ 引入的 `quickwit-aggregations` 子 crate：

#### 9.2.1 Bucket Aggregation

- **terms**：按字段频率返回 top N bucket；
- **date_histogram**：按时间桶分；
- **histogram**：按数值桶分；
- **range**：按 range 分桶；
- **rare**：rare 字段分桶。

源码：`quickwit-aggregations/src/bucket/`。

#### 9.2.2 Metric Aggregation

- **count**、**min**、**max**、**sum**、**avg**、**stats**；
- **percentiles**（tdigest）、**cardinality**（HLL）；
- **top_hits**。

源码：`quickwit-aggregations/src/metric/`。

#### 9.2.3 Pipeline Aggregation

- **derivative**、**cumulative_sum**、**moving_avg**、**avg_bucket**、**sum_bucket**、**max_bucket**、**min_bucket**；
- **bucket_selector**、**bucket_sort**。

源码：`quickwit-aggregations/src/pipeline/`。

#### 9.2.4 Aggregation 执行模型

- leaf 收集 → collector → 聚合 → reduce（partial reduce）→ final reduce；
- 用 **Two-Pass** + **分片并行**；
- 用 **fast fields** 列存 + **columnar Block-WAND**；
- 源码：`quickwit-aggregations/src/collector/`、`quickwit-search/src/aggregation_reporter/`、`quickwit-aggregations/src/agg.rs`。

### 9.3 多租户搜索（Multi-Tenancy + Federated Search）

#### 9.3.1 多租户（Tenant）

- 每个请求带 `tenant_id`；
- Source / Sink 隔离：每个 tenant 有独立 metadata + storage path；
- CLI：`POST /api/v1/<tenant_id>/<index>/ingest`；
- 搜索：`POST /api/v1/<tenant_id>/<index>/search`；
- 速率限制：per-tenant 计数；
- 源码：`quickwit-metastore/src/control_plane_metastore/`。

#### 9.3.2 Federated Search（Cross-Cluster）

- Quickwit 当前阶段（0.7-）仅单机；多集群间通过 **gateway**（外部 LB / API Gateway）实现；
- 计划中的 multi-zone / cross-cluster search；
- 源码：`quickwit-search/src/search_pipeline.rs`（multi-search 抽象）。

#### 9.3.3 Distributed Search（Leaf and Root）

- **Leaf Search**：每个 Searcher 节点处理本节点负责的 splits；
- **Root Search**：聚合 leaf 响应（top-K + aggregate partial reduce）；
- 源码：`quickwit-search/src/root.rs` + `quickwit-search/src/leaf.rs`。

### 9.4 CLI / SDK 接入

- **CLI**：
  - `quickwit run`：启动节点；
  - `quickwit index create`：创建 index；
  - `quickwit ingest` + `quickwit search`；
  - `quickwit index describe` / `list`；
- **SDK**：
  - **`quickwit-jvm`**：Java SDK；
  - **`quickwit-py`**（社区）：Python 客户端；
  - **`quickwit-jslib`**：JS client（streaming, via SSE）；
  - **OTel collector**：trace 集成。

## 十、事务与一致性

- **事务模型**：
  - **Metadata（Metastore）**：Postgres / MySQL 提供 ACID；
  - **Indexes（Splits）**：对象存储 eventual consistency（S3 / Azure 上传后 visibility）；
  - **本地 indexing**：tantivy commits 是 atomic（Library）；
- **写入一致性**：
  - 阻塞默认 `wait_for=true`（block）vs `wait_for=false`（async，task ID 返回）；
  - Async 返回 task_id，可轮询 `GET /api/v1/<tenant>/tasks`；
- **读取一致性**：
  - 默认 `commit-time` reload policy（tantivy），数据写入完成后立即可见；
- **Source offset 持久化**：
  - Kafka / Pulsar / Kinesis offset 在 Metastore 内持久化，重启恢复；
- 源码：`quickwit-indexing/src/source/`、`quickwit-metastore/`。

## 十一、复制与高可用

- **Indexing 高可用**：
  - Indexer 多节点并行（基于 Kafka partition / file source hashing）；
  - 通过 Metastore 的 split 元数据去重，避免 double indexing；
- **Searcher 高可用**：
  - Searcher 多节点无状态；
  - 每个 Searcher 都能从 storage 下载任意 split；
  - 节点失败 → 无状态切换；
- **Metastore 高可用**：
  - Postgres：自带 HA；
  - MySQL：自带 HA；
  - SQLite：单进程（仅 dev）；
- **Source 重试 + DLQ**：
  - Indexer pipeline 重试 / dead-letter 处理；
- **跨区域**：
  - 由于 split 在 S3 上，多 region 通过 S3 cross-region replication 实现；
- 源码：`quickwit-indexing/src/source/`、`quickwit-search/src/service.rs`。

## 十二、运维与可观测性

- **监控**：
  - `/metrics` Prometheus 端点；
  - 每组件 metrics：`quickwit_indexing_*`、`quickwit_search_*`、`quickwit_janitor_*`、`quickwit_storage_*`；
- **Logging**：
  - Tracing (subscriber / JSON)；
  - OpenTelemetry tracer；
  - **OTel** + **EnvFilter** + **fmt** 三种 log layer；
- **API 端点**：
  - `/health` `/ready` `/live`；
  - `/api/v1/cluster` cluster state；
  - `/api/v1/indexes` 列出索引；
- **CLI**：
  - `quickwit run --config config.yaml`；
  - 配置：`metastore_uri`、`storage`、`indexer_config`、`searcher_config`、`enable_ingest_api`；
- **Web UI**：内置 quickwit UI（v0.8+）。

## 十三、性能与基准

- **官方基准**：v0.7+ 发布与 Elasticsearch / OpenSearch 对比，
  - **对象存储查询**：Quickwit 在 S3 上 query 比 ES（hot SSD）成本低 10x；
  - **冷热对比**：ES 长期存储需要热存储节点，Quickwit 直接走冷 S3；
  - **冷查询延迟**：~秒级（membership 拉 split + 列存 cache）；
- **常见基准**：
  - Stack Overflow Public Data（Public Q&A 数据）；
  - **Osef**（Quickwit 开源演示数据集，开放数据）；
  - Github Archive（GH Archive）；
- **优化建议**：
  - **partition-by-time / partition-by-tenant**：split 自然按时间 / tenant 拆分，cache 命中率高；
  - **快字段（fast_field）+ index sort**：用快速列存加速聚合；
  - **Indexing source batching**：Kafka batch + commit 快；
  - **Searcher Cache (RAM cache + partial cache)**：减少重复搜索的 split 拉取；

## 十四、生态与集成

- **底层**：Tantivy（同团队）；
- **Source 集成**：
  - Kafka / Redpanda / Pulsar / NATS / Kinesis / File / S3 / stdin；
- **Trace / Logs**：
  - **OpenTelemetry**：trace + log 集成（`OpenTelemetryLogSource`）；
  - **Vector**：`vector.dev` 输出到 Quickwit；
  - **Fluentd / Fluentbit**：output plugin；
- **可视化**：
  - Grafana：Quickwit DataSource plugin（社区）；
  - Qryn / HyperDX / OpenObserve：可观测前端集成；
- **BI / 生态**：
  - **Metabase / Superset**：通过 JDBC（社区）查询 Quickwit；
  - **Trino**（Presto 续作）：可作为 connector（社区）；
- **AI / RAG**：
  - **LangChain / LlamaIndex**：作为 retriever；
  - **OpenAI / Cohere**：向量检索整合（v0.8+）；
- **容器化**：
  - 官方 Docker：`getquickwit/quickwit:0.7.0`；
  - Helm Chart（社区）；
  - Operator for Kubernetes（社区）。

## 十五、学习路径

### 15.1 入门（1-2 周）

1. **官方教程**：`quickwit.io/docs/get-started/`；
2. **本地运行**：
   - `curl -L https://install.quickwit.io | sh && ./quickwit run`；
   - 创建 index：`quickwit index create --index-config ./config.yaml`；
   - 写入 docs：
     ```
     curl -X POST -d '{"title": "foo", "body": "bar"}' \
       -H "Content-Type: application/json" \
       http://localhost:7280/api/v1/default/index/ingest
     ```
   - 搜索：`POST /api/v1/default/index/search ...`；
3. **结合 Kafka**：在 `quickwit index create` 中配置 `KafkaSource`；
4. **S3 配置**：把 split 推到 S3 / Minio。

### 15.2 进阶（4-6 周）

1. **必读源码**：
   - `quickwit-search/src/`：搜索核心（search_pipeline.rs / leaf.rs / root.rs / collector/）；
   - `quickwit-indexing/src/`：ingestion pipeline；
   - `quickwit-aggregations/src/`：聚合实现；
   - `quickwit-storage/src/`：S3/GCS/Azure 存储抽象；
   - `quickwit-metastore/src/`：元数据；
   - `quickwit-actors/src/`：Actor 框架；
   - `quickwit-query/src/`：DSL 解析 + AST；
   - **二级**：参考 tantivy crate：FST / columnar / postings。
2. **Splits 元数据**：`quickwit-search/src/extract_split_features.rs`；
3. **Janitor**：`quickwit-janitor/src/`；
4. **聚合 Two-Pass**：`quickwit-aggregations/src/agg.rs`。

### 15.3 高级（长期）

1. **Multi-region 部署**：基于 S3 CRR 跨 region 搜索；
2. **OTel integration**：参考 `opentelemetry-*` crate；
3. **Custom Source / Sink**：自定义 Ingest plugin；
4. **Custom Aggregation**：通过 trait `Aggregator` 自定义；
5. **Federated Gateway**：自管 LB + 多集群；
6. **Vector search + Hybrid**：稀疏 + dense 混合实现。

### 15.4 推荐资源

- **官方**：quickwit.io/docs、quickwit.io/blog；
- **Tantivy 学习 + Quickwit 实战**：官方推荐组合；
- **Quickwit O'Reilly 免费课程**（v0.7+）；
- **学术论文**：
  - *Optimized Top-K Document Retrieval Using Heaps*；
  - *Block-Max WAND*；
  - *"Search Your Petabyte in 1s"*（Quickwit 论文）；
- **社区**：Discord、Forum、GitHub Discussions。

## 十六、风险与陷阱

1. **冷查询延迟**：跨 region S3 上的查询比 ES 热存储慢 1-3 秒，**实时监控必查**；
2. **Metastore 锁竞争**：大量 split 元数据更新时 PostgreSQL 可能成为瓶颈；
3. **Kafka Offset 重置风险**：source checkpoint 重置会重复 / 漏写；
4. **Janitor 误删**：Retention 配置错误可能误删重要 split；
5. **Schema 不可变**：字段类型不可改；reindex 需新 index；
6. **Aggregation 限制**：尽管聚合框架有，但 Quickwit 仍 **不如 ES 完整**，部分高级用法（runtime fields）不支持；
7. **Vector Search 性能**：ANN 部分受 Rust 限制，未与 milvus / qdrant 等级对齐；
8. **跨集群搜索 Beta**：原生 federated 仍在计划，部署多集群需自管 LB；
9. **Multi-tenant 性能**：单个 Node 跨多 tenant，某个 tenant 高峰会拖累整体；生产需为 tenant 单独部署；
10. **Trace 集成数据格式约束**：OTel trace 必须按 schema 转换，配置复杂；
11. **AGPL 协议**：修改 + 自建服务需开源；
12. **早期版本兼容**：v0.7 → v0.8 索引格式变更，升级需 reindex；
13. **Indexing 阻塞**：单 Node 的 Indexer 处理速度是瓶颈，超大规模需 Kafka partition 调优；
14. **Searcher Cache 内存**：大量 hot split 占用内存大，需查 `quickwit_searcher_cache_*` 指标。

## 十七、与本项目的关联

本项目 `engineering/` **未实现** Quickwit 兼容 API，但下列可借鉴：

- **对象存储做索引存储（S3-like）**：`engineering/src/db/storage/` 已使用本地文件系统，可参考 `quickwit-storage/src/object_storage/` + `quickwit-storage/src/split_cache/` 在 `engineering/src/db/storage/cold.c` 加入对象存储后端；
- **多租户 Index 设计**：参考 `quickwit-metastore` 在 `engineering/src/db/core/tenant.c` 实现 partition-by-tenant；
- **Partition-by-time（split 拆分）**：参考 `quickwit-indexing/src/source/` 在 `engineering/src/db/core/ingest.c` 增加按时间窗口落盘；
- **Hybrid 聚合**：参考 `quickwit-aggregations` 在 `engineering/src/algo/` 增加分桶 / 时间直方图聚合；
- **Actor 框架**：参考 `quickwit-actors` 在 `engineering/src/algo/actor.c` 简化版 Actor 实现；
- **Split 不可变 + 后台 Cleanup**：参考 `quickwit-janitor` 在 `engineering/src/db/core/janitor.c` 增加清理任务。

## 十八、Split 身份与物理布局：ULID 时间序 + 尾页 footer + hotcache（算法展开）

> **版本锚点**：本节三节均以 Quickwit **v0.7.1**（tag `v0.7.1`，HEAD = `01c2c7f5`）公开仓库 `https://github.com/quickwit-oss/quickwit` 为准。本仓库 `reference/search/quickwit/` 子模块**未 init**（`ls -A | grep -v '^\.git$' | wc -l` = 0），故源码路径一律**只给公开 canonical 路径、零精确行号**；行号需自行校对。

### 18.1 Split ID 为什么是 ULID 而不是随机 UUID

Quickwit 的 split 身份由一个函数生成，全仓库只此一处：

```
quickwit-indexing/src/lib.rs
    pub fn new_split_id() -> String { ulid::Ulid::new().to_string() }
```

工作区依赖（`quickwit/Cargo.toml`）写得很明确：

```toml
ulid  = "1.1"
uuid  = { version = "1.7", features = ["v4", "serde"] }   # 注意：只开了 v4
```

> ⚠️ **常见误解纠正**：坊间资料（包括部分二手博客）会说 Quickwit 用 **UUIDv7** 做 split / doc id。在 v0.7.1 这不成立——`uuid` crate 的 feature 列表里**没有 `v7`**，split id 走的是 `ulid` crate 的 `Ulid::new()`。ULID 与 UUIDv7 布局相近（都是「时间前缀 + 随机」，都按字典序 = 时间序），但它们在 Quickwit 里是两回事。

ULID 是 128 bit、26 字符的 Crockford Base32 串：**48 bit 毫秒时间戳（高位）+ 80 bit 随机（低位）**。因此有恒等式

$$\text{ULID}_1 <_{\text{lex}} \text{ULID}_2 \iff t_1 < t_2 \ \vee\ (t_1 = t_2 \wedge r_1 < r_2)$$

也就是**字节序即创建序**。这条性质在 v0.7.1 里有三处硬依赖：

| # | 依赖点 | 代码位置 | 不这么做会怎样 |
|---|---|---|---|
| 1 | 本地 split store 的驱逐堆 | `quickwit-indexing/src/split_store/local_split_store.rs` | 源码注释直说：「Splits ids are generated using ULID, so that they are sorted according to their creation date. We evict the oldest split first. (Note this is not an LRU strategy … we only consider the creation time.)」——若换成随机 UUID，驱逐就得额外维护一个 `creation_time` 旁表 |
| 2 | 搜索侧缓存的排序键 | `quickwit-storage/src/split_cache/split_table.rs` | `SplitKey { last_accessed, split_ulid }` 构成 `BTreeSet<SplitKey>` 的全序；`last_accessed` 是微秒粒度会撞车，ULID 提供零成本的确定性 tie-break |
| 3 | 合并排序的确定性 | `quickwit-indexing/src/merge_policy/stable_log_merge_policy.rs` | `cmp_splits_by_reverse_time_end` 的最后一级 tie-break 就是 `split_id()`，源码注释标注 `//< for determinism.`——保证同一批 split 无论以什么顺序进入策略，产出的合并操作完全一致 |

**和自增主键 / 随机 UUID 的对比**（按「分布式可用性」而非「性能」排序）：

| 维度 | 随机 UUIDv4 | 自增主键 | ULID（Quickwit 选型） |
|---|---|---|---|
| 生成端协调 | 无需协调 | 需要中心发号器（Quickwit 无此组件） | 无需协调（本地时钟 + 本地 RNG） |
| 128 bit 熵 | 122 bit 随机 | ~48 bit，**可枚举** | 80 bit 随机 |
| 字典序 = 时间序 | ✗ | ✓（但跨发号器不成立） | ✓（跨节点成立，仅受时钟漂移影响） |
| 对象存储 key 局部性 | 差（同一 split 内的文件 key 全散） | 好 | 好（同批 split 的 ULID 前缀相邻） |
| 时钟回拨 | 不受影响 | 不受影响 | ⚠️ 48 bit ms，回拨会产生"未来"ULID；`Ulid::new()` 不做单调化保护 |

**复杂度**：`Ulid::new()` = $O(1)$（读 48 bit 毫秒时钟 + 80 bit RNG）；比较 = 16 字节定长数组比较，$O(1)$；字符串形式 26 字符，$O(26) = O(1)$。这些都落在热路径上（每个 split 一次生成、每次缓存排序一次）。

> **边界声明**：split id **不是文档 id**。Quickwit 的 tantivy 索引内部文档 id 仍是 split-local 的 `DocId`，由 tantivy 自己管理。所以「时间有序 id 把随机写变成顺序写、降低 LSM 写放大」这套论证**不适用于 Quickwit**——Quickwit 的写放大发生在 split 合并层（见 §十九），不在 segment 内。ULID 的收益是**对象存储 key 局部性 + 缓存驱逐顺序 + 策略确定性**三件事。

### 18.2 split bundle 的物理布局：尾页 footer 与一次 range GET

一个 split 上传到对象存储后是**单个文件** `{split_id}.split`，内部是「所有 tantivy 文件首尾相接 + 尾部一个 footer」。布局由 `quickwit-storage/src/split.rs` 的 `SplitPayloadBuilder::finalize` 写出，由 `quickwit-storage/src/bundle_storage.rs` 反向解析：

```
┌──────────────────────────────────────────────────────────────────────┐
│ tantivy 文件 0 │ tantivy 文件 1 │ … │ split_fields │ metadata_json   │  ← body
├──────────────────────────────────────────────────────────────────────┤
│ metadata_len : u32 LE  │ hotcache │ hotcache_len : u32 LE            │  ← footer
└──────────────────────────────────────────────────────────────────────┘
     footer_offsets.start ──────────────────────────► footer_offsets.end
```

关键常量（全部在 `quickwit-storage/src/bundle_storage.rs`）：

```rust
const SPLIT_HOTBYTES_FOOTER_LENGTH_NUM_BYTES: usize = size_of::<u32>();  // 4
const BUNDLE_METADATA_LENGTH_NUM_BYTES:       usize = size_of::<u32>();  // 4
impl VersionedComponent for BundleStorageFileOffsetsVersions {
    const MAGIC_NUMBER: u32 = 403_881_646u32;
}
enum BundleStorageFileOffsetsVersions { #[default] V1 = 1 }
```

`split_fields` 是 Quickwit 自己加的一段字段名清单（常量 `SPLIT_FIELDS_FILE_NAME = "split_fields"`，定义在 `quickwit-common/src/shared_consts.rs`），被 list-fields 端点单独读取。

**最关键的设计**：footer 的**字节偏移量存在 metastore 里，不在对象存储里**。`quickwit-metastore/src/split_metadata.rs`：

```rust
/// Contains the range of bytes of the footer that needs to be downloaded
/// The footer offsets
/// make it possible to download the footer in a single call to `.get_slice(...)`.
pub footer_offsets: Range<u64>,
```

解析伪代码（忠实对应 `open_from_split_data` + `open`）：

```text
parse_footer(file_slice):
    # 第一步：从最末尾读 4 字节 = hotcache 长度
    (rest, hotcache_len_bytes) = file_slice.split_from_end(4)
    hotcache_len = u32::from_le(hotcache_len_bytes)
    # 第二步：切出 hotcache
    (bundle_and_hotcache, hotcache) = rest.split_from_end(hotcache_len)
    # 第三步：从 bundle 末尾读 4 字节 = metadata_json 长度
    (tantivy_files, meta_len_bytes) = bundle_and_hotcache.split_from_end(4)
    meta_len = u32::from_le(meta_len_bytes)
    # 第四步：切出 JSON 并做版本校验
    meta_bytes = tantivy_files.slice_from_end(meta_len).read_bytes()
    return (hotcache, BundleStorageFileOffsetsVersions::try_read_component(meta_bytes))
```

于是 leaf 侧拿到的只是一次精确到字节的 HTTP Range 请求（`quickwit-search/src/leaf.rs::get_split_footer_from_cache_or_fetch`）：

```text
footer_data = index_storage.get_slice("{split_id}.split", split_footer_start .. split_footer_end)
```

**不需要**「先 HEAD 拿长度 → 再 Range 读尾部 → 再解析 → 再 Range 读各文件」，这在按请求计费的 S3 上是 3 次往返 vs 1 次往返的差别。

拿到 `BundleStorageFileOffsets { files: HashMap<PathBuf, Range<u64>> }` 之后，`BundleStorage` 把自己伪装成 tantivy 的 `Directory`：任何 `open_read(".term")` 都被翻译成对应的 `get_slice(range)`。**Tantivy 完全不知道自己踩在对象存储上**——这是复用边界的关键（见 §21）。

复杂度：解析 $O(\text{metadata\_json 长度})$；每次逻辑文件读 = 1 次 Range GET。footer 本身被 `split_footer_cache`（`MemorySizedCache`，默认 **500 MB**）按 split_id 缓存。

### 18.3 hotcache 怎么算出来的：FST 触发 + 10 MB 阈值

hotcache 是「打包时顺手把搜索器一定会读的那几个字节区间塞进 bundle 尾部」的做法。核心在 `quickwit-directories/src/hot_directory.rs::write_hotcache`：

```text
write_hotcache(directory, output):
    # ① 用无界缓存目录包一层：防御性保证 hotcache 里的内容
    #    与第一次读取时读到的内容逐字节一致
    caching_dir = CachingDirectory::new_unbounded(directory)
    debug_dir   = DebugProxyDirectory::wrap(caching_dir)
    index       = Index::open(debug_dir)      # 真正的 tantivy Index
    searcher    = index.reader_builder().reload_policy(Manual).try_into()?.searcher()

    # ② 强制把每个 indexed field 的 FST（term dictionary）整棵加载
    #    ——这是读操作记录的全部来源
    for (field, field_entry) in index.schema().fields():
        if !field_entry.is_indexed(): continue
        for reader in searcher.segment_readers():
            _inv_idx = reader.inverted_index(field)?

    # ③ 回放所有真实发生的读操作，按文件聚合出「读过的字节区间集合」
    per_file_slices: HashMap<PathBuf, HashSet<Range<usize>>> = {}
    for op in debug_dir.drain_read_operations():
        per_file_slices[op.path].insert(op.offset .. op.offset + op.num_bytes)

    # ④ 逐文件逐区间决定是否进 hotcache
    for file_path in list_index_files(&index):
        file_slice = debug_dir.open_read(&file_path)?
        cache_builder.add_file(&file_path, file_slice.len())
        for byte_range in per_file_slices.get(&file_path):
            keep = file_path.ends_with("store")      // doc store 索引：无条件保留
                || file_path.ends_with("term")       // term dictionary：无条件保留
                || byte_range.len() < 10_000_000    // 其他：小于 10 MB 才保留
            if keep: cache_builder.cache_slice(byte_range)
```

源码里那段 `keep` 判据的注释很直白：

> We do not want to store slices that are too large in the hotcache, but on the other hand, the term dictionary index and the doc store index are required for quickwit to work.

**这是一个「实测驱动的白名单 + 体积闸门」策略**，不是拍脑袋的配置：

- **白名单**：`.term`（FST 前缀索引）和 `.store`（doc store 的块索引）无论多大都进 hotcache——没有它们就无法在不知道 offset 的情况下定位词项 / 文档；反正 bundle 尾部已被单独 range 读取，多带一点是零边际成本。
- **体积闸门**：posting list / fast field 的其余读区间超过 **10 MB** 就不进——单个 posting list 动辄几十上百 MB，塞进 hotcache 会让每次 range GET 的字节数失控。
- **探测方式**：`reader.inverted_index(field)` 是 tantivy 里唯一会强制 FST 全量驻留的调用。这一步把「哪些字节被读过」变成可观测事实，而不是靠人工列举文件清单。

**复杂度**：设 $S$ = split 总字节，$F$ = 全部 FST 字节，$R$ = 读操作条数，$H$ = hotcache 字节。

| 阶段 | 时间 | 峰值内存 |
|---|---|---|
| ② 强制加载 FST | $O(F)$ | $O(F)$ |
| ③ 回放读区间 | $O(R)$ | $O(R)$ |
| ④ 拷进 hotcache | $O(H)$ | $O(H)$ |
| 合计 | $O(F + H)$ | $O(F)$ |

**收益**：搜索器打开一个 split 时，term dictionary 与 doc store 索引**零次对象存储往返**即可用；只有超出 10 MB 闸门的区间才走 Range GET。对比 ES 那种「整个 segment 必须在本地盘上」的模型，这是 Quickwit 冷启动延迟可控的根本原因。

### 18.4 两种 split 本地缓存：索引侧 FIFO-by-ULID vs 搜索侧 LRU

Quickwit 有**两套完全不同策略**的 split 本地缓存，这是理解「写入路径与读取路径优化目标不同」的关键对照。

| 维度 | 索引侧 `LocalSplitStore` | 搜索侧 `SplitCache` |
|---|---|---|
| 代码位置 | `quickwit-indexing/src/split_store/local_split_store.rs` | `quickwit-storage/src/split_cache/{mod,split_table,download_task}.rs` |
| 形态 | tantivy **目录**（多个小文件） | 单个 `.split` bundle 文件 |
| 数据结构 | `HashMap<Ulid, SplitFolder>` + `BinaryHeap<Reverse<Ulid>>` | `SplitTable`：3 个 `BTreeSet<SplitKey>` + `HashMap<Ulid, SplitInfo>` |
| 淘汰策略 | **FIFO by 创建时间**（显式声明「不是 LRU」） | **LRU by last_accessed** |
| 容量维度 | split 个数 + 总字节双限 | split 个数 + 总字节双限 |
| 默认配额 | `max_num_splits = 3`，`max_num_bytes = 1 MB` | `max_num_splits = 100`，`max_num_bytes = 1 GB`，并发下载 = 1 |
| 生命周期 | split 上传成功后 `move_out` 移走 | 常驻，被查询不断 touch |
| 职责 | 合并 / 上传前的**暂存**，丢了要重算 | 查询侧**热层**，丢了只是变慢 |

索引侧的准入与驱逐（`make_room_and_record_split`）：

```text
make_room_and_record_split(split_folder):
    # ① 单个 split 比总配额还大 → 直接拒收（不进缓存）
    if split_folder.num_bytes > quota.max_num_bytes: return False
    # ② 腾位置：只要放不下就逐个驱逐（evict_one_split → BinaryHeap.pop 取最旧 ULID）
    while not quota.can_fit_split(split_folder.num_bytes):
        evict_one_split()
    # ③ 顺带做一次年龄清理：清掉创建时间早于 (本 split 创建时间 - SPLIT_MAX_AGE) 的
    remove_splits_older_than_limit(split_folder.creation_time() - SPLIT_MAX_AGE)
    # ④ 记账并入堆
    quota.add_split(size); split_folders.insert(id, folder); split_ids.push(Reverse(id))
    return True
```

搜索侧的准入（`find_download_opportunity`）多一层「**先确认腾得出来，再开始下载**」：

```text
find_download_opportunity():
    best = candidate_splits.last()              # BTreeSet 的最大元 = LRU 最久未访问的候选
    if best is None: return None
    splits_to_delete = make_room_for_split_if_necessary(best.last_accessed)?
        # ↑ 关键：返回 None 表示「要驱逐的 split 比 best 还新」，宁可不下载也不驱逐
    #   若驱逐后仍超限 → 把已驱逐的 split 原样塞回去，返回 None
    return DownloadOpportunity { splits_to_delete, split_to_download: start_download(best) }
```

`make_room_for_split_if_necessary` 的两级保护值得单列：

```text
while is_out_of_limits():
    first = on_disk_splits.first()             # LRU 最旧的
    if first.last_accessed > last_access_date: break     # 保护①：比候选还新 → 停手
    remove(first)
if is_out_of_limits():                          # 保护②：仍超限 → 回滚
    reinsert_all(); return None                 # （宁可缓存小一点也不做破坏性驱逐）
```

**复杂度**：索引侧 `insert` / `evict` 均摊 $O(\log N)$（堆）；搜索侧 `touch` / `start_download` 均摊 $O(\log N)$（`BTreeSet`），哈希查表 $O(1)$。`is_out_of_limits` 是 $O(1)$（只有两个计数器和两个 `len()`）。

### 18.5 与 brief 的偏差（已剔除的项）

| brief 候选 | 处理 | 依据 |
|---|---|---|
| 「UUIDv7 作为 doc id」 | ❌ **剔除并纠正** | v0.7.1 无 `uuid` v7 feature；split id 是 ULID；文档 id 由 tantivy 管，无外部 doc id 概念 |
| 「Split store vs **Delta** split store」 | ⚠️ **改写** | v0.7.1 源码树无 `DeltaSplitStore`。最接近的真实对照是 `LocalSplitStore`（索引侧 FIFO）vs `SplitCache`（搜索侧 LRU），已改写为 §18.4 |
| 「sstable / fastfield / termdict / doc store 分块布局」 | ✅ **保留并升级** | §18.2 给出精确 bundle 布局 + 偏移表；§18.3 给出 hotcache 的白名单 + 10 MB 闸门 |

## 十九、合并策略算法：StableLog 对数分层与 ConstWriteAmplification（算法展开）

### 19.1 `MergePolicy` 契约与不变量

合并策略是 Quickwit **完全自研**的部分（tantivy 只管单个 segment 内的 LSM 合并，不管 split 之间的合并）。契约在 `quickwit-indexing/src/merge_policy/mod.rs`：

```rust
pub trait MergePolicy: Send + Sync + fmt::Debug {
    /// 返回应当执行的合并操作列表。
    /// 注意：splits 是 &mut，被合并掉的 split 会被从里面 drain 掉，
    ///       未参与合并的 split 留在原地。调用方（MergePlanner）按这个语义复用。
    fn operations(&self, splits: &mut Vec<SplitMetadata>) -> Vec<MergeOperation>;

    /// Mature 的 split 不再参与任何合并；Immature 带一个 maturation_period。
    fn split_maturity(&self, split_num_docs: usize, split_num_merge_ops: usize) -> SplitMaturity;
}
```

**核心不变量**（`StableLogMergePolicy::operations` 里有 `debug_assert_eq!` 显式守着）：

$$\sum_{op \in \text{operations}} |op.\text{splits}| + |\text{splits}_{\text{剩余}}| = |\text{splits}_{\text{输入}}|$$

策略既产出了合并计划，又把没被合并的 split 原样退回——它是一台「**边规划边消耗**」的机器，不是纯函数。这决定了调用方必须把 `splits` 当作可被清空的可变缓冲区。

产出物有两个变体（`quickwit-indexing/src/merge_policy/mod.rs`）：

```rust
pub enum MergeOperationType { Merge, DeleteAndMerge }
```

`merge_split_id` 由 `new_split_id()` 生成，即合并产物本身也是一个**新 ULID**。

三种策略（`quickwit-config/src/merge_policy_config.rs`，serde tag）：

| 配置 tag | 类型 | 默认参数 |
|---|---|---|
| `no_merge` | `NopMergePolicy` | — |
| `stable_log`（别名 `default`） | `StableLogMergePolicy` | `min_level_num_docs = 100_000`、`merge_factor = 10`、`max_merge_factor = 12`、`maturation_period = 48h` |
| `limit_merge` | `ConstWriteAmplificationMergePolicy` | 上面三个共有值 + `max_merge_ops = 4` |

全局目标（`quickwit-config/src/index_config/mod.rs`）：

```rust
fn default_split_num_docs_target() -> usize { 10_000_000 }   // 一个 split 成熟时的 doc 数
```

**`split_num_docs_target` 就是成熟判据**：达到它就 `Mature`，永不参与合并。这保证了 split 数量与总大小是 **O(数据量 / 10^7)**，而不是无界增长。

### 19.2 StableLogMergePolicy：3 倍增长的对数分层

代码在 `quickwit-indexing/src/merge_policy/stable_log_merge_policy.rs`。它的设计前提写在源码注释里：**输入 split 的时间顺序大致有序**（日志流天然如此），这样时间剪枝才能白拿。

**第一步：全序排序**

```text
cmp_splits_by_reverse_time_end(left, right):
    return extract_time_end(left).cmp(extract_time_end(right)).reverse()   # 时间末端降序
        .then(left.num_docs.cmp(right.num_docs))                          # doc 数升序
        .then(left.split_id().cmp(right.split_id()))                      # ULID 升序（保确定性）
```

排完是「最近的 → 最老的」。因为分层是**从队首开始**切的，所以切出来的每层都是一段**时间连续的** split——合并产物 `time_range` 连续，时间剪枝因此不损失。

**第二步：分层**（`build_split_levels`）

```text
build_split_levels(splits):
    assert(all split.num_docs < split_num_docs_target)   // 成熟的已经被剥掉了
    levels = []
    current_level_start = 0
    current_level_max_docs = max(splits[0].num_docs * 3, min_level_num_docs)
    for split_ord, split in enumerate(splits):
        if split.num_docs >= current_level_max_docs:
            levels.push(current_level_start .. split_ord)     # 收口上一层
            current_level_start = split_ord
            current_level_max_docs = 3 * split.num_docs       # 下界抬高 3 倍
    levels.push(current_level_start .. splits.len())
    return levels
```

> ⚠️ **实现细节提醒**：源码顶部的文档注释写的是 $l_N = \min(\text{num\_docs}(\text{split}_{l_{N-1}}) \times 3,\ \text{max\_merge\_docs})$，但 (a) `max_merge_docs` 这个字段在 v0.7.1 的 config 里**已不存在**（只有 `split_num_docs_target`）；(b) 代码里首层下界是 `max(splits[0].num_docs * 3, min_level_num_docs)` 而非文档说的 `3 × min_level_num_docs`；(c) 后续层用的是 `3 × 触发切分的那个 split 的 num_docs`。落笔以**代码**为准，注释属历史残留。

**第三步：层内选合并窗口**（`merge_candidate_from_level` + `merge_candidate_size`）

`merge_candidate_size` 是一个三态判定：

```text
merge_candidate_size(splits):
    if len(splits) <= 1:                          return TooSmall   # 单 split 不合并
    if len(splits) >= max_merge_factor:           return OneMoreSplitWouldBeTooBig
    if Σ num_docs >= split_num_docs_target:       return OneMoreSplitWouldBeTooBig
    if len(splits) < merge_factor:                return TooSmall   # 还不够 merge_factor
    return ValidSplit
```

窗口从层尾往层首**贪心回扩**，一旦撞到 `OneMoreSplitWouldBeTooBig` 就停；收尾再判一次，`TooSmall` 则整个层放弃：

```text
merge_candidate_from_level(splits, level_range):
    end   = level_range.end
    start = end
    for split_ord in level_range.rev():
        if merge_candidate_size(splits[start..end]) == OneMoreSplitWouldBeTooBig: break
        start = split_ord
    if merge_candidate_size(splits[start..end]) == TooSmall: return None
    return Some(start..end)
```

**第四步：从高层往低层扫**（这是关键一行）

```text
for split_range in split_levels.into_iter().rev():      # ← .rev()，高层优先
    if let Some(merge_range) = merge_candidate_from_level(splits, split_range):
        drain(splits[merge_range]) → MergeOperation
```

`.rev()` 的效果：先消化已经很大的层，把它们「封顶」，剩下的空间留给小层。源码注释说这套分层「每个层区间至少比上一层大 3 倍，形成文档数的对数尺度」。

**复杂度与写放大**

设 $S$ = 输入 split 数，$T$ = `split_num_docs_target`，$m$ = `min_level_num_docs`：

| 步骤 | 时间 | 空间 |
|---|---|---|
| 排序 | $O(S \log S)$ | $O(S)$（原地 `sort_unstable_by`） |
| 分层 | $O(S)$ | $O(\log_f(T/m))$ |
| 逐层窗口扫描 | $O(S)$（每个 split 只被扫一次） | $O(1)$ |

层数 $L = \lceil \log_{f}(T/m) \rceil$，默认参数下 $L = \lceil \log_{10}(10^7/10^5) \rceil = 2$（即 $10^5 \to 10^6 \to 10^7$ 两跳）。

**写放大上界 = $L$**：每个 doc 至多被重写 $L$ 次，与总数据量无关——这是分层合并族的共性保证。源码里 `max_num_splits_ideal_case` / `max_num_splits_worst_case`（`#[cfg(test)]`）就是用递推 $f(k, \text{levels}) = (f-1) + \text{sat}(k,\text{head})$ 去算「$N$ 个 doc 最多会摊出多少个 split」的理论上界。

**成熟判据**

```text
split_maturity(split_num_docs, _):
    if split_num_docs >= split_num_docs_target: return Mature
    return Immature { maturation_period: 48h }
```

注意 StableLog **不看** `num_merge_ops`（形参名带下划线前缀）——它只靠体积收口。

### 19.3 ConstWriteAmplificationMergePolicy：把写放大压成常数

代码在 `quickwit-indexing/src/merge_policy/const_write_amplification.rs`。源码注释把动机讲得比任何文档都清楚：

> The `ConstWriteAmplificationMergePolicy` has been designed for a use case where there are a several index partitions with different sizes, and partitions tend to be searched separately. (e.g. partitioning by tenant.)
>
> In that case, the StableLogMergePolicy would tend to target the same number of docs for all tenants. Assuming a merge factor of 10 and a target num docs of 10 millions, **The write amplification observed for a small tenant, emitting splits of 1 document would be 7.**
>
> These extra merges have the benefit of making less splits, but really we are over-trading write amplification for read amplification here.

翻译成算法语言：**StableLog 的「对数分层」隐含假设了「各分区的写入速率相近」**。一旦出现一个每秒只写 1 个 doc 的小租户，它的 split 永远长不到 `min_level_num_docs = 10^5`，却仍被 StableLog 一层层往上抬下界去凑——于是一个 doc 被反复重写 7 次。

CWA 的做法：**只合并「合并深度相同」的 split**，这样每个 split 的重写次数天然受 `max_merge_ops` 封顶。

```text
operations(splits):
    # ① 成熟的直接放行，不进合并池
    for split in splits.drain(..):
        if split.is_mature(now): mature_splits.push(split)
        else: group_by_num_merge_ops[split.num_merge_ops].push(split)
    splits.extend(mature_splits)

    # ② 每个「合并深度桶」独立处理
    for group in group_by_num_merge_ops.values_mut():
        merge_ops = merge_operations_within_num_merge_op_level(group)
        merge_operations.extend(merge_ops)
        splits.append(group)          # 该桶剩下的 split 原样退回

# 桶内：按创建时间升序（tie-break 用 ULID）
sort by (create_timestamp ASC, split_id ASC)

single_merge_operation_within_num_merge_op_level(splits):
    num_splits = 0; num_docs = 0
    for split in splits.take(max_merge_factor):        # 最多 merge_factor 个
        num_docs += split.num_docs; num_splits += 1
        if num_docs >= split_num_docs_target: break     # 达标就立刻停，不贪
    if num_docs < split_num_docs_target AND num_splits < merge_factor:
        return None                                    # 既不够量也不够数 → 本桶放弃
    return Some(MergeOperation(splits.drain(0..num_splits)))
```

**复杂度**：桶内排序 $O(S_b \log S_b)$，贪心 $O(S_b)$，合计 $O(S \log S)$。

**写放大上界 = `max_merge_ops` = 4**，硬常量：

```text
split_maturity(split_num_docs, split_num_merge_ops):
    if split_num_merge_ops >= max_merge_ops (4):  return Mature
    if split_num_docs >= split_num_docs_target:    return Mature
    return Immature { maturation_period: 48h }
```

源码里还有一段 `check_is_valid`（proptest 用的）把这三条不变式写成了断言：① 窗口 ≤ `max_merge_factor`；② 若窗口 < `merge_factor`，则「去掉最后一个 split 后总量就跌破 target」（保证贪心没多拿）；③ 窗口内所有 split 的 `num_merge_ops` 相同。

### 19.4 `num_merge_ops` 的递推：上界为什么成立

`quickwit-indexing/src/actors/merge_executor.rs::merge_split_attrs`：

```rust
let delete_opstamp = splits.iter().map(|s| s.delete_opstamp).max();     // 取最大
let num_merge_ops = max(splits.iter().map(|s| s.num_merge_ops)).unwrap() + 1;   // 取最大 + 1
```

`num_merge_ops` 是**单调 +1、永不回退**的合并深度计数器。这是 §19.3 那个「写放大 ≤ 4」硬上界成立的数学基础：新 split 从 0 起，每被合并一次 +1，到 4 就 `Mature` 再也不动。一个 doc 如果从 depth 0 一路活到 depth 4，重写次数就是 4（且中途每一步的合并都是同深度的，符合 CWA 的分桶约束）。

`delete_opstamp` 同样取 max——意味着**合并不会"复活"已被某次删除覆盖过的文档**：合并产物带着所有输入 split 里最新的删除水位，Janitor 之后只需再看增量删除任务。

### 19.5 三种策略对照与选型

| | `NopMergePolicy` | `StableLogMergePolicy`（默认） | `ConstWriteAmplificationMergePolicy` |
|---|---|---|---|
| 分组键 | 无 | 时间端降序后分层 | `num_merge_ops` 等值分桶 |
| 写放大上界 | ∞（不合并） | $O(\log_f(T/m))$，默认 ≈ 2 | **常数 4** |
| split 数 | 每 source batch 一个，线性增长 | $O(\text{数据量}/T)$ | 同左，但小租户不会被过度合并 |
| 依赖的时间有序性 | 无 | **强**（乱序流会造出层数爆炸） | 无 |
| 合并跨分区 | 不适用 | 不适用（由 MergePlanner 按 partition 分组） | 设计目标就是多租户分区 |
| 合并产物时间连续性 | 无保证 | **有**（时间剪枝白拿） | 弱（桶内按创建序，但桶本身可能跨越多个时间桶） |
| 适用 | 离线重建 / 测试 | 单租户、大索引、时间有序日志流 | 多租户 `partition_key` 隔离、租户规模差异悬殊 |

## 二十、调度与放置算法：索引流水线、MergePlanner 与 Rendezvous 作业分配（算法展开）

### 20.1 索引侧 actor 流水线（v0.7.1 真实拓扑）

> ⚠️ **先纠正 brief 与旧节**：v0.7.1 的 `quickwit-indexing/src/` 下**没有** `scheduler.rs`、**没有** `ingester.rs`、**没有** `pipeline/` 目录，也没有 `src/source/split.rs`。旧节 §7.2 / §15.2 引用的 `quickwit-indexing/src/pipeline/ingest_pipeline.rs` 是 0.6 时代的文件布局，0.7 已重构。真实拓扑全部在 `quickwit-indexing/src/actors/` 下：

```
[Source]                     quickwit-indexing/src/source/
  kafka / pulsar / kinesis / pubsub / file / s3 / vec_source / ingest_api
      │ DocBatch（原始 JSON 或 OTLP 字节）
      ▼
[DocProcessor]               quickwit-indexing/src/actors/doc_processor.rs
      解析 → NamedFieldDocument + (可选) timestamp + (可选) partition_id
      ▼
[Indexer]                    quickwit-indexing/src/actors/indexer.rs
      内存中写 tantivy IndexWriter；按 partition_id 分桶持有 IndexedSplitBuilder
      每处理完一个 batch 检查 3 个提交条件
      │ CommitTrigger
      ▼
[IndexSerializer]            quickwit-indexing/src/actors/index_serializer.rs
      提交 IndexWriter → IndexedSplitBuilder 落盘到 split scratch 目录
      ▼
[Packager]                   quickwit-indexing/src/actors/packager.rs
      build_hotcache() → SplitPayloadBuilder::get_split_payload() → .split 单文件
      ▼
[Uploader]                   quickwit-indexing/src/actors/uploader.rs
      写入 index storage（RAM / 本地 FS / S3 / GCS / Azure，走 opendal）
      ▼
[Sequencer + Publisher]      actors/sequencer.rs + actors/publisher.rs
      生成 SplitMetadata（footer_offsets 在这里定下来）→ metastore，状态置 Published
```

**六种提交触发器**（`quickwit-indexing/src/models/indexed_split.rs`）：

```rust
pub enum CommitTrigger { Drained, ForceCommit, MemoryLimit, NoMoreDocs, NumDocsLimit, Timeout }
```

触发判定伪代码（合并 `Indexer::index_batch` / `on_drained_messages` / `finalize` 三处）：

```text
# ① 每个 doc batch 结束后，三个 if 是并列的（不是 else-if），可以叠加触发
if workbench.memory_usage() >= indexing_settings.resources.heap_size:
    send_to_serializer(MemoryLimit)                  # 默认 heap_size = 2 GB
if workbench.num_docs_in_workbench >= indexing_settings.split_num_docs_target:
    send_to_serializer(NumDocsLimit)                 # 默认 10_000_000
if batch.force_commit:
    send_to_serializer(ForceCommit)

# ② mailbox 排空时（仅当启用了 cooperative_indexing_permits）
on_drained_messages():
    send_to_serializer(Drained)
    if elapsed < commit_timeout:                     # 默认 60s
        schedule_self_msg(commit_timeout - elapsed, Command::Resume)   # 睡到超时点

# ③ 定时消息
CommitTimeout 消息到期 → send_to_serializer(Timeout)

# ④ actor 退出
finalize(Quit | Success) → send_to_serializer(NoMoreDocs)
```

`send_to_serializer` 会 `take()` 掉整个 `IndexingWorkbench`，把 `indexed_splits.into_values()` 一次性交给下游，并显式 `drop(indexing_permit)` 归还并发许可（内存计量因此是「一个 workbench 生命周期」为单位）。

**分区溢出处理**（`IndexerState::get_or_create_indexed_split`）：新 partition 到来时若 `splits.len() >= max_num_partitions`，文档被路由到 `OTHER_PARTITION_ID` 这个特殊分区，避免 partition 基数爆炸——对应配置 `DocMapper::partition_key` + `max_num_partitions`（`quickwit-config/src/index_config/mod.rs`）。

**复杂度**：每个 doc 摊 $O(1)$ 次三次常数判定；`memory_usage_delta` 用 `index_writer.mem_usage()` 前后差值累加（tantivy 的 `mem_usage()` 是 $O(1)$ 计数器读取，不是遍历）。

### 20.2 MergePlanner：按分区隔离 + 小合并优先 + 背压回收

代码在 `quickwit-indexing/src/actors/merge_planner.rs`。三个状态字段各自解决一个问题：

| 字段 | 类型 | 解决的问题 |
|---|---|---|
| `partitioned_young_splits` | `HashMap<u64, Vec<SplitMetadata>>` | key = `partition_id`；**同一 partition 的 split 才可能合并**，从源头杜绝跨租户合并 |
| `known_split_ids` | `HashSet<String>` | actor mailbox 被复用（recycled），会重收到已处理过的 `NewSplits`，靠这个集合幂等去重（详见源码注释，关联 issue #3627） |
| `ongoing_merge_operations_inventory` | `Inventory<MergeOperation>` | 追踪在途合并，防重复下发，同时给 `rebuild_known_split_ids` 提供数据源 |

**准入**（`record_splits_if_necessary`）：mature 的 skip、已知的 skip，其余进对应 partition 桶。

**规划**（`compute_merge_ops`）：逐 partition 调策略，partition 之间 `ctx.yield_now().await` 让出调度。

**下发顺序**（`send_merge_ops`）是这一节的核心：

```text
merge_ops = compute_merge_ops()
merge_ops.sort_by_cached_key(|op| Reverse(max_merge_ops(op)))    # 小合并排前面
while let Some(op) = merge_ops.pop():        # pop() 取末尾 = 当前最小
    tracked = inventory.track(op)
    match merge_split_downloader_mailbox.try_send_message(tracked):
        Disconnected → actor 退出
        Full(op)     → ctx.send_message(op).await; break          # 遇到背压，阻塞发完就停
# 循环结束后剩下的 op（没发出去的）：
for op in merge_ops:
    for split in op.splits: record_split(split)                   # 逐个放回 young 池
try_send_self_message(PlanMerge)                                  # 安排下一轮
```

源码注释解释了为什么必须这样：

> We do not want to simply schedule all available merge operations here. The reason is that in presence of partitioning, it is very possible to receive a set of splits opening the opportunity to run a lot "large" merge operations at the same time. These large merge operation will in turn prevent small merge operations from being executed, when in fact small merge operations should be executed in priority.

排序键 `max_merge_ops(merge_op) = splits.map(num_merge_ops).max()` —— **合并深度浅的优先**。深度浅意味着涉及 split 年轻、体积小，能更快把「未成熟 split」清空，减少搜索时需要 open 的 split 数。

**`known_split_ids` 的摊还重建**：

```text
num_known_splits_rebuild_threshold() =
    1 + num_young_splits() + (1 + partitioned_young_splits.capacity()) * 20
```

`20` 是「每个 partition 最多 ~20 个在途合并」的过估（实际取决于 merge factor）。超过阈值就整体重建；重建后若 `新长度 × 2 >= 旧长度` 则 `warn!` 提示这可能是 bug。**阈值刻意取成重建后预期规模的 2 倍以上，从而把重建摊还成均摊 $O(1)$**。

**复杂度**（$P$ = partition 数，$S$ = young split 总数，$M$ = 本轮 merge op 数）：

| 步骤 | 时间 |
|---|---|
| `compute_merge_ops` | $O(\sum_p S_p \log S_p) \le O(S \log S)$ |
| 调度排序（`sort_by_cached_key`） | $O(M \log M)$ |
| `known_split_ids` 维护 | 均摊 $O(1)$ |
| 重建 | 摊还 $O(1)$/split |

### 20.3 删除 = 计划 + Merge，而不是原地改

**Quickwit 没有逐 doc 的原地 update，也没有 per-doc 墓碑。** 删除走「split 级 `delete_opstamp` + 重写 split」两件套。规划器在 `quickwit-janitor/src/actors/delete_task_planner.rs`，其源码注释本身就是一份伪算法：

```text
常量：PLANNER_REFRESH_INTERVAL = 60s，NUM_STALE_SPLITS_TO_FETCH = 1000

loop:
  1. list_delete_tasks → 推出 last_opstamp
  2. list_stale_splits：取 delete_opstamp < last_opstamp 的 split，最近 N=1000 个，按
     delete_opstamp 升序。排除 immature split（metastore 无 merge policy 知识，只能 planner 侧滤）
     ⚠️ 源码承认这个缺口：若 immature stale split 超过 N，本轮什么也不做。
        缓解理由是「merge policy 会把老 split 判为 mature，且索引不该有太多 immature split」
        （追踪 issue #2147）
  3. 无 stale split → 本轮结束
  4. 逐个 stale split：
       a. 取该 split.delete_opstamp 之后的 delete tasks
       b. 用 split 自身的 time_range + tags 过滤掉不可能命中的 query
       c. 无剩余 query → metastore.update_splits_delete_opstamp(split, last_opstamp)   ← 廉价路径
       d. 有剩余 query → 逐条发 leaf_request 试探
            任一命中 → 包成 MergeOperation{ operation_type: DeleteAndMerge, splits: vec![split] }
                       送 MergeExecutor
            全不命中 → 回到 (c)
```

**关键权衡**：(c) 是一次元数据写，$O(1)$ 个字节；(d) 要下载整个 split、重写、上传、再删旧 split。**(b) 的 metadata 预过滤是整个设计的节流阀**——因为绝大多数 delete task 的时间范围 / tags 根本无法命中该 split，它们在 (c) 就被吸收了，根本不产生重写。

`DeleteAndMerge` 复用同一套合并执行器（`quickwit-indexing/src/actors/merge_executor.rs::process_delete_and_merge` → `merge_split_directories`）：`delete_tasks` 非空时把每条 query 的 `query_ast` 反序列化成 `QueryAst` 逐条应用到合并。

**第二条删除路径：retention（时间淘汰）**

`quickwit-janitor/src/retention_policy_execution.rs`：

```text
run_execute_retention_policy(index_uid, metastore, retention_policy):
    max_retention_ts = now - retention_period
    query = ListSplitsQuery::for_index(index_uid)
              .with_split_state(SplitState::Published)
              .with_time_range_end_lte(max_retention_ts)       # ← 时间范围上界，索引友好的条件
    (expired, ignored) = list_splits(query).partition(|s| s.time_range.is_some())
    if ignored 非空: warn!("Retention policy could not be applied to N splits
                          because they lack a timestamp range.")
    metastore.mark_splits_for_deletion(expired_ids)            # 只打标记
```

标记 ≠ 删除。真正的对象存储文件删除由 `quickwit-janitor/src/actors/garbage_collector.rs` 批量执行，中间有 **32 分钟宽限期**（`quickwit-common/src/shared_consts.rs`）：

```rust
/// We cannot safely delete splits right away as a:
/// - in-flight queries could actually have selected this split,
/// - scroll queries may also have a point in time on these splits.
/// …This duration is controlled by `DELETION_GRACE_PERIOD`.
pub const DELETION_GRACE_PERIOD: Duration = Duration::from_secs(60 * 32); // 32 min
```

这个常量还被反向约束了 scroll 的 TTL 上限（`quickwit-search/src/root.rs`）：

```rust
const MAX_SCROLL_TTL: Duration = Duration::from_secs(DELETION_GRACE_PERIOD.as_secs() - 60 * 2);
```

即**保证任何 scroll 一定在 split 被物理删除之前结束**。这是「无 shard、对象存储直查」架构必须显式处理的一个正确性缺口。

### 20.4 搜索侧：三级时间剪枝 → Rendezvous 放置 → leaf

Quickwit 的查询路径上有**三处剪枝**，成本依次递增：

**一级：metastore 级，零字节读取**

`quickwit-search/src/lib.rs::list_relevant_splits`：

```text
query = ListSplitsQuery::try_from_index_uids(index_uids)
          .with_split_state(SplitState::Published)
        [ .with_time_range_start_gte(start_ts) ]
        [ .with_time_range_end_lt(end_ts)       ]
        [ .with_tags_filter(tags_filter)        ]
```

这一步**完全在元数据库里完成**，连 footer 的一个字节都不读。日志查询里 `start_timestamp` / `end_timestamp` 通常能把候选 split 砍掉 90% 以上——这是 §8.5 那张 JSON 里两个时间参数的真正作用位置。

**二级：segment 级，需要 fast field**

`quickwit-search/src/filters.rs::TimestampFilterBuilder::build`：

```text
if (start, end) == (Unbounded, Unbounded): return None
column = segment_reader.fast_fields().column_opt::<DateTime>(field)?
         ?? Column::build_empty_column(max_doc)          # 取不到 → 全不匹配
if column.index.get_cardinality() == Cardinality::Full:
    segment_range = [column.min_value(), column.max_value()]
    if is_segment_always_within_timestamp_range(segment_range, time_range): return None  # 整段放行
return Some(TimestampFilter { time_range, timestamp_column })   # 否则逐 doc 判定
```

区间语义：**`start` 是 `Bound::Included`、`end` 是 `Bound::Excluded`**，即半开区间 $[start, end)$。`Cardinality::Full` 这个前置条件很重要——稀疏 fast field 的 `min_value/max_value` 不代表真实分布，跳过整段会误杀。

**三级：leaf 内部，按当前 top-K 剪枝**（`quickwit-search/src/leaf.rs`）

leaf 维护 `CanSplitDoBetter`（从 `SearchRequest` 推出）：

| 情形 | 判据 | 剪枝谓词 |
|---|---|---|
| `SplitIdHigher` | 无 sort 字段（按 `_shard_doc` 排） | `split_id >= 当前最差 hit 的 split_id` |
| `SplitTimestampHigher` | 按时间字段**降序** | `split.timestamp_end() >= 当前 top-K 中最小值` |
| `SplitTimestampLower` | 按时间字段**升序** | `split.timestamp_start() <= 当前 top-K 中最大值` |
| `Uninformative` | 其它排序键 | 不剪 |

配套的 `optimize_split_order` 按同一键**先排处理顺序**，让最可能填满 top-K 的 split 排在前面。源码注释承认这是启发式：「In the future, as split get more metadata per column, we may be able to do this more than just for timestamp and 'unsorted' request.」

这三级剪枝的元数据（`timestamp_start` / `timestamp_end` / `split_id`）随 `SplitIdAndFooterOffsets` 一起经 gRPC 从 root 传给 leaf，**判断 split 是否可能更好不需要下载任何字节**。

**作业放置：Rendezvous hashing + power-of-two-choices**

`quickwit-common/src/rendezvous_hasher.rs`：

```text
node_affinity(node, key):
    state = DefaultHasher::new()
    key.hash(&mut state)
    node.hash(&mut state)
    return state.finish()                 # u64，越大亲和性越强

sort_by_rendez_vous_hash(nodes, key):
    nodes.sort_by_cached_key(|n| Reverse(node_affinity(n, &key)))   # 降序
```

`quickwit-search/src/search_job_placer.rs::assign_jobs`：

```text
assign_jobs(jobs, excluded_addrs):
    candidate_nodes = 池中未被排除的节点，每个 load = 0
    if candidate_nodes 为空: bail!("failed to assign search jobs. there are no available searcher nodes in the pool")
    jobs.sort_unstable_by(compare_cost)     # cost 降序；split_id 升序 tie-break
    for job in jobs:
        sort_by_rendez_vous_hash(candidate_nodes, job.split_id())   # 每步都重排
        chosen = if len >= 2 { (load[0] > load[1] ? 0 : 1) } else { 0 }
        candidate_nodes[chosen].load += job.cost()
        job_assignments[chosen.grpc_addr].push(job)
    return job_assignments
```

**为什么是这个组合**：

- **Rendezvous hashing** 解决「亲和 + 最小扰动」：同一 split_id 恒定映射到亲和度最高的节点（复用本地 split cache 命中），且节点增减时只有 $O(K/N)$ 比例的 key 需要重映射，远优于取模哈希的 $O(K)$。对比 Consistent Hashing，Rendezvous 无需虚拟节点、无需环结构，实现只有 6 行。
- **每步重排 + 取 top-2 中 load 小者** 是 **power-of-two-choices**：从亲和度最高的 2 个候选里挑更闲的。相比均匀随机分配，最大负载期望改善 $O(\log \log N)$ 倍；相比全局最小负载，代价是 $O(N)$ 而非 $O(\log N)$。
- **cost 降序**保证重(split / 贵 split)先被分配，此时它们面对的节点池最闲。

**复杂度**（$J$ = 作业数，$N$ = 候选节点数）：$O(J \cdot N \log N)$。`sort_by_cached_key` 会缓存 key 的计算，但每轮因 `key = job.split_id()` 变化仍需重排。

> ⚠️ **重要限制**：`quickwit-search/src/root.rs::compute_split_cost` 目前**恒返回 1**：
> ```rust
> fn compute_split_cost(_split_metadata: &SplitMetadata) -> usize {
>     // TODO: Have a smarter cost, by smoothing the number of docs.
>     1
> }
> ```
> 因此所有作业 cost 相同 → `compare_cost` 退化为纯 split_id 字典序 → power-of-two-choices 的「选闲」步骤虽仍在跑，但因为负载永远同步增长，实际效果**退化为按 split_id 均匀轮转**。这是本轮核实到的最值得注意的「机制已就位、代价模型待补」的位置：**先把分布做完，再谈分片**，Quickwit 在多 split 单节点场景下还没做。

**作业打包**（`quickwit-search/src/root.rs::jobs_to_leaf_requests`）：按 `index_uid` 用 `itertools::group_by` 分组，每组打成一个 `LeafSearchRequest`——**跨 split 的 gRPC 调用次数从 O(作业数) 降到 O(index 数)**。

**leaf 侧四级缓存**（`quickwit-search/src/leaf.rs::open_index_with_caches`，由外向内）：

| 层 | 结构 | 键 | 默认容量 |
|---|---|---|---|
| 1. footer 缓存 | `MemorySizedCache<String>` | split_id | **500 MB** |
| 2. split 磁盘缓存 | `SplitCache`（LRU） | split_id | **1 GB / 100 个 / 并发下载 1** |
| 3. fast field 缓存 | `MemorySizedCache` | 文件 + offset | **1 GB** |
| 4. 临时目录 | `CachingDirectory::new_unbounded` | — | 无界，生命周期绑到本次返回的 `Index` |

顺序有讲究：`SplitCache` 必须包在 `BundleStorage` **之外**（此时读的还是 `.split` 文件），而 `fast_fields_cache` 包在 `BundleStorage` **之内**（此时读的是 bundle 解出来的文件切片）——这样「split 文件的 IO」和「bundle 内字节的 IO」走两套独立的缓存策略。

其余默认值（`quickwit-config/src/node_config/mod.rs::SearcherConfig`）：`partial_request_cache_capacity = 64 MB`、`max_num_concurrent_split_searches = 100`、`max_num_concurrent_split_streams = 100`、`aggregation_memory_limit = 500 MB`、`aggregation_bucket_limit = 65000`。

### 20.5 Tantivy 复用边界

`quickwit/Cargo.toml` 里 tantivy 的依赖形态本身就说明了边界在哪：

```toml
[workspace.dependencies]
tantivy = { git = "https://github.com/quickwit-oss/tantivy/", rev = "108f30b",
            default-features = false,
            features = ["lz4-compression", "mmap", "quickwit", "zstd-compression"] }
chitchat = { git = "https://github.com/quickwit-oss/chitchat.git", rev = "a3e3f8b" }
opendal   = { version = "0.44", default-features = false }
```

三条要害：① 用的是 **Quickwit 自己 fork 的 tantivy**（`quickwit-oss/tantivy`）并锁 rev，不是 crates.io 版本；② 开了 `default-features = false`，只留压缩 + mmap + 一个 **名为 `quickwit` 的自定义 feature**；③ 对象存储走 **opendal 0.44**（`quickwit-storage/src/opendal_storage/{base,google_cloud_storage}.rs`），而不是自己实现 S3 / GCS / Azure 三套 SDK。

| 能力 | 归属 | 代码位置 |
|---|---|---|
| 倒排索引、FST、posting list、Block-Max WAND | **Tantivy（fork）** | `quickwit-search/src/collector.rs` 直接调 tantivy collector |
| fast field / columnar / `Column<DateTime>` / `Cardinality` | **Tantivy（fork）** | 同上 |
| BM25 与 `TopDocs` | **Tantivy（fork）** | 同上 |
| **聚合引擎** | **Tantivy（fork）** | `tantivy::aggregation::{AggregationLimits, AggregationSegmentCollector}`、`agg_req::{Aggregations, get_fast_field_names}`、`intermediate_agg_result::IntermediateAggregationResults` |
| Trace-ID 聚合（Jaeger 专用） | **Quickwit 自研** | `quickwit-search/src/find_trace_ids_collector.rs`，`QuickwitAggregations::FindTraceIdsAggregation` |
| Query AST / Lucene DSL / ES DSL 解析 | **Quickwit 自研** | `quickwit-query/src/query_ast/`、`quickwit-query/src/elastic_query_dsl/`（14 个查询类型 + `visitor.rs`） |
| `Directory` 抽象 / hotcache / bundle 打包 | **Quickwit 自研** | `quickwit-directories/src/{hot,bundle,caching,storage,union,debug_proxy}_directory.rs` |
| split 概念 / bundle 布局 / 合并策略 / Metastore | **Quickwit 自研** | `quickwit-indexing/src/{merge_policy,split_store}/`、`quickwit-metastore/src/split_metadata.rs` |
| 分布式调度 / Rendezvous 放置 / Janitor | **Quickwit 自研** | `quickwit-search/src/search_job_placer.rs`、`quickwit-janitor/src/` |
| Actor 框架 | **Quickwit 自研** | `quickwit-actors/` |
| 对象存储协议 | **opendal** | `quickwit-storage/src/opendal_storage/` |
| 集群成员发现 | **chitchat（fork）** | `quickwit-cluster/` |

> ⚠️ **纠正 brief 与旧节**：v0.7.1 的 workspace **没有** `quickwit-aggregations` crate（`git ls-tree v0.7.1 quickwit/` 全部 37 个目录中无此条目）。聚合来自 `tantivy::aggregation`；Quickwit 在其外只包了一层 `QuickwitAggregations` 枚举（`quickwit-search/src/collector.rs`），用于塞进一个 Jaeger 专用的 `FindTraceIdsAggregation` 变体。旧节 §9.2 引用的 `quickwit-aggregations/src/{bucket,metric,pipeline,collector}/` 路径在 v0.7.1 全部不存在。

## 二十一、Elasticsearch 兼容 API 的真实边界（算法展开）

### 21.1 路由清单：全部挂在 `_elastic` 前缀下

`quickwit-serve/src/elasticsearch_api/mod.rs::elastic_api_handlers` 用 warp 的 `.or()` 链注册了 9 个 handler，实测路由（全部以 `/api/v1/_elastic` 为前缀）：

| ES 语义 | Quickwit 路由 | 实测状态 |
|---|---|---|
| `GET /` | `_elastic/` | ✅ `es_compat_cluster_info_handler` |
| `_search`（全索引，无 index 前缀） | `_elastic/_search` | ❌ **未实现**，handler 直接返回错误提示「`_elastic/_search` is not supported yet. Please try the index search endpoint」 |
| `{index}/_search` | `_elastic/{index}/_search` | ✅ `es_compat_index_search_handler` |
| `{index}/_count` | `_elastic/{index}/_count` | ✅（内部仍走 `search_service.root_search`，只是改计数输出） |
| `_search/scroll` | `_elastic/_search/scroll` | ✅ `es_compat_scroll_handler` |
| `{index}/_msearch` | `_elastic/{index}/_msearch` | ✅ `es_compat_index_multi_search_handler` |
| `{index}/_field_caps` | `_elastic/{index}/_field_caps` | ✅ `es_compat_index_field_caps_handler` |
| `_bulk` | `_elastic/_bulk` | ✅ `es_compat_bulk_handler` |
| `{index}/_bulk` | `_elastic/{index}/_bulk` | ✅ `es_compat_index_bulk_handler` |

**第一层「不兼容」**：多了一个 `_elastic` 段。因此标准 ES 客户端（`elasticsearch-py` / `opensearch-py` 等）**不能零配置直连**，必须改 base path 或注入自定义前缀。

### 21.2 `_bulk` 的语义映射：只有 create / index，`_id` 被解析后丢弃

`quickwit-serve/src/elasticsearch_api/model/bulk_body.rs` 全文只有两个 action 变体：

```rust
#[serde(rename_all(deserialize = "lowercase"))]
pub enum BulkAction {
    Create(BulkActionMeta),
    Index(BulkActionMeta),
}

pub struct BulkActionMeta {
    #[serde(alias = "_index")] #[serde(default)] pub index_id: Option<IndexId>,
    #[serde(alias = "_id")]    #[serde(default)] pub doc_id:  Option<String>,
}
```

对照 ES 的四种 bulk action：

| ES action | Quickwit | 结果 |
|---|---|---|
| `create` | ✅ 支持 | 追加写；**不做存在性检查**（ES 的 create 冲突会 409） |
| `index` | ✅ 支持 | 与 `create` 在 Quickwit 侧**完全等价** |
| `update` | ❌ | serde 反序列化直接失败（未知 tag） |
| `delete` | ❌ | 同上；源码里有对应单测断言 `serde_json::from_str::<BulkAction>(...).unwrap_err()` |

**最坑的一条**：`doc_id`（来自 ES 的 `_id`）被 serde **解析出来**，但在 `quickwit-serve/src/elasticsearch_api/bulk.rs` 与 `bulk_v2.rs` 里**从未被读取**（`grep doc_id` 在这两个文件中零命中）。含义是：

> **Quickwit 的 `_bulk` 没有 `_id` 幂等语义。** 同一个 `_id` 重复提交会产生两份独立文档，而不是覆盖。客户端若依赖 ES 的「用 `_id` 做 upsert」，切到 Quickwit 后会静默产生重复数据。

这个「看起来兼容、实则不等价」的点是整条兼容链上最容易踩的雷。

**`_index` 缺省语义**（源码注释，与 ES 对齐）：

> when ingesting on `/my-index/_bulk`, if `_index:` is set to something else than `my-index`, ES honors it and create the doc in the requested index. That is, `my-index` is a default value in case `_index:` is missing, but not a constraint on each sub-action.

实现是 `action.into_index_id().or_else(|| default_index_id.clone())`；两处都没有则返回 400 `missing required field: _index in the line [#{line_number}]`。

**性能护栏**（`IngestApiConfig` 默认，`quickwit-config/src/node_config/mod.rs`）：`max_queue_memory_usage = 2 GiB`、`max_queue_disk_usage = 4 GiB`、`replication_factor = 1`、`content_length_limit = 10 MiB`。`refresh` / `wait_for` 语义被映射到「阻塞直到 ingest 队列接收」，而 Quickwit 的可见性终点是 split 被 publish 进 metastore，不是 ES 的 segment refresh。

**复杂度**：body 解析 $O(B)$（$B$ = body 字节数），按 `index_id` 分组用 `itertools::group_by`（要求输入预先按 index_id 有序，故 $O(B \log B)$ 以排序 + $O(B)$ 分组）。

### 21.3 `_search` 的降级路径：`search_after` 与 scroll

`partial_hit_from_search_after_param`（`quickwit-serve/src/elasticsearch_api/rest_handler.rs`）把 ES 的 `search_after` 数组翻译成 Quickwit 的 `PartialHit { split_id, segment_ord, doc_id, sort_value, sort_value2 }`，其中 `GlobalDocAddress` 的字符串形式就是 `_shard_doc` / `_doc` 的 sort 值：

```rust
const SORT_DOC_FIELD_NAMES: &[&str] = &["_shard_doc", "_doc"];
```

Quickwit 没有全局唯一 doc id（§18.1），所以 **deep pagination 只能靠 `(split_id, segment_ord, doc_id)` 三元组定位**——这正是 `_shard_doc` 的语义来源。

scroll 侧则被 §20.3 的删除宽限期约束：

```rust
const MAX_SCROLL_TTL: Duration = Duration::from_secs(DELETION_GRACE_PERIOD.as_secs() - 60 * 2);
```

即 **scroll 最长只能活 30 分钟**，因为 split 在标记删除后 32 分钟会被 GC 物理移除。这是「无 shard、对象存储直查」架构必须暴露给客户端的硬约束——ES 里 scroll 没有这个上限（它有自己的 segment 保留策略）。

### 21.4 ES 兼容层与 brief 说的差异（三条对照）

| brief 说法 | 实测（v0.7.1） |
|---|---|
| 「`_search` / `_bulk` / `_msearch` 的语义映射与差异」 | 三者都存在，但**全部在 `_elastic` 前缀下**，且 `_search`（无 index）**明确未实现** |
| 隐含 ES 的 update / delete 语义 | **不支持**。只有 `create` / `index` 两种追加写 action，`update` / `delete` 在反序列化阶段失败 |
| 隐含 `_id` 有幂等作用 | **被解析后丢弃**，不参与去重、不触发 upsert |

---

## 二十二、参考资料

1. Quickwit 官方文档：<https://quickwit.io/docs/>
2. Quickwit GitHub：<https://github.com/quickwit-oss/quickwit>
3. Quickwit Cloud：<https://quickwit.io/cloud>
4. *"Search Your Petabyte in 1 Second"*（Quickwit 论文 + 博客）
5. Tantivy 文档：<https://tantivy.com/docs/>
6. *Block-Max WAND* 学术论文
7. *Optimized Top-K Document Retrieval Using Heaps*（Ding & Bressan 2017）
8. *In Search of an Understandable Consensus Algorithm*（Raft 论文）
9. Object Storage 最佳实践：AWS S3 / Azure Blob / GCS 官方文档
10. Quickwit 官方 benchmark 与案例：<https://quickwit.io/case-studies/>
