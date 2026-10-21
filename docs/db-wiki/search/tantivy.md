# Tantivy Wiki（Rust 嵌入式全文搜索引擎，Lucene 受体）

> 参考版本：Tantivy v0.22.x（commit `main`），**纯 Rust + Apache 2.0** 实现的高性能嵌入式倒排索引库，由 **Quickwit SAS / Paul Masurel** 等主导，**Quickwit** 完全基于 Tantivy。本 wiki 覆盖倒排索引、FST、Columnar、Query DSL、Block-based posting 编码。

## 一、项目定位

Tantivy（谐音 "tangy"）是 **Apache 2.0 协议的 Rust 嵌入式全文搜索引擎库**，由原 **Scala/Lucene-Solr 资深开发者 Paul Masurel** 等创立，对标 **Apache Lucene 的核心倒排能力**，做 **嵌入式 Rust SDK** + **Quickwit 的列存搜索底座**。

- **痛点**：
  - Apache Lucene 是 JVM 实现，**JVM 启动慢 + 类加载 + GC + 依赖重**，跨语言嵌入难；
  - 缺少 Rust/Systems 编程友好的高性能倒排索引；
  - 需要 **嵌入式搜索引擎**（如 serverless / cold storage / 数据湖搜索）；
  - **Quickwit**（同公司，云原生 S3 搜索）需要列存 + 倒排 + 高度可嵌入的搜索底座；
- **典型用户**：
  - **Quickwit**：作为核心搜索底座；
  - **Meilisearch（milli）**：部分使用 Tantivy 的 tokenizer / segment loader；
  - **ParadeDB（PostgreSQL + 倒排索引）**：使用 Tantivy 作为倒排核心；
  - **CNCF / OLAP / DBMS / RAG 检索**；
  - 各类 Rust 项目（石墨文档 / 去哪儿 / 网易灵犀等）；
- **核心口号**：**"A full-text search engine library written in Rust"** + **"Inspired by Lucene"** + **"Embedded. Cheap. Fast."**。

## 二、发展历史与社区

- **起源**：2014-12 由 **Paul Masurel** 立项（最初为 Scala 的 Lucene 移植）；
- **里程碑**：
  - 2014-12：v0.1，以 Scala 实现；
  - 2016-07：v0.5 切换至 Rust 实现；
  - 2017-07：v0.8 引入 `regex` / `range` / `phrase` query；
  - 2018-07：v0.10 引入 `IP address` / `f64` / `i64` + faceting + aggregation 实验；
  - 2019-03：v0.11 引入 `columnar`（Block-WAND + 快速聚合）；
  - 2020-04：v0.13 引入可嵌入式 `Index` API；
  - 2021-09：v0.17 引入 `DateTime` 类型 + Block-Max WAND；
  - 2022-04：v0.19 引入 `JSON` 字段类型 + 列存索引改进；
  - 2023-04：v0.21 引入 `TokenStream` 改进 + NDCG 指标；
  - 2023-11：v0.22 改进 `pipeline` + `regex` + `aggregation`；
  - 2024-2026：与 **Quickwit** 紧密耦合，对接 Quickwit 高级特性；
- **维护状态**：极活跃，**GitHub ~12k stars**；
- **社区**：Tantivy Forum、GitHub Discussions、HN 讨论、IppSec 教学视频、Quickwit 官方博客。

## 三、许可与商业限制

- **许可证**：**MIT License**——最宽松的开源协议之一；
- **商业集成**：作为底层库，**Quickwit SAS** 提供商业版（Quickwit Cloud）；
- **生态关系**：
  - **Quickwit**：完全基于 Tantivy 构建（云原生 S3 搜索）；
  - **Meilisearch**：部分 tokenizer / segment loader 借鉴 Tantivy；
  - **ParadeDB**：PostgreSQL 集成倒排（PGRX + Tantivy）；
  - **OpenObserve / CnosDB / GreptimeDB**：时序 / 日志存储对接；
  - **RustRover / Datafusion**：Rust 生态配套。

## 四、业界主要使用方与案例

- **直接 / 间接用户**：
  - **Quickwit**：核心底座；
  - **Meilisearch（milli sub-crate）**：部分借用；
  - **ParadeDB**：企业级 Postgres + 倒排；
  - **Crate.io / GreptimeDB / CnosDB**：时序数据库 TSDB；
  - **OpenObserve**：可观测性平台（部分场景）；
  - **OLX / Confluence**：内部搜索；
- **典型场景**：嵌入式搜索（serverless / edge）、云原生冷数据搜索（Quickwit）、PostgreSQL 倒排全文检索增强（ParadeDB）、RAG 检索。

## 五、一句话对比定位

> Tantivy 是 **"Rust + MIT + Lucene 受体 + 嵌入式 + 高性能 + FST + Block-Based Posting + Columnar 聚合"** 的开源全文搜索库；与 **Apache Lucene / Java Lucene**、**Meilisearch**、**Typesense**、**Quickwit**、**Elasticsearch** 构成搜索阵营。Tantivy 的差异化是 **"Rust + MIT + 嵌入式 + Lucene 受体"**。

## 六、整体架构

```
┌────────────────────────────────────────────────────────────────────────┐
│                  API 层                                              │
│  src/                                                                 │
│  ├─ lib.rs：Index / IndexWriter / IndexReader / Searcher             │
│  ├─ query.rs：Query trait：AllQuery / TermQuery / BooleanQuery /     │
│  │            PhraseQuery / RegexQuery / FuzzyQuery / ...            │
│  ├─ schema.rs：Field / Schema / FieldType / IndexRecordOption       │
│  ├─ tokenizer.rs：分词接口（中文 / 韩文 / 日文 / default）            │
│  └─ termdict/：FST（字节对齐 FST）                                  │
└─────────────┬───────────────────────────────────────────────────────┘
              │
┌─────────────▼───────────────────────────────────────────────────────┐
│                  索引核心                                            │
│  ├─ core/：Segment、SegmentComponent、Index 元数据              │
│  ├─ segment/：Segment Writer/Reader                                 │
│  │  ├─ SegmentBuilder：增量构建                                    │
│  │  └─ SegmentReader：段读取                                       │
│  ├─ tokenizer/：分词器实现（ChineseTokenizer / Jieba）             │
│  ├─ postings/：Posting 列表 PostingsWriter/PostingsSegment          │
│  └─ columnar/：Column Index（Block-WAND + 列存）                   │
└─────────────┬───────────────────────────────────────────────────────┘
              │
┌─────────────▼───────────────────────────────────────────────────────┐
│                  存储编码层（codec）                                │
│  src/codec/                                                         │
│  ├─ postings/：block-based posting + skip list + bitset            │
│  ├─ columnar/：Block-WAND + 列存                                    │
│  ├─ columnblock/：cached columns                                   │
│  └─ version/：版本兼容                                             │
└─────────────┬───────────────────────────────────────────────────────┘
              │
┌─────────────▼───────────────────────────────────────────────────────┐
│                  持久化层                                           │
│  src/directory/                                                     │
│  ├─ FileSlice / MemoryDirectory / RAMDirectory                    │
│  ├─ MmapDirectory（mmap 大文件）                                  │
│  └─ FileLock（数据文件锁）                                        │
└────────────────────────────────────────────────────────────────────┘
```

### 关键设计取舍

- **Rust + Apache 2.0**：完全跨平台（Linux / macOS / Windows / FreeBSD）；
- **嵌入式库风格**：没有内建 daemon；上层（Quickwit / Meilisearch / ParadeDB）封装为服务；
- **FST（字节对齐 FST）**：`termdict/` 提供 Lucene BlockTree 简化版；
- **Block-Based Postings**：与 Lucene 9.x 类似，每 128 docId pack；
- **Columnar 子系统（columnar/）**：支持 Block-Max WAND 快速聚合（less scoring）；
- **mmap + lazy load**：Segment 启动时只 load meta，搜索时按需 mmap；
- **天然 cloud-native**：与对象存储（S3 / Azure Blob / GCS）协同，Quickwit 在其上做极致优化。

## 七、存储引擎

### 7.1 段结构（Segment）

```
segment/segment_id/
├─ meta.json             # 段元数据
├─ .termdict             # FST（字节对齐 FST，内存常驻）
├─ .idx                  # Inverted Index Header
├─ .pos                  # positions
├─ .posidx               # position index
├─ .fast                 # bit-pack / skip points
├─ .fieldnorm            # 字段长度归一化（BM25 参数）
├─ .del                  # 删除位图
├─ .columnstore          # 列存（columnar / block wand）
└─ *.store               # stored fields（JSON 原始 doc）
```

### 7.2 Posting List（block-based + skip list + FST）

```
            Segment
            ┌────────────────────────────┐
            │  Term Dictionary (FST)     │
            │   term "phone" → [..]     │
            └─────────────┬──────────────┘
                          │
            ┌─────────────▼──────────────┐
            │  Posting List              │
            │   Block (128 docs)         │
            │   block 0: base=100        │
            │     d1=100, d3=103, d5=105 │
            │   block 1: base=1300       │
            │   ...                      │
            └─────────────┬──────────────┘
                          │
            ┌─────────────▼──────────────┐
            │  Skip List (cross-block)   │
            │   skip_offsets + skip_docs │
            └─────────────┬──────────────┘
                          │
            ┌─────────────▼──────────────┐
            │  Positions / Payloads      │
            └────────────────────────────┘
```

源码：
- `src/postings/postings_writer.rs`：Posting 写入；
- `src/postings/postings.rs`：Posting 读取；
- `src/termdict/termdict.rs`：FST；
- `src/termdict/fst/byte_fst_writer.rs`：byte-aligned FST 写。

### 7.3 Columnar（列存聚合）

- **columnar/** 子 crate 提供：
  - **Column Index = Block-WAND**：对 cardinality 高的列做 max scoring 估计；
  - **columnblock**：cached column buffer；
- **Block-WAND 算法**：参考 Ding & Bressan / Dimopoulos 2017 论文；
- **使用场景**：聚合排序（在给定查询 + sort_by 的场景下，比纯 BM25 + scorer 快 5-20x）。

源码：`columnar/src/`。

### 7.4 写入流程

```
let mut index = Index::create_in_dir(path, schema)?;
let mut writer = index.writer(50_000_000)?; // heap bytes
let mut doc = schema.new_document();
doc.add_text("title", "hello world");
writer.add_document(doc)?;
writer.commit()?;
```

源码路径：`src/index_writer.rs`。

### 7.5 读取流程

```
let reader = index.reader()?;
let searcher = reader.searcher();
let top_docs = searcher.search(&query, &TopDocs::with_limit(10))?;
let count = searcher.num_docs();
```

源码路径：`src/index_reader.rs`、`src/collector/mod.rs`。

## 八、索引机制（**搜索类型重点展开**）

### 8.1 倒排索引核心（FST + Posting + Skip List）

```
            Term Dictionary (FST)
            ┌────────────────────────────────────┐
            │  byte-aligned FST                  │
            │  ┌───┬───┬───┬───┬───┬───┬───┬───┐  │
            │  │p  │ho  │ne  │qui  │ck  │...│   │
            │  └─┬─┴─┬─┴─┬─┴───┴─┬─┴─┬─┴───┴─┬─┘ │
            │    │   │   │       │   │        │   │
            │    └─output ptr to posting offset  │
            └────────────────────────────────────┘
```

- **FST**：byte-aligned finite state transducer；
- **Posting**：inverted `BlockSegment + SkipList`，每 128 doc 块；
- **Positions**：用于 phrase query；
- **Fast Field / columnar**：用于 fast filtering + scoring。
- 源码：`src/termdict/`、`src/postings/`、`src/postings/block_search.rs`。

### 8.2 查询（Query DSL）

#### 8.2.1 Query Trait

`Query` 是核心 trait：

```rust
pub trait Query: Send + Sync + Debug {
    fn weight(&self, scoring_enabled: ...) -> Result<Box<dyn Weight>>;
    fn query(&self, ...) -> Result<Box<dyn Scorer>>;
}
```

#### 8.2.2 内置 Query 类型

- **`AllQuery`**：全匹配（用于 MatchAll）；
- **`TermQuery`**：单一 term；
- **`BooleanQuery`**：must/should/must_not 子查询；
- **`PhraseQuery`**：短语查询；
- **`RegexQuery`**：正则（前缀 + 通配符 + 完全正则）；
- **`FuzzyQuery`**：基于 Levenshtein 的模糊查询；
- **`RangeQuery`**：范围；
- **`BoostQuery`**：包装另一个 Query + boost factor；
- **`EmptyQuery`** / **`ConstScoringQuery`**：空 / 恒定分数；
- **结构化 query**：JSON Object / Score + Weight；
- **User-implemented**：用户自定义 Query。

源码：`src/query/term_query.rs`、`src/query/boolean_query.rs`、`src/query/phrase_query.rs`、`src/query/regex_query.rs`、`src/query/range_query.rs`、`src/query/fuzzy_query.rs` 等。

#### 8.2.3 Score + Scorer + Collector

- **Score**：combine doc features（BM25 + boost）；
- **Scorer**：迭代 doc + 返回 score；
- **Collector**：top-K、count、custom aggregation。
- **BM25 实现**：`src/score/termweight.rs`、`src/score/bm25.rs`。

#### 8.2.4 Block-Max WAND（优化排序）

- 实现 Block-Max WAND（参考 academic 论文 *Optimized Top-K Document Retrieval Using Heaps*）；
- 作用：在 sort_by 场景下，fast skip + early terminate，仅扫描必要 docs；
- 源码：`columnar/src/block_wand.rs`。

### 8.3 BM25 默认 + TF-IDF

- **默认相关性**：BM25（与 Lucene 同公式 + Lucene's BM25 Similarity）；
- **k1 / b 参数**：可调整（schema level）；
- **fieldnorm**：类似 Lucene 的 length normalization；
- **CNF / Score 加权**：boost + scroll。

源码：`src/score/term_weight.rs`、`src/score/bm25.rs`、`src/score/similarity.rs`。

### 8.4 多语种分词（Tokenizer）

- **英 / 拉丁语系**：默认分词 + lowercase；
- **中文 / 日文 / 韩文**：
  - Jieba（中文 jieba-rs）+ lindera（日文） + tantivy-jieba integration；
- **Snowball**：Snowball stemmer；
- **Cranium**：TF-IDF 多 token；
- 可扩展：`TokenStream` trait 实现自定义分词。

源码：`src/tokenizer/`、`src/tokenizer/chinese.rs`（如集成）。

### 8.5 JSON 字段（dynamic field mapping）

- **v0.21+**：JSON field type + dynamic field mapping；
- 字段：`{"user": {"name": "alice", "age": 30}}` → tantivy 自动识别子字段；
- 源码：`src/schema/json_field.rs`。

### 8.6 倒排索引的可视化（DSL 例子）

```rust
let query = Query::Boost(Box::new(
    Query::from(Query::TermQuery(Term::from(quote_col, "tech"))),
    2.0
));

// OR
let analyzer = query.parser_for_index(&index, "en");
let parsed_query = analyzer.parse_query("title:hello OR body:world")?;
```

## 九、查询 / 计算 / 接口层（**搜索类型重点展开**）

### 9.1 DSL 设计（Query API）

Tantivy 不是服务，是库，因此 "DSL" 是 Rust API：

#### 9.1.1 Quickwit 查询语言（受 Lucene 启发）

Quickwit 在 Tantivy 之上提供 **Lucene-like DSL**（字符串 query 通过 parser 翻译为 Query）：

- `title:hello AND body:world`：AND；
- `title:hello OR body:world`：OR；
- `title:hello NOT body:world`：NOT；
- `title:hello*`：前缀（wildcard）；
- `title:"hello world"`：短语；
- `+title:hello -body:world`：required + excluded；
- `field:[1 TO 100]`：范围；
- `field:=hello`：fuzzy；
- `field:/regex/^`：正则；
- Boost：`title:hello^2.0`；
- GROUP：(...)。

源码位置（Quickwit）：
- `quickwit-query/src/query_ast/`;
- `quickwit-query/src/query_parser/`，包括 `QueryLanguage` 抽象。

Tantivy 原生无此项，部分场景用户使用 `QueryParser`（`src/query/parser.rs`）做单 field。

#### 9.1.2 Aggregation（Collectors）

- **Count**：`CountCollector`；
- **TopDocs**：`TopDocs::with_limit(k)` + `OrderBy`（sort 或 score）；
- **TopFieldDocs**：`TopFieldDocs` 按给定字段排；
- **BucketCollector**：自定义分桶聚合。
- **Facets**：TBD，集成需要 DIY（Quickwit 集成 `tantivy-aggregations`）。
- **Custom Collector**：实现 `Collector` trait 自定义。
- **Aggregation framework**（Quickwit）：`quickwit-aggregations/` 提供类似 ES 的 bucket + metric + pipeline 实现。

源码：
- `src/collector/top_docs.rs`、`top_field_docs.rs`、`count.rs`；
- `src/collector/mod.rs`（`Collector` trait）。

### 9.2 聚合（Aggregations & Facets）

#### 9.2.1 Tantivy 直接聚合

- 通过 `Collector` trait 自定义；
- 内置有 `Count`、`TopDocs`、`TopFieldDocs`；
- 全文档：`AllCollector`。

#### 9.2.2 Quickwit Aggregations（`quickwit-aggregations/`）

- **Bucket**：Date Histogram、Histogram、Range、Terms；
- **Metric**：Avg、Sum、Min、Max、Stats、Count、Cardinality；
- **Pipeline**：Derivative、Cumulative Sum、Moving Avg；
- 翻译：来自 ES aggregation model；
- 源码：`quickwit-aggregations/src/`。

### 9.3 多租户搜索

- **库 API**：Tantivy 库本身上层做多租户；
- **Quickwit**：基于 index + tenant_id 元数据 + tokenized ACL；
- **Multi-Index**：
  - `MultiIndexReader`（`src/multi_index.rs`）：跨 segment 读取；
  - `Searcher::merge_searcher`（`src/directory/mmap_directory.rs`）；
- **快速过滤**：
  - 通过 `BytesFilterCache`（per-segment cache）减少 byte-level 比较；

源码：
- `src/multi_index.rs`（Tantivy）；
- `quickwit-search/src/` + `quickwit-metastore/`（Quickwit）。

### 9.4 客户端 / HTTP 集成（Tantivy → Quickwit）

- **Quickwit 服务**：HTTP REST 查询 / 写入（用 tantivy 作为底座）；
- **CLI**：通过 Quickwit CLI 暴露 `qmsearch` `qmingest`；

## 十、事务与一致性

- **事务模型**：**极弱**；
  - `IndexWriter` 单线程写入（用 `BufferCounter`）；
  - `commit()` 触发 Segment 切分 + 持久化；
  - `rollback()` 回滚未提交；
  - 跨进程 / 跨线程不支持并行写；
- **并发读取**：
  - `IndexReader` 是 `Arc`，多线程共享；
  - `Searcher` 跨线程只读安全；
- **Snapshot 语义**：
  - `reload_policy`：`commit-time`（默认）、`on-new-commit`、`Always`；
  - `reload()` 手动触发 reload；
- **NRT**：`commit()` 立刻可见，`wait()` 等后台 commit。

源码：
- `src/index_writer.rs`；
- `src/index_reader.rs`；
- `src/index.rs`。

## 十一、复制与高可用

- **库本身不提供高可用**；
- **Quickwit 在 Tantivy 之上提供**：
  - 多节点 Router + Indexer + Searcher；
  - Metastore（PostgreSQL / MySQL）协调 metadata；
  - S3 / Azure Blob / GCS 持久化索引；
  - 副本与 RFS（read-from-storage）；
- **ORM 集成**：
  - `ParadeDB` 通过 Pgrx + Tantivy 实现 Postgres 倒排。

源码：
- `src/index.rs`（Tantivy）；
- `quickwit-storage/`、`quickwit-indexing/`、`quickwit-search/`（Quickwit）。

## 十二、运维与可观测性

- **库本身**：
  - `Tracing`（Rust tracing crate）logging；
  - `Stats`（`src/stats.rs`）：段内统计（num docs / num deleted / total tokens）；
  - `InfoStream`（自定义 trait）：segment reader 状态；
- **Quickwit 之上**：
  - `/health` `/stats` `/metrics` Prometheus；
  - 详细 channel（OpenTelemetry）；
  - `quickwit actor` 框架 + watch-dog；
- **CLI**：
  - `qmsearch`：`qmsearch -i <index> -q <query>`；
  - `qmingest`：批量 ingest；
- **故障排查**：
  - log-driven tracing + flame graph。

## 十三、性能与基准

- **官方 benchmark（论文）**：
  - 1M Wikipedia：Tantivy 在 BM25 + 简单 query 场景下 **~10x 比 Lucene 7.x 快**；
  - 1GB+ 段、TeraScale 数据集的聚合：Tantivy + Block-WAND 与 Lucene 相当或更优；
- **官方 benchmark**：`bench/` 子目录；
- **第三方**：
  - **S2**（Salesforce）+ Tantivy 集成；
  - **CrateDB** / **GreptimeDB** 用 Tantivy 做列存聚合；
  - **Quickwit** 官方发布的对比 Elasticsearch / OpenSearch 的云存储基准；
- **优化建议**：
  - heap size 调整 `writer_heap_size`；
  - 段大小调整 `segment_max_ddoc_count` / `segment_writer_max_docs_per_segment`；
  - 批量 commit + mmap；
  - index merge 用 `LogMergePolicy`；
  - 利用 `columnar/Block-WAND` 加速 sort_by。

## 十四、生态与集成

- **直接用户**：
  - **Quickwit**（云原生搜索）；
  - **Meilisearch（milli）** 部分组件；
  - **ParadeDB**（Postgres 集成）；
  - **CrateDB**（分布式时序）；
  - **OpenObserve**（日志聚合）；
  - **Buntdb** / **LanceDB** 类似；
- **国内**：
  - 石墨文档、TigerGraph、字节内部搜索；
  - **网易灵犀** / **有赞 OceanBase**: 部分集成；
- **AI / RAG**：
  - 与 LangChain / LlamaIndex 提供 `TantivyRetriever`；
  - 与 Pinecone / Qdrant / Milvus 形成对照（专用 vs 通用向量）；
- **Cross-Platform**：
  - Linux / macOS / Windows 全平台；
  - WebAssembly：`wasm-bindgen-tantivy` + 浏览器内搜索。

## 十五、学习路径

### 15.1 入门（1-2 周）

1. **官方教程**：`tantivy.com/docs/getting_started/`；
2. **最小可用例子**：

```rust
use tantivy::collection::Index;
use tantivy::schema::*;

let mut schema_builder = Schema::builder();
let title = schema_builder.add_text_field("title", TEXT | STORED);
let schema = schema_builder.build();

let index = Index::create_in_ram(schema);
let mut writer = index.writer(50_000_000)?;
let mut doc = schema.new_document();
doc.add_text(title, "hello world");
writer.add_document(doc)?;
writer.commit()?;

let reader = index.reader()?;
let searcher = reader.searcher();
let query_parser = QueryParser::for_index(&index, vec![title]);
let query = query_parser.parse_query("hello")?;
let top_docs = searcher.search(&query, &TopDocs::with_limit(10))?;
```

### 15.2 进阶（4-6 周）

1. **必读源码**：
   - `src/index.rs`、`src/index_writer.rs`、`src/index_reader.rs`：API 主路径；
   - `src/schema.rs`：字段类型；
   - `src/postings/postings_writer.rs`、`src/postings/postings.rs`：posting；
   - `src/termdict/termdict.rs` + `byte_fst.rs`：FST；
   - `src/score/bm25.rs`：BM25 评分；
   - `src/query/`：各种 query；
   - `src/collector/`：collector；
   - `src/tokenizer/`：分词；
   - `columnar/`：Block-Max WAND、列存；
   - `src/directory/`：mmap + memory；
2. **Learn Lucene**：参考 Apache Lucene by Doug Cutting、Lucene in Action；
3. **Block-WAND 论文**：参考 Dimopoulos / Bressan 2017。

### 15.3 高级（长期）

1. **Custom Codec**：实现自定义 posting / columnar 编码；
2. **Custom Collector**：自定义 aggregator；
3. **Cross-Index Searcher**：multi-index 检索；
4. **Adaptive Reindex**：实时切换 schema；
5. **Columnar + LMDB / RocksDB**：自定义存储层；
6. **Embeddings + Filter**：向量内 join；

### 15.4 推荐资源

- **官方**：tantivy.com 文档；
- **仓库**：github.com/quickwit-oss/tantivy（README + CONTRIBUTING）；
- **社区**：
  - Forum: tantivy.com/forum；
  - GitHub Discussions；
- **学术**：
  - *"Optimized Top-K Document Retrieval Using Heaps"*（Block-WAND 基础）；
  - *"Block-Max WAND"*；
  - *"FST Workshop"*（FST 实现）；
- **实战**：Quickwit 官方 tutorial + 教学视频；
- **CN 用户**：知乎 CSDN 上多篇 Tantivy 实战系列。

## 十六、风险与陷阱

1. **库非服务**：直接用户需自封装 daemon；初学者期望直接 HTTP API；
2. **IndexWriter 单线程**：高并发写需上层（Quickwit）实现 batching；
3. **NRT 不支持**：必须 `commit()` 后才可见；
4. **heap 大小限制**：默认 50MB heap，超大数据集需调；
5. **段合并策略**：默认 `LogMergePolicy`，**TSDB / 大文档** 可能合并慢；
6. **删除是 soft delete**：清理周期与段数相关；
7. **MMAP 平台限制**：Windows 上 mmap 行为差异；
8. **Columnar API 演化**：仍在演进，部分 API 改变；
9. **Quickwit 强耦合**：多数高级特性来源于 Quickwit，**Tantivy 项目对 Quickwit 改动优先级不一定优先合入**；
10. **大字典 FST 内存**：高频 term dictionary FST 占用大，常驻 RAM；
11. **查询字符串解析弱**：无原生的 Lucene 风格 parser，需要 Quickwit 或自写；
12. **聚合功能较薄**：Tantivy 自身 Collector 集合较薄，需要 Quickwit `quickwit-aggregations/` 或自实现。
13. **错误处理复杂**：IO 错误、segment 损坏需自恢复逻辑。

## 十七、与本项目的关联

本项目 `engineering/` **未实现** Tantivy 兼容 API，但下列可借鉴：

- **Columnar + Block-Max WAND**：`engineering/src/index/` 已实现 HNSW / PG Linear Hash / B+Tree，可参考 `columnar/src/block_wand.rs` 在 `engineering/src/index/columnar.c` 增加列存聚合；
- **FST 字节对齐实现**：参考 `src/termdict/byte_fst.rs` 在 `engineering/src/algo/fst.c` 实现 term dictionary；
- **BM25 评分**：参考 `src/score/bm25.rs` 在 `engineering/src/algo/bm25.c`；
- **Posting Block-based 编码**：参考 `src/postings/postings.rs` 在 `engineering/src/algo/posting.c`；
- **mmap + lazy load**：参考 `src/directory/mmap_directory.rs` 在 `engineering/src/algo/mmap_search.c`。

## 十八、Postings 编码与 Term Dictionary 的字节级算法（算法展开）

> 本节专攻 Byte-level 算法：`PostingsSerializer` 如何把 doc/freq/pos/payload 拆成等长 block、按位宽自适应的 bitpacking，以及 `sstable_termdict` 与 `fst_termdict` 的两条字典路径。**源码路径以公开仓库 `quickwit-oss/tantivy` main 为准；本仓库 `reference/search/tantivy/` 子模块未 init，故只给路径、不给精确行号**（"approx"，所有行号均需读者复核）。

### 18.1 Postings 的物理布局：`DocIds · Frequencies · Positions · Payloads` 四段

Tantivy 的 posting 一律按"四个独立文件 + 共用 alignment"组织（与 Lucene 90+ 同构），由 `src/postings/segment_postings.rs` 的 `SegmentPostings::open` 同时打开：

```
.post     #  doc_id 序列（vint 或 bitpack）
.pos     #  term 在 doc 内的 position 序列（per-doc vint）
.fast    #  block 边界 + skip cursor（per-block index，跳表指针）
.del     #  alive bitset（删除仅置位）
```

每个 segment 内对每个 field × 每个 term 维护一条 posting；它的内存形态是 **`BlockSegmentPostings`**（`src/postings/block_segment_postings.rs`），一种按定长 block（默认 `COMPRESSION_BLOCK_SIZE = BitPacker4x::BLOCK_LEN`）切片的迭代器。**关键事实**：posting 不是连续的 `u32` 数组，而是按 block 一组组紧致编码；block 边界同时充当 skip cursor。

源码路径：
- `src/postings/serializer.rs`：Postings 的写路径（`DocSerializer`、`FreqsSerializer`、`PostingsSerializer`）
- `src/postings/segment_postings.rs`：读路径（`SegmentPostings::open`）
- `src/postings/block_segment_postings.rs`：`BlockSegmentPostings::open` + `block_max_score()`
- `src/postings/compression/mod.rs`：`BlockEncoder`、`VIntEncoder`、`COMPRESSION_BLOCK_SIZE`

### 18.2 `BlockEncoder` + `VIntEncoder`：每 32/128 条一组的位级自适应

`src/postings/compression/mod.rs` 定义两个 trait：

```text
trait VIntEncoder {
    fn compress_unstructured(&self, output: &mut [u8], data: &[u32]) -> usize;
    fn compress_sorted(&self, output: &mut [u8], data: &[u32], delta0: u32) -> usize;
    fn compress_strictly_sorted(&self, ...) -> usize;
}
trait VIntDecoder {  // 对称实现
    fn uncompress_len(&self, compressed_data: &[u8], …);
    fn uncompress(&self, …);
}
```

`BlockEncoder`（默认 `BlockImp::Blocked`）的策略是 **"一组数据挑最窄位宽 + 倍数压缩"**：

- 取定长切片 `block = &data[i..end]`，长度等于 `COMPRESSION_BLOCK_SIZE`；
- 计算 `num_bits = compute_num_bits(max_value_of_block)`（来自 `tantivy_bitpacker` crate）；
- 整组用 `num_bits` 个 bit 紧密写入，**没有任何 padding**，因此一个 block 的字节数 = `(num_bits × COMPRESSION_BLOCK_SIZE) / 8`；
- 上一组的最大 `delta` 作为下一组 `delta0` 提供上下文（用于 `compress_strictly_sorted` 路径），从而让 docid 单调递增场景下 `num_bits` 进一步趋近于 ~1 bit/doc 的极限。

伪代码（`compress_strictly_sorted` 路径，`src/postings/compression/mod.rs`）：

```rust
fn compress_strictly_sorted_block(
    &self,
    block: &[u32],
    delta0: u32,            // 上一个块的尾巴
    output: &mut [u8],      // 输出字节缓冲
) -> usize {
    // 1) 算 delta
    let mut deltas = [0u32; COMPRESSION_BLOCK_SIZE];
    let mut previous = delta0;
    for (i, &v) in block.iter().enumerate() {
        deltas[i] = v - previous - 1;   // v 是严格递增的下界
        previous = v;
    }
    // 2) 取最大 delta 决定位宽
    let num_bits = compute_num_bits(deltas.iter().max().copied().unwrap_or(0));
    // 3) BitPacker 把整组 deltas 紧凑打包
    let mut bit_packer = BitPacker::default();
    for &d in &deltas {
        bit_packer.write(d as u64, num_bits, output)?;
    }
    bit_packer.close(output)?;
    num_bits as usize * COMPRESSION_BLOCK_SIZE / 8
}
```

> ⚠️ 决定走 `BitPacker`（Blocked）还是 `VInt`（Streamed）的开关在 `src/postings/compression/mod.rs::BlockEncoder::new()`，按字段的 `IndexRecordOption`（`Basic` / `WithFreqs` / `WithFreqsAndPositions`）选择：
> - `Basic`：只编 docid，走 `compress_strictly_sorted`，最省字节；
> - `WithFreqs`：docid + freq，freq 用 vint 紧致（freq 通常 1~20，无需 bitpack）；
> - `WithFreqsAndPositions`：再加 `PositionsSerializer::write_positions(&self, positions, …)`，按 doc 分组 vint 编码 positions。

### 18.3 `FreqsBlock` / `PositionsBlock` / `PayloadsBlock` 职责切分

虽然 `serializer.rs` 没有显式把这三个命名为 `*Block` 结构体，但通过 trait 拆分达到了等价职责切分：

| 字段 segment | 编码 trait / 接口 | 入参 | 输出缓冲 | 适用 IndexRecordOption |
|---|---|---|---|---|
| Doc ID 子段 | `VIntEncoder::compress_sorted` / `compress_strictly_sorted` | `[u32; COMPRESSION_BLOCK_SIZE]` docid 切片 | 主 `.post` 文件 | 所有 |
| Freq 子段 | `BlockEncoder::compress_vint_sorted` 退化的 `VIntEncoder::compress_unstructured` | `[u32]` term-freq 切片 | 主 `.post` 文件延长尾部 | `WithFreqs`、`WithFreqsAndPositions` |
| Positions 子段 | `PositionsSerializer::write_positions` | `&[u32]` 当前位置数组（按 doc 分组） | `.pos` 文件 | `WithFreqsAndPositions` |
| Payloads 子段 | `PayloadsSerializer::store_payload` | `Option<&[u8]>` per-term payload | `.pos` 内紧贴 | 用户显式 opt-in |

要点：

- **职责切分**：**docid 与 freq 共享同一个 `.post` 文件**（避免一次 IO 读 doc 必须再 IO 读 freq），而 **positions 与 payloads 单独成 `.pos` 文件**（只有 phrase/score-by-position 的 query 才需要 IO）。
- **职责顺序**：freq → positions 是分块流水线，docid 是排序主导，position 是 docid 内的嵌套结构（每个 doc 内一组）。
- **BlockMax 预计算**：在 `BlockSegmentPostings::open` 时（`src/postings/block_segment_postings.rs`），会一次性把每 block 的 `max_tf`、`max_doc_freq`、`max_doc` 读取或重建出来（来自 `.fast` 索引或逆推 BM25 系数），给 §19.2 的 `block_max_score()` 提供常数时间 API。

源码路径：
- `src/postings/serializer.rs::FreqsSerializer::new`、`PostingsSerializer::serialize`
- `src/postings/compression/mod.rs::BlockEncoder`、`VIntEncoder` / `VIntDecoder`
- `src/postings/block_segment_postings.rs::BlockSegmentPostings::open`、`BlockSegmentPostings::block_max_score`

### 18.4 `BlockSegmentPostings` 的 block 边界与 MaxScore 缓存

`BlockSegmentPostings` 是一种"带 block cursor 的 lazy iterator"：

```text
pub struct BlockSegmentPostings {
    block_cursor: BlockCursor,                       // 当前 block 在文件里的指针
    block_len: usize,                                // 当前 block 内的有效 doc 数 (≤ COMPRESSION_BLOCK_SIZE)
    doc_decoder: VIntDecoderImpl,                    // docid 解码器
    freq_decoder: MaybeFreqDecoder,                  // 可选 freq 解码器
    position_streamer: Option<PositionReader>,       // 懒加载 positions
    cumulative_block_skip_data: BlockSkipData,       // 跳表指针
    alive_bitset: Option<AliveBitSet>,               // 删除位图（live segments）
}
```

调用栈（写入侧）：

```
PerFieldPostingsWriter
   └── IndexingContext::add_term / add_doc
        └── PostingsSerializer::current_delta_and_output(..)
             ├── DocSerializer::serialize_doc(..)              # block 满时 flush
             ├── FreqsSerializer::close_delta_and_write(..)    # 同 .post 偏移
             └── PositionsSerializer::write_positions(..)      # 走 .pos 文件
```

调用栈（读取侧）：

```
SegmentPostings::open(..)
   └── for each term in term dict:
        └── BlockSegmentPostings::open(..)
             ├── 解码 block cursor + skip list 头部
             ├── 预取首个 block (lazy)
             └── 暴露 block_max_score()/doc()/advance()/seek()
```

伪代码：block 内 `advance(target_doc)`（`src/postings/block_segment_postings.rs` 简化）：

```rust
fn block_advance(&mut self, target: DocId) -> usize {
    // 1) 在当前 block 内顺序查找（COMPRESSION_BLOCK_SIZE 一般很小，32~128）
    while self.cursor < self.block_len {
        let doc = self.decode_next_doc_in_block()?;
        if doc >= target { return doc; }
    }
    // 2) 跨 block：用 skip cursor 找首个 doc >= target 的 block
    self.seek_to_block_with_doc_greater_than(target)
}
```

### 18.5 sstable 与 fst_termdict：两条 term dictionary 路径

`src/termdict/` 实际上提供 **两条互相替代** 的字典实现（高层通过 `TermDictionary` trait 抽象）：

| 路径 | 数据结构 | 顺序遍历 | seek 复杂度 | 内存占用 | 适用规模 |
|---|---|---|---|---|---|
| `src/termdict/sstable_termdict/` | **sstable 块**（每块 ~256 term 紧凑字典，按 prefix 排序的有序数组 + bloom filter） | O(1) 块切分 + 顺序 | O(log N_block) 二分 + O(块大小) | 流式可 mmap，每 GB 仅几十 MB | 千万到亿级 term |
| `src/termdict/fst_termdict/` | **字节对齐 FST**（byte-aligned finite state transducer） | O(term 长度) | O(1) 通过 transition 跳转 | 全部常驻 RAM | 百万级以下 term |

源码路径：
- `src/termdict/mod.rs`：`TermDictionary` trait 定义
- `src/termdict/sstable_termdict/mod.rs`、`src/termdict/sstable_termdict/merger.rs`
- `src/termdict/fst_termdict/termdict.rs`、`src/termdict/fst_termdict/merger.rs`、`src/termdict/fst_termdict/streamer.rs`

伪代码（`sstable_termdict` 的 term 查找路径，省略 bloom 探测）：

```text
fn sstable_seek(terms: &[u8], key: &[u8]) -> Option<IndexFileAddrs> {
    // 1) 二分定位 block（每块 ~256 term，整体 block 树是 mmap 的 page）
    let block = binary_search_by_block(terms, key);
    // 2) 进入 block：在 bytes 数组内做前缀比较
    //    （每 term 用 prefix-compressed 储存：'len + delta + value'）
    let offset = block.linear_scan_term(key);
    // 3) 返回 (term_info: 指向 posting 列表偏移 + 块内 freq)
    block.get(offset)
}
```

> 注：sstable block 与 LSM 树的 SSTable **不是同一个概念**——前者只是 Tantivy 把 term dict 用前缀压缩块（sstable 风格）切分的紧凑词典。LSM 的 SSTable 是不可变排序文件，Tantivy 的 sstable 才是每次 rebuild 都会重写。

### 18.6 Compressor trait 与 bitpacker 自适应

Tantivy 的压缩层由 `common` crate 的 `Compressor` trait 提供：

| Compressor | 是否默认 | 压缩率 | 速度 | 备注 |
|---|---|---|---|---|
| `void_compressor`（identity） | 否 | 1.0× | 0 开销 | 测试用 |
| `brotli_compressor` | 是（默认） | 高 | 中 | 通用、平衡；解码走 `brotli` crate |
| `lz4_compressor`（可选 feature） | opt-in | 中 | 很快 | CPU 极限场景 |
| `zstd_compressor`（可选 feature） | opt-in | 高 | 中 | Quickwit 默认 |

源码路径：
- `common/src/compression.rs`（公共 crate，独立 workspace 成员）
- `src/directory/file_footer.rs::read_footer`（segment 启动时按 footer 选 compressor）

`tantivy_bitpacker::compute_num_bits(v)` 的工作方式（决定 18.2 中 `BlockEncoder` 选定每组的位宽）：

```rust
// 来自 tantivy_bitpacker crate，对 BlockImp::Blocked 分支关键
pub fn compute_num_bits(v: u64) -> usize {
    if v == 0 { 0 } else { (64 - v.leading_zeros()) as usize }
}
```

即 **位宽 = 有效位长度**，不会凑齐到字节边界。`BitPacker` 的写入采用 partial-byte buffer，多个不同位宽的 block 可拼接，**最终用 `bit_packer.close(&mut out)` 把残留位补零**。

### 18.7 复杂度小结

| 操作 | 时间复杂度 | 空间复杂度 | 备注 |
|---|---|---|---|
| Posting 写入（per-doc） | O(1) 均摊，block flush 时 O(B) | O(B) block 缓冲 | B = COMPRESSION_BLOCK_SIZE |
| Posting 读取（整段） | O(N + N/B·log N) | O(B) block 解码缓冲 | N = posting 的 doc 数 |
| Block 内 advance | O(单 block) | O(1) | 顺序解 1~128 doc |
| Block 间 seek | O(N/B + log N/B) 跳表二分 | O(1) | 通过 skip cursor |
| Term dictionary seek（sstable） | O(log N_block) 二分 + O(M) 块内前缀扫描 | O(1) | M 为块内 prefix 头长度 |
| Term dictionary seek（FST） | O(|term|) 字节级 transition | 常驻 RAM | 每次访问 O(1) |
| `block_max_score()` 查询 | O(1) 读预计算 buffer | O(1) | 与 BM25 系数已离线算好 |

---

## 十九、BM25、Block-WAND 提前终止与 Fast Field 列式索引（算法展开）

> 本节专攻 §8.2.4 一句话提到的 Block-WAND 和 §7.3 一句话提到的 Columnar 的"算法展开"——给出 `block_max_score` 的上界公式、`block_wand_union` 的 pivot 剪枝伪代码，以及 BitPacker 多宽度 + row-id 索引构成的"数值列 B-tree"式剪枝。所有源码路径以公开仓库 `quickwit-oss/tantivy` main 为准；本仓库子模块未 init，不给精确行号。

### 19.1 BM25 公式 + fieldnorm 协同

`src/query/bm25.rs` 是 Tantivy 的 BM25 主实现。BM25 文档得分（对查询 = 多个 term 的"加权和"形式）：

$$
\text{BM25}(D, Q) = \sum_{t \in Q} \text{IDF}(t) \cdot \frac{f(t, D)}{f(t, D) + k_1 \cdot (1 - b + b \cdot \frac{|D|}{\text{avgdl}})}
$$

其中：

- $k_1, b$ 默认 1.2 / 0.75；
- $\text{IDF}(t) = \ln\!\big(1 + \frac{N - n_t + 0.5}{n_t + 0.5}\big)$（与 Lucene 8+ 同公式，避免负值）；
- 字段长度 $|D|$ 用 **fieldnorm** 量化存储（`.fieldnorm` 文件，1 字节对数编码），避免每次解码；
- avgdl 通过 `Searcher` 上下文获取（每段独立）。

源码路径：`src/query/bm25.rs`、`src/query/term_query.rs`、`src/query/score_combiner.rs`。

### 19.2 `block_max_score` 的上界计算

`BlockSegmentPostings::block_max_score()`（`src/postings/block_segment_postings.rs`）返回一个 block 内"任意 doc 在该 term 上的最大 BM25 贡献"——它是 Block-WAND 提前终止的 **最小公因子**。对 single-term block：

$$
M_t(b) = \text{IDF}(t) \cdot \frac{\max_{d \in b} (f(t, d))}{\max_{d \in b} (f(t, d)) + k_1 \cdot \big(1 - b + b \cdot \frac{\max|D|_\text{block}}{\text{avgdl}}\big)}
$$

注：分母用**该 block 内 doc 长度最大值**（非 avgdl），因为上界求大不求小。**Block-WAND 的关键不变量**：给一段 \([d_1, d_N]\) 的多个 posting，**任意 doc 在该 query 上的真实得分一定 ≤** $\sum_t M_t(b_t)$（block 内的最大值之和）。这就是用 max 替代 sum 的"安全剪枝"。

> ⚠️ fieldnorm 在按段缓存时，Tantivy 用了 `.fieldnorm` 文件保存每个 doc 的 `\`compressed(|D|)\``，block-max 阶段用 block 内最大 fieldnorm 解码 1 字节即可，无需遍 block。

源码路径：`src/postings/block_segment_postings.rs::block_max_score()`、`src/query/reqopt_scorer.rs`。

### 19.3 Block-WAND Union 的 pivot 剪枝伪代码

`src/query/boolean_query/block_wand_union.rs`（非完全精确版本，逻辑保真）：

```rust
fn block_wand_union(
    scorers: &mut [TermScorer],       // 已按 doc 升序 align 到首个有值 doc
    threshold: Score,                 // 当前 TopK 中第 K 名的分
    collector: &mut impl SegmentCollector,
) -> Score {
    while let Some(pivot_doc) = smallest_doc_within_all_blocks(scorers) {
        // 1) 用 block_max_score() 计算"pivot 段"内的上界之和
        let upper: Score = scorers.iter().map(|s| s.block_max_score()).sum();
        if upper <= threshold {
            // 2) 剪枝：跳过从 pivot_doc 到"任意 scorer block_max 都 <= threshold"的最小段
            let to_doc = scorers.iter()
                .map(|s| s.skip_block_if_max_score_leq(threshold))
                .min().unwrap_or(DocId::MAX);
            for s in scorers.iter_mut() { s.advance_to(to_doc); }
            continue;
        }
        // 3) 真得分：对该 pivot_doc 计算精确 BM25 之和
        let score: Score = scorers.iter().map(|s| s.score_at(pivot_doc)).sum();
        if score > threshold {
            collector.collect(pivot_doc, score);
        }
        // 4) 把所有不在 pivot_doc 的 scorer 推到比 pivot_doc 大的下一文档
        for s in scorers.iter_mut() { if s.doc() < pivot_doc { s.advance_to(pivot_doc + 1); } }
    }
    threshold
}
```

剪枝的核心是 **"upper ≤ threshold ⇒ 整段 [pivot_doc, to_doc] 都进不了 TopK"**：因为即使按 `block_max` 求和都不够第 K 名，真实累加只会更小。

### 19.4 Block-WAND Intersection 的前后缀和优化

`src/query/boolean_query/block_wand_intersection.rs` 用 **前后缀和** 加速"必须同时命中的 AND 类查询"：

```rust
// 把除 leader 外的 scorer 称作 secondary
// 关键数据结构：secondary_suffix_block_max[i] = sum of block_max for secondary[i+1..]
let mut secondary_block_max_scores: Box<[f32]> = ...;   // 当前段
let mut secondary_suffix_block_max: Box<[f32]> = ...;   // 已 prefix sum 倒序

// 对 leader 在 doc 的窗口内：
let leader_bms = leader.block_max_score();
let secondary_sum: Score = secondary.iter().map(|s| s.block_max_score()).sum();

// 当 leader + secondary ≤ threshold 时，扩展窗口到"所有 secondary block_max 在 doc 之前都包含"
if leader_bms + secondary_sum <= threshold {
    let window_end = secondary.iter()
        .map(|s| s.skip_block_if_max_score_leq(threshold - leader_bms - partial_suffix))
        .min().unwrap_or(DocId::MAX);
    leader.advance_to(window_end);
}
```

源码路径：`src/query/boolean_query/block_wand_intersection.rs`、`src/query/boolean_query/block_wand_union.rs`、`src/query/reqopt_scorer.rs`。

### 19.5 Fast Field（列式存储）与多宽度 BitPacking

`columnar/src/column_values/` 提供 Fast Field 的多套编码（每套按 column 类型与 cardinality 选）：

| 编码 | 适用类型 | 关键文件 | 关键 trait / 常量 |
|---|---|---|---|
| `BitpackedCodec` | 整数 u64 / i64 / u128 | `columnar/src/column_values/u64_based/bitpacked.rs`、`columnar/src/column_values/u128_based/bitpacked_u128.rs` | `compute_num_bits` 自适应位宽 |
| `CompactSpace` | 整数 u128（uuid 等） | `columnar/src/column_values/u128_based/compact_space/mod.rs` | 紧凑空间 + u64 重映射 |
| `BlockwiseLinear` | 单调近似整数 | `columnar/src/column_values/u64_based/blockwise_linear.rs` | 斜率+offset 编码 |
| `MonotonicMapping` | f64 / 日期 | `columnar/src/column_values/monotonic_mapping.rs` | 自动选 monotonic 编码 |
| `OptionalIndex` | sparse 行索引 | `columnar/src/column_index/optional_index/mod.rs` | Roaring-like dense/sparse block |

每个 Fast Field 列在 segment 文件中的字节结构（以 u64 bitpacked 为例）：

```text
.file_structure:
  index 块:    [coarse_index; block_min, block_max per N rows]
  data 块:     [rows: N lines × bit_width bits each, packed]
  train data:  [(offset, value) checkpoints for linear interpolation (optional)]
```

伪代码（`bitpacked.rs` 写入路径）：

```rust
fn write_column(&mut self, data: &[u64], output: &mut impl Write) -> io::Result<()> {
    let mut bit_packer = BitPacker::default();
    let mut prev_max: u64 = 0;
    for chunk in data.chunks(BLOCK_SIZE) {
        let num_bits = compute_num_bits(chunk.iter().copied().max().unwrap_or(0));
        for &v in chunk { bit_packer.write(v, num_bits, output)?; }
        // ★ 关键：位宽按 block 自适应
        self.coarse_index.push(CoarseEntry { num_bits, offset: bit_packer.byte_offset() });
    }
    bit_packer.close(output)?;
    Ok(())
}
```

### 19.6 Range query 的剪枝：B-tree-like block index

Fast Field 的 `index` 部分按 `block_min / block_max` 维护，**等价于 B-tree 的两层结构**：

```text
for query range [lo, hi]:
    1) 走 coarse_index 二分定位候选 block（block_max < lo 或 block_min > hi 的全剪）
    2) 解码命中 block 内的 row，做精确比较
```

伪代码（`columnar/src/column_values/mod.rs` / `column_index/optional_index/mod.rs`）：

```rust
fn range_filter(
    coarse_index: &[CoarseEntry],   // 每个块一条 (min, max, offset, num_bits)
    data: &MmapColumn,
    lo: u64,
    hi: u64,
) -> Vec<RowId> {
    let mut hits = vec![];
    // 1) 二分 / 顺序剪枝
    for (i, e) in coarse_index.iter().enumerate() {
        if e.max < lo || e.min > hi { continue; }    // ★ 整块丢弃
        // 2) 精细比较：解压后只对命中 block 做 cmp
        let block = data.decode_block(e.offset, e.num_bits, BLOCK_SIZE);
        for (row_off, &v) in block.iter().enumerate() {
            if lo <= v && v <= hi {
                hits.push(i * BLOCK_SIZE + row_off);
            }
        }
    }
    hits
}
```

### 19.7 复杂度与 sort_by 加速效果

| 场景 | Block-WAND 加速比 | 关键限制 |
|---|---|---|
| 高基数大段 + sort_by score | **5~20×**（官方 benchmark） | doc 分布越倾斜，max_score 越接近真实，加速越大 |
| term 数 ≥ 3 的 AND | **2~5×** | block_max 上界越紧越好（fieldnorm 缓存） |
| `term` 数 = 1 的 TermQuery | **1×**（仅 WAND 开销） | Block-WAND 反而退化为扫描 |
| range query + fast field | **N/B 与全表 scan 之比 ≈ B/|hits|** | 块内仍然得解压比较 |

复杂度：WAND 期望 `O(K · (Σ_block 1/M) · |posting|)`，K 为 Top-K 大小；最坏仍为 `O(|posting|)`（一个 block 也没剪）。

---

## 二十、Segment 合并、Fuzzy Automaton 与聚合两阶段（算法展开）

> 本节专攻 §十三"合并用 LogMergePolicy"、§8.2.2"FuzzyQuery 基于 Levenshtein"、§9.1.2"Collector 聚合"三处目前仅一句话的"算法展开"，以及它们与 §7.3 columnar 的衔接关系。

### 20.1 `LogMergePolicy` 的 level chunk_by 算法

`src/indexer/log_merge_policy.rs` 的默认值表：

| 参数 | 默认值 | 含义 |
|---|---|---|
| `min_num_segments` | **8** | 同 level 至少 8 段才合并 |
| `max_docs_before_merge` | **10_000_000** | 单段 > 1kw doc 不入候选 |
| `min_layer_size` | **10_000** | 任一段 doc 数下界 |
| `level_log_size` | **0.75** | 相邻 level 比率 |
| `del_docs_ratio_before_merge` | **1.0** | 删除比达 100% 触发合并 |

**核心算法 `compute_merge_candidates`**（`src/indexer/log_merge_policy.rs`）：

```rust
fn compute_merge_candidates(&self, segments: &[SegmentMeta]) -> Vec<MergeCandidate> {
    // 1) 过滤超大段 + 按 size 倒排
    let sorted = segments.iter()
        .filter(|s| s.num_docs() <= self.max_docs_before_merge)
        .sorted_by_key(|s| std::cmp::Reverse(s.max_doc()))
        .collect_vec();

    // 2) chunk_by：按 log2 大小把段分组到 level
    let mut current_max_log_size = f64::MAX;
    let levels = sorted.into_iter().chunk_by(|seg| {
        let seg_log = f64::from(self.clip_min_size(seg.num_docs())).log2();
        if seg_log < (current_max_log_size - self.level_log_size) {
            current_max_log_size = seg_log;        // ★ 启动一个新 level
        }
        current_max_log_size
    });

    // 3) 每个 level 选段：>= min_num_segments 或 删除超阈 触发合并
    levels
        .filter(|lvl| lvl.len() >= self.min_num_segments
                  || self.has_segment_above_deletes_threshold(lvl))
        .map(|lvl| MergeCandidate(lvl.iter().map(|s| s.id()).collect()))
        .collect()
}
```

**关键代价与 Trade-off**：

- **选段代价**：`O(N log N)`（sort）+ `O(N)`（chunk_by）+ `O(L·K)`（level 过滤），N 为段数；
- **合并代价**：合并时**每段重写一次 FST + post 索引 + fast field**，写入放大 ≈ $\log_{\text{ratio}} N$，level_log_size = 0.75 ⇒ 写放大常数 ≈ 1 / 0.75 ≈ 1.33×；
- **取舍**：默认 1.0 的 `del_docs_ratio_before_merge` 实际**几乎不会因删除触发合并**——这是为了向后兼容；常见修改是 `set_del_docs_ratio_before_merge(0.2)` 减少段数；
- **代价模型**：`segment_max_size = base * level_log_size^k`，k 为 level 数，写放大就是从 n 段回到 1 段需要的 k 步迭代。

源码路径：
- `src/indexer/log_merge_policy.rs`：`LogMergePolicy::compute_merge_candidates`、`default`
- `src/indexer/merge_policy.rs`：`MergePolicy` trait、`MergeCandidate` 结构
- `src/indexer/merger.rs`：实际合并（列存、posting 重写）

### 20.2 `FuzzyTermQuery` 的 Levenshtein Automaton

`src/query/fuzzy_query.rs` 借助 `levenshtein_automata` crate（外部 dep）将 term → DFA。流程：

```text
FuzzyTermQuery::new(term, distance, prefix?, transposition?)
   ├── transposition ? build Levenshtein with Transposition variant : Standard
   └── automaton() -> DfaWrapper (interfaced as &'static DFA)
        静态缓存：[[OnceCell<Builder>; 2]; 3]   // transposition × distance ∈ {0,1,2}
```

构造逻辑（伪代码）：

```rust
const MAX_DISTANCE: usize = 2;            // 不支持 > 2，提示用户选更小值

pub fn automaton(&self) -> Result<DfaWrapper> {
    let distance = self.distance as usize;
    if distance > MAX_DISTANCE {
        return Err(InvalidLevenshteinDistance);
    }
    let use_transposition = self.transposition as usize;
    let builder = AUTOMATON_BUILDER[use_transposition][distance]
        .get_or_init(|| LevenshteinAutomatonBuilder::new(distance, use_transposition==1));
    Ok(DfaWrapper(builder.build(self.term.as_bytes())))
}
```

**prefix + fuzzy 状态转移**：当 prefix=True 时，Tantivy 会把"前 prefix_len 个字符"作为确定性 prefix lock，再把 fuzzy automaton 限制在剩余后缀上（这避免了"全部字符都允许 0~distance 变"的高内存 DFA）：

```rust
if let Some(prefix_len) = self.prefix {
    let prefix = &self.term[..prefix_len];
    let suffix = &self.term[prefix_len..];
    // 1) 前缀必须精确匹配 → 进入 suffix automaton
    // 2) suffix 走 Levenshtein DFA，distance 仍然全局
    Ok(DfaWrapper(combine(ExactPrefixDFA::new(prefix), builder.build(suffix.as_bytes()))))
}
```

**复杂度**：

- 构造 DFA：`O(|term| · (distance + 1))`；
- 接受一候选 term：`O(|term|)` 字节切；
- 整段求和：`O(|term_dict|) × 单 term O(|term|)`（FST 走 transition 即可）。

源码路径：
- `src/query/fuzzy_query.rs`：automaton() 实现
- `src/query/regex_query.rs`：用同一份 `automaton_weight.rs`（regex 同样走 DFA）
- `src/query/automaton_weight.rs`：把 DFA 转 Weight/Scorer 的桥接

### 20.3 聚合两阶段：`SegmentCollector` → `Collector`

`src/collector/mod.rs` 定义两个 trait：

```rust
pub trait Collector: Sync + Send {
    type Child: SegmentCollector;          // per-segment 的聚合器
    fn for_segment(&self, ...) -> Self::Child;          // 创建 per-segment 拷贝
    fn merge(&self, fruits: Vec<<Self::Child as SegmentCollector>::Fruit>) -> ...; // 跨段归并
    fn requires_scoring(&self) -> bool { false }
}

pub trait SegmentCollector: Sync + Send {
    type Fruit;                          // per-segment 局部的"中间结果"
    fn collect(&mut self, doc: DocId, score: Score);
    fn harvest(self) -> Self::Fruit;     // 段结束产出 fruit，由 Collector::merge 收
}
```

**两阶段伪代码**（搜索入口简化）：

```rust
fn search(collector: &impl Collector, searcher: &Searcher) -> Result<Collector::Fruit> {
    let mut per_segment_fruits = vec![];
    for segment in searcher.segment_readers() {
        let mut seg_collector = collector.for_segment(/* segment_id, del_bitset */);
        for scorer in segment.scorers() {
            for doc in scorer.iter_docs() {
                seg_collector.collect(doc, scorer.score_at(doc));
            }
        }
        per_segment_fruits.push(seg_collector.harvest());
    }
    // ★ 跨段 merge
    Ok(collector.merge(per_segment_fruits))
}
```

### 20.4 `FacetCollector` 与 `hash aggregation` / `block aggregation`

Tantivy 主仓储只内置少数 Collector（`Count`、`TopDocs`、`TopFieldDocs`、`FacetCollector`）。两个最关键的优化分别在：

**(a) `FacetCollector`**（`src/collector/facet_collector.rs`）：

- per-segment 阶段调 `columnar::Dictionary::range().into_stream()` 把 hit docid 范围直接转成 columnar Streamer，**省去倒排→memory 反查的步骤**；
- `FacetSegmentCollector` 内维护 `HashMap<FacetOrd, u64>`（或在 `KeyCachePolicy::All` 时维护 `Vec<u64>`），通过 `facet_terms: &mut columnar::Streamer` 沿 streamer 推进 docid，配合 alive bitset 决定是否 ++；
- merge 阶段直接把 `HashMap<FacetOrd, u64>` union 起来即可。

**(b) `Quickwit quickwit-aggregations/` 里的 block aggregation**（仅为参考，**不在 Tantivy 主仓库**）：

- `BlockHashAggregator` 的核心是把 terms 按 8/64/256 一组打包聚合，**避免每个 doc 一次 hash 操作**；
- block 内通过 SIMD-friendly 累加器（`[u64; 4]`、`packed_at_simd`）做 8 倍并行 ++；
- 内存权衡：
  - **hash aggregation**：每条 term 一个 hash entry，O(|unique_terms|) 内存；
  - **block aggregation**：固定大小 block buffer（一般 4 KiB ~ 64 KiB），**摊销 `O(block_size)` 内存**，但要求 key 在每个 block 内是局部稠密的（不然会 fallback）。

源码路径：
- `src/collector/mod.rs`：`Collector` / `SegmentCollector` trait
- `src/collector/facet_collector.rs`：`FacetCollector` 与 `FacetSegmentCollector`
- `src/collector/multi_collector.rs`：多个 Collector 共享 scorer 遍历
- `columnar/src/dictionary.rs` / `columnar/src/iterable.rs`：Streamer 接口

### 20.5 「倒排索引 vs 列存索引」取舍总表

| 维度 | 倒排索引（Postings） | 列存索引（Fast Field / columnar/） |
|---|---|---|
| 主要查询 | 关键词、BM25 | 范围、sort by、聚合 |
| 数据顺序 | **doc → term → position** | **column-wise** |
| 压缩 | delta + bitpack + brotli | bitpack + monotonic mapping + Roaring sparse index |
| 主索引文件 | `.post` + `.pos` + `.termdict` | `.fast` + `.columnstore` |
| 写入代价 | block flush 触发即可 | 每列独立写；batch 写 |

`engineering/` 落地点（参考 §十七既有建议）：

- `engineering/src/index/columnar.c` 可参考 `columnar/src/column_values/u64_based/bitpacked.rs::write_column`；
- `engineering/src/algo/fst.c` 可参考 `src/termdict/fst_termdict/termdict.rs::lookup`；
- `engineering/src/algo/posting.c` 可参考 §18.4 的 `block_advance` + block skip cursor 模型。

### 20.6 复杂度小结

| 操作 | 时间复杂度 | 空间复杂度 | 备注 |
|---|---|---|---|
| LogMergePolicy 选段 | O(N log N) | O(N) | N = segments |
| 一次 merge 写回 | O(Σ \|seg_i\|) | O(Σ \|seg_i\|) 临时 | FST/post 重写 |
| Fuzzy automaton 构造 | O(\|term\|·d) | O(\|term\|·d) 状态 | d ≤ 2 |
| Fuzzy query 跑过 term dict | O(\|term_dict\| × \|term\|) | O(d²) DFA | FST 转发 O(1) |
| `SegmentCollector` 收集 | O(\|docs_in_segment\|) | 取决于聚合策略 | hash O(unique_terms) / block O(block_size) |
| `Collector::merge` | O(\|segments\| × \|fruit\|) | O(\|fruit\|) 累计 | fruit 因聚合而异 |
| FacetCollector 单段 | O(\|docs\|) | O(unique_facets) | per-segment hashmap |
| Block aggregator（参考） | O(\|docs\|/B) block 推进 | O(B) 摊销 | 哈希按 block 摊 |

---

## 二十一、参考资料


1. Tantivy 官方文档：<https://tantivy.com/docs/>
2. Tantivy GitHub：<https://github.com/quickwit-oss/tantivy>
3. Quickwit 官方（Cloud Native Search）：<https://quickwit.io/>
4. *Optimized Top-K Document Retrieval Using Heaps*（Block-WAND 基础论文）
5. Block-Max WAND 算法论文
6. Apache Lucene 9.x 文档：<https://lucene.apache.org/core/>
7. Block-Based Posting List 编码（Lucene 90 论文）
8. ParadeDB：<https://github.com/paradedb/paradedb>
9. FST 实现：<https://blog.mikemccandless.com/>（Mike McCandless 系列）
10. Meilisearch milli crate：<https://github.com/meilisearch/milli>
