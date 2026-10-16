# DuckDB Wiki（嵌入式列存 OLAP 数据库）

> 参考版本：DuckDB 仓库当前主线分支（v1.1.x 系列；2026 年仍在快速迭代，约每月发版）。本 wiki 以 v1.1+ LTS 行为为基准。

## 一、项目定位

DuckDB 是由荷兰 Centric 实验室（后独立为 DuckDB Labs）开源的**进程内嵌式（in-process）分析型 SQL 数据库**，定位为「数据科学的 SQLite」。它把列存向量化执行、Morsel-driven 流水线、ACID 事务、零配置单文件部署塞进了一个 C++ 静态库，被 Python、R、Java、Node.js、Rust、Go 等生态广泛嵌入。

- **痛点**：
  - 大数据 OLAP 引擎（ClickHouse、Trino、Spark）部署运维重、成本高；
  - Python pandas 在内存放不下的大数据集无法处理；
  - 嵌入式场景需要进程内分析能力（BI、Notebook、CLI 工具）；
  - OLTP 数据库（SQLite、Postgres）做分析慢、不支持向量化；
- **典型用户**：数据科学（取代 pandas + SQLite 组合）、BI 工具（Hex、Preset、Metabase 内嵌）、数据工程（dbt-duckdb、dlt 落盘到 DuckDB）、CLI 工具（harlequin、duckdb-cli）、科研与教学。

## 二、发展历史与社区

- **起源**：2018 年由 Mark Raasveldt 与 Hannes Mühleisen（Centric 实验室）启动，作为其 CWI Amsterdam 学术研究项目；2019 年开源 MIT License；
- **里程碑**：
  - 2019-06：v0.1 公开；
  - 2020-06：v0.2 引入向量化执行；
  - 2021-2022：0.3.x 系列成熟，Python 生态爆发；
  - 2023-06：v0.10 与 v1.0（稳定 API 里程碑），引入 Lakehouse/Glue/Snowflake 直接读取；
  - 2024-2025：v1.1.x LTS 系列、引入 MERGE INTO、Iceberg/Hive 原生支持、Secret 管理；
  - 2026 年主线持续推进；
- **维护状态**：GitHub 每月发版，社区极活跃，Discord 8000+ 用户；
- **社区**：Discord、MotherDuck 论坛、Quarterly DuckDB newsletter、学术圈引用率高。

## 三、许可与商业限制

- **许可证**：**MIT License**（`LICENSE` 在仓库根）；
  - 极宽松、几乎无限制；
  - 商业使用、修改、分发、闭源衍生均可；
- **商业版本**：
  - **DuckDB Labs** 提供商业支持与咨询服务；
  - **MotherDuck**（创始人之一 Hannes Mühleisen 创立）提供托管的 Serverless DuckDB 服务，但 DuckDB 引擎本身保持完全开源；
- **Snowflake 关联**：Snowflake 与 DuckDB 团队有部分合作，但代码独立。

## 四、业界主要使用方与案例

- **数据科学生态**：Python `duckdb` / R `duckdb` / Node `duckdb` / Rust `duckdb-rs`；
- **BI 工具**：Hex、Metabase Embedded、Preset、Tableau（部分）等均集成 DuckDB；
- **数据工程**：dbt-duckdb（dbt 的本地 target）、dlt、Meltano、Singer；
- **Notebook 环境**：Jupyter + `%sql` magic、Observable Plot、Observablehq、Marimo（原 DuckDB 团队 fork 出 Marimo）；
- **大厂**：Meta 内部数据探索、Databricks（部分子工具）、MotherDuck 用户群；
- **教学**：CMU 15-721、CMU 11-747 等课程的向量化执行参考实现。

## 五、一句话对比定位

> DuckDB 是「**进程内嵌式的列存向量化 OLAP 引擎**」；与 **SQLite（行存 OLTP）**、**ClickHouse（分布式列存）**、**Trino/Presto（分布式 SQL on everything）**、**Polars（Rust 内嵌 DataFrame）** 形成「**单进程 OLAP**」象限。DuckDB 强调「单文件、零配置、任意规模单机分析」。

## 六、整体架构

DuckDB 采用经典的「**单进程、列存、向量化执行、ACID 事务**」架构，所有组件在同一个进程内通过 C++ 类协作。

### 6.1 总体分层

```
┌──────────────────────────────────────────────────────────────────────────┐
│                       API 层 (src/include/duckdb.hpp)                     │
│   - duckdb::Database     (数据库入口)                                    │
│   - duckdb::Connection   (会话入口, 内部持有 client context)              │
│   - duckdb::QueryResult  / duckdb::MaterializedQueryResult                │
│   - C API (src/main/capi)                                                │
└────────────────────────┬─────────────────────────────────────────────────┘
                         │
┌────────────────────────▼─────────────────────────────────────────────────┐
│              Parser & Binder (src/parser + src/planner/binder)            │
│   - Postgres 风格 SQL Parser (libpg_query 移植)                          │
│   - Binder 绑定 Catalog、类型推导、semantic 校验                          │
└────────────────────────┬─────────────────────────────────────────────────┘
                         │
┌────────────────────────▼─────────────────────────────────────────────────┐
│           Optimizer (src/optimizer, src/planner/operator)                 │
│   - LogicalOperator 树 → Optimizer pass 链 (列裁剪、谓词下推、Join Reorder)│
│   - 30+ pass: filter_pushdown, statistics_propagation, top_n, ...        │
│   - Cost-based optimizer: cardinality estimation + join ordering          │
└────────────────────────┬─────────────────────────────────────────────────┘
                         │
┌────────────────────────▼─────────────────────────────────────────────────┐
│       Physical Plan / Executor (src/execution, src/main/query_plan)       │
│   - PhysicalOperator 树：ChunkScan → Filter → Projection → Aggregate     │
│   - Pipeline (src/execution/pipeline)                                     │
│   - Morsel-driven 调度 (src/parallel/pipeline_executor.cpp)               │
│   - OperatorState + LocalState + GlobalState (Volcano 风格)              │
│   - 向量化 (DataChunk, src/common/types/data_chunk.cpp)                   │
└────────────────────────┬─────────────────────────────────────────────────┘
                         │
┌────────────────────────▼─────────────────────────────────────────────────┐
│              Storage (src/storage)                                       │
│   - StorageManager (src/storage/storage_manager.cpp)                     │
│   - BufferManager (src/storage/buffer_manager.cpp) - 自管 page cache     │
│   - WAL (src/storage/wal_replay.cpp, wal_writer.cpp) - 写前日志          │
│   - Catalog (src/storage/catalog) - 系统表管理                            │
└────────────────────────┬─────────────────────────────────────────────────┘
                         │
┌────────────────────────▼─────────────────────────────────────────────────┐
│          Table Access / Index (src/catalog, src/storage/table, index)    │
│   - DataTable / LocalStorage (src/storage/table)                          │
│   - ART (Adaptive Radix Tree, src/storage/index/art/*) - 主索引          │
│   - Block-Based Row Groups (列存主存储)                                   │
│   - Optimistic MVCC (src/storage/checkpoint_writer.cpp)                  │
└────────────────────────┬─────────────────────────────────────────────────┘
                         │
┌────────────────────────▼─────────────────────────────────────────────────┐
│         Common Types / Vector / FS (src/common)                          │
│   - Vector / DataChunk / ConstantVector (src/common/types/vector.cpp)    │
│   - Segment Tree / String Heap / Decimal / Interval                       │
│   - FileSystem 抽象 (src/common/file_system.cpp) - 支持 S3/HTTP/LOCAL    │
│   - Compression (src/common/compressor) - RLE / Dict / BitPacking        │
└──────────────────────────────────────────────────────────────────────────┘
```

源码入口：
- 主头文件：`src/include/duckdb.hpp`
- Database：`src/main/database.hpp` `src/main/database.cpp`
- Connection：`src/include/duckdb/main/connection.hpp`
- 存储入口：`src/storage/storage_manager.cpp`
- 向量化核心：`src/common/types/vector.cpp`

### 6.2 进程模型

DuckDB 是**嵌入式库**：

- **应用线程**：调用 `Connection::Query`；
- **可选**：自定义并行度（SetThreads/SetMaxThreads），DuckDB 默认按 Morsel 抢占调度，自动并行；
- **后台线程**：WAL flusher、checkpoint worker、optimizer worker（必要时）；
- **锁**：Catalog 内部行级锁 + Optimistic MVCC 冲突检测；每个事务独立 write lock。

### 6.3 一条 SELECT 查询的端到端路径（以 `SELECT count(*) FROM read_parquet('a.parquet') WHERE x > 10` 为例）

1. **应用调用**：`Connection::Query(sql)`；
2. **Parser**：`src/parser/transformer` 用 Postgres 风格 parser 把文本转为 `SelectStatement` AST；
3. **Binder**：`src/planner/binder` 把 AST 转为 `BoundQuery` 节点（绑定 Catalog 列元信息、类型推导）；
4. **Optimizer**：依次跑 30+ 优化 pass（src/optimizer/*.cpp），输出 `LogicalOperator`；
5. **Plan Generator**：`src/execution/physical_plan_generator.cpp` 转为 `PhysicalOperator`；
6. **Pipeline Builder**：`src/execution/pipeline_builder.cpp` 把 PhysicalOp 拆分为 Pipeline（数据驱动执行段）；
7. **Executor**：`src/execution/executor.cpp` + `src/parallel/pipeline_executor.cpp` Morsel 调度；
8. **Operator 执行**：每 operator 通过 `DataChunk` 拉取 2048 行（默认 batch_size）执行向量化 filter/scan；
9. **返回**：`MaterializedQueryResult` 输出 `DataChunk` 流，client 转 Python 物理类型。

### 6.4 一条 INSERT 命令的端到端路径

1. **入口**：`Connection::Query("INSERT INTO t ...")`；
2. **优化**：`physical_insert.cpp` 生成 `PhysicalInsert`；
3. **BeginTx**：`src/main/client_context.cpp:ClientContext::BeginTransaction`；
4. **Append RowGroup**：`data_table.cpp:DataTable::Append` 写入 LocalStorage 的 ColumnDataCollection；
5. **WAL**：`wal_writer.cpp` 追加日志；
6. **Commit**：`LocalStorage::Commit()` 把本地 state 提交到 `data_table_`；可选 `CHECKPOINT` 时机；
7. **返回 affected_rows**。

## 七、存储引擎（嵌入式列存专项）

DuckDB 存储引擎有别于传统 LSM 和 B+tree，是为分析负载设计的**Hybrid Row-Group + Columnar** 格式。

### 7.1 Row Group + Column Data

```
┌─────────────────────────────────────────────────────────────┐
│                    DataTable (src/storage/table)              │
│   - 一个表一个 DataTable                                     │
│   - 包含若干 RowGroup                                       │
└────────────────────────┬────────────────────────────────────┘
                         │
┌────────────────────────▼────────────────────────────────────┐
│         RowGroup (src/storage/table/row_group.cpp)            │
│   - 默认 122880 行 / group (DUCKDB_ROW_GROUP_SIZE)            │
│   - 每列连续存储                                                │
└────────────────────────┬────────────────────────────────────┘
                         │
┌────────────────────────▼────────────────────────────────────┐
│        ColumnData (src/storage/table/column_data.cpp)        │
│   - 每列一个 ColumnData                                       │
│   - 内部栈式：                                                │
│     MetaDataPointer → ColumnSegment (Persistent / Transient)│
└────────────────────────┬────────────────────────────────────┘
                         │
┌────────────────────────▼────────────────────────────────────┐
│       ColumnSegment + SegmentNode                            │
│   - 物理页/buffer                                            │
│   - Compression: RLE / Dict / Bitpacking / Falcon / Patas   │
│   - 统计: min/max/null_count (zone-map)                      │
│   - Bloom Filter (row-id 可选)                               │
└─────────────────────────────────────────────────────────────┘
```

特点：
- **向量化友好**：一次扫描 122880 行，命中 SIMD + 压缩友好；
- **Zone Map**：每段的 min/max/null_count 用于谓词下推；
- **Hybrid**：单表可以是 Row Group 主存 + 动态新增的 TransientSegment。

### 7.2 WAL 与崩溃恢复

```cpp
// src/storage/wal_replay.cpp
class WALReplay {
  void Replay(storage::WriteAheadLog &wal, ClientContext &ctx);
};
```

- **WAL 文件**：`{db_dir}/{db_name}.wal`；
- **触发 checkpoint**：超过阈值（默认 16MB WAL 或 50MB）或显式调用 `CHECKPOINT`；
- **加载时**：`Database::Load` 检测 `wal` 文件 → replay → 进入正常态。

### 7.3 Buffer Manager

`src/storage/buffer_manager.cpp`：

```cpp
class BufferManager {
  // 自管 page cache (不依赖 OS mmap)
  unique_ptr<EvictionQueue> eviction_queue_;
  unordered_map<block_id_t, BufferHandle> buffer_pool_;
  // Clock-Sweep 置换算法
  void EvictBlocks();
};
```

- 页大小默认 256KB（一大块以减少 IO）；
- 置换策略 Clock-Sweep（轻量 LRU 近似）；
- 支持临时块（`temp_block_id_t`）不刷盘。

### 7.4 Catalog 系统表

`src/storage/catalog`：

```cpp
// src/include/duckdb/catalog/catalog.hpp
class Catalog {
  unordered_map<string, CatalogEntry *> tables;
  CatalogEntry *CreateTable(...);
};
```

- 三层：`CatalogSet` → `EntryLookup` → `CatalogEntry`；
- 系统表：`pg_catalog.*`（duckdb 兼容子集）、`information_schema.*`；
- 变更通过 `AlterInfo`/`CreateInfo` 序列发往 WAL。

## 八、索引机制（嵌入式专项）

### 8.1 索引类型与实现位置

| 索引类型 | 实现位置 | 适用场景 |
|----------|---------|----------|
| **ART** (Adaptive Radix Tree) | `src/storage/index/art/` | 主键索引、唯一索引 |
| **Block-Based RowId 索引** | `src/storage/index/row_id_index.cpp` | 内部物理 row identifier |
| **HNSW** | `src/execution/index/art_operations.cpp`（v1.1+） | 向量 KNN |

### 8.2 ART 详解（Adaptive Radix Tree）

ART 是 DuckDB 自主实现的乐观并发 radix tree：

```
        Node (4-256 字节，动态扩张)
         ├─ Node4      (≤4 子节)
         ├─ Node16     (5-16)
         ├─ Node48     (17-48)
         ├─ Node256    (49-256)
         └─ Leaf       (单 key)
```

- **自适应节点**：节点大小随子节点数 4→16→48→256 渐次扩张；
- **Lazy Expand**：节点升级通过 `Node::ReplaceChild` 在 split 时触发；
- **关键路径**：`src/storage/index/art/art.cpp` `art_key.cpp` `leaf.cpp` `node.cpp`；
- **事务性**：每个 ART 操作（insert/erase）记录 WalOp 到 WAL；
- **MVCC 兼容**：乐观锁版本号 + conflict detection。

### 8.3 mmap / 文件 I/O 策略

DuckDB **不直接用 OS mmap**（与 LMDB 截然不同）：

- **自管 Buffer Pool**：`src/storage/buffer_manager.cpp`；
- **页面大小**：默认 256KB；
- **预读**：通过 `FileSystem::Read` 多块顺序读；
- **远程存储**：`src/common/file_system.cpp` 通过 HTTPRange requests 直接读 S3/HTTP，绕过本地缓存，按需缓存到 BufferManager；
- **优势**：
  - 显式页替换（Clock-Sweep）；
  - 与 ACID / WAL 兼容（mmap 与 WAL 难以组合）；
  - 临时块不刷盘；
- **代价**：需要双倍内存（page cache + user cache）。

### 8.4 进程内嵌 vs 服务器

| 维度 | DuckDB（嵌入式） | 传统 C/S 数据库 |
|------|-------------------|-----------------|
| 启动 | 进程内无需 socket | TCP/Unix socket |
| 零网络 | 完全零开销 | 网络 RTT + 协议解析 |
| 多线程并行 | 库内部 Morsel | 服务器 worker 线程池 |
| 跨进程 | 不支持（只读 shared cache 模式 preview） | 支持 |

### 8.5 多线程 / 多进程安全

- **多线程**：安全，所有 Pipeline 通过 `parallel::PipelineExecutor` Morsel 调度，多核自动并行；
- **多进程**：
  - 写仅允许单进程持有；
  - 读通过 **MVCC snapshot** 让多进程共享；
  - Linux 通过 `flock` 检测写独占；
- **WAL checkpoint**：`CheckpointWriter` 串行写，事务提交时阻塞。

### 8.6 页面缓存与刷盘

- **OS Page Cache**：当 DuckDB 写入 `.wal` 时，依旧走 OS 缓存；
- **fsync 时机**：每次 Commit 通过 `FileSystem::Sync` 强制刷盘；
- **Checkpoint**：把内存 state 落到 `.db` 文件，按 page 处理。

### 8.7 崩溃恢复

- **写前日志 + Optimistic MVCC**：每个事务 Commit 前把变更写成 WAL 记录；崩溃后 replay 恢复未提交的事务；
- **影子分页 + COW**：checkpoint 期间使用旧文件，到新文件完成再原子 rename；
- **不依赖 OS mmap 的原因**：避免 mmap 数据不一致状态下做 fsync 的复杂性。

## 九、查询/计算/接口层（嵌入式专项）

### 9.1 嵌入式 SQL 引擎（向量化）

- **Parser**：`libpg_query`（Postgres parser C 库移植，`third_party/libpg_query/`），完整的 Postgres 语法兼容；
- **Binder**：`src/planner/binder/`（列绑定、类型推导、子查询展开）；
- **Optimizer**：`src/optimizer/`（30+ pass）；
- **Plan**：`src/execution/physical_plan_generator.cpp`；
- **Executor**：`src/execution/executor.cpp` + `src/parallel/parallel_executor.cpp`（向量化）；
- **批大小**：默认 2048 行 / Vector（`STANDARD_VECTOR_SIZE`）；
- **代码生成**：部分表达式走 JIT（`src/execution/expression_executor.cpp` 中 ExpressionExecutor::Execute 可走 JIT）或解释执行。

### 9.2 存储格式与自描述

- **自描述 `.db` 文件**：内置 schema（元信息存在 `WAL` 与 `.db` 表的 metadata 块），开箱即用，无需 catalog server；
- **Forward-compatible schema**：
  - 新列添加（`ALTER TABLE t ADD COLUMN`）保留默认值/类型；
  - 旧版本读取新文件：未知列忽略或类型回退；
  - 实现：`CatalogSet::AlterEntry` in `src/catalog/catalog_set.cpp`；
- **多数据库**：`ATTACH 'a.db'` 把另一个 DuckDB 文件以 catalog 形式挂载（v0.10+）。

### 9.3 并发控制

- **Optimistic MVCC**：
  - 读 snapshot：每个查询基于一个固定 snapshot id；
  - 写时检测冲突：Commit 时检查 row version 是否被并发更新；
  - 冲突回滚：UBI（Uncommitted Buffer Index）检测；
- **Snapshot Isolation**：
  - 重复读一致；
  - write-write conflict 必有 aborted 但无 phantom 锁；
  - 无 SI 后立即见（次 read 可见）；
- **多版本**：`src/storage/buffer_manager.cpp` 持有多个 version，cleanup 通过 `Checkpoint` 进行 GC。

### 9.4 小数据 / 单文件场景优化

- **单文件部署**：整个数据库 = 1 个文件 + 1 个 WAL；`ATTACH` 多文件支持更大部署；
- **零配置**：无需 initdb、pg_hba、postgresql.conf；第一次 `Database(db_path)` 自动创建；
- **嵌入式 BI**：
  - Streamlit / Dash 内嵌做 BI；
  - BI 工具（Hex、Metabase）集成 DuckDB 实现「数据库即文件」模式；
- **嵌入式科学计算**：
  - Python：`import duckdb; duckdb.sql(...)` 100% 内嵌；
  - R：`library(duckdb); duckdb::duckdb()` 共享 data.frame；
  - 直接读 Parquet/Arrow：`SELECT * FROM 'data.parquet'`；
- **复制小数据集**：`COPY ... FROM ...`，带 CSV/Parquet/Arrow 自动识别；
- **Lakehouse 集成**：v0.10+ 原生读 Iceberg、Delta Lake、Glue catalog、HTTP S3。

### 9.5 API / SDK

- **C++ API**：`Connection::Query` 返回 `MaterializedQueryResult`；
- **C API**：`src/main/capi/` 提供 `duckdb_open`、`duckdb_query` 等 100+ C 函数；
- **Python API**：`duckdb` pip 包；
- **Node.js API**：`duckdb` npm 包；
- **R API**：CRAN 上的 `duckdb`；
- **Java API**：`com.duckdb:duckdb_jdbc`；
- **Rust API**：`duckdb-rs`、`duckdb-ext-rs`（扩展开发）。

## 十、事务与一致性

- **隔离级别**：默认 **Snapshot Isolation**；
- **ACID**：完整支持；
- **MVCC**：基于 row_id + version 的 Optimistic MVCC；
- **Conflict Detection**：
  - 写时记录 inserted/deleted tuple 到 wal；
  - commit 验证：被并发事务覆盖则 abort；
- **WAL**：
  - 所有变更先写 WAL（`wal_writer.cpp`），后 commit；
  - `wal_replay.cpp` 在启动时回放未 checkpoint 部分；
- **Checkpoint**：`CheckpointWriter::WriteCheckpoint` 把内存 state 序列化到 `.db`。

## 十一、复制与高可用

DuckDB 是**嵌入式库**，无内置多副本机制。高可用：

- **静态文件**模式：直接拷贝 `.db` 文件即可（即使运行中也支持 checkpointed 副本读取）；
- **母亲项目 MotherDuck**：商业服务提供 cloud-hosted DuckDB（CBO 多节点部署）；
- **客户端层做复制**：Fivetran/Airbyte 等集成供应商可基于 DuckDB 实现自定义同步。

## 十二、扩展性

- **C++ Extension API**：`src/include/duckdb/main/extension.hpp`；
- **社区扩展**：`httpfs` (S3/HTTP)、`icu`（排序规则）、`spatial`（地理空间）、`json`（JSON 操作）、`parquet`、`arrow`、`iceberg`、`delta`；
- **Hot Reload**：通过 `INSTALL/LOAD` SQL 命令加载 extension；
- **自定义函数**：`Connection::CreateScalarFunction`（C++ API）注册任意函数。

## 十三、性能与基准

- **官方 benchmark**：参见 [DuckDB 博客 Benchmarks](https://duckdb.org/benchmarks.html) 与 [H2O.ai DB-benchmark](https://duckdb.org/2024/10/04/h2oai-db-benchmark-duckdb.html)；
- **典型数字**：
  - **TPC-H SF=10**：4 核机器约 6s；
  - **H2OAI groupby**：1B 行 4 核约 30s（vs Dask 180s，pandas 121s）；
  - **ClickBench 1B**（近似，构造数据集）：DuckDB 在 1 核 10x 同构对手；
  - **NYC taxi trip**：DuckDB 1.45x pandas + 并行优于 DuckDB；
- **向量化优势**：SSB × 4.5x vs SQLite、ClickBench 1B 接近 ClickHouse。

## 十四、运维与可观测性

- **诊断**：`PRAGMA enable_optimizer`、`PRAGMA explain_output = 'all'`、`EXPLAIN ANALYZE`；
- **Profiling**：`SET enable_profiling = 'query_tree'` 输出 JSON profile；
- **日志**：`SET log_query_path = '...'`；
- **Checkpoint 控制**：`PRAGMA checkpoint_threshold`、`CHECKPOINT` 显式；
- **WAL 监控**：`SELECT * FROM pragma_wal_size()`。

## 十五、生态与集成

- **客户端 SDK**：C、C++、Python、R、Java、Node.js、Go、Rust、C#、Julia、Lua、Elixir；
- **ORM**：Drizzle、Django（`django-duckdb`）、SQLAlchemy（通过 SQLAlchemy-DuckDB）；
- **Notebook**：Jupyter、Marimo、Observable Plot、RStudio；
- **BI**：Hex、Preset、Metabase、Tableau Connector、Rill；
- **DataFrame 互通**：Arrow/Flight、R/Python DataFrame 直接查询；
- **存储格式**：Parquet（读 + 写）、CSV、JSON、Arrow、ORC（仅读）；
- **Lakehouse**：Iceberg、Delta Lake、Hive Metastore、Glue Catalog、Polars。

## 十六、核心论文与算法原理

- **向量化执行**：参考 Daniel Abadi 等 *"Materialization Strategies in a Column-Oriented DBMS"*（ICDE 2007）；
- **Morsel-driven Parallelism**：参考 Leis 等 *"Morsel-driven parallelism: a NUMA-aware query execution framework for the many-core age"*（SIGMOD 2014）；
- **ART 索引**：参考 Leis 等 *"The Adaptive Radix Tree: ARTful Indexing for Main-Memory Databases"*（ICDE 2013）；
- **Snapshot Isolation**：参考 Cahill 等 *"Serializable Snapshot Isolation"*（SIGMOD 2008）；
- **DuckDB 论文**：Raasveldt & Mühleisen *"DuckDB: an Embeddable SQL OLAP Database Management System"*（SIGMOD Demo 2019）。

## 十七、源码学习路径

必读清单（按从浅到深顺序）：

1. **入口**：`src/include/duckdb.hpp` 看主 API；
2. **Connection**：`src/main/connection.cpp` + `src/include/duckdb/main/connection.hpp`；
3. **Parser**：阅读 `src/parser/transformer.cpp` 顶部 300 行理解 SQL → AST；
4. **Binder / Optimizer**：跟踪 SQL 从 `Binder::Bind` 到 `Optimizer::Optimize`；
5. **执行**：`src/execution/physical_plan_generator.cpp` + `src/parallel/pipeline_executor.cpp`；
6. **存储**：`src/storage/storage_manager.cpp` + `src/storage/buffer_manager.cpp`；
7. **索引**：`src/storage/index/art/art.cpp`（ART 实现入口）；

源码规模：约 **100 万行 C++**（v1.1），主要语言占比：C++ 95%，其余 C、CMake。

## 十八、可移植性与部署

- **平台**：Linux（x86_64 / ARM64）、macOS（Intel / Apple Silicon）、Windows（x64 / ARM64）、WebAssembly（实验性）；
- **构建**：CMake，`cmake -DBUILD_DUCKDB_PYTHON=1` 等可选 flag；
- **静态/动态库**：默认静态（单二进制嵌入友好）；
- **容器**：`duckdb/duckdb` Docker 镜像提供 CLI；
- **云**：MotherDuck 提供 Serverless SaaS、自部署（OSS）通过 SSH 导入数据。

## 十九、变体与衍生项目

- **MotherDuck**：商业云平台，提供 serverless DuckDB；
- **Snowflake 合作**：Snowflake 公开宣布与 DuckDB 团队合作（进一步信息非公开）；
- **academic**：CMU Database Group、Database Lab Amsterdam 等学术项目；
- **其他**：DuckDB .NET、Exasol/DuckDB 连接器等社区项目。

## 二十、风险与陷阱

| 陷阱 | 描述 | 缓解 |
|------|------|------|
| **mmap 缺失** | DuckDB 不暴露 mmap 选项；大冷数据集首次扫描较慢 | 用 `PRAGMA enable_object_cache` + 自管 Buffer Pool；预热常用表 |
| **单写者限制** | 同一文件只能一个进程写 | 多进程读+ MotherDuck 协调；并发写通过 ATTACH 多文件 |
| **Checkpoint 抖动** | 大表 checkpoint 期间阻塞写 | 控制 `wal_autocheckpoint` 阈值；空闲时段 CHECKPOINT |
| **Python GIL** | 多线程 Python 客户端受 GIL 限制 | 用 `con.execute_thread()` / `con.fetch_df_chunk()`；社区项目 duckdb-python-multithreaded |
| **WAL 文件膨胀** | 长时间不 checkpoint 导致 `.wal` 巨大 | 阈值触发 + 手动 `CHECKPOINT` |
| **Vector 大小依赖** | 极小维度的极端列追加效率低 | 调整 `duckdb.row_group_size` |
| **字符串压缩** | 长字符串的 LZ4 压缩有时不划算 | 调 `SET threads`/`SET memory_limit` 强制落盘 |

## 二十一、与本项目的关联

`engineering/` 当前实现了**KV + 关系型 + 向量 + 多模态**存储；未实现 OLAP 列存引擎。如果未来需要做嵌入式 BI 或本地分析 sandbox，可参考 DuckDB：

- **学习借鉴**：
  - **向量化执行流水线**：可借鉴到 `engineering/src/db/sql` 中的 Volcano 改造；
  - **ART 索引**：作为 `engineering/src/index/art/` 的算法模板；
  - **Optimistic MVCC**：参考实现到 `engineering/include/db/mm_storage.h`；
- **直接集成**：可通过 C API 嵌入 `libduckdb` 完成独立 BI 产品；
- **对比方案**：`engineering/src/columnar/` 中 Parquet-only reader 距离 DuckDB 的全栈还差很远。

## 二十二、学习资源

- **官方文档**：[https://duckdb.org/docs](https://duckdb.org/docs/)（api、architecture、guides）；
- **论文**：Raasveldt & Mühleisen *"DuckDB: an Embeddable SQL OLAP Database Management System"*；
- **课程**：CMU 15-721（Lectures on Modern Databases）、Hasso Plattner Institute；
- **博客**：官方 [duckdb.org/news](https://duckdb.org/news) 季度通讯、Mark Raasveldt 个人博客；
- **Discord**：`https://discord.gg/vW7T4wrBEk` 8000+ 用户；
- **MotherDuck 博客**：[motherduck.com/blog](https://motherduck.com/blog) 实战案例；
- **GitHub**：仓库 [duckdb/duckdb](https://github.com/duckdb/duckdb) 默认分支 `main`；
- **YouTube**：DuckDB Office Hours 会议回放；
- **Reddit**：r/DuckDB、r/dataengineering。

## 二十三、参考资料

1. Raasveldt & Mühleisen. *DuckDB: an Embeddable SQL OLAP Database Management System*. SIGMOD 2019. <https://duckdb.org/pdf/DuckDB.pdf>
2. Leis et al. *The Adaptive Radix Tree: ARTful Indexing for Main-Memory Databases*. ICDE 2013. <https://db.in.tum.de/~leis/papers/ART.pdf>
3. Leis et al. *Morsel-driven parallelism: a NUMA-aware query execution framework for the many-core age*. SIGMOD 2014.
4. 官方文档: <https://duckdb.org/docs/>
5. DuckDB GitHub: <https://github.com/duckdb/duckdb>
6. MotherDuck Blog: <https://motherduck.com/blog>
7. DuckDB Architecture Overview: <https://duckdb.org/why_duckdb#architecture>
8. H2O.ai DB-benchmark with DuckDB: <https://duckdb.org/2024/10/04/h2oai-db-benchmark-duckdb.html>
9. ClickBench: <https://benchmark.clickhouse.com/>
10. CPM-1 (Computer Modern SQL 标准): <https://duckdb.org/2025/02/04/sniffles.html>
