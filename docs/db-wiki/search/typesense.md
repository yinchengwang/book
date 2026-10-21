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

## 十八、存储与索引分层：单 RocksDB 键空间 + 全内存倒排（算法展开）

> **版本与路径声明**：本节源码路径以 Typesense **v25.2**（tag `v25.2`，commit `06fd80c4c60949a89baa955886b20b18aea36293`）公开仓库为准。本仓库 `reference/search/typesense/` 子模块**未 init**（目录内除 `.git` 外 0 个条目），因此本节**只给 canonical 路径、不给行号**（approx，模块未 init，行号需校对）。

### 18.1 先纠正一个流传很广的说法：v25 没有 KAC 三层存储

早期 Typesense 资料常把存储描述为 **KAC（Key / Attribute / Condition）三库分离**——key 用自有倒排、attribute 与 condition 各落一个 RocksDB。**这个描述在 v25.x 已经不成立**，实测结论如下：

| 维度 | v25.2 实测 |
| --- | --- |
| RocksDB 实例数 | **每个 collection 一个**，由 `collection` 持有单个 `Store* store` 成员 |
| 关键字类 | 全文 `class KeyValueStore` / `AttributeKV` / `ConditionKV` 在 **v0.21.0 / v0.22.0 / v0.24.1 / v25.2 四个版本的全树扫描中均为 0 命中** |
| 倒排索引位置 | **完全在内存**（ART + posting list），RocksDB 只存文档 JSON、doc_id→seq_id 映射、collection meta、curated override |
| 排序 / facet / 数值过滤索引 | 同样**在内存**（`num_tree_t` / `adi_tree_t` / `facet_index_v3`），RocksDB 无关 |

源码路径：`include/store.h`（整个 `Store` 类，**v25.2 没有 `src/store.cpp`**）、`include/collection.h`、`src/collection.cpp`、`src/index.cpp`、`include/index.h`。

也就是说，v25 的分层不是「三层 KV」而是**「一层持久化键空间 + 八类内存索引结构」**。这个转变是理解后面所有算法的前提。

### 18.2 键空间布局：五个前缀 + 保序的 seq_id

`Store` 只有一个 RocksDB，所有数据靠 key 前缀区分。前缀常量定义在 `include/collection.h`（`Collection` 的 `static constexpr` 成员）：

| 常量 | 字面量 | key 构造 | 用途 |
| --- | --- | --- | --- |
| `COLLECTION_META_PREFIX` | `$CM` | `$CM_<collection_name>` | collection schema / meta JSON |
| `COLLECTION_NEXT_SEQ_PREFIX` | `$CS` | `$CS_<collection_name>` | 自增 seq_id 计数器（RocksDB Merge 累加） |
| `COLLECTION_OVERRIDE_PREFIX` | `$CO` | `$CO_<collection_name>_<override_id>` | curated results 记录 |
| `SEQ_ID_PREFIX` | `$SI` | `<collection_id>_$SI_<4 字节大端 seq_id>` | **文档 JSON 实体** |
| `DOC_ID_PREFIX` | `$DI` | `<collection_id>_$DI_<doc_id>` | 外部 doc_id → 内部 seq_id |

两个设计点值得展开：

**① `$` 前缀把元数据段与记录段在字节序上隔开**。`include/collection.h` 里那行注释写的是 *"Using a `$` prefix so that these meta keys stay above record entries in a lexicographically ordered KV store"*。可验证的机制是**前缀区间不相交**：记录键以 `<collection_id>` 开头（首字节是 `'0'`~`'9'`，即 0x30–0x39），元数据键以 `$`（0x24）开头，两段在字典序上**永远不相邻**，因此对记录段做 `Store::scan(prefix, upper_bound)` 区间扫描时不可能误伤 meta 键，反之亦然（`src/collection.cpp` 里另一处 `include_names.equal_prefix_range("$")` 正是靠这个性质只捞元数据）。注释里的 "above" 与严格升序字节序的措辞方向不一致，这里按可验证的前缀不相交来理解。

**② seq_id 用 4 字节大端序列化而非十进制字符串**。`Collection::get_seq_id_key()` 的注释点明：*"We can't simply do `std::to_string()` because we want to preserve the byte order."*。因为 RocksDB 的 Iterator 按 key 字节序前进，把 uint32 用大端序列化后**字典序 == 数值序**，于是「按 seq_id 顺序全量扫文档」退化成一次顺序 I/O：

```text
文档写入序：seq_id = 1, 2, 300, 1000, 20000

十进制字符串： "1" "2" "300" "1000" "20000"
  字典序扫描得到： "1" "1000" "2" "20000" "300"     ✗ 顺序完全错乱

4 字节大端：     00 00 00 01 | 00 00 00 02 | 00 00 01 2C | 00 00 03 E8 | 00 00 4E 20
  字典序扫描得到： 1, 2, 300, 1000, 20000                ✓ 与写入序一致
```

代价是 key 里固定多 4 字节且不可读；收益是**零排序的顺序扫描**。典型受益者是 schema 变更后的全量重建索引（`src/collection.cpp` 的 `Collection::batch_alter_data`）：

```cpp
const std::string seq_id_prefix  = get_seq_id_collection_prefix();       // "<collection_id>_$SI"
std::string upper_bound_key = get_seq_id_collection_prefix() + "`";      // 反引号 0x60
rocksdb::Slice upper_bound(upper_bound_key);
rocksdb::Iterator* iter = store->scan(seq_id_prefix, &upper_bound);
// 记录键是 "<prefix>_" + 4 字节，'_' = 0x5F < '`' = 0x60，故该上界恰好覆盖全部记录
```

这条路径因此是「顺序读一遍 + 分批（`index_batch_size = 1000`）重索引」，不需要把整个 collection 读进内存再排序。源码路径：`include/string_utils.h`（`serialize_uint32_t` / `deserialize_uint32_t`）、`src/collection.cpp`（`Collection::get_seq_id_key` / `get_doc_id_key` / `get_seq_id_collection_prefix`）。

### 18.3 内存索引家族：八类结构及其算法选型

`Index` 类（`include/index.h`）的私有成员就是全部索引结构，按用途分八类：

| 成员 | 容器 | 服务于 | 查找算法 | 复杂度 |
| --- | --- | --- | --- | --- |
| `search_index` | `spp::sparse_hash_map<string, art_tree*>` | 文本倒排 + **prefix** 查词典 | Adaptive Radix Tree，节点 4/16/48/256 四态 | 精确 $O(\log_{|\Sigma|} \|t\|)$；容错遍历按访问节点数计 |
| `numerical_index` | `spp::sparse_hash_map<string, num_tree_t*>` | `filter_by` 数值比较 | `std::map<int64, ids>` + 有序 id 数组归并 | 点查 $O(\log D + \text{posting})$，范围查 $O(\log D + |\text{区间内 id}|)$ |
| `geopoint_index` | `spp::sparse_hash_map<string, map<string, vector<uint32>>>` | GeoSearch | S2 几何索引（`s2/`），term → id 列表 | 与区域复杂度相关 |
| `sort_index` | `spp::sparse_hash_map<string, map<uint32_t, int64_t*>>` | 数值 sort / facet range | seq_id → int64 哈希表 | $O(1)$ 取值，排序在结果集上做 |
| `str_sort_index` | `spp::sparse_hash_map<string, adi_tree_t*>` | 字符串 sort | Adaptive Digit Index：nibble 树，每个节点存子树基数 `scions`，`rank()` 沿路径累加出排序名次 | 插入 / 求 rank $O(\|k\|)$ |
| `facet_index_v3` / `single_val_facet_index_v3` | `sparse_hash_map<string, array_mapped_facet_t>`（`ARRAY_FACET_DIM = 4`） | facet 计数 | `seq_id → uint64 facet hash`（数组字段存多个） | $O(1)$，4 路分片按 `seq_id % 4` |
| `infix_index` | `sparse_hash_map<string, array_mapped_infix_t>`，其中 `array_mapped_infix_t = std::vector<tsl::htrie_set<char>*>`、`ARRAY_INFIX_DIM = 4` | infix 搜索 | 每个 token 按 `hash_wy(token) % 4` 落进 4 个 `htrie_set<char>` 之一（哈希 trie 集合），infix 查询时遍历全部分片做子串匹配 | 写入 $O(\|t\|)$；查询 $O(\text{分片内 token 数} \times \|t\|)$ |
| `vector_index` | `spp::sparse_hash_map<string, hnsw_index_t*>` | kNN 向量检索 | HNSW 图（`hnswlib`） | $O(\log N)$ |

`ARRAY_FACET_DIM = 4` 的分片是一个纯工程优化：facet 是「seq_id → 值」的高基数映射，**按 `seq_id % 4` 拆成 4 个独立哈希表**，写入时随机落片、读取时按 seq_id 定位片，避免单表哈希冲突链过长、也避免扩容时一次性搬移全部数据。`src/index.cpp` 里到处是 `field_facet_mapping[seq_id % ARRAY_FACET_DIM]->find(seq_id)` 这样的两段式查找。infix 索引用了同样的「4 路分片」思路，但分片键是 **token 的 wyhash**（`infix_sets[strhash % 4]`）而非 seq_id。

> ⚠️ 旧节 §7.3 提到「排序字段拆分为多个 bucket，bucket 内排序」——v25.2 里数值 sort 字段是**单一的 seq_id→int64 哈希表**，没有 bucket 结构；被「分桶」的是**文本匹配分数**，见 §19.6。

### 18.4 倒排链：分块 + Frame-of-Reference 位压缩

`posting_list_t`（`include/posting_list.h`）是压缩链式块结构，每个块 `block_t` 的内存布局是 `[ids... | offset_index... | offsets...]`，其中 `ids` 与 `offset_index` 是 `sorted_array`，`offsets` 是普通 `array`。分块上限 `posting_t::MAX_BLOCK_ELEMENTS = 256`（`include/posting.h`），并用 `id_block_map`（`std::map<last_id_t, block_t*>`，记录每块的**最后一个 id**）支持按 id 随机定位到块。

真正有意思的是 `sorted_array` 的编码。它不是 varint，而是 **Frame-of-Reference（帧内基准）定宽位打包**：

```text
元数据区占前 METADATA_OVERHEAD = 5 字节，其后是定宽比特流。
sorted_array::indexOf() / numFoundOf() 里的取法（src/sorted_array.cpp）：
    uint32_t base = *(uint32_t *)(in + 0);      // 大端基准值 = 本块最小值
    uint32_t bits = *(uint32_t *)(in + 4);      // 位宽
    比特流起点 = in + METADATA_OVERHEAD，每个元素存 (value - base) 的 bits 位

required_bits(v) = (v == 0) ? 0 : 32 - __builtin_clz(v)                      // include/array_base.h
sorted_append_size_required(v, n) = METADATA_OVERHEAD + 4
                                  + for_compressed_size_bits(n, required_bits(max - min))
                                  // 源码里的写法：bnew = required_bits(M - m); size_bits = for_compressed_size_bits(new_length, bnew)
```

这带来两个性质：

- **位宽由块的极差决定，而不是由元素个数决定**。倒排链里 doc_id 递增且相邻 id 的间隔通常很小（一次批量导入后 `max - min` 与元素个数同量级），`bits` 会被压到接近 $\lceil \log_2 (N) \rceil + $ 常数，即**每个 id 平均只花 1~2 字节而不是固定 4 字节**；
- **代价是非追加插入会触发整块重编码**。`sorted_array::append()` 的第一行就是：

```cpp
size_t sorted_array::append(uint32_t value) {
    if (value < max) {
        // 乱序追加：解压整个数组 → 找插入位 → 整体后移 → 重新 load()（重新压缩）
        uint32_t* arr = uncompress(length + 1);
        uint32_t gte_index = for_lower_bound_search(in, length, value, &found_val);
        ...
        load(arr, length + 1);
    } else {
        // 顺序追加：只调 for_append_sorted() 增量编码，O(新元素)
    }
}
```

Typesense 的写入路径恰好**总是顺序追加**（seq_id 单调递增写入），所以走的永远是右边这条 $O(1)$ 摊还分支；这条优化不是通用有序集优化，而是**与写入路径强耦合**的设计。

底层 `for.h` 来自外部库 `libfor`（`https://github.com/cruppstahl/libfor`，CMake 里锁 commit `49611808d08d4e47116aa2a3ddcabeb418f405f7`），暴露 `for_compress_sorted` / `for_append_sorted` / `for_select` / `for_select_bits` / `for_lower_bound_search` / `for_compressed_size_bits` 六个原语。

还有一层内存优化：**短倒排链不建块**。`posting_t::COMPACT_LIST_THRESHOLD_LENGTH = 64`——posting list 用一段连续 `uint32_t` 数组表示（`compact_posting_list_t`，格式 `[num_offsets, offset1..offsetn, id1 | num_offsets, offset1.., id2 | …]`），只有当容量需求越过 64 才升级成块链（`src/posting.cpp` 的 `posting_t::upsert`）：

```cpp
if(IS_COMPACT_POSTING(obj)) {
    int64_t extra_capacity_required = list->upsert(id, offsets);
    if(extra_capacity_required == 0) return;                              // 紧凑数组还装得下
    if((list->capacity + extra_capacity_required) > COMPACT_LIST_THRESHOLD_LENGTH) {
        obj = list->to_full_posting_list();                               // 升级为 posting_list_t 块链
    } else {
        // 只按 1.3 倍增长，且封顶在阈值内
        size_t new_capacity = min((list->capacity + extra_capacity_required) * 1.3,
                                 COMPACT_LIST_THRESHOLD_LENGTH);
        list = (compact_posting_list_t*) realloc(list, ...);
    }
}
```

而且转换是**双向**的：`src/posting.cpp` 里还有一条降级路径，当块链只剩 1 块且 `(2*ids + offsets) <= 64` 时又退回紧凑数组。低频词（绝大多数 token）因此只付一份扁平数组的成本，且随 token 热度在两种表示之间自动切换。

源码路径：`include/posting_list.h`、`src/posting_list.cpp`、`include/posting.h`、`src/posting.cpp`、`include/sorted_array.h`、`src/sorted_array.cpp`、`include/array_base.h`、`include/ids_t.h`、`src/ids_t.cpp`。

### 18.5 RocksDB 侧的参数与自定义 Merge 算子

`Store` 的构造函数（`include/store.h`）把 RocksDB 当成一个「KV + 计数器」混合库用，关键参数：

| 选项 | 值 | 说明 |
| --- | --- | --- |
| `write_buffer_size` | `4 * 1048576`（4 MB） | memtable 上限 |
| `max_write_buffer_number` | `2` | 最多 2 个 memtable |
| `compression` | `kSnappyCompression` | |
| `max_log_file_size` / `keep_log_file_num` | 4 MB / 5 | WAL 轮转与保留 |
| `WAL_ttl_seconds` / `WAL_size_limit_MB` | `24*60*60` / `1024` | **为复制场景调高**：replica 通过 `GetUpdatesSince(seq)` 拉 master 的 WAL，所以 WAL 必须活够久 |
| `merge_operator` | `UInt64AddOperator` | 自定义 |
| `OptimizeLevelStyleCompaction()` | — | 显式开启 leveled compaction 调优 |
| `write_options.disableWAL` | 默认 **true** | master 用 braft 的 WAL，本地不再写一遍 |

`UInt64AddOperator` 是理解 `$CS` 计数器并发安全的关键：它实现 `rocksdb::AssociativeMergeOperator`，写入走 `Store::increment(key, value)` → `db->Merge(key, serialize_uint32_t(value))`，RocksDB 保证同一 key 的 merge 串行化，于是「读 seq_id → 加 1 → 写回」的复合操作退化成一次原子 `Merge`：

```cpp
class UInt64AddOperator : public rocksdb::AssociativeMergeOperator {
    bool Merge(const Slice& key, const Slice* existing_value, const Slice* value,
               std::string* new_value, rocksdb::Logger*) const override {
        uint64_t existing = existing_value ? StringUtils::deserialize_uint32_t(existing_value->ToString()) : 0;
        *new_value = StringUtils::serialize_uint32_t(existing + StringUtils::deserialize_uint32_t(value.ToString()));
        return true;
    }
    const char* Name() const override { return "UInt64AddOperator"; }
};
```

并发控制方面，`Store` 用一把 `std::shared_mutex`：**除 `reload()` 的 unique_lock 外全部是 shared_lock**，注释说明 RocksDB 的 `DB*` 本身线程安全，这把锁只是保护「重开数据库时对 `db` 指针的赋值」这一个竞态。

复杂度：`Store::get` / `insert` / `remove` 是 $O(1)$ 次 LSM 查找（实际 $O(\log_{\text{levels}} N)$）；`scan(prefix, upper_bound)` 是 $O(K \log_{\text{levels}} N)$（$K$ = 命中条数）。源码路径：`include/store.h`、`include/string_utils.h`。

### 18.6 写入路径的代价与「写放大」的真实含义

对 Typesense 而言，**内存索引才是昂贵的那个**。`src/collection.cpp` 的 `Collection::batch_index` 把两件事分成两步，代价分布一目了然：先 `batch_index_in_memory(...)` 做内存索引，再对**只有 `indexed.ok()` 的记录**落盘。内存那一步要做的是：分词、算 facet hash（wyhash 变体）、往 ART 里插入 token 叶子并 upsert posting list（**已存在则整块重解码**）、往 4 路 facet 表写 seq_id、往 infix 的 htrie_set（按 token 哈希分 4 片）写 token、往 num_tree / adi_tree / sort_index / hnsw 各写一份。

按「一次文档 upsert」计：

$$\text{总代价} \;\approx\; \sum_{f \in \text{可索引字段}} \underbrace{O(|f|)}_{\text{分词}} + \underbrace{O(T_f \log_{|\Sigma|} \|t\|)}_{\text{ART 插入}} + \underbrace{O(D_f \cdot b_f)}_{\text{posting 重编码}}$$

其中 $T_f$ 是该字段的 token 数、$D_f$ 是该 token 的 posting list 长度、$b_f$ 是块的位宽（§18.4）。对**高频词的 posting list**，追加一个新 id 就要整块解压-插入-重压缩，代价 $O(256 \cdot b_f)$——这是 Typesense 写入放大的主要来源，也是它**把全部索引常驻内存、不做增量段合并**的代价：换来 $O(1)$ 的查询延迟，付出的是全部写代价与固定的 RAM 占用（对应 §十六 风险清单里的「单实例内存限制」）。

落盘那一步反而很轻：新增走 `store->batch_write(batch)`（一个 `WriteBatch` 里放 doc_id→seq_id 与 seq_id→JSON 两项），更新走 `store->insert(get_seq_id_key(...), serialized_json)`，都是 $O(\log N)$ 的 LSM 追加。

一个常被误解的类：`cached_resource_stat_t`（`include/cached_resource_stat.h`、`src/cached_resource_stat.cpp`）**不是查询缓存**，而是批量导入前的资源闸门——`has_enough_resources()` 检查磁盘余量与内存余量，返回 `OK / OUT_OF_DISK / OUT_OF_MEMORY`，`src/batched_indexer.cpp` 与 `src/raft_server.cpp` 在 enqueue 前调用它决定是否降级批量大小。源码路径：`include/batched_indexer.h`、`src/batched_indexer.cpp`、`include/cached_resource_stat.h`、`src/cached_resource_stat.cpp`。

## 十九、召回与排序：Topster 有界堆、打包式 match score 与 proximity 滑窗（算法展开）

> **版本与路径声明**：同 §十八，均以 Typesense **v25.2** 为准；`reference/search/typesense/` 子模块未 init，本节只给 canonical 路径、不给行号。

### 19.1 先纠正一个说法：候选集不是「4×k 两阶段」

常见的「Typesense 先粗召回 4×k 再精排」的描述与 v25.2 实测不符。真实情况是**三个互相独立的截断，且倍数 4 出现在 token 层而不是文档层**：

| 层级 | 容量 | 常量 / 出处 | 语义 |
| --- | --- | --- | --- |
| **token 候选** | `max_results * 4` | `src/art.cpp` 的 `art_topk_iter`：`while(!q.empty() && results.size() < max_results*4)` | 单个查询 token 从 ART 模糊遍历里最多吐出的候选词数 |
| **token 候选上限** | `NUM_CANDIDATES_DEFAULT_MAX = 10` / `NUM_CANDIDATES_DEFAULT_MIN = 4` | `include/index.h`；`src/collection_manager.cpp` 按 `num_documents < 500000` 二选一 | 每个查询 token 最多保留多少个候选词 |
| **文档候选集** | `DEFAULT_TOPSTER_SIZE = 250` | `include/collection.h` | Topster 的容量；与 `PER_PAGE_MAX = 250` 取值一致（单页上限就是候选上限） |

文档级的 `max_hits` 计算（`src/collection.cpp`）是：

```cpp
size_t max_hits = DEFAULT_TOPSTER_SIZE;                                  // 250
max_hits = std::min(std::max(fetch_size, max_hits), get_num_documents());
```

即「**至少 250，且不超过本页所需的 `offset + per_page`，再被文档总数夹住**」。深分页（`offset` 很大）会自动放大候选集——因为浅层翻页只需要前 250，深层翻页需要更多。

> ⚠️ 上面两行 `if(weighted_search_fields.size() <= 1 || query == "*")` 与 `else` 分支**代码完全相同**，是遗留的空分支（应为向量搜索保留）。此处如实记录，不做评价。

### 19.2 Topster：有界 top-K 最小堆

`Topster`（`include/topster.h`）是候选集的容器：固定容量 `MAX_SIZE` 的**最小堆**，用 `data`（连续的 `KV` 数组）+ `kvs`（指向 `data` 的指针数组）双层布局，堆操作只交换指针、值留在原地，从而把 sift 的数据搬移降到一次指针交换。关键在于 `add()`：

```text
add(kv):
  less_than_min_heap = (size >= MAX_SIZE) && is_smaller(kv, kvs[0])
  if (!distinct && less_than_min_heap):
      return 0                      # 堆已满且比堆顶还差 → 直接丢弃，O(1)

  if kv.key 已在 kv_map 中:
      if is_smaller(kv, existing): return 0     # 同 key 的更差候选，丢弃
      SIFT_DOWN = true; heap_op_index = existing.array_index   # 原地替换后下沉
  else:
      if size < MAX_SIZE: SIFT_DOWN = false; heap_op_index = size; size++
      else:               SIFT_DOWN = true;  heap_op_index = 0   # 顶掉堆顶后下沉

  kv->array_index = heap_op_index
  kvs[heap_op_index] = *kv
  手工 sift down（比较 (scores[0], scores[1], scores[2], key) 四元组）
```

复杂度：

| 操作 | 复杂度 | 说明 |
| --- | --- | --- |
| 随机到达的差候选（最常见） | **$O(1)$** | 堆满 + 不比堆顶好，直接 return |
| 进入/替换候选 | $O(\log K)$ | 手工 sift down / up |
| 同 key 去重 | $O(1)$ 期望 | `kv_map` 哈希 |
| `sort()` | $O(K \log K)$ | `std::stable_sort`，只在最后调用一次 |
| 内存 | $O(K)$，$K \le$ 250 | 与文档数**无关**，这是「单实例支持千万文档但内存可控」的根本原因 |

比较谓词是四元组字典序：

```cpp
static bool is_greater(const KV* i, const KV* j) {
    return std::tie(i->scores[0], i->scores[1], i->scores[2], i->key) >
           std::tie(j->scores[0], j->scores[1], j->scores[2], j->key);
}
```

`scores[3]` 里 `[0]` 是 match score、`[1..2]` 是最多两个自定义 sort 属性；`match_score_index` 指向真正生效的那个（`-1` 表示本次查询没有文本相关性项，比如纯 filter 或纯向量检索）。最后用 `key`（seq_id）兜底，保证排序**全序且稳定**。

源码路径：`include/topster.h`（Topster / KV）、`include/index.h`（`search_args` 的构造函数里 `new Topster(...)`、析构里 `delete`）、`src/collection.cpp`（`populate_result_kvs` 取出结果）。

### 19.3 Match：WINDOW_SIZE = 10 的滑窗 proximity 算法

proximity（邻近度）的计算在 `include/match_score.h` 的 `Match` 构造函数里，源码注释本身就是一份算法说明：

```
Given *sorted offsets* of each target token in a *single* document (token_offsets),
generates a score indicating:
  a) How many tokens are present within a match window
  b) The proximity between the tokens within the match window

How it works:
  1. Create vector with first offset from each token.
  2. Sort vector descending.
  3. Calculate distance, use only tokens within max window size from lowest offset.
  4. Reassign best window and distance if found.
  5. Pop end of vector (smallest offset).
  6. Push to vector next offset of token just popped.
  7. Until queue size is 1.
```

算法本体（伪代码化，保留原始变量名）：

```text
WINDOW_SIZE = 10
tokens_size = min(token_offsets.size(), WINDOW_SIZE)
window = [ (token_id=i, positions[i][0]) for i in 0..tokens_size-1 ]
best_num_match = 1; best_displacement = MAX_DISPLACEMENT(65535); prev_min_offset = -1

while window.size() > 1:
    按 offset 降序排 window            # size==2/3 走手写 sort2/sort3，避免 std::sort 的常数
    min_offset = window.back().offset
    if min_offset < prev_min_offset: break      # offset 回绕（超长文档），提前退出
    prev_min_offset = min_offset

    this_displacement = 0; this_num_match = 0
    for i in 0..window.size()-1:
        next_offset = (i == size-1) ? window[i].offset : window[i+1].offset
        if (window[i].offset - min_offset) <= WINDOW_SIZE:
            this_displacement += window[i].offset - next_offset   # 相邻命中位置的间隙和
            this_num_match++
    # 源码此处还会累加 all_offsets_are_same（整窗 offset 是否全同），但该变量计算后未被使用

    if this_num_match > best_num_match
       or (this_num_match == best_num_match and this_displacement < best_displacement):
        best_num_match = this_num_match; best_displacement = this_displacement
        max_offset = min(255, window.front().offset)

    if best_num_match == tokens_size and best_displacement == size-1:
        break                                # 已是理论最优，提前退出

    smallest = window.pop_back()             # 弹出当前最小 offset 所在的 token
    if smallest.offset != 该 token 的最后一个 offset:
        window.push(该 token 的下一个 offset)

words_present = best_num_match
distance      = uint8(best_displacement)
```

要点：

- **位移的定义是「窗口内相邻命中位置之间的间隙之和」**，不是首尾跨度。这使得文档中多出来的噪声 token 不会拉大 distance；
- **`WINDOW_SIZE = 10` 同时是「最多考虑前 10 个查询 token」和「跨距阈值」**：`tokens_size = min(query_token 数, 10)`，且只统计 `(offset - min_offset) <= 10` 的 token。长查询的远端 token 直接不进 proximity 计算；
- **只滑动「最小 offset 所在的那个 token」**：这是标准的 k 路归并滑动窗口，每轮 $O(k \log k)$（这里 $k \le 10$，手写 sort2/sort3 消掉 `std::sort` 的函数指针/内联损失），总轮数等于所有 token 的 position 总数，代价 $O(\sum_i |\text{positions}[i]| \cdot 10 \log 10)$；
- **`distance` 是 `uint8_t`**，隐含不变量 `best_displacement \le 255`。

### 19.4 get_match_score：把六个分量打包成一个 uint64

`Match::get_match_score()` 把全部相关性信号压进**一个 uint64**，使得「比大小」这件事退化成一次整数比较，且比较顺序恰好是业务优先级顺序：

```cpp
uint64_t match_score = (
    (int64_t(unique_words)   << 40) |   // bit 40..55  命中的唯一 token 数
    (int64_t(words_present)  << 32) |   // bit 32..39  落在窗口内的 token 数
    (int64_t(255 - total_cost)<< 24) |   // bit 24..31  typo 总代价（越小越好）
    (int64_t(100 - distance) << 16) |   // bit 16..23  proximity 位移（越小越好）
    (int64_t(exact_match)    <<  8) |   // bit  8..15  是否完整短语匹配
    (int64_t(255 - max_offset)<<  0)     // bit  0..7   命中位置（越靠前越好）
);
```

按「字典序 = 优先级」逐位展开：

| 优先级 | 位段 | 字段 | 方向 |
| --- | --- | --- | --- |
| 1（最高） | 40–55 | `unique_words` | 大者优 |
| 2 | 32–39 | `words_present` | 大者优 |
| 3 | 24–31 | `255 - total_cost` | 大者优（typo 越少越好） |
| 4 | 16–23 | `100 - distance` | 大者优（词越近越好） |
| 5 | 8–15 | `exact_match` | 1 优于 0 |
| 6（最低） | 0–7 | `255 - max_offset` | 大者优（命中越靠前越好） |

这个打包是**纯排序优化**（省掉比较时的元组解包与分支），不是 BM25 之类加权求和。**代价是对每个分量的取值范围有硬约束**：

- `unique_words << 40` 要求 `unique_words < 2^{24}`（否则会溢出到 bit 64 之外）；
- `100 - distance << 16` 要求 `distance \le 100`——§19.3 里由 `WINDOW_SIZE = 10` 天然保证（位移上界约 $O(10^2)$）。若该不变量被破坏导致该项为负，`int64_t(-30) << 16` 的负数左移会**破坏整个 64 位打包的字典序性质**，排序结果会静默退化而不是报错。这是打包式编码的典型脆弱点。

源码路径：`include/match_score.h`。

> ⚠️ 旧节 §8.2 写「BM25：默认相关性」。v25.2 全树**不含 BM25 / IDF / tf-idf 实现**，相关性完全由上述 packed score 表达。新节如实给出真实公式，§8.2 原文未改。

### 19.5 精确短语：found_token_sequence 的递归判定

`exact_match` 由 `Match` 构造函数的 `check_exact_match` 分支决定，其底层是 `posting_list_t::found_token_sequence`（`src/posting_list.cpp`）——**判定「各 token 的 position 是否恰好构成一条连续上升的序列」**：

```text
found_token_sequence(token_positions, token_index, target_pos):
  if token_index == len(token_positions): return true          # 全部对齐 → 短语命中
  for tok_pos in token_positions[token_index].positions:        # positions 升序
      if tok_pos < prev_pos: return false                       # 回绕，长文档的保护
      if tok_pos == target_pos: return found_token_sequence(..., token_index+1, target_pos+1)
      prev_pos = tok_pos
  return false
```

调用方 `has_phrase_match` 从第一个 token 的每个 position `pos` 出发，尝试 `found_token_sequence(token_positions, 1, pos + 1)`。

`Match` 侧还有两个更早的剪枝（在进入短语判定之前就 `return`，直接放弃 `exact_match`）：

```cpp
// include/match_score.h，check_exact_match 分支
if (distance > token_offsets.size() - 1) return;              // 位移已超查询长度，不可能完整匹配
...
if (total_offsets > token_offsets.size() && distance == token_offsets.size() - 1) return;
                                                                // 总命中位置数超查询长度 → 必含噪声 token
```

单 token 的「verbatim match」走 `is_single_token_verbatim_match`：对非数组字段，先用 `offsets[start_offset] != 1` **一次比较**（该 token 在文档里只出现一次）快速判否，再走完整扫描。源码路径：`include/match_score.h`、`include/posting_list.h`、`src/posting_list.cpp`、`src/posting.cpp`。

### 19.6 text_match_buckets：把分数分桶以稳定分页

深分页的经典难题：文档是**按写入顺序**逐个进入 Topster 的，同一批次的分数很接近；若直接对 Topster 前 `fetch_size` 条排序，第 2 页和第 3 页的排序键会随「哪些文档恰好进了 Topster」抖动。Typesense 的解法是**分桶 + 排序 + 还原**（`src/collection.cpp`）：

```text
num_buckets         = sort_fields[match_score_index].text_match_buckets
max_kvs_bucketed    = min(DEFAULT_TOPSTER_SIZE, raw_result_kvs.size())     # 250
block_len           = max_kvs_bucketed / num_buckets

# 阶段一：按当前分数排序后，每 block_len 条共用一个「锚点分数」= 该块第一条的分数
i = 0
while i < max_kvs_bucketed:
    anchor_score = raw_result_kvs[i][0]->scores[match_score_index]
    for j in 0..block_len-1 while i+j < max_kvs_bucketed:
        result_scores[key(i+j)] = scores[i+j]        # 备份原分
        scores[i+j]            = anchor_score        # 抹平成桶分数
    i += block_len

# 阶段二：只对前 250 条按桶分数 partial_sort（桶内维持阶段一的原序，稳定）
std::partial_sort(begin, begin+max_kvs_bucketed, end, Topster::is_greater_kv_group)

# 阶段三：还原真实分数，留给高亮与 bucket 展示用
for i in 0..max_kvs_bucketed: scores[i] = result_scores[key(i)]
```

桶内顺序来自阶段一的稳定排序，因此同一桶内的相对次序是**确定的**，翻页不会因为新文档写入而错乱。

### 19.7 复杂度总表

| 阶段 | 复杂度 | 备注 |
| --- | --- | --- |
| ART 精确查词典 | 最坏 $O(\|t\|)$，节点扇出 4/16/48/256 自适应 | 深度即 key 长度，扇出越大深度越小 |
| ART 模糊遍历 | $O(\text{访问节点数} \cdot \|t\|)$ | 每字符滚动一次编辑距离，§20.2 |
| posting list 交集 | $O(\sum_i \text{遍历过的块数})$ | `posting_t::block_intersector_t` 先按 `num_blocks()` 升序排列各链；`posting_list_t::block_intersect` 对 1 / 2 / N 条链分别特化（1 条线性扫、2 条 `equals2`+`advance_non_largest2`、N 条多元归并），每 65536 个 id 查一次熔断 |
| proximity 滑窗 | $O(\sum_i \|\text{positions}[i]\| \cdot 10\log 10)$ | §19.3 |
| Topster add（多数） | $O(1)$ | 堆满且更差 → 直接丢 |
| Topster add（进入） | $O(\log 250) \approx 8$ | |
| Topster sort | $O(250 \log 250)$ | 每次查询一次 |
| 分桶排序 | $O(250 \log 250)$ | 只前 250 条 |
| 结果集取文档 | $O(\text{本页命中数})$ 次 RocksDB 点查 | `get_document_from_store` 按 seq_id 逐条读 |

---

## 二十、容错、分词与向量融合：typo 档位、ICU 分词与 RRF（算法展开）

> **版本与路径声明**：同 §十八，均以 Typesense **v25.2** 为准；`reference/search/typesense/` 子模块未 init，本节只给 canonical 路径、不给行号。

### 20.1 typo 档位：get_bounded_typo_cost

容错档位不是「统一 1 个 typo」，而是**按 token 长度阶梯判定**（`src/index.cpp` 的 `Index::get_bounded_typo_cost`）：

```cpp
int Index::get_bounded_typo_cost(const size_t max_cost, const size_t token_len,
                                 const size_t min_len_1typo, const size_t min_len_2typo) {
    if (token_len < min_len_1typo) return 0;                  // 太短的词完全不容错
    if (token_len < min_len_2typo) return min<int>(max_cost, 1); // 只允许 1 个 typo
    return min<int>(max_cost, 2);
}
```

默认值 `min_len_1typo = 4`、`min_len_2typo = 7`（`include/collection.h`）。用户可用 `num_typos` 显式指定，但要再被这三条规则夹住：

```cpp
size_t max_cost = (max_typos < 0 || max_typos > 2) ? 2 : max_typos;   // 越界钳到 2
if (locale != "" && locale != "en" && !Tokenizer::is_cyrillic(locale))
    max_cost = 0;                                    // 非英语非西里尔 locale 直接关闭模糊遍历
```

> ⚠️ 旧节 §8.3 写「默认：≥5 char → 1 typo；≥9 char → 2 typo」。v25.2 实测默认是 **≥4 / ≥7**。新节给出正确值，§8.3 原文未改。

> ⚠️ **非英语 locale 不做 typo 容错**（西里尔语除外）。这一条在 §20.4 讲分词时会再回来呼应：CJK 走的是词典切分 + 归一化，不是编辑距离纠正。

### 20.2 Damerau-Levenshtein 三行滚动 + ART 上的剪枝

编辑距离**不是独立文件**，而是内联在 ART 模糊遍历里的一个 helper（`src/art.cpp` 的 `levenshtein_dist`）：

```cpp
static inline void levenshtein_dist(const int depth, const unsigned char p, const unsigned char c,
                                    const unsigned char* term, const int term_len,
                                    const int* irow, const int* jrow, int* krow) {
    krow[0] = jrow[0] + 1;
    for (int column = 1; column <= term_len; column++) {
        int cost = (c == term[column-1]) ? 0 : 1;
        int delete_cost    = jrow[column]   + 1;
        int insert_cost    = krow[column-1] + 1;
        int substitution   = jrow[column-1] + cost;
        krow[column] = min(min(insert_cost, delete_cost), substitution);
        if (depth > 1 && column > 1 && c == term[column-2] && p == term[column-1]) {
            krow[column] = min(krow[column], irow[column-2] + 1);   // 相邻换位：Damerau 分支
        }
    }
}
```

三个设计点：

1. **Damerau-Levenshtein（OSA）而非纯 Levenshtein**，多出的那一行处理「相邻字符换位」（`ab` → `ba`），这在打字场景里比普通编辑距离更贴近真实错字；
2. **三行滚动**：调用方维护 `row0/row1/row2` 三个长度为 `term_len + 1` 的行缓冲，每字符 `rotate(i, j, k)` 一次，$O(1)$ 摊还、$O(\|t\|)$ 空间——空间只与**查询 token 长度**有关，与索引规模无关；
3. **在 Trie 上增量计算**：标准 Levenshtein 需要二维 DP 矩阵；这里把「已消费的前缀」这维压掉，只留一维，从而可以对 ART 树的每条根到叶路径复用同一套滚动行。这正是**用 Trie 做容错检索**而非「先枚举词典再逐个算距离」的根本原因——前者访问的节点数就是候选数，后者是全词典。

剪枝由 `fuzzy_search_state` 承担：

```text
key_len = last_key_char ? key_index : key_index + 1

if last_key_char:                                    # 已到叶末
    if min_cost <= cost_row[query_len] <= max_cost: return 1     # 命中
    # 特例：q="strawberries" on key="strawberry"（查询比 key 长）
    if key_len > 5 and query_len > key_len and (query_len - key_len) <= max_cost
       and min_cost <= cost_row[key_len] <= max_cost - 1: return 1
    return -1

cost = cost_row[min(key_len, query_len)]
if key_len >= query_len and prefix:
    if min_cost <= cost <= max_cost: return 1      # prefix 查询：前缀对上就返回（容忍尾部 typo）

# 提前剪枝：cost 可能中途超标后再降回来，故对长 key 放宽到 2 倍阈值
return (cost > max_cost and (key_len > 3 ? cost > max_cost * 2 : true)) ? -1 : 0
```

那条「`cost` 可能中途超标但最终回落」的注释配了例子：key=`example`、query=`exZZample`，处理到第 5 个字符时 cost = 3，但末尾会降回 2。所以对短 key（`key_len <= 3`）不宽容，对长 key 允许临时到 `2 * max_cost` 再继续。**这是容错检索里唯一可能漏召回的剪枝**，取舍明确写在代码注释里。

复杂度：单条路径 $O(\|t\|)$；整体是 $O(\text{被访问节点数} \cdot \|t\|)$。候选吐出上限由 `art_topk_iter` 的 `max_results * 4` 兜底（§19.1）。源码路径：`src/art.cpp`、`include/art.h`、`src/index.cpp`。

### 20.3 prefix × typo 的组合形态

prefix 与 typo 在代码里是**同一个遍历的两组布尔开关**，会产生三种查询形态：

| 形态 | `prefix` | `max_cost` | 行为 |
| --- | --- | --- | --- |
| 精确 | false | 0 | `levenshtein_dist` 照跑但全 0 代价，`cost_row[term_len] == 0` 才收 |
| prefix | true | 0 | `fuzzy_search_state` 在 `key_len >= query_len` 处提前收；`art_fuzzy_search` 里 `key_len = prefix ? term_len + 1 : term_len` ——**多看一个字符**，这样 `apple` 能匹配到 `apples` |
| prefix + typo | true | ≥1 | 保留尾部容错：`iter_len = min(key_len, term_len + max_cost)`，注释写明 *"look past term_len to deal with trailing typo, e.g. searching `pltinum` on `platinum` @ max_cost = 1"*。此时 `!prefix \|\| !last_key_char` 条件让**末字符那一行不更新**，等于把末字符排除在编辑距离之外 |

组合爆炸由 `combination_limit` 兜住。`src/index.cpp` 里两层限制：

```cpp
// 外层：token × cost 的笛卡尔积
int bounded_cost = get_bounded_typo_cost(max_cost, token.length(), min_len_1typo, min_len_2typo);
for (int cost = 0; cost <= bounded_cost; cost++) all_costs.push_back(cost);
// 3-token 查询的 cost 组合：[0,0,0] [0,0,1] [0,1,1] ...

long long N = product over tokens of (bounded_cost_i + 1);
const size_t combination_limit = exhaustive_search ? COMBINATION_MAX_LIMIT /*10000*/ : COMBINATION_MIN_LIMIT /*10*/;
while (n < N && n < combination_limit) { … }
```

也就是说：**默认只枚举前 10 种 cost 组合**，开启 `exhaustive_search` 才放到 10000。同一个 `combination_limit` 也被复用成「最多尝试多少个 token 候选」的阈值（`max_candidates` 的默认值 10 / 4 就是 `NUM_CANDIDATES_DEFAULT_*`）。

另外两个阈值参数控制的是**「什么时候停止再试更差的变体」**，默认值都是 1：

```cpp
// src/index.cpp，容错扩展循环末尾
resume_typo_loop:
    if (!exhaustive_search && all_result_ids_len >= typo_tokens_threshold) return;  // 已有结果就不再试更多 typo 变体
    n++;

// src/index.cpp，drop-token 循环入口
if (exhaustive_search || all_result_ids_len < drop_tokens_threshold) {
    // 结果还不够多时，才继续「从右往左逐个丢弃查询 token」找召回
}
```

默认阈值 1 意味着：**只要精确/低变体已经捞到结果，就不再为了「更多结果」去牺牲精确度**。这正是 Algolia 式「精确优先」体验的实现方式——代价是排名字典序靠后的召回变体默认不会被尝试，开 `exhaustive_search` 才会全部枚举。

### 20.4 locale 分词：ICU BreakIterator / kakasi / iconv（没有 bigram）

**v25.2 全树不含 `bigram` / `ngram` 任何实现**。CJK 走的是「**归一化 + 词典切分**」，不是 n-gram 索引。`src/tokenizer.cpp` 的 `Tokenizer` 按 locale 分三路：

```text
路径 A：locale 非空且 ≠ "en"   → ICU 词边界切分（词典法）
    zh：先 icu::Transliterator::createInstance("Traditional-Simplified") 繁→简
    ja：JapaneseLocalizer::normalize()  → 底层 kakasi_do()，参数 "./kakasi -JH -KH -s -iutf8 -outf8"
    ko：逐词 NFKD 归一化
    th：逐词 NFKC 归一化
    西里尔（el/ru/sr/uk/be）：逐词 transliterate("Any-Latin; Latin-ASCII")
    fa：把 ZWNJ (U+200C) 全部替换成空格
    切分本身：icu::BreakIterator::createWordInstance(icu_locale)

路径 B：locale 为空或 "en"      → 字节流手工切分
    ASCII 字符：按 symbols_to_index / token_separators 三态流模式处理
    非 ASCII：手工拼 UTF-8 多字节序列，再用 iconv("ASCII//TRANSLIT", "UTF-8") 转写
               转写失败 (EILSEQ) 则原样保留

路径 C：no_op = true            → 整段文本作为单个 token（数值/地理等字段用）
```

分词时还有一个 locale 专属的**标点剔除**：

```cpp
if (locale == "zh" && (word == "，" || word == "─" || word == "。")) emit_token = false;
else if (locale == "ko" && word == "·")                             emit_token = false;
```

日文路径的依赖链值得单独记：`src/japanese_localizer.cpp` 把 base64 内嵌词典写到 `/tmp/kanwa.data`、`/tmp/itaji.data`，设 `KANWADICTPATH` / `ITAIJIDICTPATH` 环境变量，再调 `kakasi_getopt_argv()` 初始化 kakasi 的 C 库，运行时通过 `kakasi_do(text)` 做归一化（katakana→hiragana 等）。**归一化在 `init()` 里做一次全文级，`next()` 里再做逐词级**（`JapaneseLocalizer::normalize` 被调用两次）。

这条链带来 §20.1 那条规则的必然性：词典切分已经依赖 locale 词典，再叠一层编辑距离容错会让「切错」与「切对」两种错误互相放大，所以非英语 locale 直接把 `max_cost` 钳到 0。

召回质量的取舍：ICU `BreakIterator` 是**语言学词边界**，切错一个词就等于整条短语失配，且**没有退路**（不像 n-gram 索引能靠重叠窗口部分召回）。n-gram 的代价则是索引体积与假阳性。Typesense 选了前者，因此中文/日文的召回质量**强依赖 ICU/kakasi 词典版本**——升 ICU 版本可能改变召回结果，这类「静默行为漂移」在运维上比版本号本身更值得关注。

源码路径：`src/tokenizer.cpp`、`include/tokenizer.h`、`src/japanese_localizer.cpp`、`include/japanese_localizer.h`、`include/string_utils.h`。

### 20.5 Facet 两阶段计数：内存 hash 表 → 线程分片计数 → 合并截断

facet 走的是**「索引阶段写、查询阶段读」的两段式**，而不是查询时回表扫文档：

**阶段一（写入时）**。facet 字段的值先按 token 逐个算 `wyhash(token, len, 0, _wyp)`（`include/string_utils.h` 引 `wyhash_v5.h`），再用 `StringUtils::hash_combine` 折成一个 `uint64_t` 哈希（`src/index.cpp` 的 `tokenize_string_with_facets` / `tokenize_string_array_with_facets`），存进 4 路分片表 `facet_index_v3[field][seq_id % 4][seq_id] -> facet_hash_values_t`（数组字段）或 `single_val_facet_index_v3[field][seq_id % 4][seq_id] -> uint64_t`（单值字段）。另外注意 `src/index.cpp` 里的这条入索引判据：

```cpp
// non-geo faceted field should be indexed as faceted string field as well
bool non_string_facet_field = (afield.facet && !afield.is_geopoint());
if(afield.is_string() || non_string_facet_field) { … 建 ART 文本索引 + 上面那两份 facet 表 … }
```

也就是说，**只要声明了 `facet: true` 且不是 `geopoint`，无论原本是数值还是 bool，都会被字符串化后一并建进文本 ART 索引**，同时再落 facet hash 表——代价是这类字段占两份索引，收益是 `facet_by` / 排序无需按类型走不同代码路径。

**阶段二（查询时）**。`Index::do_facets` 的输入是 `all_result_ids`——多 suggestion / 多字段结果 id 的**有序并集**（`id_buff` 累积到 10 万条就先做一次 `ArrayUtils::or_scalar` 合并去重以控制 OR 次数）。分片是按 `num_threads = min(concurrency, all_result_ids_len)`、`window_size = ceil(len / num_threads)` 切出来的：

```text
num_threads = min(concurrency, all_result_ids_len)
window_size = ceil(all_result_ids_len / num_threads)

for thread_id in 0..num_threads:
    batch = all_result_ids[thread_id*window_size .. +window_size]
    thread_pool.enqueue(do_facets(facet_batches[thread_id], batch))
join 所有线程

for batch in facet_batches:  for facet in batch:  merge 到主 facet 对象
```

每个 `do_facets` 内部的单文档处理是：

```text
for each result_id in batch:
    if estimate_facets and (i % (100 / facet_sample_percent) != 0): continue   # 抽样
    fhashes = facet_index[seq_id % 4][seq_id]                                  # O(1)
    unique_facet_hashes = {}                                                   # 文档内去重
    for fhash in fhashes:
        if fhash in unique_facet_hashes: continue                              # 一个文档只能给同一 bucket 计 1
        unique_facet_hashes.insert(fhash)
        if should_compute_stats: update fvmin / fvmax / fvsum / fvcount        # 数值 facet 的统计量
        if is_range_query:  result_map[range_id].count += 1                     # range facet 按区间归桶
        elif !use_facet_query or fhash in facet_query.hashes:                   # facet_query = disjunctive faceting
            result_map[fhash].count += 1
        if group_limit: result_map[...].hash_groups[distinct_id].insert(fhash)  # group_by 下的分面
    if (i + 1) % 16384 == 0: RETURN_CIRCUIT_BREAKER                             # 每 16384 条查一次超时
```

**四个值得记的语义**：

- **文档级去重**（`unique_facet_hashes`）：一个文档的 `tags` 数组里有三个相同值，只计 1；
- **`facet_query`（`facet_query:brand=Sony`）是 disjunctive faceting**：只统计该值本身而不是全部值——这是电商「选 Sony 后其他品牌计数不消失」的标准做法；
- **数值 facet 同时产出统计量**（`facet_stats_t` 的 `fvmin/fvmax/fvcount/fvsum`），这就是 Typesense 没有 ES-style `sum/avg` metric aggregation、却仍能给出 `stats` 的原因；
- **`facet_sample_percent` 是步长抽样**：`mod_value = 100 / facet_sample_percent`，只有 `i % mod_value == 0` 的结果参与计数，`estimate_facets = (facet_sample_percent < 100 && all_result_ids_len > facet_sample_threshold)`。计数是**估计值**，`facet.sampled` 字段会标记。

复杂度：阶段一 $O(|f|)$ 哈希 / 文档；阶段二 $O(\text{batch 内文档数} \times \text{每文档 facet 值数})$，全部落在内存哈希上，且每条结果最多哈希查 4 次（4 路分片中定位 1 次）。源码路径：`src/index.cpp`（`compute_token_offsets_facets` / `initialize_facet_indexes` / `do_facets` / `compute_facet_infos`）、`include/field.h`（`facet` / `facet_count_t` / `facet_stats_t` / `facet_info_t`）、`src/collection.cpp`（结果 JSON 组装与 `max_facet_values` 截断）、`src/array_utils.cpp`。

### 20.6 向量：HNSW、flat_search_cutoff 与 RRF 融合

向量检索的三个决策点：

**① 索引结构**。每个向量字段一个 `hnsw_index_t`，构造即固定超参（`include/index.h`）：

```cpp
struct hnsw_index_t {
    hnswlib::InnerProductSpace* space;
    hnswlib::HierarchicalNSW<float>* vecdex;
    hnsw_index_t(size_t num_dim, size_t init_size, vec_dist_t distance_type):
        space(new hnswlib::InnerProductSpace(num_dim)),
        vecdex(new hnswlib::HierarchicalNSW<float>(space, init_size, 16, 200, 100, true))
        //                          ↑ max_elements   ↑M ↑ef_construction ↑seed ↑allow_replace_deleted
};
```

底层库是 GitHub `typesense` 组织下的 hnswlib（`https://github.com/typesense/hnswlib`，CMake 锁 commit `21de18ffabea1a9d1e8b16b49afc6045d7707e4c`），header-only。

**② 暴力搜索 vs 图搜索的切换**。`src/index.cpp`：

```cpp
size_t k = vector_query.k == 0 ? max<size_t>(vector_query.k, fetch_size) : vector_query.k;
if (vector_query.query_doc_given) k++;                 // 「以某文档为查询」时把自己排除
...
if (!no_filters_provided && filter_result.count < vector_query.flat_search_cutoff) {
    // 过滤器已经把候选压到很小 → 直接线性扫描，比走图更快
    for (id in filter_result.docs) { values = vecdex->getDataByLabel<float>(id); dist = space->get_dist_func()(q, values); }
} else {
    dist_labels = vecdex->searchKnnCloserFirst(q, k, &filterFunctor);
}
```

`flat_search_cutoff` 默认 **0**（即默认永远走 HNSW），只有显式设成大于 0 才在「过滤后剩余量小于阈值」时切线性——这是一个由用户按「过滤选择性」手动调的参数，不是自动策略。`distance_threshold` 默认 `2.01`（内积空间下的宽松上界），超出的结果直接丢弃（`if (vec_dist_score > vector_query.distance_threshold) continue;`）。

注意 `src/index.cpp` 里有**两处**取 `k`，默认值不同：纯向量那条取 `max(0, fetch_size)`，而 **RRF 融合那条额外兜了一个 `default_k = 100`**——源码注释写明 *"use k as 100 by default for ensuring results stability in pagination"*。也就是说即使本页只要 20 条，走图时也会按 100 取候选，避免候选太少导致排名抖动、进而让分页结果不稳定。

**③ 融合：Reciprocal Rank Fusion，不是分数线性加权**。`src/index.cpp` 里代码注释直接写了公式：

```cpp
// Reciprocal rank fusion
// Score is sum of (1 / rank_of_document) * WEIGHT from each list (text match and vector search)
const float VECTOR_SEARCH_WEIGHT = vector_query.alpha;      // 默认 alpha = 0.3
const float TEXT_MATCH_WEIGHT    = 1.0 - VECTOR_SEARCH_WEIGHT;  // 默认 0.7

// 文本侧：把 packed match score 整个替换掉，改成排名倒数
result->scores[match_score_index] = float_to_int64_t((1.0 / (i + 1)) * TEXT_MATCH_WEIGHT);

// 向量侧：命中同一文档则两个排名分相加
found_kv->text_match_score = found_kv->scores[match_score_index];       // 备份原 packed score
found_kv->scores[match_score_index] = float_to_int64_t(
        int64_t_to_float(found_kv->scores[match_score_index])
      + ((1.0 / (res_index + 1)) * VECTOR_SEARCH_WEIGHT));
// 只在向量侧命中、文本侧没有的文档，单独并进 all_result_ids
```

融合公式即：

$$\text{score}(d) \;=\; \frac{w_{\text{text}}}{1 + \text{rank}_{\text{text}}(d)} \;+\; \frac{w_{\text{vec}}}{1 + \text{rank}_{\text{vec}}(d)},\qquad w_{\text{text}} = 1 - \alpha,\; w_{\text{vec}} = \alpha$$

三种文档去向（`src/index.cpp`）：

| 情形 | 进入 Topster 的分数 |
| --- | --- |
| 只被文本命中 | $\frac{w_{\text{text}}}{1 + r_{\text{text}}}$（原 packed score 备份到 `KV::text_match_score`） |
| 只被向量命中 | $\frac{w_{\text{vec}}}{1 + r_{\text{vec}}}$，且单独并进 `all_result_ids` 供 facet 计数 |
| **两边都命中** | $\frac{w_{\text{text}}}{1 + r_{\text{text}}} + \frac{w_{\text{vec}}}{1 + r_{\text{vec}}}$——**pushed 分数被整体替换成两路排名倒数之和** |

也就是说，hybrid 查询下最终排序用的是 RRF 分数（不是 packed score），只有在非 hybrid 的纯文本查询里 §19.4 的 packed score 才直接决定名次。源码里把这条路径圈起来的条件是「`vector_query.field_name` 非空 **且** sort 字段里有 `text_match`」，两者缺一就走不了 RRF。

`float_to_int64_t` / `int64_t_to_float` 是一对**保序的 float→int64 位重解释**（负数用 `i ^= INT32_MAX` 翻转符号位），让浮点分数能塞进 `KV::scores[3]`（`int64_t`）而不破坏与既有 int64 排序键的可比性。融合结果还会经 `Index::int64_t_to_float` 回写到响应的 `hybrid_search_info.rank_fusion_score`。

> ⚠️ 旧节 §8.6 写「Hybrid ranking：BM25 + 向量 + 线性权重」，并给出 `hybrid_ranking: {alpha}` 的请求样例。v25.2 的实现是 `vector_query` 字符串 DSL 里的 `alpha`（默认 0.3），语义是 RRF 的两路权重，不是分数线性加权。新节给出真实公式，§8.6 原文未改。

源码路径：`include/index.h`（`hnsw_index_t` / `VectorFilterFunctor`）、`src/index.cpp`（向量检索与 RRF）、`src/vector_query_ops.cpp` / `include/vector_query_ops.h`（`vec:(...)` 解析）、`cmake/hnsw.cmake`（依赖锁定）、`include/field.h`（`vec_dist_t` / `num_dim`）。

## 二十一、参考资料

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
