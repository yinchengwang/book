# 多模态数据库 Wiki 库

> 本目录收录 `reference/` 下 13 个数据模型分类中 68 个开源数据库项目的深度中文介绍。

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
| [stream](./stream/) | 4 | kafka、redpanda、pulsar、nats |
| [embedded](./embedded/) | 4 | duckdb、lmdb、boltdb、pklite |
| [extension](./extension/) | 6 | arrow、pgvector、datafusion、velox、lance、hnswlib |

## 写作规范

按 `docs/superpowers/specs/2026-10-16-multimodal-db-wiki-design.md` 第 4 节文档骨架执行。每个 wiki 必须满足：

- 至少 10 个章节（深度扩展版基线）
- 至少 1 张 ASCII 架构图或关键流程图
- 至少 5 处源码路径引用
- 至少 3 个外部引用（论文/官方文档/benchmark）
- 中文为主，专有名词保留英文
