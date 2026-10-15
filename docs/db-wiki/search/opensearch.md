# OpenSearch Wiki（AWS 维护的搜索分析引擎，Elasticsearch 分支）

> 参考版本：OpenSearch 2.x（commit `main`，Lucene 9.x 内核），Java 实现，源于 Elasticsearch 7.10（最后 Apache 2.0 版本），由 **AWS + OpenSearch 社区** 主导。本 wiki 重点覆盖 OpenSearch 独有的扩展：OpenSearch Dashboards、Anomaly Detection、PPL、Sparse/Dense Retrieval、SQL Engine、Alerting。

## 一、项目定位

OpenSearch（原名 **Open Distro for Elasticsearch**，后更名）是 **AWS + Linux Foundation 旗下 OpenSearch 项目** 维护的 **分布式搜索与分析引擎**。源自 **Elasticsearch 7.10.2 的最后 Apache 2.0 fork**，并在之上加入 **OpenSearch Dashboards**（替代 Kibana）、**PPL/SQL DSL**、**Anomaly Detection**、**Alerting**、**Notebooks**、**Index State Management**、**Trace Analytics**、**ML 插件** 等原生能力。

- **痛点**（沿袭 ES）：
  - 全文搜索、NRT、聚合、向量检索、可观测性平台；
- **OpenSearch 差异化**：
  - **完全开源（Apache 2.0）**：无 SSPL 限制；
  - **原生告警与异常检测**：无需 Logstash/X-Pack；
  - **PPL / SQL 双查询语言**：与 ES SQL 不兼容；
  - **OpenSearch Dashboards**（Kibana 替代）；
  - **Trace Analytics** + **Pipelines** + **Receivers** + **Service Map**（APM 链路分析）；
  - **Federated Search**（跨集群联邦）；
  - **Asynchronous Search**（异步查询）；
  - **k-NN with Native Engine**（Lucene + Faiss / NMSLIB 自选）；
- **典型用户**：AWS OpenSearch Service 客户（VPC 内搜索 / SIEM）、Log Analytics、电商搜索、社区贡献者（OPSC / OpenSearch Software Foundation）；
- **核心口号**：**"OpenSearch: The Open Source Search and Analytics Suite"** + **"Community-driven, Open Standards"**。

## 二、发展历史与社区

- **起源**：
  - **2021-01**：AWS 在 Elasticsearch 变更协议（SSPL）后，宣布维护 **Open Distro for Elasticsearch 1.13**（基于 ES 7.10.2）；
  - **2021-04**：Linux Foundation 成立 **OpenSearch Software Foundation**，迁移至 OpenSearch 品牌；
  - **2021-07**：OpenSearch 1.0 发布（基于 ES 7.10.2 fork + 增强）；
- **里程碑**：
  - 2021-07：1.0 GA，`opensearch-project/OpenSearch` 仓库成立；
  - 2021-11：1.1 / 1.2 改进 **Trace Analytics**、**Anomaly Detection** 算法；
  - 2022-05：1.3 引入 **Asynchronous Search**、**Searchable Snapshots** 增强；
  - 2022-11：2.0 **Lucene 9.x 升级**（同步 ES 8.x）、**OpenSearch Dashboards** 重命名 + **OpenSearch Project** 拆分多仓；
  - 2023-Q1：2.4 引入 **ML 插件算法增强**、**PPL `eval` 子句**、**k-NN 性能改进**；
  - 2023-Q3：2.7 引入 **Semantic Search**、**Cohere / SageMaker 嵌入集成**、**Neural Search**；
  - 2024-Q1：2.10 引入 **Searchable Snapshot Full Tier** + **Performance Analyzer** 重写；
  - 2024-2026：2.x 持续增强 **Vector Search**、**RAG / GenAI**、**Multimodal**、**GA of Piped Query Language**；
- **维护状态**：极活跃，AWS 主导 + Linux Foundation 维护，**GitHub ~9k stars**；
- **社区**：OpenSearchCon 年度大会、Official Forum、Slack、Reddit r/opensearch、GitHub Discussions 活跃。

## 三、许可与商业限制

- **许可证**：**Apache License 2.0**（核心 + plugins / dashboards）。无任何 SSPL/BSL 限制；
- **AWS 托管服务**：
  - **Amazon OpenSearch Service**：托管域（原 ElasticSearch Service 后更名），支持 **Development / Production / Custom** 多模式；
  - **Amazon OpenSearch Serverless**：按查询/写入单元计费，无实例；
  - **Amazon OpenSearch Ingestion**（基于 Data Prepper，实时 ingest）；
  - **Amazon OpenSearch Zero-ETL Integration**（S3 / CloudWatch / DynamoDB 实时同步）；
- **AWS 之外的商业版本**：
  - **Aiven for OpenSearch**（Aiven 公司）；
  - **Bonsai / ElasticPact / OpenSearch as a Service**（多家云厂商）；
  - **OpenObserve** + **Quickwit** 也提供兼容接口；
- **生态关系**：
  - 完全替代 Elastic Stack（Kibana → OpenSearch Dashboards）；
  - 与 **Data Prepper**（同仓库，OTel 采集）打通；
  - 与 **AWS Glue / Kinesis / S3 / IAM / Cognito**（托管版内置 IAM）集成。

## 四、业界主要使用方与案例

- **超大型用户**（公开）：
  - **AWS 自家服务**：Amazon CloudWatch Logs Insights 底座（部分）、Code Search、EventBridge 规则引擎；
  - **Netflix**：日志聚合 / Trace Analytics；
  - **Uber**：调度系统指标监控；
  - **LinkedIn**：招聘搜索基础设施；
  - **Slack**：消息搜索（部分）；
  - **Expedia**：旅行内容索引；
  - **Bloomberg**：金融数据索引；
- **AWS 客户**：金融、电商、广告、安全、媒体、政府；
- **典型场景**：SIEM（Security Analytics）、Log Analytics、APM（Trace Analytics）、电商商品搜索、向量检索 + RAG、AI 应用 GenAI Search。

## 五、一句话对比定位

> OpenSearch 是 **"Apache 2.0 的 Elasticsearch fork + OpenSearch Dashboards + PPL/SQL 双 DSL + Anomaly Detection + Alerting + Trace Analytics + Federated Search + Asynchronous Search + k-NN 原生 + 异常检测"** 的全栈搜索分析平台；与 **Elasticsearch**（商业化趋强）、**Apache Solr**、**Meilisearch / Typesense**、**Tantivy**、**Quickwit** 构成搜索引擎阵营。OpenSearch 的差异化是 **"完全开源 + AWS 整合 + 全栈监控 + 内置告警异常检测"**。

## 六、整体架构

```
┌─────────────────────────────────────────────────────────────────────────┐
│                  客户端 / 客户端协议层                                   │
│  - REST API（9200，默认）/ HTTPS                                         │
│  - 官方 SDK：opensearch-java / opensearch-py / opensearch-go / .NET /   │
│    opensearch-js / opensearch-rs / opensearch-php / opensearch-ruby    │
│  - OpenSearch Dashboards（Kibana 替代）                                  │
│  - JDBC / ODBC (opensearch-sql-jdbc)                                    │
│  - Data Prepper (OTel 采集)                                             │
└─────────────────────┬───────────────────────────────────────────────────┘
                      │ HTTP/JSON
┌─────────────────────▼───────────────────────────────────────────────────┐
│                  协调节点（Coordinating Node）                          │
│  server/src/main/java/org/opensearch/action/                            │
│  ├─ search/SearchAction：search 入口（async / federated 兼容）         │
│  ├─ bulk/TransportBulkAction                                            │
│  ├─ admin/cluster/：集群管理                                             │
│  └─ RestController / RestHandler                                       │
└─────────┬───────────────────┬──────────────────────┬───────────────────┘
          │ search fan-out    │ aggregation           │ PPL / SQL 翻译
┌─────────▼───────┐ ┌────────▼──────────┐ ┌────────▼──────────┐
│  Master 节点    │ │ Data 节点          │ │ Plugin 节点       │
│  - 选举 / ZGC   │ │  - 持有 Primary    │ │  - alerting /     │
│  - cluster/    │ │  - 持有 Replica    │ │    anomaly /      │
│    service/    │ │  - shard 转发      │ │    ml / sql /     │
└──────┬─────────┘ │  - Lucene segment  │ │    knn            │
       │           └────────────┬───────┘ └────────┬──────────┘
       │                        │                 │
       │              Lucene IndexShard (per shard)
       │              ├─ IndexWriter（write + translog）
       │              ├─ Searcher（NRT）
       │              └─ codec: Lucene 9.x（CamelCase 命名保留）
       │
┌──────▼───────────────────────────────────────────────────────────────┐
│  Discovery / Cluster State                                             │
│  - cluster/coordination/JoinedClusterStatePublisher / ZGCA             │
│  - gateway/GatewayMetaState                                            │
└──────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│  Plugin / Module 体系（OpenSearch 增强的核心）                       │
│  ├─ plugins/alerting            # 告警引擎                          │
│  ├─ plugins/anomaly-detection   # Random Cut Forest + RCF          │
│  ├─ plugins/sql                 # SQL + PPL 翻译                    │
│  ├─ plugins/knn                 # kNN（Faiss / NMSLIB / Lucene）    │
│  ├─ plugins/ml-commons          # ML 任务编排                       │
│  ├─ plugins/neural-search       # Semantic / RAG                   │
│  ├─ plugins/ingest-pipelines    # Painless Script Pipeline          │
│  └─ plugins/notifications       # 邮件 / Slack / Webhook / Chime     │
└──────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│  Index Templates / Index State Management（ISM）                     │
│  - 滚动 / 收缩 / 删除 / 切副本                                      │
│  - 冷热分层 / searchable snapshot                                    │
└──────────────────────────────────────────────────────────────────────┘

                       │
┌──────────────────────▼──────────────────────────────────────────────┐
│  本地文件系统 / Searchable Snapshots（remote storage）              │
│  /var/lib/opensearch/nodes/0/indices/...                             │
│   ├─ segments_N                                                       │
│   ├─ index/（Lucene segment：.tim, .doc, .pos, .nvd, .dvd, ...）    │
│   ├─ translog/                                                        │
│   ├─ _state/                                                         │
│   └─ remote store（OBS / S3 缓存层）                                 │
└─────────────────────────────────────────────────────────────────────┘
```

### 关键设计取舍

- **从 ES 7.10 fork + 持续合并后续 ES 提交**：OpenSearch 在 ES 8.x、9.x 上游闭源后做了独立迭代，重写了 **Async Search、Federated Search、Trace Analytics、Anomaly Detection**；
- **PPL（Piped Processing Language）+ SQL 双 DSL**：PPL 是 OpenSearch 特有的管道式查询语言，类似 `|` 串联（`source` → `where` → `stats` → `eval`），与 SQL 平级；
- **k-NN Native Engine**：可插拔的向量索引引擎，支持 **Lucene（HNSW）+ Faiss（IVF/HNSW）+ NMSLIB** 三种实现；
- **Anomaly Detection 基于 RCF**：`plugins/anomaly-detection` 内置 Random Cut Forest 算法，无需外部 ML 框架；
- **Federated Search（cross-cluster）**：原生支持，符合 OpenSearch 路线图；
- **License：Apache 2.0**：与 Elastic 的 SSPL 形成对比，是 OpenSearch 最核心的差异化卖点；
- **Plugin 体系强扩展**：所有高级特性都以 plugin 形式存在（与 ES 的 x-pack 类比），但**默认全部开源**。

## 七、存储引擎（Lucene 9.x + 索引格式）

### 7.1 倒排索引（与 ES 相同的 Lucene 9.x 编码）

```
            Lucene Segment (per shard)
            ┌──────────────────────────┐
            │  Term Dictionary (FST)  │  ← in-memory
            └────────────┬─────────────┘
                         │
            ┌────────────▼─────────────┐
            │  Posting List (blocks)  │  ← block-based compression
            │   skip list (cross)     │  ← 跳表
            └────────────┬─────────────┘
                         │
            ┌────────────▼─────────────┐
            │  Position / Payload      │
            └──────────────────────────┘
```

源码（迁移自 ES 7.10 后小幅修改）：
- `server/src/main/java/org/opensearch/index/engine/InternalEngine.java`：写入引擎；
- `server/src/main/java/org/opensearch/index/mapper/FieldMapper.java`：映射；
- `core/src/main/java/org/apache/lucene/codecs/lucene90/...`：posting / doc_values 编码（vendored Lucene）。

### 7.2 索引格式

- **Lucene 9.x**：与 ES 8.x 主要差异在 codec 命名（CamelCase 命名空间 `Lucene90`/`Lucene91`/`Lucene92` 保留）；OpenSearch 2.x 默认 `Lucene92Codec`（对应 ES 8.7+ 风格）；
- **DocValues**：列存（`dvd`/`dvm`）默认开启（除 `text` 字段）；
- **Index Sort**：8.x 类同 ES，`index.sort.field` + `index.sort.order`；
- **Translog**：`server/src/main/java/org/opensearch/index/translog/Translog.java`，与 ES 7.10 一致。

### 7.3 Searchable Snapshots + Remote Store

- **Searchable Snapshots**：把 `.cfs`、`.si` 文件直接 mount 到只读缓存，远程对象存储内为权威；
- **Remote Store（2.10+）**：translog + 写入直接落对象存储（OBS/S3），本地只缓存；
- 源码：`server/src/main/java/org/opensearch/repositories/blobstore/`，`plugins/repository-s3/`，`plugins/repository-hdfs/`，`plugins/repository-oss/`。

## 八、索引机制（**搜索类型重点展开**）

### 8.1 倒排索引（Posting List + Term Dictionary + FST）

- **FST**：`core/src/main/java/org/apache/lucene/util/fst/FST.java`；
- **PostingsFormat**：`core/src/main/java/org/apache/lucene/codecs/lucene90/Lucene90PostingsFormat.java`：block-based（128 文档/block）+ skip list；
- **BlockTreeTerms**：`core/src/main/java/org/apache/lucene/codecs/lucene90/blocktree/...`：term dictionary 树形；
- 同 ES，由于直接 fork ES 7.10 + 部分上游改进，**底层倒排编码一致**。

### 8.2 BM25 / 评分（与 ES 相同）

- **默认 BM25**：`org.opensearch.common.lucene.search.function.FunctionScoreQuery`；
- **Boost、function_score**：与 ES 类似；
- **LTR（Learning to Rank）**：8.x 社区版支持，x-pack-style 但已经开源于 `plugins/learn-to-rank-plugin`（早期 ES 开源版本）；
- **script score**：Painless（OpenSearch 在 ES 的 Painless 上扩展）。

源码：
- `server/src/main/java/org/opensearch/index/similarity/SimilarityService.java`；
- `plugins/learn-to-rank-plugin/`。

### 8.3 向量检索（k-NN Plugin）

**OpenSearch k-NN 设计**（与 ES kNN 略有差别，是 plugin-first）：

| Vector Engine | 适用场景 | 关键参数 | 实现位置 |
|---------------|---------|---------|----------|
| **Lucene HNSW**（native） | 默认，10M+ | `ef_construction`, `m`, `ef_search` | `plugins/knn/lib/src/main/java/com/amazon/opensearch/knn/...` |
| **Faiss IVFFlat** | 超大规模 | `nlist`, `nprobe` | `plugins/knn-jni/src/main/java/...faiss...` |
| **NMSLIB HNSW** | 兼容 NMSLIB | 同 Lucene HNSW | `plugins/knn-jni/src/main/java/...nmslib...` |

Neural Search Plugin 在 k-NN 上加 **embedder**（`ml-commons` 提供模型 hosting），实现 **semantic_text 自动嵌入**：

```json
POST /neural-search-demo/_search
{
  "query": {
    "neural": {
      "passage_embedding": {
        "query_text": "how to clone in git",
        "model_id": "model_id"
      }
    }
  }
}
```

源码：
- `plugins/knn/src/main/java/org/opensearch/knn/...`：索引定义；
- `plugins/neural-search/src/main/java/org/opensearch/neuralsearch/...`：语义搜索。

### 8.4 过滤器 + 后置过滤

OpenSearch 与 ES 一样，`bool.filter`（filter context，**不评分 + 缓存**） vs `post_filter`（聚合时排除）。Filter cache 实现：

- 源码：`server/src/main/java/org/opensearch/indices/IndicesRequestCache.java`；
- 性能差异与 ES 完全一致。

### 8.5 Asynchronous Search（异步查询）

OpenSearch 独家提供 **异步查询**（Async Search），用于长查询场景（诊断型 ad-hoc）：

```http
POST /my_index/_async_search
{
  "size": 0,
  "query": { "match_all": {} },
  "aggs": { "avg_price": { "avg": { "field": "price" } } }
}

GET /_async_search/<id>
```

- 异步保留异步状态（`is_partial`、`completion_time_in_millis`、`response`），可分段 fetch；
- 源码：`server/src/main/java/org/opensearch/action/search/`、`server/src/main/java/org/opensearch/transport/RemoteClusterService.java`。

源码路径：
- `server/src/main/java/org/opensearch/action/search/AsyncSearchAction.java`；
- `server/src/main/java/org/opensearch/search/query/QueryPhase.java`。

## 九、查询 / 计算 / 接口层（**搜索类型重点展开**）

### 9.1 DSL 设计

#### 9.1.1 OpenSearch DSL（与 ES 大体兼容）

- 与 ES 7.10 兼容，命名小写简写：`_search`、`_bulk`、`_cluster/health`；
- 后续 8.x → 2.x 演进，去掉 `_doc` API，统一 `_index / _update / _delete`；
- 源码：`server/src/main/java/org/opensearch/rest/RestController.java`、`server/src/main/java/org/opensearch/action/ActionModule.java`。

#### 9.1.2 PPL（Piped Processing Language）— OpenSearch 独家

```sql
POST /opensearch_dashboards_sample_data_ecommerce/_search
{
  "query": {
    "query": "source = opensearch_dashboards_sample_data_ecommerce | where customer_first_name = 'Pablo' | stats count() by currency"
  }
}
```

- **特性**：
  - **管道式**：以 `|` 串联多个算子；
  - **算子集合**：`source`、`where`、`rename`、`eval`、`fields`、`stats`、`sort`、`head`、`dedup`、`parse`、`spath`、`lookup`、`join`；
  - **SQL 兼容映射**：与 SQL 算子对齐；
  - **时序函数**：`@timestamp`、`earliest` / `latest`；
- 源码：`plugins/sql/src/main/java/org/opensearch/sql/PPL/`、`plugins/sql/src/main/antlr/`（ANTLR 语法）。

#### 9.1.3 SQL Engine

```sql
POST /_plugins/_sql?format=json
{
  "query": "SELECT count(*) FROM ecommerce WHERE currency = 'EUR'"
}
```

- 源码：`plugins/sql/src/main/java/org/opensearch/sql/`、`plugins/sql/src/main/java/org/opensearch/sql/executor/`；
- 翻译为 OpenSearch DSL → Lucene Query。

#### 9.1.4 Lucene Query String（兼容）

- 复用 Lucene `queryparser-classic`；
- 源码：`server/src/main/java/org/opensearch/index/query/QueryStringQueryBuilder.java`。

### 9.2 聚合框架（Aggregation Framework）

三大类与 ES 一致：

#### 9.2.1 Bucket Aggregation

- `terms`、`date_histogram`、`histogram`、`nested`、`reverse_nested`、`sampler`、`diversified_sampler`、`significant_terms`、`geotile_grid`、`geohex_grid`、`range`、`date_range`、`ip_range`、`filters`、`adjacency`；
- 源码：`server/src/main/java/org/opensearch/search/aggregations/bucket/`。

#### 9.2.2 Metric Aggregation

- `sum`、`avg`、`min`、`max`、`stats`、`value_count`、`percentile_ranks`、`percentiles`、`cardinality`（HLL）、`top_hits`、`scripted_metric`、`geo_centroid`、`geo_bounds`、`median_absolute_deviation`；
- 源码：`server/src/main/java/org/opensearch/search/aggregations/metrics/`。

#### 9.2.3 Pipeline Aggregation

- `derivative`、`cumulative_sum`、`moving_avg`、`sum_bucket`、`avg_bucket`、`bucket_script`、`bucket_sort`、`max_bucket`、`min_bucket`、`bucket_selector`、`serial_diff`、`stats_bucket`、`percentiles_bucket`、`extended_stats_bucket`；
- 源码：`server/src/main/java/org/opensearch/search/aggregations/pipeline/`；
- **OpenSearch 扩展**：`cumulate_sum` + `derivative` 内置组合。

### 9.3 多租户搜索

#### 9.3.1 Index Alias + ISM（Index State Management）

- **ISM**：替代 ES 的 ILM，可配置 `states`(hot/warm/cold/delete) + `actions`(rollover/force_merge/delete/...)；
- 源码：`plugins/index-management/src/main/java/org/opensearch/indexmanagement/`。

#### 9.3.2 Federated Search（Cross-Cluster）

- 原生支持 `/_search?ccs_minimize_roundtrips=true`；
- 协调节点 fan-out 到指定 cluster 并 fetch；
- 源码：`server/src/main/java/org/opensearch/transport/RemoteClusterService.java`、`server/src/main/java/org/opensearch/action/search/SearchTransportService.java`；
- **Federated Indexing**（OpenSearch 独有）：写入也跨集群分发。

#### 9.3.3 Document Level Security（DLS）/ Field Level Security（FLS）

- 通过 **Security plugin**（Apache 2.0）；
- 文档级：`match`/`term`/`bool` 注入查询；
- 字段级：`fields.exclude`。

### 9.4 Trace Analytics（OpenSearch 独家）

- 配合 **Data Prepper**（OTel ingest），OpenSearch 提供：
  - `service-map`：服务依赖图（span parent/child）；
  - `trace-analytics`：trace 维度分析（latency / error rate / throughput）；
  - `service-level-objects`（SLOs）：SLO 数据点；
- 源码：`dashboards-observability/`、`observability/`、`dashboards-auditlogs/`。

### 9.5 Anomaly Detection（OpenSearch 独家）

- 源码：`plugins/anomaly-detection/src/main/java/`；
- 实现：**Random Cut Forest（RCF）+ AD-ARIMA / HC** 组合；
- 实时流式：基于 Pulse（time-series bucket），单节点可承载 200+ detector；
- 探测器接收 `plugins/ad-kibana-plugin` 的可视化。

## 十、事务与一致性（搜索库较弱）

- **单文档原子性**：与 ES 一致；
- **跨分片一致性**：`wait_for_active_shards` + translog fsync；
- **乐观并发控制**：`seq_no` + `primary_term`；
- **跨集群事务**：不支持；
- **数据副本**：`server/src/main/java/org/opensearch/action/support/replication/ReplicationOperation.java`；
- **Remote Store 事务**：translog 落对象存储，提供更好恢复能力。

## 十一、复制与高可用

- **主从 / 多主**：与 ES 一致（Primary + Replica）；
- **Allocation**：分片分配（disk watermark、forced awareness）；
- **Snapshot/Restore**：
  - 支持 S3、Azure Blob、GCS、OSS、HDFS、共享文件系统；
  - 源码：`server/src/main/java/org/opensearch/snapshots/`；
- **集群扩展**：
  - **Cluster Bootstrap**：从 cluster.initial_master_nodes 引导；
  - **Zen2/ZGCA Discovery**（与 ES 同系）；
- **跨区域**：通过 **Active-Active Federated** 或 **CCR Plugin**（Apache 2.0，`plugins/cross-cluster-replication/`）实现。

## 十二、运维与可观测性

- **监控**：
  - `_cluster/stats`、`_nodes/stats`、`_cat/indices`；
  - **Performance Analyzer（PA）**：CBO 启发的 performance analyzer，可读 TSS 状态（`plugins/performance-analyzer/`）；
  - 与 **Prometheus / Grafana**：通过 opensearch-prometheus-exporter；
- **ISM 自动化**：与 ES 的 ILM 类似，但**配置路径**在 `plugins/index-management/`；
- **告警与异常检测**：
  - **Alerting plugin**：`plugins/alerting/`；
  - **Anomaly Detection plugin**：`plugins/anomaly-detection/`；
  - **Notebooks** + **Observability Dashboards**（OpenSearch Dashboards）；
- **Observability**：
  - **OpenSearch Dashboards Observability**（Logs / Metrics / Traces 三页）；
  - **Service Map**：可视化的 trace 拓扑；
  - **Trace Analytics**：trace 维度分析。

## 十三、性能与基准

- **官方 Rally 兼容**：参考 `opensearch-project/opensearch-benchmark`（基于 elastic/rally fork）；
- **优化建议**：
  - 写入：`_bulk` + `refresh_interval: -1`；
  - 查询：filter cache + profile；
  - 集群：Master 分离 + Coordinating 分离；
- **与 ES 对比**：
  - 大多数场景性能与 ES 8.x 接近；
  - OpenSearch 优势：Apache 2.0 + 联邦 + 告警 + 异常检测原生；
  - 劣势：商业版功能（ML/Rank/Alerting）需借助 plugin 而非 x-pack 一体化。

## 十四、生态与集成

- **客户端 SDK**（官方 `opensearch-project` 名下）：
  - `opensearch-java`、`opensearch-py`、`opensearch-go`、`opensearch-rs`、`opensearch-dotnet`、`opensearch-js`、`opensearch-php`、`opensearch-ruby`；
  - JDBC：`opensearch-sql-jdbc`（与 `plugins/sql` 协同）；
- **OpenSearch Dashboards**（Kibana 替代）：
  - Visualize、Dashboard、Discover；
  - Notebooks；
  - Observability（Logs/Metrics/Traces）；
  - Security Analytics；
  - Anomaly Detection 可视化；
- **Data Prepper**：OTel 兼容的采集器；
- **AWS 集成**：
  - IAM / Cognito 内置；
  - OpenSearch Ingestion（基于 Data Prepper）；
  - OpenSearch Serverless（按查询/写入单元）；
- **ML / AI 生态**：
  - **ml-commons** 提供模型注册 + 任务编排；
  - **Semantic Search** 通过 **embedder** 集成 SageMaker / Cohere / Bedrock；
  - **Generative AI Connector**：直接对接 Bedrock Claude / Cohere Command。
- **容器化**：ACK / EKS / self-managed 全部支持 Helm Chart + Operator。

## 十五、学习路径

### 15.1 入门（1-2 周）

1. 跑通本地：`docker run -p 9200:9200 -p 9600:9600 -e "discovery.type=single-node" opensearchproject/opensearch:2.x`；
2. OpenSearch Dashboards：`docker run -p 5601:5601 opensearchproject/opensearch-dashboards:2.x`；
3. 教程：Mappings / Search / Aggregations / Alerting；
4. 试用 PPL（与 SQL 差异）。

### 15.2 进阶（4-6 周）

1. **必读源码**：
   - `server/src/main/java/org/opensearch/index/engine/InternalEngine.java`；
   - `server/src/main/java/org/opensearch/action/search/TransportSearchAction.java`；
   - `plugins/sql/src/main/java/org/opensearch/sql/`：SQL/PPL 实现；
   - `plugins/alerting/src/main/java/org/opensearch/alerting/`：告警引擎；
   - `plugins/anomaly-detection/src/main/java/org/opensearch/ad/`：RCF 异常检测；
   - `plugins/knn/src/main/java/org/opensearch/knn/`：向量引擎；
   - `plugins/ml-commons/src/main/java/org/opensearch/ml/`：ML 任务编排；
2. **PPL 语法**：`plugins/sql/src/main/antlr/PPL/` 下的 ANTLR 语法；
3. **集群原理**：Discovery + Allocation + State；
4. **跨集群联邦**：模仿 OpenSearch 的 `RemoteClusterService` 实现。

### 15.3 高级（长期）

1. **Vector 引擎选择**：Faiss / NMSLIB / Lucene HNSW 对比；
2. **Trace Analytics / Data Prepper**：集成 OTel；
3. **Searchable Snapshots + Remote Store**：对象存储优化；
4. **Plugin 开发**：`AbstractPlugin` + `ActionModule` 注入。

### 15.4 推荐资源

- **官方**：`opensearch.org/docs/latest/`、`opensearch-project/OpenSearch` GitHub；
- **会议**：OpenSearchCon 历届演讲；
- **论文**：BM25 + Faiss + RCF + HNSW 论文；
- **社区**：`discuss.opendistrocommunity.dev`（迁移到 opensearch）、Reddit、GitHub Discussions。

## 十六、风险与陷阱

1. **ES DSL 兼容性**：OpenSearch 2.x 兼容 ES 7.10 DSL，但与 ES 8.x 新特性（ES|QL、`semantic_text` 等）**不直接兼容**；迁移需评估；
2. **Plugin 与 Cluster Version 强绑定**：所有 plugin 必须对应 major.minor 版本；跨版本升级需谨慎；
3. **ISM 配置错误导致索引丢失**：state transition 未覆盖 snapshot 步骤可能在 rollover 后丢历史；
4. **Federated Query 慢**：跨集群聚合受网络影响大，对端集群压力大时容易 fan-out throttle；
5. **k-NN 训练内存大**：HNSW 索引构建需大量 RAM，>10M 100-dim 约需要 8GB+ heap；
6. **Anomaly Detection 误报率高**：RCF 在平稳时序上常出现 false positive；
7. **PPL `eval`/`lookup` 性能**：join 类操作在大数据集上慢；
8. **OpenSearch Dashboards 版本锁定**：Dashboards 与 OpenSearch 版本必须严格对齐（major.minor），且 dashboads plugin 与 OS plugin 严格对齐；
9. **Remote Store 跨区域延迟**：跨 region put 慢会拖累索引吞吐；
10. **License 上游风险**：OpenSearch 持续合并上游 ES 提交，但协议变更后部分提交无法直接 cherry-pick，可能错位演进。

## 十七、与本项目的关联

本项目 `engineering/` **未实现** OpenSearch 兼容引擎，但下列可借鉴：

- **PPL 管道式 DSL**：`engineering/src/algo/stream_query.c` 可借鉴 PPL 的 `source -> where -> stats` 管道模型设计教学级查询 DSL；
- **k-NN Native Engine 抽象层**：`engineering/src/index/` 已实现 HNSW，可参考 `plugins/knn` 的 **Engine 抽象**（Lucene/Faiss/NMSLIB 三种），在 `engineering/src/index/knn_engine.c` 增加引擎切换层；
- **ISM（Index State Management）**：参考 `plugins/index-management/` 在 `engineering/src/db/core/ilm.c` 增加状态机驱动生命周期管理；
- **Alerting + Anomaly Detection**：参考 `plugins/alerting/` + `plugins/anomaly-detection/` 在 `engineering/src/algo/` 增加 RCF + 阈值告警；

## 十八、参考资料

1. OpenSearch 官方文档：<https://opensearch.org/docs/latest/>
2. OpenSearch GitHub：<https://github.com/opensearch-project/OpenSearch>
3. OpenSearch Plugins：<https://github.com/opensearch-project>
4. k-NN Plugin：<https://opensearch.org/docs/latest/search-plugins/knn/>
5. ML Commons 文档：<https://opensearch.org/docs/latest/ml-commons-plugin/>
6. Neural Search：<https://opensearch.org/docs/latest/search-plugins/neural-search/>
7. Anomaly Detection：<https://opensearch.org/docs/latest/anomaly-detection/>
8. SQL / PPL：<https://opensearch.org/docs/latest/search-plugins/sql/index/>
9. Index State Management：<https://opensearch.org/docs/latest/im-plugin/index/>
10. Trace Analytics：<https://opensearch.org/docs/latest/observability-plugin/trace/index/>
