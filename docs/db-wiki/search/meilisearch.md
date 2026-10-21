# Meilisearch Wiki（Rust 极简全文搜索引擎）

> 参考版本：Meilisearch v1.x（commit `main`），**Rust + Actix Web** 实现的轻量级 **RESTful 即时搜索引擎**，主打 **1 分钟内集成 + 极简 HTTP API + typo tolerance + 毫秒级响应**。本 wiki 覆盖搜索类型核心：倒排索引、相关性排序、过滤与 Faceting、即时索引。

## 一、项目定位

Meilisearch 是 **Meilisearch SAS** 开源的 **轻量级、即时、零配置全文搜索引擎**，由创始人 **Quentin de Quelen / Clémentine Bégue / Thomas Pays（法国）** 发布，基于 **Rust + Actix Web + Tantivy / 自研 CRoaring 索引**（后期部分采用，但 Meilisearch 早期为自研）。它把 **"即时增量索引（1ms 内可查询）+ typo tolerance（拼写容忍）+ 高相关性 BM25 + 全文 + 高亮 + Faceting"** 整合为一站式服务，对小型 / 中型应用做到 **5 行代码集成**。

- **痛点**：
  - Elasticsearch / OpenSearch 太重，单实例 ~1GB RAM 才能跑；
  - Solr 配置繁琐，集群管理复杂；
  - Algolia 商业 SaaS 自托管不可控；
  - 关系数据库 `LIKE '%foo%'` 无法模糊匹配 + 没有相关性排序；
  - 需要 **电商商品搜索、SaaS 文档检索、本地文件搜索** 等 **毫秒级响应 + 拼写纠正 + 即时索引更新** 场景；
- **典型用户**：电商商品搜索、SaaS 全文检索、博客评论搜索、Confluence-like 内容搜索、AI Knowledge Base；
- **核心口号**：**"Lightning-fast search, made easy"** + **"Search at the speed of thought"** + **"1 minute integration"**。

## 二、发展历史与社区

- **起源**：2018 年法国里昂三个工程师用 Rust 重写初版（先是 Node/ES）；
- **里程碑**：
  - 2018-07：v0.1 开源，基于 Rust + Lua 分词；
  - 2019-09：v0.16 引入 typo tolerance；
  - 2020-01：v0.20 引入 **filter & faceting**；
  - 2020-07：v0.21 引入 **sort + 内嵌索引**；
  - 2021-05：v0.22 引入 **stop-words + 同义词**；
  - 2022-07：v0.28 引入 **GeoSearch + Multi-search**；
  - 2022-11：v0.30 引入 **Puffin Reports（Admin 报表）**；
  - 2023-01：v1.0 GA，**性能 + 可靠性稳定**；
  - 2023-04：v1.1 内置 **Embedder + vector search（向量检索）**；
  - 2023-10：v1.5 引入 **AI Search / Hybrid Search（BM25 + 向量混合）**；
  - 2024-04：v1.6 引入 **Auto-Batching of Tasks、Network-Policy**；
  - 2024-2026：v1.8+ 持续强化 **Search Relevance（LLM reranking）+ Loaders + Concurrency**；
- **维护状态**：极活跃，**GitHub ~45k stars**，月级小版本；
- **社区**：Meilisearch Cloud、官方文档、Forum、Discord ~10k 人、GitHub Discussions 活跃。

## 三、许可与商业限制

- **许可证**：**MIT License**（v0.x + v1.x 都完全 MIT）；
- **商业版本**：
  - **Meilisearch Cloud**（官方 SaaS，按 search request + data size 计费）；
  - **Meilisearch Enterprise**（截至目前未真正推出，主流是 OSS）；
- **生态关系**：
  - 与 **Algolia**（直接对标 SaaS 搜索）形成竞争；
  - 与 **Algolia DocSearch**（开源文档搜索 SDK）紧耦合，提供官方 DocSearch v3；
  - 与 **Hugging Face / Cohere / OpenAI** 通过 Embedder 集成；
  - 与 **Tantivy**（Rust 的 Lucene 实现）部分代码复用（如分词、prefix search）。

## 四、业界主要使用方与案例

- **超大型用户**（公开）：
  - **GitHub**：Issues Search（部分场景）；
  - **Docker Hub**：镜像搜索；
  - **Mozilla**：MDN 部分搜索；
  - **Algolia DocSearch v3**：Docs 文档站搜索（替代 v2 / Algolia SaaS）；
  - **Laravel / Rails / Django / Spring / FastAPI** 多个语言生态集成 SDK；
- **典型场景**：电商商品搜索（<10M SKU）、SaaS 全平台文档搜索、客服知识库、AI RAG 检索层；

## 五、一句话对比定位

> Meilisearch 是 **"Rust + MIT + 极简集成 + BM25 typo tolerance + 毫秒响应 + 即时增量索引 + 拼写纠正 + Faceting + AI 混合检索"** 的即时全文搜索引擎；与 **Elasticsearch / OpenSearch（重量级）**、**Typesense（C++ 即时）**、**Algolia（SaaS 商业）**、**Tantivy（嵌入式 Rust）**、**Quickwit（云原生对象存储）** 构成搜索引擎阵营。Meilisearch 的差异化是 **"极简集成 + 毫秒响应 + Rust 性能 + MIT 协议"**。

## 六、整体架构

```
┌─────────────────────────────────────────────────────────────────────────┐
│                  客户端层                                                 │
│  - REST API（7700，默认）                                                │
│  - 官方 SDK：meilisearch-js / -python / -go / -java / -ruby / -php /    │
│    -rust / -swift / -dart / -.NET                                       │
│  - DocSearch v3（前端即时搜索）                                         │
│  - 前端即用 SDK：instant-meilisearch（React / Vue / Svelte）              │
└─────────────────────┬───────────────────────────────────────────────────┘
                      │ HTTP/JSON
┌─────────────────────▼───────────────────────────────────────────────────┐
│                  Actix Web 服务器（Tokio runtime）                       │
│  crates/meilisearch/src/                                                 │
│  ├─ routes/：路由分发（document/search/index/settings/...）              │
│  ├─ handlers/：请求处理                                                │
│  ├─ processors/：任务入队 + 处理                                        │
│  └─ search/：搜索核心                                                  │
└─────────┬───────────────────┬──────────────────────┬───────────────────┘
          │ search            │ index                │ task scheduler
┌─────────▼───────┐ ┌────────▼──────────┐ ┌────────▼──────────┐
│  Index Module  │ │  Indexing Engine │ │  Task Scheduler  │
│  - update_index│ │  - 文档解析       │ │  - 任务队列       │
│  - search      │ │  - 写索引文件     │ │  - 状态机         │
│  - settings    │ │  - typo 设置生效  │ │  - UI 报表         │
└────────┬───────┘ └────────┬──────────┘ └────────┬──────────┘
         │                  │                     │
         │   meilisearch-types/                  │
         │   ├─ owned_by   (Schema)             │
         │   ├─ settings   (IndexConfig)        │
         │   └─ documents  (DocumentBatch)      │
         │                                       │
┌────────▼──────────────────────────────────────────────────────────────┐
│  索引层（自定义 + 部分 Tantivy）                                       │
│  crates/index-scheduler/src/                                            │
│  ├─ RoaringBitmap（CRoaring）：posting list 的压缩位图                │
│  ├─ Bucket Map / HASH/AREN：术语定位 + Posting 索引                  │
│  ├─ LMDB（folly-bucketed）：KV 元数据 + 索引段引用                   │
│  └─ Tantivy（crates/segment-loader/）：可选的 segment 读取层        │
└────────┬──────────────────────────────────────────────────────────────┘
         │
┌────────▼──────────────────────────────────────────────────────────────┐
│  持久化层                                                              │
│  ├─ data.ms（自定义 LMDB-backed 索引）                                 │
│  ├─ data/（倒排索引段：.cdt / .cid / .cm / .pos / .idx）              │
│  ├─ state/（meilisearch-types/settings）                              │
│  ├─ updates/（待处理 task 队列）                                      │
│  └─ snapshots/（snapshot 与 dump 导出目录）                           │
└──────────────────────────────────────────────────────────────────────┘
```

### 关键设计取舍

- **自研 + Tantivy 混合**：早期 Meilisearch 完全自研（RoaringBitmap + LMDB），2022+ 部分借用 Tantivy（如 segment loader）但保持轻量；
- **任务队列 + 异步处理**：所有写操作（文档 upsert、settings、index）进入 task queue，**毫秒级返回 taskId**，后台 scheduler 异步执行；
- **即时索引（refresh）**：内置默认 `refreshInterval`（5 分钟可调，立即刷新通过 task 显式触发）；
- **拼写纠正（typo tolerance）**：基于 BK-tree + Damerau-Levenshtein 距离；
- **极简 API**：单一 HTTP API（`/indexes/:uid/search`、`/indexes/:uid/documents`），**无 DSL/无 PPL**，几乎无学习成本；
- **依赖管理**：Tokio 异步运行时 + Actix Web HTTP server。

## 七、存储引擎（索引层 + RoaringBitmap）

### 7.1 索引段结构

```
data.ms (LMDB)
├─ metadata    (index settings, doc count, last update, ...)
├─ fields      (字段映射：searchable / filterable / sortable)
├─ postings    (RoaringBitmap per term)
├─ dictionary  (term → posting)
├─ documents   (docID → JSON)
└─ facets      (facet 分布统计 + RoaringBitmap)
```

- **RoaringBitmap**（`crates/roaring/src/`）：分块 65536 Roaring Bitmap，运行时常量时间集合运算；
- **DocID 序列**：单段内 docId 单调递增，跨段拼接到 RoaringBitmap；
- **数据文件**：`.cid`（chunk identifier）、`.pos`（positions）、`.cdt`（chunk data）、`.cm`（chunk metadata）、`.idx`（index）；
- **Tantivy 部分借用**：`crates/segment-loader/` 提供 Tantivy segment 反查能力。

### 7.2 写入流程

```
POST /indexes/products/documents
  → routes/indexes/documents.rs（解析）
  → updates::IndexOperation::DocumentAddOrReplace
  → task scheduler 入队（taskId 返回）
  → 后台 IndexProcessor：
      ├─ 解析文档 → 字段提取（schema）
      ├─ 分词（tokenizer 列表 + stop words）
      ├─ 计算 BK-tree 节点（typo detection）
      ├─ 写 postings（RoaringBitmap）
      └─ lmdb commit + 持久化
```

源码位置（参考）：
- `crates/meilisearch/src/updates/index_documents.rs`：文档索引主路径；
- `crates/milli/src/update/index_documents/`：索引核心逻辑。

### 7.3 段管理（Segment Merge）

- 多段并行存在，按 `merge-policies.yaml` 触发合并；
- 段合并策略与 Lucene TieredMergePolicy 类似，定期减少段数；
- 段切换是 atomic，旧段写入后原子替换。

## 八、索引机制（**搜索类型重点展开**）

### 8.1 倒排索引（Posting List + Term Dictionary + RoaringBitmap）

```
            Segment
            ┌──────────────────────────┐
            │  Term Dictionary (ART)   │  ← 紧凑可缓存
            │  term "phone" → [..]     │
            └────────────┬─────────────┘
                         │
            ┌────────────▼─────────────┐
            │  Posting List (Roaring)  │  ← RoaringBitmap 压缩位图
            │   chunk 0 (size 65536)  │
            │   bitmap (bits)          │
            └────────────┬─────────────┘
                         │
            ┌────────────▼─────────────┐
            │  DocID → JSON（Hash）    │
            └──────────────────────────┘
```

- **Posting List**：RoaringBitmap（兼容 Apache Impala 的位图），比传统 Lucene block-based 压缩位图更紧凑；
- **Term Dictionary**：紧凑 hash + ART（adaptive radix tree）的中间结构（部分实现借自 `milli` 子 crate）；
- **位置信息**：用于 phrase query 与高亮。

### 8.2 相关性排序（BM25 默认）

- **默认相关性**：BM25（与 Lucene 同公式）；
- **排序**：
  - `sort` 参数：支持 `field:asc` / `field:desc`；
  - `ranking-score` 控制字段；
  - `ranking-rules`：可配置规则权重：`words`（field 权重）、`typo`、`proximity`、`attribute`（字段顺序）、`sort`、`exactness`、`custom`；
- **Word weighting**：`searchableAttributes` 顺序（一级字段权重 > 二级 > 三级）；
- **Custom Ranking Score**：通过 `expression` 字段，可写表达式。

源码：`crates/milli/src/search/query.rs`、`crates/milli/src/score.rs`。

### 8.3 typo tolerance（拼写纠正）

- **算法**：基于 **BK-tree + Damerau-Levenshtein 距离**：
  - 索引阶段计算每个 term 的 BK-tree；
  - 搜索阶段对每个 query token 做近邻搜索；
  - 距离阈值可调整：term 长度 ≤5 → 1 typo；长度 ≥8 → 2 typos；
- **配置**：`typoTolerance { enabled, minWordSizeForTypos, typosByWord }`；
- **源码**：`crates/milli/src/criterion/best_typo_position.rs`、`crates/milli/src/levenshtein.rs`。

### 8.4 过滤与 Faceting（Filter + Post-Filter 模型）

#### 8.4.1 Filter

- Pre-Filter（强制）：search 前先按 filter 收缩 docId set；
- 性能：filter 必须基于 `filterableAttributes`（倒排），否则扫描全集；

```json
POST /indexes/products/search
{ "q": "phone", "filter": "category = 'electronics' AND price < 500" }
```

#### 8.4.2 Faceting

- 类似于 ES aggregation，分桶统计；
- 支持 `facets`（`facetsDistribution`），按字段聚合；
- 实现：`crates/milli/src/search/facet/mod.rs`。

### 8.5 排序与多匹配类型

- `match()`：`=`、`!=`、`contains`、`notContains`、`startsWith`；
- **Geo**：geoBoundingBox、geoRadius、geoPolygon（按 geoPoints 字段）；
- **Range**：`to / from / <= / >= / BETWEEN / IS EMPTY / EXISTS`。

### 8.6 Embedder 与向量检索（v1.1+）

- **生成式**：`embedders` 配置指定 type（`userProvided` / `rest` / `huggingFace` / `ollama` / `openAi`），生成向量存于 `_vectors` 字段；
- **混合搜索（Hybrid Search）**：BM25 + 向量双路召回，再 RRF 融合；
- **API**：

```json
POST /indexes/products/search
{
  "q": "wireless earphones",
  "hybrid": {
    "embedder": "default",
    "semanticRatio": 0.5
  }
}
```

源码：`crates/milli/src/vector/`、`crates/meilisearch-types/src/embeddings.rs`。

## 九、查询 / 计算 / 接口层（**搜索类型重点展开**）

### 9.1 DSL 设计

#### 9.1.1 主 API（极简）

```http
POST /indexes/products/search
{
  "q": "wireless phone",
  "limit": 20,
  "offset": 0,
  "filter": "category = 'audio'",
  "sort": ["price:asc"],
  "facets": ["brand", "tags"],
  "attributesToHighlight": ["name", "description"],
  "attributesToRetrieve": ["id", "name", "price"],
  "showMatchesPosition": true,
  "matchingStrategy": "all"  // 或 "last"（默认）
}
```

#### 9.1.2 Multi-Search（多索引 / 多查询）

```http
POST /multi-search
{
  "queries": [
    { "indexUid": "products", "q": "phone" },
    { "indexUid": "blog_posts", "q": "rust" }
  ]
}
```

源码：`crates/meilisearch/src/routes/multi_search.rs`。

#### 9.1.3 Federated Search（多索引联查）

- 通过 `federation` API：v1.10+ 提供；
- 在多个索引并行搜索并合并结果；
- 源码：`crates/meilisearch/src/federation/`。

#### 9.1.4 类似 Lucene Query String

- Meilisearch 不支持 ES 风格的 query_string，但 `q` 字段支持单 token + 多词 + 短语引号 `"..."`；
- 短语查询使用 `"hello world"`；
- 排除用 `hello -world`；
- 字段限定：`title:hello`。

### 9.2 聚合（Faceting）

- **唯一支持的"聚合"**：
  - `facets`：列出字段所有 bucket + count；
  - `facetStats`：facet 数值字段的 min/max；
- 与 ES 的 bucket/metric/pipeline aggregation 不可比，**仅等价于 terms aggregation**。

源码：`crates/milli/src/search/facet/`、`crates/meilisearch-types/src/settings.rs`。

### 9.3 多租户搜索

#### 9.3.1 Tenant Token（Multi-Tenant）

- API：生成 JWT token 嵌入 search rules（filter + sort + 选择 attribute）；
- 用于 SaaS 多租户搜索；
- 源码：`crates/meilisearch/src/tenant_tokens/`、`crates/key` 密钥生成。

#### 9.3.2 Index Alias

- `POST /indexes/{uid}/settings` 配置别名；
- 类似 ES 的 `_aliases`。

#### 9.3.3 Index Swap

- **Index Swap（v1.12+）**：把两个索引互换（原子化），用于 **zero-downtime reindex**。

源码：`crates/milli/src/update/index_documents/`。

### 9.4 客户端生态

- **官方 SDK**：JS / Python / Go / Java / Ruby / PHP / Rust / Swift / Dart / .NET（`@meilisearch/instant-meilisearch` InstantSearch.js 适配）；
- **DocSearch**：替代 Algolia DocSearch 的开源版；
- **Django / Laravel / Rails / Fastify** 多个语言 ORM 集成。

## 十、事务与一致性

- **事务模型（极弱）**：
  - **单文档原子性**：通过 LMDB 事务保护单个 docId 操作；
  - **跨文档**：无跨 doc 事务；
  - **写入一致性**：默认 task queue 至少一次，部分 task 失败时 retry；
  - **并发**：Actix Web 单实例 = Tokio runtime；多实例共享 LMDB 仓库（乐观锁）；
- **任务保证**：
  - `enqueuedAt`、`startedAt`、`finishedAt`、`failedAt`；
  - 失败保留 `error.code`、`error.message`、`error.link`，可在 UI 重试；
- **源文档不可变**：新增 docId 版本，旧 docId 仍存在直到 merge / GC。

源码：
- `crates/meilisearch-types/src/task.rs`：任务模型；
- `crates/meilisearch/src/updates/task_store.rs`：任务存储。

## 十一、复制与高可用

- **集群模式（meilisearch v1.x Cloud Tier）**：
  - 原生 **multi-node cluster**（社区 license 下可用）；
  - Leader-Follower 模式：Leader 处理写，Followers 处理读；
  - 跨节点同步：基于 Raft-based task coordination；
- **共识实现**：核心模块基于 `raft-rs` 部分改进；
- **快照（snapshot）**：
  - `POST /snapshots` 创建本地快照；
  - 上传到 S3/GCS/Azure 做异地备份；
  - 源码：`crates/meilisearch/src/snapshots/`；
- **水平扩展**：单实例不能 cluster 横向扩展（社区版）—— **Cloud tier 才支持多节点集群**。

## 十二、运维与可观测性

- **监控**：
  - `_health` 端点；
  - `_stats`：索引数 + 文档数 + 内存占用 + 任务数；
  - `_tasks`：任务列表（含失败状态）；
  - **Prometheus 集成**：`meilisearch-prometheus-exporter`（社区）；
- **可观测生态**：
  - OpenTelemetry 兼容：1.x 起支持 tracing；
  - 与 Grafana：原生 Puffin 报表 → Grafana DataSource；
- **Telemetry 关闭**：`MEILI_NO_ANALYTICS=true` 关停；
- **Web UI / Dashboard**：内置 Mini-Dashboard（`localhost:7700/ui`）；
- **备份恢复**：
  - `POST /snapshots` 本地快照；
  - `meilisearch-dump` + `meilisearch-import`（基于 NDJSON）。

## 十三、性能与基准

- **官方 benchmark**（v1.x）：
  - 单实例 100k-1M 文档：毫秒级响应（<50ms P99）；
  - 中等数据集（10M 文档）+ typo tolerance + filter + sort + facet：仍 <100ms；
- **vs Elasticsearch**：
  - Meilisearch 在小数据 + typo + 易集成场景领先；
  - 在聚合、多租户、超大规模场景 ES 更强；
- **vs Algolia**：
  - 功能差异：Meilisearch 自带 Hybrid Search，但 Algolia 在多语言 NLP/AI ranking 强；
  - 部署：Meilisearch 自管可控；
- **vs Typesense**：性能接近、易用性差，但 Meilisearch 拼写纠正 / Hybrid 更优；
- **vs Tantivy**：Meilisearch 是服务化版本，Tantivy 是嵌入式；Meilisearch 用 milli crate 借鉴 Tantivy；
- **优化建议**：
  - **ranking-rules 重新排序**：把 `typo` 调整为后置；
  - **大索引分片**：分库拆索引（`articles_2024`、`articles_2023`）；
  - **filter 必须注册**：未注册字段做 filter 会报错；
  - **停用词 / 同义词**：优化语义；
  - **Geo / faceting 数据**：放入 `filterableAttributes` 列表。

## 十四、生态与集成

- **客户端 SDK**（8+ 语言，完整覆盖前端 & 后端）；
- **InstantSearch.js / React / Vue / Svelte**：前端即用组件；
- **DocSearch v3**：替代 Algolia DocSearch 的开源版；
- **Hugging Face + Cohere + OpenAI**：通过 embedders 集成；
- **ORM 集成**：
  - Django / Laravel / Rails 等提供 ActiveRecord / Eloquent 风格封装；
- **容器化**：官方 Docker image、Helm Chart；
- **CI / CD**：通过 API 集成；
- **生产建议**：
  - 单实例数据 < 10GB；
  - 使用 Cloud tier 处理 > 10M 文档；
  - 集群（Beta）截至目前仅 Cloud tier 支持；
- **AI / RAG**：与 LangChain / LlamaIndex 提供 `Meilisearch` retriever。

## 十五、学习路径

### 15.1 入门（1-2 周）

1. **官方教程**：`meilisearch.com/docs/learn/getting_started/`；
2. **跑通本地**：
   - `curl -L https://install.meilisearch.com | sh` 启动（或 `docker run`）；
   - 提交 docs：`POST /indexes/products/documents`；
3. **即时集成**：使用 `instant-meilisearch` 在 React 5 行接入；
4. **了解 ranking rules 与 typo**：`ranking-rules` 默认与可配置项。

### 15.2 进阶（3-4 周）

1. **必读源码**：
   - `crates/milli/src/`：核心搜索引擎（milli = "MilleFEUILLE" 法语）；
   - `crates/milli/src/update/index_documents/`：写入逻辑；
   - `crates/milli/src/search/query.rs`：搜索主路径；
   - `crates/milli/src/criterion/`：相关性策略；
   - `crates/milli/src/vector/`：向量索引；
   - `crates/meilisearch/src/routes/`：HTTP 路由；
   - `crates/meilisearch/src/updates/task_store.rs`：任务队列；
2. **RoaringBitmap**：`crates/roaring/src/`（位图压缩）；
3. **LMDB Fork**：`crates/heed/src/`（Rust LMDB 封装）；
4. **Tokenizer 体系**：`crates/charabia/src/`（多语言分词）。

### 15.3 高级（长期）

1. **Hybrid Search 实现细节**：BM25 + 向量 RRF；
2. **多实例集群**：单节点 → 多节点；task group coordination；
3. **Embedder**：调用外部 embedding API；
4. **Secondary Index**（segments）：segment swap 优化。

### 15.4 推荐资源

- **官方教程**：Guides、Recipes；
- **博客**：Meilisearch 官方博客、Search Engineering 系列；
- **论文**：RoaringBitmap 论文（Chambi et al., 2016）、BK-tree 论文（Burkhard-Keller 1973）、Damerau-Levenshtein 距离；
- **社区**：Discord、Forum、GitHub Discussions、HN 讨论。

## 十六、风险与陷阱

1. **单实例内存限制**：单实例容纳 **< 10M 文档 / < 8GB 索引**，超出 OOM；需分索引或多实例；
2. **filter 字段必须预先注册**：未在 `filterableAttributes` 注册的字段用作 filter 会直接 400；
3. **typo tolerance 主题优化**：中文 / 日文等字符集的 typo 纠正效果差，默认仅适合拉丁语系；
4. **task 任务堆积**：高频更新 + 大文档集可能堆满 task 队列（默认 10000），监控 `tasks.pending`；
5. **并发刷新**：高频 `waitForTask` 阻塞 HTTP 连接；
6. **拼写权重难调**：`typo` 排前容易把专业术语误纠，**生产建议调整 ranking-rules**；
7. **缺乏权限模型**：原生无 RBAC，租户权限靠 Tenant Token（仅限制 search 规则）；
8. **可恢复能力弱**：单实例故障丢失 in-memory 索引碎片，恢复需冷 reload；
9. **地理精度**：地理搜索用 WKT，复杂多边形性能差；
10. **嵌入模型锁定**：embedder 配置与索引版本强绑定，升级嵌入模型需 **reindex 整个索引**；
11. **Cloud tier 集群限制**：仅商业版支持多节点集群，自托管只能单实例；
12. **慢查询定位差**：profile API 仍较简单，需借助 flame graph + milli span 调试。

## 十七、与本项目的关联

本项目 `engineering/` **未实现** Meilisearch 兼容引擎，下列可借鉴：

- **RoaringBitmap 压缩存储**：`engineering/src/algo/` 可参考 `crates/roaring` 在 `engineering/src/algo/roaring.c` 增加 Posting List 压缩位图（教学版）；
- **拼写纠正算法**：参考 `crates/milli/src/levenshtein.rs` 在 `engineering/src/algo/spell_correction.c` 实现 BK-tree + Damerau-Levenshtein；
- **任务队列模型**：参考 `crates/meilisearch-types/src/task.rs` 在 `engineering/src/algo/task_queue.c` 简化版实现；
- **极简 HTTP DSL**：参考 `routes/` 的简洁设计，在 `engineering/src/algo/http_search_dsl.c` 教学级实现。

## 十八、Ranking rules 的桶流水线：零分桶排序算法（算法展开）

> **源码基准**：以下路径与行为以 Meilisearch **v1.55.0**（workspace `Cargo.toml` 版本号，实测 commit `8b7c2172b9bf8be7d05f09d7174a8c15449a2c1d`，2026-10-21 的 `main`）为准。本仓库 `reference/search/meilisearch/` 子模块**未 init**（目录内除 `.git` 外 0 个条目），因此一律**只给 canonical 路径、不给行号**；不同小版本的文件布局与默认值可能已漂移。

### 18.1 默认规则序列与「attribute 被拆成两条」

`Criterion` 枚举（`crates/milli/src/criterion.rs`）定义了全部内建排序准则：

```
Words        // 按命中的查询词数量降序
Typo         // 按 typo 数升序
Proximity    // 按查询词之间的距离升序
Attribute    // 「字段排名 + 词位置」的复合准则（已废弃式写法，内部展开为两条）
AttributeRank// 只看字段排名（不看词位置）
WordPosition // 只看词位置（不看字段排名）
Sort         // 运行时按 sortableAttributes 动态排序
Exactness    // 按匹配词与查询词的形近度
Asc/Desc(field) // 自定义排序
```

设置项 `rankingRules` 的 schema 默认值（`crates/meilisearch-types/src/settings.rs`）是：

```json
["words", "typo", "proximity", "attributeRank", "sort", "wordPosition", "exactness"]
```

⚠️ **与 §8.2 的差异**：现有章节列的是 `words / typo / proximity / attribute / sort / exactness / custom`。在 v1.55 中 `attribute` 已拆分为 **`attributeRank`（字段排名）与 `wordPosition`（词位置）** 两条独立准则，且 `Asc(field)` / `Desc(field)` 是内建准则而非 `custom` 表达式。

真正把这些准则实例化成可执行对象的函数是 `get_ranking_rules_for_query_graph_search()`（`crates/milli/src/search/new/mod.rs`），它有两条容易被忽略的「重写规则」：

```
规则一（隐式补 words）：
    遍历用户配置的 rankingRules，一旦遇到 typo / proximity / attribute /
    attributeRank / wordPosition / exactness 且 words 尚未入列，
    就先把 Words 插进去。
    源码注释解释：否则把 attributeRank/wordPosition 放在 words 之前，
    会跳过 Words 做的「丢词」动作，悄悄返回更少的结果。

规则二（attribute 展开为两条）：
    Criterion::Attribute      → push(Fid::new()) ; push(Position::new())
    Criterion::AttributeRank  → push(Fid::new())
    Criterion::WordPosition   → push(Position::new())
    三者互斥，任一出现后其余跳过。

另有：matchingStrategy == All 时直接 words = true，
      因为 All 语义下不会丢词，Words 准则无意义。
```

每条准则实现 `RankingRule` trait（`crates/milli/src/search/new/ranking_rules.rs`），只有四个方法：

| 方法 | 语义 |
| --- | --- |
| `start_iteration(ctx, universe, query, deadline)` | 准备迭代；可能耗时，需检查 deadline |
| `next_bucket(ctx, universe, deadline) -> Option<RankingRuleOutput>` | 返回下一个桶；**返回的 candidates 必须是传入 universe 的子集** |
| `end_iteration(ctx)` | 迭代结束，把控制权交还父准则 |
| `non_blocking_next_bucket(ctx, universe) -> Poll<...>` | 非阻塞版本；默认保守返回 `Pending`（会阻塞） |

`RankingRuleOutput<Q>` 只有三元组：

```
RankingRuleOutput {
    query:      Q,             // 传给下一层准则的查询（对 QueryGraph 而言是收窄后的图）
    candidates: RoaringBitmap, // 下一层准则的 universe
    score:      ScoreDetails,  // 本桶的分数，直接进入最终 rankingScoreDetails
}
```

### 18.2 bucket_sort 主循环：游标 + universe 栈

核心驱动器是 `bucket_sort()`（`crates/milli/src/search/new/bucket_sort.rs`）。它不是「对候选集反复全排序」，而是维护一个**显式栈**：`ranking_rule_universes[i]` 是第 `i` 层准则尚未消费的候选集，`cur_ranking_rule_index` 是栈顶游标，`ranking_rule_scores` 是从栈底到栈顶的分数栈。

```text
bucket_sort(ranking_rules, universe, from, length):
    ranking_rule_universes[0] ← universe
    cur ← 0
    ranking_rule_scores ← []
    ranking_rules[0].start_iteration(universe)
    all_candidates ← universe          // 供 estimatedTotalHits 统计
    cur_offset ← 0
    valid ← []                          // 已确定进入结果的 docid

    while valid.len() < length:
        // (A) 当前层候选已空（或 Skip 模式下只剩 1 个）→ 整桶上浮，无须打分
        if ranking_rule_universes[cur].is_empty()
           or (strategy == Skip and ranking_rule_universes[cur].len() == 1):
            maybe_add_to_results(take(ranking_rule_universes[cur]))
            back!()                       // 弹栈
            continue

        // (B) 向当前层要下一个桶
        next ← ranking_rules[cur].next_bucket(ranking_rule_universes[cur])
        if next is None:
            back!()
            continue

        ranking_rule_scores.push(next.score)

        // (C) 已产出的桶从本层 universe 中扣除，保证「每个候选只被消费一次」
        ranking_rule_universes[cur] -= next.candidates

        // (D) 三种「就地结算」条件：到达最后一条准则 / Skip 模式的单元素桶 /
        //     整桶仍在 from 之前 / 分数低于阈值
        if cur == ranking_rules.len() - 1
           or (strategy == Skip and next.candidates.len() <= 1)
           or cur_offset + next.candidates.len() < from
           or below_ranking_score_threshold(ranking_rule_scores):
            maybe_add_to_results(next.candidates)
            ranking_rule_scores.pop()
            continue

        // (E) 下潜：把桶交给下一层准则继续细分
        cur ← cur + 1
        ranking_rule_universes[cur] ← next.candidates
        ranking_rules[cur].start_iteration(next.candidates, next.query)

    return (valid, all_candidates)

back!():                              // 弹栈宏
    ranking_rule_universes[cur].clear()
    ranking_rules[cur].end_iteration()
    if cur == 0: break                 // 栈底耗尽 → 全部准则跑完
    cur ← cur - 1
    if ranking_rule_scores.len() > cur: ranking_rule_scores.pop()
```

`maybe_add_to_results()` 负责三件事，顺序不可调换：

1. **先应用 `distinct`**：把同一 distinct 值下多余的文档放进 `excluded`，从**所有**层的 universe 与 `all_candidates` 中扣除；
2. `all_candidates |= candidates`（`estimatedTotalHits` 的来源）；
3. **分页裁剪**：桶整体落在 `from` 之前就整桶丢弃；横跨 `from` 就 `split_at(from - cur_offset)` 切开；之后按 `take(length - valid.len())` 截断。

⚠️ 源码里有一处刻意的排序：第 2 步必须**先于**第 3 步的 `drain(..from)`，否则 `estimatedTotalHits` 会少算被跳过的文档。源码注释明确写了这条约束。

### 18.3 为什么这是「零分桶排序」而不是排序

传统做法是取出全部候选、打分、全排序 `O(n log n)`、再切片。桶流水线把这个复杂度换成**按需生成**：

| 提前结算条件 | 语义 | 省掉的代价 |
| --- | --- | --- |
| `cur == len(rules) - 1` | 已到最后一条准则，再分桶无意义 | 省掉最后一层的全部图搜索 |
| `cur_offset + bucket.len() < from` | 整桶都在 `from` 之前 | 省掉深层准则对整桶的打分 |
| `universe.is_empty()` | 上一层已把候选分完 | 直接弹栈 |
| `strategy == Skip and len ≤ 1` | 单元素桶无须排序 | 常数 |
| `deadline.exceeded()` | 超时 | 返回 `degraded = true`，已算部分照常返回 |

深度上界 = 准则条数（默认 7），因此：

- **内存**：`O(Σ_i |universe_i|)`，但每层 universe 是上一层的**子集**，最坏情况才达到 `O(k·n)`；实践中第一条 `words` 准则切出的桶通常远小于全集。
- **时间**：`O(Σ_{i} |命中该桶的代价| ) + O(已产出桶数 × bucket 大小)`，与**前 `from + length` 个结果**所触及的桶数成正比，而不是与 `n` 成正比。这是它能做到「毫秒级」的根本原因。
- **早停代价**：一旦 `valid.len() >= length` 立即退出主循环（第 170 行的 `while` 条件），完全不多算一个桶。

`ScoreDetails` 是分数栈的元素，`global_score(iter)` 把当前分数栈折成一个标量，用于 `rankingScoreThreshold` 判定（`crates/milli/src/score_details.rs`）——分数是**逐层细化的**，不是一次算完的。

### 18.4 图式准则：ranking rule graph 与 cheapest paths

`words` / `typo` / `proximity` / `fid`（字段排名）/ `position`（词位置）五条准则共用同一套机制，实现在 `crates/milli/src/search/new/ranking_rule_graph/`。

**第一步：建图。** `build()`（`ranking_rule_graph/build.rs`）把查询图 `QueryGraph` 转成带权有向图：

```
Edge {
    source_node:  QueryNode,
    dest_node:    QueryNode,
    cost:         u32,                          // 走这条边的代价
    condition:    Option<Interned<Condition>>, // 该准则特有的条件
    nodes_to_skip: SmallBitmap<QueryNode>,      // TermsMatchingStrategy 的「已访问」集合
}
```

边代价由各准则的 `build_edges()` 给出：

| 准则 | 代价 | 出处 |
| --- | --- | --- |
| `words` | 恒为 `0`（只关心是否命中，不加权） | `ranking_rule_graph/words/mod.rs` |
| `typo` | `nbr_typos + base_cost`，其中 n-gram 的 `base_cost = term_ids.len()`（2-gram 折算 1 typo、3-gram 折算 2 typo） | `ranking_rule_graph/typo/mod.rs` |
| `proximity` | `cost ∈ [right_ngram_max, MAX_DISTANCE - 1 + right_ngram_max)` | `ranking_rule_graph/proximity/build.rs` |
| `fid` / `position` | 由 word-fid / word-position 倒排的 docid 集合决定 | `ranking_rule_graph/fid/`、`ranking_rule_graph/position/` |

`typo` 的 `build_edges` 会为 `nbr_typos ∈ 0..=max_typo_cost` 各生成一条边，并用 `clear_one_typo_subset()` / `clear_two_typo_subset()` 表达「恰好 0 个 typo / 恰好 1 个 typo / 恰好 2 个 typo」三种互斥子集。

`proximity` 有一条重要的短路：当 `left_term.positions.end() + 1 != right_term.positions.start()` 时（中间夹着一个被 `words` 准则丢掉的词），直接返回一条**无条件边**，跳过 docid 计算。源码注释举例：`the sun flowers are beautiful` 里 `flowers` 被丢掉后，`sun` 与 `are` 之间不建立 proximity 条件。

**第二步：找最便宜路径。** `cheapest_paths.rs` 的 `PathVisitor` 用深度优先搜索枚举 START→END 的路径，并靠两个预计算把指数复杂度压下来：

1. **Dijkstra 预计算** `all_costs_from_node`：任意节点到 END 的全部可达代价（按代价去重排序）。`next_bucket()` 直接二分出「下一个该算的代价」：`all_costs.iter().find(|c| **c >= cur_cost)`，然后 `cur_cost = c + 1`。
2. **`DeadEndsCache`**（前缀树）：记录「已选条件 → 后续被禁止的条件」。当一条路径算出的 docid 为空时，调用方可更新此缓存，把必然无解的边提前剪掉。

因此复杂度为：

```
无剪枝：O(图中路径条数)                 ← 指数级，实际不可用
有剪枝：O(E · log E  (Dijkstra)  +  K · V)   K = 实际枚举的路径数，V = 访问的边数
每个桶：O(该代价下的 docid 求交/求差)      ← RoaringBitmap 常数摊还位运算
```

### 18.5 proximity 代价函数与位置分桶

`proximity` 准则的边代价最终收敛到一个纯函数（`crates/milli/src/proximity.rs`）：

```text
MAX_DISTANCE = 4

index_proximity(lhs, rhs):
    if lhs <= rhs:  return min(rhs - lhs, MAX_DISTANCE)
    else:           return min((lhs - rhs) + 1, MAX_DISTANCE)
                   ↑ 词序颠倒要额外罚 1：查询词必须按顺序出现

positions_proximity(lhs, rhs):
    if 两者所在字段不同:  return MAX_DISTANCE     ← 跨字段视为最远
    else:                 return index_proximity(字段内下标 lhs, rhs)

path_proximity(path) = Σ 相邻两词的 positions_proximity
```

这个**非对称**的 `+1` 是 Meilisearch proximity 排序的关键：它把「词序颠倒」编码成比「距离远一位」更差，从而天然支持短语语义。复杂度 `O(路径长度)`，且代价被 `MAX_DISTANCE = 4` 截断——超过 4 的距离没有区分度，直接饱和。

位置在倒排层被**分桶**后再存储（`bucketed_position()`，`crates/milli/src/lib.rs`）：

```text
bucketed_position(relative):
    relative < 16       → relative                      // 前 16 位精确保留
    16 ≤ relative < 24  → 24                            // 这一段全部合并成 24
    relative ≥ 24       → 2^ceil(log2(relative))          // 归到「不小于它的最小 2 的幂」
```

对应的倒排库 `word-position-docids: Database<StrBEU16Codec, CboRoaringBitmapCodec>`（键 = 词 + 分桶后位置）。分桶的动机是**压缩位置维度的基数**：长文档里位置可能上万，不分桶会造出海量的极稀疏 posting；分桶后位置种类降到 `O(log 最大位置)` 量级，代价是丢失精度——`exact_attribute.rs` 里有一句注释明说：「位置是分桶存储的，长短语的查询会在这里损失精度」。

还有一个预计算库 `word-pair-proximity-docids: Database<U8StrStrCodec, CboRoaringBitmapCodec>`（键 = 词对的字符数 + 两个词），直接存「这两个词对之间距离 ≤ k 的文档集合」，把 proximity 的边代价计算从 `O(文档数 × 位置数)` 降到一次位图求交。

### 18.6 exactness：唯一一条「三桶即结束」的准则

`exactness`（`crates/milli/src/search/new/exact_attribute.rs`）不走图搜索，它显式产出**三个互斥桶**：

```
桶 1：存在某个字段，其值恰好等于整个查询
桶 2：不在桶 1 中的，存在某个字段，其值以整个查询开头
桶 3：其余（即 universe 减去桶 1、桶 2）
```

`start_iteration` 的构造过程：

1. 收集所有查询词的 `exact_term`（零 typo 精确词），按 `start_term_id` 排序去重；
2. **空洞检测**：若 `start_term_id` 不连续（如 0、2 缺 1），说明某个查询词没有被匹配，直接返回 `Empty` 状态——连桶 1、桶 2 都不必产出，桶 3 就是全部 universe；
3. **位置交集**：对每个词取 `word_position_docids`（键里的位置要过 `bucketed_position`），与 universe 求交，逐词收窄；
4. **字段交集**：对每个 searchable field，把该字段里所有词的 docid 求交，得到每字段的候选集合；再按「起始词相同」与「词数相同」两个条件切成 `start_with_exact` / `exact_word_count` 两张位图。

源码里给了这个反例说明为什么要做第 4 步：查询 `sunflower are pretty`，若 `sunflower` 在字段 A、`are` 在字段 B、`pretty` 在字段 C 分散出现，文档不该算「精确匹配」——所以必须**存在同一个字段**包含全部词。

### 18.7 本节复杂度总表

| 环节 | 数据结构 | 复杂度 |
| --- | --- | --- |
| 桶栈维护 | `Vec<RoaringBitmap>`，深度 = 准则条数 | 每层 `O(1)` 克隆/清空（位图按 chunk 共享） |
| 桶的拆分/合并 | RoaringBitmap 差/交 | `O(|A| + |B|)` 摊还 |
| 准则间切换 | `back!()` 弹栈 | `O(1)` |
| 分页裁剪 | `split_at` + `take` | `O(bucket)` |
| 图建图 | 边表 + `SmallBitmap` | `O(Σ 出边 × 条件数)` |
| 最便宜路径 | Dijkstra + DFS + `DeadEndsCache` | `O(E log E + K·V)` |
| proximity 代价 | 纯函数 | `O(路径长度)`，代价上限 4 |
| 位置分桶 | 无状态函数 | `O(1)`（`log2` 一次浮点运算） |

## 十九、Typo tolerance 的前缀容错：Levenshtein DFA 与 FST 求交（算法展开）

> **源码基准**：Meilisearch **v1.55.0**（commit `8b7c2172b9bf8be7d05f09d7174a8c15449a2c1d`）。子模块 `reference/search/meilisearch/` 未 init，故**只给路径、不给行号**。

### 19.1 typo 数量分档：按 Unicode 码点数而非字节数

分档逻辑集中在 `number_of_typos_allowed()`（`crates/milli/src/search/new/query_term/parse_query.rs`）：

```text
number_of_typos_allowed(ctx) -> 闭包:
    authorize_typos      ← index.authorize_typos()      （typoTolerance.enabled）
    min_len_one_typo     ← index.min_word_len_one_typo()（minWordSizeForTypos.oneTypo，默认 5）
    min_len_two_typos    ← index.min_word_len_two_typos()（minWordSizeForTypos.twoTypos，默认 9）
    exact_words          ← index.exact_words()          （exactAttributes 下建索引的词，FST）

    对每个查询词 word:
        if not authorize_typos
           or word.chars().count() < min_len_one_typo
           or exact_words.contains(word):
            → 0 个 typo
        else if word.chars().count() < min_len_two_typos:
            → 1 个 typo
        else:
            → 2 个 typo
```

三个必须注意的细节：

1. **`chars().count()` 而非 `len()`**——按 Unicode 码点数算，所以 5 个西里尔字母与 5 个 ASCII 字母得到相同的容忍度。源码里有专门的回归测试 `test_unicode_typo_tolerance_fixed` 锁住这一点。
2. **默认值是 5 / 9**，来自 `crates/meilisearch-types/src/settings.rs` 的 `schema_default = json!({ "oneTypo": 5, "twoTypos": 9 })`。
3. **exact words 豁免**：配置在 `exactAttributes` 里的字段，其词会额外写进 `exact_word_docids`，查询时若查询词恰好是 exact word，则**一个 typo 都不给**（避免把专业术语纠正掉）。这正是 §十六 第 6 条「拼写权重难调」的机制来源——不是调 `typo` 规则的位置，而是调 `exactAttributes`。

⚠️ **与 §8.3 的差异**：现有章节写「term 长度 ≤5 → 1 typo；长度 ≥8 → 2 typos」。实测默认阈值是 **5 / 9**（`twoTypos` 是 `>= 9` 才给 2 个，不是 `>= 8`）；且现代 Meilisearch **不使用 BK-tree**，而是 DFA×FST（见 §19.2）。

另外 `MAX_WORD_LENGTH = MAX_LMDB_KEY_LENGTH / 2 = 250`（`crates/milli/src/lib.rs`）：超长词直接**放弃 typo 派生**（`partially_initialized_term_from_word` 提前返回 `max_levenshtein_distance: 0`），因为 LMDB 的 key 有 500 字节上限，词的字节数必须留一半给编码。

### 19.2 编辑距离容错的核心：DFA × FST 求交

传统 BK-tree 是**建索引时**为整个词典建一棵度量树，查询时做近邻搜索——代价随词典规模线性增长，且无法和「前缀匹配」组合。Meilisearch 换了思路：**建索引时什么都不做，只把全部词压成一个 FST；查询时用 Levenshtein DFA 在 FST 上同步走一遍**。

三个零件：

```
① DFA 构造器（全局复用，crates/milli/src/search/mod.rs）
   static LEVDIST0/1/2: LazyLock<LevBuilder> = LevenshteinAutomatonBuilder::new(0|1|2, true)
                                                                        ↑ damerau = true
   build_dfa(word, typos, is_prefix):
       typos == 0 → LEVDIST0, typos == 1 → LEVDIST1, 其余 → LEVDIST2
       is_prefix ? lev.build_prefix_dfa(word) : lev.build_dfa(word)
                     ↑ 前缀 DFA：允许「词的前缀匹配查询词」也算命中

② 词 FST（crates/milli/src/index.rs 的 words_fst()）
   不是具名子库，而是存在 main 库中、键为 "words-fst"（同级的还有
   "words-prefixes-fst" 与 stop words 的 "stop-words"）的一个 fst::Set<Cow<[u8]>>，
   内存映射整个词典的最小 DFA；SearchContext 侧有 db_cache 做一次拷贝复用。

③ 自动机组合子（crates/milli/src/search/fst_utils.rs）
   因 BurntSushi/fst 的 PR#137 未合并，自行实现了 StartsWith / Complement /
   Intersection / Union 四个组合子，每个都是一个新的 fst::Automaton。
```

`find_one_typo_derivations()`（`crates/milli/src/search/new/query_term/compute_derivations.rs`）的完整算法：

```text
find_one_typo_derivations(word, is_prefix):
    dfa    ← build_dfa(word, 1, is_prefix)
    starts ← StartsWith(第一个字符)          // 首字符与查询词相同
    stream ← words_fst.search_with_state(Intersection(starts, dfa))

    while (derived_word, state) = stream.next():
        d ← dfa.distance(state.1)            // DFA 终态携带已消耗的编辑距离
        if d == 0: continue                 // 完全相同的词不算 typo 派生
        if d == 1: one_typo_words.insert(derived_word)
            if len(one_typo_words) >= MAX_ONE_TYPO_COUNT(150): break
```

复杂度分析：

| 项 | 复杂度 |
| --- | --- |
| DFA 状态数 | `O(|alphabet| × |word| × typos)`，一次查询只建一次 |
| FST 遍历 | `O(词典字符数)` 的**上界**，但因 DFA 剪枝 + 首字符约束，实际访问节点数 ≈ 命中词数 × `|word|` |
| 每次状态转移 | `O(1)`（FST 按字节逐层下降，每字节 `O(log 节点度数)`） |
| 派生词收集 | `O(命中数)`，并被 `MAX_ONE_TYPO_COUNT` 硬截断 |
| 相对 BK-tree | 建索引 **O(1)** 额外开销（原为 O(词典 × log 词典)）；查询从「树高 × 度数」变成「DFA×FST 同步下降」 |

这就是为什么现代 Meilisearch 里**索引阶段完全没有 typo 相关计算**——§7.2 写入流程图里画的「计算 BK-tree 节点」是历史实现的残留。

### 19.3 一次求交同时产出 1-typo 与 2-typo 派生

如果为 1 typo 和 2 typo 各走一次 FST，等于把同一个 FST 走两遍。Meilisearch 用一个**布尔分解**把两遍合成一遍（`find_one_two_typo_derivations()`）：

```text
设 first(c) = 派生词的首个字符，first(q) = 查询词的首个字符

关键不变量：
   首字符相同  ⇒  编辑距离 ∈ [1, 2]（不可能是 0，因为 0 已在 zero_typo 处理）
   首字符不同  ⇒  编辑距离 必 = 1      （首字符就已经消耗掉 1 个 typo 了）

于是：
   first  = Intersection( build_dfa(word, 1, is_prefix), Complement(StartsWith(first(q))) )
   second = Intersection( build_dfa(word, 2, is_prefix),                   StartsWith(first(q)) )
   automaton = Union(first, second)

   stream ← words_fst.search_with_state(automaton)
   while (derived_word, state) = stream.next():
       // state 是 Union 的对称差状态，distance 取自当前生效的子 DFA
       if 首字符不同:            two_typos.insert(derived_word)   // 距离必 = 2 分支
       else if distance == 1:   one_typo_words.insert(derived_word)
       else if distance == 2:   two_typos.insert(derived_word)
       两个集合各自达到 MAX_ONE_TYPO_COUNT(150) / MAX_TWO_TYPOS_COUNT(50) 时，
       只停止对应分支的收集，另一分支继续。

   一个 QueryTerm 内同时持有 one_typo: BTreeSet<Interned<String>> 与
   two_typos: BTreeSet<Interned<String>>。
```

复杂度收益：FST 遍历从 `2 × O(词典)` 降到 `1 × O(词典)`，而 DFA 侧的展开反而变小了——`Complement(StartsWith)` 那一支用距离 1 的 DFA（状态数少），`StartsWith` 那一支才用距离 2 的 DFA。

分档（§19.1）与本节的联动：若 `number_of_typos_allowed(word) == 1`，`QueryTerm.max_levenshtein_distance` 就是 1，`compute_fully_if_needed()` 只初始化 `one_typo`；只有 `> 1` 时才初始化两者。这是**按查询词长度懒加载**的扩展性设计。

### 19.4 零 typo 前缀派生的 LMDB 快路径

前缀搜索（`matchingStrategy` 允许，或查询的**最后一个 token**）还要支持「查询词是不完整词」。这里有一个专门的快路径（`partially_initialized_term_from_word()`）：

```text
use_prefix_db = is_prefix
                and (word_prefix_docids.get(word) is not None
                     or (not is_ngram and exact_word_prefix_docids.get(word) is not None))

if is_prefix and use_prefix_db is None:
    find_zero_typo_prefix_derivations(word)     // 慢路径：必须遍历词典

# use_prefix_db is Some(word) 时：整个前缀扩展已经在 word-prefix-docids /
# exact-word-prefix-docids 这两张 LMDB 库里预先算好了，
# 直接命中即可，连 FST 都不用走。
```

慢路径 `find_zero_typo_prefix_derivations()` 也不走 FST，而是走**两条 LMDB 有序游标的归并**：

```text
words       ← word_docids.prefix_iter(word)        // 字典序游标 1
exact_words ← exact_word_docids.prefix_iter(word)  // 字典序游标 2
merge_join_by(words, exact_words, 按词字节序比较)  // 两游标都已按 word 前缀截断且有序
    for 每个 (derived_word, _) in 归并结果:
        if derived_word != word:
            prefix_of.insert(derived_word)
            if len(prefix_of) >= MAX_PREFIX_COUNT(1000): break
```

复杂度对照：

| 路径 | 时间 | 何时走 |
| --- | --- | --- |
| LMDB `prefix_iter` 快路径 | `O(log N + 命中前缀数)` | `word-prefix-docids` 里已有该前缀（绝大多数常见前缀） |
| 两游标归并 | `O(命中前缀数)`，两个有序游标各一次 B-tree 下降 | 快路径未命中 |
| （对照）DFA×FST | `O(词典字符数)` 上界 | 计算 typo 派生时 |

索引阶段为此付出的代价：每个词的所有前缀都要写进 `word_prefix_docids`（`Database<Str, CboRoaringBitmapCodec>`），空间换查询时间。同前缀的 docid 会合并到同一张位图里，因此**前缀数比词数少得多**时收益最大。

`MAX_PREFIX_COUNT = 1000` 是自我保护：像 `a`、`th` 这样的前缀会命中几十万个派生词，若不截断会让单次查询爆掉。

### 19.5 限额、懒加载与同义词

`crates/milli/src/search/new/limits.rs` 定义了全部 5 个上限：

```rust
MAX_TOKEN_COUNT      = 1_000   // 单次搜索考虑的最大 token 数
MAX_PREFIX_COUNT     = 1_000   // 单个词可派生的最大前缀数
MAX_ONE_TYPO_COUNT   = 150     // 单个词最多 150 个 1-typo 派生词
MAX_TWO_TYPOS_COUNT  = 50      // 单个词最多 50 个 2-typo 派生词
MAX_SYNONYM_PHRASE_COUNT = 50   // 单个词最多展开的同义词短语数
MAX_SYNONYM_WORD_COUNT    = 100 // 所有同义词短语内的总词数（防超长同义词）
```

（源码里实际是 6 个常量，上面按用途分组列出前 5 个与同义词的 2 个。）

懒加载由 `Lazy<OneTypoTerm>` / `Lazy<TwoTypoTerm>` 表达，只有在 `words` 准则真正走到「需要区分 0/1/2 个 typo」时才展开：

```
compute_fully_if_needed():
    if max_levenshtein_distance <= 1 and one_typo 未初始化:
        initialize_one_typo_subterm()            // 只建 1-typo 派生集
        two_typo = Init(空集)                    // 显式置空，杜绝后续误用
    elif max_levenshtein_distance > 1 and two_typo 未初始化:
        initialize_one_and_two_typo_subterm()    // 一次求交同时建两个集合
```

同义词在**同一个** `QueryTerm.zero_typo` 结构里展开（`zero_typo.synonyms` 是 `BTreeSet<Interned<Phrase>>`），即同义词被视为「零 typo 的等价表述」，与 `exact` / `prefix_of` 并列。同义词查找走 LMDB `synonyms` 库（`Database<SynonymsKeyCodec<String>, SerdeJson<AssociatedSynonyms>>`），因此支持**多词短语**同义词，这也是 `MAX_SYNONYM_WORD_COUNT` 存在的原因。

### 19.6 前置环节：charabia 的「切分 → 归一化」与词典

typo 派生作用在一个已经切分归一化过的词上，所以要先说清楚分词本身。Meilisearch 把分词外包给 **charabia**（`TokenizerBuilder`，`crates/milli/src/search/new/mod.rs` 中从索引设置构建），其核心是一条两段流水线（`charabia/src/tokenizer.rs`）：

```text
Tokenizer::tokenize(text)
    → Segmenter：先用 Aho-Corasick 自动机（aho-corasick crate）在「默认分隔符集」上
                 把原串切成片段；匹配策略为 MatchKind::LeftmostLongest，
                 即最长匹配优先（"a.b" 里 ". " 不会被 " " 抢先匹配）
    → Normalizer：按 normalizer::NORMALIZERS 链做大小写折叠、
                   Unicode 规范化、复合词分解、语言判定等
    → Iterator<Item = Token>，每个 Token 带 TokenKind 与 lemma（词元）
```

分隔符来自 charabia 的 `DEFAULT_SEPARATORS`（`charabia/src/separators.rs`），源码注释说明其集合 = Unicode 类别 `Pc/Pd/Pe/Pf/Pi/Po/Ps/Sm/Zl/Zp/Zs` 的**全部字符**，外加 `"\0"`、`". "`、`", "`、`"។ល។"`（高棉语分解符）与反引号 `` ` ``（用于识别 Markdown 格式化文本）。用户侧的 `separatorTokens` / `nonSeparatorTokens`（`crates/meilisearch-types/src/settings.rs`，schema 默认均为 `[]`）分别在这套默认集上「增」与「减」。

这些设置与词典在 LMDB `main` 库里以固定键存放（`crates/milli/src/index.rs` 的 `main_key` 模块）：

| 设置 | main 库键 | 存储形式 |
| --- | --- | --- |
| `stopWords` | `stop-words` | `fst::Set`，`None` 表示未设置 |
| `separatorTokens` | `separator-tokens` | JSON |
| `nonSeparatorTokens` | `non-separator-tokens` | JSON |
| `dictionary` | `dictionary` | JSON（单 token 白名单，如人名、领域词） |
| 词典 FST | `words-fst` | `fst::Set` |
| 前缀 FST | `words-prefixes-fst` | `fst::Set` |
| 用户同义词 | `user-defined-synonyms` | JSON |

stop words 的作用点在查询解析阶段：`parse_query.rs` 里 `TokenKind::StopWord` 的 token **既不产生 `QueryTerm`、也不参与 phrase**（源码 match 分支直接 `TokenKind::StopWord | TokenKind::Separator(_) | TokenKind::Unknown => ()`），所以停用词只在「计算分词后的词序 position」时被消耗掉一个位置号。

⚠️ **与 §15.2 的差异**：现有章节写「Tokenizer 体系：`crates/charabia/src/`（多语言分词）」。charabia 在 v1.55 中**不是主仓库内的 crate**，而是 crates.io 依赖（`crates/milli/Cargo.toml` 里 `charabia = { version = "0.10.0", default-features = false }`），独立仓库为 `meilisearch/charabia`，源码在**该仓库内**的 `charabia/src/` 下。上面两处 charabia 路径均指独立仓库，不是 meilisearch 主仓库的路径。此外它按语言拆成 feature（`chinese` / `japanese` / `korean` / `hebrew` / `thai` / `khmer` / `vietnamese` / `greek` / `german-segmentation` / `swedish-recomposition` / `turkish` / `all-tokenizations`），编译期裁剪。

### 19.7 本节复杂度总表

| 环节 | 时间 | 空间 |
| --- | --- | --- |
| typo 分档 | `O(1)`/词（`chars().count()` 需 `O(词长)`） | `O(1)` |
| DFA 构造 | `O(Σ × |word| × typos)`，进程内 `LazyLock` 只建一次每档 | `O(Σ × |word|)` |
| DFA × FST 1-typo | `O(词典字符数)` 上界，实为 `O(命中数 × |word|)` | `O(150)` 派生词 |
| DFA × FST 1+2 typo | 同上，**只走一遍 FST** | `O(150 + 50)` 派生词 |
| 前缀快路径 | `O(log N + 命中前缀数)` | `O(1)` |
| 前缀慢路径 | `O(命中前缀数)`，两条 B-tree 游标归并 | `O(1000)` 派生前缀 |
| 索引期额外开销 | **0**（无 BK-tree、无词典度量树） | 仅词 FST 本身 |

## 二十、持久层：LMDB 分库布局、obkv 文档编码与 index swap 的原子性（算法展开）

> **源码基准**：Meilisearch **v1.55.0**（commit `8b7c2172b9bf8be7d05f09d7174a8c15449a2c1d`），`heed` 依赖指向 meilisearch 的 fork（分支 `mutexify-open-dbi`，版本 `0.22.1`）。子模块未 init，故**只给路径、不给行号**。

### 20.1 一个索引 = 一个 LMDB env，27 + Cellulite::nb_dbs() 个命名库

`Index::new_with_creation_dates()`（`crates/milli/src/index.rs`）的布局与 §6 / §7.1 的描述**完全不同**，需要更正：

```rust
const NUMBER_OF_DBS: u32 = 27 + Cellulite::nb_dbs();

options.max_dbs(NUMBER_OF_DBS);
let env = unsafe { options.open(path) }?;
let mut wtxn = env.unique_write_txn()?;
let main  = env.database_options().name(MAIN).create(&mut wtxn)?;
// 再逐个 env.create_database(&mut wtxn, Some(NAME))? 打开/创建 27 个具名库
```

要点：

1. **每个索引一个独立的 LMDB 环境**，环境目录名是 UUID（见 §20.4），不是一个装着所有索引的 `data.ms`；
2. LMDB 的 `main` DB 之外用具名子库（named database），因此是**按逻辑表分库**，而不是 §7.1 画的「一个库里用 metadata / postings / dictionary 当 key」；
3. `heed` 是 Meilisearch 自己的 fork（`heed = { git = "https://github.com/meilisearch/heed", branch = "mutexify-open-dbi" }`），开启了 `read_txn_without_tls()` 以省掉每个读事务的 TLS 查询开销；
4. 具名库的完整清单在 `db_name` 模块里（`crates/milli/src/index.rs`），关键几类：

| 类别 | 库名 | 类型（Key → Value） |
| --- | --- | --- |
| 倒排 | `word-docids` | `Str → CboRoaringBitmapCodec` |
| 倒排（禁 typo） | `exact-word-docids` | `Str → CboRoaringBitmapCodec` |
| 前缀倒排 | `word-prefix-docids` | `Str → CboRoaringBitmapCodec` |
| 前缀禁 typo | `exact-word-prefix-docids` | `Str → CboRoaringBitmapCodec` |
| 位置倒排 | `word-position-docids` | `StrBEU16Codec → CboRoaringBitmapCodec`（键 = 词 + 分桶位置） |
| 字段倒排 | `word-field-id-docids` | `StrBEU16Codec → CboRoaringBitmapCodec` |
| 词对 proximity | `word-pair-proximity-docids` | `U8StrStrCodec → CboRoaringBitmapCodec` |
| 词频（BM25） | `field-id-word-count-docids` | `FieldIdWordCountCodec → CboRoaringBitmapCodec` |
| 文档 | `documents` | `BEU32 → ObkvCodec` |
| 主键映射 | `external-documents-ids` | `Str → BEU32` |
| 同义词 | `synonyms` | `SynonymsKeyCodec<String> → SerdeJson<...>` |
| 词典 FST | `facet-id-string-fst` | `BEU16 → FstSetCodec` |
| 分面（数值） | `facet-id-f64-docids` | `FacetGroupKeyCodec<OrderedF64Codec> → FacetGroupValueCodec` |
| 分面（字符串） | `facet-id-string-docids` | `FacetGroupKeyCodec<StrRefCodec> → FacetGroupValueCodec` |
| 分面存在性 | `facet-id-exists-docids` / `-is-null-` / `-is-empty-` | `FieldIdCodec → CboRoaringBitmapCodec` |
| 文档→分面 | `field-id-docid-facet-f64s` / `-strings` | `FieldDocIdFacetF64Codec → Unit` / `→ Str` |
| 向量 | `vector-arroy` | hannoy™ 向量库（独立命名空间） |
| 地理 | `cellulite` | Cellulite™ 地理库（前缀，实际库数 = `Cellulite::nb_dbs()`） |

⚠️ **与 §7.3 的差异**：`segments` / `merge-policies.yaml` / Lucene 式 `TieredMergePolicy` 段合并在现代 Meilisearch 中**不存在**。它把 §8.1 描述的倒排索引直接作为多个 LMDB 命名库维护，更新是「读-改-写同一个 posting 位图」，没有段级重写。§7.3 的「段合并策略与 Lucene TieredMergePolicy 类似」属早期/误传内容。（因红线保护，本节只做更正说明，不改原章节。）

### 20.2 写事务 vs 读事务：LMDB 的单写者模型被直接用作并发闸门

LMDB 本身只允许**一个写事务**并发（由文件锁保证），但允许任意多个读事务且互不阻塞。Meilisearch 把这个模型直接暴露为 API：

```rust
// crates/milli/src/index.rs
pub fn write_txn(&self) -> heed::Result<RwTxn<'_>>           // 索引自己的写事务
pub fn read_txn(&self)  -> heed::Result<RoTxn<'_, WithoutTls>> // 索引自己的读事务
```

索引调度侧的提交粒度（`crates/index-scheduler/src/scheduler/process_batch.rs`）——**一个批次 = 一个写事务**：

```text
process_batch(batch):
    rtxn      ← self.env.read_txn()          // 任务队列 env 的读事务
    index     ← index_mapper.index(rtxn, index_uid)
    index_wtxn ← index.write_txn()           // ← 整个批次的写入独占一个写事务

    match batch:
        DocumentAdditionOrUpdate / DocumentDeletion / Settings:
            ... 所有更新都在 index_wtxn 内完成 ...
            index_wtxn.commit()               // ← 只有一次 commit，整批原子

        IndexSwap / IndexCreation:
            wtxn ← self.env.write_txn()       // 索引映射 env 的写事务
            ... swap / rename ...
            wtxn.commit()                     // ← 同样只有一次 commit
```

语义与复杂度：

| 项 | 语义 | 复杂度 |
| --- | --- | --- |
| 写事务 | 进程内**独占**，同一索引同时最多一个 | 批次内 `O(变更文档数)`，提交一次 `O(1)` fsync |
| 读事务 | 快照隔离，**看到批次提交前的一致视图** | `O(1)` 建事务，读取 `O(log N)`/次 |
| 批次原子性 | 批次内任一步失败 → 整个 `RwTxn` drop → 全部回滚 | 无部分可见状态 |
| 跨批次可见性 | 只在 `commit()` 时刻可见 | —— |
| 死锁 | 不存在（单写者，无行锁升级） | —— |

`Index::rollback()`（`crates/milli/src/index.rs`，经 `read_txn_without_tls()` 打开）提供版本迁移时的一致性点：LMDB 的 `MDB_NOTLS` 允许任意线程打开读事务而不用绑定到创建线程，这正是回滚/升级能在运行中执行的前提。

### 20.3 obkv 文档编码：追加式的紧凑 KV

文档本体存在 `documents` 库里，格式是 **obkv**（独立 crate，`obkv = "0.3.0"`）。Meilisearch 用的是 `KvWriterU16` / `KvReaderU16` 变体，物理格式为：

```text
一个 obkv = 若干条记录顺序拼接：

   ┌────────────────┬──────────────────────┬─────────────────────┐
   │ key: u16 大端   │ len: LEB128 变长整数  │ value: len 字节      │
   │ (2 字节 = 字段 id)│ (1~5 字节)          │ (serde_json 的字节)  │
   └────────────────┴──────────────────────┴─────────────────────┘
```

写入端的硬约束与实现在 `DocumentsBatchBuilder::append_json_object()`（`crates/milli/src/documents/builder.rs`）：

```rust
// 源码注释：Make sure that we insert the fields ids in order as the obkv writer has this requirement.
let mut fields_ids: Vec<_> = object.keys().map(|k| self.fields_index.insert(k)).collect();
fields_ids.sort_unstable();                       // ← 必须升序

self.obkv_buffer.clear();                        // ← 复用缓冲区，避免重分配
let mut writer = obkv::KvWriter::new(&mut self.obkv_buffer);
for field_id in fields_ids {
    self.value_buffer.clear();
    to_writer(&mut self.value_buffer, &object[key])?;   // value 是 JSON 字节
    writer.insert(field_id, &self.value_buffer)?;
}

let internal_id = self.documents_count.to_be_bytes();   // ← 内部 docid = u32 大端
let document_bytes = writer.into_inner()?;
self.writer.insert(internal_id, &document_bytes)?;      // ← 写进 grenad 批文件
```

关键权衡与代价：

| 特性 | 收益 | 代价 |
| --- | --- | --- |
| key 严格递增（`insert` 里 `key <= last_key` 直接报错） | 顺序追加，无重写、无随机写 | **随机取字段是线性扫描**：`KvReader::get(k)` = `iter().take_while(\|key\| key <= k).find(...)` |
| LEB128 变长长度 | 短字段（如 1~5 字节）只占 1 字节长度头 | 单个超长 value 最多占 5 字节头 |
| 字段名不存 | 每文档省下 `Σ 字段名字节` | 必须靠外部的字段 id ↔ 名字映射还原 |
| 内部 docid = `u32` 大端（BEU32） | 可直接做 LMDB 有序键前缀扫描 | 外部主键是字符串时需 `external-documents-ids` 二次映射 |

字节数公式（单文档、k 个字段、平均字段名长度 L、值总长 S）：

```
obkv 大小 ≈ Σ_i ( 2 + varint_len(v_i) + |v_i| )
         = 2k + S + O(k)
对比 JSON 明文 ≈ S + Σ_i L_i
→ 当 L_i 较大（典型英文 5~10 字节）时，obkv 的字段名省下的字节可与 key+len 的开销抵消，
   实测收益主要体现在「不重复存字段名」与「可直接做有序扫描」两点上。
```

**批文件的最后一个特殊键**：`DocumentsBatchBuilder::into_inner()` 会把 `DocumentsBatchIndex`（字段 id ↔ 字段名的双向映射）序列化后，作为**绝对最后一个键**写进 grenad 文件：

```rust
const DOCUMENTS_BATCH_INDEX_KEY: [u8; 8] = u64::MAX.to_be_bytes();   // crates/milli/src/documents/mod.rs
```

用 `u64::MAX` 保证它永远排在所有 `u32` 内部 docid 之后，读取时先顺序扫到文件尾拿到字段表，再回头解析正文——这样批文件本身就是自描述的（self-describing），无需外部元数据。批文件用 grenad 写入，`DocumentsBatchBuilder` 显式关掉压缩（`CompressionType::None`），实际分块压缩在 `IndexerConfig::chunk_compression_type` 层面控制（`crates/milli/src/update/indexer_config.rs`）。

### 20.4 index swap：交换两条「名字 → UUID」映射

这是 Meilisearch 最漂亮的一个设计。索引在磁盘上的目录名是 **UUID**，不是用户给的 `indexUid`；`indexUid → uuid` 的映射单独存在调度器的 LMDB 里：

```rust
index_mapping: Database<Str, UuidCodec>,                 // crates/index-scheduler/src/index_mapper/mod.rs
index_stats:   Database<UuidCodec, SerdeJson<IndexStats>>,
fn index_path_from_uuid(&self, uuid: Uuid) -> PathBuf { self.base_path.join(uuid.to_string()) }
```

于是「交换两个索引」退化成了两次 `put`：

```rust
// IndexMapper::swap()
let lhs_uuid = self.index_mapping.get(wtxn, lhs)?;   // 取出两边各自的 UUID
let rhs_uuid = self.index_mapping.get(wtxn, rhs)?;
self.index_mapping.put(wtxn, lhs, &rhs_uuid)?;       // ← 两次写，没有搬数据
self.index_mapping.put(wtxn, rhs, &lhs_uuid)?;
```

外层 `apply_index_swap()`（`crates/index-scheduler/src/scheduler/process_batch.rs`）在一个写事务里做完整套一致性修补：

```text
apply_index_swap(wtxn, task_id, lhs, rhs, rename):
    1. 校验：rename=false 时两边都必须已存在；rename=true 时 rhs 必须不存在
    2. 取 task_id 之前挂在 lhs / rhs 名下的全部任务 id（remove_range(task_id..)，只要历史）
    3. 逐个把历史任务里的 uid 改写：swap_index_uid_in_task(&mut task, (lhs, rhs))
       —— 任务详情里的 indexUid、任务在「按索引分组」队列里的归属都要跟着改
    4. tasks.update_index(lhs): lhs_tasks ← (lhs_tasks − lhs_ids) ∪ rhs_ids
       tasks.update_index(rhs): rhs_tasks ← (rhs_tasks − rhs_ids) ∪ lhs_ids
    5. index_mapper.swap(wtxn, lhs, rhs)  或  index_mapper.rename(wtxn, lhs, rhs)
    ← 以上全部在同一个 RwTxn 里，由外层 wtxn.commit() 一次性提交
```

原子性与复杂度：

| 项 | 值 |
| --- | --- |
| 数据搬迁量 | **0 字节** |
| LMDB 写操作数 | 2 次 `put` + `O(历史任务数)` 次任务改写 |
| 原子性 | 单写事务；要么全部生效，要么全部不生效 |
| 原子性的物理保证 | LMDB 的 copy-on-write B+tree，`commit()` 一次性写 meta page；读者要么看到旧映射，要么看到新映射 |
| 隔离性 | 并发读事务仍可继续按旧 uid 读到旧索引，直到提交 |
| 失败模式 | 前面所有校验失败 → 直接返回 `SwapIndexNotFound` / `SwapIndexesNotFound` / `SwapIndexFoundDuringRename`，`wtxn` 被丢弃即回滚 |

复杂度 `O(1 + T)`，其中 `T` 是两个索引的历史任务总数。`T` 通常远小于文档数——这正是「零停机 reindex」的关键：**重建一个新索引 → 等它 ready → swap → 删旧索引**，整个过程对读端零感知。

⚠️ **与 §9.3.3 的差异**：现有章节只说「v1.12+ 提供 Index Swap，用于 zero-downtime reindex」，未说明它靠 UUID 间接层实现。本节补上机制。`rename: bool` 字段把「交换」（两边都存在）与「改名」（目标不存在）统一进同一个任务类型。

### 20.5 分面聚合的两阶段算法

分面统计（`facets` / `facetDistribution`）的入口是 `FacetDistribution::execute()`（`crates/milli/src/search/facet/facet_distribution.rs`），核心是一个**三分支策略**，由候选集规模与排序方式共同决定：

```text
execute(field_id, order_by):
    match order_by:

    ①  SampledCount { sample }:
            // 两阶段：① 扫描样本（≤1000 个 docid）收集「出现过哪些 facet 值」
            //         ② 只对这些值到层级库里取全量计数（精确值）
            facet_distribution_from_sample(String, sample, candidates)
            facet_distribution_from_sample(Number, sample, candidates)
            # 若 sample.len() == candidates.len() 则跳过阶段②，直接用样本计数

    ②  Lexicographic 且 candidates.len() ≤ 3000:
            // 候选少 → 直接逐文档线性扫描 field-id-docid-facet-{f64s,strings}
            facet_distribution_from_documents(Number, candidates)
            facet_distribution_from_documents(String, candidates)

    ③  其他（Lexicographic/Count 且候选多，或未给 candidates）:
            // 候选多 → 走层级 facet 库，TreeMap 式的降序/字典序游标遍历
            facet_numbers_distribution_from_facet_levels(..., lexicographical_order)
            facet_strings_distribution_from_facet_levels(..., lexicographical_order)
```

三个常量定义了这个策略的拐点：

```rust
const LINEAR_SCAN_CANDIDATES_THRESHOLD: u64 = 3000;   // 候选 ≤3000 走线性，否则走层级
const SAMPLE_CANDIDATES: usize = 1000;                // SampledCount 的样本量
const DEFAULT_VALUES_PER_FACET: usize = 100;          // 每个分面默认返回的取值数
```

②的线性扫描实现（`facet_distribution_from_documents`）值得单独看：

```rust
let mut key_buffer: Vec<_> = field_id.to_be_bytes().to_vec();   // 手工拼 key 前缀
for docid in candidates {                                        // RoaringBitmap 迭代
    key_buffer.truncate(mem::size_of::<FieldId>());
    key_buffer.extend_from_slice(&docid.to_be_bytes());          // fid(2B) + docid(4B)
    let iter = db.prefix_iter(self.rtxn, &key_buffer)?;          // B-tree 前缀扫描
    for ((_, _, value), ()) in iter {
        *distribution.entry(value.to_string()).or_insert(0) += 1;
    }
}
```

③的层级遍历（`facet_distribution_iter.rs`）用 `BinaryHeap<Reverse<FacetValueHit>>` 做**有界 top-k 堆**，堆满 `max_values_per_facet` 后只替换最小项，因此内存是 `O(top-k)` 而非 `O(不同 facet 值数)`。

复杂度对照：

| 分支 | 时间 | 内存 | 精确性 |
| --- | --- | --- | --- |
| ① SampledCount | `O(1000 × 每文档字段数)`（阶段①）+ `O(命中值数 × log N)`（阶段②） | `O(命中值数)` | **精确**（阶段②用全量计数） |
| ② 线性扫描（≤3000 候选） | `O(候选数 × log N)` | `O(不同值数)` | 精确 |
| ③ 层级游标 | `O(log N + 访问节点数 + top-k × log k)` | `O(top-k)` | 精确 |

日期分面没有单独实现——`facet-id-f64-docids` 用 `OrderedF64Codec` 存数值，日期在索引时就被解析成 Unix 时间戳，因此「按日期分面」等价于「按整数分面 + 区间过滤」，复用同一套结构。`facetStats`（min/max）则直接从 `facet_id_f64_docids` 的首尾键取得。

### 20.6 任务状态机与本节的关系

持久层的所有变更都由任务队列驱动，状态机极简（`crates/meilisearch-types/src/tasks/mod.rs`）：

```
Status = Enqueued → Processing → Succeeded
                             → Failed      （带 error.code / message / link）
                             → Canceled
```

队列本身存在**另一个独立的 LMDB 环境**（`self.env`，与各索引的 env 分离），`max_dbs(Versioning::nb_db())`，`map_size` 由 `task_db_size` 配置。这带来一个刻意的取舍：**索引数据与任务队列不在同一个事务里**——所以 `index_wtxn.commit()` 之后、队列状态写入之前崩溃，会导致任务重放；而任务队列侧的重复执行对索引更新是幂等的（文档 upsert 按主键覆盖）。

### 20.7 本节复杂度总表

| 环节 | 数据结构 | 复杂度 |
| --- | --- | --- |
| 打开索引 | 27 + Cellulite::nb_dbs() 个具名 LMDB 库 | `O(库数)` 建对象，实际按需 mmap |
| 批次写入 | 单个 `RwTxn` | `O(变更文档数 × 变更库数)`，一次 commit |
| 提交 | LMDB copy-on-write | `O(1)` 次 fsync |
| obkv 编码 | 顺序追加 + 字段 id 升序 | `O(字段数 + 值总长)` |
| obkv 单字段读取 | 线性扫描 + 提前退出 | `O(字段序号)` |
| 批文件读取 | 先读尾键字段表，再顺序扫正文 | `O(批大小)` |
| index swap | 2 次 `put` + 任务改写 | `O(1 + 历史任务数)`，**0 字节数据搬迁** |
| 分面 ① 抽样 | 样本扫描 + 层级取值 | `O(1000·d + h log N)` |
| 分面 ② 线性 | 逐 docid 前缀扫描 | `O(n log N)`，`n ≤ 3000` |
| 分面 ③ 层级 | B-tree 游标 + 有界堆 | `O(log N + t log k)`，`k ≤ 100` |

## 二十一、参考资料

1. Meilisearch 官方文档：<https://www.meilisearch.com/docs/>
2. Meilisearch GitHub：<https://github.com/meilisearch/meilisearch>
3. InstantSearch.js：<https://github.com/meilisearch/instant-meilisearch>
4. DocSearch v3：<https://github.com/meilisearch/docsearch>
5. milli crate（核心引擎）：<https://github.com/meilisearch/milli>
6. charabia（多语言分词）：<https://github.com/meilisearch/charabia>
7. RoaringBitmap 论文：<https://github.com/RoaringBitmap/RoaringBitmapSpec>
8. BK-tree (Burkhard-Keller, 1973)
9. Damerau-Levenshtein 距离
10. Meilisearch Cloud：<https://www.meilisearch.com/cloud>
