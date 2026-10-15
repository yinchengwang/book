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

## 十八、参考资料

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
