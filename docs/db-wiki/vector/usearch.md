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

## 二十四、参考资料

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