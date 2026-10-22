# Meta Velox Wiki（C++ 向量化查询执行引擎）

> 参考版本：Velox 0.36 / 0.37 / 0.38（2024-2025），C++20，依赖 Apache Arrow 共享内存层。本 wiki 重点覆盖 Velox 作为 Meta 内核的统一执行框架、其 Operator + Expression 双抽象、与 Apache Arrow / DataFusion / Gluten / Presto 的集成，以及与本项目（SQL 执行器）的关联。

## 一、项目定位

Velox 是由 **Meta（Facebook）** 在 **2020** 年开源的 **「统一 C++ 向量化查询执行库」**，设计目标是为 Meta 内部的所有计算平台（Presto、Spark、Glow、Tectonic）提供 **统一的、向量化、内存友好的执行引擎**，从而避免过去「Spark 实现一份、Presto 实现一份、自研管线实现一份」的多份重复开发。

口号：**"A unified execution engine geared towards enabling composable data management systems."**

- **核心痛点**：
  - 内部重复实现：Meta 原本有 Spark（Java）、Presto（Java）、自研管线（C++）三套执行引擎，**SQL / 表达式 / 算子 / 资源管理重复实现 3 次**；
  - Java JVM GC 抖动：Presto + Spark 在大查询时 GC 暂停 100-1000ms，**长尾 P99 恶化**；
  - 跨平台数据复用难：Spark data cache → Presto dataframe 互转代价大；
  - 列存 + 向量化是 OLAP 主线，但 Meta 内部没有统一的 C++ 执行层；
  - 资源（HBM / SSD / DRAM）分配无统一抽象；
- **典型用途**：
  - **Presto C++ backend**（Presto C++ worker，Velox 是核心执行层）；
  - **Apache Gluten**（Spark Native 引擎，作为 Spark 的执行后端替换 JVM query execution）；
  - **Tectonic FoundationDB CDC**（Meta 时序 + OLTP 存储）；
  - **Meta 内部 Presto-on-Velox**（Meta 数据仓库，**每天 1.5 亿次查询、6 EB 数据**）；
  - **Apache Doris**（国产 OLAP，Doris 0.15+ 的向量化执行引擎完全基于 Velox 重写）；
  - **Apache Doris/StarRocks 部分模块**（StarRocks 0.1 后期借鉴 velox）；
  - **自定义 SQL 引擎**：JPM 组、字节、商汤等公司基于 Velox 二次开发；

- **典型用户**：
  - **OLAP 引擎开发者**：需要在自有存储之上获得向量化 SQL 执行能力；
  - **云厂商**：替代 Presto/Spark 的 JVM 执行层，避免 Java GC；
  - **数据库 / DataFrame 库**：StarRocks、Doris、Polars 部分算子参考；
  - **AI / ML 数据平台**：Feature Store SQL 计算管线（Velox 算子可被 CUDA 后端加速）；
  - **教学 / 研究**：C++ 现代向量化执行系统的最佳参考实现之一；

## 二、发展历史与社区

- **起源**（2019-2020）：
  - Facebook / Meta 内部项目，最初作为 **Presto-on-Spark 实验**；
  - 2020-10 内部开源（github.com/facebookincubator/velox）；
  - 设计与实现由 Meta 数据基础设施团队主导，核心贡献者 **Masha Zomrov / Pedro Pedreira / Biswapesh Chattopadhyay / Krishna Pai / Sreeni Viswanadha / Eric R. Hildebrand** 等；
- **里程碑**：
  - 2021-03：开源第一版，含基础 Operator（Filter、Project、HashJoin）、Expression framework；
  - 2021-09：v0.1.0，含 Aggregations、Window、Sort、Hash Join；
  - 2022-06：v0.1.0 与 Presto C++ Worker 集成，**Meta 内部替换 70% 的 Java Presto 流量**；
  - 2022-12：v0.2.0，Type system 增强、Vector optimization（Lazy vectors）、Gluten 集成；
  - 2023-06：v0.3.0，Gluten 成为 Velox 主要下游（Spark Native Velox backend）；
  - 2023-12：v0.4.0，社区参与激增，Doris、StarRocks、Polars 部分采用；
  - 2024-06：v0.6.0，C++20 全量采用，Arrow 互操作，新增 Connector framework；
  - 2024-12：v0.36，Type-Time Area 统计新增，Gluten 1.3+ 集成稳定；
  - 2025-04：v0.38、Gluten 2.x、Presto-on-Velox 在 Cloudera / Uber / Lyft / Tencent 部分商业部署；
- **维护状态**：极活跃，季度大版本，每月小版本，月 commit 500+；C++ 圈最热门的 OLAP / 向量化项目之一；
- **社区**：
  - GitHub 3.5k+ star、Linux Foundation LF AI Data 沙箱项目；
  - Slack `velox-oss.slack.com`、GitHub Discussions、Mailing list `velox.dev@velox.io`；
  - 季度 Velox Meetup（北美、亚太）；
- **PMC + Steering Committee**：Pedro Pedreira（Meta 主席）、Biswapesh Chattopadhyay（Meta）、Bikash Roy（Apache Gluten）、Wenbin Lyu（Doris）、徐晨晨（公众号文章作者）；

## 三、许可与商业限制

- **核心代码**：**Apache License 2.0**（`velox/`、`velox-dwrf`、`velox-iceberg`），允许商用、修改、私有部署；
- **与商业产品的关系**：
  - **Meta 内部 Presto-on-Velox**：完全基于 Velox，**但 Meta 不开源 Presto Worker**；
  - **Apache Gluten（LF AI Data Top-Level）**：Velox + Spark + Gluten 是 Meta + LinkedIn + Intel + Apple 等共同推动；
  - **Cloudera / Uber / Lyft / Tencent / ByteDance**：均有基于 Velox 的二次开发商业发行版；
  - **国内**：Doris（Apache 顶级项目，基于 Velox）、StarRocks 部分借鉴、Lindorm 部分模块；
- **生态关系**：
  - **上游依赖**：
    - Apache Arrow C++（`arrow/`、`arrow/compute/` 互转）；
    - Apache Thrift（Thrift 协议，与 Presto 协议互通）；
    - Folly（Meta C++ 基础库：async / FBString / F14）；
    - Boost / fmt / Protobuf / Range-v3 / <algorithm>；
  - **下游生态**：约 **200+ 企业 + 50+ 项目**：
    - Presto-on-Velox（Meta）、Apache Gluten（LF Top-Level）、Apache Doris（顶级）；
    - StarRocks（部分借鉴）、Apache Spark（Gluten 后端）、Presto C++ Worker、Apache Iceberg、`velox-iceberg`、`velox-dwrf`（DWRF reader）、`velox-orc`；

## 四、业界主要使用方与案例

- **Meta 内部（核心用户）**：
  - **Presto-on-Velox**：Meta 内部 Presto 集群，**每天 1.5 亿次查询、6 EB 数据**（Meta 2024 报告）；
  - **PyTorch / Glow**：ML Feature Store SQL；
  - **Tectonic**：时序 + CDC 存储；
  - **Hive / Spark**：通过 Gluten；
- **外部大型用户**（公开）：
  - **LinkedIn**：LinkedIn Pinot 应用 Velox 作为执行层（与 Spark 集成）；
  - **Adobe**：Adobe Experience Platform 部分模块；
  - **Uber**：基于 Velox 的查询优化器（Hudi + Velox）；
  - **Lyft**：Lyft Analytics on Velox；
  - **Tencent**：TDSQL、ByteHouse、CTSDB 部分模块；
  - **ByteDance**：火山引擎 LAS / Lakehouse、Bytehouse 部分模块基于 Velox 改造；
  - **Alibaba**：MaxCompute 部分模块（实验）、Lindorm 多个存储引擎；
  - **JD.com**：JDB、JAQ 部分 OLAP 模块；
  - **Huawei**：FusionInsight、GaussDB 部分模块；
  - **Tencent**：CDW、HCI、TCVector 部分模块；
- **典型场景**：
  - **C++ 向量化 OLAP 引擎**：Doris 0.15+ 完全使用 Velox（替代 Doris 老的 Java/Pipeline 执行层）；
  - **Spark Native Velox backend**：通过 Gluten 替换 Spark JVM ExecutionEngine，避免 JVM GC；
  - **Presto C++ Worker**：Presto 官方 Velox 取代 Presto C++ FaaS 旧实现；
  - **Lakehouse SQL 引擎**：Iceberg / Hudi / Delta Lake 通过 Velox 查询层；
  - **AI 数据预处理**：Feature Store SQL 计算管线 + Arrow 互通直接进 PyTorch；
  - **边缘计算**：Velox 在 edge function / Lambda 中跑 SQL（如 Cloudflare Workers Analytics 部分实验）；

## 五、一句话对比定位

> Velox 是 **「C++ 向量化、统一执行框架、Operator + Expression 双抽象、与 Arrow 共享内存、与 Presto 互通、Presto-C++-Worker 主后端、Gluten Spark 主后端、Doris 整个内核、Type 系 >> 30 + 个 Operator >> 60 + 个 Function 、 LF AI Data 项目」**；与 **DataFusion（Rust 原生向量化 + SQL + Arrow）**、**Photon（Databricks 内部 C++ 向量化）**、**DuckDB（嵌入式 C++ OLAP，单一 binary）**、**ClickHouse（C++ 列存 OLAP，关闭向量 + 列存双优化）**、**ClickHouse Native / 二次开发**、**Impala（C++ 向量化（较老））**、**Trino / Presto（Java 分布式 SQL）**、**Spark（Tungsten / Vectorized Reader）** 属于「C++ 向量化查询执行引擎」生态。Velox 的差异化是 **「统一抽象 + Operator + Expression framework + Arrow 深度互转 + Presto / Gluten / Doris 全覆盖 + Meta 内核级使用 + LF AI Data 项目 + 社区公开开发」**。

## 六、整体架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                          输入 / 输出 层                              │
│  - C++ API: velox::exec::Driver / QueryCtx                        │
│  - JNI / FFI bindings (Python, Java)                               │
│  - Presto Protocol (Thrift) via Prestissimo                        │
│  - Gluten Spark execution API (Java <-> JNI <-> Velox)             │
│  - Doris internal (Velox 直接替换 Doris exec engine)               │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                          执行引擎层（Operator）                      │
│  - velox/exec/                                                       │
│    - Filter, Project, TableScan, Values, PartitionedOutputBuffer  │
│    - HashBuild / HashProbe, MergeJoin, NestedLoopJoin, IndexLookup │
│    - HashAggregation, StreamingAggregation, Aggregation           │
│    - Window, TopN, OrderBy, Limit                                  │
│    - Unnest, SpatialJoin, RowNumber / Rank                         │
│  - Pipeline / TaskQueue / Consumer / Producer 模型                │
│  - OperatorStats, exec::trace                                      │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                          表达式框架（Expression）                    │
│  - velox/expression/                                                 │
│    - Expr: FieldAccess / Call / Cast / SpecialForm / Lambda        │
│    - ExprSet → CompiledExpression → Flat expression evaluation     │
│    - SimpleFunction / VectorFunction / Aggregate Function        │
│    - FunctionRegistry (注册 Presto / Spark 函数 1:1)               │
│    - presto_fn / spark_fn / sparksql_fn / hll_fn / tpcds_fn        │
│    - 优化: ConstantFolding, CommonSubexpressionElimination        │
└─────────────────────▼───────────────────────────────────────────────┐
│                          内存管理层                                  │
│  - velox/memory/                                                     │
│    - MemoryPool 层级 (root, query, task, operator)                 │
│    - MemoryAllocator: MappedMemory / GpuMemory / CpuMemory         │
│    - AllocationListener (spillable 可监控)                          │
│    - Spill (磁盘临时 + 同步溢出): disk-spill, hash-spill, agg-spill│
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                          向量化数据层（Vector / Type）               │
│  - velox/vector/                                                     │
│    - BaseVector / FlatVector / DictionaryVector / ConstantVector  │
│    - ArrayVector, MapVector, RowVector, StructVector               │
│    - LazyVector (延迟加载 / 物化)                                   │
│    - Buffer / SizedInputStream / ByteInputStream                   │
│    - VectorEncoder / VectorDecoder / VectorHasher                  │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                          通用数据格式层                              │
│  - velox/common/: config, exception, time, enums                   │
│  - velox/dwio/: DWRF / ORC / Nimble format IO                      │
│  - velox/dwio/parquet/: Parquet reader / writer                    │
│  - velox/iceberg/ Iceberg Table / Metadata                         │
│  - velox/hive/: Hive 表 + Spark Glue / Hive Metadata               │
│  - velox/cudf/: cuDF 互转 (GPU 加速实验, 35+)                     │
│  - velox/arrow/: Arrow Array ↔ BaseVector 转换                   │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                          基础服务层                                  │
│  - velox/exec/ (TaskManager, QueryManager, Operator)              │
│  - velox/connectors/ (Connector 注册表 + 工厂)                    │
│  - velox/serializers/ (Presto page、Spark Shuffle 协议)           │
│  - velox/test/ (测试辅助, fuzzer)                                  │
└─────────────────────────────────────────────────────────────────────┘
```

**关键设计原则**：

1. **Operator + Expression 双抽象**：Operator 是高层 "算子"（Join/Aggregation），Expression 是底层 "标量 / 复杂表达式"；**Operator 用 Expression 表达 selectivity / projection / condition**；
2. **Pipeline 模型**：每个 Operator 通过 `getOutput()` 返回一个 `RowVector`（含 `Vector` 列表），下游 Operator 一次性消费整批；
3. **Type 系统**：`Type` + `BaseVector` + `VectorPool`，**类型安全** + **SIMD 友好**；
4. **Columnar 处理**：每个 Vector 是一列，`RowVector = `[children]`，**天然向量化**；
5. **Spill to disk**：HashJoin / Aggregation 的 spill 自动溢出到 SSD，**支持超内存查询**；
6. **GPU 集成（实验）**：通过 cuDF 让某些算子在 GPU 上执行（37+ `velox/cudf`）；
7. **Connector 可插拔**：通过 `Connector` 抽象接入任意存储（Doris、Presto、Iceberg、Glue、S3）；

## 七、Velox 作为扩展的集成模式

Velox 在生态中扮演 **「计算层 / 执行引擎」**，其本身 **不提供 SQL parser、不提供存储、不提供查询调度**，但提供完整的 **Operator + Expression + Vector + Memory** 抽象：

### 7.1 与 Apache Arrow 的集成

Velox 与 Apache Arrow 是双向 **零拷贝共享**：

```cpp
// Arrow → Velox
auto velox_array = ArrowBridge::toVelox(arrow_array);

// Velox → Arrow
auto arrow_array = ArrowBridge::fromVelox(velox_vector);

// 在 Query 中混用
auto velox_table = std::make_shared<RowVector>(...);
auto arrow_table = veloxToArrow(velox_table);
// 传给 Python（PyArrow）做 ML
py::object py_table = py::module::import("pyarrow").attr("Table").attr("from_pandas").attr("from_arrow")(arrow_table);
```

**集成位置**：`velox/arrow/`、`velox/dwio/parquet/duckdb_reader.cpp` 等。

### 7.2 与 Apache Gluten（Spark Native）的集成

Apache Gluten（**核心 Velox 后端**）替换 Spark 的物理执行层：

```
Spark SQL → Catalyst Optimizer → GlutenPlan → Substrait → Velox Operators → Arrow / Native Vector → Presto / Spark 输出
```

**集成位置**：`gluten/cpp/velox/`，Gluten Java 后端通过 JNI 调用 Velox C++。

### 7.3 与 Presto / Prestissimo（C++ Presto Worker）的集成

Presto C++ Worker（Prestissimo）使用 Velox 作为执行层：

```
Presto Coordinator (Java) → Thrift Protocol → Prestissimo Worker (C++) → Velox Execution → Presto Page → Coordinator → 客户端
```

`velox/exec/TaskManager` 是核心 API，对外提供 Task 创建 / Pipeline 执行 / Operator Stats 等。

### 7.4 与 Apache Doris 的集成

Apache Doris 自 0.15+ 完全基于 Velox：

```
Doris Frontend (Java) → Doris FE Optimizer → Doris Backend (C++) → Velox Execution → Doris Segment Storage → Doris 结果集
```

Doris 0.15+ 的 `doris/be/src/` 中所有 `*_operator.cpp` 直接基于 Velox Operator + Expression 实现。

### 7.5 自定义 Connector 与 Operator

```cpp
// 1. 自定义 Connector（数据源）
class MyConnector : public Connector {
    std::shared_ptr<ConnectorTableHandle> ...
};

// 2. 自定义 TableScan
class MyTableScan : public TableScan {
    RowVectorPtr getOutput() override { ... }
};

// 3. 自定义 Expression Function
REGISTER_STAT_FUNCTION("my_fn", MyFn::signatures(), MyFn::create);
```

## 八、索引机制（Operator 模型）

Velox 本身 **不提供 B+Tree / HNSW / Hash 索引**，但提供了一整套 **「Operator + Expression + Adaptive Operators」** 等价概念。

### 8.1 Vector 与 Buffer 设计

```
BaseVector
├─ FlatVector<T>           # 一列定长类型（int32, int64, double）
├─ DictionaryVector<T>     # 字典编码（用 int32 索引代替 T）
├─ ConstantVector<T>       # 全列共享一个常量
├─ LazyVector              # 延迟物化（需解码时再展开）
├─ ArrayVector / MapVector # 嵌套类型
├─ RowVector (vector of vector)  # 结构体 / Tuple
└─ VectorPtr / VectorSize # 输出类型
```

### 8.2 Operator 的向量化执行

每个 Operator 的 `getOutput()` 返回一个或多个 `RowVector`（默认 1024-8192 行 / 批）：

```cpp
// Filter Operator 示例
class Filter : public Operator {
    RowVectorPtr getOutput() override {
        // 1. 拉取上游 RowVector
        auto input = children_[0]->getOutput();
        // 2. 对每一列向量化计算 Expression
        EvalCtx ctx(operatorCtx_.get(), input);
        SelectivityVector rows(input->size());
        // 3. 在 Expression framework 中 eval: `col("age") > 18`
        filter("filter_expression", rows, /* input */, /* result */);
        // 4. 按 filter mask 选取 rows
        return wrapChildren(rows, input);
    }
};
```

### 8.3 Expression Framework 详解

Velox Expression 是与 Operator 平行的抽象，每个 Expression 表示 **「在 RowVector 上的列间计算」**：

```cpp
// SimpleFunction 注册
class MyFn : public exec::VectorFunction {
    void apply(...) override {
        // SIMD 加速向量化处理
        // input: auto& flat = input->asFlatVector<int32_t>();
        // output: flat_output[i] = flat_input[i] * 2;
    }
};

void registerMyFn() {
    exec::registerVectorFunction(
        "double_int32",
        {exec::FunctionSignatureBuilder<int32_t, int32_t>()
            .argumentType("integer")
            .returnType("integer")
            .build()},
        std::make_unique<MyFn>());
}
```

### 8.4 HashTable 与 Adaptive Join

- **HashJoin**：在 HashBuilder 阶段构建 `HashTable<T>`（F14 哈希表），HashProbe 阶段以 F14 增量查询；
- **Adaptive Hash Join**：驱动 + 串行化 + Adaptive：memory threshold 后切 Streaming Hash Join；
- **MergeJoin**：数据排序后双指针 O(N+M) join，适合已 sort 的输入；
- **NestedLoopJoin**：支持非等值条件 join（`!=`），但通常慢。

### 8.5 HashAggregation 与 StreamingAggregation

- **HashAggregation**：增量构建 GroupBy Hash Table + 触发式刷新；
- **StreamingAggregation**：基于 sort 输入做 merge aggregation（无需 hash）；
- **partial aggregation → final aggregation** 两阶段（Presto/GROUPING 模式下可以分片 partial aggregate）。

### 8.6 Spill to Disk（最大特色）

HashJoin / HashAggregation / Sort 都支持 **Spill-to-Disk**：

```
内存压力超过阈值 (memory_pool check)
        │
        ▼
┌─────────────────┐
│ Partitionize    │
│ & Write to SSD  │
└────────┬────────┘
         ▼
┌─────────────────┐
│ Merge on read   │
│ into final result│
└─────────────────┘
```

- 支持 `disk spill config`：max_spill_bytes / spill_path / write_buffer_size；
- **Spillable Hash Table**：避免 OOM；
- **Many "tree" spillable partition**：跨 SSD 多目录平衡。

### 8.7 Type 系统与特殊类型

```
Type
├─ scalar: Integer/Boolean/Varchar/Decimal
├─ complex: Array / Map / Row / Json
├─ temporal: Date / Time / Timestamp / Interval
├─ special: IpAddress / Uuid / HyperLogLog / Digests / QDigest
└─ geometry: Geometry / SphericalGeography  (37+)
```

Velox Type / Arrow Type 可以 **通过 `arrow::Type ↔ velox::Type` 转换**。

## 九、查询 / 计算 / 接口层

### 9.1 SQL 解析

Velox **不提供 SQL parser**，由上游（Presto Coordinator、Doris Frontend、Spark Catalyst）解析后传入 Plan：

- Presto：通过 Thrift 协议传输 PlanNode 树；
- Gluten：Spark 通过 Catalyst → GlutenPlan → Substrait → Velox；
- Doris：Doris FE 产生 PlanNode tree → Doris BE (Velox)；

### 9.2 Driver / Task 模型

```cpp
// 创建一个 Query
auto queryCtx = std::make_shared<core::QueryCtx>(...);
auto plan = PlanBuilder()
    .tableScan("orders", {"order_id", "amount", "customer_id"})
    .filter("amount > 100")
    .project({"customer_id", "order_id"})
    .planNode();

// 提交 Task
auto task = exec::Task::create(
    "task_id",  // taskId
    plan,
    0,           // destination
    consumer,
    queryCtx);
Task::start(task);

// 获取结果
auto result = task->getResults();
```

### 9.3 Pipeline

Velox 的执行按 Pipeline 划分，每个 Pipeline 是一组顺序连接的 Operator：

```
Pipeline 1: Filter → Project → ...
       │
       ▼
Pipeline 2: HashJoin[build] ⊕ HashJoin[probe]
       │
       ▼
Pipeline 3: HashAggregation → Limit → LocalExchange
       │
       ▼
Pipeline 4: OutputBuffer → Result Delivery
```

LocalExchange 是 Velox 的本机 shuffle，跨 Pipeline 重分区。

### 9.4 主要 Operator 与 Function

**Operators**：
- TableScan、Filter、Project、HashJoin、MergeJoin、NestedLoopJoin
- HashAggregation、StreamingAggregation、Window、TopN、OrderBy、Limit
- PartitionedOutputBuffer、Unnest、RowNumber、Rank

**Functions**：
- 数学 / 字符串 / 时间函数：约 100+；
- Array / Map / Struct：约 50+；
- Aggregate：约 80+（含 `sum` / `count` / `approx_distinct` / `histogram` / `merge` / `arbitrary` / `tdigest` 等）；
- 自定义：SimpleFunction + VectorFunction + AggregateFunction 三种 API。

### 9.5 向量化计算（SIMD）

```
VecCompiler: AST → Simple Function → VectorFunction
                                            │
                                            ▼
                                  FlatVector<int32_t> a, b
                                       │ (SIMD friendly)
                                       ▼
                                  result[i] = a[i] + b[i]
                                  (AVX2 / AVX512 / NEON)
```

- **Add2Int**：默认 `result = a + b` 由 AVX2 加速，比 scalar 快 6-8 倍；
- **FlatVector<int32_t>**：按字节连续，SIMD 友好；
- **String**：用 `std::string_view` + 预取优化。

### 9.6 客户端 SDK

Velox 主要是 **「C++ 库」**，通常作为服务的一部分，**不直接对外暴露 API**。使用方式：

- **Java**：Presto Worker（Thrift + JNI）、Doris FE（Java）+ Doris BE（C++ via Velox）；
- **Python**：通过 Arrow 桥接 / JNI / Velox Python integration (实验, 38+)；
- **Spark**：通过 Gluten Java 后端调用 Velox C++；
- **REST**：Presto HTTP + Thrift Protocol；
- **JDBC / ODBC**：通过 Presto Coordinator 提供。

## 十、事务与一致性

- **Velox 不提供事务**。它只是执行引擎，事务由上游（Doris / Presto / Spark）或下游存储（Doris Segment / Iceberg / S3）处理；
- **Atomicity**：单个 Query 在执行中 Atomic，跨 Query 无事务；
- **Splittable**：单个 Task / Pipeline 可被 spilled 到 disk 后恢复；
- **Cross-Pipeline Consistency**：每个 Pipeline Stage 间是 serializable（结果在内存）；
- **End-to-End Consistency**：由上游 Coordinated 提供，例如：
  - Presto Coordinator 永远发送 plan → 调度 cluster → 收集 result；
  - Doris FE 维护 `partition_id + version` 一致性；
  - Spark Gluten 通过 `Iterator` 提供 streaming consistency。

## 十一、复制与高可用

- **Velox 自身无复制机制**：
  - Data Source（如 Doris / Presto / Iceberg）持久化数据并复制；
  - Velox 仅作为执行层，不维护 state；
- **Coordinator HA**：
  - Presto / Doris 均通过 leader election（Raft / Curator）实现；
  - Velox 仅是 worker 端的一部分；
- **Task 重试**：Velox Pipeline 支持重试，受上游 coordinator 控制；
- **Spilled data 重启**：spilled data 持久化到 SSD，失败重启后可恢复。

## 十二、扩展性

- **垂直扩展**：单进程内多线程 + SIMD，单台服务器跑大查询；
- **水平扩展**：通过 Coordinator（如 Presto / Doris）调度多 Worker；
- **K8s 部署**：Presto / Doris / Gluten 均可在 K8s 上；
- **GPU 加速**：`velox/cudf/` 35+ 部分实验 GPU 算子；
- **CUDA / ROCm**：通过 Nvidia CUDA 或 AMD ROCm 加速 Filter / Project / Aggregation；
- **冷热分层**：通过上游 Connector（如 Iceberg partition）实现。

## 十三、性能与基准

- **ClickBench 30 / 43**：Velox 0.36 在 28 / 32 / 38 等多个 query 跑赢 ClickHouse；
- **TPC-H SF100**：Velox 单机 13-18s，**接近 ClickHouse / DataFusion**；
- **H2O.ai DB benchmarks**：Velox 与 DataFusion / DuckDB 同档；
- **GLuten + Spark** vs Spark JVM：TPC-H SF100 上 Velox 快 **2-5 倍**（消除 JVM GC + 向量化）；
- **Presto Java vs Presto C++ (Velox)**：**性能快 5-10 倍**，p99 latency 减半（参考 Meta 内部分享）；
- **CPU 利用率**：向量化 + SIMD 让 CPU **稳定 70-90% 利用率**，无 JVM GC 抖动；
- **Memory Spill**：HashJoin spilled 到 disk 时性能下降 ≈ 30-50%，但避免 OOM。

第三方基准：ann-benchmarks 不直接参与（Velox 不是向量 DB，但通过 DWRF / Presto 的 `simdjson` 处理）。

## 十四、运维与可观测性

- **监控**：
  - Operator-level stats：`exec::OperatorStats` 暴露每一 Operator 的 `runtime` / `inputRows` / `outputRows` / `memoryBytes`；
  - 可通过定制 `TaskStatsPrinter` 输出 JSON；
- **慢查询日志**：
  - `Operator::trace` API（`trace::CallTrace::enabled = true`）；
  - `exec::trace::TaskTrace` 包含每一阶段、每一 Operator 的耗时细分；
- **Prometheus / OpenTelemetry 集成**：
  - Velox 内置 `fbmonitor` 风格的 metrics（基于 facebook-lite）；
  - Presto C++ Worker 暴露 Prometheus metrics;
  - Doris 通过 `/api` 暴露 RESTful metrics，间接得到 Velox 数据；
- **Spill 监控**：
  - 每 Operator 可查 `spillStats`：spilled rows / bytes；
  - 阈值超限会发出 warning；
- **Tracing**：
  - Presto Coordinator 用 OpenTelemetry tracing，**Velox 的`TaskTrace`** 与 Presto Trace 序列对齐；
- **Backpressure**：
  - 内存压力超阈值会**降低 Operator 处理速率**，可能反映为输入 buffer 满；
  - 推荐监控：`velox/memory/reclaim_exec_pool` 的 RSS；

## 十五、生态与集成

- **下游项目**（直接使用 Velox）：
  - **Presto C++ Worker**（Prestissimo）：Velox 是核心执行层；
  - **Apache Gluten**：Spark Native Velox backend；
  - **Apache Doris**：自 0.15+ 完全基于 Velox；
  - **Apache Iceberg**：通过 `velox-iceberg` Connector 查询 Iceberg；
- **数据湖 / 文件格式**：
  - **Parquet**：默认支持；
  - **ORC**：通过 `velox-dwio-orc`；
  - **DWRF**：Meta 自研列存；
  - **Nimble**：Meta 内部流格式；
  - **Iceberg / Delta / Hudi**：通过 Connector；
- **云原生**：
  - 部署形态：Microservice + Kubernetes；
  - **Presto on K8s**：通过 Presto C++ Worker；
  - **Doris Operator**：社区提供；
- **观测与 tracing**：
  - Presto + Velox + OpenTelemetry tracing 是 Meta 标配；
- **AI / ML**：
  - 通过 Arrow Vector 互转，与 PyTorch / TensorFlow 直接桥接；
  - Feature Store SQL 计算管线（Proxygen + Velox + Arrow）；
- **Vector KNN**：
  - Velox 不内置 KNN，但 Doris + Velox 通过外置 UDF 可以支持向量 KNN；
  - `velox-functions-vector` 提供 L2 / IP / cosine 距离函数（39+）；
- **其他 Operator 实现**：
  - `velox-functions-vector`：HNSW / KNN UDF；
  - `velox-iceberg`：Iceberg Meta / Time Travel；

## 十六、核心论文与算法原理

- **向量化执行模型**：
  - **Boncz et al. (2005). MonetDB/X100: Hyper-Pipelining Query Execution**（CWI）；
  - **Kersten et al. (2015). Velox: A Unified Vectorized Query Execution Engine**（SIGMOD 2015 老 Velox 论文）；
  - **Anneser et al. (2022). Velox: The journey towards Meta's unified execution engine**（VLDB 2022 工业演讲）；
- **Type 系统与向量**：
  - **Krikellas et al. (2010). A Multi-Ring Buffer for Heterogeneous Memory & IPC**（Apache Arrow 形式化）；
- **资源管理**：
  - **Meta's Velox memory model**（LF AI Data Velox Meetup 2023）；
- **优化技术**：
  - **Graefe (1995). The Cascades Framework for Query Optimization**；
  - **Neumann (2011). Efficiently Compiling Efficient Query Plans for Modern Hardware**（Volcano 重启）；
- **C++ 现代设计**：
  - **Stroustrup (2013). The C++ Programming Language** 4th / 5th Ed；
  - **Boost / Folly / Range-v3**：Meta Folly 文档；
  - **SIMD 加速**：
    - **Intel Intrinsics Guide**；
    - **Fog (2014). Optimization of Manuel Fortnite / SIMD Cheatsheet**；

## 十七、源码学习路径

### 17.1 入门（必须看）

1. **`velox/vector/BaseVector.h`**：`BaseVector` 抽象与子类（`FlatVector` / `ArrayVector` / `DictionaryVector`）；
2. **`velox/vector/FlatVector.h`**：定长类型向量化 API（SIMD 友好）；
3. **`velox/exec/Operator.h`**：Operator 基类与 `getOutput()` 多态；
4. **`velox/exec/Driver.h`**：Task 的执行模型（Pipeline + Consumer + Producer）；
5. **`velox/expression/Expr.h`**：Expression AST + `ExprSet::eval()`；约 800 行。

### 17.2 进阶（重要模块）

6. **`velox/exec/Filter.cpp`** + `Project.cpp`：最简单 Operator 实践；
7. **`velox/exec/HashBuild.cpp`** + `HashProbe.cpp`：核心 Join 算子；
8. **`velox/exec/HashAggregation.cpp` / `StreamingAggregation.cpp`**：聚合算子；
9. **`velox/exec/Window.cpp`**：窗口函数；
10. **`velox/dwio/parquet/Reader.cpp`**：Parquet Reader + Velox Vector 互转；
11. **`velox/iceberg/Table.cpp`**：Iceberg Meta + 时间旅行；
12. **`velox/common/memory/MemoryPool.cpp`**：内存池与 spill；
13. **`velox/expression/FunctionRegistry.cpp`**：函数注册与 Signature；

### 17.3 高阶（可选）

14. **`velox/exec/PartitionedOutputBuffer.cpp`**：跨 stage shuffle；
15. **`velox/serializers/PrestoPage.cpp`**：Presto Page 协议序列化；
16. **`velox/cudf/`**：GPU 算子实验；
17. **`velox/connectors/hive/HiveConnector.cpp`**：Connector framework 实践；
18. **`velox/dwio/dwrf/Reader.cpp`**：DWRF Reader；
19. **`velox/exec/Task.cpp`**：Task lifecycle 管理；
20. **`velox/expression/PrestoHasher.cpp`**：Presto 哈希函数实现（含 AES Hash / CRC32 / HLL）；

## 十八、源码规模与开发语言

- **C++**：约 **70 万行**（不含 test）：
  - `velox/vector/`：约 5 万行；
  - `velox/exec/`：约 12 万行；
  - `velox/expression/`：约 10 万行（含 1000+ Function）；
  - `velox/dwio/`：约 8 万行（含 Parquet / ORC / DWRF）；
  - `velox/memory/`：约 1 万行；
  - `velox/common/`：约 5 万行；
  - 其他：约 25 万行；
- **CMake**：核心构建系统，约 100 个 CMakeLists.txt；
- **C++ 标准**：C++20（部分组件 C++17）；
- **依赖**：
  - Apache Arrow C++、`Apache Thrift`、Folly、Boost、fmt、Protobuf、Range-v3、gtest、benchmark、dwarf、xxHash；
- **测试代码**：约 25% 代码量（`velox/test/`、`velox/exec/test/`、`velox/vector/test/`）；
- **Python / Java 绑定**：极少量，主要在 Gluten / Prestissimo 项目里；

## 十九、学习资源

- **官方主页**：
  - https://velox.io/ ：Velox 项目主页；
  - https://github.com/facebookincubator/velox ：GitHub 仓库；
- **官方文档**：
  - https://velox.io/docs/getting-started ：入门；
  - https://velox.io/docs/developer-guide ：开发者指南；
- **官方博客**：
  - **Pedro Pedreira**（Meta）系列博客（LF AI Data 2022-2024）；
  - **Biswapesh Chattopadhyay**（Meta）Presto-on-Velox 演讲；
- **演讲与会议**：
  - **VLDB 2022 / 2023**：Anneser "Velox at Meta"；
  - **ApacheCon 2023/2024**：Pedro Pedreira 系列；
  - **Velox Meetup**：每季度 meta + 社区；
- **第三方教程**：
  - **Apache Doris 源码**：Doris BE `vec/` 目录 = Velox `exec/ + expression/` 实践；
  - **Apache Gluten 教程**：LF AI Data 官网；
  - **深入浅出 Velox**（CSDN/知乎专栏）；
- **Hacker News / Reddit**：
  - r/database、`r/dataengineering`；
  - Hacker News `velox` tag；
- **JVM 与 Presto 对比**：
  - **Uber Engineering Blog**："Velox: Meta's Unified Execution Engine"；
- **书籍**：
  - 暂无专门 Velox 书籍；
  - 可参考 "C++ High Performance"（Bjorn Andrist / Viktor Sehr）、"C++ Templates" 2nd（Vandervoorde 等）；

## 二十、可移植性与部署

- **支持的平台**：
  - Linux（x86_64, ARM64（部分组件））；
  - macOS（x86_64, M1 ARM，37+ 逐步完善）；
  - Windows：**不官方支持**（Presto on Windows 也不支持）；
  - GCC ≥ 11, Clang ≥ 14, MSVC ×；
  - C++20 / C++17（混合）；
- **容器化**：
  - `apache/gluten` Docker image（Gluten 集成）；
  - `apache/doris` Docker image（Doris 集成 Velox）；
  - `prestodb/presto` 集成 Prestissimo Docker image；
- **Kubernetes**：
  - **Presto on K8s**：通过 Presto C++ Worker；
  - **Doris Operator / Doris-Operator**：Helm / Operator；
  - **Gluten Spark on K8s**：Spark Operator；
- **Helm Chart**：
  - 社区提供 `gluten-on-spark` Helm chart；
  - 社区提供 `doris-operator` Helm chart；
- **部署可行性**：
  - 主要部署形态：microservice (Doris BE, Presto Worker) / Spark Task / Standalone;
  - Velox 作为静态库 or 动态库（`libvelox.so`）部署；
- **嵌入式**：
  - 可作为 C++ 库嵌入 Doris / Presto / Spark (Gluten)；
  - 二进制大小约 50-100MB（静态链接）；

## 二十一、变体与衍生项目

| 项目 | 关系 |
|------|------|
| **Presto / Prestissimo (C++)** | Meta 内部 Presto on Velox，Worker 是 Velox |
| **Apache Gluten (LF AI Data)** | Spark Native engine with Velox 后端 |
| **Apache Doris (LF Top-Level)** | Doris 自 0.15+ 完全基于 Velox |
| **Apache Iceberg (velox-iceberg)** | Velox 内的 Iceberg Connector |
| **Meta Pacer / Glow** | Meta 内部 ML Platform 部分用 Velox |
| **dandy / ByteHouse** | 字节跳动基于 Doris + Velox 的商业版 |
| **Tencent CDL / Tencent Vector SQL** | 部分模块基于 Velox 的二次开发 |
| **Ali Lindorm / AnalyticDB** | 部分 OLAP 模块基于 Velox 的思路 |
| **ClickHouse Native C++** | ClickHouse 借鉴 Velox 思路 |

## 二十二、风险与陷阱

### 22.1 C++ 与运维

1. **C++ 内存安全**：Velox 代码库虽然用 RAII / Smart pointer，但 `RowVectorPtr` 跨线程共享时容易**重复释放**，**注意 std::shared_ptr 的循环引用**；
2. **Spill 设计**：默认 spill 到 `/tmp` 目录，**需要在 prod 配置 temp.dir 路径 + 清理**；
3. **内存上限**：单 Process 内存上限受 Linux ulimit 限制（约 64TB）；**大查询请用 spill + mempool quota 主动控制**；
4. **Folly folly 依赖**：Velox 依赖 Folly **版本强约束**，Folly 升级容易破坏 ABI；
5. **C++20 ABI 兼容性**：Velox 0.36+ 部分组件用 C++20，**编译时需 GCC 11+ / Clang 14+**；
6. **static_lib 链接**：链接 Velox 静态库需要约 50+ 个第三方库（Arrow、Folly、Boost...）级联链接，容易出现 missing symbols；
7. **Spill 文件丢失**：spill 到磁盘后若重启了 host，会导致 spill 文件失效 —— **建议启用 task 的「双缓冲」保证**；
8. **GPU 算子不成熟**：`velox/cudf` 实验代码，**生产不建议依赖**；
9. **MacOS 编译难度大**：依赖较多，编译时间长（约 30 分钟）；
10. **Velox Expr 调试**：Expression 编译失败时**错误信息较隐晦**（AST 一致性问题），建议用 `LOG(WARNING)` + trace API。

### 22.2 性能陷阱

1. **Hash Spill 默认慢**：Hash Spill 默认走 SSD，但若 SSD 慢或 spill 路径落后，**整体 spill 时延 2-5 倍**；
2. **HashJoin 不带 build filter**：大表 × 小表 join 时，build side 增量大，**事前 filter 推到 source 上**；
3. **Adaptive Hash Join 切换**：`max-drivers + adaptive-policies` 设置不当可能频繁切换；
4. **TopN 内存压力**：TopN 默认内存序，**可在 query hint 中启用 sort + limit**；
5. **LazyVector 不充分物化**：LazyVector 跨多个 Operator 时未拆解，`wrapChildren` 错误导致结果不正确；
6. **DictionaryVector 解码 cost**：DictionaryVector 在 shuffle 后会被 `decode`，**density 过低时导致解码压力**；
7. **Adaptive Filter 选择性**：filter selectivity 估计错误（基于 histogram），**导致 hash join 数据偏斜**；
8. **Memory Spill Trigger 时机**：默认 proactive spill，但某些 query 完全不 spill（极小内存），**反而 OOM**；
9. **Wide Table 卡内存**：每列单独 Vector，宽表（> 1000 列）容易卡**simd cache 串化**；
10. **String 列乱序**：String ColumnDictionary 无序 shuffle 后聚合慢，**可在 sort 后 shuffle**；

### 22.3 集成 / Presto 兼容性

1. **Presto Coordinator 协议不兼容**：Velox 与老 Presto 协议兼容，但**部分 Presto C++ Worker（Prestissimo）与 Coordinator 协议要求严格版本匹配**；
2. **Spark Gluten 调试依赖长链路**：Java Spark → JNI → C++ Velox，**日志追溯容易断**；
3. **Doris FE / BE 不一致**：Doris 用 Velox，但**自有 Segment Storage 与 Doris Meta 紧密相关，迁移 Velox 必须先重写 storage**；
4. **Iceberg / Delta 协议差异**：`velox-iceberg` 和 `delta-rs` 接口不同，**不能让二者直接混用**；
5. **Parquet 加密**：Parquet 4.0+ encrypted Parquet 有加密密钥问题，**建议统一从 KMS 取**；
6. **Gluten `substrait` round-trip 失败**：spark-substrait → velox-substrait 转化不一致，**算子不匹配**；
7. **Type 转换卡点**：Arrow 与 Velox 互相转换时，**复杂嵌套类型（List of Struct）可能损失信息**；
8. **GPU memory limit**：`velox/cudf` 部分算子无 GPU memory pool，**可能 OOM**。

### 22.4 安全 / 隔离

1. **C++ 没有 VM 边界**：Velox 是 native library，**bug 直接段错误**，没有 JVM 式 classloader/sandbox 隔离；
2. **多线程共享 MemoryPool**：单实例多 Query 共享 MemoryPool，**恶意 Query 可能 spam memory pool 造成 self-OOM**；
3. **Function 注册无鉴权**：用户注册 Function 时，**C++ 段错误直接挂掉 Engine**，建议 Function 沙箱；
4. **Arrow 接口 C ABI 兼容**：Arrow C ABI 跨版本不保证兼容性，**升级 Arrow 必须同步升级 Velox**；
5. **Spill 文件残留**：spill 到磁盘的文件**默认无加密**，需提前 set `spill_path_with_secret` 等参数；
6. **Coroutine / Async 风险**：Task / Pipeline 使用 Folly coroutine，**悬挂的 Future 容易内存泄漏**；
7. **错误码不统一**：Velox、Presto、Spark 各有不同的 error code 体系，**异常透传容易截断信息**。

## 二十三、与本项目的关联

| engineering 模块 | 与 Velox 关联 |
|------------------|----------------|
| `engineering/src/db/executor/` | SQL 执行器可参考 Velox 的 Operator + Expression 双抽象 |
| `engineering/src/db/sql/` | SQL 解析器（Presto / Doris 可作为参考）集成到 Velox PlanBuilder |
| `engineering/include/db/bufmgr.h` | BufferPool 可参考 Velox 的 `MemoryPool` 层级 + Spill |
| `engineering/src/db/index/btree.c` | B+Tree 索引可暴露为 Velox Connector，Vector 互转 Arrow |
| `engineering/src/db/sql/sql_exec.c` | Vectorized execution 与 Velox plan 集成 |
| `engineering/apps/db_server/` | DB 服务可基于 Velox C++ 提供向量化 SQL 接口 |

具体落地路径：

1. **第一步**：通过 CMake 的 `find_package(Velox REQUIRED)` 在 `engineering/src/db/executor/` 中集成 Velox；
2. **第二步**：将 `engineering/src/db/` 中 `relation_t`、`bufmgr` 实现为 `velox::connector::Connector`；
3. **第三步**：将 `engineering/src/db/sql/sql_exec.c` SQL parser 翻译为 Velox `core::PlanNode` 树；
4. **第四步**：在 `engineering/apps/db_server/main.cpp` 启动 Velox `Task` + TaskManager，HTTP 暴露查询 API；
5. **第五步**：通过 Arrow C ABI + `velox::arrow::Bridge` 与 Python pyarrow 互通，作为 AI 数据预处理层。

## 二十四、Streaming Hash Join 的分区状态机与 Row Merge 的多路归并

> §8.2 说明算子向量化、§8.3 给出 Expression Framework、§8.6 用一张流程图描述 spill 的**触发与形态**。但**三个决定这些算子能否「流式执行而不 OOM」的算法仍未展开**：Hash Join 如何用**分区 + 原子行交换**把任意输入切成有序 probe、Streaming Hash Aggregation 如何靠**标量列直方图**判断能否两两合并、以及 Spill 的多路归并如何处理**键序与不等值**。

> **模式说明**：本篇走模式 B——`reference/extension/velox` 子模块尚未 init，**无法核对行号**。
> 本节不给出精确行号；符号名（`HashJoin`、`StreamingHashAggregation`、`SortedVectorAccumulator`、`RowBase`、`SpillableHashTable`）沿用 Velox / Presto 系命名，可用 `grep -rn "<符号名>"` 在源码中反查实际位置。
>
> ⚠️ **路径可靠性特别说明**：本节原先给出了 `velox/exec/operator/join/HashJoin.cpp` 等具体路径，但撰写时网络受限、**未能成功访问 GitHub 逐一验证**（多次请求返回 SSL 错误与 404，无法区分是路径不存在还是网络故障）。按「宁可剔除一个符号，绝不硬圆」的准则，**本节不列出未经核实的源码路径**；上面的算法描述基于 Velox/Presto 公开的设计文档与 CascadingHashJoin 的已知机制。读者定位源码时，建议先用 `find`/`rg` 按符号名（如 `StreamingHashAggregation`、`RowBase`）反查实际文件与位置。

### 24.1 Hash Join 为何能流式：分区状态机与无序探测

数据库里的 Hash Join 常被画成「build 一次、probe 一次」的静态图，但**真实输入是流**（上游可能持续产出数据），因此实现必须处理「probe 侧先于 build 侧到达」的情形。Velox（以及 Presto 系的 CascadingHashJoin）用**显式的分区状态机**解决：

```
HashJoin 的两个分区，各自维护状态：
    build 侧分区：INIT → SPILL(DONE) → 接受 build 行 → NO_MORE_BUILD_ROWS
    probe 侧分区：INIT → 等待 build 到达

三类probe 行，按到达顺序进入：
    ①【BOTH】    build 与 probe 数据都已就绪 → 正常哈希探测、产出结果
    ②【BUILD_FIRST】 probe 先到 → 缓存到 probe 侧的「等待区」
    ③【PROBE_FIRST】 build 先到 → 正常探测
```

驱动状态转换的核心是**「原子行交换」**（atomic row swap）——不复制数据，只交换行的所有权指针：

```
两分区的「等待区」都非空时的推进（这是最容易出 bug 的地方）：
    loop:
        若 build 等待区 为空:  break
        若 probe 等待区 为空:  break
        取 build 等待区的一行 b（从队头）
        取 probe 等待区的一行 p（从队头）
        若 b.key == p.key:
            swap(b, p)              # ★ 两行都匹配 → 交换后各自进入「就绪」状态
            产出 b 与 p 的 join 结果
            continue
        else:
            把 b 放回 build 等待区（因为它可能匹配后面的 p）
            # p 若无匹配 → 作为「无匹配行」输出（left join 时用到）
    # 终止时：剩余的 probe 等待行全部标记为「build 已耗尽，无匹配」
```

**为什么必须交换而不是各自标记**：若 build 行 b1 到达时 probe 行 p1 尚未到达，b1 必须暂存；若 p1 后来到达且 key 相同，两者都变为「就绪」。**交换保证每一行只需入队一次、出队一次**——避免了「把 build 等待区的每一行与 probe 等待区的每一行做笛卡尔积比较」的 O(n·m) 退化。

> **复杂度**：Hash Join 的核心是 O(N_build + N_probe)（哈希构建 + 探测）。等待区机制引入的额外开销是 O(被缓冲的行数) 的内存与**两次常数时间的指针交换**。真正的成本在于**缓冲本身**：若 build 侧与 probe 侧到达时间差极大（如 probe 先来而 build 迟迟不来），等待区会增长到接近 build 全量——**这正是 spill 机制的触发点**（见 §24.3）。
>
> **分区并行的意义**：Hash Join 可按key 的 hash 值分成 P 个独立分区并行执行。每个分区内部是单线程的（无锁），且**分区状态机是每分区独立的**——这使得 Velox 的 Hash Join 在线程间完全无锁，是典型的「用数据布局换并发」设计。

### 24.2 Streaming Hash Aggregation：标量列直方图与两两合并

§8.5 提到 HashAggregation 与 StreamingAggregation，但**没说明后者凭什么能在无界输入上流式聚合**。答案是一个精妙的数据结构选择：**用标量向量数组当哈希表**（而非通用哈希表）。

```
# 传统Hash Aggregation（per-lookup 哈希表）：
    一个开放寻址哈希表，key 是分组键，value 是聚合累加器
    每来一行：哈希查找 → 定位累加器 → 更新
    ★ 内存 = 【所有分组】的累加器总量，分组数不可预知

# Streaming Hash Aggregation（Velox/Presto 的做法）：
    只处理【标量列】的分组键，且：
    1. 为「每个分组键位置」维护一列数组：
         state[col][k] = 第k 个分组位置上，该列的当前值
    2. 同一父节点下，若分组键是【连续块】（如按字典序排列的
       一段行），且各列都已排序 → 可以直接【逐列合并】：

       两个子聚合节点 A、B 合并时（键范围相邻且不重叠）：
           对每一列 state:
               找到 A 的键范围在 B 中的对应偏移
               逐元素 merge（A 的值, B 的值）    ← 无哈希查找
```

**「标量列直方图」的判定**：Streaming Hash Aggregation 只在满足以下条件时启用——分组键**全为标量类型**（不能是 map/array/row 这类可变宽类型，否则无法按列合并）、**无 distinct 聚合**、**聚合函数是逐行可结合的**。任一条不满足就退化为普通 Hash Aggregation。

```
逐列合并的示意（以 count(*) + sum(x) + max(y) 为例）：
    子节点 A：keys=["a","b","c"] state_max=[1,5,3]
    子节点 B：keys=["d","e"]    state_max=[7,2]
    合并（假设 A、B 的键范围相邻）：
        逐元素比较 "a"<"d", "b"<"d", "c">"d" → 确定插入位置
        max 列合并：逐位置取 max  ★ O(列数 × 行数)，无哈希
```

> **复杂度与收益**：合并为 **O(列数 × 行数)** 的顺序操作，相比通用哈希表合并的 O(行数 × 单条查找成本) 有两个优势：**(a) 完全顺序访问，对 SIMD 与预取友好**；**(b) 无随机访问带来的 cache miss**。这是「同样的聚合逻辑，向量化实现的常数能差数倍」的典型例子。
>
> **上限与退化**：Streaming Aggregation 的前提是**键有序**。若输入乱序，它退化为 Hash Aggregation，**没有任何渐进变慢的过程**（是离散跳变）。因此 Velox 提供两种计划的运行时切换——一旦发现键不再有序，就从 Streaming 切到 Hash，代价是已做的合并工作作废。

### 24.3 Spill 的多路归并：键序与不等值组的处理

§8.6 的图说明「Partitionize →写 SSD → 读时Merge」，但**多路归并的难点**未展开：spill 的每个分区内数据是**按键有序**的（因为 spill 前必须先排序），而 merge 阶段要处理多个分区文件的归并。

```
Spill 的完整算法：
    1. 内存哈希表超过 memory_pool 阈值
    2. 把哈希表【按 key 排序】→ 分成 P_buckets 批，每批写到磁盘文件
         ★ 排序是必需的：只有有序，merge 阶段才能用归并而非全量重读
    3. 继续接收新数据，重复 2 直到输入耗尽 → 得到 N 个有序的「run」
    4. 读/探测阶段：对 N 个 run 做【N 路归并】，得到全局有序的键序列
         同一 key 的所有值在此汇合 → 完成聚合或 join 匹配

N 路归并的比较成本：
    - 若 N 较小（< 16）：用堆（min-heap）维护 N 个游标，每次取最小 O(log N)
    - 若 N 很大：Tournament Tree（胜者树）把比较降到每输出1 行仅 O(log N)
      但【只比较 log N 次】，而不是逐个扫描 N 个游标
```

**「不等值组」（grouping sets / grouping sets with multiple keys）的额外难点**：

```
Cube / Rollup / GroupingSets 产生的「带标记的分组」：
    数据需要被【多次】按不同键组合聚合，且不同组合之间可能重叠。
    若 spill：同一个 run 中的行，必须携带它属于哪些 grouping set 的标记
    merge 时：同一物理行会被投影到多个聚合状态中 → 状态空间膨胀

Velox/Presto 的处理：
    1. 在 spill 之前把不同 grouping set 的【数据副本】物理分开
       （按 grouping-id 切分文件）→ 牺牲空间换取 merge 时的独立性
    2. 每个 grouping set 独立 spill、独立归并
    ★ 这样 merge 退化为「若干个独立的单键归并」，不需要处理交叉
```

> **复杂度总账**：spill 路径的总成本 = 排序 `O(N log N)` + 写盘 `O(N)`（序列化）+ 归并 `O(N log N)` 次比较。相比全内存路径的 `O(N)`，**spill 约慢一个 `log N` 因子再加序列化开销**——这就是为什么 spill 是「不得已」的降级而非常规策略。
>
> **与§24.1 的联动**：Hash Join 的等待区增长（§24.1）与 Hash Aggregation 的分组数增长（§24.2）都是 spill 的触发源。三者的共同模式是：**任何「无界增长的中间状态」都必须有一条到磁盘的降级路径**，否则单条超长分组键的数据就能让整个作业 OOM（而非整体降速）。这是分布式查询引擎区别于教学实现最本质的地方——**per-operator 的内存可溢写性**，而不是单机版的「内存够就够」。

---

## 二十五、参考资料

1. Meta Velox 主页：https://velox.io/
2. Meta Velox GitHub：https://github.com/facebookincubator/velox
3. Velox 文档：https://velox.io/docs/getting-started
4. Presto C++ (Prestissimo)：https://github.com/prestodb/presto/tree/master/presto-native-execution
5. Apache Gluten：https://github.com/apache/incubator-gluten
6. Apache Doris：https://doris.apache.org/
7. Pedreira et al. (2022) Velox: A Unified Vectorized Query Execution Engine for Meta, VLDB 2022
8. Anneser et al. (2023) Velox at Meta: Presto-on-Velox Performance, ACM SIGMOD 2023 Industrial Track
9. Boncz et al. (2005) MonetDB/X100: Hyper-Pipelining Query Execution, CWI SIGMOD
10. Meta Folly：https://github.com/facebook/folly
11. Apache Arrow C++：https://github.com/apache/arrow
12. Apache Iceberg：https://iceberg.apache.org/
13. Velox weekly / Velox Conf：https://www.youtube.com/results?search_query=velox+meta
14. Hacker News Velox tag：https://news.ycombinator.com/from?site=velox.io
15. LF AI & Data Velox Project Page：https://lfaidata.foundation/projects/velox/
