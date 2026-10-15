# Elasticsearch Wiki（搜索与分析引擎事实标准）

> 参考版本：Elasticsearch 8.x（Lucene 9.x 内核），Java 实现，基于 Apache Lucene 构建分布式 RESTful 搜索与分析引擎。本 wiki 重点覆盖搜索类型特有章节（倒排索引、DSL、聚合、相关性排序）。

## 一、项目定位

Elasticsearch（简称 ES）是由 **Elastic N.V.**（原 Shay Banon / Compass 公司）开源的 **分布式搜索与分析引擎**，基于 **Apache Lucene** 内核构建对外提供 **RESTful JSON API**。它把 **倒排索引（Inverted Index）、近实时搜索（Near-Real-Time, NRT）、水平扩展分片（Shard）、副本（Replica）、聚合框架（Aggregation）** 全部封装为一套引擎，并且和 **Logstash（采集）、Kibana（可视化）、Beats（轻采集）** 共同组成 **ELK/Elastic Stack**。

- **痛点**：
  - 关系数据库（MySQL/PG）的 `LIKE '%foo%'` 无法使用索引，全文检索性能坍塌；
  - 通用 KV（Redis）缺少相关性排序、聚合、分词；
  - 传统搜索服务（Solr）配置繁琐、集群管理薄弱、缺乏近实时刷新；
  - 大数据方案（Hive）实时性不足，亚秒级响应不可达；
  - 需要 **应用日志（APM）、安全告警（SIEM）、企业搜索（Workplace Search）、向量检索（kNN）** 一站式引擎；
- **典型用户**：运维与日志平台（ELK Stack）、电商商品搜索、SaaS 多租户、SIEM 安全厂商、车联网时序检索、AI/RAG（向量+BM25 混合检索）；
- **核心口号**：**"Elasticsearch — the heart of the Elastic Stack"** + **"Search. Observe. Protect."** + **"You Know, for Search"**（衍生）。

## 二、发展历史与社区

- **起源**：2010 年 Shay Banon 出于对妻子的烹饪书检索需求，作为 **Compass** 项目的下一代发布，2010-05 首发，**1.0** 2014-02；
- **里程碑**：
  - 2010-05：0.4 首发；
  - 2014-02：1.0 GA；
  - 2015-03：2.0 引入 `doc_values`、`update_by_query`、`cluster.routing` 改进；
  - 2017-11：6.0 引入 **doc_values 的 ordinals 索引**、索引排序（Index Sorting）、`shrink / split` API；
  - 2019-04：7.0 默认分片 1 个、移除 `mapping types`、内置 **Elastic License（SSPL 类似）** + **JDK 11**；
  - 2020-07：7.8 引入 **Searchable Snapshots**、**Frozen Tier**（对象存储降本）；
  - 2021-02：7.11 **Open Source** 部分（按 **SSPL + Elastic License v2** 双协议）；
  - 2022-02：8.0 引入 **Elasticsearch Security 默认免费**、**Native Searchable Snapshots**、**T-Digest** 等内置；
  - 2022-10：8.5 起 **Elasticsearch 作为向量数据库** 全面发力（`dense_vector` + `knn_search`、HNSW、`semantic_text`）；
  - 2024-2026：8.x 持续强化 **ELSER（Elastic Learned Sparse Encoder）、Hybrid Retrieval、ES|QL**（类 SQL DSL）、**Cohere / OpenAI / HuggingFace 嵌入集成**、**RAG / Inference API**；
- **维护状态**：极活跃，Elastic N.V. 主导，月度小版本 + 季度大版本；AWS 推出 **OpenSearch** 分支（2021）后 ES 主线移除 Apache 2.0；
- **社区**：ElasticOn / Elastic{ON} 年度大会、官方培训认证（Elastic Certified Engineer）、Forum、Slack、Reddit r/elasticsearch、Stack Overflow 高活跃。

## 三、许可与商业限制

- **Elasticsearch 7.10 之前**：**Apache License 2.0**（部分 x-pack 功能需付费）；
- **Elasticsearch 7.11+ / 8.x**：**Server Side Public License (SSPL) + Elastic License v2.0** 双协议。**SSPL 是 copyleft**：所有连接 ES 的客户端必须开源；**Elastic License v2.0** 禁止提供托管服务。**核心搜索引擎能力仍是开源**（Apache 2.0 仅给 ELasticsearch v7.10 及之前的最后 Apache 版本）；
- **商业版本**：**Elastic Cloud**（AWS / GCP / Azure 多区域）、**Platinum / Enterprise**（含 ML、Security、Alerting、Canvas、Graph 等高级功能）、**ECE（Elastic Cloud Enterprise）** 自管；
- **生态关系**：
  - 与 **Kibana**（同公司，Visualize / Dashboard）紧耦合；
  - 与 **Logstash**（同公司，ETL 采集）通过 Beats 协议通信；
  - 与 **Beats**（同公司，Filebeat/Metricbeat/Heartbeat 等 9+ 采集器）打通；
  - 与 **AWS OpenSearch**（2021 分支，对应原 ES）构成 **分歧**：AWS 维护 OpenSearch 主线，Elastic 转 SSPL 商业化；
  - 与 **HuggingFace / Cohere / OpenAI** 通过 Inference API 集成嵌入。

## 四、业界主要使用方与案例

- **超大型用户**（公开）：
  - **Wikipedia**：站内全文检索；
  - **GitHub**：代码搜索（早期）；
  - **Stack Overflow**：标签 + 全文检索；
  - **Netflix / Uber / Yelp**：日志、APM、商品/餐厅搜索；
  - **eBay**：商品搜索基础设施；
  - **Sony**：游戏与设备遥测；
  - **Cisco**：Webex 搜索；
  - **国内**：阿里云 / 腾讯云 / 华为云 / 百度云均提供 **Elasticsearch Service** 托管；
- **行业**：电商搜索、运维日志、APM、SIEM 安全、企业全文检索（Confluence-like）、AI/RAG 检索；
- **典型场景**：AIOps（异常检测）、电商商品搜索 + 个性化推荐、AI 客服知识库、SIEM 告警、IoT 设备遥测检索。

## 五、一句话对比定位

> Elasticsearch 是 **"基于 Lucene 的分布式搜索与分析引擎 + ELK 生态 + 近实时 NRT + 分片副本 + DSL 查询 + 聚合框架 + 向量检索(kNN)"** 的一站式搜索引擎；与 **OpenSearch（AWS 分支）**、**Apache Solr（Lucene 直系）**、**Meilisearch（Rust 极简）**、**Typesense（C++ 即时）**、**Tantivy（Rust 嵌入式）**、**Quickwit（Rust 云原生对象存储）**、**vespa（Yahoo/NTT，搜索 + ML 推理）** 构成搜索引擎阵营。ES 的差异化是 **"完整 ELK 生态 + 强大聚合 + 向量检索 + 商业化平台"**。

## 六、整体架构

```
┌────────────────────────────────────────────────────────────────────────────┐
│                  客户端层                                                  │
│  - HTTP REST（9200，默认）/ HTTPS                                          │
│  - 官方客户端：Elasticsearch Java / Python / Go / .NET / Ruby / PHP / JS   │
│  - Kibana / Logstash / Beats / Cerebro / elasticdump                       │
│  - JDBC / ODBC（商业 x-pack）                                              │
└─────────────────────┬──────────────────────────────────────────────────────┘
                      │ HTTP/JSON
┌─────────────────────▼──────────────────────────────────────────────────────┐
│                  协调节点（Coordinating Node）                              │
│  server/src/main/java/org/elasticsearch/action/                            │
│  ├─ search/TransportSearchAction：search 请求入口                          │
│  ├─ bulk/TransportBulkAction：bulk/index/update/delete                     │
│  ├─ admin/cluster/：集群管理（Reroute / Allocation）                        │
│  └─ ActionModule / ActionPlugin：注册所有 Action                          │
└─────────┬───────────────────┬───────────────────────┬─────────────────────┘
          │ query            │ query                │ shard fan-out
┌─────────▼───────┐ ┌────────▼──────────┐ ┌────────▼──────────┐
│  Master 节点   │ │  Coordinating     │ │  Coordinating     │
│  - Zen2 / GCA │ │  + Data 节点       │ │  + Data 节点       │
│  - cluster/   │ │  - 持有主/副分片   │ │  - 持有其他分片   │
│    service/  │ │                    │ │                    │
└──────┬───────┘ └────────┬───────────┘ └────────┬───────────┘
       │                  │ shard-level query    │
       │                  ▼                      ▼
       │         ┌────────────────────────────────────┐
       │         │   Lucene Index（per Shard）        │
       │         │   server/src/main/java/org/       │
       │         │   elasticsearch/index/engine/     │
       │         │   ├─ InternalEngine              │
       │         │   │  ├─ IndexWriter（Lucene）    │
       │         │   │  ├─ Searcher（near-real-time）│
       │         │   │  └─ MergeScheduler           │
       │         │   └─ ReadOnlyEngine              │
       │         │                                  │
       │         │   Lucene 核心（vendored）：       │
       │         │   lucene/core/src/java/org/      │
       │         │   apache/lucene/                 │
       │         │   ├─ index/（倒排、doc_values） │
       │         │   ├─ search/（Scorer / Collector）│
       │         │   ├─ codec/（压缩 / 编码格式）   │
       │         │   └─ analysis/（分词器）         │
       │         └────────────────────────────────────┘
       │
┌──────▼───────────────────────────────────────────────────────────────┐
│  Cluster 状态 / Discovery：                                            │
│  - cluster/routing/AllocationService                                  │
│  - discovery/SeedHostsProvider / GossipedSeed                        │
│  - gateway/GatewayMetaState（持久化集群元数据）                        │
└──────────────────────────────────────────────────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────────────────────────┐
│  Indices / Shard 状态（IndexShard + IndicesStore）                  │
│  - Routing / Allocation / Reroute                                    │
│  - Snapshot/Restore（Searchable Snapshots）                          │
└──────────────────────┬──────────────────────────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────────────────────────┐
│  x-pack/ 商业 + 安全 + ML + Watcher + SQL + ES|QL                    │
│  - x-pack/plugin/src/main/java/org/elasticsearch/xpack/             │
└──────────────────────┬──────────────────────────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────────────────────────┐
│  本地文件系统（per Data Node）                                        │
│  /var/lib/elasticsearch/nodes/0/indices/...                          │
│   ├─ segments_N（segments 元数据）                                    │
│   ├─ index/（Lucene 索引目录：segments, .tim, .doc, .pos, .nvd...）  │
│   ├─ translog/（写入前日志）                                          │
│   ├─ _state/（index metadata.json）                                  │
│   └─ nodes/0/_state/global*.st（cluster state 持久化）               │
└─────────────────────────────────────────────────────────────────────┘
```

### 关键设计取舍

- **协调节点 / 数据节点 / Master 节点 分离**：请求先到 Coordinating，由 Master 维护 cluster state（zen2/GCA discovery），数据按主/副分片 fan-out 到数据节点；数据节点承载 Lucene shard 的 index + search；
- **分片 + 副本（Shard + Replica）**：每个索引被切分为 N 个 Primary Shard，每个 Primary 有 R 个副本。Primary 负责写入、副本负责读和 HA；Lucene IndexWriter 实现 segment 写入；
- **近实时 NRT**：写入先到 **translog + in-memory buffer**，默认每 1s 调用 `IndexWriter.commit`（refresh）使 doc 可见；segment 不可变，后台 MergeScheduler 合并；
- **聚合在搜索时执行**：Aggregation Framework 与 Query 并列执行（bucket + metric + pipeline），复用 Lucene 的 **DocValues** 列存；
- **x-pack 模块化商业功能**：安全、ML、Watcher、SQL、ES|QL、向量检索等通过 plugin 注入，单一 JAR 启动；
- **Cluster State 集中 + 持久化**：cluster state 序列化到 `global*.st`，Master 单点写入后 gossip 同步。
- **Elastic License 与 Apache OpenSearch 分叉**：SSPL 出现后，AWS 维护 OpenSearch 主线，Elastic 主线强化商业（Elastic Cloud / Inference API / ELSER 等）。

## 七、存储引擎（Lucene 索引格式）

### 7.1 倒排索引（Inverted Index，**搜索类型核心**）

Elasticsearch 的底层索引基于 **Apache Lucene**，每一行文档（document）写入时被分析为 **term + positions + payloads**：

```
Field "title" → 分词（Analyzer） → terms: ["快速", "入门", "Elasticsearch"]
                     ↓
            Posting Lists（倒排表，按 docId 升序）

term "快速"     → [docId=3, pos=0], [docId=12, pos=2], [docId=45, pos=5]   (posting list)
term "入门"     → [docId=3, pos=1], [docId=12, pos=3]
term "elasticsearch" → [docId=3, pos=2], [docId=45, pos=0]

Term Dictionary（term → postings）：
  "elasticsearch" → [posting list offset, docFreq=2, ...]
  "入门"            → [posting list offset, docFreq=2, ...]
  "快速"            → [posting list offset, docFreq=3, ...]
```

源码位置（Lucene 项目内）：
- `lucene/core/src/java/org/apache/lucene/index/IndexWriter.java`：写入；
- `lucene/core/src/java/org/apache/lucene/codecs/lucene90/Lucene90PostingsFormat.java`：posting list 编码（FST + skip list + block-based compression）；
- `lucene/core/src/java/org/apache/lucene/codecs/lucene90/Lucene90DocValuesFormat.java`：doc_values 列存。

ES 包装层：
- `server/src/main/java/org/elasticsearch/index/mapper/FieldMapper.java`：每个字段映射到 Lucene FieldType；
- `server/src/main/java/org/elasticsearch/index/engine/InternalEngine.java`：ES 写引擎包装 Lucene IndexWriter。

### 7.2 主要字段类型与 Posting List 编码

- **`text` / `keyword` / `numeric` / `date` / `dense_vector` / `sparse_vector` / `binary` / `completion` / `percolator` / `search_as_you_type` / `histogram`**；
- **Posting List 压缩**：Lucene 9.x 使用 **Packed Blocks + Skip List + FST**：
  - **Block-based compression**：每 128 个 docId 打包为 `block`（存 base + 128 个 delta 值），解压代价可控；
  - **Skip List**：跨 block 跳表，支持 `skipTo(docId)`；
  - **FST（Finite State Transducer）**：term dictionary 编译为 **字节对齐 FST**，内存常驻，支持前缀/范围/正则查询（如 autocomplete、wildcard）；
  - 编码器实现：`lucene/core/src/java/org/apache/lucene/util/fst/FST.java`、`lucene/core/src/java/org/apache/lucene/codecs/lucene90/Lucene90PostingsFormat.java`。

### 7.3 DocValues（聚合/排序列存）

- **DocValues = 列存**：每个字段（除 `text`）默认生成 `column-stride` 的 doc_values，用于 **聚合、排序、script**。存于 `*.dvd` / `*.dvm`（Lucene 9.x 拆分两种文件）；
- 与 Posting List（行存倒排）互补：doc_values 是正排（docId → value），用于快速聚合、reorder。

### 7.4 translog（Write-Ahead Log）

- 源码：`server/src/main/java/org/elasticsearch/index/translog/Translog.java`；
- 每次写先追加 translog（默认 `request` 级别同步 fsync），再写入 in-memory buffer；
- 异常恢复：从 translog 重建尚未 refresh/flush 的 doc；
- 默认每 5s 或 translog 大于 512MB 触发 flush（Lucene commit）。

### 7.5 Refresh / Flush / Merge

- **Refresh（近实时可见）**：默认 1s，将 in-memory buffer 写成 Lucene segment；
- **Flush（持久化）**：将 segment commit 到磁盘，删除旧 translog；
- **Merge（合并段）**：TieredMergePolicy / LogByteSizeMergePolicy，合并小段为大段、减少文件数；
- 触发条件：后台 merge scheduler；段合并可在后台执行，查询时优先走小段索引。

## 八、索引机制（**搜索类型重点展开**）

### 8.1 倒排索引核心（Posting List + Term Dictionary + FST）

```
                    Lucene Segment
            ┌──────────────────────────┐
            │  Term Dictionary (FST)  │  ← 在内存,支持前缀/范围查询
            │   quick    → ptr=[0]   │
            │   quiet    → ptr=[... ]
            │   quality  → ptr=[... ]
            └────────────┬─────────────┘
                         │
            ┌────────────▼─────────────┐
            │  Posting List (indexed) │  ← block-based,128 docs/block
            │  term="quick"           │
            │   block 0: base=100     │
            │     d1=100,d2=103,d3=105│
            │     ...                 │
            │   block 1: base=1280    │
            │   ...                   │
            └────────────┬─────────────┘
                         │
            ┌────────────▼─────────────┐
            │  Skip List (跨 block)   │  ← block 间隔跳表,支持 skipTo
            └────────────┬─────────────┘
                         │
            ┌────────────▼─────────────┐
            │  Position List / Payload │  ← phrase query,slop,highlight
            └──────────────────────────┘
```

源码位置：
- **FST**：`lucene/core/src/java/org/apache/lucene/util/fst/FST.java`；
- **PostingsFormat**：`lucene/core/src/java/org/apache/lucene/codecs/lucene90/Lucene90PostingsFormat.java`；
- **BlockTreeTermStore**：`lucene/core/src/java/org/apache/lucene/codecs/lucene90/blocktree/Lucene90BlockTreeTermsWriter.java`（BlockTree 字典）。

### 8.2 BM25 / TF-IDF / 向量检索（Hybrid Retrieval）

ES 8.x 检索模型分三种：

| 模型 | 用途 | 关键参数 | 评分公式 |
|------|------|---------|----------|
| **BM25** | 全文检索默认（`similarity: BM25Similarity`） | `k1`（饱和度）、`b`（长度归一化） | `sum(tf * (k1+1)) / (tf + k1*(1 - b + b*dl/avgdl)) * idf` |
| **TF-IDF** | 经典/遗留（`similarity: ClassicSimilarity`） | 无 | `tf * idf` |
| **Vector（kNN）** | 语义/向量检索（`dense_vector` + HNSW） | `ef_construction` / `m` / `ef_search` | 余弦 / 点积 / L2 |

**BM25 关键参数**（`index.similarity`）：
- `k1` 默认 1.2，控制 TF 饱和速度；
- `b` 默认 0.75，控制文档长度归一化强度。

**Hybrid Retrieval**（ES 8.x 主力）：
- 检索路径：`server/src/main/java/org/elasticsearch/action/search/TransportSearchAction.java`、`server/src/main/java/org/elasticsearch/search/vectors/KnnSearchBuilder.java`；
- `knn` 子句 + `query` 子句并行执行，再 `rank`（RRF / 学习排序）。
- **RRF（Reciprocal Rank Fusion）**：8.8+ 默认融合，公式 `score = sum(1 / (k + rank_i))`；可通过 `rank.rrf.window_size`、`rank.rrf.rank_constant` 调优。

源码：
- `server/src/main/java/org/elasticsearch/search/rank/RankFeaturePhaseCoordinator.java`（RRF 协调）；
- `server/src/main/java/org/apache/lucene/search/similarities/BM25Similarity.java`（BM25 评分）。

### 8.3 过滤器 + 后置过滤（Filter vs Post-Filter）

- **Pre-Filter（filter context）**：进入评分前的 `bool.filter`，使用 **Bitset / DocValues 缓存**（`FilterCache` 由 IndicesRequestCache 缓存），**不参与打分**、**可缓存**；
- **Post-Filter（在 Query 后）**：默认 query + filter 时，filter 用于收缩已打分文档；post_filter 用于聚合前不收缩的过滤（如 facet）；
- 性能差异：**Pre-Filter 通常更优**（缓存 + 跳过打分）；**Post-Filter** 通常仅用于聚合时避免过滤影响 bucket。

源码：
- `server/src/main/java/org/elasticsearch/search/aggregations/AggregationPhase.java`（post_filter 实现）；
- `server/src/main/java/org/elasticsearch/indices/IndicesRequestCache.java`（filter cache）。

### 8.4 相关性排序与 Learning to Rank（LTR）

- **BM25** 默认；
- **Boosting**：通过 `boost` 参数或 `function_score` 调整；
- **function_score**：脚本 + 衰减函数（`gauss`、`linear`、`exp`）；
- **LTR（Learning to Rank）**：
  - x-pack `plugin/src/main/java/org/elasticsearch/xpack/rank/`；
  - `ltr` 端点训练模型，推理时给 Lucene Query 出 score；
  - 支持 LambdaMART / RankNet 等模型转换为 `feature` 注入；
- **ES|QL（SQL/DSL 演进）**：`/esql` 端点（8.11+），用 SQL-like 语法，集成查询、聚合、向量、JOIN。

### 8.5 倒排索引的可视化（DSL 例子）

```json
POST /products/_search
{
  "query": {
    "bool": {
      "must": [
        { "match": { "title": "elasticsearch 入门" } }
      ],
      "filter": [
        { "term": { "status": "active" } },
        { "range": { "price": { "lte": 200 } } }
      ]
    }
  },
  "knn": {
    "field": "embedding",
    "query_vector": [0.1, 0.2, ...],
    "k": 10,
    "num_candidates": 100
  },
  "rank": {
    "rrf": { "window_size": 50, "rank_constant": 20 }
  },
  "aggs": {
    "by_category": { "terms": { "field": "category.keyword" } }
  }
}
```

## 九、查询 / 计算 / 接口层（**搜索类型重点展开**）

### 9.1 DSL 设计

ES 默认 **Query DSL（JSON over HTTP）**：

#### 9.1.1 Match Query（评分）

```json
{ "match": { "message": "quick fox" } }
```

源码：`server/src/main/java/org/elasticsearch/index/query/MatchQueryBuilder.java` → 翻译为 Lucene `BooleanQuery`。

#### 9.1.2 Term Query（不过滤）

```json
{ "term": { "user.keyword": "alice" } }
```

#### 9.1.3 Bool Query

```json
{
  "bool": {
    "must":     [...],   // 评分
    "should":   [...],   // 评分（可参与）
    "must_not": [...],   // filter，不评分
    "filter":   [...]    // filter，不评分（可缓存）
  }
}
```

源码：`server/src/main/java/org/elasticsearch/index/query/BoolQueryBuilder.java`。

#### 9.1.4 Query String / Simple Query String

- **query_string**：解析完整的 Lucene Query Syntax（`field:value AND other`）；
- **simple_query_string**：容错更强、不抛错。

源码：
- `server/src/main/java/org/elasticsearch/index/query/QueryStringQueryBuilder.java`；
- `server/src/main/java/org/apache/lucene/queryparser/classic/QueryParserBase.java`。

#### 9.1.5 KQL（Kibana Query Language，**Kibana 端**）

KQL 由 Kibana 提供客户端解析，发送到 ES 端编译为 Query DSL（Lucene Query → ES Internal Query）。KQL 不存在于 ES 服务端。

### 9.2 聚合框架（Aggregation Framework）

聚合语法与查询并列，分为三大类：

#### 9.2.1 Bucket Aggregation（分桶）

- **terms**：按字段值分桶；
- **date_histogram** / **histogram**：按时间/数值区间分桶；
- **nested** / **reverse_nested**：嵌套文档；
- **geotile_grid** / **geohex_grid**：地理瓦片。

源码：`server/src/main/java/org/elasticsearch/search/aggregations/bucket/terms/TermsAggregator.java`。

#### 9.2.2 Metric Aggregation（指标）

- **sum / avg / min / max / stats / value_count**；
- **percentiles / percentiles_ranks / cardinality**（HLL 算法）；
- **top_hits / scripted_metric**。

源码：`server/src/main/java/org/elasticsearch/search/aggregations/metrics/`。

#### 9.2.3 Pipeline Aggregation（管道）

- **derivative / cumulative_sum / moving_avg / sum_bucket / avg_bucket** 等，把多聚合串接；
- 例：`derivative(sales_sum)` 计算相邻时段差分。

源码：`server/src/main/java/org/elasticsearch/search/aggregations/pipeline/`。

#### 9.2.4 Aggregation 执行模型

- 树形聚合器：每个 bucket 生成独立 `Aggregator`；
- **Two-pass 聚合**：第一次 `collect()` 收集 doc + 局部 bucket；第二次 `_post_collect()` 收敛 shard-local bucket；
- **Profiler**：`?profile=true` 返回每个 aggregator 耗时；
- **Cardinality / Percentiles** 使用近似算法（HLL / TDigest），可在精度和性能间折衷。

### 9.3 多租户搜索（Index Aliasing / Cross-Cluster / Federated）

#### 9.3.1 Index Alias（索引别名）

```http
POST /_aliases
{ "actions": [ { "add": { "index": "logs-2024*", "alias": "logs" } } ] }
```

写入多索引（`_aliases` + `is_write_index: true`）。
源码：`server/src/main/java/org/elasticsearch/cluster/metadata/IndexNameExpressionResolver.java`。

#### 9.3.2 Cross-Cluster Search（CCS，远程集群搜索）

- 设置远端 cluster seed：`cluster.remote.<cluster_name>.seeds`；
- 查询：`GET /<remote_cluster>:logs-*/_search`；
- 协调节点分发到对端集群，得到结果后融合排序；
- 源码：`server/src/main/java/org/elasticsearch/transport/RemoteClusterService.java`。

#### 9.3.3 Cross-Cluster Replication（CCR）

- 8.x 仍为 **Platinum** 功能，把 leader 索引增量复制到 follower；
- 源码：`x-pack/plugin/ccr/`。

#### 9.3.4 Federated / Async Search（商业版）

- **Async Search**：长查询异步化，适用于诊断型查询；
- **Cross-Project Search**（OpenSearch）：跨项目 federation（参考 OpenSearch 章节）。

## 十、事务与一致性

- **事务模型（搜索库较弱）**：
  - **单文档级别原子**：通过 Lucene `IndexWriter.updateDocument`（先标记删除 + 写入新 doc），但**不跨文档事务**；
  - **translog 提供 at-least-once**：translog fsync 前不会确认写入成功；
  - **跨分片写入**：ES 7.x + 默认 **wait_for_active_shards=1**，可调整 quorum=2；
  - **乐观并发控制（OCC）**：通过 `_seq_no` + `_primary_term` 实现，避免读写冲突。
- **一致性与副本**：
  - Primary Term + Sequence Number 协调；
  - 副本同步：`replication(action.support, ReplicationOperation.java)`；
  - 读取一致性：默认 `realtime=true`（同步转 refresh 取最新段），可设为 `false` 加速历史查询。

源码：
- `server/src/main/java/org/elasticsearch/index/seqno/SeqNoStats.java`；
- `server/src/main/java/org/elasticsearch/action/support/replication/ReplicationOperation.java`。

## 十一、复制与高可用

- **主从架构（Primary + Replica）**：
  - Primary 写入、副本异步拉取（同 `Translog` Replay）；
  - 配置：`index.number_of_replicas`；
  - `replica shard` 与 primary 不在同一节点（Master 校验 `cluster.routing.allocation`）。
- **Shard Allocation**：
  - 启用 **Disk Watermark**（`cluster.routing.allocation.disk.threshold_enabled`）；
  - **Forced Awareness**（zone/rack）；
  - **Total Shards Per Node**（`cluster.routing.allocation.total_shards_per_node`）。
- **Snapshot/Restore**：
  - 支持 S3、Azure Blob、GCP、共享文件系统、HDFS；
  - **Searchable Snapshots**（7.11+）：把快照 mount 为 partial / full / frozen tier；
  - 源码：`server/src/main/java/org/elasticsearch/snapshots/`。
- **Cluster State 管理**：
  - **Master Election**：默认基于 `ZenDiscovery`（7.0+ 改进为基于 `no_master_block` 的 Zen2/GCA）；
  - 主节点维护 cluster state（路由、配置、模板），通过 `MasterService` 串行处理；
- **Master 故障切换**：脑裂保护（`cluster.publish.timeout`、`discovery.zen.minimum_master_nodes` 已弃用，7.x 用 `cluster.election` 控制）。

## 十二、运维与可观测性

- **监控**：
  - `_cluster/stats`、`_nodes/stats`、`_cat/indices`、`_cat/nodes` 端点；
  - **Elastic Stack Monitoring**（x-pack）：集成 Metricbeat；
- **慢查询日志**：`index.search.slowlog.threshold.query.*` / `index.search.slowlog.threshold.fetch.*`；
- **索引生命周期（ILM）**：
  - 热/温/冷/冻结 四层；
  - 滚动（rollover）、收缩（shrink）、合并（forcemerge）、删除；
  - 源码：`x-pack/plugin/ilm/`；
- **API 稳定性保证**：主版本（8.x → 9.x）保留兼容，老 REST 路径保留别名；
- **可观测生态**：
  - 与 **APM Server**（OpenTelemetry 兼容）集成；
  - 与 **Prometheus + Grafana**：通过 `elasticsearch-exporter` 或 `Metricbeat` 输出指标；
  - **distributed tracing**：通过 `_tasks` API、ES interceptor 端到端。

## 十三、性能与基准

- **官方 benchmark**（8.x）：
  - 单 shard 写入：~5k-50k docs/s 取决于分词复杂度；
  - 聚合查询：10M 文档下 cardinality / histogram < 1s；
  - 真实生产：参考 `rally` 项目（`elasticsearch/rally`）；
- **OpenSearch Project Rally**：基准测试套件，行业通用；
- **社区案例**：ES 8.x + 1TB 索引，单节点 8 核即可支持亿级文档；
- **优化要点**：
  - 批量写入 `bulk` API 替代单文档写入；
  - 用 `keyword` 代替 `text`（避免分词开销）；
  - 用 **`index.refresh_interval: 30s`** 降写入放大；
  - 用 `index.codec: best_compression`（zstd / LZ4）；
  - 用 **searchable snapshot + cold tier** 降存储成本。

## 十四、生态与集成

- **客户端 SDK**（官方）：Java、Python、Go、.NET、Ruby、PHP、Perl、JS（server 端）、Rust（独立客户端）；
- **JDBC / ODBC**：商业版提供，跨 BI 工具；
- **ELK 栈**：Logstash（日志采集）、Kibana（可视化）、Beats（轻采集）；
- **AI / 推理集成**：
  - **ELSER（Elastic Learned Sparse Encoder）**：内置稀疏向量模型；
  - **Inference API**：对接 Cohere / OpenAI / HuggingFace / Azure OpenAI；
  - **semantic_text**（8.11+）：自动嵌入字段；
  - **ES|QL**（8.11+）：SQL-like DSL；
- **向量数据库竞争**：
  - 与 Milvus / Weaviate / Pinecone / Qdrant / pgvector 构成 **向量数据库** 阵营；
  - ES 8.5+ 起在向量检索上发力（HNSW + BBQ 量化 + `dense_vector` + `knn`），但严格场景仍由专用向量库承担。
- **容器化**：官方 Helm Chart、Operator（ECK - Elastic Cloud on Kubernetes）。

## 十五、学习路径

### 15.1 入门（1-2 周）

1. **官方文档**：`elastic.co/guide/en/elasticsearch/reference/current/`；
2. **跑通本地**：
   - `docker run -p 9200:9200 -p 9300:9300 -e "discovery.type=single-node" docker.elastic.co/elasticsearch/elasticsearch:8.x`；
3. **阅读官方教程**：
   - `Getting Started — Indexing`：`POST /<index>/_doc`；
   - `Searching`：`_search` + match + term + bool + aggs；
   - `Mappings`：text/keyword/date/numeric/boolean/dense_vector；
4. **配合 Kibana Dev Tools**：体验 Console 自动补全。

### 15.2 进阶（4-6 周）

1. **必读源码**：
   - `server/src/main/java/org/elasticsearch/index/engine/InternalEngine.java`：写引擎（Lucene IndexWriter 包装）；
   - `server/src/main/java/org/elasticsearch/action/search/TransportSearchAction.java`：搜索请求入口；
   - `server/src/main/java/org/elasticsearch/index/mapper/DocumentParser.java`：Mapping parse + 文档写入；
   - `server/src/main/java/org/elasticsearch/search/aggregations/AggregationPhase.java`：聚合执行；
   - `x-pack/plugin/ilm/`：生命周期源码；
   - `x-pack/plugin/sql/`：SQL 翻译；
2. **Lucene 内核**：
   - `lucene/core/src/java/org/apache/lucene/index/IndexWriter.java`：写入模型；
   - `lucene/core/src/java/org/apache/lucene/search/IndexSearcher.java`：检索核心；
   - `lucene/core/src/java/org/apache/lucene/util/fst/FST.java`：term dictionary；
   - `lucene/core/src/java/org/apache/lucene/codecs/lucene90/Lucene90PostingsFormat.java`：Posting 编码；

### 15.3 高级（长期）

1. **向量检索实现**：`x-pack/plugin/migrate/src/main/java/`（如 HNSW、BBQ）；
2. **Discovery / Zen2 / GCA**：`server/src/main/java/org/elasticsearch/cluster/`；
3. **Searchable Snapshots** 跨存储集成：`server/src/main/java/org/elasticsearch/repositories/blobstore/`；
4. **Plugin 开发**：参考官方示例 + AbstractPlugin SPI。

### 15.4 推荐资源

- **官方**：[elastic.co/guide](https://www.elastic.co/guide/)、Elastic{ON} 演讲、Elastic YouTube 频道；
- **书籍**：*Elasticsearch: The Definitive Guide*（Clinton Gormley / Zachary Tong）、*Elasticsearch 8.x Cookbook*；
- **论文**：
  - *"The Lucene Practical Scoring Function"*（Lucene 官方 wiki）；
  - *"BM25 and the Vector Space Model"*；
  - *"Lucene's Block-Based Posting List Encoding"*（Doug Cutting 团队博客）；
- **培训**：Elastic Certified Engineer、Elasticsearch Service Specialist；
- **社区**：Forum、Reddit r/elasticsearch、Stack Overflow、HN 讨论。

## 十六、风险与陷阱

1. **默认 1s Refresh Interval → 写入放大**：批量索引场景务必把 `index.refresh_interval` 调高（30s-`-1`），否则 refresh + segment 重建严重拖累写吞吐；
2. **深度分页（from + size）成本爆炸**：深度 from（如 10000+）会触底 `max_result_window`；推荐 **search_after / scroll / PIT（Point In Time）**；
3. **Mapping 类型不可变**：字段类型一旦写入不能改（不能 string → int）；写入错误会引发 `_mapper_parsing_exception`，合理 `reindex`；
4. **高基数聚合耗内存**：unique count、terms 用 `cardinality` 替代 `terms` 计数，避免 OOM；
5. **副本数=R 写入延迟叠加**：副本越多 Primary 写入等待越久；可调 `index.write.wait_for_active_shards` 与 `wait_for_active_shards`；
6. **Filter cache 占用堆**：filter 后缓存 keystore 内存大，监控 `indices.requests.cache.memory_size`；
7. **冷数据膨胀**：默认不开 `index.codec: best_compression`，存储比 zstd 高 30%；
8. **向量检索不适合 top-k 很大**：HNSW + HNSW + DiskANN 在 >10M 高维向量上性能不及专用向量库（Milvus / Qdrant）；
9. **集群状态广播成本**：Master 修改 cluster state 会 gossip 同步，大集群慎用 `_template` 频繁更新；
10. **Snapshot 重建索引的代价**：跨版本恢复前必须确认 `repository` 兼容；7.x → 8.x 升级前先 `POST /_reindex` 到中间版本。
11. **多租户权限复杂**：Security 配置（RABC / Role / API Key）错配导致越权访问，生产必须审计。

## 十七、与本项目的关联

本项目 `engineering/` **未实现** Elasticsearch 全兼容引擎，但下列借鉴点：

- **WAL + translog**：`engineering/src/db/core/wal.c` 已实现简化 WAL，可参考 `server/src/main/java/org/elasticsearch/index/translog/Translog.java` 的 **分段切分 + Checkpoint + Recovery** 设计；
- **倒排索引 / FST / Posting List**：`engineering/src/index/` 已实现 HNSW/PG Linear Hash/B+Tree，可参考 **Lucene 的 BlockTree + FST** 在 `engineering/src/index/inverted/` 添加教学版倒排索引；
- **批量写入与并发控制**：参考 `_seq_no` + `_primary_term`，可在 `engineering/src/db/core/` 增加 OCC 实现；
- **聚合框架的 Two-Pass 模型**：可借鉴 partial_agg → reduce 流程在 `engineering/src/algo/groupby.c` 引用；
- **REST API + JSON DSL 解析**：参考 `server/src/main/java/org/elasticsearch/rest/RestController.java`，可在 `engineering/src/algo/json_dsl.c` 简化版；不引入大依赖。

## 十八、参考资料

1. Elasticsearch 8.x 官方文档：<https://www.elastic.co/guide/en/elasticsearch/reference/current/>
2. Elasticsearch GitHub：<https://github.com/elastic/elasticsearch>
3. Apache Lucene 9.x 官方文档：<https://lucene.apache.org/core/>
4. Lucene 源码：<https://github.com/apache/lucene-solr>
5. *"The Lucene Practical Scoring Function"*：<https://lucene.apache.org/core/scoring.html>
6. *"Block-Max WAND Algorithm"*（Lucene 跳表优化论文）
7. ES Rally（官方基准）：<https://github.com/elastic/rally>
8. *Elasticsearch: The Definitive Guide*（Clinton Gormley / Zachary Tong, O'Reilly）
9. ES|QL 官方博客：<https://www.elastic.co/blog/tag/esql>
10. Elastic Stack 8.x 新特性：<https://www.elastic.co/blog/whats-new-elasticsearch-8-x>
