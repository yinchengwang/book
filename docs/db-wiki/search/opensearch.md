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

## 十八、分叉坐标系与三条内置机制：Analyzer 管线、Shard 分配决策链与集群故障检测

### 18.0 源码事实口径

本节（及 §十九、§二十）源码路径以 **OpenSearch 2.19.0**（主仓实测 tag `2.19.0`，commit `fd9a9d9`）+ **k-NN 插件 2.19.0.0**（独立仓实测 tag `2.19.0.0`）为 ground truth。本仓库 `reference/search/opensearch/` 子模块**未 init**（目录内除 `.git` 外 0 个条目），故一律**只给 canonical 路径、不给精确行号**（approx，行号需校对）。§二十中 k-NN 插件的路径位于**独立仓库** `opensearch-project/k-NN`，不是主仓子目录 —— 这点与 §8.3 / §15.2 的旧写法不同。

### 18.1 分叉坐标系：模块边界与「插件已独立成仓」

ES 7.10.2 fork 之后，OpenSearch 走了一条与 ES 完全不同的模块化路线：**主仓只留内核，功能全部外迁成独立仓库**。

| 层次 | 归属 | 载体 |
|------|------|------|
| 内核（server） | 主仓 `opensearch-project/OpenSearch` | 索引 / 搜索 / 分片 / 集群 / 转储 |
| Lucene | 上游 `apache/lucene`，以 Gradle 依赖引入 | **不在主仓内**（旧节 `core/src/main/java/org/apache/lucene/...` 是 ES 1.x 时代的布局） |
| 安全 | 独立仓 `opensearch-project/security` | DLS / FLS / Audit log |
| SQL / PPL | 独立仓 `opensearch-project/sql` | Calcite 解析 + 算子下推 |
| k-NN | 独立仓 `opensearch-project/k-NN` | Lucene / Faiss / NMSLIB 三引擎 |
| 公共库 | 独立仓 `opensearch-project/common-utils`、`job-scheduler` | 供上述插件共享 |

这条边界的**算法含义**是：内核侧（§18.2–§18.5、§十九）全部在主仓里可以直接读到源码；而能力侧（§二十的 k-NN、SQL）必须切到对应 tag 的独立仓才看得到。**版本对齐是最容易踩的坑**：主仓 tag 是 `2.19.0`，k-NN 仓的 tag 是 `2.19.0.0`（多一个 `.0`），SQL 仓又是另一套命名。

### 18.2 Analyzer 链的装配：charFilter → tokenizer → tokenFilter

OpenSearch 的 analyzer 不是黑盒，而是**三段固定结构**。`AnalyzerComponents.createComponents()` 把一份 Settings 解析成有序组件数组：

```java
// 简化自 server/.../index/analysis/AnalyzerComponents.java（2.19.0）
static AnalyzerComponents createComponents(name, analyzerSettings, tokenizers, charFilters, tokenFilters) {
    // 1) tokenizer 是强制的，缺失直接 IllegalArgumentException
    TokenizerFactory tokenizer = tokenizers.get(analyzerSettings.get("tokenizer"));

    // 2) char_filter 按声明顺序装配成数组
    List<CharFilterFactory> charFiltersList = new ArrayList<>();
    for (String n : analyzerSettings.getAsList("char_filter")) charFiltersList.add(charFilters.get(n));

    // 3) filter 按声明顺序装配，并且每装一个就把「已装前缀」传给工厂
    List<TokenFilterFactory> tokenFilterList = new ArrayList<>();
    for (String n : analyzerSettings.getAsList("filter")) {
        TokenFilterFactory f = tokenFilters.get(n);
        // 关键：链式感知工厂——shingle 等 filter 需要知道 tokenizer 用了什么、
        // 前置 char_filter 有哪些，才能推导出自己该输出几 gram
        f = f.getChainAwareTokenFilterFactory(tokenizer, charFiltersList, tokenFilterList, tokenFilters::get);
        tokenFilterList.add(f);
    }
    return new AnalyzerComponents(tokenizer, charFiltersList.toArray(...), tokenFilterList.toArray(...));
}
```

三点值得注意：

1. **顺序即语义**。charFilter 是「字符改写」（HTML strip、mapping 映射同义词、ICU 归一化），tokenizer 只看到一个 token 流，tokenFilter 再逐 token 变换。数组下标就是执行顺序。
2. **tokenizer 之前不做 token 级处理**。所以 `char_filter` 的设计前提是「不改 token 边界」——`NormalizingCharFilterFactory` 就是这条约束的守卫。
3. **AnalysisMode 在装配期归并**。构造函数里有一个容易忽略的循环：

   ```java
   // AnalyzerComponents 构造函数（2.19.0）
   AnalysisMode mode = AnalysisMode.ALL;
   for (TokenFilterFactory f : tokenFilters) mode = mode.merge(f.getAnalysisMode());
   this.analysisMode = mode;
   ```

   一个 filter 若声明了 `AnalysisMode.INDEX_TIME`（只在索引时跑）而 analyzer 整体是 `SEARCH_TIME`，装配期就会报错；反过来 analyzer 声明 `INDEX_TIME` 而 filter 只支持 `SEARCH_TIME` 同样拒绝。这条**编译期（索引创建期）校验**把「search_analyzer 里放了 asciifolding」这类错误提前到 mapping 阶段，而不是等到查询时。

源码路径（主仓，approx，无行号）：
- `server/src/main/java/org/opensearch/index/analysis/AnalyzerComponents.java`
- `server/src/main/java/org/opensearch/index/analysis/AnalysisMode.java`
- `server/src/main/java/org/opensearch/index/analysis/AnalysisRegistry.java`
- `server/src/main/java/org/opensearch/index/analysis/CustomAnalyzer.java` / `ReloadableCustomAnalyzer.java`
- `server/src/main/java/org/opensearch/index/mapper/TextParams.java`

### 18.3 三层 analyzer 覆写点与 normalizer 通道

一次文本分析用哪个 analyzer，要按**三层优先级**依次求值：

```text
查询级（一次查询可覆盖）
  └─ match / simple_query_string 的 "analyzer": "<name>"
索引级（settings.index.analysis.analyzer）
  ├─ "default"           ← 缺失时 AnalysisRegistry 自动补 StandardAnalyzerProvider
  ├─ "default_search"    ← 缺失时回落到 "default"
  └─ "default_search_quoted"  ← 缺失时回落到 "default_search"
字段级（mapping）
  ├─ "analyzer"              （只允许 AnalysisMode.INDEX_TIME）
  ├─ "search_analyzer"       （只允许 SEARCH_TIME，默认取 default_search，再回落 default）
  └─ "search_quote_analyzer" （只允许 SEARCH_TIME，默认取 default_search_quoted，再回落 search_analyzer）
```

三个常量定义在 `AnalysisRegistry`：`DEFAULT_ANALYZER_NAME = "default"`、`DEFAULT_SEARCH_ANALYZER_NAME = "default_search"`、`DEFAULT_SEARCH_QUOTED_ANALYZER_NAME = "default_search_quoted"`。回落链在 `IndexAnalyzers.getDefaultSearchAnalyzer()` 里写死为 `analyzers.getOrDefault("default_search", getDefaultIndexAnalyzer())`。

**normalizer 是并行的第二条通道**，专供 `keyword` 字段：它只能包含「每个 token 独立产出 1 个 token」的 filter（`NormalizingTokenFilterFactory` 的默认实现就是把 `normalize(TokenStream)` 委托给 `create(TokenStream)`）。`IndexAnalyzers` 里 `analyzers` / `normalizers` / `whitespaceNormalizers` 是**三个并列 Map**，查询期按字段类型分别查表。`LowercaseNormalizer` + `LowercaseNormalizerProvider` 是内置实现，`CustomNormalizerProvider` 支持自定义。

⚠️ **一个常见误解需要澄清**：网上流传的 `index.analyzed` / `search.analyzed` 两个布尔参数（分别控制「按索引侧结构分析」和「按查询侧结构分析」）**不是 OpenSearch 的参数**，它们出自 Elasticsearch `hybrid` 查询的 RRF 文档。在 OpenSearch 2.19.0 源码里，`MatchQueryBuilder` / `SimpleQueryStringBuilder` 暴露的是**单一字符串参数 `analyzer`**（`MatchQueryBuilder.analyzer(String)`、`SimpleQueryStringBuilder.analyzer(String)`）；填了一个未注册的 analyzer 名字会直接抛 `QueryShardException`：

```java
// 简化自 server/.../index/query/MatchQueryBuilder.java（2.19.0）
if (analyzer != null) {
    if (context.getIndexAnalyzers().get(analyzer) == null) {
        throw new QueryShardException(context, "[" + NAME + "] analyzer [" + analyzer + "] not found");
    }
    matchQuery.setAnalyzer(analyzer);
}
```

源码路径（主仓，approx，无行号）：
- `server/src/main/java/org/opensearch/index/analysis/IndexAnalyzers.java`
- `server/src/main/java/org/opensearch/index/analysis/CustomNormalizerProvider.java`
- `server/src/main/java/org/opensearch/index/analysis/NormalizingTokenFilterFactory.java` / `NormalizingCharFilterFactory.java`
- `server/src/main/java/org/opensearch/index/analysis/LowercaseNormalizer.java` / `LowercaseNormalizerProvider.java`
- `server/src/main/java/org/opensearch/index/query/MatchQueryBuilder.java` / `SimpleQueryStringBuilder.java`
- `server/src/main/java/org/opensearch/index/mapper/FieldMapper.java`

**代价**：

| 项 | 复杂度 | 说明 |
| --- | --- | --- |
| 索引期分析 | `O(|text|)`（单 token 恒定） | 每 charFilter/tokenizer/tokenFilter 各扫一遍流，层数 `k` 则 `O(k·\|text\|)` |
| 分词器预编译 | 建索引时一次性 | 分词状态机按「已消费前缀」缓存，长尾 term 才有状态扩张 |
| 词典膨胀 | — | `asciifolding`/`stemmer`/`synonym_graph` 会把 1 个 term 展开成多个 term，postings 与 FST 同步变大 |
| 索引 / 查询不对称 | — | `search_analyzer` 与 `analyzer` 不同是**特性不是 bug**，但配错会导致 0 命中且不报错 |

### 18.4 Shard 分配决策链：注册顺序、短路规则与三态决策

分片能否落在某个节点上，由一串 `AllocationDecider` 依次裁决。注册顺序是**有语义的**——`ClusterModule.createAllocationDeciders()` 用 `LinkedHashMap` 按类名去重并保序：

```text
1  MaxRetryAllocationDecider                  （重试次数上限，最先拦）
2  ResizeAllocationDecider
3  ReplicaAfterPrimaryActiveAllocationDecider （primary 激活前不许放副本）
4  RebalanceOnlyWhenActiveAllocationDecider
5  ClusterRebalanceAllocationDecider          （indices.recovery.max_concurrent_*_clusters）
6  ConcurrentRebalanceAllocationDecider
7  ConcurrentRecoveriesAllocationDecider
8  EnableAllocationDecider                    （cluster.routing.allocation.enable）
9  NodeVersionAllocationDecider               （混合版本集群版本约束）
10 SnapshotInProgressAllocationDecider
11 RestoreInProgressAllocationDecider
12 FilterAllocationDecider                    （require/include/exclude 过滤）
   [SearchReplicaAllocationDecider]           （受 feature flag 控制）
13 SameShardAllocationDecider
14 DiskThresholdDecider                       （low/high/flood stage 磁盘水位）
15 ThrottlingAllocationDecider
16 ShardsLimitAllocationDecider
17 AwarenessAllocationDecider                 （机架/可用区反亲和）
18 NodeLoadAwareAllocationDecider             （节点负载再平衡）
19 TargetPoolAllocationDecider
20 RemoteStoreMigrationAllocationDecider
   …最后追加各 ClusterPlugin 贡献的 decider
```

**短路规则**（`AllocationDeciders.canAllocate`）：遍历时一旦某个 decider 返回 `Decision.NO`，且当前 allocation **不是 debug 模式**，立即 `return`，后续 decider 一个都不跑。也就是说 `NO` 是「一票否决 + 提前退出」，链上第 1 个和第 20 个的代价差可能接近 20 倍。

**三态决策**（`Decision.Type`）不是布尔而是三值：

| Type | 序列化 id | 语义 | 能否提前终止 |
| --- | --- | --- | --- |
| `NO` | 0 | 绝对不允许 | ✅ |
| `YES` | 1 | 允许 | ❌ |
| `THROTTLE` | 2 | 允许，但触发限流 | ✅ |

优先级由 `Type.higherThan(other)` 给出：`NO > THROTTLE > YES`；而 `canPreemptivelyReturn()` 允许 `THROTTLE` 与 `NO` 短路，`YES` 必须走完全链（因为后面的 decider 仍可能给出 `NO`）。debug 模式下不短路，改用 `Decision.Multi` 聚合全部理由 —— 这正是 `_cluster/allocation/explain` 能给出「被哪几条规则、按什么顺序否决」的机制基础。

源码路径（主仓，approx，无行号）：
- `server/src/main/java/org/opensearch/cluster/ClusterModule.java`（`createAllocationDeciders`）
- `server/src/main/java/org/opensearch/cluster/routing/allocation/decider/AllocationDeciders.java`
- `server/src/main/java/org/opensearch/cluster/routing/allocation/decider/Decision.java`
- `server/src/main/java/org/opensearch/cluster/routing/allocation/decider/DiskThresholdDecider.java`
- `server/src/main/java/org/opensearch/cluster/routing/allocation/decider/ThrottlingAllocationDecider.java`

### 18.5 集群故障检测与 split-brain 防护

两层机制，缺一不可。

**第一层：follower check（活体探测）**。cluster-manager 周期性 ping 每个 follower，并统计连续失败次数：

| 设置 | 默认 | 范围 |
| --- | --- | --- |
| `cluster.fault_detection.follower_check.interval` | **1000 ms** | 100 ms – 150000 ms |
| `cluster.fault_detection.follower_check.timeout` | **10000 ms** | 1 ms – 150000 ms |
| `cluster.fault_detection.follower_check.retry_count` | **3** | ≥ 1 |

判故障的实际判据是「累计失败数 > retry_count」，不是单次超时 —— 即最快也要 `3 × interval ≈ 3 s` 才把一个节点判死，`onNodeFailure` 回调此时才触发。超时是**单次**的，所以最坏 `3 × 10 s = 30 s` 才判死一个网络半通的节点。这套配置是 split-brain 的**时间下界**。

**第二层：PreVote（投票前置）**。`PreVoteCollector` 在真正发起 term 递增前先做一轮非正式询问，避免「刚重启的节点凭陈旧 term 就把现任 leader 顶掉」：

```text
handlePreVoteRequest(request)          // 被动应答
  └─ 读 volatile Tuple<DiscoveryNode, PreVoteResponse> state
       ├─ state 无已知 leader → 拒绝
       └─ 有 leader 且 (term, lastAcceptedTerm, version) 不比请求落后 → 同意

PreVotingRound                            // 主动发起
  ├─ 向 VotingConfiguration 内所有节点并发发 PreVoteRequest
  ├─ handlePreVoteResponse：全部同意 → 正式进入 Election
  └─ 任一拒绝 → 放弃本轮，不递增 term
```

**第三层：Voting 配置排除（人工闸门）**。Voting 配置决定「几票算多数」，是 Raft 式的安全前提；`CoordinationMetadata.VotingConfigExclusion` 把某节点**排除出投票集**但不真正下线，用于「确定要摘掉这台机器但还没停进程」的窗口期。相关逻辑散布在：
- `Coordinator.handleJoinRequest` —— 校验加入请求是否落在被排除节点上；
- `JoinTaskExecutor` —— 新节点以旧 nodeName 加入时，把排除项里的 `MISSING_VALUE_MARKER` 替换成真实 nodeId；
- `ClusterState` —— `voting_config_exclusions` 作为 cluster state 一等字段随发布传播。

源码路径（主仓，approx，无行号）：
- `server/src/main/java/org/opensearch/cluster/coordination/FollowersChecker.java`
- `server/src/main/java/org/opensearch/cluster/coordination/PreVoteCollector.java` / `PreVoteRequest.java` / `PreVoteResponse.java`
- `server/src/main/java/org/opensearch/cluster/coordination/Coordinator.java` / `CoordinationState.java` / `CoordinationMetadata.java`
- `server/src/main/java/org/opensearch/cluster/coordination/JoinTaskExecutor.java` / `PublicationTransportHandler.java`

**复杂度**：`FollowersChecker` 的探测代价是 `O(N_voters / interval)` 次 ping / cluster-manager；PreVote 每轮 `O(N_voters)` 个 `PreVoteRequest`，网络往返 `O(1)` RTT。真正贵的是**判故障期间两个 cluster-manager 可能同时自认 leader** 的窗口，`interval × retry_count` 就是这个窗口的下界，调小它以加快故障转移会直接抬高误判概率。

---

## 十九、副本一致性、恢复两阶段与深分页游标

### 19.0 源码事实口径

同 §18.0：以 **OpenSearch 2.19.0** 主仓为 ground truth，**只给路径、不给精确行号**（approx，行号需校对）。

### 19.1 三个序列号与一次 peer recovery 的骨架

主备一致性的全部秘密在三个数字上：

| 记号 | 含义 | 谁产生 |
| --- | --- | --- |
| `startingSeqNo` | 本次 recovery 起点的 seq_no（source 侧 `shard.countNumberOfHistoryOperations` 的下界） | source |
| `lastKnownGlobalCheckpoint` | source 侧最后已知的 global checkpoint，**用来初始化副本的 translog** | source |
| `trimAboveSeqNo` | 需要在副本上裁掉的 seq_no 上界 | target 侧算：`trimAboveSeqNo = startingSeqNo - 1` |

`LocalStorePeerRecoverySourceHandler` 用一串 `StepListener` 把整个流程串成固定的四步 DAG：

```text
sendFileStep        ← phase 1：文件差分 + 传输 + 建 retention lease + 清理本地多余文件
        │
        ▼
prepareEngineStep   ← target 侧 prepareForTranslogOperations(totalTranslogOps)
        │            预分配 translog 写入缓冲，避免 phase 2 第一条 op 就触发分配抖动
        ▼
sendSnapshotStep    ← phase 2：把 startingSeqNo 之后的 translog 操作流式重放
        │
        ▼
finalizeStep        ← finalizeRecovery(globalCheckpoint = targetLocalCheckpoint,
        │                            trimAboveSeqNo = startingSeqNo - 1)
        ▼
   RecoveryResponse（汇总 phase1 复用/传输的文件数、总字节、phase2 op 数、耗时）
```

`finalizeStepAndCompleteFuture()` 里那句注释点明了 `trimAboveSeqNo` 的**不变式**：

> // Recovery target can trim all operations >= startingSeqNo as we have sent all these operations in the phase 2

即：**phase 2 已经全量发过了，所以 ≥ startingSeqNo 的旧记录可以安全丢弃**。若 phase 2 漏发，`trimAboveSeqNo` 就会裁掉未重放的 op —— 这是该协议唯一的正确性前提。

源码路径（主仓，approx，无行号）：
- `server/src/main/java/org/opensearch/indices/recovery/RecoverySourceHandler.java`
- `server/src/main/java/org/opensearch/indices/recovery/LocalStorePeerRecoverySourceHandler.java`
- `server/src/main/java/org/opensearch/indices/recovery/PeerRecoveryTargetService.java`
- `server/src/main/java/org/opensearch/indices/recovery/RecoveryResponse.java` / `RecoveryState.java`
- `server/src/main/java/org/opensearch/indices/recovery/RecoverySettings.java`

### 19.2 phase 1：文件差分与复用判定

phase 1 的本质是**用校验和做文件级差分**，不是全量重传。`RecoverySourceHandler.phase1()` 的骨架：

```java
// 简化自 server/.../indices/recovery/RecoverySourceHandler.java（2.19.0）
void phase1(IndexCommit snapshot, long startingSeqNo, IntSupplier translogOps, listener, skipCreateRetentionLease) {
    Store.MetadataSnapshot sourceMeta = store.getMetadata(snapshot);

    // 0) 自检：snapshot 里列的每个文件都必须能在 metadata 里找到
    for (String name : snapshot.getFileNames()) {
        if (sourceMeta.get(name) == null) throw new CorruptIndexException(...);
    }

    // 1) 目标节点已有 metadata 与 source 完全一致时，整个 phase 1 可跳过
    if (canSkipPhase1(sourceMeta, request.metadataSnapshot()) == false) {
        // 2) 三路差分：identical（复用）/ different（重传）/ missing（重传）
        Store.RecoveryDiff diff = sourceMeta.recoveryDiff(request.metadataSnapshot());

        for (StoreFileMetadata md : diff.identical) {          // → existingFileNames
            existingTotalSizeInBytes += md.length();
        }
        List<StoreFileMetadata> phase1Files = [diff.different..., diff.missing...];
        for (StoreFileMetadata md : phase1Files) {             // → fileNames
            totalSizeInBytes += md.length();
        }

        // 3) 四步串行：报文件清单 → 传文件 → 建 retention lease → 清理本地多余文件
        recoveryTarget.receiveFileInfo(fileNames, sizes, existingNames, existingSizes, translogOps, l);
        sendFileInfoStep.whenComplete(r -> sendFiles(store, phase1Files, translogOps, sendFilesStep));
        sendFilesStep.whenComplete(r -> createRetentionLease(startingSeqNo, createRetentionLeaseStep));
        createRetentionLeaseStep.whenComplete(lease -> cleanFiles(store, sourceMeta, translogOps,
                                                                lastKnownGlobalCheckpoint, cleanFilesStep));
    }
}
```

**「identical」的判定标准是 size + checksum 双相等**，不是文件名相等。`recoveryDiff` 遍历的是 source 的 metadata map，对每个文件查 target 的 map：

```text
identical  : 本地有 && length 相同 && checksum 相同   → 不传，只记名字和大小
different  : 本地有 && (length 或 checksum 不同)       → 传
missing    : 本地没有                                → 传
```

这一步的**杠杆率极高**：副本每次重建都能复用绝大多数 Lucene segment 文件，实际网络流量只有 `totalSizeInBytes - existingTotalSizeInBytes`。反过来，若 `canSkipPhase1` 成立（两边 metadata 逐字节一致），则连 `receiveFileInfo` 都不发，整个 phase 1 变成 O(1)。

**节流**：`sendFiles` 按 `RecoverySettings.DEFAULT_CHUNK_SIZE = 512 KB - 16` 切块，配合 `indices.recovery.max_bytes_per_sec` / `indices.replication.max_bytes_per_sec` 限速；并发块数由 `indices.recovery.max_concurrent_file_chunks` 控制。

**复杂度**：

| 项 | 复杂度 | 说明 |
| --- | --- | --- |
| metadata 差分 | `O(F)`，F = 文件数 | 典型个位到几十，全是 map 查表 |
| chunk 数 | `⌈B_diff / 512KB⌉` | `B_diff` = different+missing 的总字节 |
| 实际网络量 | `B_diff` | 与 `totalSizeInBytes` 无关，只与差集有关 |
| 耗时统计 | 记 `phase1Time` 与 `phase1ThrottlingWaitTime` | 后者用于判断是否被 `max_bytes_per_sec` 卡住 |

### 19.3 phase 2：translog 重放与 primary context 移交

phase 1 交付的是**某个 Lucene commit 点**的快照，phase 2 补齐从该点到当前的全部增量。骨架：

```java
// 简化自 server/.../indices/recovery/LocalStorePeerRecoverySourceHandler.java（2.19.0）
sendFileStep.whenComplete(r -> {
    prepareTargetForTranslog(countNumberOfHistoryOperations(startingSeqNo), prepareEngineStep);   // 预估 op 总数
});

prepareEngineStep.whenComplete(prepareEngineTime -> {
    assert Transports.assertNotTransportThread("[phase2]");
    Translog.Snapshot phase2Snapshot = shard.newChangesSnapshot(...);   // 冻结增量视图
    phase2(..., phase2Snapshot, ..., sendSnapshotStep);
});

finalizeStepAndCompleteFuture(startingSeqNo, sendSnapshotStep, sendFileStep, prepareEngineStep, onFailure);
```

target 侧接收侧（`RecoveryTarget` 接口）暴露了完整的 4 个动作：

| 接口方法 | 动作 | 关键参数 |
| --- | --- | --- |
| `prepareForTranslogOperations(int totalTranslogOps, …)` | 预分配 | `totalTranslogOps` 是**预估**总数，只用于容量预热 |
| `indexTranslogOperations(List<Translog.Operation>, …)` | 逐批重放 | 另带 `maxSeqNoOfDeletesOrUpdatesOnPrimary`、`primaryTerm`、`mappingVersionOnPrimary` |
| `handoffPrimaryContext(ReplicationTracker.PrimaryContext)` | 移交 primary 上下文 | 把 source 的保留租约 / replication tracker 状态搬到 target |
| `finalizeRecovery(globalCheckpoint, trimAboveSeqNo, …)` | 收尾 | `globalCheckpoint` 取 `r.targetLocalCheckpoint`，**不是** source 的值 |

`finalizeRecovery` 里那句注释解释了一个容易搞错的点：

> Because (in rare corner cases) this commit might not be a safe commit here on the primary,
> and in these cases the max seqno would be too high to be valid as a global checkpoint.

即 **global checkpoint 不能直接抄 source 的 max seq_no** —— 那个 commit 在 source 上未必是 safe commit，抄过去会让副本承认一批并未被多数派确认的 op。

**复杂度**：phase 2 的代价是 `O(O × s)`，O = `startingSeqNo` 之后的 op 条数，s = 单 op 平均字节。本地 store peer recovery 走同一套协议，只是 `translogOps` 从网络流换成本地文件读。

### 19.4 translog checkpoint 与四级保留阶梯

`Checkpoint` 是 translog 目录里的一个小状态文件，字段全是定位量：

```java
// 简化自 server/.../index/translog/Checkpoint.java（2.19.0）
final long offset;                  // writer 内的字节偏移
final int  numOps;                  // 累计 op 数
final long generation;              // 代号
final long minSeqNo, maxSeqNo;      // translog 里所有 op 的 seq_no 下/上界
final long globalCheckpoint;
final long minTranslogGeneration;   // 当前仍需保留的最小代
final long trimmedAboveSeqNo;       // 已被 trim 掉的 seq_no 上界
```

对外只暴露两个「安全下界」：`getMinSeqNo()` 返回 `maxSeqNo`（全部 op 都已持久化，可以整体丢弃的上界），`getMaxSeqNo()` 返回 `min(trimmedAboveSeqNo, maxSeqNo)`。

**保留阶梯**才是「哪些旧 generation 可以删」的裁决者。`DefaultTranslogDeletionPolicy.minTranslogGenRequired()` 把四条独立约束合成一条：

```java
// 简化自 server/.../index/translog/DefaultTranslogDeletionPolicy.java（2.19.0）
long minByLocks     = getMinTranslogGenRequiredByLocks();                 // 引用计数
long minByAge       = getMinTranslogGenByAge(readers, writer, retentionAge, now);
long minBySize      = getMinTranslogGenBySize(readers, writer, retentionSize);
long minByAgeAndSize = (minBySize == Long.MIN_VALUE && minByAge == Long.MIN_VALUE)
                     ? Long.MAX_VALUE                    // 两条都禁用 → 不设下限
                     : Math.max(minByAge, minBySize);   // 两条都启用 → 取更严格者
long minByNumFiles  = getMinTranslogGenByTotalFiles(readers, writer, retentionTotalFiles);
return Math.min(Math.max(minByAgeAndSize, minByNumFiles), minByLocks);
```

写成公式（generation 号**越大越新**，所以「保留下限」取 max = 更新的那个代）：

$$\text{minGen} = \min\Big(\ \max\big(\ \underbrace{\max(g_{\text{age}},\, g_{\text{size}})}_{g_{\text{age}} \wedge g_{\text{size}} \text{ 同时启用}} ,\ \ g_{\text{files}}\ \big),\ \ g_{\text{locks}}\ \Big)$$

读法：

- **age / size 取 max**（更严格的约束胜出，因为它给出一个**更新**的代，必须留更久）；
- **与 files 取 max**（同上，三条资源类约束合并成一个资源下界）；
- **最后与 locks 取 min**（引用计数是**绝对**下界：只要还有 snapshot 或 peer recovery 持锁，该代及其以上全部不可删，锁的值一定 ≤ 资源下界，所以取 min 强制满足）。

三个阶梯各自的扫描方式不同：

| 阶梯 | 遍历方向 | 停止条件 | 复杂度 |
| --- | --- | --- | --- |
| `getMinTranslogGenByAge` | 从最老 reader 往新 | `now - reader.getLastModifiedTime() > retentionAge` | `O(G)` 最坏，`O(1)` 平均 |
| `getMinTranslogGenBySize` | 从最新往最老 | 累加大小 `>= retentionSize` | `O(G)` 最坏，`O(1)` 平均 |
| `getMinTranslogGenByTotalFiles` | 从最新往最老 | `count >= retentionTotalFiles` | `O(retentionTotalFiles)` |
| `getMinTranslogGenRequiredByLocks` | 取所有被锁代号的 min | — | `O(L)`，L = 活跃锁数 |

`acquireTranslogGen(translogGen)` 返回一个 `Releasable`，用引用计数保证「拿到 snapshot 描述那一刻的 translog 代，不会被并发的 trim 掉」。

源码路径（主仓，approx，无行号）：
- `server/src/main/java/org/opensearch/index/translog/Checkpoint.java`
- `server/src/main/java/org/opensearch/index/translog/DefaultTranslogDeletionPolicy.java` / `TranslogDeletionPolicy.java`
- `server/src/main/java/org/opensearch/index/translog/Translog.java` / `TranslogSnapshot.java` / `LocalTranslog.java`

### 19.5 PIT reader context 与 search_after 游标

**问题**：默认 `from + size` 必须 ≤ `index.max_result_window`（默认 **10000**），因为每一页都要把前 N 条重新收集一遍；而 scroll 又要求「一次会话期间视图冻结」，不能用于用户翻页。

OpenSearch 2.x 的 PIT（Point In Time）实现方式**和 ES 不同**：它不是一个跨节点共享的 reader，而是一个**per-shard 的 reader context**，由协调节点汇总。

```java
// 简化自 server/.../search/SearchService.java（2.19.0）
createPitContext(request, listener):
    shard.awaitShardSearchActive(ignored -> {
        // 1) 容量闸门：per-node 的活跃 PIT 数不能超上限
        if (openPitContexts.incrementAndGet() > maxOpenPitContext) {
            throw new OpenSearchRejectedExecutionException("Trying to create too many Point In Time contexts ...");
        }
        // 2) 真正冻结视图：向 shard 要一个 SearcherSupplier（持有 ref + segment 列表快照）
        searcherSupplier = shard.acquireSearcherSupplier();
        // 3) 包成 PitReaderContext，登记到 activeReaders
        readerContext = new PitReaderContext(new ShardSearchContextId(sessionId, idGen++),
                                              indexService, shard, searcherSupplier,
                                              keepAlive.millis(), false);
        putReaderContext(readerContext);
    });
```

相关三个设置（**OpenSearch 明确把 PIT 与 scroll 区分限额**，注释原文：*"This limit is less than scroll since users are encouraged to share the PIT details"*）：

| 设置 | 默认 | 作用域 |
| --- | --- | --- |
| `point_in_time.max_keep_alive` | **24 h** | node，可动态 |
| `search.max_open_pit_context` | **300** | node，可动态（`0` = 不限） |
| `index.max_result_window` | **10000** | index，可动态 |

**游标编码**：`search_after` 的值就是上一页**最后一条命中**的 sort 值数组，由 `SearchAfterBuilder.setSortValues(Object[])` 校验类型：

```java
// 简化自 server/.../search/searchafter/SearchAfterBuilder.java（2.19.0）
if (sortValues[i] == null)                     continue;   // 允许 —— 表达「该段缺失」
if (sortValues[i] instanceof String)           continue;
if (sortValues[i] instanceof Text)             continue;
if (sortValues[i] instanceof Long)             continue;
if (sortValues[i] instanceof Integer)          continue;
if (sortValues[i] instanceof Short)            continue;
if (sortValues[i] instanceof Byte)             continue;
if (sortValues[i] instanceof Double)           continue;
if (sortValues[i] instanceof Float)            continue;
if (sortValues[i] instanceof Boolean)          continue;
if (sortValues[i] instanceof BigInteger)       continue;
throw new IllegalArgumentException("Can't handle search_after field value of type [" + ... + "]");
```

它的**正确性前提**与 `from` 完全不同：`from` 只要求前 `from+size` 条被收集过；`search_after` 要求 **sort 值全局全序且可比较**，所以游标是排序序列的严格前驱，Lucene 侧由 `TopFieldCollector.create(sortAndFormats.sort, numHits, (FieldDoc) searchAfter, hitCountThreshold)` 把它作为初始堆底。

**复杂度对比**（`size` 固定为 `m`，共取第 `i` 页）：

| 方案 | 单页代价 | 取到第 i 页的累计 | 视图一致性 |
| --- | --- | --- | --- |
| `from + size` | `O((im + m) · log(im + m))` | `O(i²m · log(im))` | ❌ 每页重查，可能漏/重 |
| `search_after` | `O(m · log m)` | `O(i · m · log m)` | ❌ 单独用无保证 |
| `search_after` + PIT | `O(m · log m)` | `O(i · m · log m)` | ✅ 视图冻结 |

省下的正是 `im` 那一项 —— `from` 分页的累计代价里 `i²` 这个二次项被去掉了。

源码路径（主仓，approx，无行号）：
- `server/src/main/java/org/opensearch/search/SearchService.java`
- `server/src/main/java/org/opensearch/search/builder/PointInTimeBuilder.java`
- `server/src/main/java/org/opensearch/search/searchafter/SearchAfterBuilder.java`
- `server/src/main/java/org/opensearch/search/query/TopDocsCollectorContext.java`
- `server/src/main/java/org/opensearch/search/DefaultSearchContext.java`

### 19.6 composite 聚合：分桶堆 + after_key 剪枝

`composite` 是唯一支持**无限翻页**的 bucket 聚合，它不返回全局 top-k，而是返回一个 `after_key`，用户把它原样回传即可从断点继续。其收集器的核心是一段只有 30 行的剪枝逻辑：

```java
// 简化自 server/.../search/aggregations/bucket/composite/CompositeValuesCollectorQueue.java（2.19.0）
boolean addIfCompetitive(int indexSortSourcePrefix, long inc) {
    // (a) 候选 key 已经在 top-N 里：直接累加计数，O(1)
    Integer curSlot = getCurrentSlot();
    if (curSlot != null) { docCounts.increment(curSlot, inc); return true; }

    // (b) after_key 剪枝：比上一页断点更小或相等的 key 整段跳过
    if (afterKeyIsSet) {
        int cmp = compareCurrentWithAfter();
        if (cmp <= 0) {
            // 索引序保证后续 key 单调递减 → 可以提前终止整个收集
            if (indexSortSourcePrefix < 0 && cmp == indexSortSourcePrefix) throw new CollectionTerminatedException();
            return false;
        }
    }

    // (c) 堆已满：候选比堆顶还差就丢弃
    if (size() >= maxSize) {
        int cmp = compare(CANDIDATE_SLOT, top());
        if (cmp > 0) {
            // 同上，索引序保证后续更差 → 提前终止
            if (cmp <= indexSortSourcePrefix) throw new CollectionTerminatedException();
            return false;
        }
    }

    // (d) 入堆：复用被弹出的槽位，避免每页重新分配
    final int newSlot;
    if (size() >= maxSize) { int slot = pop(); map.remove(new Slot(slot)); newSlot = slot; }
    else                    { newSlot = size(); }
    copyCurrent(newSlot, inc);
    map.put(new Slot(newSlot), newSlot);
    add(newSlot);
    return true;
}
```

四个要点：

1. **`map` 是「槽位 ↔ 堆下标」的数组级映射**，配合 `Slot` 值对象避免装箱；分支 (a) 的 `getCurrentSlot()` 是一次数组下标查找，而不是哈希查找。
2. **`after_key` 剪枝把翻页代价从「每页全量扫描」降到「每页只扫增量」**。第 i 页只处理断点之后的新 key，不重扫之前的。
3. **`indexSortSourcePrefix` 是可选的加速通道**。只有当索引本身按聚合字段排好序（`index.sort.field` 匹配 sources 的前缀）时，才敢抛 `CollectionTerminatedException` 提前终止；否则 cmp==prefix 只是巧合，不能终止。分支 (b)(c) 各留了这个判据，说明**索引序匹配是精确翻页提速的前提**。
4. **`inc` 是增量计数**而不是每次置 1 —— 同一个 composite key 在一个 segment 里出现多次时只累加一次槽位计数（分支 (a)）。

**复杂度**：

| 项 | 复杂度 |
| --- | --- |
| 单文档处理 | 摊还 `O(1)`（分支 a 命中）或 `O(log k)`（分支 d 一次 `updateTop`） |
| 一页总代价 | `O(N' · log k + k log k)`，N' 为断点之后的新文档数 |
| 未剪枝对照 | `O(N · log k)`，N 为全量 |
| 提前终止 | `O(N')`，若索引序匹配且 key 单调 |

这与 §19.5 的 `search_after` 是同一套「用有序游标换掉重扫」的思想，一个作用在**文档排序**上，一个作用在**分桶键排序**上。

源码路径（主仓，approx，无行号）：
- `server/src/main/java/org/opensearch/search/aggregations/bucket/composite/CompositeValuesCollectorQueue.java`
- `server/src/main/java/org/opensearch/search/aggregations/bucket/composite/CompositeAggregator.java` / `CompositeAggregationFactory.java`
- `server/src/main/java/org/opensearch/search/aggregations/bucket/composite/ParsedComposite.java` / `CompositeKey.java`
- `server/src/main/java/org/opensearch/search/aggregations/bucket/composite/TermsSortedDocsProducer.java`

---

## 二十、k-NN 引擎演进、过滤决策与两阶段重打分

### 20.0 源码事实口径

本节源码位于**独立仓库** `opensearch-project/k-NN`，以实测 tag **`2.19.0.0`**（commit `107c4f1`）为 ground truth。路径均相对该仓根目录。本仓库 `reference/search/opensearch/` 子模块未 init，故**只给路径、不给精确行号**（approx，行号需校对）。

⚠️ §8.3 的表格写的是 `plugins/knn/lib/src/main/java/com/amazon/opensearch/knn/...` 与 `plugins/knn-jni/src/main/java/...faiss...` —— 这是 **1.x 时代**的布局（包名还是 `com.amazon.opensearch`）。2.x 已是独立仓 + `org.opensearch.knn` 包名。下文以实测为准。

### 20.1 三引擎谱系与 2.19 的默认引擎解析

引擎清单本身就记录了迁移史：

```java
// 简化自 src/main/java/org/opensearch/knn/index/engine/KNNEngine.java（2.19.0.0）
public enum KNNEngine implements KNNLibrary {
    @Deprecated(since = "2.19.0", forRemoval = true)
    NMSLIB("nmslib", Nmslib.INSTANCE),       // 1.0 时代的默认，2021→2022 被 Lucene 抢走位置
    FAISS("faiss", Faiss.INSTANCE),          // 2.0 时代接手默认
    LUCENE("lucene", Lucene.INSTANCE);       // 2.4 引入，2.19 起在特定配置下被优先选中

    public static final KNNEngine DEFAULT = FAISS;

    // 会写出「自定义 segment 文件」（.nms / .faiss）的引擎
    private static final Set<KNNEngine> CUSTOM_SEGMENT_FILE_ENGINES = {NMSLIB, FAISS};
    // 支持 knn 查询内联 filter 的引擎
    private static final Set<KNNEngine> ENGINES_SUPPORTING_FILTERS = {LUCENE, FAISS};
    private static final Set<KNNEngine> ENGINES_SUPPORTING_RADIAL_SEARCH = {LUCENE, FAISS};
    private static final Set<KNNEngine> DEPRECATED_ENGINES = {NMSLIB};
}
```

`CUSTOM_SEGMENT_FILE_ENGINES` 揭示了根本差异：**Faiss / NMSLIB 把原生索引写进 Lucene segment 目录作为自定义文件（`.faiss` / `.nms`），靠 JNI 读；Lucene 引擎则完全不动 Lucene 存储结构，走 Lucene 自带的 `KnnVectorsFormat`**。代价是 Lucene 引擎拿不到 Faiss 的 PQ / SQ 量化器与 IVF 聚类。

三个引擎的**能力矩阵**：

| 维度 | NMSLIB | Faiss | Lucene |
| --- | --- | --- | --- |
| 图结构 | HNSW | HNSW + IVF | 仅 HNSW |
| 自定义 segment 文件 | ✅ `.nms` | ✅ `.faiss` | ❌ |
| 查询内联 filter | ❌ | ✅ | ✅ |
| radial search（半径查询） | ❌ | ✅ | ✅ |
| 向量量化 | ❌ | ✅ SQ / PQ / xN | ✅ SQ（Lucene 侧） |
| 向量训练 | ❌ | ✅（唯一支持） | ❌ |
| 状态（2.19） | **@Deprecated forRemoval** | 活跃 | 活跃 |

**默认引擎解析**（`EngineResolver.getEngine(...)`）是一条有明确优先级的判定树：

```java
// 简化自 src/main/java/org/opensearch/knn/index/engine/EngineResolver.java（2.19.0.0）
KNNEngine getEngine(cfgCtx, methodCtx, requiresTraining, version) {
    // 1) 用户显式指定 → 直接采纳（不再做任何推断）
    if (hasUserConfiguredEngine(methodCtx)) return methodCtx.getKnnEngine();

    // 2) 需要训练 → Faiss 是唯一支持训练的引擎
    if (requiresTraining) return KNNEngine.FAISS;

    Mode mode = cfgCtx.getMode();  CompressionLevel level = cfgCtx.getCompressionLevel();

    // 3) mode 与 compression_level 都没配 → 全局默认
    if (!Mode.isConfigured(mode) && !CompressionLevel.isConfigured(level)) return KNNEngine.DEFAULT;   // FAISS

    // 4) x4 压缩只有 Lucene 能做
    if (level == CompressionLevel.x4) return KNNEngine.LUCENE;

    // 5) x1 / 未配压缩 → 交给版本相关的判定
    if (!CompressionLevel.isConfigured(level) || level == CompressionLevel.x1) return resolveEngineForX1OrNoCompression(mode, version);

    // 6) 其余压缩等级（x2/x8/x16/x32/x64）→ Faiss
    return KNNEngine.FAISS;
}

private KNNEngine resolveEngineForX1OrNoCompression(Mode mode, Version version) {
    if (version != null && version.onOrAfter(Version.V_2_19_0)) return KNNEngine.FAISS;   // 2.19 起不再回落 nmslib
    return mode == Mode.ON_DISK ? KNNEngine.FAISS : KNNEngine.NMSLIB;                     // 2.19 之前的老逻辑
}
```

**这条判定树就是 nmslib → lucene → faiss 迁移的完整证据链**：老索引（未显式指定 engine、没配 mode/compression）在 2.19 之前解析到 NMSLIB，2.19 起强制解析到 FAISS；同时 `logAndReturnEngine()` 对 `DEPRECATED_ENGINES` 打 warn 日志。也就是说**升级到 2.19 会静默改变未显式声明 engine 的字段的底层引擎**，这是一次真实的迁移陷阱。

源码路径（k-NN 仓，approx，无行号）：
- `src/main/java/org/opensearch/knn/index/engine/KNNEngine.java`
- `src/main/java/org/opensearch/knn/index/engine/EngineResolver.java`
- `src/main/java/org/opensearch/knn/index/engine/MethodResolver.java` / `AbstractMethodResolver.java`
- `src/main/java/org/opensearch/knn/index/mapper/Mode.java` / `CompressionLevel.java`

### 20.2 HNSW 参数映射与默认值

三个引擎共享同一套 mapping 层的参数名（`KNNConstants`），但**支持范围不同**：

| mapping 参数 | 含义 | Lucene | Faiss | NMSLIB |
| --- | --- | --- | --- | --- |
| `algo_param.m` | HNSW 每节点最大出边数（构图宽度） | ✅ | ✅ | ✅ |
| `algo_param.ef_construction` | 建图期候选队列长度 | ✅ | ✅ | ✅ |
| `algo_param.ef_search` | 查询期候选队列长度 | ✅ | ✅ | ❌ |
| `nlist` / `nprobes` | IVF 聚类数 / 探测簇数 | ❌ | ✅ | ❌ |

集群级默认值（`KNNSettings`，**索引 mapping 未指定时的兜底**）：

| 设置 | 默认 | 说明 |
| --- | --- | --- |
| `index.knn.algo_param.m` | **16** | 三引擎共同默认 |
| `index.knn.algo_param.ef_construction` | **100** | 只影响建图期 |
| `index.knn.algo_param.ef_search` | **100** | 只影响查询期 |
| `index.knn.space_type` | **`l2`** | 二值向量字段另有默认 **`hamming`** |
| `index.knn.advanced.filtered_exact_search_threshold` | **-1（关闭）** | Faiss 专用，见 §20.4 |
| `knn.circuit_breaker.unset.percentage` | **75** | 原生内存熔断水位（heap 百分比） |
| `knn.memory.circuit_breaker.limit` | **`50%`** | 原生内存熔断上限 |

`NMSLIB` 不支持 `ef_search` 不是遗漏：`NmslibHNSWMethod` 的参数列表里只注册了 `m` 与 `ef_construction` 两项 —— NMSLIB 的查询候选队列在 JNI 层固定，不能从 mapping 调。迁移到 Faiss/Lucene 后 `ef_search` 才成为可调旋钮。

**调参方向**：

| 参数 | 调大的效果 | 调大的代价 |
| --- | --- | --- |
| `m` | 图连通性↑、召回↑ | 内存 `O(N·m·4B)`、建图 `O(log N·m·ef_construction)` |
| `ef_construction` | 图质量↑ | 只在**建索引**时付出 |
| `ef_search` | 召回↑ | 只在**查询**时付出，与 `k` 无关 |

`ef_search` 是**唯一不影响索引大小、只影响查询延迟**的旋钮，因此是灰度调参的首选；而 `m` 与 `ef_construction` 一旦改就要全量重建（reindex）。

### 20.3 距离度量与 Lucene 打分域换算

原生库返回的是**距离**，Lucene 需要的是**分值**（越大越相似，且落在 `[0, Float.MAX_VALUE]`）。换算在 `SpaceType.scoreTranslation(float rawScore)` 里逐 space 实现：

| space | 原生返回 `raw` | Lucene 分值 `score` |
| --- | --- | --- |
| `l2` / `l1` / `linf` / `hamming` | 距离 `d ≥ 0` | $\text{score} = \dfrac{1}{1 + d}$ |
| `cosinesimil` | $1 - \cos(x,y) \in [0, 2]$ | $\text{score} = \max\Big(\dfrac{2 - \text{raw}}{2},\ 0\Big)$ |
| `innerproduct` | 点积，可正可负 | $\text{score} = \begin{cases} \dfrac{1}{1+\text{raw}}, & \text{raw} \ge 0 \\[6pt] -\text{raw} + 1, & \text{raw} < 0 \end{cases}$ |

`l2` 还提供反变换 `scoreToDistanceTranslation(score) = 1/score - 1`（`score == 0` 直接抛 `IllegalArgumentException`，因为距离无穷大无法反解）。

`innerproduct` 的分段函数值得单独说：点积在 `[-Float.MAX_VALUE, +Float.MAX_VALUE]` 上，且**负点积表示更相似**（归一化向量下点积越大越近，负值说明方向相反但取绝对值后更接近 1 —— 这是 MIPS 的约定）。所以负半轴用线性映射 `-raw + 1` 把「越大越相似」翻正，正半轴仍用 `1/(1+raw)` 与其它距离度量保持同一形式。

源码路径（k-NN 仓，approx，无行号）：
- `src/main/java/org/opensearch/knn/index/SpaceType.java`
- `src/main/java/org/opensearch/knn/index/KNNSettings.java`
- `src/main/java/org/opensearch/knn/common/KNNConstants.java`
- `src/main/java/org/opensearch/knn/index/engine/lucene/LuceneHNSWMethod.java`
- `src/main/java/org/opensearch/knn/index/engine/faiss/FaissHNSWMethod.java`
- `src/main/java/org/opensearch/knn/index/engine/nmslib/NmslibHNSWMethod.java`

### 20.4 过滤决策：何时从近似退化为精确

k-NN 查询里内联一个 `filter` 会带来经典矛盾：**后置过滤**可能返回不足 `k` 条，**前置过滤**则可能让过滤后的候选集小到近似索引失去意义。OpenSearch 的解法是**按过滤后候选数 `P` 与 `k` 的关系自动切换**。

先看精确搜索器内部的分叉（`ExactSearcher.searchLeaf`）：

```java
// 简化自 src/main/java/org/opensearch/knn/index/query/ExactSearcher.java（2.19.0.0）
Map<Integer, Float> searchLeaf(LeafReaderContext ctx, ExactSearcherContext c) {
    KNNIterator it = getKNNIterator(ctx, c);
    if (it == null) return emptyMap();

    // (1) 半径查询 → 走 distance→score 换算后按 minScore 过滤（仅 Faiss 支持）
    if (c.getKnnQuery().getRadius() != null) return doRadialSearch(ctx, c, it);

    // (2) 过滤后的候选数 P ≤ k → 直接全打分，ANN 完全没必要
    if (c.getMatchedDocsIterator() != null && c.numberOfMatchedDocs <= c.getK())
        return scoreAllDocs(it);                       // 遍历 P 条，O(P)

    // (3) 否则走 top-k 小顶堆
    return searchTopCandidates(it, c.getK(), Predicates.alwaysTrue());   // O(P log k)
}
```

分支 (2) 就是「**P ≤ k → 精确前置过滤**」的源码形态：既然要打分的文档数不超过要返回的条数，用近似图索引去召回本身就是净亏损。分支 (3) 是「P > k → 用堆取 top-k」，此处 Lucene 引擎走 Lucene 自己的 `KnnVectorQuery` 过滤策略（近似 + 修改后的后置过滤），Faiss 引擎走 JNI 的原生过滤。

再看近似搜索之后**是否补一次精确搜索**的判定（`KNNWeight`）：

```java
// 简化自 src/main/java/org/opensearch/knn/index/query/KNNWeight.java（2.19.0.0）
private boolean isExactSearchRequire(LeafReaderContext ctx, int filterIdsCount, int annResultCount) {
    // 触发条件 A：segment 里根本没有原生引擎文件（Lucene 引擎索引 / 旧段）→ 必须精确
    if (annResultCount == 0 && isMissingNativeEngineFiles(ctx)) return true;
    // 触发条件 B：过滤 id 数够 k，但 ANN 没能凑满 k 条 → 兜底精确
    if (isFilteredExactSearchRequireAfterANNSearch(filterIdsCount, annResultCount)) return true;
    return false;
}

private boolean isFilteredExactSearchRequireAfterANNSearch(int filterIdsCount, int annResultCount) {
    return filterWeight != null && filterIdsCount >= knnQuery.getK() && knnQuery.getK() > annResultCount;
}
```

条件 B 是一条纯算术不变式：**候选池 `P ≥ k` 却只召回 `A < k` 条，说明过滤把 ANN 的召回打穿了**，必须回退。另外还有第三个开关 `isExactSearchThresholdSettingSet(v)` = `v != -1`，即 `index.knn.advanced.filtered_exact_search_threshold` **默认关闭**（`-1`）；显式设置后可以为 Faiss 限定「过滤结果集不超过该阈值才考虑精确搜索」。

`isMissingNativeEngineFiles` 的判定很朴素：`FieldInfoExtractor.extractKNNEngine(fieldInfo)` 拿到引擎后，用 `KNNCodecUtil.getEngineFiles(engine.getExtension(), field, segmentInfo.info)` 看 segment 目录里有没有对应扩展名的文件；空则说明这段数据没有原生索引（例如 Lucene 引擎写的段）。

**复杂度**：

| 策略 | 触发条件 | 代价 |
| --- | --- | --- |
| 精确全打分（分支 2） | `P ≤ k` | `O(P·d)`，无索引开销 |
| 堆取 top-k（分支 3） | `P > k` | `O(P · log k · d)` |
| ANN + 兜底精确 | 条件 A 或 B | `O(log N · ef_search · d + k·d)` |
| 后置过滤（可能不足 k 条） | Lucene 近似路径 | `O(log N · ef_search · d)`，召回率无保证 |

### 20.5 两阶段：ANN 召回 + 全精度重打分

`NativeEngineKnnVectorQuery` 的执行是一个**两阶段流水线**，对应「量化换内存 / 压缩换 IO / 全精度换召回」这条反复出现的工程权衡：

```java
// 简化自 src/main/java/org/opensearch/knn/index/query/nativelib/NativeEngineKnnVectorQuery.java（2.19.0.0）
// ---- 阶段一：ANN 召回（可用量化 / 磁盘向量）----
List<PerLeafResult> doSearch(IndexSearcher s, List<LeafReaderContext> leaves, KNNWeight w, int k) {
    // 每个 leaf 一个 Callable，并发执行；结果 = Map<docId, score> + filterBits
    List<Callable<PerLeafResult>> tasks = leaves.stream().map(lc -> () -> searchLeaf(lc, w, k)).toList();
    return s.getTaskExecutor().invokeAll(tasks);
}

// ---- 阶段二：全精度重打分 ----
List<PerLeafResult> retrieveAll(IndexSearcher s, leaves, w, perLeafResults, useQuantized) {
    for (每个 leaf i) {
        PerLeafResult r = perLeafResults.get(i);
        // ① 沿 parent/child 关系把命中的 docId 展开成「所有兄弟文档」
        DocIdSetIterator allSiblings = queryUtils.getAllSiblings(
                leafReaderContext, r.getResult().keySet(), w.getParentsFilter(), r.getFilterBits());

        // ② 组装精确搜索上下文：k = 兄弟文档总数；关键：关闭量化向量
        ExactSearcherContext ctx = ExactSearcherContext.builder()
                .matchedDocsIterator(allSiblings)
                .numberOfMatchedDocs(allSiblings.cost())
                .useQuantizedVectorsForSearch(false)   // 注释原文：rescoring 要用全精度
                .k((int) allSiblings.cost())
                .isParentHits(false)
                .knnQuery(knnQuery)
                .build();

        // ③ 用全精度向量重算距离，替换掉阶段一的近似分
        r.setResult(w.exactSearch(leafReaderContext, ctx));
    }
}
```

四个设计点：

1. **`useQuantizedVectorsForSearch(false)` 是整个流程的语义核心**：阶段一用量化/disk 向量召回（快、召回偏低），阶段二用全精度向量重算（慢、准确）。两阶段的分值**不可直接比较**，所以必须重算而非加权。
2. **`getAllSiblings` 展开 nested 文档**。命中的是 nested 子文档时，返回给用户的往往是父文档或全部兄弟；展开后 `k = allSiblings.cost()`，即**实际参与重打分的文档数会超过原始 `k`**。`getTotalTopDoc()` 在 `expandNestedDocs` 为真时改为累加 `topDoc.scoreDocs.length`，而不是直接返回 `knnQuery.getK()`。
3. **`filterBits` 一路透传**。`PerLeafResult` 的构造里有一句兜底：`this.filterBits = filterBits == null ? new Bits.MatchAllBits(0) : filterBits;` —— 即无过滤时也要给下游一个合法的 `Bits`，避免阶段二里做两遍 null 判断。
4. **`searchTopCandidates` 用小顶堆且初始为 `-INF`**：源码里有一行专门的清理 —— `while (queue.size() > 0 && queue.top().score < 0) queue.pop();`，注释说明「堆用 `-INF` 初始化，当 `filterIds < k` 时堆里会残留负分条目」。这是 §20.4 分支 (2)/(3) 判定逻辑在堆里的直接体现。

**复杂度**：

| 阶段 | 时间 | 空间 |
| --- | --- | --- |
| ANN 召回 | `O(log N · ef_search · d)` | `O(N · d_bytes)` 索引常驻 |
| 展开兄弟 | `O(k · fanout)` | `O(k · fanout)` docId 集 |
| 全精度重打分 | `O(k' · d)`（k' = 展开后文档数） | `O(k')` |
| 合计 | `O(log N · ef_search · d + k' · d)` | 常驻索引 + `O(k')` |

调大 `ef_search` 只抬第一项，调 `k` 只抬第二项 —— 这解释了为什么「召回不满 k」时正确的解法是调 `ef_search` 或退回精确，而不是调 `k`。

源码路径（k-NN 仓，approx，无行号）：
- `src/main/java/org/opensearch/knn/index/query/nativelib/NativeEngineKnnVectorQuery.java`
- `src/main/java/org/opensearch/knn/index/query/lucene/LuceneEngineKnnVectorQuery.java`
- `src/main/java/org/opensearch/knn/index/query/ExactSearcher.java`
- `src/main/java/org/opensearch/knn/index/query/KNNWeight.java`
- `src/main/java/org/opensearch/knn/index/query/PerLeafResult.java`
- `src/main/java/org/opensearch/knn/index/query/KNNQuery.java` / `KNNQueryBuilder.java`
- `src/main/java/org/opensearch/knn/index/query/iterators/VectorIdsKNNIterator.java` / `NestedVectorIdsKNNIterator.java`
- `src/main/java/org/opensearch/knn/index/codec/util/KNNCodecUtil.java`
- `src/main/java/org/opensearch/knn/common/FieldInfoExtractor.java`

### 20.6 mode / compression_level 的组合约束

`Mode`（`IN_MEMORY` 默认 / `ON_DISK`）与 `CompressionLevel`（`x1` … `x64`）不是自由组合，两者由**压缩级别本身携带的合法 Mode 集合**约束：

```java
// 简化自 src/main/java/org/opensearch/knn/index/mapper/CompressionLevel.java（2.19.0.0）
NOT_CONFIGURED(-1, "", null,                    {}),
x1 ( 1, "1x",  null,                             {}),   // 无量化，等价于不配
x2 ( 2, "2x",  null,                             {}),   // 无量化
x4 ( 4, "4x",  null,                             {}),   // 只能 Lucene
x8 ( 8, "8x",  new RescoreContext(2.0f, false, false), {Mode.ON_DISK}),
x16(16, "16x", new RescoreContext(3.0f, false, false), {Mode.ON_DISK}),
x32(32, "32x", new RescoreContext(3.0f, false, false), {Mode.ON_DISK}),
x64(64, "64x", new RescoreContext(5.0f, false, false), {Mode.ON_DISK});
```

- **x1–x4 没有 `RescoreContext`**：这四级是 Lucene `ScalarQuantizer` 的原生档位，不需要额外重打分配置。
- **x8 及以上强制 `Mode.ON_DISK`**：64× 压缩意味着单个 float 只剩 0.5 bit，内存里放不下，只能落盘 —— 这直接触发 §20.5 的两阶段路径（disk 向量召回 + 全精度重打分）。
- **`RescoreContext(oversampleFactor, …)` 是重打分候选放大系数**。x8 用 2.0、x16/x32 用 3.0、x64 用 5.0 —— 压缩越狠、量化误差越大，就要把 ANN 阶段的候选集放大越多，才能保证重打分后 top-k 仍然正确。这条经验规律在源码里被硬编码成了一串常量。
- **`x1` + `ON_DISK` 是非法组合**，`AbstractMethodResolver` 会直接抛：

```java
if (knnMethodConfigContext.getCompressionLevel() == CompressionLevel.x1
        && knnMethodConfigContext.getMode() == Mode.ON_DISK) {
    throw new IllegalArgumentException("Cannot specify \"x1\" compression level when using \"on_disk\" mode");
}
```

**选型速查表**：

| 场景 | `mode` | `compression_level` | 隐含 engine | 理由 |
| --- | --- | --- | --- | --- |
| 小规模、要求最高精度 | `in_memory` | 不配 / `x1` | Faiss（2.19+） | 全精度，ANN 直接可用 |
| 大规模、内存受限 | `on_disk` | `x2` / `x4` | Faiss / Lucene | 2–4× 压缩，仍有精度余量 |
| 超大规模、可接受召回损失 | `on_disk` | `x16` / `x32` | Faiss | 强制两阶段，靠 `RescoreContext` 补精度 |
| 极端压缩、成本优先 | `on_disk` | `x64` | Faiss | 0.5 bit/维，重打分放大 5× |
| 需要训练 / PQ / IVF | 不限 | 不限 | **必须 Faiss** | 唯一支持训练的引擎 |
| 无 filter 的纯检索 | 不限 | 不限 | Lucene | 不写自定义 segment，存储最省 |

**复杂度总表**：

| 项 | 空间 | 建索引时间 | 查询时间 |
| --- | --- | --- | --- |
| x1 HNSW | $O(N \cdot m \cdot 4B)$ | `O(N·log N·m·ef_construction·d)` | `O(log N·ef_search·d)` |
| x16 HNSW + 重打分 | `O(N·m·2B + N·d/16)` | 同上 + 训练 | `O(log N·ef_search·d + 3k'·d)` |
| IVF（Faiss） | `O(N·d)` + 倒排表 | `O(N·d)` 聚类 + 分配 | `O(nprobes · (N/nlist) · d)` |
| 精确全打分 | `O(N·d)` | `O(N·d)` | `O(N·d)` |

---

## 二十一、参考资料

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
