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

## 十八、参考资料

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
