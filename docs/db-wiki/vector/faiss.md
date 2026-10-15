# Faiss Wiki（向量相似度检索算法库事实标准）

> 参考版本：Faiss 1.7.x（Meta AI Research，C++ + CUDA + Python binding）。向量检索领域的"算法库标准"，不提供分布式 / 持久化，但定义了几乎所有主流 ANN 算法的参考实现。本 wiki 重点覆盖向量类型特有章节（IVF / PQ / HNSW / GPU / ScaNN / Opq）。

## 一、项目定位

Faiss（**F**acebook **A**I **S**imilarity **S**earch）是由 **Meta AI Research（原 Facebook AI Research）**开源的 **稠密向量相似度检索与聚类算法库**，由 **Hervé Jégou、Jeff Johnson、Matthijs Douze** 等人主导，是当前 ANN（Approximate Nearest Neighbor）研究领域最权威、引用最广的 C++ 库。

- **痛点**：
  - 学术圈缺乏工业级、高性能、可复现的 ANN 实现；
  - 大规模向量检索（亿-百亿级）需要 GPU 加速、复杂量化、SIMD 优化；
  - 多种距离度量（L2 / IP / COSINE / Hamming）和多种索引类型（Flat / IVF / PQ / HNSW）需要统一接口；
  - 需要 Python 快速原型 + C++ 高性能部署的双层体验；
- **典型用户**：ANN 研究者、推荐系统工程师、RAG 应用开发者、大模型 Embedding 检索、Meta 内部（FAIR、Instagram、WhatsApp）；
- **核心口号**：**"A library for efficient similarity search and clustering of dense vectors"**。

## 二、发展历史与社区

- **起源**：2015 年 Meta AI Research 启动，源于 Jeff Johnson 对 ANN 算法的研究积累；
- **里程碑**：
  - 2017-02：开源 v1.0，初期含 IVF / PQ / HNSW 基础；
  - 2018-08：v1.4 引入 **OPQ（Optimized Product Quantization）**；
  - 2019-04：v1.5 引入 **IVF + HNSW 混合索引（Index2Layer）**；
  - 2020-04：v1.6 引入 **HNSW 完整实现** + **PanoRAMa**；
  - 2021-05：v1.7 引入 **FastScan** 优化（4-bit PQ + SIMD）、**Binary Index** 完善；
  - 2022-2023：v1.7.x 持续优化 **GPU 实现**（CAGRA via cuVS）；
  - 2024-06：v1.8 引入 **ScaNN-like** 量化（基于 PQ + 重排）；
  - 2024-12：v1.8.0 与 **NVIDIA cuVS** 深度整合（GPU CAGRA / IVF）；
  - 2025-2026：与 **cuVS** 进一步整合（CAGRA GPU 加速）；
- **维护状态**：活跃，Meta 团队持续投入，GitHub 30k+ star；
- **社区**：学术 + 工业界双重引用，Faiss 用户群涵盖几乎所有大型 AI 公司。

## 三、许可与商业限制

- **核心代码**：**MIT License**（最宽松的开源许可之一）；
- **GPU 部分**：MIT License（基于 CUDA + cuVS）；
- **生态关系**：
  - 与 **PyTorch**（Meta）紧耦合，常作为 Torch 的向量后端；
  - 与 **NVIDIA cuVS**（原 RAFT）整合 GPU 加速；
  - 与 **ScaNN（Google）**、**DiskANN（Microsoft）** 是 ANN 算法层面的竞品；
  - 大量下游产品基于 Faiss：**Milvus（Knowhere 分支自 Faiss）**、**Vespa**、**Qdrant**、**Weaviate** 等。

## 四、业界主要使用方与案例

- **Meta**：内部数十亿级向量检索（推荐、广告、内容相似度）；
- **学术界**：几乎所有 ANN 论文的 baseline 实现；
- **工业界**：Microsoft、Google、Uber、字节、阿里、百度、华为；
- **典型场景**：推荐系统 Embedding 检索、图像检索（相似图）、跨模态检索、视频去重、大模型 RAG、生物序列检索。

## 五、一句话对比定位

> Faiss 是 **"MIT 许可 + C++ + Python + GPU + 数十种 ANN 算法 + 量化 + SIMD + 最权威学术参考实现"** 的 ANN 算法库；与 **ScaNN（Google）**、**DiskANN（Microsoft）**、**Hnswlib（Yury Malkov）**、**Annoy（Spotify）**、**USearch（Unum Cloud）**、**NMSLib（Yandex）** 构成 ANN 算法库阵营。Faiss 的差异化是 **"算法最全 + 最权威 + 工业验证 + GPU 支持 + 下游生态最丰富"**。**注意**：Faiss 仅是算法库，不提供分布式、持久化、并发、远程 API，**生产需要包装**（Milvus / Qdrant / Vespa 等）。

## 六、整体架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                       用户层                                         │
│  - C++：直接调用 faiss/Index.h                                       │
│  - Python：faiss（swig 绑定 / pybind11）                             │
│  - SWIG：自动生成 C# / Java 绑定                                     │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│                  核心抽象：Index                                     │
│  faiss/Index.h：所有索引的统一基类                                  │
│  ├─ train(x[N, d])：训练（k-means / PQ / HNSW）                  │
│  ├─ add(x[N, d], ids?)：添加向量                                  │
│  ├─ search(x[q, d], k)：Top-K 检索                                │
│  ├─ reconstruct / reconstruct_n：从 code 反推原向量                │
│  ├─ serialize / deserialize：持久化                                │
│  └─ assign(x, k)：把向量分配到聚类（k-means assign）              │
└─────┬──────────────────┬────────────────────┬──────────────────────┘
      │                  │                    │
┌─────▼──────┐  ┌───────▼────────┐  ┌───────▼─────────┐
│ Flat 索引 │  │  IVF / PQ 系列 │  │  HNSW 系列      │
│ IndexFlat │  │  IndexIVF*     │  │  IndexHNSW*     │
│ IndexFlatCodes│ │  IndexIVFPQ   │  │  IndexHNSWPQ    │
│ IndexFlatIP│ │  IndexIVFSQ    │  │  IndexHNSWSQ    │
└────────────┘  │  IndexIVF2Layer│  └─────────────────┘
                │  IndexIVFPQR   │
                │  IndexIVFOPQ   │
                └────────────────┘
┌──────────────────────────────────────┐
│              量化层                      │
│  - ProductQuantizer（PQ）              │
│  - ScalarQuantizer（SQ）              │
│  - AdditiveQuantizer（AQ / LVQ）       │
│  - ResidualQuantizer（RQ / PRQ）       │
│  - LocalOptimizedQuantizer（Lloyd）   │
└──────────────────────────────────────┘
┌──────────────────────────────────────┐
│            工具与基础设施                │
│  - Clustering（k-means）              │
│  - AutoTune（自动选参）                │
│  - IVFlib（IVF 分布式拆分 / 合并）    │
│  - IndexBinary*（二值向量）            │
│  - IndexShards（多 GPU 分片）          │
│  - IndexReplicas（多 GPU 副本）        │
└──────────────────────────────────────┘
┌──────────────────────────────────────┐
│              GPU 实现                   │
│  faiss/gpu/：基于 CUDA + cuVS        │
│  ├─ GpuIndexFlat（GPU Flat）          │
│  ├─ GpuIndexIVF*（GPU IVF）          │
│  ├─ GpuIndexCagra（GPU HNSW）        │
│  └─ GpuIndexBinaryCagra              │
└──────────────────────────────────────┘
┌──────────────────────────────────────┐
│           距离计算底层                  │
│  - faiss/utils/distances.h            │
│  - SIMD 优化：SSE / AVX / AVX512    │
│  - 距离：L2 / IP / L1 / Hamming      │
└──────────────────────────────────────┘
```

### 关键设计取舍

- **统一 Index 抽象**：所有索引继承自 `faiss::Index`，统一 train/add/search 接口；
- **训练 + 添加 + 检索 三段式**：train 必须显式调用（k-means / PQ 训练）；
- **算法库而非数据库**：无持久化（serialize/deserialize 提供）、无并发、无分布式；
- **GPU 优先**：所有常见索引都有 GPU 版本，且 GPU/CPU API 一致；
- **算法透明**：每个算法的 paper / 参考实现都在 `benchs/`，便于学术复用；
- **量化生态**：PQ / SQ / AQ / RQ / OPQ 全套量化方案，可自由组合。

## 七、存储引擎

### 7.1 内存存储

Faiss 不提供持久化存储，所有数据都在内存中：

- **IndexFlat**：存原始向量，1 个 float = 4 bytes；
- **IndexIVF**：存原始向量 + Inverted Lists；
- **IndexIVFPQ**：存量化码（1 byte/code）+ 码本；
- **IndexHNSW**：存图结构 + 向量（量化可选）；
- **IndexDiskANN**：存图 + 部分向量 + SSD mmap（社区扩展，Faiss 1.8+ 引入）。

### 7.2 Serialize / Deserialize

- 所有 Index 都支持 `serialize()` 输出到文件 / 内存；
- 格式：自定义二进制（含 header + 索引参数 + 数据）；
- **可作为单文件持久化**使用，但**多进程 / 分布式不支持**；
- 代码：`faiss/Index.cpp`, `faiss/IndexBinary.cpp`。

### 7.3 On-disk 模式（mmap）

- 索引可序列化后通过 `mmap()` 加载，避免全量读入内存；
- 适合 Flat / IVF 等大数据集；
- 代码：`faiss/impl/io.h`。

## 八、索引机制（**向量类型核心**）

### 8.1 向量索引算法全景

Faiss 支持的索引（截至 1.8）：

| 类别 | 算法 | 文件 | 特点 |
|------|------|------|------|
| **Flat** | `IndexFlatL2`, `IndexFlatIP`, `IndexFlatCosine` | `faiss/IndexFlat.*` | 暴力精确 |
| **IVF** | `IndexIVFFlat`, `IndexIVFPQ`, `IndexIVFSQ8`, `IndexIVF2Layer` | `faiss/IndexIVF*.*` | 倒排 |
| **PQ** | `IndexPQ`, `IndexIVFPQ`, `IndexHNSWPQ` | `faiss/IndexPQ.*`, `faiss/impl/ProductQuantizer*` | 乘积量化 |
| **SQ** | `IndexScalarQuantizer`, `IndexIVFScalarQuantizer` | `faiss/impl/ScalarQuantizer.*` | 标量量化 |
| **AQ** | `IndexAdditiveQuantizer`, `IndexIVFAdditiveQuantizer` | `faiss/IndexAdditiveQuantizer.*` | LVQ / 加性量化 |
| **RQ** | `IndexIVFPQR`, `IndexIVFRQ` | `faiss/IndexIVFPQR.*` | 残差量化 |
| **OPQ** | `IndexIVFOPQ`, `IndexPreTransform(OPQ)` | `faiss/impl/ResidualQuantizer*` | 优化旋转 + PQ |
| **FastScan** | `IndexIVFPQFastScan`, `IndexIVFAdditiveQuantizerFastScan` | `faiss/IndexIVFPQFastScan.*` | 4-bit + SIMD |
| **HNSW** | `IndexHNSWFlat`, `IndexHNSWPQ`, `IndexHNSWSQ` | `faiss/IndexHNSW*.*`, `faiss/impl/hnsw*` | 图索引 |
| **NSG** | `IndexNSGFlat`, `IndexNSGPQ` | `faiss/IndexNSG*.*` | 导航图 |
| **EDEN** | `IndexEDEN`, `IndexIVFEDEN` | `faiss/IndexEDEN.*` | 哈希量化 |
| **2Layer** | `Index2Layer` | `faiss/Index2Layer.*` | IVF + 残差图 |
| **PanoRAMa** | `IndexIVFFlatPanorama` | `faiss/IndexIVFFlatPanorama.*` | 蒸馏 IVF |
| **Binary** | `IndexBinaryFlat`, `IndexBinaryIVF`, `IndexBinaryHNSW` | `faiss/IndexBinary*.*` | 二值向量 |
| **GPU** | `GpuIndex*`, `GpuIndexCagra`, `GpuIndexBinaryCagra` | `faiss/gpu/GpuIndex*.*` | NVIDIA 加速 |

### 8.2 IVF（Inverted File）详解（Faiss 经典）

参见 Milvus Wiki 的 IVF 章节。Faiss 的 IVF 是参考实现：

- **核心 API**：`faiss::IndexIVF::search()`，`nprobe` 控制 recall / 延迟；
- **聚类**：`faiss::Clustering`（k-means）`faiss::Clustering.cpp`；
- **倒排表**：`InvertedLists` 接口 + `ArrayInvertedLists` / `HNSWInvertedLists`；
- **GPU 版本**：`faiss/gpu/GpuIndexIVF*`，NVIDIA cuVS 加速；
- **FastScan**：`IndexIVFPQFastScan` 用 4-bit PQ + SIMD LUT，**5-10x** 加速。

### 8.3 PQ（Product Quantization）详解

**PQ** 是 Jégou et al. 2011 提出的向量量化方法：

```
原始向量 x ∈ ℝ^d（如 d=128）
   ↓ 切分为 m 段
x = [x_1, x_2, ..., x_m]   每段 d/m 维（如 128/8=16 维）
   ↓ 每段独立 k-means 聚类（k=256）
码本：C_i ∈ ℝ^{k × d/m}（第 i 段）
   ↓ 编码 x
code_i = argmin_j ||x_i - C_i[j]||^2   j ∈ {0..255}
   ↓ 输出
PQ-code = [code_1, code_2, ..., code_m]   共 m bytes
```

**距离计算**：

```
d(x, y) ≈ ||C[x_code] - C[y_code]||^2
         = Σ_i ||C_i[x_code_i] - C_i[y_code_i]||^2   查表求和（m 次查表 + m 次加法）
```

**优势**：
- 压缩比 **1/32**（128 维 float = 512 bytes → PQ code 8 bytes）；
- 距离查表代替计算，**10x** 加速；
- 缺点：召回率有损（默认 90%+ R@10）。

**源码**：
- `faiss/ProductQuantizer.h`：定义；
- `faiss/impl/ProductQuantizer.cpp`：训练 + 编码；
- `faiss/IndexIVFPQ.cpp`：IVF + PQ 组合；
- `faiss/IndexPQ.cpp`：纯 PQ 索引。

### 8.4 HNSW（Faiss 1.7+ 完整实现）

**HNSW** 在 Faiss 1.7 起成为一等公民：

- **完整实现**：`faiss/IndexHNSW.*`, `faiss/impl/hnsw.h`, `faiss/impl/hnsw.cpp`；
- **变体**：
  - `IndexHNSWFlat`：HNSW + 原向量；
  - `IndexHNSWPQ`：HNSW + PQ 压缩（内存大幅降低）；
  - `IndexHNSWSQ`：HNSW + SQ8；
  - `IndexHNSW2Level`：HNSW + 二级索引；
- **参数**：`M`（每节点连接数）、`efConstruction`、`efSearch`；
- **持久化**：完整索引可 serialize 到文件。

### 8.5 ScaNN（参考实现）

Faiss 通过 **OPQ + PQ + 重排（Refine）** 实现 ScaNN 风格检索：

```python
# 1. 训练 OPQ 旋转
opq = faiss.OPQMatrix(d, M=8)
opq.train(x_train)

# 2. 训练 PQ 码本
pq = faiss.ProductQuantizer(d, M=8, nbits=8)

# 3. 训练残差量化（Re-rank）
rq = faiss.ResidualQuantizer(d, M=8, nbits=8)

# 4. 组合：粗检索 PQ + 精排残差
```

代码：`faiss/impl/ResidualQuantizer.*`, `faiss/IndexIVFPQR.*`。

### 8.6 GPU 索引（CAGRA / IVF / Flat）

- **`GpuIndexFlat`**：GPU 暴力检索；
- **`GpuIndexIVF*`**：GPU IVF 检索，10-50x 加速（vs CPU）；
- **`GpuIndexCagra`**：GPU HNSW（CAGRA 算法），NVIDIA cuVS 实现，**50-100x** 加速；
- **多卡**：`IndexShards`（分片）、`IndexReplicas`（副本）；
- **代码**：`faiss/gpu/GpuIndexCagra.cu`（基于 cuVS 库）；
- **API 一致性**：CPU / GPU 索引都继承 `faiss::Index`，API 完全一致。

### 8.7 量化方案全景

| 量化器 | 文件 | 压缩比 | 适用 |
|--------|------|--------|------|
| **PQ（Product Quantizer）** | `ProductQuantizer.*` | 1/32 | IVF_PQ、HNSW_PQ |
| **SQ（Scalar Quantizer）** | `ScalarQuantizer.*` | 1/4 | IVF_SQ8 |
| **AQ（Additive Quantizer / LVQ）** | `AdditiveQuantizer.*` | 1/32 | IVF_AQ |
| **RQ（Residual Quantizer）** | `ResidualQuantizer.*` | 1/16 ~ 1/32 | IVF_PRQ |
| **PRQ（Product Residual Quantizer）** | `AdditiveQuantizer.*` | 1/32 | IVF_PQFastScan |
| **LSQ（Learned Scalar Quantization）** | 实验 | 1/4 | research |
| **QAT（Quantization Aware Training）** | 实验 | 训练时 | research |

### 8.8 OPQ（Optimized Product Quantizer）

**OPQ** = PQ + 旋转矩阵学习（Ge et al. 2013）：

- 把 PQ 训练时的"段切分"问题转换为旋转矩阵 + PQ 组合；
- 训练成本比 PQ 高，但召回率提升 **5-15%**；
- 代码：`faiss/impl/OPQ.h`。

### 8.9 FastScan（4-bit PQ + SIMD）

**FastScan** 是 Faiss 1.7 引入的关键加速：

- 把 PQ 码从 8-bit 压缩到 **4-bit**（进一步 2x 内存节省）；
- 距离计算用 **SIMD LUT**（SSE / AVX / AVX-512）；
- 批量查询时，**5-10x** 加速 vs 传统 IVF_PQ；
- 代码：`faiss/IndexIVFPQFastScan.*`。

## 九、查询/计算/接口层

### 9.1 Python 接口

```python
import faiss
import numpy as np

# 1. 构建索引
d = 128                            # 维度
nb = 1_000_000                     # 向量数
nlist = 4096                       # IVF 聚类数
m = 8                              # PQ 子量化器数

quantizer = faiss.IndexFlatL2(d)
index = faiss.IndexIVFPQ(quantizer, d, nlist, m, 8)

# 2. 训练（必须！）
xb = np.random.random((nb, d)).astype('float32')
index.train(xb)

# 3. 添加
index.add(xb)

# 4. 查询
xq = np.random.random((10, d)).astype('float32')
D, I = index.search(xq, k=10)      # 10 个查询的 Top-10

# 5. 持久化
faiss.write_index(index, "index.faiss")
index = faiss.read_index("index.faiss")
```

### 9.2 C++ 接口

```cpp
#include <faiss/IndexFlat.h>
#include <faiss/IndexIVFPQ.h>

int d = 128;
int nlist = 4096;
int m = 8;

auto quantizer = new faiss::IndexFlatL2(d);
faiss::IndexIVFPQ index(d, nlist, m, 8);
index.train(nb, xb);
index.add(nb, xb);
index.search(nq, xq, k, D, I);
```

### 9.3 多语言绑定

- **Python**：`faiss`（pip install faiss-cpu / faiss-gpu）；
- **C# / Java**：SWIG 自动生成；
- **Go / Rust**：社区 binding；
- **JavaScript**：WebAssembly（WASM）实验性。

### 9.4 自动选参：AutoTune

```python
# 自动选择最佳参数（nprobe / efSearch 等）
index = faiss.index_factory(d, "IVF4096,PQ8")
auto_index = faiss.index_cpu_to_gpu(provider, 0, index)
faiss.optimize_search_parameters(auto_index, xq, xb)
```

代码：`faiss/AutoTune.cpp`。

### 9.5 距离计算 API

- **C++**：`faiss::fvec_L2sqr()`, `faiss::fvec_inner_product()` 等；
- **SIMD 优化**：自动根据 CPU 选 SSE / AVX / AVX-512；
- **批量距离**：`faiss::knn(...)` 通用 k-NN 接口。

## 十、事务与一致性

Faiss **无事务概念**：

- 单线程 `add()` 是非线程安全的；
- 多线程需要外部加锁（`std::mutex`）；
- 多 GPU 副本（`IndexReplicas`）提供**只读并发读**；
- 数据一致性由用户负责。

## 十一、复制与高可用

- **算法库无 HA 概念**；
- 生产部署需要包装层（Milvus / Qdrant / Vespa）实现 HA；
- **IVFlib**：Faiss 内置的 IVF 拆分 / 合并工具，用于分布式场景的索引分发。

## 十二、扩展性

- **垂直扩展**：增加内存 + GPU；
- **水平扩展**：
  - **IVFlib**（`faiss/IVFlib.cpp`）：把 IVF 索引拆分为多个 shard，分布式部署；
  - **外部包装**：Milvus / Qdrant / Vespa 承担分布式责任；
- **GPU 多卡**：`IndexShards` + `IndexReplicas` 提供多卡并行。

## 十三、性能与基准

- **ann-benchmarks**：Faiss 是大多数数据集（SIFT-1M, GloVe-100, GIST-950K）的 **baseline**；
- **Flat**：约 5-10 万 QPS（128 维，CPU AVX）；
- **IVF_PQ**：约 1-5 万 QPS（百万级，CPU AVX）；
- **GPU CAGRA**：约 **50-100 万 QPS**（百万级，A100）；
- **FastScan**：5-10x 加速（vs 传统 IVF_PQ）。

## 十四、运维与可观测性

- **无内置监控**；
- **benchmark 工具**：`benchs/` 目录提供多种 benchmark 脚本；
- **faiss_bench**：标准 benchmark 工具；
- **可视化**：用户自行处理；
- **持久化**：单文件 `index.faiss`。

## 十五、生态与集成

- **PyTorch**：可与 torch 张量互转；
- **NVIDIA cuVS**：GPU 加速；
- **Milvus（Knowhere）**：直接 fork Faiss；
- **Vespa**：内嵌 Faiss；
- **Weaviate / Qdrant**：参考 Faiss 设计；
- **dgl-ke / pykg2vec**：知识图谱 embedding；
- **sentence-transformers**：HuggingFace 生态。

## 十六、核心论文与算法原理

- **PQ**：Jégou et al., "Product Quantization for Nearest Neighbor Search", 2011, IEEE TPAMI；
- **IVF + Residual Quantization**：Jégou et al., "Searching in one billion vectors: re-rank", 2011；
- **OPQ**：Ge et al., "Optimized Product Quantization for Approximate Nearest Neighbor Search", CVPR 2013；
- **HNSW**：Malkov & Yashunin, 2016, IEEE TPAMI；
- **FastScan**：André et al., "Accelerating Vector Search with Inverted File and FastScan", 2023；
- **CAGRA**：Ootomo et al., 2023, NVIDIA；
- **ScaNN**：Guo et al., 2020, ICML；
- **EDEN**：Ni et al., 2023。

## 十七、源码学习路径

| 优先级 | 模块 | 路径 | 说明 |
|--------|------|------|------|
| P0 | Index 基类 | `faiss/Index.h` | 统一接口 |
| P0 | IndexFlat | `faiss/IndexFlat.cpp` | 暴力检索 |
| P0 | IndexIVF | `faiss/IndexIVF.cpp` | 倒排框架 |
| P0 | IndexIVFPQ | `faiss/IndexIVFPQ.cpp` | IVF + PQ 经典组合 |
| P0 | ProductQuantizer | `faiss/impl/ProductQuantizer.cpp` | 量化核心 |
| P0 | k-means | `faiss/Clustering.cpp` | 聚类 |
| P1 | IndexHNSW | `faiss/IndexHNSW.cpp`, `faiss/impl/hnsw.cpp` | 图索引 |
| P1 | FastScan | `faiss/IndexIVFPQFastScan.cpp` | SIMD 加速 |
| P1 | ResidualQuantizer | `faiss/impl/ResidualQuantizer.cpp` | 残差量化 |
| P2 | GpuIndexCagra | `faiss/gpu/GpuIndexCagra.cu` | GPU HNSW |
| P2 | IndexBinaryIVF | `faiss/IndexBinaryIVF.cpp` | 二值向量 |
| P2 | IVFlib | `faiss/IVFlib.cpp` | 分布式辅助 |

## 十八、源码规模与开发语言

- **C++**：约 70% LOC（核心算法）；
- **CUDA**：约 15% LOC（GPU 实现）；
- **Python（swig 绑定）**：约 10% LOC；
- **C**：约 5% LOC（C API）；
- **构建系统**：CMake；
- **CI**：GitHub Actions；
- **LOC 总量**：约 30 万行（含测试、benchmark）。

## 十九、学习资源

- **官方文档**：https://faiss.ai/
- **GitHub Wiki**：https://github.com/facebookresearch/faiss/wiki
- **教程**：https://github.com/facebookresearch/faiss/tree/main/tutorial
- **论文集**：https://faiss.ai/（含完整 PDF 链接）；
- **Doxygen API**：项目根目录 `Doxyfile` 生成；
- **ann-benchmarks**：第三方基准对比；
- **书籍**：《Similarity Search》（Webbook）。

## 二十、可移植性与部署

| 平台 | 支持 |
|------|------|
| Linux x86_64 | 原生 |
| macOS x86_64 / ARM64 | 原生 |
| Windows x86_64 | 原生（conda）|
| Linux ARM64 | 实验性 |
| Docker | 官方镜像（CPU / GPU）|
| conda | faiss-cpu / faiss-gpu |
| pip | pip install faiss-cpu |

## 二十一、变体与衍生项目

- **Knowhere**：Milvus 的 C++ 内核，直接 fork Faiss；
- **cuVS**：NVIDIA 的 GPU ANN 库（与 Faiss 整合）；
- **FAISS-IVFlib**：分布式部署辅助；
- **faiss-wasm**：浏览器内 ANN；
- **第三方绑定**：faiss-go、faiss-rs。

## 二十二、风险与陷阱

1. **算法库而非数据库**：Faiss 不提供持久化（serialize 只支持单文件）、不提供并发、不提供远程 API；生产需要包装层；
2. **train 必须显式调用**：忘记 train 会导致 `search()` 返回垃圾；
3. **线程不安全**：单进程内 `add()` / `search()` 并发需加锁；
4. **GPU 内存有限**：千万级 FP32 向量需要 **40+ GB GPU 显存**；
5. **PQ 召回有损**：默认参数下 recall@10 ≈ 90-95%，需调 `nprobe` / `nlist`；
6. **HNSW 大数据内存**：千万级向量 HNSW 内存占用 10-30 GB；
7. **维度灾难**：>1024 维时所有 ANN 算法性能下降；
8. **serialize 兼容性**：跨大版本（如 1.6 → 1.8）二进制格式可能不兼容；
9. **FastScan 数值精度**：4-bit PQ 召回率略低于 8-bit；
10. **不擅长超高维**（>2048）：需要先用 PCA / OPQ 降维。

## 二十三、与本项目的关联

- **`engineering/src/index/`** 中已有 HNSW / PQ 实现，可参考 Faiss 的统一 `Index` 抽象统一接口；
- **`engineering/include/algo/`** 中已有 k-means、SIMD 距离计算，可与 Faiss 对照学习；
- **`learning/algo-c/ann/`** 是 ANN 算法的学习目录，可参考 Faiss 论文 + 源码；
- **MiniVecDB**：参考 Faiss 的 Index 接口设计统一 IVF / HNSW / PQ 抽象。

## 二十四、参考资料

- [Faiss GitHub](https://github.com/facebookresearch/faiss)
- [Faiss Wiki](https://github.com/facebookresearch/faiss/wiki)
- [PQ 论文](https://hal.inria.fr/inria-00514462/en)
- [OPQ 论文](https://www.cv-foundation.org/openaccess/content_cvpr_2013/papers/Ge_Optimized_Product_Quantization_2013_CVPR_paper.pdf)
- [HNSW 论文](https://arxiv.org/abs/1603.09320)
- [ScaNN 论文](https://arxiv.org/abs/1908.10396)
- [CAGRA / NVIDIA cuVS](https://github.com/rapidsai/cuvs)
- [ann-benchmarks](https://github.com/erikbern/ann-benchmarks)
- [FastScan 论文](https://arxiv.org/abs/2306.02206)
- [Faiss 教程](https://github.com/facebookresearch/faiss/tree/main/tutorial)