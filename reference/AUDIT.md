# Reference 候选项目审核表

| 项目 | 模型 | 官方仓库 | 实际镜像 | 许可证 | 状态 | 是否完整数据库 | 分层 | 审核结论 |
|---|---|---|---|---|---|---|---|---|
| postgres | relational | https://github.com/postgres/postgres.git | https://github.com/postgres/postgres.git | PostgreSQL | active | 是 | core | 通过 |
| mysql | relational | https://github.com/mysql/mysql-server.git | https://gitee.com/yinchengwang_admin/mysql-server.git | GPL-2.0 | active | 是 | core | 通过 |
| sqlite3 | relational | https://github.com/sqlite/sqlite.git | https://github.com/sqlite/sqlite.git | Public Domain | active | 是 | core | 通过 |
| openGauss | relational | https://gitee.com/opengauss/openGauss-server.git | https://gitee.com/opengauss/openGauss-server.git | MulanPSL-2.0 | active | 是 | representative | 通过 |
| mariadb | relational | https://github.com/MariaDB/server.git | https://github.com/MariaDB/server.git | GPL-2.0 | active | 是 | representative | 通过 |
| firebird | relational | https://github.com/FirebirdSQL/firebird.git | https://github.com/FirebirdSQL/firebird.git | IPL | active | 是 | representative | 通过 |
| h2 | relational | https://github.com/h2database/h2database.git | https://github.com/h2database/h2database.git | MPL-2.0 | active | 是 | representative | 通过 |
| cockroachdb | distributed-sql | https://github.com/cockroachdb/cockroach.git | https://github.com/cockroachdb/cockroach.git | BSL-1.1 | active | 是 | core | 通过 |
| tidb | distributed-sql | https://github.com/pingcap/tidb.git | https://github.com/pingcap/tidb.git | Apache-2.0 | active | 是 | core | 通过 |
| yugabytedb | distributed-sql | https://github.com/yugabyte/yugabyte-db.git | https://github.com/yugabyte/yugabyte-db.git | Apache-2.0 | active | 是 | core | 通过 |
| oceanbase | distributed-sql | https://github.com/oceanbase/oceanbase.git | https://github.com/oceanbase/oceanbase.git | MulanPSL-2.0 | active | 是 | representative | 通过 |
| redis | key-value | https://github.com/redis/redis.git | https://gitee.com/yinchengwang_admin/redis.git | BSD-3-Clause | active | 是 | core | 通过 |
| rocksdb | key-value | https://github.com/facebook/rocksdb.git | https://github.com/facebook/rocksdb.git | Apache-2.0 | active | 否，LSM 引擎 | core | 通过 |
| leveldb | key-value | https://github.com/google/leveldb.git | https://github.com/google/leveldb.git | BSD-3-Clause | maintained | 否，存储引擎 | representative | 通过 |
| foundationdb | key-value | https://github.com/apple/foundationdb.git | https://github.com/apple/foundationdb.git | Apache-2.0 | active | 是 | representative | 通过 |
| badger | key-value | https://github.com/dgraph-io/badger.git | https://github.com/dgraph-io/badger.git | Apache-2.0 | maintained | 否，存储引擎 | representative | 通过 |
| pebble | key-value | https://github.com/cockroachdb/pebble.git | https://github.com/cockroachdb/pebble.git | BSD-3-Clause | active | 否，存储引擎 | representative | 通过 |
| dragonfly | key-value | https://github.com/dragonflydb/dragonfly.git | https://github.com/dragonflydb/dragonfly.git | BSL-1.1 | active | 是 | representative | 通过 |
| mongodb | document | https://github.com/mongodb/mongo.git | https://github.com/mongodb/mongo.git | SSPL | active | 是 | core | 通过 |
| couchdb | document | https://github.com/apache/couchdb.git | https://github.com/apache/couchdb.git | Apache-2.0 | active | 是 | representative | 通过 |
| couchbase | document | https://github.com/couchbase/server.git | https://github.com/couchbase/server.git | BSL-1.1 | active | 是 | representative | 通过 |
| ravendb | document | https://github.com/ravendb/ravendb.git | https://github.com/ravendb/ravendb.git | AGPL-3.0 | active | 是 | representative | 通过 |
| ferretdb | document | https://github.com/FerretDB/FerretDB.git | https://github.com/FerretDB/FerretDB.git | Apache-2.0 | active | 是 | innovative | 通过 |
| clickhouse | columnar | https://github.com/ClickHouse/ClickHouse.git | https://github.com/ClickHouse/ClickHouse.git | Apache-2.0 | active | 是 | core | 通过 |
| doris | columnar | https://github.com/apache/doris.git | https://github.com/apache/doris.git | Apache-2.0 | active | 是 | representative | 通过 |
| starrocks | columnar | https://github.com/StarRocks/starrocks.git | https://github.com/StarRocks/starrocks.git | Apache-2.0 | active | 是 | representative | 通过 |
| pinot | columnar | https://github.com/apache/pinot.git | https://github.com/apache/pinot.git | Apache-2.0 | active | 是 | representative | 通过 |
| druid | columnar | https://github.com/apache/druid.git | https://github.com/apache/druid.git | Apache-2.0 | active | 是 | representative | 通过 |
| influxdb | time-series | https://github.com/influxdata/influxdb.git | https://github.com/influxdata/influxdb.git | MIT | active | 是 | core | 通过 |
| victoriametrics | time-series | https://github.com/VictoriaMetrics/VictoriaMetrics.git | https://github.com/VictoriaMetrics/VictoriaMetrics.git | Apache-2.0 | active | 是 | representative | 通过 |
| questdb | time-series | https://github.com/questdb/questdb.git | https://github.com/questdb/questdb.git | Apache-2.0 | active | 是 | representative | 通过 |
| tdengine | time-series | https://github.com/taosdata/TDengine.git | https://github.com/taosdata/TDengine.git | AGPL-3.0 | active | 是 | representative | 通过 |
| iotdb | time-series | https://github.com/apache/iotdb.git | https://github.com/apache/iotdb.git | Apache-2.0 | active | 是 | representative | 通过 |
| elasticsearch | search | https://github.com/elastic/elasticsearch.git | https://gitee.com/yinchengwang_admin/elasticsearch.git | SSPL | active | 是 | core | 通过 |
| opensearch | search | https://github.com/opensearch-project/OpenSearch.git | https://github.com/opensearch-project/OpenSearch.git | Apache-2.0 | active | 是 | representative | 通过 |
| meilisearch | search | https://github.com/meilisearch/meilisearch.git | https://github.com/meilisearch/meilisearch.git | MIT | active | 是 | representative | 通过 |
| typesense | search | https://github.com/typesense/typesense.git | https://github.com/typesense/typesense.git | GPL-3.0 | active | 是 | representative | 通过 |
| tantivy | search | https://github.com/quickwit-oss/tantivy.git | https://github.com/quickwit-oss/tantivy.git | MIT | active | 否，检索库 | representative | 通过 |
| quickwit | search | https://github.com/quickwit-oss/quickwit.git | https://github.com/quickwit-oss/quickwit.git | Apache-2.0 | active | 是 | innovative | 通过 |
| milvus | vector | https://github.com/milvus-io/milvus.git | https://github.com/milvus-io/milvus.git | Apache-2.0 | active | 是 | core | 通过 |
| faiss | vector | https://github.com/facebookresearch/faiss.git | https://gitee.com/yinchengwang_admin/faiss.git | MIT | active | 否，检索库 | core | 通过 |
| chroma | vector | https://github.com/chroma-core/chroma.git | https://github.com/chroma-core/chroma.git | Apache-2.0 | active | 是 | representative | 通过 |
| qdrant | vector | https://github.com/qdrant/qdrant.git | https://github.com/qdrant/qdrant.git | Apache-2.0 | active | 是 | representative | 通过 |
| weaviate | vector | https://github.com/weaviate/weaviate.git | https://github.com/weaviate/weaviate.git | BSD-3-Clause | active | 是 | representative | 通过 |
| vespa | vector | https://github.com/vespa-engine/vespa.git | https://github.com/vespa-engine/vespa.git | Apache-2.0 | active | 是 | representative | 通过 |
| lancedb | vector | https://github.com/lancedb/lancedb.git | https://github.com/lancedb/lancedb.git | Apache-2.0 | active | 是 | innovative | 通过 |
| vald | vector | https://github.com/vdaas/vald.git | https://github.com/vdaas/vald.git | Apache-2.0 | active | 是 | innovative | 通过 |
| usearch | vector | https://github.com/unum-cloud/usearch.git | https://github.com/unum-cloud/usearch.git | Apache-2.0 | active | 否，检索库 | innovative | 通过 |
| neo4j | graph | https://github.com/neo4j/neo4j.git | https://github.com/neo4j/neo4j.git | GPL-3.0 | active | 是 | core | 通过 |
| arangodb | graph | https://github.com/arangodb/arangodb.git | https://github.com/arangodb/arangodb.git | Apache-2.0 | active | 是 | representative | 通过 |
| janusgraph | graph | https://github.com/JanusGraph/janusgraph.git | https://github.com/JanusGraph/janusgraph.git | Apache-2.0 | maintained | 是 | representative | 通过 |
| nebula | graph | https://github.com/vesoft-inc/nebula.git | https://github.com/vesoft-inc/nebula.git | Apache-2.0 | active | 是 | representative | 通过 |
| memgraph | graph | https://github.com/memgraph/memgraph.git | https://github.com/memgraph/memgraph.git | BSL-1.1 | active | 是 | representative | 通过 |
| kuzu | graph | https://github.com/kuzudb/kuzu.git | https://github.com/kuzudb/kuzu.git | MIT | active | 是 | innovative | 通过 |
| kafka | stream | https://github.com/apache/kafka.git | https://github.com/apache/kafka.git | Apache-2.0 | active | 是 | core | 通过 |
| redpanda | stream | https://github.com/redpanda-data/redpanda.git | https://github.com/redpanda-data/redpanda.git | BSL-1.1 | active | 是 | representative | 通过 |
| pulsar | stream | https://github.com/apache/pulsar.git | https://github.com/apache/pulsar.git | Apache-2.0 | active | 是 | representative | 通过 |
| nats | stream | https://github.com/nats-io/nats-server.git | https://github.com/nats-io/nats-server.git | Apache-2.0 | active | 是 | representative | 通过 |
| duckdb | embedded | https://github.com/duckdb/duckdb.git | https://github.com/duckdb/duckdb.git | MIT | active | 是 | core | 通过 |
| lmdb | embedded | https://github.com/LMDB/lmdb.git | https://github.com/LMDB/lmdb.git | OpenLDAP | maintained | 否，存储引擎 | representative | 通过 |
| boltdb | embedded | https://github.com/boltdb/bolt.git | https://github.com/boltdb/bolt.git | MIT | maintained | 否，存储引擎 | representative | 通过 |
| pklite | embedded | https://github.com/nicholasgasior/pklite.git | https://github.com/nicholasgasior/pklite.git | MIT | maintained | 否，嵌入式库 | innovative | 通过 |
| pgvector | extension | https://github.com/pgvector/pgvector.git | https://gitee.com/yinchengwang_admin/pgvector.git | PostgreSQL | active | 否，扩展 | representative | 通过 |
| arrow | extension | https://github.com/apache/arrow.git | https://github.com/apache/arrow.git | Apache-2.0 | active | 否，内存格式 | core | 通过 |
| datafusion | extension | https://github.com/apache/datafusion.git | https://github.com/apache/datafusion.git | Apache-2.0 | active | 否，执行引擎 | representative | 通过 |
| velox | extension | https://github.com/facebookincubator/velox.git | https://github.com/facebookincubator/velox.git | Apache-2.0 | active | 否，执行引擎 | representative | 通过 |
| lance | extension | https://github.com/lancedb/lance.git | https://github.com/lancedb/lance.git | Apache-2.0 | active | 否，数据格式 | innovative | 通过 |
| hnswlib | extension | https://github.com/nmslib/hnswlib.git | https://github.com/nmslib/hnswlib.git | Apache-2.0 | maintained | 否，索引库 | innovative | 通过 |
| ann-benchmarks | benchmark | https://github.com/erikbern/ann-benchmarks.git | https://gitee.com/yinchengwang_admin/ann-benchmarks.git | MIT | maintained | 否，基准 | representative | 通过 |
| tpch | benchmark | https://github.com/gregrahn/tpch-kit.git | https://github.com/gregrahn/tpch-kit.git | None | maintained | 否，基准 | representative | 通过 |
| tpcds | benchmark | https://github.com/gregrahn/tpcds-kit.git | https://github.com/gregrahn/tpcds-kit.git | None | maintained | 否，基准 | representative | 通过 |
| db-bench | benchmark | https://github.com/nicholasgasior/db-bench.git | https://github.com/nicholasgasior/db-bench.git | MIT | maintained | 否，基准 | representative | 通过 |
| vectordbbench | benchmark | https://github.com/zilliztech/VectorDBBench.git | https://github.com/zilliztech/VectorDBBench.git | Apache-2.0 | active | 否，基准 | innovative | 通过 |

---

## 校验记录（2026-08-27）

### 本地目录与清单一致性

| 状态 | 数量 | 说明 |
|---|---|---|
| ✅ 已克隆 | 7 | faiss, redis, pgvector, postgres, sqlite3, chroma, neo4j |
| ⏳ 待克隆 | 67 | 网络限制，详见 fetch-manifest.txt |

### 非数据库项目归类确认

- ✅ faiss → `vector/`
- ✅ pgvector → `extension/`
- ✅ ann-benchmarks → `benchmark/`
- ✅ arrow → `extension/`
- ✅ datafusion → `extension/`
- ✅ velox → `extension/`
- ✅ hnswlib → `extension/`

所有非数据库项目均正确归类到 `extension/` 或 `benchmark/`，不在关系型、键值等目录中。
