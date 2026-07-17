# VectorBench 增强设计：C 库构建、Embedding C API、大规模测试、全链路优化与可视化报告

> **日期**: 2026-07-14
> **状态**: 设计稿

## 1. 背景与目标

VectorBench 已实现 Python 原型（Phase 1-5），但存在三个核心缺口：

1. **C 库未编译**：`vector_index_c_api.c` 已写但未构建，ctypes 绑定自动回退到纯 Python 占位实现，无法获得真实索引性能
2. **Embedding 依赖 Python 生态**：需要 `sentence-transformers` + PyTorch，部署重且延迟高
3. **测试规模小**：仅在 fashion-mnist 60K 上验证，未在大规模数据集上跑出可对比的基线
4. **报告格式为 Markdown**：缺乏交互式可视化能力

本设计的目标是补齐这四个缺口，使 VectorBench 能产出真实、可对比、可展示的性能报告。

### 性能基线

以本地 faiss 实现为性能基线，所有优化目标为持平或接近 faiss 同等参数下的表现。

### 执行顺序

Phase 1 → 2 → 3 → 4 → 5，每阶段有可验证交付物。

## 2. 系统架构

```
┌──────────────────────────────────────────────────────────────┐
│                   Python Benchmark 框架                        │
│  benchmark/main.py + core/ + adapters/ + quantization/        │
└──────────┬────────────────────────────┬───────────────────────┘
           │ ctypes                      │ ctypes
           ▼                             ▼
┌─────────────────────┐   ┌───────────────────────────────┐
│  libvector_index_   │   │  infra_embed_c_api.dll         │
│  c_api.dll           │   │  (C Embedding 推理)           │
│  (索引 C API)       │   │                               │
└──────────┬──────────┘   └──────────┬────────────────────┘
           │                          │
           ▼                          ▼
┌─────────────────────┐   ┌───────────────────────────────┐
│  HNSW / DiskAnn     │   │  GGUF 模型                     │
│  / LSH (C 实现)     │   │  (all-MiniLM-L6-v2 等)        │
└─────────────────────┘   └───────────────────────────────┘
```

两个独立的动态库：

| 库 | 功能 | Python 端 |
|------|------|-----------|
| `libvector_index_c_api.dll` | HNSW / IVF-PQ / LSH 索引操作 | `CIndexAdapter` 包装 |
| `infra_embed_c_api.dll` | 文本 Embedding 推理 | `EmbeddingCAPI` 包装 |

## 3. Phase 1：C 库构建

### 3.1 目标

编译生成 `libvector_index_c_api.dll`，ctypes 可成功加载并调用真实索引操作。

### 3.2 当前问题

现有 `c_api/CMakeLists.txt` 的问题：
- `find_path` 搜索路径硬编码 `../../../include`，跨平台兼容性差
- `add_subdirectory` 引用了 `hnsw/diskann/delete` 子目录，这些目录没有独立 CMakeLists.txt
- 输出目录设为 `../lib`，但 ctypes 绑定在 `build/engineering/` 下查找

### 3.3 修复方案

重写 `c_api/CMakeLists.txt`，链接工程主构建系统已编译的静态库，不再独立 `add_subdirectory`：

```cmake
cmake_minimum_required(VERSION 3.20)
project(vector_index_c_api C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# 链接工程已有的索引库
target_link_libraries(vector_index_c_api PRIVATE
    hnsw_index
    diskann_index
    vector_delete
)

add_library(vector_index_c_api SHARED vector_index_c_api.c)

if(WIN32)
    target_compile_definitions(vector_index_c_api PRIVATE VECTOR_INDEX_EXPORTS)
endif()

# 输出到 build/engineering/，与 ctypes 查找路径一致
set_target_properties(vector_index_c_api PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
```

### 3.4 集成方式

作为工程构建的一部分，通过 `--target vector_index_c_api` 单独编译：

```bash
cmake --build build/engineering --target vector_index_c_api --config Release
```

### 3.5 验证

```python
from benchmark.c_api.ctypes_bindings import load_library
lib = load_library()
assert lib is not None, "C 库加载失败"
```

### 3.6 数据流

```
Python 适配器 (HNSWAdapter)
  └─ ctypes 调用 hnsw_create / hnsw_build / hnsw_search
      └─ C 函数调用 faiss_hnsw_index_create / _add / _search
          └─ 操作内存中的图结构
```

## 4. Phase 2：Embedding C API

### 4.1 目标

将 `kbase/infra` 的 `infra_embed` API 导出为独立的 C 动态库，Python 可通过 ctypes 直接调用，替代 `sentence-transformers` 依赖。

### 4.2 新增文件

```
engineering/src/kbase/embed_c_api/
├── embed_c_api.h       # C API 头文件
├── embed_c_api.c       # C API 实现
└── CMakeLists.txt      # 构建配置
```

### 4.3 API 设计

```c
// embed_c_api.h
void*   embed_create(const char* model_path);
int32_t embed_get_dim(void* handle);
int32_t embed_encode(void* handle, const char** texts, int32_t n,
                     float** embeddings_out, int32_t* dim_out);
void    embed_free(float* embeddings, int32_t n);
void    embed_destroy(void* handle);
```

### 4.4 实现要点

`embed_create`：
1. 调用 `infra_init()` 初始化算子注册表
2. 加载 GGUF 模型文件（解析词表、构建 MiniLM 计算图）
3. 返回 `embed_context_t*`（持有 `infra_model_t*` + dim 信息）

`embed_encode`：
1. 调用 `infra_embed(ctx->model, texts, n, &embeddings, &dim)`
2. 返回 `int32_t` 状态码（0=成功, -1=失败）

### 4.5 Python 包装

新增 `c_api/embed_ctypes_bindings.py`，提供 `EmbeddingCAPI` 类：

```python
class EmbeddingCAPI:
    def __init__(self, model_path: str):
        # 加载 DLL + 调用 embed_create
        # 暴露 .dimension 属性

    def encode(self, texts: List[str]) -> np.ndarray:
        # 1. ctypes 参数准备
        # 2. 调用 embed_encode
        # 3. 结果复制到 numpy 数组
        # 4. 调用 embed_free 释放 C 端内存
```

### 4.6 集成到现有框架

修改 `real_embedding_model.py` 中的 `create_embedding_model()`，增加 C API 优先路径：

```python
if model_path and os.path.exists(model_path):
    try:
        from c_api.embed_ctypes_bindings import EmbeddingCAPI
        return EmbeddingCAPI(model_path)
    except Exception:
        pass  # 回退到 sentence-transformers
```

### 4.7 模型文件

GGUF 格式模型放在 `engineering/tools/benchmark/models/` 目录，通过环境变量 `EMBED_MODEL_PATH` 指定路径。

### 4.8 数据流

```
Python: EmbeddingCAPI.encode(["文本1", "文本2"])
  └─ ctypes: embed_encode(handle, texts, 2, &embeddings_out, &dim_out)
      └─ C: infra_embed(model, texts, 2, &embeddings, &dim)
          └─ MiniLM 计算图执行（token_embed → 6×encoder → pooler → norm）
              └─ 输出 float* 二维数组 (2 × 384)
```

## 5. Phase 3：大规模数据集测试

### 5.1 目标

在 GloVe 1.18M 和 Deep-Image 9.9M（子集）上运行完整基准测试，验证 C 库和 Embedding C API 的正确性与性能基线。

### 5.2 测试策略

两阶段策略，考虑内存限制（全量 Deep-Image 9.9M × 96 × 4B ≈ 3.8GB）：

| 阶段 | 数据集 | 规模 | 内存估算 |
|------|--------|------|----------|
| 3a: 子集验证 | fashion-mnist | 60K | ~180MB |
| 3a: 子集验证 | GloVe 采样 | 100K | ~40MB |
| 3a: 子集验证 | Deep-Image 采样 | 100K | ~38MB |
| 3b: 全量测试 | GloVe 全量 | 1.18M | ~470MB |
| 3b: 全量测试 | Deep-Image 采样 | 1M | ~380MB |

### 5.3 子集采样

在 `DatasetManager` 中增加 `sample()` 方法：

```python
def sample(self, max_samples: int) -> Dataset:
    """均匀采样，返回子集 Dataset"""
    indices = np.linspace(0, len(self.train)-1, max_samples, dtype=int)
    train = self.train[indices]
    test = self.test[:min(len(self.test), max_samples // 10)]
    ...
```

### 5.4 测试参数

| 索引 | 参数（子集） | 参数（全量） |
|------|-------------|-------------|
| HNSW | M=16, ef_c=200, ef_s=64 | M=16, ef_c=200, ef_s=64 |
| IVF-PQ | nlist=1024, pq_m=8, nprobe=64 | nlist=4096, pq_m=8, nprobe=128 |
| LSH | n_hash=8, table=1024 | n_hash=16, table=4096 |
| DiskANN | M=32, L=128, subq=8 | M=64, L=256, subq=12 |

### 5.5 输出

每次测试生成 CSV 文件写入 `benchmark/results/`：

```csv
algorithm,dataset,n_vectors,dims,k,build_time_s,qps,recall_at_k,memory_mb
hnsw,fashion-mnist-784-euclidean,60000,784,10,1.23,15234.5,0.973,45.2
```

## 6. Phase 4：全链路性能优化

### 6.1 目标

测量驱动优化，三步循环：测量 → 分析 → 优化，以 faiss 基线为参照。

### 6.2 性能剖析

新增 `benchmark/scripts/profile_bottleneck.py`，提供计时上下文管理器，拆分数据加载、索引初始化、索引构建、查询各阶段。

```python
with timer("数据加载"):
    vectors = dataset.train

with timer("索引初始化"):
    index = HNSWIndex(dim=100, M=16)

with timer("索引构建"):
    index.build(vectors)

with timer("批量查询"):
    ids, dists = index.search_batch(queries, k=10)
```

### 6.3 faiss 基线参照

新增 `benchmark/scripts/faiss_baseline.py`，在同一数据集上运行 faiss（IndexHNSWFlat / IndexIVFPQ / IndexLSH），输出性能数据作为优化目标：

```bash
python -m benchmark.scripts.faiss_baseline --dataset glove-100-angular
```

输出：
```json
{
    "build_time_s": 15.23,
    "qps": 12345.6,
    "recall_at_10": 0.982,
    "memory_mb": 502.3
}
```

### 6.4 优化目标

| 索引 | 对比基线 | 目标 |
|------|----------|------|
| HNSW | faiss IndexHNSWFlat | QPS 达到 faiss 80%+ |
| IVF-PQ | faiss IndexIVFPQ | Recall ≥ faiss 85% |
| LSH | faiss IndexLSH | 功能正确即可（非重点） |
| DiskANN | faiss IndexHNSWFlat + PQ | 内存优于 HNSW 30%+ |

### 6.5 优化方向

| 瓶颈类型 | 可能原因 | 优化方案 |
|----------|----------|----------|
| 数据加载慢 | HDF5 解压 | 预转为 npy 格式 + mmap |
| 构建慢 | C 层算法未优化 | 检查 HNSW 邻居插入策略、并行化 |
| 查询 QPS 低 | Python→C 开销 | 批量查询合并、内存池复用 |
| 内存峰值高 | 向量拷贝过多 | 零拷贝传递、预分配缓冲区 |

### 6.6 回归验证

每次优化后运行：

```bash
python -m pytest engineering/tools/benchmark/test/ -v
```

确保通过率不下降。

## 7. Phase 5：可视化仪表盘

### 7.1 目标

生成单文件静态 HTML 报告，包含全部图表，浏览器可直接打开。

### 7.2 技术选型

| 组件 | 技术 | 理由 |
|------|------|------|
| 图表 | Plotly.js (CDN) | 交互式、免费、嵌入单文件 |
| 表格 | DataTables.js (CDN) | 排序/筛选/分页 |
| 样式 | Bootstrap 5 (CDN) | 响应式布局 |
| 数据 | JSON 内联 | 单文件，无后端 |

### 7.3 报告结构

```
benchmark_report.html
├── 标题 + 生成时间
├── 导航链接
├── 概览卡片（最高 QPS / Recall / 性价比）
├── 性能对比表格（DataTables）
├── Recall vs QPS 权衡曲线（Plotly 散点图）
├── 内存占用柱状图（Plotly）
├── 构建时间对比（Plotly 水平柱状图）
├── 量化对比曲线（可选，PQ/LVQ/SQ bit-depth）
└── JSON 数据下载链接
```

### 7.4 报告生成器

扩展 `core/report_generator.py` 中的 `HTMLReportGenerator` 类：

```python
class HTMLReportGenerator:
    def generate(self, results: List[BenchmarkResult], output_path: str):
        """生成静态 HTML 报告"""
        # 1. 提取表格数据和图表数据
        # 2. 渲染 Jinja2 模板
        # 3. 写入 HTML 文件
```

## 8. 文件变更清单

### 新增文件

| 文件 | 所属 Phase | 说明 |
|------|-----------|------|
| `engineering/src/kbase/embed_c_api/embed_c_api.h` | 2 | C Embedding API 头文件 |
| `engineering/src/kbase/embed_c_api/embed_c_api.c` | 2 | C Embedding API 实现 |
| `engineering/src/kbase/embed_c_api/CMakeLists.txt` | 2 | Embedding 库构建 |
| `engineering/tools/benchmark/c_api/embed_ctypes_bindings.py` | 2 | Python ctypes 绑定 |
| `engineering/tools/benchmark/scripts/profile_bottleneck.py` | 4 | 性能剖析脚本 |
| `engineering/tools/benchmark/scripts/faiss_baseline.py` | 4 | faiss 基线参照 |
| `engineering/tools/benchmark/templates/report_template.html` | 5 | HTML 报告模板 |

### 修改文件

| 文件 | 所属 Phase | 变更内容 |
|------|-----------|----------|
| `engineering/tools/benchmark/c_api/CMakeLists.txt` | 1 | 重写，链接工程已有库 |
| `engineering/tools/benchmark/c_api/ctypes_bindings.py` | 1 | 优化库路径查找 |
| `engineering/tools/benchmark/core/dataset_manager.py` | 3 | 增加 `sample()` 方法 |
| `engineering/tools/benchmark/core/benchmark_runner.py` | 3 | 支持采样参数 |
| `engineering/tools/benchmark/core/report_generator.py` | 5 | 增加 HTML 报告生成器 |
| `engineering/tools/benchmark/inference/real_embedding_model.py` | 2 | 增加 C API 回退路径 |
| `engineering/tools/benchmark/CMakeLists.txt` | 1 | 增加 c_api 子目录构建 |

## 9. 测试计划

### 9.1 C 库验证

- ctypes 加载测试：验证 DLL 加载成功
- HNSW 功能测试：create → build(10) → search → 检查结果是否正确
- DiskANN 功能测试：同上
- LSH 功能测试：同上

### 9.2 Embedding C API 验证

- 模型加载测试：验证 GGUF 文件加载
- 编码测试：输入 3 条文本，输出形状为 (3, 384)
- 与 sentence-transformers 输出对比：余弦相似度 ≥ 0.95

### 9.3 大规模测试

- 子集测试：fashion-mnist 60K 全量，四种索引全部通过
- GloVe 采样 100K：功能验证
- GloVe 全量 1.18M：性能基线采集

### 9.4 优化验证

- faiss 基线运行
- 优化后 QPS ≥ 基线 80%
- 优化后 recall 不显著下降

### 9.5 可视化验证

- 生成 HTML 报告
- 浏览器打开验证：所有图表可渲染、可交互

## 10. 风险与 mitigation

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| 内存不足跑全量 Deep-Image | 无法获得 9.9M 基线 | 使用 1M 采样，渐进式扩大 |
| `embed_c_api` 链接 `kbase` 库困难 | Embedding C API 延迟 | 与主项目构建解耦，独立 CMakeLists |
| GGUF 模型加载失败 | Embedding C API 不可用 | 保留 sentence-transformers 回退路径 |
| faiss 基线性能远高于自研实现 | 优化压力大 | 目标设为 80%，逐步提升 |

## 11. 交付物

| Phase | 交付物 | 验证方式 |
|-------|--------|----------|
| 1 | `libvector_index_c_api.dll` + ctypes 加载通过 | `python -c "from benchmark.c_api.ctypes_bindings import load_library; assert load_library()"` |
| 2 | `EmbeddingCAPI` 类可编码文本 | 3 条文本编码为 (3, 384) 向量 |
| 3 | GloVe 1.18M + Deep-Image 1M 测试报告 (CSV) | 文件存在于 `benchmark/results/` |
| 4 | faiss 基线数据 + 性能对比报告 | JSON 文件显示自研/faiss 对比 |
| 5 | `benchmark_report.html` | 浏览器打开可交互 |