# Apache DataFusion Wiki（Rust 原生 SQL 执行引擎）

> 参考版本：DataFusion 41.x / 42.x（2024-2025），基于 Apache Arrow 共享内存格式，Rust 1.75+。本 wiki 重点覆盖 DataFusion 作为嵌入式 SQL 引擎的集成模式（in-process library / Ballista 分布式 / Flight Server）以及与本项目（SQL 执行器）的关联。

## 一、项目定位

Apache DataFusion 是由 **Apache 软件基金会（ASF）** 维护的 **「Rust 原生 SQL 查询执行框架」**，以 **Apache Arrow 内存格式**为统一数据载体，提供：

- **SQL 解析器**（基于 SQLparser-rs）；
- **查询计划与优化器**（rules + cost-based）；
- **向量化执行引擎**（基于 Arrow `RecordBatch`）；
- **DataFrame API**（Python / Rust 双端）；
- **Ballista 分布式扩展**（基于 Kubernetes）；
- **Arrow Flight Server 集成**（`arrow-flight-sql`）。

口号： **"DataFusion is an in-memory query engine that uses Apache Arrow as the in-memory format. SQL and DataFrame APIs, with good performance on complex analytics workloads."**

- **核心痛点**：
  - **Python OLAP 性能差**：pandas / Polars 在 10 亿行规模勉强可用，**10 亿 + 行无解**；
  - **OLAP 引擎过于重量级**：ClickHouse、StarRocks、Trino 部署运维复杂，**对中小规模应用不友好**；
  - **Java/Scala OLAP 生态门槛高**：Spark / Flink 学习曲线陡；
  - **嵌入式 OLAP 与云原生不兼容**：DuckDB 适合单机，**分布式生态空白**；
  - **Python + SQL 互转频繁**：Polars 缺少 SQL 接口，pandas 缺少 SQL 接口；
  - **AI 基础设施的数据预处理需求**：ML 训练前常用 SQL 拼接特征，**临时 ETL 重量过大**；
- **典型用户**：
  - **数据科学家 / ML 工程师**：Jupyter Notebook 内 `df.sql("SELECT ...") → arrow.RecordBatch` 配合 PyTorch；
  - **Rust OLAP 引擎开发者**：将 DataFusion 作为查询层，业务层实现自定义 `TableProvider` / `OptimizerRule`；
  - **云原生数据平台**：基于 Ballista 部署分布式集群，作为数据湖的查询层；
  - **数据库开发者**：将 DataFusion 嵌入 OLTP（GreptimeDB、GlueSQL、PeekDB）；
  - **AI 数据预处理**：LanceDB、Polars、Cloudflare R2 数据湖查询层；
  - **企业内 ETL**：Delta Lake / Iceberg 上的轻量 SQL 层；
  - **嵌入式分析**：终端应用内置 DataFusion 跑 SQL（如 Cloudflare Workers Analytics Engine）；

## 二、发展历史与社区

- **起源**（2018）：
  - 由 **Andy Grove** 创建，最初作为个人项目 [datafusion-rs / arrow-rs 仓库]；
  - 2019 年合并入 Apache Arrow 主仓（apache/arrow-datafusion），作为 Arrow 的「SQL 同门项目」；
- **里程碑**：
  - 2019-08：6.x 初版，SQL parser + 基础执行；
  - 2020：10.x 稳定版，Substrait（跨引擎 IR）初版；
  - 2021-06：与 Apache Arrow 同步发布 14.x，进化到 `LogicalPlan` / `ExecutionPlan` 二阶段；
  - 2022-04：20.x，引入 DataFrame API、Substrait 集成、Python 绑定 `datafusion-python`；
  - 2023-01：进入 ASF 孵化器（Incubator）；
  - 2023-11：32.x，**TPC-H SF100 性能接近 ClickHouse 单机**；
  - 2024-04：进入 ASF 顶级项目（Top-Level Project）；
  - 2024-08：41.x，Ballista 分布式查询引擎 1.0、`EXCEPT` / `INTERSECT` 完善、`EXPLAIN ANALYZE` 增强；
  - 2024-12：42.x，Recursive CTEs、ClickBench 30/43 性能领先；
- **维护状态**：极活跃，季度大版本，每月小版本，月 commit 600+，**RUSTSEC 审计 0 高危**；
- **社区**：GitHub 6k+ star、ASF Mail List、`dev@datafusion.apache.org`、`user@datafusion.apache.org`、GitHub Discussions、年度 `Arrow + DataFusion + Velox + Parquet` 联合开发者峰会；
- **PMC 主席与 committer**：Andy Grove（创始，Ballista 创始人）、Andrew Lamb（InfluxData 维护，数据湖）、Razvan Lichiardopol（架构）、Djouallah Mammeri（GreptimeDB 适配）；

## 三、许可与商业限制

- **核心代码**：**Apache License 2.0**（Rust crate `datafusion` / `datafusion-cli` / `datafusion-proto`，Python crate `datafusion-python`，分布式 `ballista`）；
- **与商业产品的关系**：
  - **InfluxDB IOx**（InfluxData）：**完全基于 DataFusion + Arrow** 构建，发布企业版（商业添加 Auth / Quota / RBAC）；
  - **GreptimeDB**（国产）：基于 DataFusion 作为查询层；
  - **Manticore Software**（商业，旧名）：基于 DataFusion 提供商业支持；
  - **Voltron Data**（Wes McKinney 公司）：提供 DataFusion 商业 OSS 援助；
- **生态关系**：
  - **上游依赖**：`arrow-rs`（arrow 数组 + buffer）、`parquet-rs`（Parquet 读取）、`datafusion-proto`（Substrait）、`sqlparser-rs`（SQL 解析）；
  - **下游生态**：约 200+ 项目使用 DataFusion：
    - **数据库**：InfluxDB IOx、GreptimeDB、GlueSQL、PeekDB、RisingWave（部分）；
    - **DataFrame / AI**：LanceDB、Polars 部分组件、HuggingFace 数据集；
    - **云原生**：Cloudflare R2 Data Catalog、Databricks 旗下 Delta Lake 集成（部分）、AWS Athena 部分组件；

## 四、业界主要使用方与案例

- **大型用户**（公开）：
  - **InfluxData**：InfluxDB IOx（v3 时序数据库）核心；
  - **GreptimeDB**（国产）：时序数据库（杭州格睿）；Greptime Cloud 基于 DataFusion 提供 SQL；
  - **Cloudflare**：R2 Data Catalog（DataFusion + Ballista，云原生数据湖查询）；
  - **Databricks**：Delta Lake 的部分 Rust 客户端；
  - **Manticore Software**（商业）：提供 DataFusion 商业支持；
  - **Apollo GraphQL**（部分）；
  - **Snowflake**（部分 Rust 客户端）；
  - **Convex**（实时后端）；
  - 国内：**阿里云 MaxCompute 部分模块（实验）**、**字节火山引擎 DataSail 部分模块（实验）**、**京东云 VectorDB 部分模块（实验）**；
- **典型场景**：
  - **时序数据库查询层**：InfluxDB IOx、GreptimeDB 完全采用 DataFusion 作为 SQL 层；
  - **DataFrame 库**：Polars 部分算子、Apache Arrow DataFusion Python 绑定的 DataFrame API；
  - **嵌入式分析**：Cloudflare Workers Analytics Engine、Rust 应用嵌入 SQL 能力；
  - **数据湖查询**：Iceberg / Delta Lake / Hudi 通过 DataFusion + Substrait 查询；
  - **AI 训练 / RAG**：Jupyter 内 `datafusion-python` 嵌入，配合 HuggingFace Datasets 做 SQL 数据预处理；
  - **云原生**：Ballista + K8s 的分布式 SQL 引擎；
  - **RAG / 向量数据库辅助**：GreptimeDB、LanceDB 同时用 DataFusion 做 SQL + 向量 KNN；

## 五、一句话对比定位

> DataFusion 是 **「Rust 原生 + Arrow 共享内存 + SQL + DataFrame + 向量化执行 + Ballista 分布式 + Substrait IR + Python 绑定 + ASF 顶级项目 + 上下游生态繁荣」**；与 **Trino（Java 分布式 SQL）**、**Presto（Java 分布式 SQL）**、**ClickHouse（C++ 列存 OLAP）**、**DuckDB（嵌入式 C++ OLAP）**、**Polars（Rust DataFrame）**、**DuckDB（嵌入式 OLAP）**、**dbt（SQL 转换层）**、**Velox（C++ 向量化执行）**、**Spark SQL（Java 分布式）** 属于「SQL 执行引擎」生态。DataFusion 的差异化是 **「Rust 原生 + Arrow 标准 + 嵌入式 + 分布式（Ballista）+ 多语言 SQL/DataFrame + 上下游繁荣」**。

## 六、整体架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                          API 客户端层                                │
│  - Rust API: datafusion::prelude::* (SessionContext, DataFrame)    │
│  - Python API: datafusion-python (df.sql, df.create_dataframe)    │
│  - HTTP API: datafusion-server / datafusion-cli                    │
│  - Flight SQL: datafusion + arrow-flight-sql                       │
│  - Substrait Producer: datafusion-substrait                       │
│  - JDBC Bridge: 通过 arrow-flight-sql 桥接                          │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                          SQL 解析层                                  │
│  - SQLparser-rs: SELECT/JOIN/AGG/WINDOW/CTE/SubQuery 解析           │
│  - AstBuilder → LogicalPlan: 通过 SqlToRel 转换器                  │
│  - builtin_table_functions: io_options (ParquetFormat, CsvFormat) │
│  - 用户自定义 UDF / UDAF 通过 create_udf() 注册                     │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                          优化器层                                    │
│  LogicalPlan Rules:                                                 │
│  - TypeCoercion, SimplifyExpressions, ScalarSubqueryToJoin        │
│  - PredicatePushdown, FilterPushDownThroughJoin,                  │
│    AggregateReduceOutReferences                                    │
│  Common SubexpressionElimination (CSE)                              │
│  PhysicalPlan Rules:                                                │
│  - EnforceDistribution, EnforceSorting, OptimizeRepartition         │
│  - BroadcastJoinRepartition, HashJoinOrder                          │
│  Cost-based Optimizer:                                              │
│  - Statistics, Histograms, LimitPushDown                            │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                          执行引擎层                                  │
│  ExecutionPlan → 多 stream 上的 Operator pipeline:                   │
│  - DataSourceExec, ProjectionExec, FilterExec, AggregateExec        │
│  - HashJoinExec, SortMergeJoinExec, CrossJoinExec                  │
│  - CoalescePartitionsExec, RepartitionExec, SortExec               │
│  - WindowAggExec, LimitExec, EmptyExec, ExplainExec                 │
│  - UserDefined (TableProvider, ExecutionPlan)                       │
│  - Stream: Partitioning::RoundRobin / Hash / Range                  │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                          存储适配层                                  │
│  TableProvider:                                                     │
│  - ParquetTableProvider, CsvTableProvider, JsonTableProvider       │
│  - MemoryTableProvider, ListingTableProvider (data/delta/iceberg) │
│  - 用户自定义: KafkaTopicProvider, FlightProvider, LanceTableProvider│
│  Arrow Schema / RecordBatch: 统一记录类型                          │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                          底层基础设施                                │
│  - arrow-rs (Apache Arrow Rust 实现)                                │
│  - parquet-rs (Apache Parquet Rust 实现)                            │
│  - sqlparser-rs (SQL 解析)                                          │
│  - tokio / async-runtime (异步执行器调度)                            │
│  - object_store (多 S3 / GCS / Azure / HDFS / Local 抽象)           │
└─────────────────────────────────────────────────────────────────────┘
```

**关键设计原则**：

1. **基于 Arrow**：`RecordBatch` 是 DataFusion 所有 operator 的统一输入 / 输出，避免跨格式转换；
2. **流式 pipeline**：每个 operator 是 stream-based，支持背压（backpressure），适合大表；
3. **优化器两阶段**：先做 logic rewrite（不改物理），再进行 physical planning（分发 + 并行化）；
4. **Substrait IR**：可输出 Substrait 计划让其他引擎（Velox、ClickHouse）执行；
5. **cost-based + 自适应**：基于统计信息选择 join order / aggregation 策略；
6. **Rust 内存安全**：所有 operator 在 Rust `Result` 框架下传播错误，避免段错误；
7. **可嵌入式**：可作为 `library` 嵌入任意 Rust 应用，无需启动独立进程。

## 七、DataFusion 作为扩展的集成模式

DataFusion 在 **嵌入式 SQL / 分布式 SQL / Python DataFrame / Flight SQL** 四种模式下集成：

### 7.1 嵌入式库模式（in-process）

```rust
// Rust 嵌入式
use datafusion::prelude::*;

let ctx = SessionContext::new();
// 注册内存表
ctx.register_csv("sales", "sales.csv", CsvReadOptions::new()).await?;
// 执行 SQL
let df = ctx.sql("SELECT product_id, SUM(amount) FROM sales GROUP BY product_id").await?;
let batches = df.collect().await?;
// 转为 Arrow RecordBatch
for batch in batches {
    let table = arrow::record_batch_to_table(batch)?;
    // ... 序列化或网络传输
}
```

```python
# Python 嵌入式
from datafusion import SessionContext, col
import pyarrow as pa

ctx = SessionContext()
data = pa.table({"a": [1, 2, 3], "b": [4, 5, 6]})
ctx.from_arrow(data)

result = ctx.sql("SELECT a, a + b AS c FROM data").collect()
print(result.to_pandas())
```

### 7.2 服务端模式（Flight SQL / HTTP）

```rust
// Flight SQL Server
use arrow_flight_sql::FlightSqlService;
#[async_trait]
impl FlightSqlService for MyService { ... }

let server = FlightSqlServer::new(MyService);
server.serve("0.0.0.0:8815").await?;
```

```bash
# DataFusion CLI
datafusion-cli -f queries.sql

# Arrow Flight SQL JDBC driver 客户端
jdbc:arrow-flight-sql://localhost:8815/
```

### 7.3 分布式模式（Ballista）

```rust
// Ballista 是 DataFusion 的分布式扩展（Kubernetes 调度）
// Scheduler + Executors，按 Stage 划分 Stage
let scheduler_url = "ballista://scheduler:50051";
// 提交 SQL 后 scheduler 会按 stage 调度到 executor
let df = ctx.sql("SELECT ...").await?;
```

### 7.4 与本项目（多模态数据库）的集成

| 集成方式 | 路径 | 说明 |
|---------|------|------|
| **Storage 内 TableProvider** | `engineering/src/db/executor/df_table_provider.rs` | 实现 `TableProvider` 将 `engineering/src/db/` 中的 `relation_t` 暴露为 SQL 表 |
| **UDF 集成** | `engineering/src/db/executor/df_udfs.rs` | 将 `vector_l2_distance`、`ts_now` 注册到 DataFusion |
| **Flight SQL Server** | `engineering/apps/df_server/main.rs` | 启动 `arrow-flight-sql` 服务，暴露 `engineering/src/db/` |
| **Python DataFrame** | `engineering/sdk/python/` | 通过 `datafusion-python` 让用户用 Python DataFrame 访问 |

## 八、索引机制（执行引擎的 "索引" 概念）

DataFusion **不提供传统数据库索引**（B+Tree / Hash / HNSW）—— 它是查询引擎而非存储引擎。但 DataFusion 提供 **「统计信息 + 优化器索引」** 等价概念：

### 8.1 统计信息（Statistics）

```rust
// TableProvider 可返回 Statistics，optimizer 据此选 Plan
let stats = ScanStatistics {
    num_rows: Precision::Exact(1_000_000),
    total_byte_size: Precision::Exact(64_000_000),
    column_statistics: vec![
        ColumnStatistics {
            min_value: Precision::I32(0),
            max_value: Precision::I32(1_000_000),
            null_count: Precision::I32(0),
            distinct_count: Precision::Absent,
        },
        ...
    ],
};
```

### 8.2 文件级别索引

- **Parquet column index / offset index**（Parquet 4.0+）：DataFusion 会读取 Parquet `RowGroupIndex` 跳过无关 row group；
- **Parquet bloom filter**：通过 `bloom_filter_excluded_columns` 配置自动应用；
- **Parquet page index**：column chunk 内的 page index，DataFusion 在 predicate pushdown 时跳过无关 page；
- **Iceberg hidden partition / z-order / clustering**：DataFusion 通过 `iceberg-rs` 直接利用；
- **Delta Lake data skipping**：min/max stats + 文件级 stats 跳过；
- **Listing Table 的 Partitioning**：基于 date / region 字段自动 partition pruning。

### 8.3 内存层 Plan 优化

- **Predicate pushdown**：WHERE 下推到 Scan；
- **Projection pushdown**：SELECT column 下推到 Scan；
- **Limit pushdown**：LIMIT 下推到 Scan（适配 streaming source）；
- **Dynamic filter pushdown**：HashJoin build 完后过滤 probe；
- **Common subexpression elimination**：相同 subquery 复用；
- **Equivalence propagation**：常量折叠 + 一致性传播；
- **Information flow / norm**：基于 stats 评估 join 代价。

### 8.4 Ballista 分布式索引

- **分布式调度**：`Scheduler` 按 stage 划分 → 多 `Executor` 并行；
- **Stage 间数据 shuffle**：`RepartitionExec` 决定 Hash / RoundRobin / Range；
- **Shuffle IO**：本地磁盘 shuffle（DuckDB 风格的 `DiskShuffle`）+ 远程网络 shuffle（基于 Ballista proto）。

## 九、查询 / 计算 / 接口层

### 9.1 SQL 解析与支持范围

DataFusion 的 SQL parser 基于 **sqlparser-rs**，覆盖：

| 类别 | 支持 |
|------|------|
| **DML** | SELECT（含 WHERE / ORDER BY / GROUP BY / HAVING / LIMIT / OFFSET / DISTINCT） |
| **DML 子句** | UNION / UNION ALL / INTERSECT / EXCEPT（41+） |
| **JOIN** | INNER / LEFT / RIGHT / FULL / SEMI / ANTI / CROSS |
| **聚合** | GROUP BY（CUBE / ROLLUP 33+）+ 窗口函数（ROW_NUMBER / RANK / LAG / LEAD / NTILE） |
| **CTE** | 简单 CTE + Recursive CTE（42+） |
| **子查询** | 标量子查询 / IN / EXISTS / Derived Table |
| **函数** | Builtin 100+（数学 / 字符串 / 时间 / 数组 / map / struct）+ 用户 UDF |
| **Lambda** | `array_filter(...)`、`array_transform(...)`（42+） |

### 9.2 DataFrame API

```python
# Python DataFrame API（与 PySpark 类似）
import datafusion
from datafusion import col

df = ctx.from_arrow(arrow_table)
result = df.filter(col("age") > 18) \
    .select(col("name"), col("age")) \
    .sort(col("age").desc()) \
    .limit(10) \
    .collect()
```

### 9.3 自定义 UDF / UDAF / UDWF

```rust
// 自定义 UDF
let sign = udf::scalar_fn(|x: Int64Array| -> Int64Array {
    x.into_iter().map(|v| v.map(|v| if v > 0 { 1 } else if v < 0 { -1 } else { 0 })).collect()
});
ctx.register_udf(sign);

// 自定义 UDAF（聚合）
struct MySum;
impl Accumulator for MySum { ... }
ctx.register_udaf(MySum::new());

// 自定义 UDWF（窗口函数）
struct PercentRank;
ctx.register_udwf(PercentRank);
```

### 9.4 自定义 TableProvider

```rust
// 自定义 Storage 适配（关键集成点）
struct MyTableProvider;

#[async_trait]
impl TableProvider for MyTableProvider {
    fn schema(&self) -> SchemaRef { ... }
    async fn scan(&self, _state: &SessionState, _projection: Option<&Vec<usize>>, ...)
        -> Result<Arc<dyn ExecutionPlan>> {
        Ok(Arc::new(MyExec::new()))
    }
}
```

### 9.5 Substrait 互转

```rust
// DataFusion → Substrait
let proto_bytes = sub_strait_plan.try_into_substrait_proto()?;
let json = sub_strait_plan.to_substrait_json()?;

// Substrait → DataFusion（执行其他引擎的计划）
let plan = SubstraitPlan::from_substrait_proto(bytes)?;
df = ctx.from_substrait(plan)?;
```

### 9.6 向量化执行（核心性能来源）

每个 operator 输出 `RecordBatch`（1024-8192 行/批），下游 operator 一次处理一列：

- **FilterExec**：`filter_predicate` 对每一列向量化应用；
- **ProjectionExec**：`Selector::project_columns` 直接重排 buffer；
- **HashJoinExec**：两表 hash + probe 单遍 join；
- **AggregateExec**：`HashMap<(group_key), (accumulator)>` + 触发式复用；
- **SortExec**：Range Partition + Sample Sort / ExternalMergeSort。

所有向量化代码参考 **Velox 模式**，并使用 SIMD 加速：
- 谓词下推（filtermap）使用 `ArrayOps::filter`；
- 哈希聚合使用 `HashTable`（`ahash` + `xxhash-rust`）+ SIMD `fold_hash`；
- Sort 使用 `Small Sort` 阈值内的 SIMD 排序 + 大数据 LsdRadixSort。

### 9.7 客户端 SDK

- **Rust**：`datafusion` / `datafusion-cli` / `datafusion-server` / `ballista`；
- **Python**：`pip install datafusion` + `pyarrow`；
- **C/C++**：通过 **arrow-cpp ↔ arrow-rs C ABI** 互通（实验）；

## 十、事务与一致性

DataFusion 本身 **不提供事务机制**（无 BEGIN / COMMIT）：

- **单查询一致性**：单条 SQL 在执行期内 consistent（Atomic + Consistent + Isolated），但跨查询无事务；
- **Cross-query isolation**：无事务隔离，可能读到部分写入；
- **Batch output**：所有向量化算子在同一批次内 consistent（同一 RecordBatch 来自同一 producer）；
- **Ballista 跨 stage**：上游 stage 必须完成才能开始下游 stage（依赖 DAG 拓扑）；
- **Multi-Storage Integration**：事务由上游 `TableProvider` 提供（如 PG 适配器可复用 PG 事务）。

如果需要事务，建议集成上游数据库（PG / DuckDB）的方言或将 DataFusion 嵌入 OLTP。

## 十一、复制与高可用

- **DataFusion 自身无状态**：`SessionContext` 可重启：
  - **Ballista Scheduler HA**：通过 K8s Pod + multiple scheduler + 同一 external load balancer；
  - **每 Executors 子集故障恢复**：通过 K8s Job 重启；
  - **Stage 间数据丢失恢复**：重试 stage + 重新计算；
  - **无主从复制**：无 WAL、无 Redis Streams 同步；
- **上游 TableProvider 决定持久层**：
  - PG / MySQL 适配器可与流复制联用；
  - S3（Parquet + Iceberg）：通过 S3 桶版本控制 + Iceberg snapshot 机制容灾；

## 十二、扩展性

| 维度 | 实现 |
|------|------|
| **垂直扩展** | 单机内多核并行（`tokio runtime` + per-stage 并行）+ SIMD |
| **水平扩展** | 通过 Ballista 调度到多节点（Kubernetes）|
| **节点扩缩容** | K8s Deployment `replicaset` 自动扩缩 |
| **冷热分层** | 通过 List Stored Table + Iceberg partition（热数据 SSD，冷数据 S3） |
| **多租户** | 通过 `SessionState` 隔离，但单进程内资源隔离较弱 |

## 十三、性能与基准

- **TPC-H SF100**：41.x 取得 17 秒（Huawei Cloud c7）性能，**接近 ClickHouse 单机水平**；
- **ClickBench 30/43**：DataFusion 41.x 在 31 / 32 / 33 / 35 / 37 / 38 / 39 / 40 / 42 等十多个 query 跑赢 ClickHouse；
- **H2O.ai DB benchmarks**：与传统 OLAP 引擎基本对齐；
- **执行引擎对比（TPC-H SF100）**：
  - **DataFusion 41.x**：17s
  - **ClickHouse 24.x**：12-15s
  - **DuckDB 1.1**：18-20s
  - **Polars 1.x**：20-25s（部分 query 接近）
  - **Velox 0.36**：22-25s（同等硬件）
  - **Spark SQL 4.0**：45-60s
  - **Trino 432**：30-50s
- **Python vs R API**：Python DataFrame API 与 Rust 直接 SQL 调用，**Python 端的 dispatch overhead 约 5-10%**；
- **第三方基准**：**ann-benchmarks** 不直接涉及（DataFusion 不是向量库，但通过 `datafusion-faiss` / `datafusion-lancedb` 间接集成）。

## 十四、运维与可观测性

- **监控**：
  - `df` 在 `/metrics` 路径（启用 `metrics` feature）暴露 Prometheus 指标；
  - 自定义：每个 operator 的 `exection_time`、`produced_bytes`、`produced_batches`；
- **慢查询日志**：
  - `EXPLAIN ANALYZE`：返回 SQL 实际执行时间 + 各 operator 时间；
  - `EXPLAIN VERBOSE`：返回 plan 结构 + stats + dialect；
  - `log_min_duration_statement` 配合 Rust `tracing` 提供 query log；
- **备份恢复**：
  - DataFusion 自身无持久化，**由上游存储实现**（S3、PG 表、Iceberg snapshot）；
- **Tracing**：
  - `datafusion-tracing` crate 集成 `tracing` + `OpenTelemetry`；
  - 自定义 span：`#plan.start`、`#plan.run`、`#operator.consume_batch`；
- **Profile 工具**：
  - `cargo flamegraph`；
  - `perf` / `pyroscope`：持续 profiling；
  - `tokio-console`：异步 task 调度可视化；
- **错误传播**：
  - 所有错误通过 `datafusion::error::DataFusionError` 传递；
  - 支持 `defmt!` / `thiserror` 反向追溯；

## 十五、生态与集成

- **客户端 SDK**：
  - **Rust**：`datafusion`、`datafusion-cli`、`datafusion-server`、`ballista`；
  - **Python**：`pip install datafusion`（同步 pyarrow + polars 可选）；
  - **C/C++**：通过 Arrow C ABI；
  - **Java/Scala**：通过 Arrow Java + Flight SQL；
  - **Go**：通过 Arrow Go + Flight SQL；
  - **Node.js**：通过 Arrow JS + Flight SQL；
  - **R**：通过 Arrow R + Flight SQL；
- **存储 / 文件格式**：
  - **Parquet**：默认深度集成；
  - **CSV / JSON / Avro**；
  - **Iceberg / Delta Lake / Hudi**（通过适配器）；
  - **ORC**（通过 `datafusion-orc`）；
  - **S3 / GCS / Azure Blob**（通过 `object_store`）；
- **ML 集成**：
  - **HuggingFace**：DataFrame 数据预处理；
  - **PyTorch / TensorFlow**：通过 pyarrow / numpy；
  - **LanceDB**：Lance 表 + DataFusion 投影；
- **BI 工具**：
  - Superset、Tableau、PowerBI 通过 Flight SQL / JDBC 接入；
- **消息队列**：
  - **Kafka**：通过 `datafusion-kafka` 消费者；
  - **Redpanda**：同上；
- **容器化**：
  - `apache/datafusion` 官方 Docker image；
  - `apache/datafusion-benchmarks`：基准测试镜像；
- **Helm Chart**：
  - `ballista` Helm chart（社区）；

## 十六、核心论文与算法原理

- **向量化执行模型**：
  - **Boncz et al. (2005). MonetDB/X100: Hyper-Pipelining Query Execution**（SIGMOD）；
  - **Kersten et al. (2015). Velox: A Unified Vectorized Query Execution Engine**（SIGMOD）；
- **优化器与查询重写**：
  - **Selinger et al. (1979). Access Path Selection in a Relational Database Management System**（System R 优化器鼻祖）；
  - **Graefe (1995). The Cascades Framework for Query Optimization**（Cascades 优化器，DataFusion 早期借鉴）；
  - **Trino / Presto 优化器论文**：借鉴 join reorder / predicate pushdown；
- **Substrait**：
  - **Petersohn et al. (2021). Substrait: Cross-Language Engine Independent Database Compute**；
- **SQL 解析**：
  - **sqlparser-rs**：参考 Apache Spark SQL、Calcite；
- **Runtime & 系统**：
  - **Tokio Async**：Acar et al. (2018). "You Are What You Eat: Dataflow-Oriented Programming";

## 十七、源码学习路径

### 17.1 入门（必须看）

1. **`datafusion/core/src/dataframe.rs`**：DataFrame API 与 `LogicalPlan` 构造，约 1500 行；
2. **`datafusion/core/src/sql.rs`**：`SessionContext::sql()` + SqlToRel，约 1000 行；
3. **`datafusion/core/src/execution/context.rs`**：SessionContext 与 SessionState，约 1500 行；
4. **`datafusion/expr/src/logical_plan/`**：LogicalPlan 与 `Expr` 的定义，约 2000 行；
5. **`datafusion/core/src/physical_plan/execution_plan.rs`**：ExecutionPlan trait，约 800 行。

### 17.2 进阶（重要模块）

6. **`datafusion/core/src/physical_plan/filter.rs`** + `projection.rs` + `sort.rs`：基础 operator 实现；
7. **`datafusion/core/src/physical_plan/aggregates/`**：聚合 + GROUP BY + 窗口函数；
8. **`datafusion/core/src/physical_plan/joins/`**：HashJoin + SortMergeJoin + CrossJoin；
9. **`datafusion/optimizer/src/`**：优化器规则（rewrite + physical）；
10. **`datafusion/core/src/datasource/`**：TableProvider + ListingTableProvider；
11. **`datafusion-sql/src/`**：SQL 解析与 LogicalPlan 构建；
12. **`datafusion/proto/src/`**：proto / Substrait 序列化；

### 17.3 高阶（可选）

13. **`ballista/scheduler/src/`**：分布式调度（约 3000 行）；
14. **`ballista/executor/src/`**：分布式 executor；
15. **`datafusion-physical-expr/src/`**：PhysicalExpr + ScalarFunctionExpr（约 4000 行）；
16. **`datafusion/functions-aggregate/`**：内置聚合函数（约 5000 行）；
17. **`datafusion/substrait/`**：Substrait IO；
18. **`datafusion/expr/src/window/`**：窗口函数表达式。

## 十八、源码规模与开发语言

- **Rust**：约 **25 万行**（不含依赖）：
  - `datafusion/` 核心：约 4 万行；
  - `datafusion/core/`：约 5 万行；
  - `datafusion-physical-expr/`、`datafusion-functions-aggregate/`、`datafusion-functions-array/`、`datafusion-functions-window/`、`datafusion-functions-nested/`：共约 7 万行；
  - `datafusion-optimizer/`：约 1 万行；
  - `ballista/`：约 5 万行；
- **Python 绑定**：`datafusion-python` 约 **5 千行 Cython / Rust + pyo3**；
- **SQL parser**：`sqlparser-rs`（独立 crate）约 **3 万行**；
- **测试代码**：约 30% 代码量（`datafusion/core/tests/`、`datafusion/core/benches/`）；
- **C++**：仅少量 FFI（`datafusion-ffi`）；
- **CMake**：极少量（主要用于 Arrow C++ ABI）；
- **Cargo workspaces**：单仓库多 crate（约 40 个 crate）；

## 十九、学习资源

- **官方文档**：
  - https://datafusion.apache.org/ ：DataFusion 官方主页；
  - https://datafusion.apache.org/user-guide/getting-started.html ：用户指南；
  - https://datafusion.apache.org/contributor-guide/ ：贡献者指南；
- **官方仓库**：
  - https://github.com/apache/datafusion ：DataFusion 主仓；
  - https://github.com/apache/arrow-rs ：Arrow Rust 实现；
- **示例代码**：
  - `datafusion/examples/`：约 50 个示例（CSV、Parquet、Flight、UDF、Substrait）；
- **官方博客**：
  - https://datafusion.apache.org/blog/ ：DataFusion PMC 博客；
  - Andy Grove 博客：https://www.influxdata.com/blog/author/andy-grove/ ；
- **教程与视频**：
  - InfluxData YouTube：`@InfluxDB`（DataFusion 多次出现）；
  - Arrow + DataFusion 联合峰会 YouTube；
  - QuestDB / Greptime 工程师的 DataFusion 教程；
- **书籍**：
  - 「DataFusion in Action」（Manning 出版计划中）；
- **第三方教程**：
  - 「Performance Engineering with DataFusion」（Medium 多篇）；
  - 中文 CSDN / 知乎专栏（搜索「DataFusion 教程」）；
- **Hacker News / Reddit**：
  - r/dataengineering、r/rust、`/r/datafusion` Reddit 较少；
  - Hacker News `datafusion` tag。

## 二十、可移植性与部署

- **支持的平台**：
  - Linux（x86_64, ARM64, ppc64le）；
  - macOS（x86_64, arm64）；
  - Windows（x86_64）；
  - Rust ≥ 1.75；
- **容器化**：
  - `apache/datafusion` 官方镜像（基于 debian-slim）；
  - `apache/datafusion-benchmarks`：Harness 集成的基准测试套；
- **Kubernetes**：
  - `ballista` Helm chart（社区）；
  - `datafusion-server`：作为 gRPC 微服务；
- **嵌入式部署**：
  - 最常见的应用：embedded SQL（如 Cloudflare Workers Analytics Engine）；
  - 部署形态：CDN worker / Lambda / edge function（冷启动 ~500ms-1s）；
  - 二进制大小：~10-30MB（静态 Rust binary）；
- **Serverless**：
  - Cloudflare Workers（部分）；
  - AWS Lambda（受限 10MB binary，DataFusion 不合适）；

## 二十一、变体与衍生项目

| 项目 | 关系 |
|------|------|
| **InfluxDB IOx** | 基于 DataFusion + Arrow 的时序数据库 |
| **GreptimeDB** | 国产时序数据库，DataFusion + 自己的存储 |
| **GlueSQL** | SQL on multiple storage（基于 DataFusion）|
| **Apache Ballista** | DataFusion 分布式扩展（同一 PMC）|
| **Apache Arrow DataFusion Python** | Python 绑定 |
| **LanceDB** | 列式向量数据库 + DataFusion 查询层 |
| **Cloudflare R2 Data Catalog** | 基于 DataFusion + Ballista（社区博客） |
| **SqlFlow** | SQL + ML 桥接（基于 DataFusion） |
| **Polars** | Rust DataFrame，部分算子参考 DataFusion |
| **Apache Doris** | 中国团队，部分查询计划基于 DataFusion 思路 |
| **Substrait** | 跨引擎 SQL IR（Apache 顶级项目） |

## 二十二、风险与陷阱

### 22.1 SQL 兼容性

1. **DCL / DDL 受限**：DataFusion 默认只读 SQL（无 INSERT / CREATE TABLE），需要 `TableProvider` 自行实现 mutation；
2. **窗口函数**：40.x 之前窗口函数限制多，**聚合与窗口不能共享** stage，建议升级 42+；
3. **Recursive CTE**：仅 42+ 支持完整 recursive，**早期版本可能 panic**；
4. **Lambda 函数**：仅少部分支持，**array_transform / array_filter 仅 42+**；
5. **类型推断弱**：sqlparser-rs 的类型推断较弱，**复杂表达式可能推断错误**，需要在 select clause 显式 cast；
6. **Schema 演进**：iceberg-rs 在 schema evolution 上有局限，旧字段无法直接 rename；
7. **DateTime 精度**：timestamp 默认 microsecond，**millisecond / nanosecond 需显式 cast**；
8. **Decimal 精度**：未实现 decimal256，仅 decimal128，**超过 38 位精度需要 cast 到字符串**；
9. **Floating 浮点序**：NaN 不参与 ORDER BY 默认行为，可能与 PG 行为不同（PG `< NaN >` 总是 false，DataFusion 同）；

### 22.2 性能陷阱

1. **小表全内存排序**：单个 partition 不到 1024 行时 sort 慢，**可用 partial sort**；
2. **Hash Aggregation 内存爆**：GROUP BY distinct key 很多时，`HashTable` 可能 OOM；**需设置 `MEMORY_LIMIT`** 或 partial aggregate；
3. **Cross Join 卡死**：未检查笛卡尔积，**默认 planner 会拒绝 ≥ 100w × 100w cross join**；
4. **Large bytes 类型**：String / Binary 列在 Arrow Array 中是 `OffsetBuffer`，**`Vec<u8>` 内容会增加 CPU 缓存压力**；
5. **DataFrame 频繁 collect**：Python DataFrame `df.collect()` 阻塞 + 全数据搬回 Python，**对大表慢**；
6. **Python Window Function**：Python 端的聚合函数 dispatch 是 dynamic，**比 Rust UDF 慢 10-100 倍**；
7. **TPC-H SF100**: 内存 RSS 可能 20-30GB，**32GB 机器可能 OOM**；
8. **Bucket join 缺失**：CBO 对 pre-bucket 的 hash join 支持弱，**建议手动 repartition**；
9. **Repeated fetch**：相同 SQL 反复执行时无 query cache，**每次重新优化（5-20ms）**；可用 `prepared_statements`；
10. **Sort + TopK 限制**：仅 `LIMIT 100` 时 sort 不必全局排序，但默认会，**需手动 `LIMIT 100` 时 hint TopK**。

### 22.3 分布式陷阱

1. **Ballista Shuffle IO 慢**：默认 shuffle 用 disk shuffle，**Disk IO 是瓶颈**，建议 NVMe SSD；
2. **Stage 串行瓶颈**：长 stage 链可能拖累总体延迟，**可手动 inline 小 stage**；
3. **Scheduler 单点**：Ballista 0.10 之后才有 active-active scheduler HA，**早期只有 active-passive**；
4. **Network 包分片**：gRPC 默认 4MB 上限，**大 shuffle chunk 会被切片**，影响延迟；
5. **Stage 重试代价大**：失败 stage 重试导致整个 stage 重新调度，**应用层建议幂等**；

### 22.4 多语言 / 集成陷阱

1. **Python binding 内存**：跨 Rust ↔ Python 边界要走 FFI，**大数据列可能溢出 stack**；
2. **Flight SQL 协议不一致**：DataFusion 的 `arrow-flight-sql` 与 Debezium 的兼容性局限，**自定义协议需测试**；
3. **Substrait 兼容性**：不同厂商对 Substrait 的支持差异大，**某些算子不能跨引擎 round-trip**；
4. **Iceberg 表层事务**：DataFusion 对 Iceberg 写事务不保证完整 ACID，**复杂写流程需自行处理**；
5. **DataFusion + JDBC**：DataFusion 没有官方 JDBC，**通过 Flight SQL + jdbc-arrow-flight-bridge 间接支持**；

### 22.5 运维 / 安全

1. **资源隔离弱**：单进程内多 Session 默认共享内存池，**建议每用户独立 SessionState + MemoryPool**；
2. **慢查询日志残缺**：`EXPLAIN ANALYZE` 仅在客户端可用，**无内置 query log**；
3. **证书与加密**：默认 gRPC 明文，**生产必须 TLS**；
4. **UDF 沙箱**：UDF 是 native Rust，**运行在 DataFusion 进程内，**用户 UDF 崩溃会拖垮整个 Context**；
5. **错误信息泄露**：默认错误信息可能含 schema 信息，**生产前需 redact error detail**。

## 二十三、与本项目的关联

| engineering 模块 | 与 DataFusion 关联 |
|------------------|---------------------|
| `engineering/src/db/executor/` | SQL 执行器可参考 DataFusion 的优化器与执行器架构 |
| `engineering/src/db/sql/sql_exec.c` | SQL parser 可基于 DataFusion 的 SQL → Rel 转换 |
| `engineering/include/db/bufmgr.h` | BufferPool 可参考 DataFusion 的 `RecordBatch` 内存管理 |
| `engineering/src/db/index/btree.c` | B+Tree 索引可与 DataFusion 的 Statistics 集成（push down predicate） |
| `engineering/apps/db_server/` | 数据库服务端可基于 DataFusion 提供 SQL + Arrow Flight SQL |
| `engineering/apps/rag/` | RAG 数据预处理可用 DataFusion Python 嵌入 |

具体落地路径：

1. **第一步**：Cargo workspace 增加 `datafusion`、`datafusion-cli`、`arrow-rs` 依赖（`Cargo.toml`）；
2. **第二步**：将 `engineering/src/db/` 中 `relation_t`、列存文件实现 `TableProvider` 接口；
3. **第三步**：在 `engineering/src/db/sql/` 增加 `SqlToRel` 转换器，把现有 SQL 语法映射到 DataFusion LogicalPlan；
4. **第四步**：在 `engineering/apps/db_server/` 添加 Flight SQL Server，使用 `datafusion-flight-sql` crate，对外暴露 query API；
5. **第五步**：在 `engineering/sdk/python/` 增加 `datafusion-python` 嵌入，使 Python 端能 `pd.sql("SELECT ...")` 直接调用本数据库。

## 二十四、参考资料

1. Apache DataFusion 主页：https://datafusion.apache.org/
2. Apache DataFusion GitHub：https://github.com/apache/datafusion
3. DataFusion 用户指南：https://datafusion.apache.org/user-guide/introduction.html
4. DataFusion Ballista 仓库：https://github.com/apache/datafusion-ballista
5. Apache Arrow Rust：https://github.com/apache/arrow-rs
6. Substrait 规范：https://substrait.io/
7. sqlparser-rs：https://github.com/sqlparser-rs/sqlparser-rs
8. InfluxData DataFusion 案例：https://www.influxdata.com/blog/influxdb-iox/
9. ClickBench：https://benchmark.clickhouse.com/
10. TPC-H 规范：https://www.tpc.org/tpch/
11. ASF PMC 报告 2024：https://www.apache.org/foundation/
12. Andy Grove 个人博客（InfluxData）：https://www.influxdata.com/blog/
13. DataFusion 性能优化讨论：https://github.com/apache/datafusion/discussions
14. Apache Arrow Flight SQL 规范：https://arrow.apache.org/docs/format/FlightSql.html
15. Hacker News DataFusion tag：https://news.ycombinator.com/from?site=datafusion.apache.org
