# VectorBench - 向量索引基准测试框架

> 基于 ann-benchmarks 的向量索引性能基准测试框架，支持项目自研索引（HNSW/IVF-PQ/LSH/DiskANN）的性能评估。

## 特性

- **多种数据集**: 支持 ANN Benchmarks 标准数据集（GloVe/Fashion-MNIST/DeepImage 等）
- **多种算法**: 支持项目自研索引实现
- **全面评估**: QPS/召回率/内存/构建时间多维度评估
- **报告生成**: CSV/Markdown 格式测试报告
- **Python ctypes**: 零额外依赖的 C→Python 绑定
- **量化对比**: PQ/LVQ/SQ 在不同 bit 位下的精度-速度曲线
- **端侧推理**: Embedding 推理延迟对搜索 QPS 的影响分析

## 安装

### 依赖

```bash
# 核心依赖（必需）
pip install numpy h5py pyyaml

# 完整安装（支持真实 Embedding 模型）
pip install -r requirements-benchmark.txt
```

### 构建 C 库（可选）

构建项目后，C 库会自动被 ctypes 加载。如未构建，将使用 Python 占位实现。

## 使用

### 下载数据集

```bash
# 下载所有数据集
python -m benchmark.scripts.download_datasets --all

# 下载指定数据集
python -m benchmark.scripts.download_datasets --dataset glove-100-angular

# 列出可用数据集
python -m benchmark.scripts.download_datasets --list

# 强制重新下载
python -m benchmark.scripts.download_datasets --dataset fashion-mnist --force
```

### 运行基准测试

```bash
# 运行所有算法（默认数据集）
python -m benchmark.main

# 指定数据集
python -m benchmark.main --dataset fashion-mnist-784-euclidean

# 指定算法
python -m benchmark.main --algorithm faiss_flat

# 指定参数
python -m benchmark.main --algorithm faiss_hnsw --params M=16,ef_construction=200

# 生成报告
python -m benchmark.main --report --output results/benchmark.csv

# 列出可用数据集
python -m benchmark.main --list
```

### Python API

```python
from benchmark.core import DatasetManager, BenchmarkRunner

# 加载数据集
manager = DatasetManager()
dataset = manager.load("glove-100-angular")

# 运行测试
runner = BenchmarkRunner()
result = runner.run(
    algorithm="faiss_hnsw",
    dataset=dataset,
    k=10,
    params={"M": 16}
)

print(f"QPS: {result.qps:.2f}")
print(f"Recall@10: {result.recall_at_10:.4f}")
```

### 量化对比

```python
from benchmark.quantization import QuantizationBenchmark, QuantizationConfig

# 运行量化对比
benchmark = QuantizationBenchmark()
results = benchmark.run_comparison("fashion-mnist-784-euclidean")

# 生成报告
report = benchmark.generate_report(results)
print(report)
```

### 端侧推理分析

```python
from benchmark.inference import InferenceLatencyAnalyzer

# 分析推理延迟影响
analyzer = InferenceLatencyAnalyzer()
results = analyzer.run_model_comparison(index_search_qps=10000)

# 生成报告
report = analyzer.generate_report(results)
print(report)
```

## 项目结构

```
benchmark/
├── __init__.py
├── main.py                     # 主入口
├── config/
│   ├── datasets.yaml           # 数据集配置
│   ├── algorithms.yaml         # 算法配置
│   └── metrics.yaml            # 指标配置
├── adapters/                   # 索引适配器
│   ├── __init__.py
│   ├── base.py                 # 基类
│   ├── hnsw_adapter.py        # HNSW 适配器 (C/Python 双实现)
│   ├── ivf_pq_adapter.py      # IVF-PQ 适配器
│   ├── lsh_adapter.py         # LSH 适配器
│   ├── diskann_adapter.py     # DiskANN 适配器
│   └── faiss_adapter.py       # faiss 基线
├── core/                       # 核心模块
│   ├── __init__.py
│   ├── dataset_manager.py      # 数据集管理
│   ├── benchmark_runner.py     # 测试执行器
│   └── report_generator.py     # 报告生成器
├── quantization/               # 量化对比模块
│   ├── __init__.py
│   └── quantization_benchmark.py  # PQ/LVQ/SQ 对比
├── inference/                  # 端侧推理模块
│   ├── __init__.py
│   └── inference_latency.py    # 推理延迟分析
├── c_api/                      # C API 绑定层
│   ├── __init__.py
│   ├── ctypes_bindings.py      # ctypes 绑定
│   └── vector_index_c_api.c    # C 导出层
├── data/                       # 数据集缓存
├── results/                    # 测试结果
└── scripts/
    └── download_datasets.py    # 下载脚本
```

## 支持的数据集

| 数据集 | 维度 | 规模 | 距离度量 |
|--------|------|------|----------|
| fashion-mnist-784-euclidean | 784 | 60K | euclidean |
| glove-100-angular | 100 | 1.18M | angular |
| deep-image-96-euclidean | 96 | 9.9M | euclidean |

## 支持的算法

### 项目实现

- `project_hnsw`: HNSW 图索引 (faiss_hnsw_t)
- `project_ivf_pq`: IVF-PQ 量化索引 (DiskANN 实现)
- `project_lsh`: LSH 索引 (随机投影哈希)
- `project_diskann`: DiskANN 索引 (Vamana 图 + PQ)

## 评估指标

| 指标 | 说明 |
|------|------|
| Recall@K | Top-K 召回率 |
| QPS | 查询吞吐量 |
| Build Time | 索引构建时间 |
| Memory | 内存占用 |
| Compression | 量化压缩比 |

## 演进路线

- **Phase 1 (完成)**: VectorBench 框架 + 项目索引适配器
- **Phase 2 (完成)**: C API 绑定 + ctypes 调用层
- **Phase 3 (完成)**: 量化对比 (PQ/LVQ/SQ bit-depth curve)
- **Phase 4 (完成)**: 端侧推理集成 (embedding latency)
- **Phase 5 (完成)**: 真实模型推理 + 端到端基准测试

### Phase 5: 真实 Embedding 模型集成

支持 sentence-transformers 真实 Embedding 推理：

```bash
# 最小安装（仅 Mock）
pip install numpy h5py pyyaml

# 完整安装（支持真实模型）
pip install -r requirements-benchmark.txt

# 使用真实模型运行基准测试
python -m benchmark.main --dataset glove-100-angular --embedding-model all-MiniLM-L6-v2

# 强制使用 Mock
python -m benchmark.main --dataset glove-100-angular --mock-inference
```

**支持的模型**：
- `all-MiniLM-L6-v2`: 384 维，快速
- `bge-small-en`: 384 维，高质量
- `e5-small`: 384 维，均衡

**端到端测试**：

```python
from benchmark.inference import EndToEndBenchmark

bench = EndToEndBenchmark()
result = bench.run(
    corpus_texts=["文档1", "文档2", ...],
    query_texts=["查询1", ...],
    embedding_model="auto"
)
print(f"有效 QPS: {result.effective_qps:.0f}")
print(f"推理占比: {result.inference_overhead_percent:.1f}%")
```

## 许可证

本项目继承主项目的 MIT 许可证。
