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

## 十八、Lucene 倒排索引的存储布局与编码算法（算法展开）

> **版本锚点**：本节源码路径以 **Elasticsearch 8.19 + Lucene 9.12.2** 为准 —— ES 8.19 的 `build-tools-internal/version.properties` 明确写着 `elasticsearch = 8.19.24` 与 `lucene = 9.12.2`，两者严格配对。本仓库 `reference/search/elasticsearch/` 子模块未 init（目录内除 `.git` 外 0 个条目），故**一律只给 canonical 路径、不给行号**，行号需对照上述版本自行校对。
>
> ⚠️ **与已有章节的版本差异**（按骨架保护原则，旧节原文一字未改）：§7.1 与 §8.1 引用的 `lucene/core/src/java/org/apache/lucene/codecs/lucene90/Lucene90PostingsFormat.java` 在 **lucene-core 9.12.2 中已不存在** —— Lucene 9.12 起默认倒排 codec 换成了 `Lucene912PostingsFormat`。旧节路径对应的是 Lucene 9.0~9.11 时代的默认格式。本节按 9.12.2 实测结果书写。

### 18.1 一个 segment 落盘成哪些文件

Lucene 9.12.2 的默认 codec 是 `Lucene912Codec`，它把每个 segment 拆成「一组列式文件 + 一棵词典树」：

| 扩展名 | 默认 codec | 内容 | 访问模式 |
|--------|-----------|------|---------|
| `.fnm` | `Lucene94FieldInfosFormat` | 字段元数据（字段名、`IndexOptions`、doc_values 类型） | 打开 segment 时读一次 |
| `.tim` | `Lucene90BlockTreeTermsWriter` | 词项字典（BlockTree：分块 + 前缀/后缀压缩） | 顺序 / 二分定位 |
| `.tip` | `Lucene90BlockTreeTermsWriter` | 词项索引：**每字段一棵 FST**，词项前缀 → 块的文件指针 | 常驻内存随机访问 |
| `.tmd` | `Lucene90BlockTreeTermsWriter` | 词项元数据（每字段统计量、FST 元信息） | 打开 segment 时读一次 |
| `.doc` | `Lucene912PostingsFormat` | docId 增量 + 词频（分块打包） | 顺序扫描 / 分块随机定位 |
| `.pos` | `Lucene912PostingsFormat` | position 增量 | 短语查询、slop |
| `.pay` | `Lucene912PostingsFormat` | payload / offset | 高亮、词向量 |
| `.psm` | `Lucene912PostingsFormat` | 倒排表元数据：每个 term 一条，指向它在 `.doc` 中的偏移 | 随机定位某个 term |
| `.nvd` | `Lucene90NormsFormat` | 长度归一化因子（**每文档 1 字节**） | 随 docId 顺序读 |
| `.dvd` / `.dvm` | `Lucene90DocValuesFormat` | doc_values 数据 / 元数据 | 按 docId 直接寻址（列存） |
| `.fdt` / `.fdx` / `.fdm` | `Lucene90StoredFieldsFormat` | stored fields 文档原文 / 稀疏索引 / 元数据 | 按需取字段 |
| `.liv` | `Lucene90LiveDocsFormat` | 删除位图 | 位运算 |
| `.vem` / `.vex` | `Lucene99HnswVectorsFormat` | 向量元数据 / HNSW 图 | 图遍历 |

关键设计：**倒排（词 → doc）与正排（doc → 词）物理分离**。查询时只需要打开其中一侧的文件，这就是 §7.3「doc_values 与 posting list 互补」的物理原因。

ES 侧的包装层（负责决定 mapping 如何映射成 Lucene `FieldInfo`）：

- `server/src/main/java/org/elasticsearch/index/mapper/FieldMapper.java`
- `server/src/main/java/org/elasticsearch/index/mapper/KeywordFieldMapper.java`（`keyword`：只有倒排，默认开 doc_values）
- `server/src/main/java/org/elasticsearch/index/mapper/NumberFieldMapper.java`（数值：倒排 + doc_values + points）
- `server/src/main/java/org/elasticsearch/index/codec/CodecService.java`（注册 codec）

> **一个反直觉的事实**：ES 的 `_source` **不是 Lucene 的独立格式**。它由 `server/src/main/java/org/elasticsearch/index/mapper/SourceFieldMapper.java`（`NAME = "_source"`）作为**一个 stored field 写进去**。所以 `_source` 的压缩行为完全由 stored fields codec 决定（ES 8.19 另有私有的 `server/src/main/java/org/elasticsearch/index/codec/zstd/Zstd814StoredFieldsFormat.java`），而不是由 `_source` 自己的开关决定。

### 18.2 BlockTree 词项字典：为什么不是「一词一条记录」

如果每个词项写一条「词长 + 词字节 + 偏移」记录，词典会被词条自身撑爆。BlockTree 的做法是**先按词典序把词项切成块，再让块内词项只存「相对块首词的前缀公共部分之后的增量后缀」**。

**块大小的工程取值**（`Lucene90BlockTreeTermsWriter` 实测常量）：

| 常量 | 值 | 含义 |
|------|----|------|
| `DEFAULT_MIN_BLOCK_SIZE` | **25** | 建议的最小块内词项数 |
| `DEFAULT_MAX_BLOCK_SIZE` | **48** | 建议的最大块内词项数 |

**块内记录的字节布局**（写入侧实测）：

```
每块先写一个 vLong 作为「token」：
  token = (suffixLength << 3) | (isLeafBlock ? 0x04 : 0) | compressionAlgorithmCode
  ├─ 高位：块内所有词项的「后缀」总字节数（不是单个词的）
  ├─ bit2  ：本块是否为叶子块（块内再无下级块）
  └─ 低 3 位：后缀压缩算法编号
                 ├─ NO_COMPRESSION  : 原样写后缀字节
                 ├─ LOWERCASE_ASCII : 1 字节/词的上下文编码（大量小写 ASCII 时显著变小）
                 └─ LZ4             : 仅当 LZ4 比原文省 > 25% 时才启用
随后依次写：
  1) 后缀字节（按上面选定的算法压缩后写入）
  2) 后缀长度数组（每词 1 字节）
       └─ 若所有长度相同 → 特殊编码：writeVInt((numSuffixBytes << 1) | 1) 再写 1 个字节
       └─ 否则            → writeVInt(numSuffixBytes << 1) 后跟 numSuffixBytes 个字节
  3) 统计量（docFreq / totalTermFreq 等）字节块
  4) term metadata 字节块（指向 .doc 的偏移等）
```

**「后缀长度全相同」的特判**（上面的 `<< 1 | 1`）是典型的工程优化：像 ID 这类结构化字段，同一块里词项长度往往一致，用 1 个字节替代 N 个字节几乎零成本。

**`.tip` 里的 FST 索引 + floor blocks**：FST 把「词项前缀」映射到「磁盘块」。但当一个前缀下的词项多到超过 `DEFAULT_MAX_BLOCK_SIZE` 时，这个块会被再切成若干 **floor block**，此时 FST 的输出值里额外编码了每个 floor block 的首字节与文件指针。这样 FST 的大小始终与「块数」而非「词项数」成正比。

源码：`lucene/core/src/java/org/apache/lucene/codecs/lucene90/blocktree/Lucene90BlockTreeTermsWriter.java`、`.../blocktree/Lucene90BlockTreeTermsReader.java`、`.../blocktree/SegmentTermsEnumFrame.java`。

### 18.3 FST 的状态转移压缩：把词典再压一层

`.tip` 的 FST 由 `lucene/core/src/java/org/apache/lucene/util/fst/FST.java` 与 `FSTCompiler.java` 实现，它是一个**最小化确定有限状态自动机**（DAWG 化），输出压缩为一个 `byte[]`。三条关键性质：

1. **最小化**：构建时把同构子状态合并（要求输入按 UTF8 码点**字典序**喂入，乱序会直接抛异常）；
2. **字节寻址**：整棵树就是一个 `byte[]`，每条弧的 flag/label/target 都按字节偏移读取，**无需反序列化对象**，可 mmap 常驻；
3. **弧的目标相对编码**：`BIT_TARGET_NEXT` 表示「目标就是 `bytes.get(pos + 1)`」——即下一个节点紧接着当前节点存放，把「随机访问」退化成「顺序前进」。

**节点的三种弧存储布局**（由节点 flags 决定，`FST.java` 实测常量）：

| 布局 | flags 值 | 含义 | 查标号成本 |
|------|---------|------|-----------|
| 稀疏定长 + 二分 | `ARCS_FOR_BINARY_SEARCH` | 定长弧数组，按 label 二分 | `O(log 出度)` |
| 稠密 + 位表直接寻址 | `ARCS_FOR_DIRECT_ADDRESSING` | 位表判存在性 + 连续数组 | `O(1)` |
| 连续弧 | `ARCS_FOR_CONTINUOUS` | 稠密 + `ARCS_FOR_DIRECT_ADDRESSING`，且 `label - firstLabel` 直接作下标 | `O(1)` |

其余弧标志位：`BIT_FINAL_ARC = 1<<0`、`BIT_LAST_ARC = 1<<1`、`BIT_TARGET_NEXT = 1<<2`、`BIT_STOP_NODE = 1<<3`、`BIT_ARC_HAS_OUTPUT = 1<<4`、`BIT_ARC_HAS_FINAL_OUTPUT = 1<<5`。

**这就是 ES 通配符 / 前缀 / 自动补全查询能跑得动的原因**：查询不是对词典做全量扫描，而是从 FST 根节点按查询串逐字节下钻，只访问匹配的路径；`seekCeil` 语义保证「≥ 目标的最小词项」在 `O(|查询串|)` 次弧跳内定位（多带一个块的线性/二分收敛）。

> ⚠️ **与 §7.2 / §8.1 的表述差异**：旧节把「FST + Skip List」并列为 posting 的编码，且称 postings 格式实现为 `Lucene90PostingsFormat`。实测 9.12.2 中 **FST 只服务词项字典（`.tim` / `.tip`），skip data 服务倒排表（`.doc`）**，两者分属不同文件、不同结构；`Skip List` 一词在实测源码里对应的是 skip data 的**两级层次**（见 18.5），不是独立的一张表。

### 18.4 倒排表的分块编码：d-gap + PackedInts + VInt 尾巴

`.doc` 文件的组织（`Lucene912PostingsFormat` 实测常量与 javadoc）：

| 常量 | 值 | 含义 |
|------|----|------|
| `BLOCK_SIZE` | **128** | 每个 packed block 打包 128 个 docId（等于 `ForUtil.BLOCK_SIZE`） |
| `LEVEL1_FACTOR` | **32** | 每 32 个 block 额外插一级 skip data |
| `LEVEL1_NUM_DOCS` | **4096** | `LEVEL1_FACTOR × BLOCK_SIZE`，即 level-1 skip 的 doc 跨度 |

**两级块结构**：

```
TermFreqs = <PackedBlock32>^(PackedDocBlockNum / 32) , VIntBlock?
PackedBlock32 = Level1SkipData , <PackedBlock>^32
PackedBlock   = Level0SkipData , PackedDocDeltaBlock , PackedFreqBlock?
其中 PackedDocBlockNum = floor(DocFreq / 128)
```

**docId 用「差分 + 位打包」**：先算相邻 docId 的差（d-gap），每 128 个差值按 PackedInts 打包进一个 block。两种 block 的差别是精心设计的：

| block | 编码方式 | 原因 |
|-------|---------|------|
| `PackedDocDeltaBlock` | PackedInts，**不做 patching** | 文档数天然近似均匀分布，极端值很少，paying 不划算 |
| `PackedFreqBlock` | PackedInts，**使用 patching**（`PForUtil`） | 词频分布长尾严重（绝大多数 tf=1，少数 tf=上千），把超阈值的大值单独记为「exception」，主体用更窄的位宽 |

`PackedInts` 本身在 `lucene/core/src/java/org/apache/lucene/util/packed/PackedInts.java`：
- `Format.PACKED` 是紧凑连续位（`byteCount = ceil(count × bits / 8)`）；
- 旧的 `PACKED_SINGLE_BLOCK`（补齐到 64 位对齐）**已废弃**，解包时由 `ForUtil.expand8 / expand16 / expand32` 用「掩码左移展开」把打包位摊开到整条 64 位 lane（`expandMask8/16/32`），这是位级解包的核心技巧。

**VInt 尾巴的 `DocDelta × 2` 编码**（不足一个 block 的残余 docId/freq）：若索引了词频，则
- `DocDelta` 为**奇数** → 该 doc 的 tf 恒为 **1**；
- `DocDelta` 为**偶数** → 紧接着再读一个 VInt 作为 tf；
- 若未索引词频（`IndexOptions.DOCS`），则不乘 2，直接就是 docId 差值。

实测 javadoc 给的样例：某 term 在 doc 7 出现 1 次、在 doc 11 出现 3 次，编码为 VInt 序列 **`15, 8, 3`**（`7×2+1=15`；`4×2=8` 随后 3 是 tf）；若关闭词频则编码为 `7, 4`。这是一个「用 1 个多余比特换掉一整条 VInt 读」的设计。

**Singleton 优化**：若某 term 只在一个文档里出现，写入端不再往 `.doc` 写指针 + block，而是把该 docId **直接写进词项字典**（`.psm`/`.tim` 中的 `SingletonDocID`）。

源码：`lucene/core/src/java/org/apache/lucene/codecs/lucene912/Lucene912PostingsFormat.java`、`Lucene912PostingsWriter.java`、`Lucene912PostingsReader.java`、`.../lucene912/ForUtil.java`、`.../lucene912/PForUtil.java`。

### 18.5 两级 skip data 与 block-max 的原料（Impacts）

每个 block 头部挂一段 skip data，**level-0 每 128 个 doc 一份，level-1 每 4096 个 doc 一份**。实测字段布局：

```
Level1SkipData = DocDelta, DocFPDelta, Skip1NumBytes?, ImpactLength?, Impacts?,
                 PosFPDelta?, NextPosUpto?, PayFPDelta?, NextPayByteUpto?
Level0SkipData = Skip0NumBytes, DocDelta, DocFPDelta, PackedBlockLength,
                 ImpactLength?, Impacts?, PosFPDelta?, NextPosUpto?, PayFPDelta?, NextPayByteUpto?
```

三个设计点：

1. **`Skip0NumBytes`**：记录本块 skip data 自身的长度。只用 `nextDoc()` 顺序扫描时可以**整段跳过 skip data 都不读**；只关心前几个字段时也能跨过剩余字段。
2. **`DocFPDelta`**：本 term 的词频数据与**上一个 term** 数据的文件偏移之差（块内相对编码），使 term 之间可以「相对跳转」而不用每 term 存绝对 8 字节指针。
3. **`Impacts`（`ImpactLength` + `Impacts`）**：仅当索引了词频时才写。它是 **block-max 提前终止算法的全部原料** —— 一个 `Impact` 就是一对 `(int freq, long norm)`（见 `lucene/core/src/java/org/apache/lucene/index/Impact.java`），代表「这个块里至少有一条 doc 的词频 ≥ freq，且长度因子 norm ≤ 该值」。用最小 norm + 最大 freq 就能给整块算出一个**分数上界**，见 §十九。

源码：`lucene/core/src/java/org/apache/lucene/index/Impact.java`、`Impacts.java`、`ImpactsEnum.java`、`ImpactsSource.java`。

### 18.6 存储布局各环节的复杂度

| 操作 | 复杂度 | 依据 |
|------|--------|------|
| 定位一个 term | `O(|term 字节数|)` 弧跳 + 1 次块定位 | FST 常驻内存、按字节下钻；`.tip` 的每字段一棵 FST |
| 块内定位 posting | `O(1)` | `.psm` 给出 term 在 `.doc` 的文件指针 + 块内偏移 |
| 解码一个 block 的 128 个 docId | `O(BLOCK_SIZE) = O(128)` | `ForUtil.expand8/16/32` 位展开 |
| `advance(target)` 跳块 | `O(跨过的块数)` 次常数级读 + 一次 `O(128)` 解码 | level-0 每 128 doc、level-1 每 4096 doc 各一份 skip |
| 顺序扫完一个 term 的 posting | `O(df)` | 全量读取是下界 |
| doc_values 随机读第 N 行 | `O(1)` | `.dvm` 元数据 + 直接寻址块表 |
| `_source` 取一个字段 | `O(定位字节数)` | `.fdx` 稀疏索引给出字节偏移 |
| `.liv` 判活 | `O(1)` | `O(N/8)` 字节位图 + `BitSet` |

**空间账**：`.tip`（FST）大小 ∝ **块数** ∝ 词项数 / 25~48，与词项总数呈平方根级以下的关系；这是「词典可以常驻内存而词项本身不能」的根本原因。

---

## 十九、BM25 评分与 Block-Max 提前终止（算法展开）

> **版本锚点与免责**：同 §十八，以 **Elasticsearch 8.19.24 + Lucene 9.12.2** 实测源码为准；本仓库 `reference/search/elasticsearch/` 子模块未 init，故**只给 canonical 路径、不给行号**。
>
> ⚠️ **重要纠偏（brief 与旧节的常见误区）**：Lucene 9.12 的 `lucene-core` 里**已经没有** `MaxScoreScorer` 与 `BlockMaxWANDScorer` 这两个类（实测 `find` 结果为空）。论文里的「MaxScore / Block-Max WAND」是算法名；9.12 的实现落到了 `WANDScorer`、`BlockMaxConjunctionScorer`、`MaxScoreCache`、`ImpactsDISI` 四个类上，由 `Boolean2ScorerSupplier` 按查询形状选择。本节按 9.12.2 的真实实现书写。

### 19.1 BM25 的真实公式：idf 不是教科书的 `log(N/df)`

`lucene/core/src/java/org/apache/lucene/search/similarities/BM25Similarity.java` 实测：

```
score(q, d) = Σ_{t ∈ q}  idf(t) × ( freq(t,d) × (k1 + 1) ) / ( freq(t,d) + k1 × (1 − b + b × |d| / avgdl) )

默认参数：k1 = 1.2f,  b = 0.75f（discountOverlaps = true）
avgdl     = sumTotalTermFreq / docCount          ← 实测 avgFieldLength()
idf(t)    = log(1 + (N − df + 0.5) / (df + 0.5))  ← 实测 idf()，非 log(N/df)
```

`idf` 用 `log(1 + …)` 而不是 `log(N/df)` 的原因很实际：`log(N/df)` 在 `df > N/2` 时会变**负数**，把常见词（出现在过半文档里）的分数拉到负值以下。`log(1 + x)` 保证 `idf ≥ 0`。源码里 `idfExplain` 的解释文本原样写着 `idf, computed as log(1 + (N - n + 0.5) / (n + 0.5))`，用 `?explain=true` 就能在响应里看到它。

ES 侧可换 `similarity`（`BM25Similarity` / `ClassicSimilarity` / LTR），入口是 `server/src/main/java/org/elasticsearch/index/similarity/SimilarityService.java`；ES 自带的「最小竞争分」包装器在 `server/src/main/java/org/elasticsearch/common/lucene/search/function/MinScoreScorer.java`。

### 19.2 长度因子被压成 1 字节：256 档查表

`|d|` 是每个文档都可能不同的量，不可能逐 doc 存一个 float。Lucene 的做法是**把长度因子预先折进 norm 的倒数、量化成 1 个字节**：

```
// BM25Similarity.scorer() 实测
float[] LENGTH_TABLE = new float[256];
for (int i = 0; i < 256; i++) LENGTH_TABLE[i] = SmallFloat.byte4ToInt((byte) i);   // 解码出长度

float[] cache = new float[256];                                                    // 预计算长度归一化的倒数
for (int i = 0; i < 256; i++)
    cache[i] = 1f / (k1 * ((1 - b) + b * LENGTH_TABLE[i] / avgdl));
```

于是打分退化成**一次数组查表 + 一次乘加**。norm 本体存在 `.nvd`（`Lucene90NormsFormat`）里，每文档 1 字节。

**一个容易被忽略的工程细节**（`BM25Scorer.score()` 实测注释）：

```
// 教科书写法：freq / (freq + k1*(1-b+b*dl/avgdl))
// Lucene 实际写成：
float normInverse = cache[((byte) encodedNorm) & 0xFF];
return weight - weight / (1f + freq * normInverse);        // weight = boost × idf
```

源码注释解释了原因：`freq / (freq + norm)` 在 float 精度下**对 freq 和 norm 都不保证单调**，可能因为舍入而出现「文档更长、分数反而更高」的逆序；改写成 `1 − 1/(1 + freq × 1/norm)` 后，单调性由「乘除各自就近舍入 → `x ↦ 1+x`、`x ↦ 1−1/x` 复合保持单调」严格保证。**这是排序型数据库里「浮点单调性即正确性」的典型写法。**

### 19.3 剪枝阈值从哪来：minCompetitiveScore

提前终止不是猜一个阈值，而是由 top-N 收集器**动态回灌**的（`lucene/core/src/java/org/apache/lucene/search/TopScoreDocCollector.java`）：

```
收集器维护一个大小为 N 的最小堆：
  if (score > minCompetitiveScore):
      入堆
      minCompetitiveScore = 堆顶分数          # 第 N 名
      scorer.setMinCompetitiveScore(minCompetitiveScore)   # 回灌给 scorer

实测还有一个 docId tie-break 的细节：
  docBase >= maxMinScore.docBase ? Math.nextUp(maxMinScore.score) : maxMinScore.score
```

`Math.nextUp` 那一行值得注意：Lucene 的 top-N 在分数相同时**按 docId 升序 tie-break**，且收集本身按 docId 顺序进行，所以「分数等于堆顶」的文档**一定比堆顶更晚出现、不可能进入结果**。因此可以把阈值**上调一个 ULP**（`nextUp`）换取更激进的剪枝，且不会漏召回。这是典型的「用浮点表示的上确界换剪枝率」。

### 19.4 块级上界从哪来：Impacts → MaxScoreCache

有了阈值 τ，还需要「这一段文档的分数上界」。上界来自 §18.5 写进 skip data 的 `Impacts`：

```
// lucene/core/src/java/org/apache/lucene/search/MaxScoreCache.java 实测
globalMaxScore = scorer.score(Float.MAX_VALUE, 1L)      # 字段级最乐观上界

getMaxScore(upTo):
    level = 第一个满足 impacts.getDocIdUpTo(level) >= upTo 的 level   # 找不到返回 -1
    return level == -1 ? globalMaxScore : getMaxScoreForLevel(level)

getMaxScoreForLevel(level):    # 结果按 level 缓存，块内不重复算
    return max over impacts.getImpacts(level) of scorer.score(impact.freq, impact.norm)
```

因为一个 `Impact` 记的是「块内 (最大 tf, 最小 norm)」这一对极值，而 BM25 对 `freq` 单调递增、对 `norm`（长度）单调递减（b>0 时），所以「取块内最大 tf 配最小 norm 算一次」得到的就是**块内任意文档分数的上界**。缓存以 level 为粒度，块内反复查询不再重算。

`TermScorer` 把这套能力打包成两个接口方法（`getMaxScore(upTo)` / `advanceShallow(target)`），由 `ImpactsDISI` 提供近似迭代器。

### 19.5 WAND：双堆 + 24 位定点缩放

`lucene/core/src/java/org/apache/lucene/search/WANDScorer.java` 的算法骨架（实测字段与流程）：

```
输入：一组 scorer（每个 term 一个）、τ（minCompetitiveScore）、minShouldMatch

【第 0 步：定点化】
  常量 FLOAT_MANTISSA_BITS = 24,  MAX_SCALED_SCORE = (1<<24) − 1
  maxScoreSum = Σ_t scorer_t.getMaxScore(NO_MORE_DOCS)
  scalingFactor = 能把 maxScoreSum 的量级映射到 2^24 的 2 的幂
  scaleMaxScore(f) = ceil(f × 2^scalingFactor)        # 向上取整 → 上界永不低于真值

【第 1 步：lead / tail 两个按 doc 排序的小顶堆】
  lead = 已经定位到当前 doc 的 scorer（论文里的 pivot）
  tail = 其余 scorer，按 maxScore 排序的小顶堆
  leadMaxScore = Σ_{t∈lead} scaledMax(t)
  tailMaxScore = Σ_{t∈tail} scaledMax(t)

【第 2 步：updateMaxScores(target)，实测核心】
  if lead 为空:
      upTo = tail.top().advanceShallow(target)        # 用最大分贡献者当 lead
  else:
      upTo = min over lead of advanceShallow(lead.doc) # 取 lead 集合中最早的块边界
  对每个 tail 元素：advanceShallow(target)，重算 scaledMaxScore，tailMaxScore 累加

  while (tailSize > 0 && tailMaxScore >= τ):           # tail 自己就够进 top-N
      t = popTail(); t.doc = t.advance(target); lead.add(t)

  # 退出时若 tailMaxScore < τ ⇒ 「剩下的 tail 全算上也不够 τ」⇒ 可整段跳过

【第 3 步：收束为下一个候选】
  若 leadMaxScore < τ：跳到 upTo 之后（不逐 doc 打分）
  否则：pivot = lead 中 doc 最小者，只需验证 pivot 所在的候选文档
```

**为什么要定点化**（源码注释原话大意）：阈值比较发生在循环最内层，用 float 会因累加误差而「剪过头」漏召回。把分数缩放到 long 并保留 24 位有效位（恰好是 float 的尾数位数），既能保证 long 运算不溢出（`MAX_SCALED_SCORE = 2^24 − 1`），又不会比 float 更精确到「反而跳过本该命中的文档」；所有上界一律 `ceil`（向上取整）以保证保守。

**lead 边界不取 tail 参与的原因**（源码注释）：tail 是分数贡献最小的一批，把它们的块边界纳入取 min 会让 `upTo` 推进得太慢，导致反复重算块边界反而更费 CPU。

### 19.6 Boolean 查询到底选哪个剪枝器

`lucene/core/src/java/org/apache/lucene/search/Boolean2ScorerSupplier.java` 实测选择规则：

| 查询形状（且处于顶层评分） | 选用的 Scorer | 剪枝方式 |
|---------------------------|---------------|---------|
| 多个 `must`/`filter` 参与打分（`ScoreMode.TOP_SCORES`） | `BlockMaxConjunctionScorer` | 交集的块级 max 之和 < τ 时整块跳过 |
| 多个 `should`（纯析取，`minShouldMatch ≤ 1`） | `WANDScorer` | 上面的 lead/tail 双堆 |
| 纯析取但不需要 top-N 分数（`COMPLETE`）或非顶层 | `DisjunctionSumScorer` | 不剪枝，穷举打分 |

源码里明确写了「WANDScorer 理论上能处理上面三种情况，但为了纯穷举析取更快，仍保留 `DisjunctionSumScorer`」。

`BlockMaxConjunctionScorer`（`lucene/core/src/java/org/apache/lucene/search/BlockMaxConjunctionScorer.java`）的三个取舍：

1. 构造时按 `iterator().cost()` 排序，最便宜的当 **lead**；
2. 存在 `TwoPhaseIterator` 的 scorer 只用它的 **approximation**（不下钻到打分层）；
3. `advanceShallow(target)` **只采用 lead 的块边界**（注释明确：纳入其他子句的边界能给出更紧的上界，但有「推进过慢、反而更慢」的风险）。

另外 `BooleanQuery` 在 `minimumNumberShouldMatch <= 1` 时会**扁平化嵌套的纯析取子句**，源码注释直言「这对 block-max WAND 跑得好很重要」——因为 lead/tail 机制要求析取列表是**扁平的 term 级 scorer**，嵌套 BooleanQuery 会把剪枝机会藏起来。

ES 侧的对应物：`server/src/main/java/org/elasticsearch/search/query/QueryPhase.java`（把 `search_after` 转成 filter，见 §17 提到的深分页路径）与 `server/src/main/java/org/elasticsearch/search/internal/ContextIndexSearcher.java`（ES 自有的 IndexSearcher 子类，在 `searchLeaf` 里叠加 ES 层的过滤/超时）。

### 19.7 剪枝强度与代价对照

| 剪枝方式 | 上界粒度 | 上界计算代价 | 最坏退化情形 |
|---------|---------|-------------|-------------|
| 无剪枝（穷举打分） | 无 | — | 必须对 `Σ df` 条 posting 全部打分 |
| 动态阈值（minCompetitiveScore） | 单 term 全局 max | 极低 | 所有 term 的 max 都很高 → τ 抬不动，剪枝几乎无效 |
| MaxScore / lead-tail 判定 | 单 term 全局 max + 求和 | 一次堆操作 | 同上；上界过松导致无法整段跳过 |
| Block-Max（impacts + `MaxScoreCache`） | block 级（level-0 = 128 doc，level-1 = 4096 doc） | 每 level 一次 `score(maxFreq, minNorm)`，结果缓存 | 词频在块内分布均匀时，块级 max 逼近真实值 → 退化为不剪枝（这正是罕见词的典型情形） |

一句话总结三者的关系：**阈值回答「还值不值得打分」，block-max 回答「这一段最多能打多少分」，WAND 用 lead/tail 回答「现在能不能整段跳过」**。三者叠加才构成 Block-Max WAND。

---

## 二十、Segment 合并策略与 DocValues 分工（算法展开）

> **版本锚点与免责**：同 §十八，以 **Elasticsearch 8.19.24 + Lucene 9.12.2** 实测源码为准；子模块未 init，**只给 canonical 路径、不给行号**。

### 20.1 合并要解决什么：允许段数的几何分层计算

每 refresh 产生一个小 segment；段太多会让「一次查询要开 N 个 Searcher + N 棵词典 FST」。`TieredMergePolicy` 的判定第一步是算出**当前索引允许存在多少个段**（`lucene/core/src/java/org/apache/lucene/index/TieredMergePolicy.java` 实测）：

```
输入：所有段的 (sizeInBytes, maxDoc, delCount)，按 size 降序排好

1) 统计量
   totalMaxDoc = Σ maxDoc ;  totalDelDocs = Σ delCount
   totalDelPct = 100 × totalDelDocs / totalMaxDoc
   allowedDelCount = deletesPctAllowed × totalMaxDoc / 100        // 默认 20%

2) 豁免两类「本轮不参与合并」的段
   (a) 超大段：sizeInBytes > maxMergedSegmentBytes/2 且 (整体删除率 ≤ 20% 或自身删除率 ≤ 20%)
       → tooBigCount++，并从 totIndexBytes 中扣除
   (b) 前 targetSearchConcurrency−1 个最大的段整段保留（为检索并发留副本）
   allowedSegCount += concurrencyCount

3) 几何分层求剩余配额（核心循环，实测）
   mergeFactor    = min(maxMergeAtOnce, segsPerTier)              // 默认 min(10, 10) = 10
   levelSize      = max(minSegmentBytes, floorSegmentBytes)        // 默认 2 MB
   bytesLeft      = totIndexBytes
   while (true):
       segCountLevel = bytesLeft / levelSize
       if (segCountLevel < segsPerTier || levelSize == maxMergedSegmentBytes):
           allowedSegCount += ceil(segCountLevel);  break
       allowedSegCount += segsPerTier
       bytesLeft     -= segsPerTier × levelSize
       levelSize      = min(maxMergedSegmentBytes, levelSize × mergeFactor)
   allowedSegCount = max(allowedSegCount, max(segsPerTier, targetSearchConcurrency))

4) 若 eligible 段数 ≤ allowedSegCount → 本轮不合并
```

**关键洞察**：段越大，允许的段数越少，但**不是线性递减** —— 每放大 `mergeFactor`（10 倍）才允许段数**再涨 `segsPerTier`（10 个）**。所以长期稳态是「少量大段 + 若干小段」的金字塔，而不是所有段一样大。

**实测默认值**：

| 常量 | 默认值 | ES 侧 setting |
|------|-------|--------------|
| `maxMergeAtOnce` | **10** | `index.merge.policy.max_merge_at_once` |
| `segsPerTier` | **10.0** | `index.merge.policy.segments_per_tier` |
| `floorSegmentBytes` | **2 MB** | `index.merge.policy.floor_segment` |
| `maxMergedSegmentBytes` | **5 GB** | `index.merge.policy.max_merged_segment` |
| `deletesPctAllowed` | **20.0 %** | `index.merge.policy.deletes_pct_allowed` |
| `forceMergeDeletesPctAllowed` | **10.0 %** | `index.merge.policy.expunge_deletes_allowed` |
| `targetSearchConcurrency` | **1** | 无（随副本数变化） |
| `DEFAULT_NO_CFS_RATIO` | **0.1** | 段小于 `maxCFSSegmentSize` 的 10% 时不打包成复合文件（ES 侧 `index.compound_format` 默认 `1gb`） |

辅助函数也一并实测：`floorSize(bytes) = max(floorSegmentBytes, bytes)`；`getMaxAllowedDocs(totalMaxDoc, totalDelDocs) = (totalMaxDoc − totalDelDocs + targetSearchConcurrency − 1) / targetSearchConcurrency`（把存活文档数摊到并发份数上）。

### 20.2 候选合并怎么打分：skew × size^0.05 × nonDelRatio²

从排好序的段列表里**贪心**取窗口作为候选，逐个打分，**取分数最小者**（`score()` 实测）：

```
对候选段集合 C：
  totAfterMergeBytes         = Σ sizeInBytes(i)                    // 估算输出大小
  totAfterMergeBytesFloored  = Σ floorSize(sizeInBytes(i))
  totBeforeMergeBytes        = Σ info.sizeInBytes(i)               // 含已删空间的原始大小

  if hitTooLarge:              // 已撞到 5 GB 上限，此次合并不会「级联」，skew 无所谓
      skew = 1 / mergeFactor
  else:
      skew = floorSize(C 中最大段的 sizeInBytes) / totAfterMergeBytesFloored

  nonDelRatio = totAfterMergeBytes / totBeforeMergeBytes           // ≤ 1，回收删除的比例
  mergeScore  = skew
              × (totAfterMergeBytes) ^ 0.05
              × (nonDelRatio) ^ 2
  # 取 mergeScore 最小者执行
```

三个因子的意图（源码注释）：

- **`skew`（强偏好）**：`1/numSegs` 最好（完全均衡），`1.0` 最差（一个巨无霸吃掉整个合并）。skew 接近 1 意味着「反复重写最大段」，会产生 **`O(N²)` 级合并成本**。
- **`size^0.05`（弱偏好）**：只轻轻偏好小合并。指数刻意取 0.05 而非更大，是为了**避免为了躲大合并而去做糟糕的小段合并**。
- **`nonDelRatio²`（强偏好回收删除）**：平方项让「回收删除」的候选获得压倒性优势。

### 20.3 反退化护栏与 forcemerge

实测中有三道护栏，都不是打分能覆盖的：

1. **「输出至少要比最大输入段大 50%」**：`bytesThisMerge < maxCandidateSegmentSize.sizeInBytes × 1.5` 且该最大段删除率未超阈值时，**直接放弃这次候选**。注释写得很直白：不加这道护栏就会出现「合并结果只比最大段大一点点 → 下轮又被选上 → `O(N²)`」的病态路径。
2. **「单段零删除不合并」**：`candidate.size() == 1 && delCount == 0` 直接跳过（forcemerge 循环里会反复撞到这个条件）。
3. **「撞到 5 GB 上限的合并同一时刻只允许一个在跑」**：`(!hitTooLarge || !maxMergeIsRunning)`，避免并发多个超大合并把 IO 打满。

`findForcedMerges`（`index.optimize.max_num_segments` / ILM 的 forcemerge）走另一条路径：直接按「把索引压到 ≤ maxSegmentCount 个段」反推合并集合，仍复用上面的候选构造与打分。

ES 侧的合并执行：`server/src/main/java/org/elasticsearch/index/engine/InternalEngine.java`（Lucene `IndexWriter` 的 ES 包装，持有 MergeScheduler）。

### 20.4 ES 的接线：8 个设置 + TSDB 专用策略

Lucene 的 `TieredMergePolicy` 是通用策略，但**ES 在 `server/src/main/java/org/elasticsearch/index/MergePolicyConfig.java` 里做了一层重要改写**，这是与「直接用 Lucene 默认」最容易踩的差异：

| ES setting | 默认值 | 作用 |
|------------|-------|------|
| `index.merge.policy.type` | `UNSET` | `TIERED` / `TIME_BASED` / `LOG_BYTE_SIZE` |
| `index.merge.policy.max_merge_at_once` | **10** | 单次合并的段数上限 |
| `index.merge.policy.max_merge_at_once_explicit` | — | 显式设置后的只读标记（防止被静默调回） |
| `index.merge.policy.max_merged_segment` | **5 GB** | 合并产物上限 |
| `index.merge.policy.segments_per_tier` | **10.0** | 每层允许段数 |
| `index.merge.policy.floor_segment` | **2 MB** | 小段下限 |
| `index.merge.policy.deletes_pct_allowed` | **20.0 %** | 触发合并的删除率 |
| `index.merge.policy.expunge_deletes_allowed` | **10.0 %** | forcemerge 纯删段合并的删除率阈值 |
| `index.merge.policy.merge_factor` | **32**（仅 TIME_BASED） | 见下 |
| `index.merge.enabled` | `true` | 可关闭自动合并 |
| `indices.merge.policy.max_time_based_merged_segment` | **100 GB** | TSDB 的合并产物上限 |

**两个分层参数是互斥的**（实测 `MergePolicyConfig` 的 setter 注释）：`setSegmentsPerTier` 只写进 `tieredMergePolicy`（注释原文「LogByteSizeMergePolicy ignores this parameter, it always tries to have between 1 and `merge_factor` − 1 segments per tier」），`setMergeFactor` 只写进 `timeBasedMergePolicy`（注释原文「TieredMergePolicy ignores this setting, it configures a number of segments per tier instead, which has different semantics」）。**所以 `segments_per_tier` 只对 `TIERED` 生效、`merge_factor` 只对 `TIME_BASED` 生效**，两个都调不会叠加。

另外 ES 在构造时会做一次**隐式钳制**：`adjustMaxMergeAtOnceIfNeeded()` —— 若 `segments_per_tier < max_merge_at_once`，就把 `max_merge_at_once` 降为 `floor(segments_per_tier)`（下限 2），并打一条 debug 日志（对应 Lucene `TieredMergePolicy#setMaxMergeAtOnce` 的约束）。

**`TIME_BASED` 分支值得单独记住**：ES 对**时序索引（`index.mode: time_series`）**不用 `TieredMergePolicy`，而改用 Lucene 的 `LogByteSizeMergePolicy`（**只合并相邻段**），并把 `mergeFactor` 默认从 Lucene 的 10 改成 **32**、把产物上限从 5 GB 提到 **100 GB**。实测注释给出的理由：时序数据按时间顺序摄入，**只合并相邻段 ⇒ 各段的 `@timestamp` 区间天然不重叠**，时间范围查询时大多数段要么整体命中要么整体不命中，即使段数很多也很高效；而人为抬高上限是为了**减少合并次数、降低写放大**。

源码：`server/src/main/java/org/elasticsearch/index/MergePolicyConfig.java`、`server/src/main/java/org/elasticsearch/index/IndexSettings.java`（`getMergePolicy(boolean isTimeBasedIndex)` 是唯一的取用入口）。

### 20.5 doc_values / _source / stored fields / script_fields 的分工

这是「同一份数据为什么要存四遍」的答案（ES 侧实现路径已逐个在 8.19 核实）：

| 存储形式 | 物理位置 | 解决的问题 | 代价 | ES 侧路径 |
|---------|---------|-----------|------|----------|
| **倒排索引** | `.tim` / `.doc` / `.pos` / `.pay` | 「哪些文档含 term t」（全文检索 + 短语 + 高亮） | 写放大最大；聚合/排序用不了 | `FieldMapper.java` / `KeywordFieldMapper.java` |
| **doc_values**（列存） | `.dvd` / `.dvm` | 「文档 N 的 price 是多少」（排序、聚合、脚本、runtime field） | 额外一份列存；**不可搜索** | `server/src/main/java/org/elasticsearch/index/codec/perfield/XPerFieldDocValuesFormat.java`、`server/src/main/java/org/elasticsearch/index/mapper/BlockDocValuesReader.java` |
| **stored fields** | `.fdt` / `.fdx` / `.fdm` | 「按需取回原文的某几个字段」（`fields` 参数、高亮取原串、reindex） | 与 `_source` 重复存一份时纯属浪费 | `server/src/main/java/org/elasticsearch/search/fetch/subphase/FetchSourcePhase.java`、`FetchFieldsPhase.java` |
| **`_source`** | **就是 stored fields 里的一项** | 文档原始 JSON（重建文档、reindex、update） | 大；重复存储大字段 | `server/src/main/java/org/elasticsearch/index/mapper/SourceFieldMapper.java` |
| **script_fields** | **不落盘** | 「按需用 painless 现算一个值」 | **每篇命中文档执行一次脚本**，无批量、无向量化 | `server/src/main/java/org/elasticsearch/search/fetch/subphase/ScriptFieldsPhase.java` |

三条容易被忽略的机制：

1. **doc_values 的五种类型**：`XPerFieldDocValuesFormat` 按 `Numeric` / `Binary` / `Sorted` / `SortedNumeric` / `SortedSet` 五类分别路由到不同 codec，**并把同类的字段分组后再统一 merge**（源码里按 consumer 身份做 identity map 分组）。
2. **`keyword` 的 doc_values 默认开、`text` 默认不开**：这是 ES 里「能用聚合/排序就别用 text」的根本原因 —— 不是语义问题，是物理存储问题。另有 `server/src/main/java/org/elasticsearch/index/fielddata/StoredFieldSortedBinaryIndexFieldData.java`：当某字段的 doc_values 不可用时，可以**降级到 stored fields 做排序**，代价是要解压原文。
3. **`script_fields` 的真实代价**：它在 **fetch 阶段**执行，**只有命中的那 `size` 篇文档**会跑脚本。所以「用 script_fields 代替 doc_values 排序」在 `size=10` 时看着便宜，一旦要排序全部命中文档就会退化成全量脚本执行 —— 这是 ES 官方反复强调「别用脚本代替 doc_values 做排序/聚合」的实现层原因。

### 20.6 合并策略的复杂度与取舍

| 环节 | 复杂度 | 说明 |
|------|--------|------|
| 选段排序 | `O(S log S)` | `S` = 段数 |
| 允许段数计算 | `O(S + log_{mergeFactor}(总字节/floorSegmentBytes))` | 几何分层的循环次数 ≈ 段数位数的对数级 |
| 候选构造 + 打分 | `O(S × mergeFactor)` | 每个起点窗口最多装 `mergeFactor` 个段 |
| 一次合并的 IO | `O(Σ 输入段字节)` | 重写全部 posting / doc_values / stored fields |
| 全生命周期写放大 | 稳态约 `O(log_{mergeFactor}(总字节/floorSegmentBytes))` 级别的重写层数 | 因为几何分层保证每放大 10 倍只需多容纳 10 个段 |
| TSDB（`TIME_BASED`） | 相邻合并，时间区间天然不重叠 | 范围查询友好、合并次数少（`mergeFactor = 32`、上限 100 GB） |

---

## 二十一、参考资料

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
