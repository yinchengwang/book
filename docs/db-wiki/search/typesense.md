# Typesense Wiki（C++ 极简即时搜索引擎）

> 参考版本：Typesense v25.x（commit `main`），**C++（C++17）+ RocksDB + JNI 嵌入选项** 实现的轻量级 **RESTful 即时搜索引擎**，定位为 **Algolia 的开源替代**。主打 **1 分钟集成 + typo tolerance + 几何搜索 + 多租户 + 高亮 + Curated Results**。本 wiki 覆盖倒排索引、相关性排序、过滤与 Faceting、几何搜索、即时索引。

## 一、项目定位

Typesense 是 **Typesense Inc.（旧名 Apptide Labs / Jason Chen / Rain Storey 等创立）** 开源的 **C++ 即时全文搜索引擎**，定位为 **Algolia SaaS 搜索的开源替代**，强调 **"Algolia 的体验，开源 + 自托管"**。它把 **"毫秒级响应 + typo tolerance + 几何搜索 + 多租户 API Key + curated results + instant search"** 整合为一站式 RESTful 服务。

- **痛点**：
  - Algolia SaaS 收费 + 数据出门 + 复杂计费；
  - Elasticsearch / OpenSearch 单实例占用 1GB+ RAM；
  - Solr / Lucene 部署繁琐、缺乏 typo tolerance；
  - Meilisearch 是 Rust，Typesense 是 C++，性能 / 内存控制各有偏好；
  - 需要 **电商商品搜索、SaaS 文档检索、多租户 SaaS 搜索、Geo-aware 搜索** 等场景；
- **典型用户**：电商 / SaaS / DXP / 内容平台 / 内部工具搜索；
- **核心口号**：**"Algolia Alternative"** + **"Fast. Typo-Tolerant. In-Memory Search"** + **"Search as you type"**。

## 二、发展历史与社区

- **起源**：2015 年由 **Jason Chen** 作为 Algolia 的开源替代品立项，前期用 Go 实现；
- **里程碑**：
  - 2016-05：v0.1（Go）；
  - 2018-08：v0.9 重写为 **C++**（基于 RocksDB + 自研文本索引）；
  - 2019-09：v0.15 发布 typo tolerance + curated results；
  - 2020-04：v0.16 加入 **multi-search + synonyms + stop-words**；
  - 2020-12：v0.19 引入 **API Keys 多租户**；
  - 2021-09：v0.22 引入 **GeoSearch / Analytics**；
  - 2022-04：v0.23 加入 **Voice Search / Snowball Tokenizer**；
  - 2022-08：v0.24 加入 **Conversation Model + Seq2Seq（向量检索雏形）**；
  - 2023-02：v0.25 重写 **Ranking + Index Merging**；
  - 2023-09：v25.0 大版本号变更；
  - 2024-2026：v25+ 持续优化 **vector search + Recommenders + Conversational Search**；
- **维护状态**：活跃，**GitHub ~20k stars**，公司 **Typesense Inc.** 提供 **Typesense Cloud**；
- **社区**：官方论坛、Discord、Twitter、GitHub Discussions 活跃。

## 三、许可与商业限制

- **许可证**：**GNU Affero General Public License v3.0（AGPL-3.0）**；
  - 自托管免费；
  - 修改并对外提供服务必须开源（copyleft）；
- **商业版本**：
  - **Typesense Cloud**：官方 SaaS（AWS / GCP / DigitalOcean / Hetzner 多区域）；
  - **Typesense Enterprise**（on-prem）：商业版（提供 SAML、SSO、合规审计、日志聚合）；
- **生态关系**：
  - 与 **Algolia**：直接对标；
  - 与 **Meilisearch**：开源即时搜索赛道；
  - 与 **RocksDB**：底层 KV 存储；
  - 与 **Apache SOLR / Elasticsearch**：复杂搜索场景；
  - 与 **DocSearch**：替代 Algolia DocSearch 的 DocSearch by Typesense。

## 四、业界主要使用方与案例

- **公开用户**：
  - **Apple Developer**：Apple 开发者文档搜索（自托管 Typesense）；
  - **GitHub**：GitHub Pages 站搜索部分场景；
  - **Mindvalley**：课程搜索；
  - **Houzz**：家居图集搜索；
  - **Zapier**：内部文档搜索；
  - **Product Hunt**（部分场景）；
  - **多家 SaaS**：基于多租户 API Key 模式集成；
- **典型场景**：电商商品搜索（<10M SKU）+ 多租户 SaaS 平台搜索、电商多店中台、AI RAG 检索层（v25+）。

## 五、一句话对比定位

> Typesense 是 **"C++ + AGPL + 即时搜索 + typo tolerance + 多租户 API Key + geometric search + curated results + 推荐系统"** 的开源全文搜索引擎；与 **Elasticsearch / OpenSearch（重量级）**、**Meilisearch（Rust MIT）**、**Algolia（SaaS 商业）**、**Tantivy（嵌入式 Rust）**、**Quickwit（云原生对象存储）** 构成引擎阵营。Typesense 的差异化是 **"Algolia 体验 + C++ 性能 + AGPL 开源 + 多租户 API Key"**。

## 六、整体架构

```
┌────────────────────────────────────────────────────────────────────────┐
│                  客户端层                                              │
│  - REST API（8108，默认）/ HTTPS                                       │
│  - 官方 SDK：typesense-js / -python / -go / -java / -ruby / -php /    │
│    -swift / -kotlin / -rust / -flutter                                │
│  - InstantSearch.js（前端即用）                                       │
│  - Vue / React / Svelte 即时组件                                      │
└─────────────────────┬──────────────────────────────────────────────────┘
                      │ HTTP/JSON
┌─────────────────────▼──────────────────────────────────────────────────┐
│                  HTTP Server（Httplib / Crow / Avalanche）              │
│  src/http_server.cpp + src/http_server.h                                │
│  ├─ HTTP/1.1 + HTTP/2（部分）                                         │
│  ├─ CORS + API Key 鉴权                                              │
│  ├─ 请求路由（POST /collections/{name}/documents 等）                  │
│  └─ 异步处理（std::async / thread-pool）                              │
└─────────┬───────────────────┬──────────────────────┬───────────────────┘
          │ search            │ index               │ admin
┌─────────▼───────┐ ┌────────▼──────────┐ ┌────────▼──────────┐
│  Index 引擎    │ │  RocksDB 存储     │ │  API Key + ACL    │
│  - 倒排表      │ │  - Key-Value    │ │  - 多租户管理   │
│  - 评分       │ │  - 索引段         │ │  - 速率限制     │
│  - 几何/向量   │ │  - 持久化         │ │                 │
└────────┬───────┘ └────────┬──────────┘ └─────────────────┘
         │
┌────────▼─────────────────────────────────────────────────────────────┐
│  核心库（src/）                                                     │
│  ├─ index.*（text 字段倒排）                                       │
│  ├─ in_mem_index.*（in-memory 倒排层）                              │
│  ├─ RocksDB 持久化层                                              │
│  ├─ analytics.*（统计模块）                                        │
│  ├─ auth_manager.* + api_*：API Key 鉴权                          │
│  ├─ conversation_model.* / seq2seq_*（推荐/排序）                │
│  └─ geometry.*（GeoSearch）                                       │
└─────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│  本地文件系统                                                      │
│  /tmp/typesense-data/ 或 /var/lib/typesense/                         │
│  ├─ state.db（RocksDB：API Keys + 索引元数据）                      │
│  ├─ <db>/index/（文本索引段 + RocksDB）                            │
│  ├─ <db>/store/（文档存储 + RocksDB）                              │
│  └─ analytics/（访问日志 + RocksDB）                               │
└─────────────────────────────────────────────────────────────────────┘
```

### 关键设计取舍

- **C++ 单进程**：通过 RocksDB 提供持久化层，**所有搜索都在内存中**（in_memory_index）；
- **多租户原生**：基于 API Key + Scope（按 collection / action / limit）；
- **集合（Collection）模型**：字段（fields）+ schema（type, optional, facet, sort, index）；
- **API Key 类型**：
  - **Admin**（完全权限）；
  - **Search-only**（只读）；
  - **Scoped**（限制 collection / operation）；
  - 速率限制 embedded；
- **单实例 SCAN**：单实例没有集群模式；Typesense Cluster（商业版）提供高可用；
- **即写即索引**：写入即更新内存索引，立即可查。

## 七、存储引擎（In-Memory + RocksDB）

### 7.1 索引文件

```
data/
├─ state.db  (RocksDB：API Keys + meta)
├─ <db_name>/
│   ├─ index/
│   │  ├─ *.log (RocksDB WAL)
│   │  ├─ *.sst (RocksDB SST 文件)
│   │  └─ CURRENT / LOCK / LOG
│   └── store/
│      ├─ docs/（文档原始 JSON，SST）
│      └─ *.sst / *.log
```

### 7.2 写入流程

```
POST /collections/products/documents
  → http_server.cpp 解析
  → Collection::add()
  → RocksDB::WriteBatch 提交到 store
  → in_mem_index.cpp:index_recursive_levenshtein_trees()
  → 索引刷新 → 内存 HASH + Posting
```

源码（典型路径）：
- `src/collection.cpp` + `src/collection.hpp`：集合核心；
- `src/index.cpp` + `src/index.hpp`：文本索引；
- `src/in_mem_index.cpp`：in-memory 倒排 + 评分；
- `src/store.cpp` + `src/store.hpp`：RocksDB 持久化。

### 7.3 段合并 / Sort / Filter

- **Sort / Filter**：`src/filter_sort.cpp`、`src/filter_sort_iterator.cpp`；
- **排序字段**：使用 RocksDB 持久化的 **ArtIndex（Adaptive Radix Tree）**；
- **分桶**：排序字段拆分为多个 bucket，bucket 内排序。

## 八、索引机制（**搜索类型重点展开**）

### 8.1 倒排索引（In-Memory Posting List）

```
            In-Memory Index
            ┌────────────────────────────┐
            │  Term Dictionary (Hash)    │
            └─────────────┬──────────────┘
                          │
            ┌─────────────▼──────────────┐
            │  Posting List (Vector)    │
            │   docId, position, score   │
            └─────────────┬──────────────┘
                          │
            ┌─────────────▼──────────────┐
            │  Token Map (offsets)       │
            └────────────────────────────┘
```

- **Term 到 Posting**：HASH（term 字符串 → offset）；
- **Posting**：以 vector of pairs（docId, freq, position）存储；
- **Analytic 字段 token map**：用于 phrase query / phrase match；
- **源码**：`src/in_mem_index.cpp`、`src/index.cpp`。

### 8.2 评分（BM25 + 多策略）

- **BM25**：默认相关性；
- **`text_match_weight`** 字段权重；
- **`field_weight`**：单个 field 内权重；
- **`tie_breaker`**：多个 query token 都命中时的合并；
- **`ranking_weights`**：
  - `word_match` → `typo` → `proximity` → `attribute` → `sort` → `exactness` → `tag`（v25+）；
- **`num_typos` / `min_len_1typo` / `min_len_2typo`**：typo tolerance 参数；
- **`priority`**：文档优先级；
- **`curated`**：手工覆盖排序（按 curate list）；
- **`voice_query`**：基于 conversation_model 的意图匹配。

源码：
- `src/weighted_levenshtein.cpp`：typo 距离 + 权重；
- `src/sequence_matcher.cpp`：phrase match。

### 8.3 typo tolerance

- **算法**：基于 **Levenshtein 距离（UTF-8 编辑距离）**；
- **配置**：typo 容忍度按 term 长度阶梯：
  - 默认：≥5 char → 1 typo；≥9 char → 2 typo；
- **实现**：`src/levenshtein.cpp`、`src/word_alternator.cpp`、`src/aggregation/agg.cpp`。

### 8.4 过滤器与 Faceting

#### 8.4.1 filter

- 字段必须声明为 `filter: true`；
- filter expression 支持：`=`, `!=`, `<`, `<=`, `>`, `>=`, `:[..]`, `:[..]`, `:<x..y<`：

```json
POST /collections/products/documents/search
{
  "q": "phone",
  "filter_by": "price:>500 && category:=audio"
}
```

- **Pre-Filter / Post-Filter**：filter 在排序之前收缩候选（Pre-Filter），但支持 `sort_by` 时调整顺序。

#### 8.4.2 faceting

- 字段必须声明为 `facet: true`；
- API：

```json
{
  "q": "phone",
  "facet_by": "brand"
}
```

返回：
```json
{
  "facet_counts": [{
    "field_name": "brand",
    "counts": [
      {"value": "Sony", "count": 12},
      {"value": "Apple", "count": 8}
    ]
  }]
}
```

源码：`src/aggregation/`、`src/field.cpp`。

### 8.5 几何搜索（GeoSearch）

- 字段类型 `geopoint`（lat/lng）；
- API：

```json
{
  "q": "hotel",
  "geo_filter": "loc:(-122.45, 37.78, 5km)"   // 圆
}
```

支持 `geo_bounding_box`、`geo_polygon`。源码：`src/geometry.cpp`。

### 8.6 向量检索（v25+ / v26+）

- 字段类型 `float[]` 或由 embedder 生成；
- API：

```json
POST /collections/products/documents/search
{
  "q": "red running shoes",
  "vector_query": "embedding:([0.1, 0.2, ...])",
  "hybrid_ranking": {
    "alpha": 0.5
  }
}
```

- **Hybrid ranking**：BM25 + 向量 + 线性权重；
- **Conversational Search**：基于 conversation_model 重写 query。

### 8.7 多样化 / 分组（v25+）

- **`diversify`**：`diversity_similarity` 0-1；
- **`group_by`**：按字段分组；
- **`group_limit`**：每组返回 N；
- **`include_fields` / `exclude_fields`**：字段过滤；
- **`enable_synonyms` / `synonyms`**：同义词；
- **源码**：`src/diversity.cpp`、`src/group.cpp`。

## 九、查询 / 计算 / 接口层（**搜索类型重点展开**）

### 9.1 DSL 设计

#### 9.1.1 主 API（极简）

```http
POST /collections/products/documents/search
{
  "q": "wireless phone",
  "query_by": "name,description",
  "query_by_weights": "4,2",
  "filter_by": "price:>100",
  "sort_by": "price:asc",
  "facet_by": "brand,category",
  "num_typos": 2,
  "page": 1,
  "per_page": 20,
  "highlight_full_fields": "name,description"
}
```

#### 9.1.2 Multi-Search

```http
POST /multi_search
{
  "searches": [
    { "collection": "products", "q": "phone" },
    { "collection": "articles", "q": "rust" }
  ]
}
```

源码：`src/http_server.cpp` + `src/raft_server.cpp`。

#### 9.1.3 类似 Lucene Query String

- Typesense **没有 Lucene syntax**；
- query 中可使用：
  - `title:hello` （字段限定）；
  - `tag:phone -discontinued` （- 排除）；
  - `"exact phrase"` （短语）；
  - `+required -excluded`（增强符号）；
  - `prefix:`（前缀搜索 v25+）。

源码：`src/query_analytics.cpp`、`src/parsing.cpp`。

### 9.2 聚合 / Faceting

Typesense 的"聚合" = **facet_by**，**类似 ES bucket aggregation**：

- 字段类型 `facet`；
- 支持 `num_facets`、`facet_sample_percent`、`facet_query`；
- 嵌套 faceting：`facet_by: "brand,categories"`（v25+ 多级 facet）；
- 不支持 ES-style `metric aggregation`（无 `sum / avg`），但支持 `quantity_*` 字段（v25+ 引入 quantity）。

### 9.3 多租户搜索（API Keys）

#### 9.3.1 API Key 类型

- **Admin**（root）：完全访问；
- **Search-only**：只读 + 限定 collection；
- **Scoped**：限定 collection + action + scope；
- **生成**：`POST /keys` 生成 scoped key。

#### 9.3.2 多租户场景

- SaaS 平台每个用户分配 scoped key；
- key 内嵌 `filter_by: tenant_id:=123`（通过 `metadata`）+ `limit_multi_searches`；
- 速率限制：multi_search / per_search；

源码：
- `src/api_key.cpp`、`src/auth_manager.cpp`、`src/collection.cpp`（权限检查）。

#### 9.3.3 Curated Results（v25+ 专属）

- 一组手工标注的 rank override；
- API：`override_tags`、`override_curation`、`override_rules`；
- 在相关性前位插入"硬排序"。
- 源码：`src/override.cpp`。

### 9.4 Conversational Search 与 NLP

- **`conversation`** 模式：基于 LLM 重写 query；
- **`enable_nlp_features`**：snowball + 多语言同义词；
- **`voice_query`**：内置 conversation_model 解析。
- 源码：`src/nlp/`、`src/conversation_model.cpp`。

## 十、事务与一致性

- **事务模型（极弱）**：
  - **单文档原子性**：通过 RocksDB `WriteBatch` 保证；
  - **跨文档**：没有跨 doc 事务；
  - **写入一致性**：默认 at-least-once，单实例下基本强一致；
- **快照**：`POST /operations/snapshot` 保存 RocksDB 快照；
- **集群一致性**（**仅商业版** Typesense Cluster）：
  - **Raft-based**（基于 `RaftServer`）；
  - 多数派写入、leader-followers；
  - 跨节点 RocksDB raft log 复制；
- **开源单实例**：单机内存数据，重启恢复自 RocksDB；
- **源码**：`src/raft_server.cpp`（商业版）、`src/store.cpp`。

## 十一、复制与高可用

- **单实例（开源）**：没有内建集群能力；
- **Typesense Cluster（商业版）**：
  - **Raft 共识**：内置 raft_server；
  - 数据分片；
  - leader + followers；
  - 网络分区自动恢复；
- **备份**：通过 RocksDB snapshot；
- **异地容灾**：通过 S3 等上传 snapshot；
- **高可用配置**：cluster size 3 节点起步，5/7 节点 majority 提升。

源码：
- `src/raft_server.cpp`：`raft` 协议骨架；
- `src/cluster_state.cpp`：raft state machine。

## 十二、运维与可观测性

- **监控**：
  - `/health` / `/debug` 端点；
  - 内建 Prometheus `/metrics`（v27+）；
- **Analytics**：
  - `analytics_*` 字段配置写日志；
  - `popular_queries` 提取；
- **慢请求日志**：`req_timeout` 内置；
- **运维工具**：
  - `typesense-cli`：scaffold / config；
  - **TSImport / TSExport**：bulk 工具；
- **Web UI**：内置 Mini Dashboard（`localhost:8108/dashboard`）。

## 十三、性能与基准

- **官方 benchmark（与 Algolia 对比）**：
  - 1M 文档集合 + typo + filter + faceting：<50ms；
  - 在多数场景 **与 Algolia 接近**；
- **vs Meilisearch**：
  - 性能接近，Typesense 略优（lib cpp 优化）；
  - 协议也接近，Typesense 多租户更强；
  - Meilisearch 协议略简、社区更大；
- **vs Elasticsearch**：
  - 数据量超过 10GB 后 ES 在聚合 + 复杂 DSL 上更强；
  - Typesense 在轻量 + 即时搜索 + typo + 多租户上领先。
- **优化建议**：
  - **filter / facet / sort 字段必须 schema 声明**；
  - **单实例文档数控制在 < 10M**；
  - **集群版本（商业）扩展性能**。

## 十四、生态与集成

- **客户端 SDK**（10+ 语言）；
- **InstantSearch.js（官方 fork）**：即时搜索前端组件；
- **Vue / React / Svelte / Vite** 适配组件；
- **DocSearch-typesense**：Algolia DocSearch v3 替代品；
- **ORM / RAG 集成**：
  - **LangChain / LlamaIndex**：作为 retriever；
  - **Directus** / **Strapi**（headless CMS）：TypeSense 适配；
- **容器化**：官方 Docker image、Helm Chart（社区）；
- **AI / RAG**：向量字段 `float[]` + `vector_query` + `hybrid_ranking`。

## 十五、学习路径

### 15.1 入门（1-2 周）

1. 跑通本地：Docker `docker run -p 8108:8108 -v "$(pwd)/typesense-data:/data" typesense/typesense:25.x --api-key=Hu52dwsas2Adxdh`；
2. 创建 collection：定义 fields；
3. 添加 docs；
4. 测试 search / faceting / filter / typo。

### 15.2 进阶（4-5 周）

1. **必读源码**：
   - `src/collection.cpp` + `src/collection.hpp`：集合核心；
   - `src/index.cpp` + `src/index.hpp`：text 索引；
   - `src/in_mem_index.cpp`：in-memory 倒排；
   - `src/filter_sort.cpp`：filter / sort；
   - `src/aggregation/`：faceting；
   - `src/levenshtein.cpp` + `src/word_alternator.cpp`：typo；
   - `src/auth_manager.cpp` + `src/api_key.cpp`：权限；
   - `src/server.cpp` + `src/http_server.cpp`：入口；
2. **集群原理**：`src/raft_server.cpp`（商业版）；
3. **矢量 / Hybrid**：`src/embedder*`、`src/hnsw_index.cpp`（如使用 nmslib 集成）。
4. **测试**：`tests/`、`fuzz/`。

### 15.3 高级（长期）

1. **向量扩展**：与 HNSW、Faiss、Qdrant 的差异；
2. **多租户 SLO**：scope 设计；
3. **Conv 模型**：LLM 重写 query；
4. **Raft integration**：自管的 raft_server。

### 15.4 推荐资源

- **官方**：typesense.org/docs、typesense.org/blog；
- **Benchmark**：`benchmark.typesense.org`；
- **行业参考**：Algolia 对比；
- **论文**：BM25 + Levenshtein + Raft 论文；
- **社区**：Forum、Discord、GitHub Discussions。

## 十六、风险与陷阱

1. **AGPL 协议**："修改并对外提供服务"也必须开源；非 SaaS 自托管需谨慎评估商业影响；
2. **单实例内存限制**：单实例最多 ~10M 文档；超出 OOM；商业 Cluster 才能水平扩展；
3. **filter / facet / sort 字段必须声明**：schema 不可变；
4. **typo tolerance 主题优化**：中文 / 日文等字符 typo 容易偏离预期；
5. **Curated 数据迁移**：升 / 降 curated 标签会引入行为漂移；
6. **集群仅商业版**：HA / 扩展 / 跨区域 → 需购买 Typesense Cluster；
7. **C++ 二次开发难度**：C++ 学习曲线 + 跨平台编译（mac / linux / windows）；
8. **Sort 字段 RAM 占用**：SortBy 字段在 RocksDB + ART 中持久化，但 query 时仍需部分内存；
9. **RocksDB 调优**：默认在 small SSD 上工作良好，nvme / raid0 / zfs 调优需了解；
10. **Embedder + Index Size**：`embedders` 生成的 vector 字段大，10M x 768dim 索引约 30GB+；
11. **延迟敏感集群**：跨节点 Raft 同步受 WAN 延迟影响，多 region 慎用；
12. **API Key 管理**：API Key 体系复杂，scope 调试容易遗漏，自托管导出/导入容易丢权限信息。

## 十七、与本项目的关联

本项目 `engineering/` **未实现** Typesense 兼容引擎，下列可借鉴：

- **In-Memory 倒排索引**：`engineering/src/algo/inverted_index.c` 实现简化版；
- **多租户 API Key 体系**：`engineering/src/algo/api_key_acl.c` 实现 scoped key + 多租户；
- **Levenshtein + 双精度 spell correction**：参考 `src/levenshtein.cpp` 在 `engineering/src/algo/levenshtein.c`；
- **GeoSearch**：`engineering/src/algo/geo.cpp` 实现圆 / 多边形；
- **RocksDB 集成**：`engineering/` 已用 RocksDB，可参考 `src/store.cpp` 集成 RocksDB 包装。

## 十八、参考资料

1. Typesense 官方文档：<https://typesense.org/docs/>
2. Typesense GitHub：<https://github.com/typesense/typesense>
3. Typesense Cloud：<https://cloud.typesense.org/>
4. InstantSearch.js：<https://github.com/typesense/instantsearch.js>
5. Vectore Search (v25+)：<https://typesense.org/docs/vector-search.html>
6. Conversational Search：<https://typesense.org/docs/conversational-search.html>
7. RocksDB 集成：<https://github.com/facebook/rocksdb>
8. Raft 论文：*In Search of an Understandable Consensus Algorithm*（Ongaro & Ousterhout, 2014）
9. Levenshtein 距离
10. BM25 论文
