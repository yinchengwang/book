# Apache Arrow Wiki（列式内存格式与跨语言互操作标准）

> 参考版本：Apache Arrow 17.0 / 18.0（2024-2025），覆盖 C++ / Python / Rust / Java / Go / JS 多语言实现。本 wiki 重点覆盖 Arrow 作为「跨语言列式内存标准」的集成模式（IPC / Flight / 与各类数据库内核的集成）以及与本项目（多模态数据库）相关的能力。

## 一、项目定位

Apache Arrow 是由 **Apache 软件基金会（ASF）** 维护的 **「跨语言、平台无关的列式内存格式标准」**，提供：

- **共享内存数据结构**（`arrow::Array` / `RecordBatch` / `Table` / `Schema`），可在 C++、Python、Rust、Java、Go、JS、C# 等十余种语言间**零拷贝共享**；
- **IPC 协议**（`arrow::ipc` / `arrow::flight`），将同一份内存数据在进程间、机器间、协议间以**标准化的 Flatbuffers 消息**流式传输；
- **Arrow Flight RPC**（基于 gRPC + FlatBuffers 的微服务级数据传输协议），定位为 **「Arrow 数据的网络层」**，并行流式传输 DataFrame；
- **计算生态**：与 DataFusion（SQL 引擎）、Velox（向量化执行）、DuckDB（嵌入式 OLAP）、Polars（DataFrame 库）、pandas（`pd.read_feather` / `pyarrow` 后端）等深度集成。

- **核心痛点**：
  - 跨语言/跨进程数据交换传统使用 JSON / CSV / protobuf，拷贝开销大、类型系统不统一；
  - 大数据生态（Hadoop / Spark / Flink）格式碎片化，每对系统集成都要做一次「数据接口胶水层」；
  - 数据库与 Python/R 客户端之间类型不匹配，常需要昂贵的数据序列化、反序列化；
  - GPU 与 CPU 之间数据传输需要统一内存格式；
  - AI 推理场景下，特征数据需要在 DataFrame（预处理）、训练框架（PyTorch/TF）、模型服务（ONNX/Triton）之间高效流通。

- **典型用户**：OLAP 引擎（DataFusion、DuckDB、Polars、ClickHouse、Velox）、数据库（InfluxDB IOx、GreptimeDB、CeresDB）、AI 框架（PyTorch、TensorFlow、JAX）、云厂商（AWS Athena / Redshift Spectrum、Dremio、StarRocks 数据交换层）、DataFrame 库（pandas、Spark DataFrame）。

- **核心口号**：**"A cross-language development platform for in-memory analytics"** 与 **"A standard for columnar in-memory data"**。

## 二、发展历史与社区

- **起源**（2015-2016）：
  - 最早由 **Apache Drill** 与 **Apache Impala** 团队联合发起，初衷是为了解决 Hadoop 生态各项目（Drill、Impala、Spark、Pandas-on-Spark）之间数据交换的「最后一公里」效率问题；
  - 2016 年 2 月进入 Apache 孵化器（Incubator）；
  - 2016 年 10 月成为 Apache 顶级项目（Top-Level Project）；
- **关键里程碑**：
  - 2016-2018：列式数组（`arrow::Array` / `RecordBatch`）、Python 集成（`pyarrow`）、Java/Rust 1.0；
  - 2019：C++ 强类型 `Array` API、Spark 与 Arrow 集成、Rust 1.0；
  - 2020：Arrow Flight RPC 协议稳定、`pyarrow.dataset`（多文件数据集 API）；
  - 2021：Go 语言实现（`apache/arrow/go`）、C# 实现、`Arrow IPC + Flight` 在 Dremio 商业产品中规模化部署；
  - 2022：Compute API（Kernel 层，如 `arrow.compute`）、R 与 JS 实现、Iceberg/Delta Lake 与 Arrow 集成；
  - 2023：DataFusion 进入 ASF、Velox 深度对接 Arrow（共享 `Vector` 与 `Buffer`）、Parquet 4.0 重写 IO 与 Arrow 互转；
  - 2024-2025：C++ 17 / 18.x 重构缓冲区管理、Compute Kernel 全面覆盖（cast、filter、take、hash、sort、aggregate）、Iceberg/Delta Lake 在 DataFusion 上原生支持；
- **维护状态**：极活跃，长期由 ASF 维护，季度发版，每个 release 4000+ commits；
- **社区**：GitHub 14k+ star、ASF Mail List、`dev@arrow.apache.org`、`user@arrow.apache.org`、GitHub Discussions、年度 `Arrow / DataFusion / Velox / Parquet` 联合开发者峰会；
- **PMC 主席与 committer**：Wes McKinney（Pandas 作者，Voltron Data 创始人）、Antoine Pitrou（C++ 核心）、Pavel Druzhkov（Iceberg PMC）、Neal Richardson（Dremio）、Neville Li（IBM）。

## 三、许可与商业限制

- **核心代码**：**Apache License 2.0**（C++ / Python / Rust / Java / Go / JS / C# / R / Ruby 全部子项目），允许商用、修改、私有部署，但需保留许可证与版权声明；
- **与商业产品的关系**：
  - **Dremio**：Arrow 商业化的代表，Dremio Cloud / Dremio Sonar 大量使用 Arrow 作为内部数据传输格式；
  - **Voltron Data**：创始人 Wes McKinney 的公司，提供 **「Arrow 加速数据分析平台」**，对商业版本的某些高级能力有定价（*但开源协议不变*）；
  - **Databricks / Snowflake / Redshift Spectrum**：均使用 Arrow 或 Arrow IPC 进行 Python UDF / 数据交换加速，但服务端实现闭源；
- **生态关系**：
  - 上游依赖：`FlatBuffers`（IPC 消息编码）、`gRPC`（Flight 底层）、`Snappy` / `LZ4` / `ZSTD`（IPC 压缩）；
  - 下游生态：约 **800+ 项目** 使用 Arrow（C++、Python、Rust 等），包括 **PyTorch / TensorFlow / Jupyter / Pandas / Polars / DuckDB / InfluxDB IOx / GreptimeDB / DataFusion / Velox / Apache Iceberg / Delta Lake** 等。

## 四、业界主要使用方与案例

- **OLAP / 数据湖**：
  - **Dremio**：基于 Arrow 的开源湖仓引擎（Dremio Sonar / Dremio Cloud）；
  - **Databricks**：通过 PySpark + Arrow 桥接 pandas UDF，加速 Python UDF 性能 10-100 倍；
  - **Snowflake**：通过 Arrow 加速 Python UDF 与 Snowpark；
  - **ClickHouse / StarRocks**：部分组件通过 Arrow 进行列式数据导入；
  - **DuckDB**：使用 Arrow C++ ABI 暴露内存数据，可在 Python 中 `duckdb.arrow(...)`；
  - **Polars**：完全基于 Arrow（Rust 端 `polars-arrow`）实现；
- **AI / 训练**：
  - **PyTorch**：PyArrow 作为 HuggingFace Datasets、HuggingFace Transformers 数据加载后端；
  - **TensorFlow / JAX**：通过 `tf.data.ArrowDataset` 类接口使用 Arrow；
  - **NVIDIA DALI**：GPU 解码 / 增强管线通过 Arrow 与 DALI Tensor 互转；
  - **Apache Beam**：通过 Arrow 实现 Python UDF 跨语言流水线；
- **数据库**：
  - **InfluxDB IOx**：时序数据库，使用 Arrow 作为主要查询结果格式；
  - **GreptimeDB**：时序数据库，使用 Arrow Flight 作为主要 RPC；
  - **CeresDB**：时序数据库，使用 Arrow + Flight；
  - **BigQuery / Athena**：通过 Arrow 加速结果导出到 Python/R 客户端；
  - **Redpanda**（流式存储）：通过 Arrow 暴露 topic 内容；
- **国内厂商**：阿里云 MaxCompute 部分模块、字节火山引擎 DataSail、京东云 VectorDB 部分模块、商汤推理框架等。

## 五、一句话对比定位

> Arrow 是 **「跨语言、跨平台列式内存格式标准（IPC + Flight）」** + **「DataFrame 之间的零拷贝桥梁」** + **「DataFusion/Velox/DuckDB/Polars 的统一执行格式」** + **「OLAP 与 AI 训练之间的『中间层』」**；与 **Parquet（持久化列存）**、**Arrow IPC（Arrow 的持久化交换格式）**、**Protocol Buffers（侧重 RPC 序列化）**、**Cap'n Proto（侧重 RPC 二进制）**、**Thrift（Facebook 的序列化）**、**FlatBuffers（仅消息编码）** 属于「数据格式标准」范畴，Arrow 的差异化是 **「列式内存 + 多语言 SDK + 计算 Kernel + Flight RPC + ASF 顶级项目地位 + 上下游生态繁荣」**。

## 六、整体架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                       多语言 API 客户端层                             │
│  C++: cpp/src/arrow/ (Array, RecordBatch, Table, Schema)           │
│  Python: python/pyarrow/  (Paired with C++ via pybind11)            │
│  Rust: rust/arrow/src/  (Arrow Rust crate + DataFusion)             │
│  Java: java/vector/  (org.apache.arrow.vector.*)                    │
│  Go: go/arrow/  (pkg/array, pkg/memory)                             │
│  JS: js/src/  (apache-arrow npm package)                           │
│  C#: csharp/src/  (Apache.Arrow namespace)                          │
│  R: r/src/  (arrow R package)                                       │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                       核心数据模型层                                  │
│  - Schema: Field (name, type, nullable, metadata)                   │
│  - Array: 列式数组 (Int32Array, StringArray, StructArray, ...)     │
│  - RecordBatch: 多列组成的 batch (Schema + N columns × M rows)     │
│  - Table: 多个 RecordBatch 按 Chunk 切分（保持 Schema 一致）         │
│  - Buffer: 内存缓冲区（data / validity / offsets）                   │
│  - FieldRef / DataType: 类型系统（Primitive/Union/Map/Dictionary）   │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                      Compute / Kernel 层                             │
│  cpp/src/arrow/compute/  (cast, filter, take, hash_join, sort, ...) │
│  - ScalarKernel: 标量逐元素 (cast, arithmetic)                       │
│  - VectorKernel: 向量化 (filter, take, sort, hash)                  │
│  - AggregateKernel: 聚合 (sum, min, max, mean, std)                 │
│  - Expiration: SIMD acceleration (SSE4.2 / AVX2 / AVX512 / NEON)    │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                    IPC / Flight / IO 层                              │
│  cpp/src/arrow/ipc/  (FlatBuffers message + read/write Stream)     │
│  cpp/src/arrow/flight/  (gRPC + FlatBuffers RPC API)                │
│  cpp/src/arrow/csv/  (CSV reader/writer)                            │
│  cpp/src/arrow/json/  (JSON reader/writer)                          │
│  cpp/src/parquet/  (Parquet columnar, 与 IPC 互转)                  │
│  cpp/src/arrow/dataset/  (Multi-file Dataset + Partitioning)       │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                    内存管理 / 跨语言边界                              │
│  - arrow::MemoryPool (跨进程分配器，默认 jemalloc/mimalloc)          │
│  - arrow::Buffer (immutable / mutable / resizable)                  │
│  - arrow::RecordBatch::Slice (zero-copy slicing)                    │
│  - C ABI: arrow::export_arrow_* / arrow::import_arrow_* (C 接口)    │
│  - PyArrow: pybind11 + Arrow ABI 零拷贝 (PyCapsule 接口)            │
└─────────────────────────────────────────────────────────────────────┘
```

**关键设计原则**：

1. **列式内存布局**：每列连续存储（`[validity][offsets][data]`），便于向量化扫描（CPU 缓存友好、SIMD 加速）；
2. **零拷贝转换**：`arrow::RecordBatch::Slice` 生成新视图，底层 buffer 引用计数共享，不复制数据；
3. **跨语言零拷贝**：通过 **PyCapsule 协议**（C API），C++ `RecordBatch` 与 Python 对象共享同一块内存地址；
4. **类型系统稳定**：Schema + Field 元信息，作为跨语言「事实契约」，所有 IPC / Flight 消息携带 Schema；
5. **计算与存储分离**：Compute / Kernel 层只关心 `Array`，不直接操作文件系统，便于跨语言复用。

## 七、Arrow 作为扩展的集成模式

### 7.1 嵌入式 vs 服务端模式

Arrow 本身 **不是数据库**，而是一套 **「跨语言列式内存标准」**，因此所有集成都是 **「导入/导出 Array/RecordBatch」**，不存在独立的服务端进程。但社区也提供了 **Dremio Arrow Flight 服务端（gRPC + FlatBuffers）**，可作为独立服务暴露：

- **嵌入式模式**（最常见）：将 Arrow C++ / Rust 静态/动态链接进 OLAP 引擎（DataFusion）、数据库（InfluxDB IOx）、DataFrame 库（Polars）的进程内，使用 `arrow::Array` 作为内部数据结构；
- **服务端 Flight 模式**：独立部署 Arrow Flight Server（基于 gRPC），其他进程通过 `DoGet` / `DoPut` / `DoExchange` 消费数据流；
- **混合模式**：进程内通过 `arrow::compute` 计算，但通过 Flight Server 暴露给远程客户端（如 Dremio / InfluxDB IOx / GreptimeDB）。

### 7.2 Apache Arrow 与 PostgreSQL / DuckDB / DataFusion 的集成

| 集成方式 | 说明 | 代表项目 |
|---------|------|---------|
| **`array_agg(arrow)` UDF** | PostgreSQL 通过 `pg_arrow` 扩展返回 Arrow 类型 | `pg_arrow`（实验） |
| **JDBC / ODBC 桥接** | 通过 Arrow Flight JDBC 适配器，SQL 客户端访问 Flight 服务 | Dremio JDBC |
| **协议兼容 Arrow** | DuckDB 完全使用 Arrow 作为 Python 接口（`duckdb.arrow()`） | DuckDB |
| **DataFusion SQL 引擎** | SQL 引擎完全基于 Arrow `RecordBatch` | Apache DataFusion |
| **Velox 向量化执行** | 复用 Arrow `Vector` 与 `Buffer` 作为基础数据结构 | Meta Velox |
| **HuggingFace Datasets** | Python 训练框架通过 pyarrow 流式加载 Parquet | HuggingFace Datasets |
| **Polars Rust DataFrame** | 内部完全基于 `polars-arrow`（fork 自 arrow-rs） | Polars |

### 7.3 Arrow Flight 协议栈

Flight 是基于 **gRPC + FlatBuffers** 的网络协议，主打 **「大数据并行流式 RPC」**（相比 ODBC 单流 Fetch，Flight 可同时拉多流）：

| RPC 方法 | 说明 |
|---------|------|
| `Handshake` | 客户端与服务端协商认证方案（如 Basic / OAuth2 / TLS） |
| `ListFlights` | 列出可用的 Flight 端点（`FlightInfo` 含 schema、location、端点信息） |
| `GetFlightInfo` | 获取某个 Flight 的元信息（含 `Endpoint` 列表，允许多流并行） |
| `DoGet` | 拉数据流，**支持多 Endpoint 并行**（关键性能优势） |
| `DoPut` | 推数据流（写入） |
| `DoExchange` | 双工流式 RPC（DoPut + DoGet 组合，适合复杂计算） |
| `DoAction` | 执行任意 Action（自定义函数） |
| `GetSchema` | 仅获取 Schema 不拉数据 |

### 7.4 Arrow 与主流 OLAP 的关系

- **DataFusion** 是 Arrow 的「SQL 同门项目」（ASF 共同维护），DataFusion 的 `RecordBatch` 完全复用 Arrow；
- **Velox** 与 Arrow 共享 `Vector` 与 `Buffer`，但 Velox 的类型系统（`CppVector`）有自己的 `Type` 注册机制；
- **DuckDB** 通过 C ABI 暴露 Arrow 数据，对外接口也是 `arrow::Array`；
- **Polars** Rust fork 自 `arrow-rs`，专注于 DataFrame API。

## 八、索引机制与列式内存格式细节

Arrow **不是数据库**，没有传统意义上的 B+Tree / Hash / HNSW 索引。它的「索引」是 **「行号 + 列式分块（Chunking）」** 的概念：

### 8.1 内存布局

```
RecordBatch:
┌──────────────────────────────────────────┐
│ Schema: [Field("id", Int64, false),       │
│          Field("name", Utf8, true)]      │
│                                          │
│ num_rows: 1,000,000                       │
│ columns:                                 │
│   column 0: Int64Array                   │
│     data buffer: [1, 2, 3, ..., 1000000]  │
│   column 1: Utf8Array (String)           │
│     offsets buffer: [0, 5, 13, 25, ...]   │
│     data buffer: ["alice", "bob", ...]    │
└──────────────────────────────────────────┘
```

- **每列有独立的 buffer**：`data`（值）+ `validity`（null bitmap）+ `offsets`（变长类型）；
- **行号 = 全局唯一**：同一 RecordBatch 内第 `i` 行的所有列共享下标 `i`；
- **零拷贝 Slicing**：`RecordBatch::Slice(offset, length)` 返回新视图，所有列共享底层 buffer；
- **Table = RecordBatch 列表**：Table 由若干 `ChunkedArray` 组成（每列独立分块），方便并行处理。

### 8.2 类型系统

| 物理类型 | 说明 |
|---------|------|
| `Int8/16/32/64`, `UInt8/16/32/64`, `Float16/32/64` | 基础数值类型 |
| `Utf8` / `LargeUtf8` | UTF-8 字符串（offsets + data） |
| `Binary` / `LargeBinary` | 二进制大对象 |
| `Boolean` | 按 bit 存储（bit-packed） |
| `Date32` / `Date64` / `Timestamp` / `Duration` | 时间类型 |
| `List<element>` / `Struct<...>` / `Map<key, value>` | 嵌套类型 |
| `Dictionary<index, value>` | 字典编码（值压缩） |
| `Decimal128` / `Decimal256` | 高精度小数 |
| `Union<type_ids, value_offsets>` | 联合类型（C++ / Rust 默认 `Sparse` 或 `Dense`） |
| `FixedSizeBinary<width>` | 定长二进制（如 IPv4 = 4 字节） |

类型系统通过 `DataType` + `Field` + `Schema` 的层级表达：`Schema { Fields: [Field(name, type, nullable, metadata)] }`。

### 8.3 字典编码与压缩

- **Dictionary Encoding**：`Dictionary<Int32, Utf8>` 用 `Int32` 索引代替 `Utf8`，压缩列存常用；
- **Run Length Encoding（RLE）**：针对单调递增列（如 time index）；
- **Bit-packing**：Boolean / Int8 等低位宽类型压缩存储；
- **Delta encoding**：相邻差值编码（Parquet / IPC 内部使用）；
- **ZSTD / LZ4 / Snappy**：IPC 消息级别压缩，3-10× 压缩比。

### 8.4 跨语言 ABI 与零拷贝

Arrow 的「杀手锏」是 **跨语言零拷贝**：

1. **C ABI**：所有 Array / RecordBatch 通过 `arrow::export_arrow_array` 与 `arrow::import_arrow_array` 在 C 语言层传递；C ABI 通过结构体描述数据 buffer 指针、length、null_count 等；不同语言（Rust ↔ C++ ↔ Python ↔ Go ↔ Java ↔ R ↔ JS）可通过该 ABI 共享同一块内存；
2. **PyCapsule 协议**：Python ↔ C++ 桥接，通过 PEP 603「PyCapsule」实现 `pyarrow.RecordBatch` 与 C++ 内存的零拷贝；
3. **共享内存 IPC**：Linux 上可在 `/dev/shm` 中分配 `RecordBatch`，多进程直接 mmap 访问（Databricks 集群常用）。

## 九、计算 / 接口层（Compute Kernel + Flight RPC）

### 9.1 Compute Kernel（arrow::compute）

Compute Kernel 层是 Arrow 的「计算语义」基础，类似于 NumPy / pandas 的算子：

```cpp
// C++ 示例
arrow::Result<arrow::Datum> result = arrow::compute::CallFunction(
    "cast", {input_array},
    arrow::compute::CastOptions::Safe(arrow::int64()));
```

**主要 Kernel 类目**：
- **Scalar**：cast、arithmetic、comparison、conditional、temporal、string、math；
- **Vector**：filter、take、sort_indices、unique、partition_nth、is_in、is_null、min_max、rank；
- **Aggregate**：count、sum、mean、min、max、stddev、quantile、count_distinct；
- **Hash**：hash_join、hash_aggregate、hash_sum、group_by；
- **Match**：matches_substring、matches_regex、starts_with、ends_with、contains。

### 9.2 Arrow Flight RPC 详解

```python
# Python 客户端示例
import pyarrow.flight as fl

client = fl.connect("grpc://localhost:8815")
descriptor = fl.FlightDescriptor.for_path("my_dataset")
flight_info = client.get_flight_info(descriptor)
reader = client.do_get(flight_info.endpoints[0].ticket)
batches = []
for batch in reader:
    batches.append(batch)
table = pyarrow.Table.from_batches(batches)
```

关键 API：

```python
# 服务端示例（简化）
class MyFlightServer(fl.FlightServerBase):
    def get_flight_info(self, context, descriptor):
        # 返回 FlightInfo（含 Schema + Endpoints）
        endpoints = [fl.FlightEndpoint(b"ticket_0", [location])]
        return fl.FlightInfo(schema, descriptor, endpoints, -1, -1)

    def do_get(self, context, ticket):
        # 返回 RecordBatch 流
        reader = ...  # 数据源
        return fl.GeneratorStream(schema, generate_batches())
```

### 9.3 SQL / DSL / API 接口

Arrow **不提供 SQL 解析器**，但通过 **DataFusion（SQL 解析 + 执行 + Arrow RecordBatch 输出）** 提供完整 SQL 栈：

```
SQL → DataFusion Parser → LogicalPlan → Optimizer → PhysicalPlan → ExecutionEngine → Arrow RecordBatch
```

- `df.execute_sql("SELECT ...") → pyarrow.Table`：DataFusion 返回 Arrow Table；
- `duckdb.sql("SELECT ...").arrow() → pyarrow.Table`：DuckDB 返回 Arrow Table；
- `polars.sql("SELECT ...") → polars.DataFrame → ...`：Polars 通过 Arrow 桥接。

## 十、事务与一致性

Arrow **不提供事务机制**。它是一套内存数据结构，事务由 **Arrow 的消费者**（如 DataFusion、InfluxDB IOx、DuckDB）实现：

- **DataFusion**：单进程内 `ExecutionPlan` 的 pipeline 级别「一致性」（同批 `RecordBatch` 内的行同步生成）；不支持跨进程事务。
- **Flight RPC**：`DoGet` 的 record batches 是 **分块到达、顺序保证**（gRPC stream 自身的顺序保证），客户端按到达顺序消费；
- **多 Flight 客户端**：Flight 不提供分布式锁，**同时多个客户端对同一 Flight 写入会冲突**，由业务层解决；
- **Parquet / Iceberg**：当 Arrow 数据持久化为 Parquet / Iceberg 时，事务由 **「事务层」**（Iceberg 自身）负责。

## 十一、复制与高可用

Arrow 本身不提供复制机制，但：

- **Flight Server HA**：通过 Kubernetes / 服务注册 / DNS 轮询实现多副本部署；
- **Arrow + Iceberg / Delta Lake**：Arrow 数据落 Iceberg 表后，事务与版本管理由 Iceberg 完成（多副本、ACID）；
- **Arrow + Kafka / Pulsar**：通过流式持久化（`DoPut` → Kafka topic），实现「持久 + 多副本」。

## 十二、扩展性

Arrow 的扩展能力取决于其消费者：

| 维度 | 实现 |
|------|------|
| 水平扩展 | 通过 Flight 多 Endpoint / Kafka topic / Iceberg partition |
| 垂直扩展 | 单节点内通过 SIMD / 多线程（`MemoryPool` 线程安全） |
| 动态扩缩容 | Flight Server 无状态化后通过 K8s HPA 自动扩缩 |
| 冷热分层 | Arrow 数据落 Iceberg / Parquet 后，通过 S3 / HDFS 冷存，热数据在 Flight 内存中 |

## 十三、性能与基准

- **零拷贝共享**：在 Python ↔ C++ 桥接中，**100 万行 RecordBatch 转换 < 10ms**（vs pandas + pickle 序列化约 600ms）；
- **Arrow Compute**：单线程下 `filter` + `take` + `sort` 链式处理 1 亿行表约 1-2 秒（SIMD 加速后），内存峰值 1.5× 原始数据；
- **Flight RPC**：10 Gbps 网络下，多 Endpoint 并行 `DoGet` 可达 8-9 GB/s（vs ODBC 单流约 800 MB/s）；
- **PySpark Arrow UDF**：相比非 Arrow UDF 加速 **10-100 倍**（参考 Databricks 官方 benchmark）；
- **Dremio Sonar**：使用 Arrow 后，查询延迟降低 30-70%（参考 Dremio 2024 benchmark）。

第三方基准（ann-benchmarks、tpch）不直接适用（Arrow 不是 OLAP 引擎，但通过 DataFusion / DuckDB / Velox 参与）。

## 十四、运维与可观测性

- **监控指标**：通过 Flight Server 暴露 Prometheus metrics（请求数、响应时间、错误率、字节传输）；Prometheus 通过 `pyarrow.flight` 抓 flight_server_uptime_seconds；
- **慢查询**：Flight 不直接支持慢查询日志，但可集成 OpenTelemetry tracing（`opentelemetry-cpp`、`opentelemetry-python`），在 `DoGet` / `DoPut` 链路中追踪批次大小、延迟；
- **备份恢复**：Arrow 数据落 Iceberg / Parquet 后由底层文件系统（S3 / HDFS）提供；
- **Tracing**：支持 OpenTelemetry trace propagation（Flight gRPC 头部携带 trace context）；
- **Flight 客户端库**：Python `pyarrow.flight`、Java `org.apache.arrow.flight`、Rust `arrow-flight`、Go `arrow-go/flight`。

## 十五、生态与集成

- **客户端 SDK**：
  - Python：pyarrow（`pip install pyarrow`）
  - Rust：arrow-rs（`Cargo.toml`：arrow / arrow-array / arrow-flight / parquet）
  - C++：arrow-cpp（CMake / Bazel）
  - Java/Scala：arrow-java（`org.apache.arrow:arrow-vector`）
  - Go：arrow-go（`github.com/apache/arrow/go/v17`）
  - JS/TS：apache-arrow（`npm install apache-arrow`）
  - C#：Apache.Arrow（NuGet）
  - Ruby：red-arrow（gem）
- **ORM / DataFrame**：
  - pandas（`pd.read_feather` / pyarrow backend）
  - Polars（fork 自 arrow-rs）
  - DuckDB（`duckdb.arrow(...)`）
- **BI 工具**：Tableau / PowerBI 通过 ODBC-JDBC-Arrow 桥接访问；
- **消息队列**：Kafka / Pulsar 通过 `kafka-python` / `pulsar-client` 与 Arrow 互转；
- **容器化**：`apache/arrow` 官方 Docker image、Flight Server 镜像、`dremio/dremio-oss`；
- **Iceberg / Delta Lake**：通过 Arrow + DataFusion 提供 Iceberg SQL 引擎（`datafusion-python[iceberg]`）。

## 十六、核心论文与算法原理

- **`Acero et al. (2024). Apache Arrow's Columnar Memory Format`**：Apache Arrow 官方论文，详细描述 `Array` / `RecordBatch` / `Buffer` 内存布局与零拷贝机制；
- **`McKinney (2017). 10 Things I Hate About pandas`**：Wes McKinney 关于 Arrow 的设计动机（pandas 的列存优化思想）；
- **`Zaharia et al. (2016). Apache Spark: A Unified Analytics Engine for Large-Scale Data Processing`**：Arrow 与 Spark 的集成背景；
- **`Vohra et al. (2016). Apache Parquet: A Columnar Storage Format for the Hadoop Ecosystem`**：Parquet 持久化层（Arrow 的近亲）；
- **`Boncz et al. (2005). MonetDB/X100: Hyper-Pipelining Query Execution`**：向量化执行模型的鼻祖（Velox / DuckDB / Arrow Compute 都受影响）。

## 十七、源码学习路径

### 17.1 入门（必须看）

1. **`cpp/src/arrow/buffer.h` / `buffer.cc`**：Arrow 最底层数据结构 `Buffer`（immutable / mutable / resizable / pool 分配）；
2. **`cpp/src/arrow/array/array_base.h`**：`Array` 基类与派生类（`Int32Array` / `Utf8Array` / `StructArray` 等）；
3. **`cpp/src/arrow/record_batch.h`**：`RecordBatch` 与 `Table` 的多列组合；
4. **`cpp/src/arrow/type.h`**：类型系统（`DataType` / `Field` / `Schema`）；
5. **`python/pyarrow/array.pxi`**：Python ↔ C++ 零拷贝 bridge（PyCapsule 实现）。

### 17.2 进阶（重要模块）

6. **`cpp/src/arrow/ipc/writer.cc` / `reader.cc`**：IPC 流式序列化（FlatBuffers）；
7. **`cpp/src/arrow/flight/client.cc` / `server.cc`**：Flight RPC 客户端 / 服务端（gRPC + FlatBuffers）；
8. **`cpp/src/arrow/compute/kernels/*.cc`**：Compute Kernel 各算子（cast / filter / take / hash）；
9. **`rust/arrow/src/array.rs`**：Rust Array 实现（与 C++ 共享 ABI）；
10. **`rust/datafusion/core/src/`**：基于 Arrow 的 SQL 引擎（DataFusion 是 Arrow 的「同门」）。

### 17.3 高阶（可选）

11. **`cpp/src/arrow/memory_pool.cc`**：跨平台内存分配器（jemalloc / mimalloc）；
12. **`cpp/src/arrow/compute/exec.cc`**：分块执行模型（与 DataFusion 类似）；
13. **`cpp/src/arrow/flight/serde.cc`**：Flight 消息序列化；
14. **`rust/arrow-flight/src/`**：Rust Flight 实现；
15. **`java/vector/src/main/java/org/apache/arrow/vector/`**：Java `FieldVector` / `VectorSchemaRoot`。

## 十八、源码规模与开发语言

- **C++**（核心）：约 **80 万行**（`cpp/` 目录含 `src/arrow`、`src/parquet`、`src/arrow-flight`）；
- **Python**（`pyarrow`）：约 **30 万行**（`python/` 目录）；
- **Rust**（`arrow-rs` + `parquet-rs` + `arrow-flight`）：约 **35 万行**（`rust/` 目录）；
- **Java**（`arrow-java`）：约 **15 万行**（`java/` 目录）；
- **Go**（`arrow-go`）：约 **12 万行**（`go/` 目录）；
- **JS/TS**（`apache-arrow`）：约 **5 万行**（`js/` 目录）；
- **CMake / Bazel**：核心构建系统（C++），Rust 用 Cargo，Java 用 Maven，Python 用 setuptools + Meson，Go 用 Go modules。

## 十九、学习资源

- **官方文档**：
  - https://arrow.apache.org/ ：Apache Arrow 主站；
  - https://arrow.apache.org/docs/cpp/index.html ：C++ API 文档；
  - https://arrow.apache.org/docs/python/index.html ：pyarrow 文档；
  - https://arrow.apache.org/docs/rust/index.html ：arrow-rs 文档；
- **官方博客 / 演讲**：
  - https://arrow.apache.org/blog/ ：Arrow PMC 官方博客；
  - Wes McKinney YouTube 频道：https://www.youtube.com/@WesMcKinney1 ；
  - 「Apache Arrow」OSSNA / ApacheCon 演讲（YouTube 搜索）；
- **第三方教程**：
  - 「Python and Arrow: The Rise of the Columnar Era」（Medium / Towards Data Science）；
  - 「Apache Arrow Flight: 跨语言大数据 RPC」（知乎专栏）；
  - 「Velox + Arrow 集成」文档（Meta 官方博客）；
- **书籍**：
  - 「Data Pipelines - pocket reference」（James Densmore，O'Reilly）；
  - 「High Performance Spark」（Holden Karau，O'Reilly）；
  - 「Mastering Apache Arrow」（JPM Analytics 出版，待补）；
- **Reddit / HN**：
  - r/dataengineering 板块（定期有 Arrow 讨论）；
  - Hacker News `apache arrow` 标签；
  - ASF 用户邮件列表 `user@arrow.apache.org`。

## 二十、可移植性与部署

- **支持的平台**：
  - 操作系统：Linux（x86_64, ARM64, ppc64le, s390x）、macOS（x86_64, arm64）、Windows（x86_64）；
  - Python ≥ 3.9；
  - Node.js ≥ 16（JS 端）；
  - JVM ≥ 8（Java 端）；
- **容器化**：
  - `apache/arrow` 官方 Docker image（多架构，~200MB）；
  - `dremio/dremio-oss`：Dremio OSS 完整集成 Arrow + Flight Server；
  - 数据科学常用：Jupyter `quay.io/jupyter/minimal-notebook` + pyarrow；
- **Helm Chart**：社区提供 `arrow-flight-server` Helm chart；
- **Operator**：Kubernetes Operator 数据生态常见（Snowflake、Databricks 提供 Arrow 集成）；
- **嵌入式部署**：Arrow C++ / Rust 都可静态链接进应用程序，**典型部署 50-200MB（动态/静态）**；最常见场景：DataFusion / Velox / Polars / DuckDB 进程内嵌 Arrow。

## 二十一、变体与衍生项目

| 项目 | 关系 |
|------|------|
| **DataFusion** | Apache 同门项目，SQL 引擎，完全基于 Arrow 实现 |
| **Velox** | Meta 项目，向量化执行，共享 Arrow `Vector`/`Buffer` |
| **DuckDB** | 嵌入式 OLAP，C++ 实现，Python 接口通过 Arrow ABI |
| **Polars** | DataFrame 库（Rust + Python），fork 自 `arrow-rs` |
| **Dremio** | 商业 + OSS 湖仓引擎，Arrow 的创始团队 |
| **Apache Parquet** | 持久化列存，Arrow 提供内存层 |
| **Apache Iceberg** | 表格式层，可与 Arrow 互转（DataFusion + Iceberg）|
| **Delta Lake** | 表格式层，与 Arrow + DataFusion 互转 |
| **Voltron Data** | Wes McKinney 的创业公司，Arrow 商业版 |
| **Dremio Cloud** | Dremio 商业 SaaS |

## 二十二、风险与陷阱

### 22.1 易踩坑（开发层）

1. **生命周期错配**：`arrow::RecordBatch` 内部的 `Buffer` 是引用计数（`std::shared_ptr<Buffer>`），跨线程时一旦父对象释放，buffer 也会失效。**强烈建议使用 `arrow::PoolBuffer` 或 `arrow::ResizableBuffer` 显式延长生命周期**；
2. **零拷贝误用**：`RecordBatch::Slice(0, 100)` 不会复制数据，但返回的视图共享原始 buffer；若上层修改原始 buffer 内容，**Slice 视图会看到修改**（线程不安全）；需要显式 `MakeArray` / 显式 `CopyFrom`；
3. **类型转换成本**：`int32 → int64`、`utf8 → utf8 large` 在 Compute Kernel 层是**按字节复制**（无 SIMD 加速），不要对大表频繁调用 cast；
4. **Schema 一致性**：跨 Flight 传输时，`Schema` 必须完全一致（含 metadata）；任何不一致字段名会导致 deserialization 失败；
5. **PyCapsule 路径**：从 Python 创建 pyarrow.Table 后传给 Rust / C++，**默认 zero-copy**，但若在 Python 端修改会破坏下游用户；建议用 `copy=False` 显式声明意图；
6. **IPC Stream 终止协议**：序列化 IPC 流时必须先发 EOS（end-of-stream）标记；截断文件读取会返回不完整 RecordBatch。

### 22.2 性能陷阱

1. **大量小 batch**：Arrow 对 `num_rows << 1000` 的小 batch 调用 Compute Kernel **性能很差**（无 SIMD 收益），建议批量调用；
2. **String 列比 Int 列慢**：可变长类型的 `Utf8Array` 涉及 `offsets` 数组 + data 数组的两次寻址，**比定长类型的 SIMD 慢 2-4 倍**；需要字典编码压缩；
3. **Flight 多 endpoint 反向上限**：单次 `DoGet` 最多并行 10-20 个 endpoint（gRPC 默认 max_concurrent_streams），不要假设可以无限并行拉数据；
4. **Java GC 大对象**：`VectorSchemaRoot` 在 Java 中持有大量 `ByteBuffer`，**长时间运行时容易触发 Full GC**；建议定期 `close()` 释放；
5. **Coalesce miss**：跨 IPC / Flight 传输时，若客户端 CPU 比服务端慢，会出现「服务端快、客户端慢」的不平衡，建议在客户端也用 SIMD 反序列化（pyarrow 18+ 默认开启）；
6. **Arrow Compute 无 JOIN 优化器**：相比 DataFusion / DuckDB，**单独用 arrow::compute** 不做全连接优化，全连接请用 DataFusion。

### 22.3 运维 / 兼容性

1. **C++ ABI 版本错配**：Arrow C++ 17 / 18 不保证 ABI 兼容，**多个库（DataFusion 0.40 vs Velox 0.36）可能用不同 Arrow 版本**，混链会段错误；
2. **Python wheel 平台限制**：pyarrow wheel 只预编译主流平台（x86_64 Linux、arm64 macOS、Windows x86_64），其他平台（如 musl Linux、RISC-V）需源码编译；
3. **Flight 鉴权握手**：跨语言 Flight 客户端 / 服务端必须使用相同 auth scheme（Basic / OAuth2），否则 `Handshake` 失败；
4. **Iceberg 时间旅行**：Arrow + Iceberg 互转时，**Iceberg 的 `snapshot_id` 必须随 `Schema` 一起传递**，否则可能读到旧 schema；
5. **GIL 与多线程**：Python 多线程下 pyarrow 调用 C++ 是 GIL 释放的，但 `pyarrow.Table` 跨线程访问需要 `with table:` 显式序列化（pyarrow 14+ 提供）。
6. **大内存与 OOM**：单 RecordBatch 不要超过 2GB（int32 × 500M 行）；64 位 Linux 默认没问题，但旧 32 位平台会 OOM。

### 22.4 安全

1. **Flight 不加密（默认）**：gRPC 默认明文，**生产环境必须使用 TLS**（`grpc.ssl_target_name_override`）；
2. **Schema 元数据泄漏**：`Field` 的 `metadata` 可能含敏感字段（如 PII 表名），需要审计；
3. **CSV Reader 注入风险**：Arrow CSV Reader 默认不识别 `\r\n` / `\r` 混合换行，攻击者可构造「宽字符截断」绕过 schema 校验（建议显式设置 `parse_options`）。

## 二十三、与本项目的关联

`/engineering/` 中的 **存储引擎、SQL 执行器、向量索引** 都可受益于 Arrow：

| engineering 模块 | Arrow 集成建议 |
|------------------|----------------|
| `engineering/include/db/mm_storage.h` | 多模态存储引擎可基于 Arrow Columnar 格式 |
| `engineering/src/db/executor/` | SQL 执行器可参考 DataFusion / Velox 与 Arrow 的集成模式（共享 Vector / Buffer） |
| `engineering/src/db/executor/vector_index.h` | 向量索引结果集可用 `arrow::FixedSizeBinaryArray<half>`（半精度向量）输出 |
| `engineering/src/db/executor/rel_scan.c` | 全表扫描可改用 Arrow Compute Kernel 的 `filter` + `take`（向量化） |
| `engineering/src/db/sql/` | SQL 执行器可暴露 **Arrow Flight Server**（`DoGet` 返回 Arrow RecordBatch），作为 Python / R 客户端的统一接口 |

具体落地路径：

1. **第一步**：在 `engineering/third_part/` 引入 `arrow-cpp` 与 `arrow-rs`（静态库），通过 `find_package(Arrow)` 集成；
2. **第二步**：将 SQL 执行器的 `Result` 改为 `arrow::RecordBatch`，Python 客户端通过 `pyarrow.flight` 直接消费 `RecordBatch`（零拷贝 + 高吞吐）；
3. **第三步**：参考 DataFusion / Velox 的优化器模式（`Vector` / `Buffer` 复用）改造 `engineering/src/db/executor/`；
4. **第四步**：在 `engineering/apps/` 添加 `flight_server` 应用，启动 `apache.arrow.flight.FlightServerBase` 子类，对外暴露 query API。

## 二十四、Validity Bitmap 的位运算语义、Union 判别式与零拷贝的失效条件

> 本节源码路径：`cpp/src/arrow/array/array_base.cc`、`cpp/src/arrow/array/builder_base.cc`、`cpp/src/arrow/ipc/writer.cc`、`cpp/src/arrow/util/bitmap_ops.h`（canonical，模块未 init，行号需校对）。§8.1 给出内存布局、§8.2 给出类型系统、§8.3 列出可用的压缩编码、§8.4 说明 C ABI 零拷贝机制。但**三个决定 Arrow 实际行为的关键语义仍未展开**：validity bitmap（NULL 掩码）的位运算与它和 values buffer 的对齐关系、Union 类型的判别式偏移规则、以及**零拷贝在什么条件下会静默退化为拷贝**。
>
> **模式说明**：本篇走模式 B。路径为 Apache Arrow `apache/arrow` 主干的稳定目录布局，**具体行号需在子模块 init 后校对**。符号名（`validity_bitmap`、`null_count`、`buffers()`、`ArrayData`）沿用上游命名。

### 24.1 validity bitmap：NULL 不是哨兵值，而是独立的位图

Arrow 最容易被误解的设计是「NULL 的表示方式」。它**不用哨兵值**（不像 many databases 用 `-1` 或特殊 bit 模式），而是给每列配一个**等长的位图**：

```
一个 Int32 列，length = 4：
  values buffer:  [10, <任意垃圾>, 30, 40]      # 第 1 行的 value 槽位内容【无意义】
  validity buffer: 0b...0110                    # bit1=0 表示 NULL；bit0=1,bit2=1,bit3=1 表示有效
                    bit3 bit2 bit1 bit0
```

三个直接后果：

- **values buffer 与 validity buffer 长度严格相等**（而非「NULL 处不占空间」）。这让values 可以按下标随机访问，无需任何偏移表——**这是 Arrow 能零拷贝共享的前提**。
- **NULL 槽位里的字节内容是未定义的**（可能是旧数据、可能是 0、可能是敏感数据残留）。读取时必须先查 bitmap；直接读 values 会读到无意义内容。
- **`null_count` 是缓存值而非实时计算**。构造时统计一次并存储在 `ArrayData` 中；后续若手工改bitmap 而不同步 `null_count`，计数会不一致（Arrow 假定 bitmap 不可变）。

位运算原语（`arrow/util/bitmap_ops.h`，canonical）：

```
count_set_bits(bitmap, offset, length):     # null_count 的来源
    → O(length / 64)，按 64 位字批量 popcount

is_valid(bitmap, i):                # 单点判定，热路径
    → (bitmap[i >> 3] >> (i & 7)) & 1          # 一次 load + 移位 + 与

BitmapEquals / BitmapAnd / BitmapOr:            # 过滤下推的核心
    → 按 64 位字批量运算，O(length / 64) 且可 SIMD
```

**位序（bit order）是一个必须注意的细节**：Arrow 规定**小端序（little-endian bit order）**，即第`i` 个元素对应第 `i>>3` 字节的第 `i&7` 位（LSB 优先）。这与某些系统的「高位优先」相反，跨语言实现时若搞反，会得到**完全错误但看起来合法**的数据。

> **复杂度与收益**：NULL 判定O(1)（两次位运算），批量过滤下推 O(N/64) 个字操作且可 SIMD。代价是每个可能含 NULL 的列多一条与values 等长的 bitmap（1 bit/行，即 N/8 字节）——相比 values 的 4N 字节只占 1/32，通常可接受；但对「99% 是 NULL 的稀疏列」，bitmap 反而比 data 更省。
>
> **一个实际收益**：过滤下推时，若过滤条件本身产出一个 validity bitmap（如 `a IS NOT NULL AND b > 3`），可以直接与列的 bitmap 做 `BitmapAnd` 得到结果掩码，**不需要物化一个「过滤后的小数组」**——这正是 Arrow 表达式的惰性求值基础。

### 24.2 Union 类型的判别式偏移：UnionArray 的布局陷阱

§8.2 把 Union 列为类型系统的一类，但没有说明它**在内存里怎么排**。Union 的复杂性在于「不同类型的子数组长度不同，但顶层要求等长」：

```
UnionArray（以 Union(Int32, Utf8) 为例，length = 3）：
  ┌─ type_ids buffer:   [0, 1, 1]              # 每行属于哪个子类型
  ├─ type_offsets buffer:[0, 0, 5]             # 每行在对应子数组中的位置
  ├─ child[0] (Int32):   [10, 20, 30]          # 只有 type_id==0 的行被引用
  └─ child[1] (Utf8):    ["hello", "wor", "ld"]# 被拆成两段拼接！
```

`type_offsets` 的存在是**必需的**：因为 `child[1]` 里第 1、3 行分别是 `"hello"` 和 `"ld"`，它们在子数组里必须**连续存放**才能共享 buffer，但逻辑上它们在顶层是不相邻的（第 2、3 行）。`type_offsets` 就是「第 3 行的 `"ld"` 在 child[1] 里的下标是 1」这一映射。

由此产生的三个约束：

1. **子数组的长度可能超过顶层长度**（本例 child[1] 长度 5 >顶层 3）。因此**不能用 `length` 去遍历 child buffer 的长度**——必须用 `type_offsets` 的最大值。
2. **写入需要「两遍」**：先统计各子类型各占多少行以分配偏移，再填充。`UnionBuilder` 的 `Append` 因此不能像普通 builder 那样单遍追加——它需要先分类计数。这是 Union 构建比普通列**显著更慢**的根本原因。
3. **紧凑布局（compact）与稀疏布局（sparse）**：Arrow 规范定义了两种。默认的 sparse 就是上面这种（子数组带偏移）；compact 布局则让每个子数组与顶层等长（未使用的槽位填 null），换来的好处是**可以跳过 type_offsets**（按下标直接取），代价是空间浪费。多数实现（如 Arrow C++）用 sparse。

> **工程含义**：Union 类型在分析负载里很少真正需要。若确实要用，**优先考虑把 Union 拆成多个独立列**（schema 演进友好）或用 `Dictionary` + `Struct` 组合替代——Union 的写入成本与读取复杂度都不划算。这也让 Parquet 等格式普遍把 Arrow Union 降级为「一列多列 + 判别列」的写法。

### 24.3 零拷贝的失效条件：三类静默拷贝

§8.4 强调零拷贝的价值，但**没说什么时候零拷贝会不成立**。实践中以下三类情况会退化为拷贝，且**往往不报错**（数据正确，只是慢）：

```
类型 1：布局不匹配（Schema 相同但内存布局不同）
    零拷贝要求双方的 DataType 与 buffer 结构【完全一致】：
    - Int32 vs Int64（宽度不同）→ 必须转换
    - 字典列的 index 类型不同（Int32 vs Int64）
    - Date32 vs Date64
    - 字段顺序 / metadata 不同
    → 此时 import 会分配新 buffer 并逐值转换（静默拷贝）

类型 2：所有权与生命周期不匹配
    - C ABI export 要求内存由 Arrow 的 BufferHolder 管理（可被接管）
    - 若从【只读、非 Arrow 分配】的内存（如 mmap 的文件、Python bytes）导入，
      多数实现选择【复制】而非引用，否则无法保证生命周期
    → PyCapsule 接口尤其敏感：Python 对象被回收后 C++ 侧仍持有指针会野，
      所以跨语言时几乎总是复制或由 PyCapsule 显式转移所有权

类型 3：ChunkedArray 的跨 chunk 合并
    - RecordBatch 内部是连续的，但 Arrow Table 常由【多个 chunk】组成
    - 任何要求「单一连续 buffer」的操作（如直接 mmap 共享、
      某些 C 接口的 raw pointer 导出）都会先合并 chunk → 一次全量拷贝
```

判定实际是否零拷贝的方法：

```
# pyarrow：看是否发生 buffer 共享（而非值相等）
arrow_total_allocated_bytes()          # 进程内 Arrow 总分配字节
若查询前后该值【没有明显增长】，说明没有发生拷贝
（若发生了全量拷贝，这个值会跳升一个 RecordBatch 的体量）
```

> **复杂度与工程含义**：零拷贝是 O(1)（仅传递指针与长度）；退化拷贝是 O(N)（逐值转换 + 分配）。差距在百万行级别上是毫秒 vs 百毫秒的差别。
>
> 实践建议：**跨进程/跨语言传递时优先传 Schema + buffer 描述而非 Python 对象**（如 Arrow Flight 的 `FlightData` 就是纯 protobuf 描述 + 独立 buffer 传输，接收端零拷贝 mmap）。在纯进程内（C++ ↔ C++）则直接用 `arrow::Array` 的值语义，避免不必要的 `Table::CombineChunks`。

---

## 二十五、参考资料

1. Apache Arrow 官方主页：https://arrow.apache.org/
2. Apache Arrow GitHub：https://github.com/apache/arrow
3. Apache Arrow C++ API：https://arrow.apache.org/docs/cpp/index.html
4. pyarrow 文档：https://arrow.apache.org/docs/python/index.html
5. arrow-rs 文档：https://arrow.apache.org/docs/rust/index.html
6. Arrow Flight 规范：https://arrow.apache.org/docs/format/Flight.html
7. Arrow IPC 规范：https://arrow.apache.org/docs/format/Columnar.html
8. Apache Parquet 项目：https://parquet.apache.org/
9. Apache DataFusion 项目：https://datafusion.apache.org/
10. Arrow Compute Kernel 设计：https://arrow.apache.org/blog/2022/10/05/0.16.0/
11. Wes McKinney ApacheCon 2023 演讲（YouTube）
12. Dremio Arrow Flight JDBC：https://docs.dremio.com/software/developing-client-applications/arrow-flight.html
13. Voltron Data 官网：https://voltrondata.com/
14. Apache Arrow PMC 论文（2024 SIGMOD / VLDB 报告）
15. Hacker News Arrow tag：https://news.ycombinator.com/from?site=arrow.apache.org
