# Reference 目录模型化重构 设计

> 日期：2026-08-28
> 状态：实现完成（71/71 子模块全部克隆）
> 目标：重构 `reference/` 目录为按数据模型分类的层次结构

## 一、目录布局

```
reference/
├── relational/         # 关系型数据库（postgres, mysql, sqlite3 等）
├── distributed-sql/    # 分布式 SQL（cockroachdb, tidb, yugabytedb）
├── key-value/          # KV 存储（redis, rocksdb, leveldb 等）
├── document/           # 文档数据库（mongodb, couchdb 等）
├── columnar/           # 列式存储（clickhouse, druid 等）
├── time-series/        # 时序数据库（influxdb, iotdb 等）
├── search/             # 搜索引擎（elasticsearch 等）
├── vector/             # 向量数据库（faiss, milvus, chroma 等）
├── graph/              # 图数据库（neo4j, kuzu 等）
├── stream/             # 流处理（kafka, pulsar 等）
├── embedded/           # 嵌入式存储（duckdb, lmdb 等）
├── extension/          # 数据库扩展（arrow, pgvector 等）
├── benchmark/          # 基准测试工具（ann-benchmarks, tpch 等）
├── catalog.yml         # 子模块元数据清单（74 项）
├── fetch-manifest.txt  # 分阶段克隆指引
└── README.md           # 顶层说明
```

## 二、迁移策略

1. **路径迁移**：将原 `reference/open-source/<project>/` 改为
   `reference/<model>/<project>/`
2. **元数据规范**：每个子模块在 `catalog.yml` 中记录：
   - url：仓库地址
   - branch：默认分支
   - tag：版本标签
   - description：用途描述
3. **分阶段克隆**：
   - 第一阶段：核心必备（postgres, redis, faiss 等）
   - 第二阶段：特色与生态（milvus, elasticsearch 等）

## 三、执行记录

- 2026-08-27：完成目录结构、元数据规范、审核表、子模块迁移
- 2026-08-28：71/71 子模块全部克隆完成
- 部分仓库因 GitHub 闭源（couchbase）或改名（db-bench）已替换为等价替代