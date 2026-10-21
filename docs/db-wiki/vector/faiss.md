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

## 二十四、index_factory 字符串语法与索引组合链

Faiss 把「索引结构」编码成一条**声明式字符串**，`faiss::index_factory()` 解析后直接构造出组合索引对象。这条语法是理解 Faiss 索引族谱最有效的钥匙 —— **工厂字符串的每一段就对应一层 `Index` 包装**，而不是一个独立算法。绝大多数「A 索引 + B 索引」的组合（粗排 + 重排、变换 + 编码、向量 + ID 映射）都是靠这条语法拼出来的。

### 24.1 解析器结构：递归下降 + 严格优先级

入口 `faiss/index_factory.cpp:1227` 的 `index_factory()` 只是薄壳，真正的解析在同文件的 `index_factory_sub()`（`faiss/index_factory.cpp:958`）。它是一个**递归下降**函数，签名 `(d, description, metric, own_invlists)`，每次递归都可能改变 `d`（维度）—— 这一点是「为什么 `PCA64` 必须写在最前面」的根源。

`re_match()`（`faiss/index_factory.cpp:85`）把整条字符串交给 `std::regex_match` **整串匹配**（不是 `regex_search`），所以每个分支的 pattern 都必须写成完整的文法产生式。解析顺序**严格依赖优先级**，不是「最长匹配」：

```
index_factory_sub(desc):
  1. IDMap2 / IDMap          → 递归解析剩余部分，用 IndexIDMap2/IndexIDMap 包裹   (faiss/index_factory.cpp:976, :984)
  2. RefinePanorama(...)      → 递归解析两层                                     (faiss/index_factory.cpp:994)
  3. Refine(...) / ,RFlat     → 递归解析 base + refine                            (faiss/index_factory.cpp:1005)
  4. VectorTransform 前缀链   → while 循环剥离 "OPQ8,PCA64,L2norm," 等前缀        (faiss/index_factory.cpp:1029)
  5. 括号子索引               → () 递归展开为 Index0/Index1 占位符                (faiss/index_factory.cpp:1059, :1063)
  6. parse_other_indexes      → Flat / PQ / SQ / OPQ / EDEN 等基础类型            (faiss/index_factory.cpp:1080)
  7. HNSW<M>[,_<code>]        → 递归解析 code_string                             (faiss/index_factory.cpp:1089)
  8. NSG / MinMax / SVS       →                                                  (faiss/index_factory.cpp:1131, :1153, :1111)
  9. IVF 前缀 → parse_IndexIVF(code)                                (faiss/index_factory.cpp:1176, :1210)
 10. 否则 FAISS_THROW_FMT("could not parse index string %s")                     (faiss/index_factory.cpp:1221)
```

关键点：**第 4 步必须排在第 5/6/7/9 步之前**。`VectorTransform` 会改变维度（`PCA64`、`OPQ32_64`），只有先剥离它们才能确定后续子索引的 `d`。剥离循环每成功一次就把 `d` 更新为 `vts.back()->d_out`（`faiss/index_factory.cpp:1033`）。

### 24.2 IVF 双段语法：`粗量化器[,编码器]`

`IVF...` 之后的第一��逗号是**硬性分隔符**（`faiss/index_factory.cpp:1180` 处的 `description.find(',')`）。左半段由 `parse_coarse_quantizer()`（`faiss/index_factory.cpp:278`）解析，右半段由 `parse_IndexIVF()`（`faiss/index_factory.cpp:367`）解析。

**左半段（决定 nlist 与粗量化器类型）**：

| 语法 | 产出 | 源码 |
|------|------|------|
| `IVF<n>` | `IndexFlat`，nlist = n | `faiss/index_factory.cpp:291` |
| `IVF<n>_HNSW<m>` | `IndexHNSWFlat`，m 缺省 32 | `faiss/index_factory.cpp:303` |
| `IVF<n>_NSG<R>` | `IndexNSGFlat` | `faiss/index_factory.cpp:308` |
| `IVF<n>(Index<k>)` | 复用第 k 个括号子索引 | `faiss/index_factory.cpp:335` |
| `IMI2x<nbits>` | `MultiIndexQuantizer`，nlist = 4^nbits | `faiss/index_factory.cpp:295` |
| `Residual<n>x<nbits>` | 走 `Index2Layer` 而非 `IndexIVF` | `faiss/index_factory.cpp:344` |

`nlist` 数字支持 `k`/`M` 后缀：`parse_nlist()`（`faiss/index_factory.cpp:264`）中 `k` → ×1024、`M` → ×1024²（`faiss/index_factory.cpp:268`、`faiss/index_factory.cpp:272`），所以 `IVF4096`、`IVF4k`、`IVF4M` 完全等价。

粗量化器建好后，`fix_ivf_fields()`（`faiss/index_factory.cpp:145`，调用点在 `faiss/index_factory.cpp:1218`）做三件事：

- `quantizer_trains_alone = get_trains_alone(quantizer)`（`faiss/index_factory.cpp:123`）：`IndexFlat` → 0（可直接当 k-means 索引）；`MultiIndexQuantizer` / `ResidualCoarseQuantizer` → 1（只需量化一次）；`IndexHNSWFlat` / SVS-Vamana → 2
- `cp.spherical = (metric_type == METRIC_INNER_PRODUCT)`：内积下 k-means 自动切到球面聚类
- `own_fields = true`：粗量化器转由 IVF 索引接管生命周期

**右半段（决定倒排表里存什么）**：

| 语法 | 产出索引 | 关键源码行 |
|------|----------|-----------|
| `Flat` | `IndexIVFFlat` | `faiss/index_factory.cpp:380` |
| `FlatDedup` | `IndexIVFFlatDedup` | `faiss/index_factory.cpp:383` |
| `FlatPanorama<nlevels>_<bs>` | `IndexIVFFlatPanorama`（缺省 8 层） | `faiss/index_factory.cpp:386` |
| `SQ8`/`SQ4`/`SQ6`/`SQfp16`/`SQbf16`/`SQ8_direct*`/`SQ<tq\|tqmse><n>` | `IndexIVFScalarQuantizer` | `faiss/index_factory.cpp:159`、`faiss/index_factory.cpp:178`、`faiss/index_factory.cpp:393` |
| `PQ<m>` / `PQ<m>x<nbits>` | `IndexIVFPQ` | `faiss/index_factory.cpp:402` |
| `PQ<m>np` | 同上，`do_polysemous_training = false` | `faiss/index_factory.cpp:406` |
| `PQ<m>+<m2>` | `IndexIVFPQR`（PQ + 残差量化两级），**仅 L2** | `faiss/index_factory.cpp:409` |
| `PQ<m>x4fs(r?)(_<bbs>)` | `IndexIVFPQFastScan`，nbits 硬编码 4 | `faiss/index_factory.cpp:416`、`faiss/index_factory.cpp:420` |
| `RQ<nbits>[_<norm>]` / `LSQ<nbits>` | `IndexIVFResidualQuantizer` / `IndexIVFLocalSearchQuantizer` | `faiss/index_factory.cpp:424` |
| `PRQ<...>` / `PLSQ<...>` | Product Additive Quantizer | `faiss/index_factory.cpp:441` |

SQ 名字到枚举的映射表是 `sq_types`（`faiss/index_factory.cpp:159` 起，共 17 个条目），正则 `sq_pattern` 在 `faiss/index_factory.cpp:178`。注意 `PQ<m>+<m2>` 分支里有一条硬约束：`faiss/index_factory.cpp:410` 直接抛异常，因为残差量化没有内积版本。

### 24.3 三个最易误解的后缀

**`np` 不是「内积」，而是「非对称距离」。** `PQ<m>` 默认 `do_polysemous_training = true`（`faiss/index_factory.cpp:406`）—— 训练结束后会跑一轮 Polysemous 模拟退火（`pt->optimize_pq_for_hamming(pq, n, x)`，调用点 `faiss/IndexIVFPQ.cpp:89`），把 PQ 码本重排成「Hamming 距离更可分」的形态。这条路线最终走 **SDC（对称距离）**：查询端**也**先编码成码，数据库码与查询码之间算 `sdc_table`。`np` 把它关掉，回到 **ADC（非对称距离）**：查询端保留原始浮点向量，对码本算距离表，只有数据库端用码。

这条选择被 `IndexPQ::Search_type_t`（`faiss/IndexPQ.h:64`）显式化为六个枚举值：`ST_PQ`（默认即 ADC）、`ST_HE`、`ST_generalized_HE`、`ST_SDC`、`ST_polysemous`、`ST_polysemous_generalize`。ADC 更准（查询不引入量化误差），SDC 更快（查询端也能进 LUT），这是 PQ 检索里唯一一处真正的「精度 / 速度」可调开关。

**`fs` 只支持 4-bit。** pattern 里 `x4` 是**字面量**而非捕获组（`faiss/index_factory.cpp:416`），构造出的 `IndexIVFPQFastScan` 硬编码 `nbits = 4`（`faiss/index_factory.cpp:420`）。`r` 后缀 = `by_residual`，`_N` 后缀 = block size，缺省 32（`faiss/index_factory.cpp:417`）。

**`RFlat` 是 `Refine(Flat)` 的语法糖。** `faiss/index_factory.cpp:1005` 同时匹配 `(.+),RFlat` 和 `(.+),Refine\((.+)\)`：若 `sm.size() == 3` 走通用 `IndexRefine`（两个索引都要显式构造），否则构造 `IndexRefineFlat(filter_index.release(), nullptr)`（`faiss/index_factory.cpp:1018`），refine 索引留空。

### 24.4 组合示例逐段拆解

```
"IVF1024,PQ8x4fs,RFlat"
  │
  ├─ 第 3 步命中 "(.+),RFlat"                                          (faiss/index_factory.cpp:1005)
  │    base = index_factory_sub("IVF1024,PQ8x4fs")
  │      ├─ 第 4 步：无 VectorTransform 前缀
  │      ├─ 第 5 步：无括号
  │      ├─ 第 6 步：parse_other_indexes 不匹配
  │      └─ 第 9 步：comma 落在 "IVF1024" 之后
  │           parse_coarse_quantizer("IVF1024") → IndexFlat, nlist=1024 (faiss/index_factory.cpp:291)
  │           parse_IndexIVF("PQ8x4fs")
  │             → IndexIVFPQFastScan(q, d, 1024, M=8, nbits=4, mt, bbs=32)
  │                                                                  (faiss/index_factory.cpp:416-421)
  │           fix_ivf_fields → spherical / trains_alone             (faiss/index_factory.cpp:1218)
  │    → IndexRefineFlat(base, nullptr)                               (faiss/index_factory.cpp:1018)
  └─ 语义：1024 桶 IVF 粗排 + 每桶 8 子量化器 4-bit FastScan + 全量 L2 重排
     压缩率 = 4 字节/向量，召回率靠 RFlat 重排拉回
```

```
"HNSW32"                     → hnsw_M=32；code_string 为空串，等价于 HNSW32,Flat
                               (faiss/index_factory.cpp:1090-1093, :1101)
"PQ16x4np"                   → IndexPQ(d, M=16, nbits=4)，ADC，16 字节码
                               (faiss/index_factory.cpp:567)
"OPQ32_64,PQ16x4np,IVF4096,Flat"
                            → 先剥 OPQ32_64（d_out=64，d 随之变为 64），再解析剩余
                               (faiss/index_factory.cpp:248, :1029)
"IDMap,IVF100,PQ16x4"        → IndexIDMap 包裹 + IVF 前缀均支持（IDMap 允许作前缀或后缀）
                               (faiss/index_factory.cpp:984, :986)
"IVF100(PQ16),PQ16x4fs"      → 括号子索引 PQ16 成为第 100 个桶的粗量化器
                               (faiss/index_factory.cpp:1063, :335)
```

### 24.5 括号机制：`(Index0)` 的占位符重写

`faiss/index_factory.cpp:1059` 起的循环用 `find_matching_parentheses()`（`faiss/index_factory.cpp:91`）逐对定位括号，把括号内的子串**递归解析成真实索引对象**压进 `parenthesis_indexes`，同时把原字符串里的括号内容替换成 `Index0` / `Index1` 占位符。随后 `IVF<n>(Index<k>)`（`faiss/index_factory.cpp:335`）从 `parenthesis_indexes[k].release()` 取回所有权。这样「括号里的任意组合索引」都能当粗量化器用。括号不配对会抛 `unbalanced parentheses`（`faiss/index_factory.cpp:114`）。

`index_factory` 也有**反方向**的 `reverse_index_factory()`（C++ 版 `faiss/factory_tools.cpp:93`，Python 版 `contrib/factory_tools.py:83`），但它**不是严格可逆的** —— `contrib/factory_tools.py:90` 明确注释说 `QT_8bit_uniform` / `QT_4bit_uniform` 根本没有对应的工厂字符串名，`reverse_index_factory` 造出的名字无法再被 `index_factory` 解析回去。

---

## 二十五、量化器家族：PQ / SQ / OPQ / 残差量化 / FastScan

本节补 §8.3 ~ §8.9 未展开的**实现级细节**：训练如何收敛、编码器如何分派、查表布局怎么排、内存为什么是这个数。

### 25.1 PQ 的派生量与内存账

`ProductQuantizer::set_derived_values()`（`faiss/impl/ProductQuantizer.cpp:61`）在构造时一次性定死四个派生量：

```
dsub      = d / M                     (faiss/impl/ProductQuantizer.cpp:67)   ← d 必须能被 M 整除
code_size = (nbits * M + 7) / 8       (faiss/impl/ProductQuantizer.cpp:69)
ksub      = 1 << nbits                (faiss/impl/ProductQuantizer.cpp:70)
centroids 分配 d * ksub 个 float      (faiss/impl/ProductQuantizer.cpp:71)
```

码本内存 = `d * ksub * 4` 字节（`faiss/impl/ProductQuantizer.h:54`）。以 `d=128, M=8, nbits=8` 为例：码本 `128 × 256 × 4 = 128 KB`，每向量码 `8` 字节，**压缩比 512/8 = 64×**。注意 Faiss 的 `nbits > 24` 会直接抛异常（`faiss/impl/ProductQuantizer.cpp:68`）。

`faiss/impl/ProductQuantizer.h:26` 的注释点出一个常被忽略的事实：**PQ 的量化误差天然偏向 L2**，即使支持内积搜索，误差也是往 L2 方向偏的 —— 这就是 `PQ...np` 之外，用内积时通常还要再叠一层 `L2norm` 变换的原因。

### 25.2 PQ 训练：切段 → 逐段 k-means

`ProductQuantizer::train()`（`faiss/impl/ProductQuantizer.cpp:130`）默认 `train_type == Train_default`，走**逐段独立训练**分支：

```
for m in 0 .. M-1:
    xslice[j] = x[j][m*dsub : (m+1)*dsub]      # strided 切片，faiss/impl/ProductQuantizer.cpp:145-150
    clus = Clustering(dsub, ksub, cp)          # faiss/impl/ProductQuantizer.cpp:151
    clus.train(n, xslice, IndexFlatL2(dsub))  # faiss/impl/ProductQuantizer.cpp:188
    set_params(clus.centroids, m)             # faiss/impl/ProductQuantizer.cpp:189
```

`ClusteringParameters` 的关键默认值（`faiss/Clustering.h:24` 起）：

| 参数 | 默认 | 源码行 | 作用 |
|------|------|--------|------|
| `niter` | 25 | `faiss/Clustering.h:24` | 每次聚类的 Lloyd 迭代次数 |
| `nredo` | 1 | `faiss/Clustering.h:27` | 重复聚类次数，保留目标函数最优的一组 |
| `min_points_per_centroid` | 39 | `faiss/Clustering.h:44` | 低于此值打 WARNING（不是报错） |
| `max_points_per_centroid` | 256 | `faiss/Clustering.h:46` | 超出则**自动下采样**，训练集被截到 `k*256` |

下采样在 `faiss/Clustering.cpp:104` 触发；训练点不足在 `faiss/Clustering.cpp:113` 只打警告；训练点数恰好等于 k 时走 `faiss/Clustering.cpp:124` 的「直接拷贝成簇」特例。

**单次 PQ 训练的复杂度**：`O(M · niter · n · ksub · dsub) = O(niter · n · d · ksub)`。以 n=10^6, d=128, ksub=256, niter=25 计约 8×10^11 次浮点距离计算 —— 这就是 `max_points_per_centroid=256` 存在的意义：把实际训练量压到 `ksub × 256 = 65536` 点/段。

`Train_shared` 分支（`faiss/impl/ProductQuantizer.cpp:192` 起）把所有 M 段拼起来共享一套码本，适合各段分布高度一致的数据，可省 `M` 倍码本内存。

### 25.3 编码器分派：为什么有三种 Encoder

`compute_code()`（`faiss/impl/ProductQuantizer.cpp:282`）按 `nbits` 分派到三个模板：

```
nbits == 8   → compute_1_code<PQEncoder8>        # 一个字节一个子码
nbits == 16  → compute_1_code<PQEncoder16>       # 两个字节
otherwise    → compute_1_code<PQEncoderGeneric>  # 位流打包，任意 nbits
```

外层还包了一层 `with_simd_level_a1(...)`（`faiss/impl/ProductQuantizer.cpp:284`），运行时按 CPU 特性选 SIMD 等级。`compute_1_code` 内部有个反直觉但关键的优化（`faiss/impl/ProductQuantizer.cpp:215` 起的注释）：**先把所有 `ksub` 个距离全算完存进 `std::vector<float> distances`，再扫一遍取 argmin**，而不是边算边比。源码注释解释这是因为「让编译器生成排序好的循环」，直接边算边比的写法会产生显著更多的 CPU 指令。

解码侧对称：`decode()`（`faiss/impl/ProductQuantizer.cpp:312`）按同样的 nbits 分派，批量版本带 OpenMP（`faiss/impl/ProductQuantizer.cpp:330` 的 `#pragma omp parallel for if (n > 100)`）。

### 25.4 ADC 距离表与 SDC 查表：两条完全不同的路

**ADC（非对称）**：`compute_distance_table()`（`faiss/impl/ProductQuantizer.cpp:444`）为每个查询向量算一张 `M × ksub` 的表：

```
for m in 0..M-1:
    fvec_L2sqr_ny(dis_table + m*ksub,        # 输出
                  x + m*dsub,                 # 查询子向量（原始 float）
                  get_centroids(m, 0), dsub, ksub)
```

结果 `dis_table` 的布局是 `(m, j) -> 查询第 m 段到第 m 段第 j 个码字的距离`。若 `transposed_centroids` 非空（`faiss/impl/ProductQuantizer.cpp:448`），改走 `fvec_L2sqr_ny_transposed` 变体，并额外用 `centroids_sq_lengths` 摊薄平方范数的重复计算 —— 转置表由 `sync_transposed_centroids()`（`faiss/impl/ProductQuantizer.cpp:909`）在码本变化后重建。

扫描一个倒排表时，单条向量距离 = `Σ_m dis_table[m*ksub + code[m]]`，即 **M 次查表 + M 次加法**，复杂度 `O(M)` 而非 `O(M·dsub)`。

**SDC（对称）**：`search_sdc()`（`faiss/impl/ProductQuantizer.cpp:856`）要求查询端也编码，且 `nbits` 必须为 8（`faiss/impl/ProductQuantizer.cpp:863` 的断言）。它查的表是 `sdc_table`，尺寸 `M × ksub × ksub`（`faiss/impl/ProductQuantizer.cpp:863` 断言）—— 也就是**两个码字之间的距离表**，体积是 ADC 表的 `ksub` 倍：

```
# 预计算每个查询的 m 个行指针（faiss/impl/ProductQuantizer.cpp:884-888）
q_row[m] = sdc_table + m*ksub*ksub + qcode[m]*ksub

# 内层扫描（faiss/impl/ProductQuantizer.cpp:893-895）
for j in 0..nb-1:
    dis = 0
    for m in 0..M-1:
        dis += q_row[m][bcode[m]]     # ← 双重索引，单次内存访问
```

SDC 省掉了「查询端算 `M × ksub × dsub` 距离表」的开销，代价是查询引入量化误差 —— 这正是 25.2 节里 `np` 后缀的取舍点。SDC 的 `sdc_table` 预计算在 `IndexPQ::search_core` 里，属于**每次查询一次**的 O(M·ksub·dsub)，与库规模无关。

### 25.5 IVFPQ 的预计算反查表 `use_precomputed_table`

这是 IVFPQ 最有价值也最容易配错的优化。`faiss/IndexIVFPQ.cpp:377` 的文档注释给出了完整推导。设查询 `x`、粗量化质心 `y_C`、残差码重建的 PQ 向量 `y_R`：

```
d = || x - y_C - y_R ||²
  = ||x - y_C||²  +  ||y_R||²  +  2·(y_C|y_R)  −  2·(x|y_R)
      ── term 1 ──   ── term 2 ──   ── term 3 ──
      第一阶段已有   与 x 无关，     非残差 ADC
      （粗排距离）   可预计算        距离表
```

- **term 1** 是粗量化器距离，第一阶段 `nprobe` 阶段已经算过，**零成本**
- **term 3** 是普通的非残差 ADC 距离表，每查询算一次
- **term 2** `||y_R||²` 与查询无关，可以预计算成 `nlist × M × ksub` 的表 —— 但这个表**很大**，所以源码明确写着「this is why use_precomputed_table is off by default」（`faiss/IndexIVFPQ.cpp:395`）

因此收益条件是源码给出的那句总结：`At search time, the tables for term 2 and term 3 are added up. This is faster when the length of the lists is > ksub * M.`（`faiss/IndexIVFPQ.cpp:404`）—— **倒排表平均长度必须超过 `ksub × M`，否则预计算的读取开销超过省下的计算**。

`use_precomputed_table` 是三态开关，字段在 `faiss/IndexIVFPQ.h:47`：

| 值 | 含义 | 何时由 0 自动升级 |
|----|------|------------------|
| `-1` | 显式禁用，resize(0) 直接返回 | 用户显式设置 |
| `0` | 不预计算（**默认值**，构造函数 `faiss/IndexIVFPQ.cpp:543`） | —— |
| `1` | 全量表 `nlist × M × ksub` | 粗量化器**不是** MultiIndexQuantizer，且 `M·ksub·nlist·4 ≤ precomputed_table_max_bytes` |
| `2` | 紧凑表 `cpq.ksub × M × ksub` | 粗量化器是 MultiIndexQuantizer 且 `pq.M % miq->pq.M == 0`（`faiss/IndexIVFPQ.cpp:438`） |

**自动升级的两个硬前提**（`faiss/IndexIVFPQ.cpp:428`）：`quantizer->metric_type == METRIC_L2 && by_residual`。内积模式或 `by_residual=false` 时直接返回空表。内存上限 `precomputed_table_max_bytes = 2^31`（`faiss/IndexIVFPQ.cpp:375`），超了会打日志并退回 `use_precomputed_table = 0`（`faiss/IndexIVFPQ.cpp:445`）。

三条执行分支在 `QueryTables::precompute_list_tables_L2()`（`faiss/impl/pq_code_distance/IVFPQ_QueryTables.cpp:129`）：
- `0 / -1`：算残差 + 现算距离表（`faiss/impl/pq_code_distance/IVFPQ_QueryTables.cpp:129`）
- `1`：`fvec_madd_dispatch(M*ksub, precomputed_table + key*M*ksub, -2.0, sim_table_2, sim_table)` 融合加法（`faiss/impl/pq_code_distance/IVFPQ_QueryTables.cpp:137`）
- `2`：按 MIQ 的子码位逐段解出 `ki`，再用 `fvec_madd_and_argmin_dispatch` 直接出 argmin（`faiss/impl/pq_code_distance/IVFPQ_QueryTables.cpp:152`）

`IndexIVFPQ::train()` 在 `by_residual` 为真时会自动调 `precompute_table()`（`faiss/IndexIVFPQ.cpp:92`）。**反查表不写进索引文件** —— `faiss/impl/index_read.cpp:1559` 明确注释「The precomputed table is not stored; precompute_table() rebuilds it on read」，反序列化时在 `faiss/impl/index_read.cpp:1634` 重建，并且尊重 `use_precomputed_table` 标志位（`faiss/impl/index_read.cpp:1628`）。

### 25.6 SQ：per-dim vs uniform 两条缩放路径

`ScalarQuantizer` 的所有类型都在 `faiss/impl/ScalarQuantizer.h:27` 的 `QuantizerType` 枚举里，1.15.0 共 20+ 种。它们共用一个「先归一化到 [0,1]，再交给 Codec 位打包」的两段结构（`faiss/impl/scalar_quantizer/quantizers.h:56` 的注释）。

**缩放段有两种模板**：

- **NON_UNIFORM（per-dim）**：`QuantizerTemplate<..., NON_UNIFORM>`（`faiss/impl/scalar_quantizer/quantizers.h:107`）持有 `vmin[i]` / `vdiff[i]` 两个 d 长度数组
- **UNIFORM**：`QuantizerTemplate<..., UNIFORM>`（`faiss/impl/scalar_quantizer/quantizers.h:66`）只持有标量 `vmin` / `vdiff`，所有维度**共享**同一区间（`faiss/impl/scalar_quantizer/quantizers.h:73`）

缩放公式（`faiss/impl/scalar_quantizer/quantizers.h:80` / `faiss/impl/scalar_quantizer/quantizers.h:122`）：
```
xi = (x[i] - vmin[i]) / vdiff[i];  clamp(xi, 0, 1);  Codec::encode_component(xi)
x[i] = vmin + Codec::decode_component(code) * vdiff    # 解码反变换，faiss/impl/scalar_quantizer/quantizers.h:93
```

**Codec 层的位打包公式**（`faiss/impl/scalar_quantizer/codecs.h`）：

| Codec | 编码 | 解码 | 源码 |
|-------|------|------|------|
| `Codec8bit` | `code[i] = (int)(255 * x)` | `(code[i] + 0.5) / 255.0` | `faiss/impl/scalar_quantizer/codecs.h:33`、`faiss/impl/scalar_quantizer/codecs.h:38` |
| `Codec4bit` | `code[i/2] \|= (int)(x*15) << ((i&1)<<2)` | `((code[i/2] >> ((i&1)<<2)) & 0xf) + 0.5) / 15.0` | `faiss/impl/scalar_quantizer/codecs.h:52`、`faiss/impl/scalar_quantizer/codecs.h:57` |

**code_size 的实际取值**（`faiss/impl/ScalarQuantizer.cpp` 的 `set_derived_sizes`）：`QT_8bit*` → `d`（`faiss/impl/ScalarQuantizer.cpp:490`）；`QT_4bit*` → `(d+1)/2`（`faiss/impl/ScalarQuantizer.cpp:497`）；`QT_6bit` → `(d*6+7)/8`（`faiss/impl/ScalarQuantizer.cpp:506`）；`QT_fp16` / `QT_bf16` → `d*2`（`faiss/impl/ScalarQuantizer.cpp:513`）；`QT_0bit` → `0`（`faiss/impl/ScalarQuantizer.cpp:522`，纯 IVF 场景下只比质心）。

精度/内存比一目了然：`SQ8` = 4×压缩、`SQ4` = 8×、`SQ6` = 5.33×、`SQfp16` = 2×（无损位模式，**不做训练**，见 `faiss/impl/ScalarQuantizer.cpp:590` 的 `// no training necessary`）、`SQ0` = ∞（退化成 IVF 质心比较）。

**uniform 的适用边界**：per-dim 每维独立区间，对各维量纲差异大的数据（如 TF-IDF 向量）精度明显更高，但训练出的参数是 d 个区间，且 SIMD 摊销差；uniform 只有一个区间，SIMD 友好但遇到异量纲数据会被最大 range 撑爆，导致其余维度分辨率塌陷。工厂串里 `SQ8` 走 per-dim，`SQ8_uniform` / `SQ4_uniform` 走 uniform（枚举定义 `faiss/impl/ScalarQuantizer.h:30`）。

### 25.7 OPQ：交替优化 + Procrustes 旋转

OPQ 的结构定义在 `faiss/VectorTransform.h:255`（论文引用在 `faiss/VectorTransform.h:252`）。它是 `LinearTransform` 的子类，核心训练过程在 `OPQMatrix::train()`（`faiss/VectorTransform.cpp:1208`）：

```
输入 x（n × d_in），输出目标 x'（n × d_out），M 个子量化器

1. 下采样到 max_train_points = 256*256                      (faiss/VectorTransform.h:261, cpp:1209)
2. 对 x 中心化（减去每维均值）                                (faiss/VectorTransform.cpp:1244-1262)
3. 初始化旋转矩阵 A：
     A ← float_randn(d×d, seed=1234);  A ← matrix_qr(A)      (faiss/VectorTransform.cpp:1275-1276)
   （即 d 个高斯向量做 QR 分解，得到随机正交矩阵）
4. 外层迭代 niter = 50 次：
     x' ← Aᵀ x                                                # 投影
     PQ 训练/更新码本 C（首轮 niter_pq_0=40 次，后续 niter_pq=4 次）
     x'' ← Reconstruct_C(x')                                   # 量化-重建
     求解 Procrustes 问题： min_A' ‖x' − A'·x''‖²
     → A' ← argmin，即 SVD(x'ᵀx'') 的左右奇异向量乘积
     A ← A'（旋转矩阵更新）
```

参数默认值在 `faiss/VectorTransform.h:257`：`niter = 50`（外层）、`niter_pq = 4`、`niter_pq_0 = 40`（首轮 PQ 训练更充分，因为初始码本质量直接决定后续收敛质量）。

**训练代价**：外层 50 轮 × 内层 4 次 PQ 训练，每次 PQ 训练本身就是 `O(niter_pq · n · d · ksub)`。相比裸 PQ 大约 **50~100× 的训练时间** —— 这是 OPQ 换召回率的真实价格，源码为此把 `max_train_points` 压到 65536（`faiss/VectorTransform.h:261`）来兜底。

OPQ 之所以有效：裸 PQ 的段切分是**沿原始维度轴硬切**的（第 m 段就是第 m 个连续维度区间），如果数据在原始维度上各向异性很强（不同维方差差几个数量级），每段聚类就很难形成紧致簇。OPQ 先用一个**正交旋转**（保范数、不引入额外失真）对齐维度轴，再切段 —— 旋转矩阵学的是「怎么把能量最集中的方向摆到同一段里」。

`IndexPreTransform`（`faiss/IndexPreTransform.h:25`）负责把变换链挂在索引前面：`chain` 是 `std::vector<VectorTransform*>`（`faiss/IndexPreTransform.h:26`），`prepend_transform()` 会断言 `ltrans->d_out == d` 然后 `d = ltrans->d_in`（`faiss/IndexPreTransform.cpp:47`）。`apply_chain()`（`faiss/IndexPreTransform.cpp:122`）在每次 `train` / `add` / `search` 时**逐级重算中间向量**（用 `unique_ptr` 链式转移所有权避免拷贝），所以链越长开销越大 —— 实际用 1~2 级为宜。

### 25.8 残差量化 RQ：位掩码式 train_type

`ResidualQuantizer`（`faiss/impl/ResidualQuantizer.h:27`）继承 `AdditiveQuantizer`，与 PQ 的根本区别是**逐级对残差做 k-means**：`x⁰ = x`，第 m 级量化 `x^{m-1}` 得 `c_m`，`x^m = x^{m-1} − c_m`。

它的 `train_type` 不是枚举而是**位掩码**（源码注释 `faiss/impl/ResidualQuantizer.h:31` 直言「Was enum but that does not work so well with bitmasks」），默认 `Train_progressive_dim`（`faiss/impl/ResidualQuantizer.h:34`）：

| 位 | 名称 | 语义 |
|----|------|------|
| `0` | `Train_default` | 常规 k-means，计算量最小 |
| `1` | `Train_progressive_dim` | **默认**，渐进维度聚类 |
| `2` | `Train_refine_codebook` | 首级估计后再跑 `niter_codebook_refine = 5` 轮码本精修 |
| `1024` | `Train_top_beam` | 束搜索只训练 beam 首位（更快、精度更低） |
| `2048` | `Skip_codebook_tables` | 训练后不自动算码本查表 |

编码时用**束搜索**（beam search）而非贪心：`max_beam_size = 5`（`faiss/impl/ResidualQuantizer.h:57`），每级保留 top-5 残差候选继续往下走，复杂度 `O(ksub · beam · M)` 而非贪心的 `O(ksub · M)`。`use_beam_LUT`（`faiss/impl/ResidualQuantizer.h:59`）控制是否用查表加速束内比较。

### 25.9 FastScan：4-bit LUT 的内存布局

`IndexIVFPQFastScan` 的类注释（`faiss/IndexIVFPQFastScan.h:19`）写得很直白：「Works for 4-bit PQ for now」。加速来自两处**布局重排**：

**数据库端：按 block 重排**。码不再顺序存放，而是按 `bbs` 分块（`faiss/IndexIVFPQFastScan.h:21`）。`bbs` 必须是 32 的倍数（构造函数断言 `faiss/IndexFastScan.cpp:39`），总向量数向上取整到 `bbs` 的倍数（`faiss/IndexFastScan.cpp:81`），块数 `ntotal2 / bbs`（`faiss/IndexFastScan.cpp:82`）。打包由 `pq4_pack_codes()`（`faiss/impl/fast_scan/fast_scan.h:34`）完成，未用字节填 0。

**查询端：LUT 打包**。`pq4_pack_LUT()`（`faiss/impl/fast_scan/fast_scan.cpp:287`）把每个查询的距离表压成 **每子量化器 16 字节**，即每查询 `16 × nsq` 字节（`faiss/impl/fast_scan/fast_scan.cpp:302`）。4-bit 码只有 16 种取值，所以 `float` 距离表先被量化成 8-bit 相对值再打包 —— 一个 32 字节的 SIMD 寄存器正好装下 **1 个查询的 2 个子量化器行**（16 + 16）。

**内核累加器**：`kernel_accumulate_block`（`faiss/impl/fast_scan/kernels_simd256.h:29`）用 `simd16uint16 accu[NQ][BB][4]`（`faiss/impl/fast_scan/kernels_simd256.h:40`）—— 累加器是 `simd16uint16`（16 × uint16 = 32 字节），内层循环**每次推进 2 个子量化器**（`faiss/impl/fast_scan/kernels_simd256.h:51` 的 `sq += 2`）：先用 `simd16uint16(c) >> 4` 拆出高 4 位、`c & 15` 拆出低 4 位（`faiss/impl/fast_scan/kernels_simd256.h:62`、`:63`），再对 32 字节 LUT 寄存器调 `lookup_2_lanes(clo)` / `lookup_2_lanes(chi)`（`faiss/impl/fast_scan/kernels_simd256.h:67`、`:68`）—— 对应 AVX2 的 `_mm256_shuffle_epi8` / AVX-512 的 `_mm512_shuffle_epi8`，**在两个 128-bit lane 内各做一次 16 项查表**。两次查表结果转成 uint16 后累加进 `accu`（`faiss/impl/fast_scan/kernels_simd256.h:70`）。这是**纯访存 + 查表**，没有乘加，理论上限极高。

`implem` 参数（`faiss/IndexIVFPQFastScan.h:25`）可选 0（自动）/ 1（原版重实现）/ 2（按倒排表重排）/ 10~13（int16 优化器 + qbs + 堆或蓄水池收集）。

---

## 二十六、HNSW 图索引、Refine / IDMap 组合与距离度量陷阱

### 26.1 存储布局：扁平邻居数组

Faiss 的 HNSW 实现**不是** `hnswlib`（仓库里没有 `hnswlib/` 目录），而是自有类 `HNSW`（`faiss/impl/HNSW.h:63`，文件名首字母大写）。

它用**三段式扁平数组**而非 `vector<vector<int>>`：

```
levels[i]                                    # 每点最高层，size = ntotal          (faiss/impl/HNSW.h:121)
offsets[i], offsets[i+1]                     # 邻居区起始偏移，size = ntotal+1    (faiss/impl/HNSW.h:125)
neighbors[offsets[i]:offsets[i+1]]           # 所有层邻居连续存放                   (faiss/impl/HNSW.h:129)
```

一层邻居数是 `cum_nb_neighbors(layer+1) - cum_nb_neighbors(layer)`（`faiss/impl/HNSW.cpp:33`），由构造时的 `set_default_probas()` 预先算好。节点 ID 是 `int32_t`（`faiss/impl/HNSW.h:65`，注释直言「this is expensive」）—— 超过 21 亿向量会溢出。

### 26.2 层级采样公式

`random_level()`（`faiss/impl/HNSW.cpp:66`）不是 `floor(-ln(U)·levelMult)`，而是**查累积概率表**：预先算好的 `assign_probas[level]` 满足

```
p(level) = exp(-level / levelMult) · (1 − exp(-1 / levelMult))     (faiss/impl/HNSW.cpp:83)
```

`levelMult` 默认 `1/ln(M)`（`faiss/impl/HNSW.cpp:62`），M=32 时 `levelMult ≈ 0.2885`，故每上一层概率约 `exp(-3.466) ≈ 3.1%`。循环在 `p < 1e-9` 时终止（`faiss/impl/HNSW.cpp:84`）。`random_level()` 从 `rng.rand_float()` 取 U，按累积概率逐层减去直到落入某层（`faiss/impl/HNSW.cpp:70`），代码注释指出用二分查找可以更快。

**每层边数不同**：level 0 是 `2M`，其他层是 `M`（`faiss/impl/HNSW.cpp:88`）—— level 0 边多是因为它承载全部查询流量。

### 26.3 `search_layer` 贪心伪代码

Faiss 把搜索拆成「上层贪心下降」+「底层 ef 束搜索」两段。`search_impl()`（`faiss/impl/HNSW.cpp:1685`）：

```
nearest ← hnsw.entry_point;  d_nearest ← dist(q, nearest)

# 阶段 A：高层贪心下降，ef = 1，纯「谁更近就跳过去」
for level = max_level down to 1:
    greedy_update_nearest(nearest, level)          # faiss/impl/HNSW.cpp:1713
    # 内层：遍历 nearest 在该层的全部邻居，只要存在更近的就替换，循环到不动点
    #        实现见 greedy_update_nearest_impl, faiss/impl/HNSW.cpp:701

# 阶段 B：level 0 的 ef 束搜索
ef ← max(efSearch, k)                             # faiss/impl/HNSW.cpp:1718
candidates ← min-heap{ (d_nearest, nearest) }     # faiss/impl/HNSW.cpp:1723
search_from_candidates(candidates, level=0)
```

`greedy_update_nearest_impl()`（`faiss/impl/HNSW.cpp:701`）的终止条件是 `nearest == prev_nearest`（`faiss/impl/HNSW.cpp:770`）—— 走不动了才停。它同样有 4 路批处理优化：攒满 4 个邻居 ID 就调 `qdis.distances_batch_4(...)`（`faiss/impl/HNSW.cpp:742`）一次算 4 个距离，再逐个比较。

### 26.4 底层 ef 束搜索与 `efSearch` 的两种停止条件

`search_from_candidates_fixVT()`（`faiss/impl/HNSW.cpp:1123`）是真正的 `search_layer`。它用**双堆**：`ResultHandler` 维护 top-k 结果，`MinimaxHeap candidates` 是待探索节点的 min-heap。

```
vt ← VisitedTable，预分配 efSearch 位                (faiss/impl/HNSW.cpp:1141)
while candidates 非空:
    (d0, v0) ← candidates.pop_min()                (faiss/impl/HNSW.cpp:1161)

    # 停止条件 A（check_relative_distance = true，默认）
    if candidates.count_below(d0) >= efSearch: break    (faiss/impl/HNSW.cpp:1170)

    # 预取：先扫一遍邻居，把 4 个一批塞进 visited set
    for v1 in neighbors(v0, level):
        vt.prefetch(v1); saved_j[counter++] ← v1      (faiss/impl/HNSW.cpp:1208)

    for 每 4 个邻居:
        qdis.distances_batch_4(...)                     # 批量距离
        对每个 (dis, idx)：
            if idx 通过 IDSelector 且 dis 优于阈值:  加进结果堆
            candidates.push(idx, dis)                   # 无论是否入结果都进候选堆

    # 停止条件 B（check_relative_distance = false）
    if nstep > efSearch: break                          (faiss/impl/HNSW.cpp:1244)
```

两种停止条件的语义差别很大（源码注释在 `faiss/impl/HNSW.cpp:1165`）：**条件 A 是「候选堆里距 d0 更近的点已达 efSearch 个，说明再探索也不会找到更好的」**；条件 B 只是「走了 efSearch 跳就停」。默认值 `check_relative_distance = true`（`faiss/impl/HNSW.h:152`），走条件 A。生产上把 `efSearch` 从 16（默认，`faiss/impl/HNSW.h:144`）往上调，召回率单调上升、延迟近似线性上升。

### 26.5 邻居剪枝启发式 `shrink_neighbor_list`

HNSW 论文的「多样性剪枝」在 `HNSW::shrink_neighbor_list()`（`faiss/impl/HNSW.cpp:244`）：

```
从大到小遍历候选（优先保留最近的）
  对候选 v1：
    对每个已保留的 v2：
      若 d(v1, v2) < d(v1, q)   →  v1 "冗余"，丢弃      # faiss/impl/HNSW.cpp:263 附近
    若不冗余 → 保留；否则若 keep_max_size_level0 → 暂存 outsiders
保留数不足 max_size 时，从 outsiders 补齐            # faiss/impl/HNSW.cpp:283 起
```

即：若 v1 的某个**已被保留的邻居** v2 离 v1 比查询 q 离 v1 还近，说明 v1 处于「拥挤角」，边 v1 不会带来新的搜索方向，故剪掉。

`keep_max_size_level0`（level 0 传 true）保证 0 层不会被剪到少于 `2M` 条边 —— 源码注释指出这是 `GpuIndexCagra::copyFrom(IndexHNSWCagra*)` 能工作的前提（`faiss/impl/HNSW.cpp:253`）。`add_link_tpl` 每次插入还会先按 `prune_headroom = 0.2f`（`faiss/impl/HNSW.h:148`）预留 20% 余量（`faiss/impl/HNSW.cpp:410`），避免频繁触发 O(n²) 的全量剪枝 —— 默认 `HNSW(int M = 32)`（`faiss/impl/HNSW.h:191`）。

### 26.6 Refine 两阶段重排：`RFlat` 的内部机制

`IndexRefine`（`faiss/IndexRefine.h:24`）持两个索引：`base_index`（快、粗排）与 `refine_index`（准、精排），核心参数 `k_factor`（`faiss/IndexRefine.h:36`）—— **向 base 要结果的放大倍数**，要求 `≥ 1`。

`IndexRefineFlat::search()`（`faiss/IndexRefine.cpp:278`）的实际流程：

```
k_base ← k * k_factor                              (faiss/IndexRefine.cpp:292)
断言 k_base >= k                                   (faiss/IndexRefine.cpp:297)

# 阶段 1：粗排
base_index->search(n, x, k_base, base_distances, base_labels)   (faiss/IndexRefine.cpp:318)
每个 base_labels[i] 断言在 [0, ntotal) 范围内                    (faiss/IndexRefine.cpp:322)

# 阶段 2：精算 —— 注意是「只算这 k_base 个候选」，不是全库重搜
rf ← dynamic_cast<IndexFlat*>(refine_index)         (faiss/IndexRefine.cpp:326)
rf->compute_distance_subset(n, x, k_base, base_distances, base_labels)
                                                        (faiss/IndexRefine.cpp:329)

# 阶段 3：重排取前 k
if metric_type == METRIC_L2:      reorder_2_heaps<CMax<...>>   (faiss/IndexRefine.cpp:332)
elif metric_type == METRIC_INNER_PRODUCT: reorder_2_heaps<CMin<...>>
else: FAISS_THROW_MSG("Metric type not supported")
```

复杂度对比很直观：粗排 `O(N/B · log(B) + k_base·B)`，精算 `O(k_base · log(k_base))`，总体多出的成本与 `k_factor` **线性相关**而与库规模无关。`k_factor = 1` 时精排等于没做（白花钱）；常用 `4~20`。

关键实现细节：`compute_distance_subset` 是 `IndexFlat` 特有的能力，所以 `IndexRefineFlat` 在 `faiss/IndexRefine.cpp:326` 断言 refine 索引**必须是** `IndexFlat`；通用 `IndexRefine` 则走完整的 `refine_index->search`（`faiss/IndexRefine.cpp:393`，Panorama 分支）。

### 26.7 IDMap / IDMap2：外部 ID 映射

`IndexIDMapTemplate`（`faiss/IndexIDMap.h:21`）持一个 `std::vector<idx_t> id_map`（`faiss/IndexIDMap.h:27`），内部第 i 个向量对应外部 ID `id_map[i]`。它的 `add()` 被**故意禁用**（`faiss/IndexIDMap.h:41` 只留声明，实现里直接抛异常），必须用 `add_with_ids()` —— 这是因为外部 ID 不保证连续或单调递增。

`IndexIDMap2Template`（`faiss/IndexIDMap.h:94`）多维护一个 `std::unordered_map<idx_t, idx_t> rev_map`（`faiss/IndexIDMap.h:98`），从而支持 O(1) 的 `reconstruct()` 和 `remove_ids()`。代价是额外一份哈希表内存 + 随机访问。**只做 top-k 检索就用 IDMap；需要按外部 ID 精确删除/重建向量才上 IDMap2。**

工厂串里 `IDMap` / `IDMap2` **既能作前缀也能作后缀**（`faiss/index_factory.cpp:976` 与 `faiss/index_factory.cpp:986` 分别匹配 `(.+),IDMap` 和 `IDMap,(.+)`），源码注释（`faiss/index_factory.cpp:974`）解释这是历史兼容 —— 两种写法等价。

### 26.8 陷阱一：`METRIC_INNER_PRODUCT` 不等于余弦相似度

`faiss/MetricType.h:29` 的枚举只有 `METRIC_INNER_PRODUCT`（最大内积）和 `METRIC_L2`（**平方** L2），**没有 `METRIC_COSINE`** —— 余弦必须自己归一化后转成内积。

等价式（对非零向量成立）：

```
            x·y                              ⟨x̂, ŷ⟩
cos(x, y) = ─────  ，其中  x̂ = x/‖x‖₂ ,  ŷ = y/‖y‖₂
            ‖x‖‖y‖
```

即 `x·y = ‖x‖·‖y‖·cos(x,y)`。**只有当两侧都归一化到单位长度（‖x‖=‖y‖=1）时，`x·y` 才恰好等于 `cos(x,y)`，且此时 `‖x − y‖₂² = 2 − 2·x·y`（与 L2 完全等价）。** 一般向量下最大内积会系统性偏好长向量，与余弦的排序不同。

所以用内积做余弦检索的标准流程是**索引和查询两侧都归一化**：

```python
faiss.normalize_L2(xb)   # 入库前
faiss.normalize_L2(xq)   # 查询前
index = faiss.index_factory(d, "IVF4096,Flat", faiss.METRIC_INNER_PRODUCT)
```

`faiss.normalize_L2()` 是 Python 薄封装（`faiss/python/extra_wrappers.py:147`），底层调 `fvec_renorm_L2()`（`faiss/utils/distances.h:238`，实现 `faiss/utils/distances.cpp:269`）。也可以直接用工厂串 `"L2norm,IVF4096,Flat"`（`faiss/index_factory.cpp:236` 会构造 `NormalizationTransform`，类定义 `faiss/VectorTransform.h:301`）—— 但注意 `NormalizationTransform` 的 `reverse_transform` 是**恒等变换**（`faiss/VectorTransform.h:307` 的注释「Identity transform since norm is not revertible」），所以 `reconstruct` 拿回的是归一化后的向量，**不能直接当原始向量用**。

### 26.9 陷阱二：`METRIC_L2` 返回的是平方距离

`faiss/MetricType.h:31` 的注释明写 `squared L2 search`。这不影响 top-k 排序（`sqrt` 单调），但有两个实际后果：

1. **不要对返回值做二次开方**：`search` 返回的 `D` 已经是 `‖x−y‖²`，再 `sqrt` 是错的
2. **`IndexRefine` 的重排比较器按度量类型分叉**：L2 走 `CMax<float, idx_t>`（`faiss/IndexRefine.cpp:332`），内积走 `CMin<float, idx_t>`，其余度量直接抛 `Metric type not supported`（`faiss/IndexRefine.cpp:341`）。所以 `RFlat` 重排**只支持 L2 与内积两种度量**

`HNSW` 也有同样的二义性，通过 `is_similarity` 标志切换比较器语义（`faiss/impl/HNSW.h:163`）：`false` 时距离小者优（`C_distance = CMax`），`true` 时相似度大者优（`C_similarity = CMin`，`faiss/impl/HNSW.h:70`）。源码注释（`faiss/impl/HNSW.h:164`）特别提醒：`is_similarity` **不参与序列化**，反序列化后必须由上层索引重新设置。

---

## 二十七、GPU 执行模型、CPU 并行与召回率评估

本节补 §8.6 未展开的执行层细节：GPU 到底怎么切 tile、CPU 的 OpenMP 并行粒度、以及**怎么量化地测召回率**。

### 27.1 GPU 的 tile 式批处理：一切从「显存装不下」倒推

GPU 检索的核心矛盾是：**倒排表可能装不进显存**。Faiss 的应对不是「分片 + 多次往返」，而是**在显存允许的窗口内尽可能多地并发查询**。

`getIVFQueryTileSize()`（`faiss/gpu/impl/IVFUtils.cu:67`）就是这个决策点：

```
输入：numQueries（总查询数）、tempMemoryAvailable（可用临时显存）、sizePerQuery（单查询开销）

kMinQueryTileSize = 8        # 理想最小并发查询数            (faiss/gpu/impl/IVFUtils.cu:72)
kMaxQueryTileSize = 65536    # 硬上限，受 CUDA max grid Y 维约束 (faiss/gpu/impl/IVFUtils.cu:76)

withinTempMemoryNumQueries = min(tempMemoryAvailable / sizePerQuery, numQueries)
withinTempMemoryNumQueries = min(withinTempMemoryNumQueries, kMaxQueryTileSize)  (faiss/gpu/impl/IVFUtils.cu:87)

# 若连 8 个查询都放不下（临时显存被大列表挤爆），放弃临时显存改用通用分配
if withinTempMemoryNumQueries < numQueries and withinTempMemoryNumQueries < kMinQueryTileSize:
    kMinMemoryAllocation = 512 MiB                                      (faiss/gpu/impl/IVFUtils.cu:108)
    withinMemoryNumQueries = min(kMinMemoryAllocation / sizePerQuery, numQueries)
    ...再不够就退回单查询串行
```

所以 GPU 端的实际行为是：**把查询集切成 `queryTileSize` 个一批，逐批流过 GPU**。外层循环就在 `faiss/gpu/impl/IVFFlatScan.cu:441`，`for (query = 0; query < nq; query += queryTileSize)`。每批内部再按 `nprobe` 个倒排表展开成 `{queryTileSize, nprobe, maxListLength}` 的 3-D 张量（`faiss/gpu/impl/IVFFlatScan.cu:405`），第二遍 top-k 选择按 `pass2Chunks`（`faiss/gpu/impl/IVFFlatScan.cu:413`）再分块，避免一次性为所有 chunk 分配堆内存。

**线程块配置是固定的**：`kIVFFlatScanWarps = 4`（`faiss/gpu/impl/IVFFlatScan.cu:49`），launch 时 `dim3 block(warpSize * kIVFFlatScanWarps)`（`faiss/gpu/impl/IVFFlatScan.cu:216`）。非 L2 / 内积的通用距离内核用 `TILE_SIZE = 32`（`faiss/gpu/impl/GeneralDistance.cuh:37`），源码注释解释了为什么不是 `kWarpSize`：在 `kWarpSize=64` 的 AMD 平台上 64×64 的 block 配置非法（`faiss/gpu/impl/GeneralDistance.cuh:33`）。

**硬性上限**：`GPU_MAX_SELECTION_K = 2048`（`faiss/gpu/utils/DeviceDefs.cuh:39`，ROCm 分支为 1024，见 `faiss/gpu/utils/DeviceDefs.cuh:67`），`nprobe` 与 `k` 都不得超过它（`faiss/gpu/impl/IVFFlat.cu:177`）。**GPU 上做 IVF 时 `k > 2048` 会直接断言失败** —— 这是从 CPU 迁到 GPU 最常踩的坑之一。

`add()` 路径的并行维度完全不同：一个 block 处理**一条向量**（`faiss/gpu/impl/IVFAppend.cu:205` 的 `dim3 grid(vecs.getSize(0))`），block 内线程数是 `min(dim / kDimPerIter, maxThreads)`（`faiss/gpu/impl/IVFAppend.cu:206`）—— 维度小则线程少，维度大则多，是「一向量一 block」的并行模型。

### 27.2 CAGRA：GPU 图索引的构建路径

`GpuIndexCagra` 的图构建**不走 HNSW 的逐点插入**，而是先建全量 kNN 图再剪枝。`graph_build_algo`（`faiss/gpu/GpuIndexCagra.h:44`）有四种策略：

| 值 | 含义 | 源码行 |
|----|------|--------|
| `IVF_PQ` | 用 IVF-PQ 递归构造 all-neighbors 图（**默认**） | `faiss/gpu/GpuIndexCagra.h:46` |
| `NN_DESCENT` | NN-Descent，`nn_descent_niter = 20` 轮 | `faiss/gpu/GpuIndexCagra.h:48` |
| `ITERATIVE_SEARCH` | 迭代搜索构图 | `faiss/gpu/GpuIndexCagra.h:50` |
| `BRUTE_FORCE` | tiled 暴力精确构图（正确性基准） | `faiss/gpu/GpuIndexCagra.h:52` |

度数参数是两级：`intermediate_graph_degree = 128`（输入图度数，`faiss/gpu/GpuIndexCagra.h:208`）→ 剪枝 → `graph_degree = 64`（输出图度数，`faiss/gpu/GpuIndexCagra.h:210`）。这就是「先生成 128 邻居再剪到 64」的 oversampling 策略。

`GpuIndexCagra::train()`（`faiss/gpu/GpuIndexCagra.h:324`）在 GPU 上建 CAGRA 图（源码注释在 `faiss/gpu/GpuIndexCagra.h:320` 明确说「The use of the train function here is to build the CAGRA graph」——**不是训练量化器**）。检索期的主旋钮是 `itopk_size = 64`（`faiss/gpu/GpuIndexCagra.h:267`），源码注释称其为「accuracy vs search speed 的主要调节旋钮」（`faiss/gpu/GpuIndexCagra.h:264`），配合 `max_iterations`（`faiss/gpu/GpuIndexCagra.h:270`，0 = 自动）。

### 27.3 CPU 并行：三种 OpenMP 粒度

Faiss 的并行粒度**按成本分档**，不是无脑 `#pragma omp parallel for`：

| 粒度 | 触发条件 | 源码 | 适用 |
|------|---------|------|------|
| **查询级并行**（无 `if` 限定，显式定线程数） | `nt = min(n, omp_get_max_threads())` | `faiss/IndexFlat.cpp:586`、`faiss/IndexFlat.cpp:589` | 大批量查询 |
| **带阈值的查询级并行** | `if (n > 10000)` | `faiss/IndexFlat.cpp:461`（位于 `IndexFlat1D::search`，起于 `faiss/IndexFlat.cpp:447`） | 1-D 标量索引（二分查找 + 线性扫描） |
| **纯数据级并行** | `if (n > 100)` / `if (nx > 1)` | `faiss/impl/ProductQuantizer.cpp:330`、`faiss/impl/ProductQuantizer.cpp:502` | 编解码等细粒度任务 |

关键差异在**线程数计算**：大批量查询路径显式算 `nt = std::min(int(n), omp_get_max_threads())`（`faiss/IndexFlat.cpp:586`）并用 `#pragma omp parallel num_threads(nt)`（`faiss/IndexFlat.cpp:589`）—— 当查询数少于线程数时**按查询数收缩线程**，避免大量线程空转。细粒度内核则直接用默认线程数。

`knn_inner_product()` / `knn_L2sqr()`（`faiss/utils/distances.cpp:773`、`faiss/utils/distances.cpp:839`）内部还叠了一层 BLAS 阈值判断（`faiss/utils/distances.cpp:768`）：只有 `nx` 足够大且 `ny` 超过 `min_ny` 时才走 GEMM，否则走手写 SIMD 内核——矩阵乘的大 batch 优势在小 batch 上会被 GEMM 的调用开销吃掉。

线程数通过标准的 `OMP_NUM_THREADS` 环境变量或 `omp_set_num_threads()` 控制，Faiss 本身**不提供**更高层的线程池抽象。

### 27.4 召回率评估与自动调参

**先说一个必须知道的事实**：`faiss.optimize_search_parameters()` 在 Faiss 1.15.0 中**已不存在**（全仓库无此符号）。替代路径是 C++ 的 `ParameterSpace::explore()`（`faiss/AutoTune.cpp:743`）：

```
1. ps = ParameterSpace()                # 构造时按索引类型填入可调参数范围
   ps.initialize(index)                  # 依据索引实际结构推导参数候选值
   n_comb = ps.n_combinations()          # 各参数候选值的笛卡尔积规模
2. for 每个组合 cno:
     ps.set_index_parameters(index, cno) # 写入索引（如 nprobe / efSearch）
     t0 = getmillisecs(); index->search(...); t_search = ...
     perf = crit.evaluate(D, I)          # 按 AutoTuneCriterion 算 recall@k
     ops->add(perf, t_search, ...)
```

`ParameterSpace`（`faiss/AutoTune.h:131`）的探索控制项：`n_experiments`（`faiss/AutoTune.h:141`，0 = 试全部组合）、`batchsize`（`faiss/AutoTune.h:144`）、`thread_over_batches`（`faiss/AutoTune.h:148`）、`min_test_duration`（`faiss/AutoTune.h:152`，重复跑直到达到该时长以消除 MT 抖动）。`n_experiments > 0` 时会做随机采样，但代码强制保证最慢和最快的两个组合必被测到（`faiss/AutoTune.cpp:790` 的 `perm[0] = 0` 与 `faiss/AutoTune.cpp:792` 的 `perm[1] = n_comb - 1`，注释见 `faiss/AutoTune.cpp:789`）。

**标准评估流程**：

```
① 生成 ground truth：
   contrib/exhaustive_search.py:15  knn_ground_truth(xq, db_iterator, k,
                                        metric_type, shard, ngpu)
   —— 分块迭代器 + IndexFlat，ngpu >= 0 时自动走 GPU
       (contrib/exhaustive_search.py:15 起，ngpu 判定见 contrib/exhaustive_search.py:29)

② CPU 基准：faiss.knn(xq, xb, k)                # 暴力精确
③ GPU 基准：faiss.knn_gpu(res, xq, xb, k, ..., use_cuvs=False)
                                                 # 见 faiss/python/gpu_wrappers.py:60
   —— knn_gpu 不构造索引，单卡跑完即弃，适合做 recall 上界参照

④ 候选索引：index.search(xq, k) → 与 GT 求 R@k
   recall@k = |top_k ∩ GT_k| / k
```

CPU/GPU 对比的正确做法是：**用 `knn_gpu` 的结果作为「GPU 硬件上限」参照，而不是用 `GpuIndexFlat` 与 `IndexFlat` 直接比** —— 因为 `GpuIndexFlat` 内部也走同一套 tile 逻辑，测的是同一件事。

### 27.5 `faiss.contrib` 的实际可用状态

`contrib/` 编成独立的 `faiss.contrib` 模块（`contrib/README.md:8`），但 README 紧接着警告（`contrib/README.md:9`）：**依赖 GPU Faiss / pytorch / hdf5 的模块并不保证被编译进去，「由用户自己负责提供」**。

所以生产代码里 `import faiss.contrib.torch_utils` 之类的写法有风险。已验证会引入可选依赖的模块：

| 模块 | 可选依赖 | 用途 |
|------|---------|------|
| `contrib/datasets.py` | `h5py`（README 原文「may require h5py」，见 `contrib/README.md:51`） | SIFT / GIST / GloVe 等标准数据集读取 |
| `contrib/torch_utils.py`（`contrib/README.md:32`） | `torch`（+ GPU Faiss） | PyTorch 张量互转、`torch_replacement_knn_gpu` |
| `contrib/rpc.py`（`contrib/README.md:13`）+ `contrib/client_server.py`（`contrib/README.md:17`） | 无（自带 pickle RPC） | 多机分片检索 |

另有 `contrib/exhaustive_search.py`（GPU 加速的精确 ground truth 计算）、`contrib/ondisk.py`（On-disk 索引合并）、`contrib/ivf_tools.py`（替换 IVF 粗量化器）、`contrib/factory_tools.py`（工厂串工具，`contrib/README.md:55`）。**库本身可无依赖编译，实验工具才需要额外装包** —— 这是设计意图。

---

## 二十八、参考资料

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