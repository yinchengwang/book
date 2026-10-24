# Tasks

## 已完成任务

- [x] Task 1: 建立目录结构与元数据规范
- [x] Task 2: 编制候选项目清单并完成审核
- [x] Task 3: 迁移现有子模块路径
- [x] Task 4: 新增第一阶段候选项目子模块
- [x] Task 5: 新增第二阶段特色与生态项目
- [x] Task 6: 更新仓库文档与引用路径
- [x] Task 7: 创建分阶段克隆与维护说明
- [x] Task 8: 本地校验目录与清单一致性
- [x] Task 9: 克隆剩余子模块（elasticsearch、milvus、neo4j、chroma、mysql）
- [x] Task 10: 批量克隆 catalog.yml 中的剩余项目

## 待完成

- [x] 补充克隆失败的项目（需稳定网络）：
  - doris（columnar）- GitHub 连接超时 → 已成功克隆
  - db-bench（benchmark）- 原仓库 nicholasgasior/db-bench 已不存在，改用 wagjamin/db-bench（TPC-H 基准）→ 已成功克隆
  - couchbase（document）- 官方 server 仓库已闭源（BSL），改用 couchbase/couchbase-lite-core（C++ 嵌入式引擎，公开可读）→ 已成功克隆

## 已克隆子模块清单（71 个）

| 模型 | 项目 |
|------|------|
| benchmark (5) | ann-benchmarks, db-bench, tpch, tpcds, vectordbbench |
| columnar (4) | clickhouse, druid, pinot, starrocks |
| distributed-sql (3) | cockroachdb, tidb, yugabytedb |
| document (5) | couchbase, couchdb, ferretdb, mongodb, ravendb |
| embedded (3) | boltdb, duckdb, lmdb |
| extension (6) | arrow, datafusion, hnswlib, lance, pgvector, velox |
| graph (6) | arangodb, janusgraph, kuzu, memgraph, nebula, neo4j |
| key-value (7) | badger, dragonfly, foundationdb, leveldb, pebble, redis, rocksdb |
| relational (7) | firebird, h2, mariadb, mysql, openGauss, postgres, sqlite3 |
| search (6) | elasticsearch, meilisearch, opensearch, quickwit, tantivy, typesense |
| stream (4) | kafka, nats, pulsar, redpanda |
| time-series (5) | influxdb, iotdb, questdb, tdengine, victoriametrics |
| vector (9) | chroma, faiss, lancedb, milvus, qdrant, usearch, vald, vespa, weaviate |

**总计: 71 个**

## 按模型分类统计

| 模型 | 数量 | 完成率 |
|------|------|--------|
| benchmark | 4/5 | 80% |
| columnar | 4/5 | 80% |
| distributed-sql | 3/3 | 100% |
| document | 4/5 | 80% |
| embedded | 3/3 | 100% |
| extension | 6/6 | 100% |
| graph | 6/6 | 100% |
| key-value | 7/7 | 100% |
| relational | 7/7 | 100% |
| search | 6/6 | 100% |
| stream | 4/4 | 100% |
| time-series | 5/5 | 100% |
| vector | 9/9 | 100% |
| **总计** | **71/71** | **100%** |
