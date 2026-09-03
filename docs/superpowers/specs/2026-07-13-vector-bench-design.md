# VectorBench 基准测试体系设计文档

> 向量索引性能基准测试框架，基于 ann-benchmarks 集成

## 1. 整体架构

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         VectorBench 基准测试框架                            │
├─────────────────────────────────────────────────────────────────────────────┤
│  数据层                                                                     │
│  ┌──────────────────┐   ┌──────────────────────────────────────────────┐   │
│  │ ANN Benchmarks   │   │              本项目索引适配层                  │   │
│  │ 标准数据集        │   │  ┌──────┐ ┌──────┐ ┌───────┐ ┌──────┐     │   │
│  │                  │   │  │ HNSW │ │ IVF  │ │DiskANN│ │ LSH  │     │   │
│  │ glove-100-angular│──▶│  │      │ │      │ │       │ │      │     │   │
│  │ deep-image-96-eu │   │  └──────┘ └──────┘ └───────┘ └──────┘     │   │
│  │ fashion-mnist-784│   │  ┌───────┐ ┌─────────┐ ┌────────────┐     │   │
│  │ ...              │   │  │IVF-PQ│ │IVF-HNSW│ │BM25/文本  │     │   │
│  └──────────────────┘   │  └───────┘ └─────────┘ └────────────┘     │   │
│                         │                                              │   │
│                         │  + faiss 参考实现 (外部基线)                  │   │
│                         └──────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────────────────────────┤
│  评估层                                                                     │
│  QPS · 召回率@10/100 · 内存占用 · 构建时间 · 精度-速度权衡曲线             │
├─────────────────────────────────────────────────────────────────────────────┤
│  报告层                                                                     │
│  CSV 导出 · Markdown 报告 · 对比图表                                       │
└─────────────────────────────────────────────────────────────────────────────┘
```

## 2. 核心组件

| 组件 | 职责 | 位置 |
|------|------|------|
| `ann_benchmark_runner` | 测试执行器，封装 ann-benchmarks 接口 | `engineering/tools/benchmark/` |
| `dataset_manager` | 数据集下载、缓存、预处理 | `engineering/tools/benchmark/` |
| `index_adapter` | 项目索引的 C→Python 绑定层 | `engineering/tools/benchmark/` |
| `report_generator` | 结果分析、对比报告生成 | `engineering/tools/benchmark/` |

## 3. 目录结构

```
engineering/tools/benchmark/
├── CMakeLists.txt
├── README.md
├── __init__.py
├── main.py                     # 主入口
├── config/
│   ├── datasets.yaml           # 数据集配置
│   ├── algorithms.yaml         # 算法配置
│   └── metrics.yaml            # 评估指标配置
├── data/
│   └── .gitkeep                # 数据集缓存目录
├── adapters/
│   ├── __init__.py
│   ├── base.py                 # 基类
│   ├── hnsw_adapter.py         # HNSW 适配器
│   ├── ivf_adapter.py          # IVF 适配器
│   ├── diskann_adapter.py      # DiskANN 适配器
│   ├── lsh_adapter.py          # LSH 适配器
│   ├── ivf_pq_adapter.py       # IVF-PQ 适配器
│   └── faiss_adapter.py        # faiss 基线适配器
├── core/
│   ├── __init__.py
│   ├── dataset_manager.py      # 数据集管理
│   ├── benchmark_runner.py     # 测试执行器
│   └── report_generator.py     # 报告生成器
├── results/
│   └── .gitkeep                # 测试结果目录
└── scripts/
    ├── download_datasets.py    # 数据集下载脚本
    └── export_results.py       # 结果导出脚本
```

## 4. 数据集配置

### 4.1 支持的数据集

```yaml
# config/datasets.yaml
datasets:
  # 小规模数据集（快速验证）
  fashion-mnist-784-euclidean:
    dimension: 784
    distance: euclidean
    size: 60000
    test_size: 10000
    url: "http://ann-benchmarks.com/fashion-mnist-784-euclidean.hdf5"

  # 中等规模
  glove-100-angular:
    dimension: 100
    distance: angular
    size: 1183514
    test_size: 10000
    url: "http://ann-benchmarks.com/glove-100-angular.hdf5"

  deep-image-96-euclidean:
    dimension: 96
    distance: euclidean
    size: 9900000
    test_size: 10000
    url: "http://ann-benchmarks.com/deep-image-96-euclidean.hdf5"

  # 大规模数据集（全面评估）
  text2image-200-angular:
    dimension: 200
    distance: angular
    size: 10000000
    test_size: 10000
    url: "http://ann-benchmarks.com/text2image-200-angular.hdf5"
```

### 4.2 数据集下载流程

```
1. 检查本地缓存 (data/ 目录)
2. 如不存在，从 URL 下载 .hdf5 文件
3. 解析 HDF5，提取:
   - train: 训练数据 (用于构建索引)
   - test: 测试查询 (用于评估)
   - neighbors: 真实近邻 (用于计算召回率)
4. 缓存解析后的数据为 .npy 格式
```

## 5. 索引适配器设计

### 5.1 基类接口

```python
class BaseIndexAdapter:
    """索引适配器基类"""

    def __init__(self, dimension: int, metric: str, **kwargs):
        self.dimension = dimension
        self.metric = metric
        self.index = None

    def fit(self, vectors: np.ndarray) -> None:
        """构建索引"""
        raise NotImplementedError

    def add(self, vectors: np.ndarray) -> None:
        """增量添加向量"""
        raise NotImplementedError

    def search(self, queries: np.ndarray, k: int) -> np.ndarray:
        """搜索 Top-K"""
        raise NotImplementedError

    def get_size(self) -> int:
        """获取索引大小(字节)"""
        raise NotImplementedError

    def get_params(self) -> dict:
        """获取索引参数"""
        raise NotImplementedError
```

### 5.2 HNSW 适配器

```python
class HNSWAdapter(BaseIndexAdapter):
    """HNSW 索引适配器 - 绑定项目 C 实现"""

    def __init__(self, dimension: int, metric: str = "euclidean",
                 M: int = 16, ef_construction: int = 200, ef_search: int = 100):
        super().__init__(dimension, metric)
        self.M = M
        self.ef_construction = ef_construction
        self.ef_search = ef_search

    def fit(self, vectors: np.ndarray) -> None:
        # 通过 ctypes 调用 C 库
        self.index = vector_hnsw_create(
            dim=self.dimension,
            M=self.M,
            ef_construction=self.ef_construction,
            metric=self.metric
        )
        vector_hnsw_build(self.index, vectors.ctypes.data, len(vectors))

    def search(self, queries: np.ndarray, k: int) -> np.ndarray:
        results = np.zeros((len(queries), k), dtype=np.int32)
        vector_hnsw_search(
            self.index,
            queries.ctypes.data,
            len(queries),
            k,
            self.ef_search,
            results.ctypes.data
        )
        return results
```

### 5.3 IVF-PQ 适配器

```python
class IVFPQAdapter(BaseIndexAdapter):
    """IVF-PQ 索引适配器"""

    def __init__(self, dimension: int, metric: str = "euclidean",
                 nlist: int = 256, nprobe: int = 32,
                 M: int = 8, nbits: int = 8):
        super().__init__(dimension, metric)
        self.nlist = nlist
        self.nprobe = nprobe
        self.M = M
        self.nbits = nbits

    def fit(self, vectors: np.ndarray) -> None:
        self.index = vector_ivf_pq_create(
            dim=self.dimension,
            nlist=self.nlist,
            M=self.M,
            nbits=self.nbits,
            metric=self.metric
        )
        vector_ivf_pq_train(self.index, vectors.ctypes.data, len(vectors))
        vector_ivf_pq_add(self.index, vectors.ctypes.data, len(vectors))

    def search(self, queries: np.ndarray, k: int) -> np.ndarray:
        results = np.zeros((len(queries), k), dtype=np.int32)
        vector_ivf_pq_search(
            self.index,
            queries.ctypes.data,
            len(queries),
            k,
            self.nprobe,
            results.ctypes.data
        )
        return results
```

### 5.4 faiss 基线适配器

```python
class FaissAdapter(BaseIndexAdapter):
    """faiss 参考实现适配器 - 外部基线"""

    def __init__(self, dimension: int, metric: str = "euclidean",
                 index_type: str = "hnsw", **kwargs):
        super().__init__(dimension, metric)
        self.index_type = index_type
        self._build_index_factory()

    def _build_index_factory(self) -> None:
        if self.metric == "angular":
            self.index = faiss.IndexFlatIP(self.dimension)
        else:
            self.index = faiss.IndexFlatL2(self.dimension)

    def fit(self, vectors: np.ndarray) -> None:
        faiss.normalize_L2(vectors) if self.metric == "angular" else None
        self.index.add(vectors)

    def search(self, queries: np.ndarray, k: int) -> np.ndarray:
        faiss.normalize_L2(queries) if self.metric == "angular" else None
        distances, indices = self.index.search(queries, k)
        return indices
```

## 6. 算法配置

```yaml
# config/algorithms.yaml
algorithms:
  # 内部实现
  project_hnsw:
    adapter: HNSWAdapter
    parameters:
      M: [8, 16, 24, 32]
      ef_construction: [100, 200, 400]
      ef_search: [50, 100, 200]

  project_ivf_pq:
    adapter: IVFPQAdapter
    parameters:
      nlist: [128, 256, 512]
      nprobe: [16, 32, 64]
      M: [8, 16]
      nbits: [8]

  project_diskann:
    adapter: DiskANNAdapter
    parameters:
      L: [50, 75, 100]
      R: [32, 64, 100]
      alpha: [1.2, 1.5, 2.0]

  project_lsh:
    adapter: LSHAdapter
    parameters:
      num_hash: [8, 16, 32]
      table_size: [1000, 5000, 10000]

  # 外部基线
  faiss_hnsw:
    adapter: FaissAdapter
    parameters:
      index_type: hnsw
      M: [16, 32]
      efConstruction: [40, 200]

  faiss_ivf:
    adapter: FaissAdapter
    parameters:
      index_type: ivf
      nlist: [256, 1024]
```

## 7. 测试执行流程

### 7.1 主流程

```python
def run_benchmark(config: BenchmarkConfig) -> BenchmarkResult:
    """基准测试主流程"""

    # 1. 加载数据集
    dataset = DatasetManager.load(config.dataset_name)
    train_data = dataset.train
    test_data = dataset.test
    true_neighbors = dataset.neighbors

    # 2. 初始化适配器
    adapter = create_adapter(config.algorithm, config.params)

    # 3. 构建索引 (计时)
    build_start = time.perf_counter()
    adapter.fit(train_data)
    build_time = time.perf_counter() - build_start
    memory_usage = adapter.get_size()

    # 4. 执行查询 (批量 QPS)
    queries_per_batch = 100
    batches = len(test_data) // queries_per_batch

    latencies = []
    for i in range(batches):
        batch = test_data[i * queries_per_batch:(i + 1) * queries_per_batch]
        start = time.perf_counter()
        results = adapter.search(batch, k=10)
        latencies.append(time.perf_counter() - start)

    # 5. 计算召回率
    recall = compute_recall(results, true_neighbors[:len(test_data)], k=10)

    # 6. 汇总结果
    return BenchmarkResult(
        algorithm=config.algorithm,
        dataset=config.dataset_name,
        qps=len(test_data) / sum(latencies),
        recall=recall,
        build_time=build_time,
        memory_bytes=memory_usage,
        params=config.params
    )
```

### 7.2 召回率计算

```python
def compute_recall(predicted: np.ndarray, ground_truth: np.ndarray, k: int) -> float:
    """计算 Top-K 召回率"""
    correct = 0
    total = 0
    for pred_row, gt_row in zip(predicted, ground_truth):
        pred_set = set(pred_row[:k])
        gt_set = set(gt_row[:k])
        correct += len(pred_set & gt_set)
        total += k
    return correct / total
```

## 8. 报告生成

### 8.1 CSV 格式

```csv
# results/benchmark_2026-07-13.csv
algorithm,dataset,dimension,recall@10,recall@100,qps,build_time_s,memory_mb,params
project_hnsw,glove-100-angular,100,0.952,0.987,12500,45.2,256,M=16,ef=200
project_ivf_pq,glove-100-angular,100,0.948,0.982,18000,12.3,128,M=8,nbits=8
faiss_hnsw,glove-100-angular,100,0.954,0.988,13200,48.1,268,M=16,ef=200
```

### 8.2 Markdown 报告

```markdown
# VectorBench 测试报告

生成时间: 2026-07-13

## 数据集概览

| 数据集 | 维度 | 训练集 | 测试集 | 距离度量 |
|--------|------|--------|--------|----------|
| glove-100-angular | 100 | 1.18M | 10K | angular |
| fashion-mnist-784-euclidean | 784 | 60K | 10K | euclidean |

## 召回率对比

| 算法 | glove-100 | fashion-mnist | 平均召回率 |
|------|-----------|---------------|-----------|
| project_hnsw | 0.952 | 0.978 | 0.965 |
| project_ivf_pq | 0.948 | 0.972 | 0.960 |
| faiss_hnsw | 0.954 | 0.980 | 0.967 |

## QPS 对比

[图表: QPS vs 召回率散点图]

## 内存占用对比

[图表: 内存 vs 召回率条形图]
```

## 9. 评估指标

| 指标 | 说明 | 计算方式 |
|------|------|----------|
| Recall@K | Top-K 召回率 | 真实近邻出现在 Top-K 的比例 |
| QPS | 查询吞吐量 | 总查询数 / 总耗时 |
| Build Time | 索引构建时间 | 从开始构建到完成的时间 |
| Memory | 内存占用 | 索引占用的内存空间 |
| Precision-Speed | 精度-速度权衡 | Recall/QPS 曲线下面积 |

## 10. 集成方式

```
ann-benchmarks (Python)
       ↓  算法接口协议
  index_adapter (ctypes/cffi)
       ↓  C ABI
  项目索引 (C/C++)
       ↓
  vector_engine / hnsw_index / ivf_index / diskann ...
```

### 10.1 C 库导出

```c
// engineering/src/db/index/vector_index/CMakeLists.txt
# 导出 C 库供 Python 调用
add_library(vector_index_c SHARED vector_index_c_api.c)
target_link_libraries(vector_index_c vector_hnsw vector_ivf_pq ...)
```

## 11. 使用方式

### 11.1 命令行

```bash
# 运行全部基准测试
python -m benchmark.main --all

# 运行指定数据集
python -m benchmark.main --dataset glove-100-angular

# 运行指定算法
python -m benchmark.main --algorithm project_hnsw

# 指定参数
python -m benchmark.main --algorithm project_hnsw --params M=16,ef=200

# 生成报告
python -m benchmark.main --report markdown --output results/report.md
```

### 11.2 Python API

```python
from benchmark import BenchmarkRunner, DatasetManager

# 加载数据集
dataset = DatasetManager.load("glove-100-angular")

# 运行测试
runner = BenchmarkRunner()
result = runner.run(
    algorithm="project_hnsw",
    dataset=dataset,
    params={"M": 16, "ef_construction": 200}
)

print(f"QPS: {result.qps}, Recall@10: {result.recall}")
```

## 12. 预期产出

- ✅ 标准化的索引性能对比报告
- ✅ 召回率/QPS/内存对比数据
- ✅ 与 faiss 基线的对比验证
- ✅ 为后续 NSG/量化研究提供量化依据

## 13. 后续演进

| 阶段 | 内容 |
|------|------|
| Phase 2 | 接入 NSG 索引，实现 NSGAdapter |
| Phase 3 | 量化对比：PQ/LVQ/SQ 在不同 bit 位下的精度-速度曲线 |
| Phase 4 | 端侧推理：Embedding 推理延迟对搜索 QPS 的影响 |
