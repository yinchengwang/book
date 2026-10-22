# USearch Wiki（单文件 C++ 向量检索库）

> 参考版本：USearch 2.x（C++11 单头文件库，C++17 实现）。**单文件嵌入式 ANN 库**，主打 **头文件 + 依赖极少 + 多语言绑定 + 性能极佳**。本 wiki 重点覆盖向量类型特有章节（HNSW 实现、单头文件设计、多语言绑定、SIMD 优化）。

## 一、项目定位

USearch（**U**niversal **Search**）是由 **Unum Cloud**（Ash Vardanian 创立）开源的 **单文件 C++ 向量检索库**，定位类似 Faiss 的"算法库"，但**仅一个头文件 + 一个实现文件 + 极致 SIMD 优化 + 多语言绑定（Python/JS/Java/Go/Rust/C#/Swift）**。口号 **"Smaller and Faster than FAISS"**。

- **痛点**：
  - Faiss 编译复杂、依赖多、单语言（C++ + Python）、单算法（GPU 需额外编译）；
  - Hnswlib 仅 C++，无 Python 之外的现代语言绑定；
  - 嵌入式场景需要 **极小依赖 + 单头文件 + 高性能** 的库；
  - 多语言现代项目（Rust / Go / JS）需要原生 ANN 绑定；
- **典型用户**：AI/ML 工程师、嵌入式开发者、多语言项目、Edge AI、IoT；
- **核心口号**：**"Smaller and Faster than FAISS"** + **"Single-header C++ vector search"** + **"Multi-language bindings"**。

## 二、发展历史与社区

- **起源**：2022 年 Ash Vardanian 创建，初为 Unum Cloud 内部项目；
- **里程碑**：
  - 2022-11：v1.0（首个版本，C++11 + Python 绑定）；
  - 2023-04：v2.0 引入 **多语言绑定**（JS / Go / Rust / Java）；
  - 2023-09：v2.5 引入 **JIT / Dynamic Index Configuration**；
  - 2024-04：v2.10 引入 **量化（FP16 / BF16 / int8）**、**GPU 实验**；
  - 2024-09：v2.15 引入 **多距离度量**（Hamming / Jaccard / Cosine / L2 / IP / Haversine）；
  - 2025-04：v2.20 引入 **Index Serialization 跨语言兼容**、**GPU CUDA 优化**；
  - 2025-12：v2.x 持续优化 **SIMD + 多语言绑定**；
- **维护状态**：极活跃，Ash Vardanian 全职投入，月度版本，GitHub 3k+ star；
- **社区**：GitHub Discussions、Reddit r/cpp、r/MachineLearning、Discord。

## 三、许可与商业限制

- **核心代码**：**Apache License 2.0**；
- **商业版本**：
  - **Unum Cloud Platform**：托管向量搜索服务（USearch 商业化版本）；
- **生态关系**：
  - 与 **Hnswlib（Yury Malkov）** 同为单文件 HNSW 实现；
  - 与 **FAISS** 算法层面竞品；
  - 与 **DuckDB**（向量扩展）、**Chroma**（嵌入式后端）、**LanceDB** 集成；
  - 与 **LangChain / LlamaIndex** 是可选向量后端。

## 四、业界主要使用方与案例

- **大型用户**（公开）：
  - **Unum Cloud**：自家向量搜索平台；
  - **多家 Edge AI 公司**；
  - **DuckDB 向量扩展**（v0.10+ 实验集成）；
  - 国内：阿里、字节、网易等试点；
- **行业**：Edge AI、嵌入式 AI、RAG、多模态检索、推荐系统；
- **典型场景**：浏览器内 ANN（WASM）、移动端 ANN、嵌入式 RAG、IoT、多语言 AI 应用。

## 五、一句话对比定位

> USearch 是 **"Apache 2.0 + C++11 单头文件 + HNSW + 量化 + SIMD + 多语言绑定（10+ 语言）+ 极致性能"** 的嵌入式 ANN 库；与 **FAISS（算法库标准）**、**Hnswlib（单文件 HNSW）**、**Annoy（Spotify 树索引）**、**NGT（Yahoo JAPAN）**、**ScaNN（Google）**、**DiskANN（Microsoft）**、**Milvus（向量数据库）**、**Qdrant（向量数据库）** 构成 ANN 算法库与向量库两大阵营。USearch 的差异化是 **"单头文件 + 多语言绑定 + SIMD 极致优化 + 跨语言 Index 序列化"**。

## 六、整体架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                       用户代码（C++）                                 │
│  #include <usearch/index.hpp>                                       │
│  usearch::index_t index(...);                                       │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                  核心抽象：index_dense_t / index_t                    │
│  include/usearch/index.hpp                                           │
│  ├─ reserve(capacity)：预留容量                                    │
│  ├─ add(key, view)：添加向量                                       │
│  ├─ search(query, k)：Top-K 检索                                  │
│  ├─ remove(key)：删除                                              │
│  ├─ save(ostream)：序列化                                        │
│  ├─ load(istream)：反序列化                                      │
│  ├─ view(label)：按 key 访问向量                                  │
│  ├─ distance()：距离度量                                         │
│  └─ change_metric()：运行时切换距离                              │
└─────┬──────────────────┬────────────────────┬───────────────────────┘
      │                  │                    │
┌─────▼──────┐  ┌───────▼────────┐  ┌───────▼─────────┐
│ HNSW 算法 │  │  距离度量       │  │  量化层         │
│ index_hnsw│  │ cos / L2 / IP  │  │ FP32 / FP16    │
│ .hpp      │  │ / Hamming      │  │ / BF16 / int8  │
│ src/      │  │ / Jaccard      │  │ / bit          │
└────────────┘  └────────────────┘  └─────────────────┘
┌──────────────────────────────────────┐
│              工具与基础设施             │
│  - BitSet（visited 集合）            │
│  - serialization（二进制 I/O）       │
│  - joined_t（多线程迭代）            │
│  - concurrent_reads_t（并发读）      │
│  - result_pool_t（结果池）           │
└──────────────────────────────────────┘
┌──────────────────────────────────────┐
│            多语言绑定                   │
│  - Python: setup.py + pybind11       │
│  - JS/WASM: emscripten              │
│  - Java: JNI                         │
│  - Go: cgo                           │
│  - Rust: autocxx-bindgen            │
│  - C#: P/Invoke                      │
│  - Swift: Swift Package Manager     │
│  - WASM: Emscripten                  │
└──────────────────────────────────────┘
┌──────────────────────────────────────┐
│              距离计算底层              │
│  - SIMD 优化：SSE / AVX / AVX-512  │
│  - 距离：cos / L2 / IP / Hamming    │
│  - 硬件：CPU + GPU（CUDA 实验）     │
└──────────────────────────────────────┘
```

### 关键设计取舍

- **单头文件 + 单实现文件**：`usearch/index.hpp` + `cpp/*.cpp`；
- **C++11 兼容**：编译选项宽松（同时支持 C++11 / C++17 / C++20）；
- **极小依赖**：不依赖 Boost / TBB / OpenMP，可选依赖 Eigen / OpenMP；
- **HNSW 为核心**：单一索引算法（vs Faiss 几十种）；
- **SIMD 极致**：AVX-512 / AVX2 / SSE 路径编译时选择；
- **多语言绑定第一公民**：Python / JS / Java / Go / Rust / C# / Swift / WASM；
- **跨语言 Index 序列化**：Java 写入 → Python 读，反之亦然；
- **零拷贝视图**：`span<V> str_view` 风格避免内存拷贝。

## 七、存储引擎

### 7.1 单文件持久化

- **save(ostream) / load(istream)**：单文件二进制格式；
- **自定义二进制**：header + metadata + HNSW graph + vectors；
- **跨语言兼容**：所有绑定共享同一二进制格式；
- 源码：`include/usearch/index.hpp`, `cpp/*.cpp`。

### 7.2 内存存储

- **HNSW 图**：结构 + 向量同时存内存；
- **量化存储**：可选（FP16 / BF16 / int8 / bit）；
- **可选 mmap**：大索引可 mmap；
- 源码：`include/usearch/index_dense.hpp`, `cpp/index_dense.cpp`。

### 7.3 零拷贝视图

- **`span<T>`**：C++20 风格的 span，访问向量零拷贝；
- **`str_view`**：字符串视图；
- 避免序列化时的拷贝，提升性能。

## 八、索引机制（**向量类型核心**）

### 8.1 索引类型全景

USearch 提供 **两类索引**：

| 索引 | 文件 | 特点 |
|------|------|------|
| **`index_dense_t`** | `include/usearch/index_dense.hpp` | 稠密向量（FP32 / FP16 / BF16 / int8）|
| **`index_t`（稀疏 / 多模态）** | `include/usearch/index.hpp` | 自定义度量（任意 metric）|

**注**：2.x 起两个类合并。

### 8.2 HNSW 实现

USearch 的 HNSW 实现位于 `include/usearch/index_hnsw.hpp` + `cpp/index_hnsw.cpp`：

**核心结构**：
```
class index_dense_t {
    graph_t<...> graph_;            // HNSW 图
    index_meta_t meta_;             // 元数据（维度、度量等）
    buffer_t vectors_;              // 向量存储
    slots_t slots_;                 // key → slot 映射
};
```

**参数**：

| 参数 | 默认 | 含义 |
|------|------|------|
| `connectivity` | 16 | 每节点连接数（M）|
| `expansion_add` | 128 | 构建候选数（efConstruction）|
| `expansion_search` | 64 | 搜索候选数（efSearch）|

**距离度量**：
- **cosine**（默认，余弦相似度）；
- **L2sq**（欧氏距离平方）；
- **inner_product**（内积）；
- **hamming**（汉明距离，二值向量）；
- **jaccard**（杰卡德距离，集合）；
- **haversine**（球面距离，地理）；
- **pearson**（皮尔逊相关系数）；
- **cosine_f16**、**inner_product_bf16** 等量化变体。

**运行时切换度量**：
```cpp
index.change_metric(usearch::metric_cos_t{});
```
不需要重建索引！

### 8.3 HNSW 图存储

```
HNSW 图节点：
- neighbors[layer][i]：邻居 ID
- vector_：向量数据
- label_：外部 key
- visited_at_：访问时间戳（用于 Lock-Free 优化）
```

**图结构紧凑**：
- 使用 `std::vector<uint32_t>` 存储邻居；
- 每层独立 `parallel_slots_t`；
- **Lock-Free CAS**：无锁访问（提升并发性能）。

### 8.4 量化方案

| 量化 | 压缩比 | 精度 | 备注 |
|------|--------|------|------|
| **FP32**（默认） | 1/1 | 100% | 原始 |
| **FP16** | 1/2 | 99.9%+ | 半精度 |
| **BF16** | 1/2 | 99.9%+ | 脑浮点 |
| **int8** | 1/4 | 99%+ | 标量量化 |
| **bit（1-bit）** | 1/32 | 90%+ | 二值化 |

**USearch 量化实现位置**：`include/usearch/index_dense_quantization.hpp`（2.10+）。

### 8.5 SIMD 优化

USearch 的核心优势是 **极致 SIMD 优化**：

**距离计算 SIMD 路径**：
- **SSE2**：x86 基础；
- **SSE4.1**：进阶；
- **AVX2**：Hadamard 量化 + 距离计算；
- **AVX-512**：批量 PQ LUT 加速；
- **NEON**：ARM（Apple Silicon / AWS Graviton）；
- **WASM SIMD**：浏览器内。

**编译时选择**：
```cmake
target_compile_options(usearch PRIVATE -mavx2 -mfma)
```

源码：`include/usearch/internal/avx.hpp`、`include/usearch/internal/neon.hpp`。

### 8.6 多线程

- **OpenMP**（可选）：构建阶段并行；
- **C++ Threads**（默认）：查询阶段并行；
- **join_iterator**：并行搜索 + 合并；
- **atomic CAS**：无锁图操作。

### 8.7 GPU 实验

- **CUDA 路径**：v2.20+ 实验性支持；
- **GPU 距离计算**：`cpp/cuda/`；
- **当前状态**：CPU 实现成熟，GPU 实验中。

## 九、查询/计算/接口层

### 9.1 C++ 接口（核心）

```cpp
#include <usearch/index.hpp>

// 1. 创建索引
usearch::index_dense_t<usearch::metric_cos_t> index(
    usearch::metric_cos_t{},
    usearch::default_engine_config()
);

// 2. 预留容量
index.reserve(1'000'000);

// 3. 添加向量
std::vector<float> vec(128);
for (size_t i = 0; i < 1'000'000; ++i) {
    index.add(i, usearch::span_t<float>(vec.data(), 128));
}

// 4. 查询
std::vector<float> query(128);
auto results = index.search(query.data(), 10);
for (auto& [dist, id] : results) {
    std::cout << "id=" << id << " dist=" << dist << std::endl;
}

// 5. 持久化
std::ofstream out("index.usearch");
index.save(out);

// 6. 量化
index.change_metric(usearch::metric_cos_f16_t{});
```

### 9.2 Python 接口

```python
import usearch

# 1. 创建索引
index = usearch.Index(
    ndim=128,
    metric='cos',
    dtype='f32',
    connectivity=16,
    expansion_add=128,
    expansion_search=64,
)

# 2. 添加向量
import numpy as np
vectors = np.random.random((1_000_000, 128)).astype('f4')
keys = np.arange(1_000_000)
index.add(keys, vectors)

# 3. 查询
query = np.random.random((10, 128)).astype('f4')
matches = index.search(query, k=10)
print(matches.keys)    # [[id0, id1, ...], ...]
print(matches.distances)

# 5. 持久化
index.save("index.usearch")
index.load("index.usearch")

# 6. 跨语言兼容
index.save("index.usearch")  # Python
# Java / Go / Rust 可读取同一文件
```

### 9.3 JavaScript / WASM

```javascript
import { Index } from 'usearch';

// 浏览器内
const index = new Index({
    metric: 'cos',
    ndim: 128,
    connectivity: 16,
});
index.add(BigUint64Array.from([0, 1, 2, 3]), new Float32Array([...]));
const matches = index.search(new Float32Array([...]), 10);
```

### 9.4 多语言绑定

| 语言 | 路径 | 备注 |
|------|------|------|
| C++ | `include/`, `cpp/` | 原生 |
| Python | `setup.py`, `python/` | pybind11 |
| JavaScript | `javascript/` | NAPI / WASM |
| WASM | `wasm/` | Emscripten |
| Java | `java/` | JNI |
| Go | `golang/` | cgo |
| Rust | `rust/` | autocxx-bindgen |
| C# / .NET | `csharp/` | P/Invoke |
| Swift | `Package.swift` | SwiftPM |
| Ruby | `ruby/` | FFI |
| Dart / Flutter | `dart/` | FFI |

### 9.5 跨语言 Index 序列化

**杀手锏**：任何语言写入的 index，任何其他语言可读取！

```
Python: index.save("model.usearch")
Java:   index.load("model.usearch")  # 同一文件
Go:     index.Load("model.usearch")
Rust:   Index::load("model.usearch")
```

**适用场景**：训练用 Python，部署用 Java / Go / Rust。

### 9.6 高级 API

- **joined_iterator**：并发批量查询，结果合并；
- **concurrent_reads_t**：无锁并发读；
- **dynamic index**：运行时切换度量 / 量化；
- **exact search**：暴力 Top-K（用于 ground truth）。

## 十、事务与一致性

- **无事务概念**（算法库）；
- **线程安全**：
  - 多线程 **读** 安全；
  - **写** 需外部锁；
- **atomic CAS**：无锁图更新（部分操作）；
- **崩溃恢复**：基于 save/load 周期持久化。

## 十一、复制与高可用

- **算法库无 HA**；
- 多个进程需用户自管；
- **shared memory**：可通过 mmap 实现进程间共享。

## 十二、扩展性

- **垂直扩展**：增加内存 + CPU；
- **水平扩展**：
  - **分片**：用户自行 split index 到多个进程；
  - **跨节点**：通过网络 RPC 调用（用户自行实现）；
- **GPU 加速**：实验性 CUDA 路径。

## 十三、性能与基准

- **ann-benchmarks**：USearch 在 SIFT-1M / GloVe-100 / Deep-1B 上排名 **第 1 或前 3**；
- **构建速度**：百万级向量构建 5-10 秒（vs Hnswlib 20-30 秒）；
- **查询延迟**：百万级向量约 **0.5-3ms / query**（AVX-512）；
- **内存占用**：FP32 HNSW 约 50-80 bytes/dim；
- **量化收益**：FP16 / BF16 内存减半；int8 内存 1/4；
- **官方 benchmark**：https://github.com/unum-cloud/usearch/tree/main/benchmarks。

## 十四、运维与可观测性

- **无内置监控**；
- **benchmark 工具**：`BENCHMARKS.md` + 多个 benchmark 脚本；
- **可视化**：用户自行处理；
- **持久化**：单文件 `*.usearch`。

## 十五、生态与集成

- **DuckDB**：向量扩展（v0.10+）；
- **Chroma**：可用 USearch 作为向量后端；
- **LanceDB**：嵌入式后端选项；
- **LangChain**：Python / JS 绑定集成；
- **LlamaIndex**：Python / Go 绑定集成；
- **Unum Cloud Platform**：USearch 商业化；
- **多语言 SDK**：10+ 语言绑定。

## 十六、核心论文与算法原理

- **HNSW**：Malkov & Yashunin, 2016, IEEE TPAMI；
- **PQ**：Jégou et al., 2011；
- **SIMD 优化**：基础 SIMD 优化文献；
- **NEON SIMD**：ARM 文档；
- **Lock-Free 算法**：经典无锁算法文献。

## 十七、源码学习路径

| 优先级 | 模块 | 路径 | 说明 |
|--------|------|------|------|
| P0 | 入口 | `include/usearch/index.hpp` | 主头文件 |
| P0 | Dense Index | `include/usearch/index_dense.hpp` | 稠密向量索引 |
| P0 | HNSW | `include/usearch/index_hnsw.hpp`, `cpp/index_hnsw.cpp` | 图算法 |
| P1 | 度量 | `include/usearch/index_dense_metatypes.hpp` | 距离度量 |
| P1 | 量化 | `include/usearch/index_dense_quantization.hpp` | 量化方案 |
| P1 | SIMD | `include/usearch/internal/avx.hpp` | AVX 优化 |
| P2 | 多线程 | `include/usearch/index_join.hpp` | 并发搜索 |
| P2 | Python 绑定 | `python/` | pybind11 |
| P2 | WASM | `wasm/` | 浏览器内 ANN |

## 十八、源码规模与开发语言

- **C++**：约 60% LOC（核心算法 + 头文件）；
- **Python**：约 10% LOC（绑定 + 测试）；
- **C**：约 10% LOC（C API + 平台兼容）；
- **JavaScript / WASM**：约 5% LOC；
- **Java / Go / Rust / C#**：各约 2-5% LOC（绑定）；
- **构建系统**：CMake + setup.py + Cargo + Makefile；
- **CI**：GitHub Actions；
- **LOC 总量**：约 10 万行（含多语言绑定）。

## 十九、学习资源

- **官方文档**：https://unum-cloud.github.io/usearch/
- **GitHub**：https://github.com/unum-cloud/usearch
- **博客**：https://ashvardanian.com/tag/usearch/
- **论文**：HNSW 原始论文；
- **BENCHMARKS.md**：官方 benchmark；
- **Discord / Reddit**：r/cpp、r/MachineLearning；
- **YouTube**：Ash Vardanian 演讲。

## 二十、可移植性与部署

| 平台 | 支持 |
|------|------|
| Linux x86_64 | 原生 + AVX-512 |
| Linux ARM64 | 原生 + NEON |
| macOS x86_64 / ARM64 | 原生 |
| Windows x86_64 | 原生 |
| WASM / 浏览器 | Emscripten |
| Docker | 官方镜像 |
| PyPI | pip install usearch |
| npm | npm install usearch |

## 二十一、变体与衍生项目

- **Unum Cloud Platform**：USearch 商业化；
- **DuckDB 向量扩展**：集成 USearch；
- **多语言 SDK**：10+ 语言；
- **社区分叉**：少量自托管分支。

## 二十二、风险与陷阱

1. **单一索引算法**：只支持 HNSW（vs Faiss 几十种），复杂场景需其他库；
2. **无内置分布式**：需要用户自行分片；
3. **GPU 实验性**：CUDA 路径尚未成熟；
4. **算法库非数据库**：无持久化（save/load 仅单文件）、无并发管理、无远程 API；
5. **HNSW 大数据内存**：千万级向量 HNSW 内存占用 10-30GB；
6. **构建时间**：HNSW 构建仍需分钟级（百万级向量）；
7. **量化召回损失**：int8 / bit 召回率损失 5-15%；
8. **跨语言序列化限制**：版本升级时二进制格式可能不兼容；
9. **SIMD 编译选项**：需正确启用 SIMD 才能达到最佳性能；
10. **无 TLS / 鉴权**：远程调用需用户自行实现。

## 二十三、与本项目的关联

- **`engineering/src/index/`** 已实现 HNSW，可参考 USearch 的 SIMD 优化路径；
- **`engineering/include/algo/`** 中已有 SIMD 距离计算，可对照 USearch 的 AVX-512 实现优化；
- **学习价值**：单文件库设计 + 多语言绑定 + 极致 SIMD 优化；
- **MiniVecDB**：USearch 是嵌入式向量检索的标杆，可作为嵌入式模式的算法内核；
- **`learning/notes/`** 可沉淀 USearch 的 SIMD + 单头文件设计经验。

## 二十四、HNSW 图构建与邻居剪枝的算法细节

本节把 §八.2 的参数表展开成可复现的算法：层级采样公式、插入全流程、`refine_` 剪枝启发式与反向链接。所有行号对应 USearch **2.26.0**（`reference/vector/usearch/VERSION`）。

### 24.1 四个超参数与每节点字节预算

| 源码符号 | 论文符号 | 默认值 | 源码 |
|---|---|---|---|
| `connectivity` | `M` | 16 | `include/usearch/index.hpp:1611`（常量在 `include/usearch/index.hpp:1588`）|
| `connectivity_base` | `M0` | 32（= 2×M）| `include/usearch/index.hpp:1616` |
| `expansion_add` | `efConstruction` | 128 | `include/usearch/index.hpp:1593` |
| `expansion_search` | `ef` | 64 | `include/usearch/index.hpp:1598` |

`index_config_t::validate()`（`include/usearch/index.hpp:1625`）拒绝两类退化配置：`connectivity < 2` 会让索引退化成链表（rope），返回 `"Connectivity must be at least 2, otherwise the index degenerates into ropes"`（`include/usearch/index.hpp:1635`）；`connectivity_base < connectivity` 会让底层图比上层更稀疏，返回 `"Base layer should be at least as connected as the rest of the graph"`（`include/usearch/index.hpp:1637`）。

每节点的字节数在构造时预计算一次（`include/usearch/index.hpp:4172`）：

```
neighbors_bytes      = M  * sizeof(compressed_slot_t) + sizeof(neighbors_count_t)   // 上层每层
neighbors_base_bytes = M0 * sizeof(compressed_slot_t) + sizeof(neighbors_count_t)   // 底层
node_bytes(level)    = node_head_bytes_() + neighbors_base_bytes + neighbors_bytes * level
```

其中 `node_head_bytes_() = sizeof(vector_key_t) + sizeof(level_t)`（`include/usearch/index.hpp:2366`），`node_bytes_(level_t)` 见 `include/usearch/index.hpp:4183`。以 `compressed_slot_t = uint32`、`level_t = int16`、默认 M=16/M0=32 估算：底层节点约 `8 + 4 + 132 = 144` 字节，每多一层再加 `68` 字节。

### 24.2 层级生成：指数分布采样

USearch 沿用论文的经典采样（等价于 hnswlib 的实现）：

$$L = \left\lfloor -\ln U \cdot \frac{1}{\ln M} \right\rfloor, \qquad U \sim \mathrm{Uniform}(0,1)$$

源码只有两行（`include/usearch/index.hpp:4361`）：

```cpp
level_t choose_random_level_(std::default_random_engine& level_generator) const noexcept {
    std::uniform_real_distribution<double> distribution(0.0, 1.0);
    double r = -std::log(distribution(level_generator)) * pre_.inverse_log_connectivity;
    return (level_t)r;
}
```

倒数系数 `inverse_log_connectivity = 1.0 / std::log(connectivity)` 在构造时算好（`include/usearch/index.hpp:4174`），避免每次插入都取对数。取 M=16 时 `1/ln 16 ≈ 0.3607`，于是：

| 事件 | 概率 |
|---|---|
| `L ≥ 1`（节点进入上层图）| ≈ 0.361 |
| `L ≥ 2` | ≈ 0.130 |
| `L ≥ 3` | ≈ 0.047 |
| 平均层数 `1/(1 − 1/ln M)` | ≈ 1.56 |

这就是「上层稀疏、底层稠密」的成本结构：约 36% 的节点占用额外一层内存，而 64% 的节点只有底层——底层图每节点 2×M 条边（M0=32），上层每层 M 条边（M=16）。

### 24.3 插入全流程

`index_gt::add`（`include/usearch/index.hpp:3177`）的骨架（伪代码）：

```text
add(key, value):
    ctx = contexts[config.thread]                       # per-thread scratch，见 §二十六
    top_limit = max(max(M0, M) + 1, expansion_add)     # index.hpp:3204，多 1 个槽给剪枝启发式
    ctx.top.reserve(top_limit); ctx.next.reserve(expansion_add)

    lock(global_mutex_)                                 # 只保护 entry_slot_ / max_level_
        max_level_copy = max_level_; entry_slot_copy = entry_slot_
        new_level = choose_random_level_(ctx.level_generator)
    if new_level > max_level_copy: unlock(global_mutex_)   # 矮节点立刻放锁，index.hpp:3229

    slot = nodes_count_.fetch_add(1)                    # 无锁容量预留，超容量则回滚
    new_node = node_make_(key, new_level)               # tape arena 分配，见 §二十六

    if slot == 0: entry_slot_ = slot; max_level_ = new_level; return

    # 阶段一：从顶层贪心下降到 new_level
    closest = search_for_one_(value, entry_slot_copy, max_level_copy, new_level, ctx)

    # 阶段二：逐层做 ef 扩展搜索 + 双向连边
    for level in min(new_level, max_level_copy) down to 0:
        search_to_insert_(value, closest, level, expansion_add, ctx)     # index.hpp:4480
        lock(node_lock_(slot))                                           # 只锁新节点
            neighbors_(new_node, level).clear()
            picked = form_links_to_closest_(value, slot, level, ctx)      # index.hpp:4287
            closest = picked[0].slot
        form_reverse_links_(value, slot, picked, level, ctx)              # index.hpp:4310

    if new_level > max_level_copy: entry_slot_ = slot; max_level_ = new_level
```

**锁粒度的关键设计**：`global_mutex_` 只在读 `max_level_` / `entry_slot_` 快照与采样新层数时持有，且新节点层数不超过现最高层时**立刻释放**（`include/usearch/index.hpp:3231`）。其余全部靠 per-slot 分片锁（§二十六.3）。这是 USearch 能做到「多线程并发 add」而不串行化的原因。

### 24.4 底层扩展搜索 `search_to_insert_`

`include/usearch/index.hpp:4480`，等价于 HNSW 论文 Algorithm 1 的 SEARCH-LAYER：

```text
search_to_insert_(query, start, level, expansion, ctx):
    visits.clear(); next.clear(); top.clear()
    radius = d(query, start)
    next.push((-radius, start))          # 最大堆存 -distance → 等价最小堆，index.hpp:2387
    top.push(( radius, start))           # 有序缓冲，满 top_limit 自动挤掉最差，index.hpp:2386
    visits.insert(start)

    while next 非空:
        cand = next.top()
        if (-cand.distance) > radius and top.size() == top_limit: break   # 剪枝
        next.pop()
        for nxt in neighbors(cand.slot, level):
            if visits 已含 nxt: continue
            visits.insert(nxt)
            d = dist(query, nxt)
            if top.size() < top_limit or d < radius:
                next.push((-d, nxt))
                top.push((d, nxt), top_limit)      # 满了自动淘汰当前 top_limit 名之外
                radius = top.top().distance        # 半径随结果集收紧而单调下降
```

两个工程细节：
- `visits` 是 `growing_hash_set_gt<compressed_slot_t>`（`include/usearch/index.hpp:2370`），`clear()` 只重置逻辑大小不释放内存，配合 per-thread context 复用，避免每次查询向分配器要新块；
- 预留容量在最悲观假设下完成（`include/usearch/index.hpp:4493`）：`visits.reserve(connectivity_base + 1)`，主循环中每进一个节点还要 `visits.reserve(visits.size() + candidate_neighbors.size())`，失败即返回 OOM 而不是抛异常。

**复杂度**：每轮展开一个候选、至多 `M0` 个后继，距离计算量约 `O(ef · M0 · log ef)`（`log` 来自有序插入）。这与论文一致。

### 24.5 `refine_`：邻居剪枝启发式（HNSW 核心）

`include/usearch/index.hpp:4786`，与 hnswlib 的 `getNeighborsByHeuristic2` 同源。目标是**只保留彼此互相靠近的邻居**，从而避免「挤在一起的一堆节点互相霸占度数」。

```text
refine_(metric, needed, top, ctx, counter):
    if top.size() < needed: return top                    # 指数级早退：候选不够就跳过昂贵计算
    top.sort_ascending()                                  # 距离升序
    submitted = 1; consumed = 1                            # top[0] 必选，作为锚点
    while submitted < needed and consumed < top.size():
        cand = top[consumed]
        good = true
        for i in [0, submitted):
            if inter_dist(cand, top[i]) < cand.distance:  # 存在更近的已选邻居 → cand 是冗余的
                good = false; break                        # 提前退出
        counter += i                                       # 统计实际付出的 inter 距离
        if good: top[submitted++] = cand
        consumed++
    top.shrink(submitted)
    return top[0 .. submitted)
```

- `inter_dist(cand, submitted)` 是**两节点之间的互距离**，不是它们到 query 的距离。判据 `inter_dist < cand.distance` 意味着：cand 有一个已选邻居比 query 更靠近 cand，于是 cand 不给 cand 带来新信息，剪掉。
- 复杂度 `O(needed²)` 次距离计算。源码用 `refines_counter` 累加**实际执行的** inter 距离次数（而非最坏值），因为 `for` 循环带 `break` 会提前退出，这个计数才是真实成本。
- 早退分支 `if (top_count < needed) return {top_data, top_count}` 是关键优化：绝大多数节点层级为 0，候选数天然不足，直接跳过整个剪枝。

**一处比 hnswlib 更细的工程处理**：`update()` 场景下新向量还没 memcpy 进 `vectors_lookup_`，若直接读会拿到陈旧值。USearch 用 `override_slot` / `override_value` 参数把该槽位的读取替换掉（`include/usearch/index.hpp:4753`）：

```cpp
if (candidate.slot == override_slot)          return ctx.measure(override_value, citerator_at(submitted.slot), metric);
else if (submitted.slot == override_slot)     return ctx.measure(override_value, citerator_at(candidate.slot), metric);
else                                          return ctx.measure(citerator_at(candidate.slot), citerator_at(submitted.slot), metric);
```

配套还有一个 `std::nullptr_t` 重载（`include/usearch/index.hpp:4766`），在不需要 override 时避免实例化那个分支，保持 C++11 兼容。

### 24.6 反向链接与写放大

`form_reverse_links_`（`include/usearch/index.hpp:4310`）对新节点的每个邻居 `close_slot` 做三件事：

1. **取 per-slot 锁** `node_lock_(close_slot)`（`include/usearch/index.hpp:4322`），并断言 `close_header.size() <= connectivity_max` 防止溢出（`include/usearch/index.hpp:4330`）；
2. **去重**：若邻接表已含 `new_slot` 直接跳过（`include/usearch/index.hpp:4335`）；
3. **未饱和**（`size() < connectivity_max`）→ 直接 `push_back` 收下（`include/usearch/index.hpp:4341`）；**已饱和** → 走 `refine_` 重新选举（`include/usearch/index.hpp:4353`）：

```text
top_for_refine = { (d(value, close_slot), new_slot) }                  # 相对 query 的距离
              ∪ { (d(close_slot, succ)  , succ) for succ ∈ close_header }  # 互距离
top_view = refine_(metric, connectivity_max, top_for_refine, ctx, ..., override_slot=new_slot, override_value=value)
close_header.clear(); close_header.push_back(top_view[*].slot)
```

注意饱和分支构造的候选表**混用了两种距离**：`d(value, close_slot)` 是新节点到该邻居的距离，而 `d(close_slot, succ)` 是该邻居到它既有邻居的距离。这正是原论文启发式——用互距离判断哪些既有邻居该被挤掉。`connectivity_max` 在底层取 `connectivity_base`、上层取 `connectivity`（`include/usearch/index.hpp:4315`）。

**写放大**：一次 `add` 的距离计算总量约为 `O(ef · M0)`（搜索）`+ O(ef²)`（正向 refine）`+ O(ef · M0²)`（反向 refine），后者是主导项。源码因此单独统计 `computed_distances_in_reverse_refines`，与搜索、正向 refine 的计数分开（`include/usearch/index.hpp:2491`）。

## 二十五、标量量化与 SIMD 度量分派

§八.4 给了量化压缩比表，本节给出**转换公式、误差来源与运行时分派机制**。

### 25.1 `scalar_kind_t`：11 种标量的统一枚举

`include/usearch/index_plugins.hpp:139` 定义 `enum class scalar_kind_t : std::uint8_t`，稠密索引实际使用的分支为：

| kind | 位宽 | 字节宽 | 用途 |
|---|---|---|---|
| `f64_k` / `f32_k` | 64 / 32 | 8 / 4 | 原始精度 |
| `f16_k` / `bf16_k` | 16 | 2 | 半精度 / 脑浮点 |
| `e5m2_k` / `e4m3_k` | 8 | 1 | FP8（S×E×M）|
| `e3m2_k` / `e2m3_k` | 8 | 1 | FP6（S×E×M）|
| `i8_k` / `u8_k` | 8 | 1 | 有符号 / 无符号 8-bit |
| `b1x8_k` | 1（按位打包）| 0.125 | 二值向量（Hamming / Jaccard）|

两个宽度函数刻意区分「有效位」与「存储字长」：`bits_per_scalar()`（`include/usearch/index_plugins.hpp:250`）对 `b1x8_k` 返回 1，而 `bits_per_scalar_word()`（`include/usearch/index_plugins.hpp:279`）返回 8——前者用于算字节预算，后者用于算需要几趟扫描。每向量字节数由 `bytes_per_vector()` 给出（`include/usearch/index_plugins.hpp:3011`）：

```cpp
return divide_round_up<CHAR_BIT>(dimensions_ * bits_per_scalar(scalar_kind_));
```

### 25.2 转换函数表 `casts_punned_t`

所有标量两两转换被编译成一张**函数指针矩阵**。签名只有一条（`include/usearch/index_plugins.hpp:2149`）：

```cpp
using cast_punned_t = bool (*)(byte_t const*, std::size_t, byte_t*);   // 返回 false 表示"无需转换"
```

`casts_punned_t`（`include/usearch/index_plugins.hpp:2156`）持有 `from` / `to` 两组各 11 个 `cast_punned_t`，由 `make<scalar_at>()` 一次性填满（`include/usearch/index_plugins.hpp:2192`）——`result.from.f32 = &cast_gt<f32_t, scalar_at>::try_` 这样成对展开，共 121 个特化。取用靠 `operator[](scalar_kind_t)`（`include/usearch/index_plugins.hpp:2170`）。

这个设计带来一个副作用值得注意：调用方永远**不知道**自己的 `float*` 有没有被转换过，只知道 `vector_data` 指针可能被替换（`include/usearch/index_dense.hpp:2227`）：

```cpp
bool casted = cast(vector_data, dimensions(), casted_data);
if (casted) vector_data = casted_data;      // 同类型转换的特化直接 return false，零拷贝
```

`cast_gt<f32_t, f32_t>` 等同类型特化（`include/usearch/index_plugins.hpp:1899`）直接返回 `false`，所以 f32 → f32 索引是**真正零拷贝**的。

### 25.3 i8 / u8 量化：逐向量 L2 范数对称缩放

`cast_to_i8_gt`（`include/usearch/index_plugins.hpp:1960`）：

```cpp
magnitude = sqrt(Σ_i v_i²)                                  // 逐向量 L2 范数
if (!(magnitude > 0)) { 全填 0; return true; }               // 顺带捕获 NaN，避免 cast-to-int UB
q_i = static_cast<int8_t>(clamp(v_i * 127.0 / magnitude, -127.0, 127.0));
```

反量化 `cast_from_i8_gt`（`include/usearch/index_plugins.hpp:1986`）就是 `v_i = q_i / 127.0f`。`u8` 版本同构，量程改成 `[0, 255]`（`include/usearch/index_plugins.hpp:1996`），反量化除 255（`include/usearch/index_plugins.hpp:2017`）。

**这是对称量化，不是非对称量化**。源码注释写得很直白（`include/usearch/index_plugins.hpp:1964`）：先估量级再缩放，而不是直接 clamp；并明确警告该做法假设距离度量是 dot-product 类，对 L2 不成立，所以不建议自动把浮点转整数。

误差分析：量化区间 `[-127, 127]` 被 254 个格点均分，步长 $\Delta = \frac{2}{254} \approx \frac{1}{127} \approx 0.787\%$，取整误差上界 $\le \Delta/2 \approx 0.394\%$（相对满量程）。但**对余弦距离的影响不是 0.394%**——因为量化前先做了 L2 归一化缩放，i8 内积恰好近似余弦值：$\hat{q}\cdot\hat{q} \approx (v/\|v\|_2)\cdot(w/\|w\|_2)$，误差主要来自每维独立取整，累积到 $d$ 维上的相对偏差约为 $\frac{0.00394}{\sqrt{3}} \cdot \sqrt{d}$ 量级。这就是 §二十二第 7 条「int8 召回率损失 5~15%」的来源。

若要真正的非对称量化（逐维 `min/max` + 每维 scale/zero-point），需要查询时用原始 scale 还原——USearch **没有**做这件事，它把 scale 隐含在「每个向量各自归一化」里，代价是不同向量之间不能直接比较绝对范数信息。

### 25.4 低位宽浮点：查表法上转

FP8/FP6 的上转不走位运算，而是查表：

- `e5m2_to_f32`（`include/usearch/index_plugins.hpp:701`）：128 项 `uint32_t` LUT 覆盖 7-bit 量级，符号单独处理 —— `conv.u = sign | lut[u8 & 0x7F]`（`include/usearch/index_plugins.hpp:744`），表尾三项是 `inf / nan / nan`；
- `e2m3_to_f32`（`include/usearch/index_plugins.hpp:1053`）：64 项 LUT **包含符号**，`conv.u = lut[u8 & 0x3F]`（`include/usearch/index_plugins.hpp:1070`）；
- `e4m3_to_f32`（`include/usearch/index_plugins.hpp:824`）与 `e3m2_to_f32`（`include/usearch/index_plugins.hpp:1140`）同构，各 64 项。

一次 4 字节的 L1 命中替代了「拆符号 → 减偏置 → 规格化 → 补尾数 → 组装」的五步整数运算，在标量回退路径上明显更快。存储侧则统一走 `cast_through_f32_gt` 模板（`include/usearch/index_plugins.hpp:2062` 等），即先升到 f32 再降，避免为每对 FP8 变体手写位运算。

### 25.5 `metric_punned_t`：运行时函数指针分派

这是 USearch 与 Faiss 最本质的架构分歧点。

`metric_punned_t`（`include/usearch/index_plugins.hpp:2856`）只存三样东西（`include/usearch/index_plugins.hpp:2871`）：

```cpp
using metric_routed_t = result_t (metric_punned_t::*)(uptr_t, uptr_t) const;   // 成员函数指针
metric_routed_t metric_routed_ = nullptr;
uptr_t metric_ptr_ = 0;              // 被调函数地址（伪装成整数以保证 trivially-copyable）
uptr_t metric_third_arg_ = 0;        // 维度数；b1x8 时是位数而非字节数
```

调用路径（`include/usearch/index_plugins.hpp:2891`）：

```cpp
inline result_t operator()(byte_t const* a, byte_t const* b) const noexcept {
    return (this->*metric_routed_)(reinterpret_cast<uptr_t>(a), reinterpret_cast<uptr_t>(b));
}
```

而 `invoke_array_array_third`（`include/usearch/index_plugins.hpp:3098`）再把 `metric_ptr_` 转回函数指针调用一次：

```cpp
auto function_pointer = (metric_array_array_size_t)(metric_ptr_);
return function_pointer(a, b, metric_third_arg_);
```

**代价**：每次距离计算 2 次间接跳转（成员函数指针 + 普通函数指针）。**收益**：同一份二进制里可以承载任意 `(metric_kind, scalar_kind)` 组合，且序列化文件里的度量字段可被任意语言的绑定解释——这是 §九.5「跨语言 Index 序列化」能成立的架构前提。

构造流程 `builtin()`（`include/usearch/index_plugins.hpp:2916`）先设默认路由，再按可用后端择优：

```cpp
metric.metric_third_arg_ = (scalar_kind == b1x8_k) ? divide_round_up<CHAR_BIT>(dimensions) : dimensions;
if (!metric.configure_with_numkong())      // 优先 NumKong 汇编 kernel
    metric.configure_with_autovec();       // 回退到编译期模板 + 编译器自动向量化
```

**`configure_with_numkong()`**（`include/usearch/index_plugins.hpp:3045`）把 USearch 的枚举翻译成 NumKong 的 kernel 枚举（`include/usearch/index_plugins.hpp:3048`）：

| `metric_kind_t` | `nk_kernel_kind_t` |
|---|---|
| `ip_k` | `nk_kernel_dot_k` |
| `cos_k` | `nk_kernel_angular_k` |
| `l2sq_k` | `nk_kernel_sqeuclidean_k` |
| `hamming_k` | `nk_kernel_hamming_k` |
| `tanimoto_k` / `jaccard_k` | `nk_kernel_jaccard_k` |

`nk_find_kernel_punned` 选出当前 CPU 上最强的可用实现，一个细节是它会按**输出类型**再选一次路由模板（`include/usearch/index_plugins.hpp:3066`）：i8 × dot 的输出是 `int32`、i8 × l2sq 是 `uint32`、i8 × cos 是 `float`，于是 `metric_routed_` 被绑定到 `numkong_routed<int32_t, true>` 之类的特化（`include/usearch/index_plugins.hpp:3040`），把「结果要不要取负」和累加器宽度都固化进类型，避免每次调用再分支。另外 b1x8 的第三参数必须传**位数**（`include/usearch/index_plugins.hpp:3087`），因为 Hamming/Jaccard 的 kernel 数的是 bit 而不是 byte。

**`configure_with_autovec()`**（`include/usearch/index_plugins.hpp:3108`）是回退路径：`metric_kind` 外层 switch × `scalar_kind` 内层 switch，把 `metric_ptr_` 指向 `equidimensional_<metric_cos_gt<f16_t, f32_t>>` 这类模板实例。`equidimensional_`（`include/usearch/index_plugins.hpp:3227`）只做一次指针转换然后转调模板对象，真正的向量化交给编译器。以 `metric_cos_gt`（`include/usearch/index_plugins.hpp:2277`）为例：

```cpp
result_t ab{}, a2{}, b2{};
#if USEARCH_USE_OPENMP
#pragma omp simd reduction(+ : ab, a2, b2)
#elif defined(USEARCH_DEFINED_CLANG)
#pragma clang loop vectorize(enable)
#elif defined(USEARCH_DEFINED_GCC)
#pragma GCC ivdep
#endif
for (std::size_t i = 0; i != dim; ++i) { ab += ai*bi; a2 += ai*ai; b2 += bi*bi; }
// 2×2 分支表处理零向量，避免 0/0
result_t result_if_zero[2][2] = {{1 - ab/(sqrt(a2)*sqrt(b2)), 1}, {1, 0}};
return result_if_zero[a2 == 0][b2 == 0];
```

**一次扫描同时累加 `ab` / `a2` / `b2` 三个约化量**（靠 `reduction(+ : ab, a2, b2)`），比朴素写法少两趟内存。末尾用 `[a2==0][b2==0]` 索引 2×2 表处理零向量而非 `if` 分支，是为了避开编译器在热循环里生成预测失败的分支——这是与 Faiss `MetricComputer` 系列一致的 SIMD 友好写法。

### 25.6 SIMD 后端的真实来源（事实纠正）

本 wiki 对应的是 **2.26.0**，其 SIMD 事实与早期版本不同：

- `USEARCH_USE_NUMKONG` **默认为 0**（`include/usearch/index_plugins.hpp:25`），全部汇编 kernel 来自 `numkong/` 子模块；
- `hardware_acceleration_available()` / `hardware_acceleration_compiled()`（`include/usearch/index_plugins.hpp:2819`、`include/usearch/index_plugins.hpp:2834`）分别返回「运行时可用」与「编译期内置」的 ISA 列表；未启用 NumKong 时两者都退化为常量 `"serial"`（`include/usearch/index_plugins.hpp:2846`）；
- 这两个函数经 pybind 暴露给 Python（`python/lib.cpp:1242`），可在运行时查询当前二进制到底编进去了哪些 ISA；
- ⚠️ 早期 2.x 曾在头文件里内嵌 **simdjson 的 half 转换**代码。当前版本已彻底移除——`grep -rn simdjson include/usearch/` 无任何命中，取而代之的是 §25.4 的 LUT 方案与 §25.5 的 NumKong 委派。阅读旧资料时不要沿用这条说法。

### 25.7 量化后的重打分：只有暴力全扫一种

`search(..., bool exact = false)` 的 `exact` 参数（`include/usearch/index_dense.hpp:884`）在底层对应 `index_search_config_t::exact`（`include/usearch/index.hpp:1718`）。为真时**不走图**，直接 `search_exact_`（`include/usearch/index.hpp:4730`）线性扫全部槽位：

```cpp
for (std::size_t i = 0; i != size(); ++i) {
    auto slot = static_cast<compressed_slot_t>(i);
    if (!is_dummy<predicate_at>() && !predicate(at(slot))) continue;
    distance_t distance = ctx.measure(query, citerator_at(slot), metric);
    top.insert(candidate_t{distance, slot}, count);
}
```

所以 USearch **没有** Faiss 式的「PQ 粗排 + 原始向量重排（rescore）」两级检索。量化误差无法在检索后修正——唯一的补救是先用全精度索引算 ground truth，或干脆用 `exact=True` 暴力搜。这是「单文件 + 极致性能」设计取向的代价：省下的代码体积换走了召回率上的一个档次。

## 二十六、内存布局、并发控制与过滤

§八.3 与 §八.6 只给了粗粒度描述，本节拆到字节与锁。

### 26.1 节点存储：连续 tape + 非对齐引用

USearch 的图**不是**「每节点一个 `std::vector`」的经典布局（hnswlib 的做法），而是把整张图铺进一条可增长的 tape 分配器。`node_t`（`include/usearch/index.hpp:2397`）只有一个成员：

```cpp
byte_t* tape_{};                                                  // index.hpp:2398
byte_t* neighbors_tape() const noexcept { return tape_ + node_head_bytes_(); }
misaligned_ref_gt<vector_key_t const> ckey() const noexcept { return {tape_}; }
misaligned_ref_gt<level_t> level() noexcept { return {tape_ + sizeof(vector_key_t)}; }
```

节点头部是 `[key][level]`，邻接表紧跟在头部之后按层线性排列（第 0 层用 `M0` 条，之后每层 `M` 条）。

`misaligned_ref_gt`（`include/usearch/index.hpp:309`）是这里最精巧的一处：由于邻居数组长度可变，不同节点的邻接表起点**不保证按 4 字节对齐**，直接 `reinterpret_cast` 解引用在 C++ 里是 UB。USearch 的做法是把裸地址包成引用代理，只在 `operator mutable_t` 里调 `misaligned_load`（memcpy 语义）：

```cpp
template <typename at> class misaligned_ref_gt {
    byte_t* ptr_;
    operator mutable_t() const noexcept { return misaligned_load<mutable_t>(ptr_); }
    misaligned_ref_gt& operator=(mutable_t const& v) noexcept { misaligned_store<mutable_t>(ptr_, v); return *this; }
};
```

这个代理同时是**文件系统头解析器**——`index_dense_head_t`（`include/usearch/index_dense.hpp:42`）的所有字段也都是 `misaligned_ref_gt<...>`，因为 64 字节文件头里 `version_t`(2B)、`metric_kind_t`(1B) 依次排布，必然非对齐。

**与 hnswlib 的对比**：hnswlib 每个节点的邻接表是一个独立堆分配（`std::vector<labeltype>`），额外 24 字节对象头 + 一块可能碎片化的连续区；usearch 全图一条 tape，邻接表内联、零指针追逐，且整条 tape 可以被 mmap 后直接只读共享。

### 26.2 per-thread scratch：`context_t`

`context_t`（`include/usearch/index.hpp:2483`）集中承载一次调用所需的全部临时状态（`include/usearch/index.hpp:2484`）：

| 成员 | 类型 | 作用 |
|---|---|---|
| `top_candidates` | `sorted_buffer_gt` | 有序 top-K，满则挤掉最差 |
| `top_for_refine` | `sorted_buffer_gt` | 反向链接时的候选池 |
| `next_candidates` | `max_heap_gt` | 存 `-distance`，等效最小堆 |
| `visits` | `growing_hash_set_gt` | 本次搜索的已访问集合 |
| `level_generator` | `std::default_random_engine` | 层级采样专用，per-thread 无锁 |
| `iteration_cycles` / `computed_distances` / `computed_distances_in_refines` / `computed_distances_in_reverse_refines` | 计数器 | 成本统计 |

类型别名见 `include/usearch/index.hpp:2386`（top）与 `include/usearch/index.hpp:2387`（next）。整个数组在 `try_reserve` 时一次性分配 `limits.threads()` 份（`include/usearch/index.hpp:2910`），此后每次 `add` / `search` 只按 `config.thread` 索引对应份，**全程无锁、无共享**。这是把无锁竞争彻底消除在数据结构层面的做法：连随机数引擎都是 per-thread 的。

四个统计计数器在 `add` 返回前做差分（`include/usearch/index.hpp:3271`），把「本次调用实际算了几次距离、其中几次花在剪枝、几次花在反向链接」原样返回给调用方。**这是 USearch 少见的内建可观测性**，Python 绑定的 `index.stats` 就建立在这之上。

### 26.3 锁设计：一个全局锁 + Fibonacci 分片锁

**全局锁只有一个**：`std::mutex global_mutex_`，注释明确它只保护 `max_level_` 与 `entry_slot_` 两个变量。任何有该 `if (某线程正在更新这两个值)` 语义的操作需独占它。插入时持锁读快照 + 采样层数，若新节点不够高就立刻释放（§24.3）。

**节点锁是分片锁** `striped_locks_gt`（`include/usearch/index.hpp:668`）：

```cpp
static constexpr std::uint64_t fibonacci_k = 0x9E3779B97F4A7C15ull;   // index.hpp:674
struct alignas(cache_line_ak) padded_lock_t {                           // index.hpp:676
    atomic_flag_t flag{0};
    char padding_[cache_line_ak - sizeof(atomic_flag_t)];
};
static_assert(sizeof(padded_lock_t) == cache_line_ak, ...);            // index.hpp:680
inline std::size_t stripe_for_(std::size_t slot) const noexcept {       // index.hpp:686
    return static_cast<std::size_t>((static_cast<std::uint64_t>(slot) * fibonacci_k) >> shift_);
}
```

三点设计：
1. `cache_line_ak` 默认 **128 字节**（`include/usearch/index.hpp:667`），`alignas` + 填充保证一条锁独占一整条 cache line，彻底杜绝伪共享；
2. slot → 条纹用乘法散列（黄金分割常数），比取模更快且分布更均匀；
3. 条纹数 = `next_pow2(max(threads × connectivity_max × 4, 256))`（`include/usearch/index.hpp:724`），下界 256 条保证小索引也不至于锁冲突；
4. 分配时**多申请一条条纹的余量再手动向上对齐**（`include/usearch/index.hpp:737`），注释解释了原因——普通分配器只保证 16 字节对齐，而 `padded_lock_t` 要求 128 字节，与其强制分配器支持过对齐，不如自己算。

锁的 RAII 封装有两层：`node_lock_(slot)`（`include/usearch/index.hpp:4244`）无条件加锁；`optional_node_lock_(slot, condition)`（`include/usearch/index.hpp:4258`）在 `condition` 为假时返回一个 `slot = SIZE_MAX` 的哑锁，析构时跳过解锁。搜索路径正是后者：`bool need_lock = !is_immutable()`（`include/usearch/index.hpp:4449`）——**mmap 只读索引下，遍历完全无锁**。

### 26.4 线程池：index 自己管，不 spawn 线程

`index_dense_gt`（`include/usearch/index_dense.hpp:406`）持有一个可用线程号环形队列：

```cpp
using available_threads_t = ring_gt<std::size_t, available_threads_allocator_t>;  // index_dense.hpp:481
mutable available_threads_t available_threads_;                                  // index_dense.hpp:484
mutable std::mutex available_threads_mutex_;                                     // index_dense.hpp:487
```

`thread_lock_`（`include/usearch/index_dense.hpp:2140`）的语义很关键：

```cpp
thread_lock_t thread_lock_(std::size_t thread_id) const {
    if (thread_id != any_thread()) return {*this, thread_id, false};  // 调用方自带槽位，直接用
    std::unique_lock<std::mutex> lock(available_threads_mutex_);
    if (!available_threads_.try_pop(thread_id)) return {*this, any_thread(), false};  // 池满
    return {*this, thread_id, true};
}
```

- 调用方显式传 `thread` 参数 → 跳过池，直接用该槽位；
- 传默认的 `any_thread()` → `try_pop` 抢一个；抢不到就返回未 engaged 的锁，调用方立刻失败并报 `"Reserve capacity ahead of searches!"`（`include/usearch/index_dense.hpp:2223`）；
- `thread_lock_t` 析构自动归还（`include/usearch/index_dense.hpp:539`），异常路径也不会泄漏槽位；
- `search_result_t` **持有锁引用**（`include/usearch/index_dense.hpp:581`），保证结果存活期间该槽位不被其他线程抢走覆写。

池大小来自 `index_limits_t`（`include/usearch/index.hpp:1668`），默认 `max(1, hardware_concurrency())`（`include/usearch/index.hpp:1677`），`threads() = max(threads_add, threads_search)`（`include/usearch/index.hpp:1681`），另有 `concurrency() = min(...)` 给出同时读写都能保证的并发度。**USearch 从不 spawn 线程**——它只分配 context 槽位，真正的并行由调用方（Python 的 `ThreadPoolExecutor`、Java 的 `Executor`、或 C++ 自己的线程）驱动。

### 26.5 过滤：只有「搜索中过滤」一种模式

`filtered_search` 为 11 种标量各提供了一个重载（`include/usearch/index_dense.hpp:896`）。实现只是把用户的谓词包进 `allow` lambda（`include/usearch/index_dense.hpp:2245`）：

```cpp
auto allow = [free_key_copy, &predicate](member_cref_t const& member) noexcept {
    return (vector_key_t)member.key != free_key_copy && predicate(member.key);
};
```

底层 `search_to_find_in_base_`（`include/usearch/index.hpp:4654`）里谓词只在**两个时刻**生效：

- 起点 slot 通过谓词才进 top 列表（`include/usearch/index.hpp:4679`）；
- 后继 slot 通过谓词才进 top 列表（`include/usearch/index.hpp:4715`）。

但**被过滤掉的节点仍然会展开它的全部邻居**——图遍历的连通性完全不受过滤影响。这是标准的「搜索中过滤（in-search filtering）」：它不会破坏图结构，但当过滤条件选择性很低时，top 列表迟迟填不满，`radius` 收敛变慢，实测耗时会向暴力搜索靠拢。

⚠️ **USearch 不提供** Milvus / Qdrant 风格的过滤控制参数：`purity`、`stability`、`may_return_any` 三个开关在本版本源码中全部不存在（`grep -rniE "purity|stability|may_return_any|bitmap|multi_label" include/usearch/*.hpp python/lib.cpp c/usearch.h` 无任何命中），也没有多值标签的 bitmap 加速路径。过滤只能是任意 C++ 可调用对象，代价是每次候选比较一次间接调用，无法编译成位运算。追求过滤性能的场景应当把过滤下推到上层引擎（如 Milvus / Qdrant），而不是指望 usearch。

### 26.6 键→槽映射、墓碑与槽位复用

- `slot_lookup_` 是 `flat_hash_multi_set_gt<key_and_slot_t, ...>`（`include/usearch/index_dense.hpp:519`）。multi-set 的意义是 `multi = true` 时同名 key 可映射多个槽（一个大文档切多块），`equal_range` 能取回全部。由 `shared_mutex_t slot_lookup_mutex_` 保护（`include/usearch/index_dense.hpp:522`）。
- `enable_key_lookups = false` 可关掉反向索引省内存（`include/usearch/index_dense.hpp:138`），代价是 `get` / `rename` / `remove` 不可用；注意该开关**不进入序列化文件**，跨进程不保留。
- **墓碑机制**：删除不是真删，而是把槽位的 key 改写成 `free_key_`（`default_free_value<vector_key_t>()`，`include/usearch/index_dense.hpp:531`），写入点见 `include/usearch/index_dense.hpp:1655`。查询时用 `member.key != free_key_copy` 把墓碑挡掉（`include/usearch/index_dense.hpp:2239`）。
- **槽位复用**：`free_keys_` 是个 `ring_gt<compressed_slot_t>`（`include/usearch/index_dense.hpp:525`），`add_` 开始时在 `free_keys_mutex_` 保护下 `try_pop` 一个（`include/usearch/index_dense.hpp:2181`），复用其节点 tape 而不新分配，节点数不会因反复增删而单调膨胀。
- **向量数据独立于图**：`vectors_tape_allocator_` 是 `memory_mapping_allocator_gt<8>`（`include/usearch/index_dense.hpp:470`），`vectors_lookup_` 是 64 字节对齐的 `byte_t*` 数组（`include/usearch/index_dense.hpp:474`），在 `on_success` 回调里 memcpy 落盘（`include/usearch/index_dense.hpp:2201`）。图与向量两条 tape 分离，使得 `exclude_vectors = true` 时索引文件只存图（`include/usearch/index_dense.hpp:107`）。

### 26.7 序列化格式与 mmap 只读视图

文件头定长 **64 字节**，并有编译期断言（`include/usearch/index_dense.hpp:29`）：

```cpp
using index_dense_head_buffer_t = byte_t[64];
static_assert(sizeof(index_dense_head_buffer_t) == 64, "File header should be exactly 64 bytes");
```

魔数是字符串 `"usearch"`（`include/usearch/index_dense.hpp:27`），用于识别文件类型。头部布局（`index_dense_head_t`，`include/usearch/index_dense.hpp:42`）：

| 区段 | 字段 | 字节 |
|---|---|---|
| 版本 | magic(7) + version_major/minor/patch(2×3) | 13 |
| 结构 | kind_metric / kind_scalar / kind_key / kind_compressed_slot（各 3）| 12 |
| 规模 | count_present(8) / count_deleted(8) / dimensions(8) / multi(1) | 25 |
| 尾部 | 图长度、向量长度、校验和 + 5 字节空置 | 29 |

三个入口：`view()`（`include/usearch/index_dense.hpp:1348`）、`save()`（`include/usearch/index_dense.hpp:1500`）、`load()`（`include/usearch/index_dense.hpp:1555`）。

**`view()` 是最有意思的一个**：它把文件 mmap 后**不拷进 RAM**，直接把 tape 指针指到映射区。此后：

```cpp
bool is_immutable() const noexcept { return bool(viewed_file_); }    // index.hpp:2586
```

`is_immutable` 为真时三件事同时发生：`add` 直接失败（`include/usearch/index.hpp:3189` 返回 `"Can't add to an immutable index"`）、所有 `node_lock_` 退化为哑锁（`include/usearch/index.hpp:4449`）、`max_level_` / `entry_slot_` 也不再需要原子保护。**这是「训练用 Python、部署用 Java/Go」这类场景的正解**：一份索引文件 mmap 进多个进程，零拷贝共享、多进程并发只读、完全不需要锁。

### 26.8 绑定层的 GIL 纪律

Python 绑定的做法非常一致：**整个长耗时 C++ 调用段释放 GIL，回调 Python 之前重新获取**。`python/lib.cpp` 中 `py::gil_scoped_release` 出现在 `python/lib.cpp:112`、`python/lib.cpp:212`、`python/lib.cpp:333`、`python/lib.cpp:405`、`python/lib.cpp:607`、`python/lib.cpp:689`、`python/lib.cpp:787`、`python/lib.cpp:862`、`python/lib.cpp:904`、`python/lib.cpp:928` 等处；对应的 `py::gil_scoped_acquire` 在 `python/lib.cpp:60`、`python/lib.cpp:121`、`python/lib.cpp:229`、`python/lib.cpp:354`、`python/lib.cpp:442`、`python/lib.cpp:612`、`python/lib.cpp:695` 等处。模块本身声明为 `py::mod_gil_not_used()`（`python/lib.cpp:1123`），运行时完全不持锁。

C 语言出口是 `c/usearch.h`（508 行）+ `c/lib.cpp`（540 行），供需要 FFI 而非 pybind 的语言使用。

## 二十七、参考资料

- [USearch GitHub](https://github.com/unum-cloud/usearch)
- [USearch 官方文档](https://unum-cloud.github.io/usearch/)
- [USearch 博客](https://ashvardanian.com/tag/usearch/)
- [BENCHMARKS.md](https://github.com/unum-cloud/usearch/blob/main/BENCHMARKS.md)
- [HNSW 论文](https://arxiv.org/abs/1603.09320)
- [PQ 论文](https://hal.inria.fr/inria-00514462/en)
- [ann-benchmarks](https://github.com/erikbern/ann-benchmarks)
- [Unum Cloud Platform](https://unum.cloud/)
- [DuckDB 向量扩展](https://duckdb.org/docs/api/vector)
- [单头文件 C++ 库范例](https://github.com/nothings/stb)