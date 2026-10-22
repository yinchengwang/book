# 多模态数据库 Wiki 库

> 本目录收录 `reference/` 下 13 个数据模型分类中 68 个开源数据库项目的深度中文介绍。
>
> **两种完成度需区分**（2026-10-22 更新）：
> - **建档 ✅**：68/68。每篇均含完整章节骨架、ASCII 架构图、源码路径引用与外部参考。
> - **算法深化 🔬**：68/68。指在骨架之外追加了源码级算法章节（数据结构选型、核心算法伪代码与复杂度分析、关键源码路径）。
>
> 「项目状态总览」中的**深化**列标注 🔬 / ⬜。**深化质量分两档**：
> - 子模块已init 的篇目给出行号级引用（如 `UnifiedLog.java:563`），可用脚本全量校验；
> - 子模块未 init 的篇目（embedded 4 + extension 6）按 canonical 路径 + 「行号需校对」标注处理。

## 分类索引

| 分类 | 项目数 | 项目清单 |
|------|--------|----------|
| [relational](./relational/) | 7 | postgres、mysql、sqlite3、openGauss、mariadb、firebird、h2 |
| [distributed-sql](./distributed-sql/) | 4 | cockroachdb、tidb、yugabytedb、oceanbase |
| [key-value](./key-value/) | 7 | redis、rocksdb、leveldb、foundationdb、badger、pebble、dragonfly |
| [document](./document/) | 5 | mongodb、couchdb、couchbase、ravendb、ferretdb |
| [columnar](./columnar/) | 5 | clickhouse、doris、starrocks、pinot、druid |
| [time-series](./time-series/) | 5 | influxdb、victoriametrics、questdb、tdengine、iotdb |
| [search](./search/) | 6 | elasticsearch、opensearch、meilisearch、typesense、tantivy、quickwit |
| [vector](./vector/) | 9 | chroma、milvus、faiss、qdrant、weaviate、vespa、lancedb、vald、usearch |
| [graph](./graph/) | 6 | neo4j、arangodb、janusgraph、nebula、memgraph、kuzu |
| [stream](./stream/) | 4 | kafka、nats、pulsar、redpanda |
| [embedded](./embedded/) | 4 | boltdb、duckdb、lmdb、pklite |
| [extension](./extension/) | 6 | arrow、datafusion、hnswlib、lance、pgvector、velox |

## 写作规范

按 `docs/superpowers/specs/2026-10-16-multimodal-db-wiki-design.md` 第 4 节文档骨架执行。每个 wiki 必须满足：

- 至少 10 个章节（深度扩展版基线）
- 至少 1 张 ASCII 架构图或关键流程图
- 至少 5 处源码路径引用
- 至少 3 个外部引用（论文/官方文档/benchmark）
- 中文为主，专有名词保留英文

## 项目状态总览

以下 68 个项目**建档全部完成**，每篇 wiki 含完整章节、架构图、源码引用与外部参考。

「深化」列区分是否额外追加了源码级算法章节：🔬 = 已深化（68 篇），⬜ = 待深化（0 篇）。

### relational（7 篇）

| 分类 | 项目 | 建档 | 深化 |
|------|------|------|------|
| relational | [postgres.md](./relational/postgres.md) | ✅ | 🔬 |
| relational | [mysql.md](./relational/mysql.md) | ✅ | 🔬 |
| relational | [sqlite3.md](./relational/sqlite3.md) | ✅ | 🔬 |
| relational | [openGauss.md](./relational/openGauss.md) | ✅ | 🔬 |
| relational | [mariadb.md](./relational/mariadb.md) | ✅ | 🔬 |
| relational | [firebird.md](./relational/firebird.md) | ✅ | 🔬 |
| relational | [h2.md](./relational/h2.md) | ✅ | 🔬 |

### distributed-sql（4 篇）

| 分类 | 项目 | 建档 | 深化 |
|------|------|------|------|
| distributed-sql | [cockroachdb.md](./distributed-sql/cockroachdb.md) | ✅ | 🔬 |
| distributed-sql | [tidb.md](./distributed-sql/tidb.md) | ✅ | 🔬 |
| distributed-sql | [yugabytedb.md](./distributed-sql/yugabytedb.md) | ✅ | 🔬 |
| distributed-sql | [oceanbase.md](./distributed-sql/oceanbase.md) | ✅ | 🔬 |

### key-value（7 篇）

| 分类 | 项目 | 建档 | 深化 |
|------|------|------|------|
| key-value | [redis.md](./key-value/redis.md) | ✅ | 🔬 |
| key-value | [rocksdb.md](./key-value/rocksdb.md) | ✅ | 🔬 |
| key-value | [leveldb.md](./key-value/leveldb.md) | ✅ | 🔬 |
| key-value | [foundationdb.md](./key-value/foundationdb.md) | ✅ | 🔬 |
| key-value | [badger.md](./key-value/badger.md) | ✅ | 🔬 |
| key-value | [pebble.md](./key-value/pebble.md) | ✅ | 🔬 |
| key-value | [dragonfly.md](./key-value/dragonfly.md) | ✅ | 🔬 |

### document（5 篇）

| 分类 | 项目 | 建档 | 深化 |
|------|------|------|------|
| document | [mongodb.md](./document/mongodb.md) | ✅ | 🔬 |
| document | [couchdb.md](./document/couchdb.md) | ✅ | 🔬 |
| document | [couchbase.md](./document/couchbase.md) | ✅ | 🔬 |
| document | [ravendb.md](./document/ravendb.md) | ✅ | 🔬 |
| document | [ferretdb.md](./document/ferretdb.md) | ✅ | 🔬 |

### columnar（5 篇）

| 分类 | 项目 | 建档 | 深化 |
|------|------|------|------|
| columnar | [clickhouse.md](./columnar/clickhouse.md) | ✅ | 🔬 |
| columnar | [doris.md](./columnar/doris.md) | ✅ | 🔬 |
| columnar | [starrocks.md](./columnar/starrocks.md) | ✅ | 🔬 |
| columnar | [pinot.md](./columnar/pinot.md) | ✅ | 🔬 |
| columnar | [druid.md](./columnar/druid.md) | ✅ | 🔬 |

### time-series（5 篇）

| 分类 | 项目 | 建档 | 深化 |
|------|------|------|------|
| time-series | [influxdb.md](./time-series/influxdb.md) | ✅ | 🔬 |
| time-series | [victoriametrics.md](./time-series/victoriametrics.md) | ✅ | 🔬 |
| time-series | [questdb.md](./time-series/questdb.md) | ✅ | 🔬 |
| time-series | [tdengine.md](./time-series/tdengine.md) | ✅ | 🔬 |
| time-series | [iotdb.md](./time-series/iotdb.md) | ✅ | 🔬 |

### search（6 篇）

| 分类 | 项目 | 建档 | 深化 |
|------|------|------|------|
| search | [elasticsearch.md](./search/elasticsearch.md) | ✅ | 🔬 |
| search | [opensearch.md](./search/opensearch.md) | ✅ | 🔬 |
| search | [meilisearch.md](./search/meilisearch.md) | ✅ | 🔬 |
| search | [typesense.md](./search/typesense.md) | ✅ | 🔬 |
| search | [tantivy.md](./search/tantivy.md) | ✅ | 🔬 |
| search | [quickwit.md](./search/quickwit.md) | ✅ | 🔬 |

### vector（9 篇）

| 分类 | 项目 | 建档 | 深化 |
|------|------|------|------|
| vector | [chroma.md](./vector/chroma.md) | ✅ | 🔬 |
| vector | [milvus.md](./vector/milvus.md) | ✅ | 🔬 |
| vector | [faiss.md](./vector/faiss.md) | ✅ | 🔬 |
| vector | [qdrant.md](./vector/qdrant.md) | ✅ | 🔬 |
| vector | [weaviate.md](./vector/weaviate.md) | ✅ | 🔬 |
| vector | [vespa.md](./vector/vespa.md) | ✅ | 🔬 |
| vector | [lancedb.md](./vector/lancedb.md) | ✅ | 🔬 |
| vector | [vald.md](./vector/vald.md) | ✅ | 🔬 |
| vector | [usearch.md](./vector/usearch.md) | ✅ | 🔬 |

### graph（6 篇）

| 分类 | 项目 | 建档 | 深化 |
|------|------|------|------|
| graph | [neo4j.md](./graph/neo4j.md) | ✅ | 🔬 |
| graph | [arangodb.md](./graph/arangodb.md) | ✅ | 🔬 |
| graph | [janusgraph.md](./graph/janusgraph.md) | ✅ | 🔬 |
| graph | [nebula.md](./graph/nebula.md) | ✅ | 🔬 |
| graph | [memgraph.md](./graph/memgraph.md) | ✅ | 🔬 |
| graph | [kuzu.md](./graph/kuzu.md) | ✅ | 🔬 |

### stream（4 篇）

| 分类 | 项目 | 建档 | 深化 |
|------|------|------|------|
| stream | [kafka.md](./stream/kafka.md) | ✅ | 🔬 |
| stream | [redpanda.md](./stream/redpanda.md) | ✅ | 🔬 |
| stream | [pulsar.md](./stream/pulsar.md) | ✅ | 🔬 |
| stream | [nats.md](./stream/nats.md) | ✅ | 🔬 |

### embedded（4 篇）

| 分类 | 项目 | 建档 | 深化 |
|------|------|------|------|
| embedded | [duckdb.md](./embedded/duckdb.md) | ✅ | 🔬 |
| embedded | [lmdb.md](./embedded/lmdb.md) | ✅ | 🔬 |
| embedded | [boltdb.md](./embedded/boltdb.md) | ✅ | 🔬 |
| embedded | [pklite.md](./embedded/pklite.md) | ✅ | 🔬 |

### extension（6 篇）

| 分类 | 项目 | 建档 | 深化 |
|------|------|------|------|
| extension | [arrow.md](./extension/arrow.md) | ✅ | 🔬 |
| extension | [pgvector.md](./extension/pgvector.md) | ✅ | 🔬 |
| extension | [datafusion.md](./extension/datafusion.md) | ✅ | 🔬 |
| extension | [velox.md](./extension/velox.md) | ✅ | 🔬 |
| extension | [lance.md](./extension/lance.md) | ✅ | 🔬 |
| extension | [hnswlib.md](./extension/hnswlib.md) | ✅ | 🔬 |

---

**最近更新：2026-10-22**
**统计：68 篇 wiki，建档全部完成 ✅；算法深化 68/68 🔬**

> benchmark/ 分类下 5 个项目（ann-benchmarks、vectordbbench、tpch、tpcds、db-bench）作为参考资料在 wiki 引用部分提及，不单独写 wiki。

---

## 差距分析

基于本 wiki 库的「自研 vs 业界」分析已发布，详见：

- 总报告：[docs/gap-analysis/README.md](../gap-analysis/README.md)
- 子报告：[docs/gap-analysis/relational.md](../gap-analysis/relational.md)、[key-value.md](../gap-analysis/key-value.md)、[document.md](../gap-analysis/document.md)、[columnar.md](../gap-analysis/columnar.md)、[time-series.md](../gap-analysis/time-series.md)、[search.md](../gap-analysis/search.md)、[vector.md](../gap-analysis/vector.md)、[graph.md](../gap-analysis/graph.md)、[stream.md](../gap-analysis/stream.md)、[embedded.md](../gap-analysis/embedded.md)、[extension.md](../gap-analysis/extension.md)

最后更新：2026-10-16
