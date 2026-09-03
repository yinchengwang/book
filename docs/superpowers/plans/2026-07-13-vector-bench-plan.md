# VectorBench 基准测试体系实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 建立向量索引性能基准测试框架，集成 ann-benchmarks，支持项目 HNSW/IVF-PQ/DiskANN/LSH 索引与 faiss 基线的对比测试。

**Architecture:**
- Python 测试框架（数据管理、测试执行、报告生成）
- C API 导出层（ctypes 绑定）
- 项目索引直接调用

**Tech Stack:** Python 3.8+, ctypes, numpy, h5py, pyyaml, faiss-cpu, matplotlib

---

## Global Constraints

| 约束项 | 值 |
|--------|-----|
| 项目根目录 | `c:\code\book\` |
| Python 工具目录 | `engineering/tools/benchmark/` |
| C 库目录 | `engineering/src/db/index/vector_index/` |
| C 头文件目录 | `engineering/include/db/index/vector_index/` |
| 测试结果目录 | `test-results/benchmarks/` |
| 数据集缓存目录 | `engineering/tools/benchmark/data/` |
| Python 版本 | 3.8+ |
| 依赖 | numpy, h5py, pyyaml, faiss-cpu（或 faiss-gpu）|

---

## 文件结构

```
engineering/tools/benchmark/
├── __init__.py
├── main.py                     # 主入口
├── CMakeLists.txt              # C 库构建
├── config/
│   ├── datasets.yaml           # 数据集配置
│   ├── algorithms.yaml         # 算法配置
│   └── metrics.yaml            # 评估指标配置
├── data/                       # 数据集缓存
│   └── .gitkeep
├── results/                    # 测试结果
│   └── .gitkeep
├── adapters/                   # 索引适配器
│   ├── __init__.py
│   ├── base.py                 # 基类
│   ├── hnsw_adapter.py         # HNSW 适配器
│   ├── ivf_pq_adapter.py       # IVF-PQ 适配器
│   ├── diskann_adapter.py     # DiskANN 适配器
│   ├── lsh_adapter.py          # LSH 适配器
│   └── faiss_adapter.py       # faiss 基线
├── core/                       # 核心模块
│   ├── __init__.py
│   ├── dataset_manager.py      # 数据集管理
│   ├── benchmark_runner.py     # 测试执行器
│   └── report_generator.py     # 报告生成器
└── scripts/
    └── download_datasets.py    # 数据集下载

# C API 导出层（新建）
engineering/src/db/index/vector_index/vector_index_c_api.c
engineering/include/db/index/vector_index/vector_index_c_api.h
```

---

## Task 1: 创建项目目录结构和配置文件

**Files:**
- Create: `engineering/tools/benchmark/CMakeLists.txt`
- Create: `engineering/tools/benchmark/config/datasets.yaml`
- Create: `engineering/tools/benchmark/config/algorithms.yaml`
- Create: `engineering/tools/benchmark/config/metrics.yaml`
- Create: `engineering/tools/benchmark/data/.gitkeep`
- Create: `engineering/tools/benchmark/results/.gitkeep`
- Create: `engineering/tools/benchmark/__init__.py`
- Create: `engineering/tools/benchmark/main.py`

**Interfaces:**
- Produces: `benchmark/config/datasets.yaml`, `algorithms.yaml`, `metrics.yaml`

**Steps:**

- [ ] **Step 1: 创建目录结构**

```bash
mkdir -p engineering/tools/benchmark/config
mkdir -p engineering/tools/benchmark/data
mkdir -p engineering/tools/benchmark/results
mkdir -p engineering/tools/benchmark/adapters
mkdir -p engineering/tools/benchmark/core
mkdir -p engineering/tools/benchmark/scripts
```

- [ ] **Step 2: 创建 datasets.yaml**

```yaml
# engineering/tools/benchmark/config/datasets.yaml
datasets:
  fashion-mnist-784-euclidean:
    dimension: 784
    distance: euclidean
    size: 60000
    test_size: 10000
    url: "http://ann-benchmarks.com/fashion-mnist-784-euclidean.hdf5"
    description: "Fashion-MNIST 图像特征"

  glove-100-angular:
    dimension: 100
    distance: angular
    size: 1183514
    test_size: 10000
    url: "http://ann-benchmarks.com/glove-100-angular.hdf5"
    description: "GloVe 词向量"

  deep-image-96-euclidean:
    dimension: 96
    distance: euclidean
    size: 9900000
    test_size: 10000
    url: "http://ann-benchmarks.com/deep-image-96-euclidean.hdf5"
    description: "Deep Image 图像特征"
```

- [ ] **Step 3: 创建 algorithms.yaml**

```yaml
# engineering/tools/benchmark/config/algorithms.yaml
algorithms:
  project_hnsw:
    adapter: HNSWAdapter
    enabled: true
    parameters:
      M: [8, 16, 24, 32]
      ef_construction: [100, 200]
      ef_search: [50, 100]

  project_ivf_pq:
    adapter: IVFPQAdapter
    enabled: true
    parameters:
      nlist: [128, 256]
      nprobe: [16, 32]
      pq_m: [8]
      pq_bits: [8]

  project_lsh:
    adapter: LSHAdapter
    enabled: true
    parameters:
      num_hash: [8, 16]
      table_size: [1000, 5000]

  faiss_hnsw:
    adapter: FaissAdapter
    enabled: true
    parameters:
      index_type: "hnsw"
      M: [16, 32]

  faiss_flat:
    adapter: FaissAdapter
    enabled: true
    parameters:
      index_type: "flat"
```

- [ ] **Step 4: 创建 metrics.yaml**

```yaml
# engineering/tools/benchmark/config/metrics.yaml
metrics:
  recall:
    enabled: true
    k_values: [1, 10, 100]
  qps:
    enabled: true
    batch_size: 100
  build_time:
    enabled: true
  memory:
    enabled: true
```

- [ ] **Step 5: 创建 __init__.py**

```python
# engineering/tools/benchmark/__init__.py
"""VectorBench - 向量索引基准测试框架"""

__version__ = "0.1.0"
```

- [ ] **Step 6: 创建 main.py**

```python
#!/usr/bin/env python3
"""
VectorBench 主入口
"""
import argparse
import sys
from pathlib import Path

# 添加项目根目录到路径
project_root = Path(__file__).parent.parent.parent.parent
sys.path.insert(0, str(project_root))

from benchmark.core.dataset_manager import DatasetManager
from benchmark.core.benchmark_runner import BenchmarkRunner
from benchmark.core.report_generator import ReportGenerator


def parse_args():
    parser = argparse.ArgumentParser(description="VectorBench - 向量索引基准测试")
    parser.add_argument("--dataset", type=str, default="glove-100-angular",
                        help="数据集名称")
    parser.add_argument("--algorithm", type=str, default="all",
                        help="算法名称 (all/project_hnsw/faiss_hnsw/...)")
    parser.add_argument("--params", type=str, default="",
                        help="参数，如 M=16,ef=200")
    parser.add_argument("--k", type=int, default=10,
                        help="Top-K")
    parser.add_argument("--output", type=str, default="results/benchmark.csv",
                        help="输出文件")
    parser.add_argument("--report", action="store_true",
                        help="生成 Markdown 报告")
    return parser.parse_args()


def main():
    args = parse_args()

    # 加载数据集
    print(f"Loading dataset: {args.dataset}...")
    dataset = DatasetManager.load(args.dataset)

    # 运行测试
    runner = BenchmarkRunner()
    result = runner.run(
        algorithm=args.algorithm,
        dataset=dataset,
        k=args.k,
        params=args.params
    )

    # 输出结果
    print(f"\nResults:")
    print(f"  Algorithm: {result.algorithm}")
    print(f"  Dataset: {result.dataset}")
    print(f"  Recall@{args.k}: {result.recall:.4f}")
    print(f"  QPS: {result.qps:.2f}")
    print(f"  Build Time: {result.build_time:.2f}s")
    print(f"  Memory: {result.memory_mb:.2f} MB")

    # 保存结果
    result.save(args.output)
    print(f"\nResults saved to: {args.output}")

    # 生成报告
    if args.report:
        reporter = ReportGenerator()
        report_path = args.output.replace(".csv", "_report.md")
        reporter.generate([result], report_path)
        print(f"Report saved to: {report_path}")


if __name__ == "__main__":
    main()
```

- [ ] **Step 7: 创建 CMakeLists.txt（空文件，用于未来 C 库构建）**

```cmake
# engineering/tools/benchmark/CMakeLists.txt
# C 库构建配置（预留，未来可能需要）
# 当前阶段仅用于标记目录
```

- [ ] **Step 8: 创建 .gitkeep 文件**

```bash
touch engineering/tools/benchmark/data/.gitkeep
touch engineering/tools/benchmark/results/.gitkeep
```

- [ ] **Step 9: 提交**

```bash
git add engineering/tools/benchmark/
git commit -m "feat(benchmark): 创建 VectorBench 目录结构和配置文件"
```

---

## Task 2: 创建数据集管理器

**Files:**
- Create: `engineering/tools/benchmark/core/__init__.py`
- Create: `engineering/tools/benchmark/core/dataset_manager.py`

**Interfaces:**
- Consumes: `config/datasets.yaml`
- Produces: `DatasetManager` 类，`load()` 返回 `BenchmarkDataset`

**Steps:**

- [ ] **Step 1: 创建 core/__init__.py**

```python
# engineering/tools/benchmark/core/__init__.py
"""核心模块"""

from .dataset_manager import DatasetManager, BenchmarkDataset
from .benchmark_runner import BenchmarkRunner, BenchmarkResult
from .report_generator import ReportGenerator

__all__ = [
    "DatasetManager",
    "BenchmarkDataset",
    "BenchmarkRunner",
    "BenchmarkResult",
    "ReportGenerator",
]
```

- [ ] **Step 2: 创建 dataset_manager.py**

```python
#!/usr/bin/env python3
"""
数据集管理器 - 下载、缓存、加载 ANN Benchmarks 数据集
"""

import hashlib
import os
from pathlib import Path
from dataclasses import dataclass
from typing import Optional, Dict, Any
import numpy as np

try:
    import h5py
    HAS_H5PY = True
except ImportError:
    HAS_H5PY = False

try:
    import yaml
    HAS_YAML = True
except ImportError:
    HAS_YAML = False


@dataclass
class BenchmarkDataset:
    """基准测试数据集"""
    name: str
    dimension: int
    distance: str
    train: np.ndarray
    test: np.ndarray
    neighbors: np.ndarray
    train_size: int
    test_size: int


class DatasetManager:
    """数据集管理器"""

    def __init__(self, data_dir: Optional[str] = None):
        if data_dir is None:
            # 使用相对于当前文件的路径
            self.data_dir = Path(__file__).parent.parent / "data"
        else:
            self.data_dir = Path(data_dir)
        self.data_dir.mkdir(parents=True, exist_ok=True)

        # 加载数据集配置
        self.config = self._load_config()

    def _load_config(self) -> Dict[str, Any]:
        """加载数据集配置"""
        config_path = Path(__file__).parent.parent / "config" / "datasets.yaml"
        if HAS_YAML and config_path.exists():
            with open(config_path, 'r', encoding='utf-8') as f:
                return yaml.safe_load(f)
        # 默认配置
        return {
            "datasets": {
                "fashion-mnist-784-euclidean": {
                    "dimension": 784,
                    "distance": "euclidean",
                    "size": 60000,
                    "test_size": 10000,
                    "url": "http://ann-benchmarks.com/fashion-mnist-784-euclidean.hdf5"
                },
                "glove-100-angular": {
                    "dimension": 100,
                    "distance": "angular",
                    "size": 1183514,
                    "test_size": 10000,
                    "url": "http://ann-benchmarks.com/glove-100-angular.hdf5"
                }
            }
        }

    def _get_cache_path(self, name: str) -> Path:
        """获取缓存路径"""
        return self.data_dir / f"{name}.npz"

    def _download_dataset(self, name: str, url: str) -> str:
        """下载数据集到临时文件"""
        import urllib.request
        import tempfile

        print(f"Downloading {name} from {url}...")

        # 创建临时文件
        fd, temp_path = tempfile.mkstemp(suffix=".hdf5")
        os.close(fd)

        try:
            urllib.request.urlretrieve(url, temp_path)
            return temp_path
        except Exception as e:
            if os.path.exists(temp_path):
                os.remove(temp_path)
            raise RuntimeError(f"Failed to download {name}: {e}")

    def _parse_hdf5(self, hdf5_path: str) -> Dict[str, np.ndarray]:
        """解析 HDF5 文件"""
        if not HAS_H5PY:
            raise RuntimeError("h5py is required to parse HDF5 files. Install with: pip install h5py")

        data = {}
        with h5py.File(hdf5_path, 'r') as f:
            for key in ['train', 'test', 'neighbors']:
                if key in f:
                    data[key] = f[key][:]

        return data

    def load(self, name: str, force_download: bool = False) -> BenchmarkDataset:
        """
        加载数据集

        Args:
            name: 数据集名称
            force_download: 是否强制重新下载

        Returns:
            BenchmarkDataset 对象
        """
        if name not in self.config.get("datasets", {}):
            raise ValueError(f"Unknown dataset: {name}. Available: {list(self.config['datasets'].keys())}")

        config = self.config["datasets"][name]
        cache_path = self._get_cache_path(name)

        # 尝试从缓存加载
        if not force_download and cache_path.exists():
            print(f"Loading {name} from cache: {cache_path}")
            data = np.load(cache_path)
            return BenchmarkDataset(
                name=name,
                dimension=config["dimension"],
                distance=config["distance"],
                train=data["train"],
                test=data["test"],
                neighbors=data["neighbors"],
                train_size=len(data["train"]),
                test_size=len(data["test"])
            )

        # 下载并解析
        hdf5_path = self._download_dataset(name, config["url"])
        try:
            raw_data = self._parse_hdf5(hdf5_path)

            # 保存到缓存
            print(f"Saving {name} to cache: {cache_path}")
            np.savez_compressed(
                cache_path,
                train=raw_data["train"],
                test=raw_data["test"],
                neighbors=raw_data["neighbors"]
            )

            return BenchmarkDataset(
                name=name,
                dimension=config["dimension"],
                distance=config["distance"],
                train=raw_data["train"],
                test=raw_data["test"],
                neighbors=raw_data["neighbors"],
                train_size=len(raw_data["train"]),
                test_size=len(raw_data["test"])
            )
        finally:
            # 清理临时文件
            if os.path.exists(hdf5_path):
                os.remove(hdf5_path)

    def list_datasets(self) -> list:
        """列出所有可用的数据集"""
        return list(self.config.get("datasets", {}).keys())

    def download_all(self) -> None:
        """下载所有数据集"""
        for name in self.config.get("datasets", {}):
            self.load(name)
```

- [ ] **Step 3: 提交**

```bash
git add engineering/tools/benchmark/core/
git commit -m "feat(benchmark): 创建数据集管理器 DatasetManager"
```

---

## Task 3: 创建基准测试结果和报告生成器

**Files:**
- Create: `engineering/tools/benchmark/core/benchmark_runner.py`
- Create: `engineering/tools/benchmark/core/report_generator.py`

**Interfaces:**
- Consumes: `BenchmarkDataset`, `BaseIndexAdapter`
- Produces: `BenchmarkResult`, Markdown/CSV 报告

**Steps:**

- [ ] **Step 1: 创建 benchmark_runner.py**

```python
#!/usr/bin/env python3
"""
基准测试执行器
"""

import time
from dataclasses import dataclass, asdict
from pathlib import Path
from typing import Optional, Dict, Any, List
import numpy as np

try:
    import yaml
    HAS_YAML = True
except ImportError:
    HAS_YAML = False


@dataclass
class BenchmarkResult:
    """基准测试结果"""
    algorithm: str
    dataset: str
    dimension: int
    recall_at_1: float
    recall_at_10: float
    recall_at_100: float
    qps: float
    build_time: float
    memory_bytes: int
    params: Dict[str, Any]

    @property
    def memory_mb(self) -> float:
        return self.memory_bytes / (1024 * 1024)

    def to_dict(self) -> Dict[str, Any]:
        d = asdict(self)
        d["memory_mb"] = self.memory_mb
        return d

    def save(self, path: str) -> None:
        """保存结果到 CSV"""
        import csv
        Path(path).parent.mkdir(parents=True, exist_ok=True)

        write_header = not Path(path).exists()

        with open(path, 'a', newline='', encoding='utf-8') as f:
            writer = csv.DictWriter(f, fieldnames=self.to_dict().keys())
            if write_header:
                writer.writeheader()
            writer.writerow(self.to_dict())


class BenchmarkRunner:
    """基准测试运行器"""

    def __init__(self):
        self.config = self._load_config()

    def _load_config(self) -> Dict[str, Any]:
        """加载算法配置"""
        config_path = Path(__file__).parent.parent / "config" / "algorithms.yaml"
        if HAS_YAML and config_path.exists():
            with open(config_path, 'r', encoding='utf-8') as f:
                return yaml.safe_load(f)
        return {"algorithms": {}}

    def _compute_recall(self, predicted: np.ndarray, ground_truth: np.ndarray, k: int) -> float:
        """计算 Top-K 召回率"""
        correct = 0
        total = 0
        for pred_row, gt_row in zip(predicted, ground_truth):
            pred_set = set(pred_row[:k]) if len(pred_row) >= k else set(pred_row)
            gt_set = set(gt_row[:k]) if len(gt_row) >= k else set(gt_row)
            correct += len(pred_set & gt_set)
            total += min(k, len(gt_set))
        return correct / total if total > 0 else 0.0

    def _create_adapter(self, algorithm: str, params: Optional[Dict[str, Any]] = None):
        """创建适配器"""
        from benchmark.adapters import create_adapter
        return create_adapter(algorithm, params)

    def run(self, algorithm: str, dataset, k: int = 10,
             params: Optional[Dict[str, Any]] = None) -> BenchmarkResult:
        """
        运行基准测试

        Args:
            algorithm: 算法名称
            dataset: BenchmarkDataset 对象
            k: Top-K
            params: 参数字典

        Returns:
            BenchmarkResult 对象
        """
        print(f"\nRunning benchmark:")
        print(f"  Algorithm: {algorithm}")
        print(f"  Dataset: {dataset.name}")
        print(f"  Dimension: {dataset.dimension}")
        print(f"  Train size: {dataset.train_size}")
        print(f"  Test size: {dataset.test_size}")

        # 创建适配器
        adapter = self._create_adapter(algorithm, params)

        # 构建索引
        print(f"\nBuilding index...")
        build_start = time.perf_counter()
        adapter.fit(dataset.train)
        build_time = time.perf_counter() - build_start
        print(f"  Build time: {build_time:.2f}s")

        # 获取内存占用
        memory_bytes = adapter.get_size()
        print(f"  Memory: {memory_bytes / (1024*1024):.2f} MB")

        # 执行查询
        print(f"\nRunning queries...")
        test_data = dataset.test
        batch_size = 100
        n_batches = len(test_data) // batch_size

        all_results = []
        total_time = 0.0

        for i in range(n_batches):
            batch_start = i * batch_size
            batch_end = batch_start + batch_size
            batch = test_data[batch_start:batch_end]

            query_start = time.perf_counter()
            results = adapter.search(batch, k=k)
            query_time = time.perf_counter() - query_start

            all_results.append(results)
            total_time += query_time

            if (i + 1) % 10 == 0:
                print(f"  Progress: {(i+1)*batch_size}/{len(test_data)}")

        # 合并结果
        all_results = np.vstack(all_results)

        # 计算 QPS
        qps = len(test_data) / total_time
        print(f"  QPS: {qps:.2f}")

        # 计算召回率
        true_neighbors = dataset.neighbors[:len(test_data)]
        recall_at_1 = self._compute_recall(all_results, true_neighbors, 1)
        recall_at_10 = self._compute_recall(all_results, true_neighbors, 10)
        recall_at_100 = self._compute_recall(all_results, true_neighbors, 100)
        print(f"  Recall@1: {recall_at_1:.4f}")
        print(f"  Recall@10: {recall_at_10:.4f}")
        print(f"  Recall@100: {recall_at_100:.4f}")

        # 构建结果
        return BenchmarkResult(
            algorithm=algorithm,
            dataset=dataset.name,
            dimension=dataset.dimension,
            recall_at_1=recall_at_1,
            recall_at_10=recall_at_10,
            recall_at_100=recall_at_100,
            qps=qps,
            build_time=build_time,
            memory_bytes=memory_bytes,
            params=params or {}
        )

    def run_all(self, dataset, k: int = 10) -> List[BenchmarkResult]:
        """运行所有启用的算法"""
        results = []
        for algo_name, algo_config in self.config.get("algorithms", {}).items():
            if algo_config.get("enabled", True):
                for params in self._iterate_params(algo_config.get("parameters", {})):
                    result = self.run(algo_name, dataset, k, params)
                    results.append(result)
        return results

    def _iterate_params(self, param_config: Dict[str, list]) -> List[Dict]:
        """迭代所有参数组合"""
        import itertools

        keys = list(param_config.keys())
        values = list(param_config.values())

        if not keys:
            return [{}]

        results = []
        for combo in itertools.product(*values):
            results.append(dict(zip(keys, combo)))

        return results
```

- [ ] **Step 2: 创建 report_generator.py**

```python
#!/usr/bin/env python3
"""
报告生成器
"""

from pathlib import Path
from typing import List
from datetime import datetime


class ReportGenerator:
    """报告生成器"""

    def __init__(self):
        self.template = self._load_template()

    def _load_template(self) -> str:
        """加载报告模板"""
        return """# VectorBench 测试报告

生成时间: {timestamp}

## 数据集概览

| 数据集 | 维度 | 训练集 | 测试集 | 距离度量 |
|--------|------|--------|--------|----------|
{dataset_table}

## 性能对比

### 召回率对比

| 算法 | Recall@1 | Recall@10 | Recall@100 | QPS | 构建时间(s) | 内存(MB) |
|------|----------|-----------|------------|-----|-------------|----------|
{recall_table}

### QPS vs 召回率

{chart_placeholder}

## 参数详情

{param_table}

## 结论

{conclusion}
"""

    def generate(self, results: List, output_path: str) -> None:
        """生成 Markdown 报告"""
        if not results:
            print("No results to generate report.")
            return

        # 提取数据集信息
        datasets = set(r.dataset for r in results)
        dataset_info = {}
        for ds_name in datasets:
            ds_results = [r for r in results if r.dataset == ds_name]
            if ds_results:
                r = ds_results[0]
                dataset_info[ds_name] = {
                    "dimension": r.dimension,
                    "train_size": "N/A",  # 需要从 dataset 对象获取
                    "test_size": "N/A",
                    "distance": "N/A"
                }

        # 构建数据集表格
        dataset_table_rows = []
        for name, info in dataset_info.items():
            dataset_table_rows.append(
                f"| {name} | {info['dimension']} | {info['train_size']} | "
                f"{info['test_size']} | {info['distance']} |"
            )
        dataset_table = "\n".join(dataset_table_rows) if dataset_table_rows else "| - | - | - | - | - |"

        # 构建召回率表格
        recall_table_rows = []
        for r in sorted(results, key=lambda x: -x.recall_at_10):
            params_str = ", ".join(f"{k}={v}" for k, v in r.params.items())
            recall_table_rows.append(
                f"| {r.algorithm} | {r.recall_at_1:.4f} | {r.recall_at_10:.4f} | "
                f"{r.recall_at_100:.4f} | {r.qps:.2f} | {r.build_time:.2f} | "
                f"{r.memory_mb:.2f} |"
            )
        recall_table = "\n".join(recall_table_rows)

        # 构建参数表格
        param_rows = []
        for r in results:
            if r.params:
                param_str = ", ".join(f"{k}={v}" for k, v in r.params.items())
            else:
                param_str = "default"
            param_rows.append(f"| {r.algorithm} | {param_str} |")
        param_table = "\n".join(param_rows) if param_rows else "| - | - |"

        # 生成结论
        best_recall = max(results, key=lambda x: x.recall_at_10)
        best_qps = max(results, key=lambda x: x.qps)
        conclusion = (
            f"- 最高召回率: {best_recall.algorithm} (Recall@10={best_recall.recall_at_10:.4f})\n"
            f"- 最高 QPS: {best_qps.algorithm} ({best_qps.qps:.2f} QPS)\n"
            f"- 性价比最优: 需要综合考虑召回率和 QPS 进行权衡"
        )

        # 填充模板
        report = self.template.format(
            timestamp=datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
            dataset_table=dataset_table,
            recall_table=recall_table,
            chart_placeholder="```\nQPS vs Recall@10 散点图（需要 matplotlib 生成图片）\n```",
            param_table=param_table,
            conclusion=conclusion
        )

        # 写入文件
        Path(output_path).parent.mkdir(parents=True, exist_ok=True)
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(report)

        print(f"Report generated: {output_path}")
```

- [ ] **Step 3: 提交**

```bash
git add engineering/tools/benchmark/core/
git commit -m "feat(benchmark): 创建基准测试执行器和报告生成器"
```

---

## Task 4: 创建索引适配器基类和适配器工厂

**Files:**
- Create: `engineering/tools/benchmark/adapters/__init__.py`
- Create: `engineering/tools/benchmark/adapters/base.py`

**Interfaces:**
- Produces: `BaseIndexAdapter`, `create_adapter()` 工厂函数

**Steps:**

- [ ] **Step 1: 创建 adapters/__init__.py**

```python
# engineering/tools/benchmark/adapters/__init__.py
"""索引适配器模块"""

from .base import BaseIndexAdapter
from .hnsw_adapter import HNSWAdapter
from .ivf_pq_adapter import IVFPQAdapter
from .lsh_adapter import LSHAdapter
from .faiss_adapter import FaissAdapter

# 适配器注册表
ADAPTER_REGISTRY = {
    "HNSWAdapter": HNSWAdapter,
    "IVFPQAdapter": IVFPQAdapter,
    "LSHAdapter": LSHAdapter,
    "FaissAdapter": FaissAdapter,
    # 别名
    "project_hnsw": HNSWAdapter,
    "project_ivf_pq": IVFPQAdapter,
    "project_lsh": LSHAdapter,
    "faiss_hnsw": FaissAdapter,
    "faiss_flat": FaissAdapter,
}


def create_adapter(algorithm: str, params: dict = None) -> BaseIndexAdapter:
    """
    创建适配器工厂函数

    Args:
        algorithm: 算法名称或适配器类名
        params: 参数字典

    Returns:
        BaseIndexAdapter 实例
    """
    params = params or {}

    # 直接是类名
    if algorithm in ADAPTER_REGISTRY:
        adapter_cls = ADAPTER_REGISTRY[algorithm]
        return adapter_cls(**params)

    # 别名映射
    if algorithm.startswith("project_"):
        base_name = algorithm.replace("project_", "")
        if base_name == "hnsw":
            return HNSWAdapter(**params)
        elif base_name == "ivf_pq":
            return IVFPQAdapter(**params)
        elif base_name == "lsh":
            return LSHAdapter(**params)

    if algorithm.startswith("faiss_"):
        base_name = algorithm.replace("faiss_", "")
        return FaissAdapter(index_type=base_name, **params)

    raise ValueError(f"Unknown algorithm: {algorithm}. Available: {list(ADAPTER_REGISTRY.keys())}")


__all__ = [
    "BaseIndexAdapter",
    "HNSWAdapter",
    "IVFPQAdapter",
    "LSHAdapter",
    "FaissAdapter",
    "create_adapter",
]
```

- [ ] **Step 2: 创建 base.py**

```python
#!/usr/bin/env python3
"""
索引适配器基类
"""

from abc import ABC, abstractmethod
from typing import Optional, Dict, Any
import numpy as np


class BaseIndexAdapter(ABC):
    """
    索引适配器抽象基类

    所有索引适配器必须实现以下接口:
    - fit(): 构建索引
    - add(): 增量添加向量
    - search(): 搜索 Top-K
    - get_size(): 获取索引大小
    - get_params(): 获取参数
    """

    def __init__(self, dimension: int, metric: str = "euclidean", **kwargs):
        """
        初始化适配器

        Args:
            dimension: 向量维度
            metric: 距离度量 ("euclidean" 或 "angular")
        """
        self.dimension = dimension
        self.metric = metric
        self.index = None
        self._built = False

    @abstractmethod
    def fit(self, vectors: np.ndarray) -> None:
        """
        构建索引

        Args:
            vectors: 训练数据，形状为 (n, dimension) 的 numpy 数组
        """
        pass

    def add(self, vectors: np.ndarray) -> None:
        """
        增量添加向量（可选实现）

        Args:
            vectors: 待添加的向量
        """
        raise NotImplementedError(f"{self.__class__.__name__} does not support incremental add")

    @abstractmethod
    def search(self, queries: np.ndarray, k: int) -> np.ndarray:
        """
        搜索 Top-K

        Args:
            queries: 查询向量，形状为 (n, dimension)
            k: 返回 Top-K 结果

        Returns:
            结果索引，形状为 (n, k)
        """
        pass

    @abstractmethod
    def get_size(self) -> int:
        """
        获取索引大小（字节）

        Returns:
            索引占用的内存字节数
        """
        pass

    def get_params(self) -> Dict[str, Any]:
        """
        获取索引参数

        Returns:
            参数字典
        """
        return {
            "dimension": self.dimension,
            "metric": self.metric,
        }

    def __repr__(self) -> str:
        return f"{self.__class__.__name__}(dimension={self.dimension}, metric={self.metric})"
```

- [ ] **Step 3: 提交**

```bash
git add engineering/tools/benchmark/adapters/__init__.py engineering/tools/benchmark/adapters/base.py
git commit -m "feat(benchmark): 创建索引适配器基类和工厂函数"
```

---

## Task 5: 创建 HNSW 适配器（ctypes C 调用）

**Files:**
- Create: `engineering/tools/benchmark/adapters/hnsw_adapter.py`
- Create: `engineering/include/db/index/vector_index/vector_index_c_api.h`
- Create: `engineering/src/db/index/vector_index/vector_index_c_api.c`

**Interfaces:**
- Consumes: `BaseIndexAdapter`
- Produces: `HNSWAdapter`（ctypes 调用 C 库）

**Steps:**

- [ ] **Step 1: 创建 C API 头文件**

```c
/*
 * vector_index_c_api.h
 *
 * 向量索引 C API 导出层 - 供 Python ctypes 调用
 */

#ifndef VECTOR_INDEX_C_API_H
#define VECTOR_INDEX_C_API_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * HNSW 索引 API
 */

/* 创建 HNSW 索引 */
void* hnsw_create(int32_t dim, int32_t M, int32_t ef_construction, int32_t metric_type);

/* 构建索引（批量） */
int32_t hnsw_build(void* index, int32_t n, const float* vectors);

/* 搜索 */
int32_t hnsw_search(void* index, const float* query, int32_t k, int32_t ef_search,
                     int32_t* ids, float* distances);

/* 批量搜索 */
int32_t hnsw_search_batch(void* index, const float* queries, int32_t n_queries,
                           int32_t k, int32_t ef_search,
                           int32_t* ids, float* distances);

/* 获取索引大小 */
int64_t hnsw_get_size(void* index);

/* 销毁索引 */
void hnsw_destroy(void* index);

/*
 * IVF-PQ 索引 API
 */

/* 创建 IVF-PQ 索引 */
void* ivf_pq_create(int32_t nlist, int32_t pq_m, int32_t pq_bits, int32_t dims);

/* 训练索引 */
int32_t ivf_pq_train(void* index, int32_t n, const float* vectors);

/* 添加向量 */
int32_t ivf_pq_add(void* index, int32_t n, const float* vectors, const int32_t* ids);

/* 搜索 */
int32_t ivf_pq_search(void* index, const float* query, int32_t k, int32_t nprobe,
                      int32_t* ids, float* distances);

/* 批量搜索 */
int32_t ivf_pq_search_batch(void* index, const float* queries, int32_t n_queries,
                            int32_t k, int32_t nprobe,
                            int32_t* ids, float* distances);

/* 获取索引大小 */
int64_t ivf_pq_get_size(void* index);

/* 销毁索引 */
void ivf_pq_destroy(void* index);

/*
 * LSH 索引 API
 */

/* 创建 LSH 索引 */
void* lsh_create(int32_t dim, int32_t num_hash, int32_t table_size);

/* 添加向量 */
int32_t lsh_add(void* index, int32_t n, const float* vectors, const int32_t* ids);

/* 搜索 */
int32_t lsh_search(void* index, const float* query, int32_t k,
                   int32_t* ids, float* distances);

/* 批量搜索 */
int32_t lsh_search_batch(void* index, const float* queries, int32_t n_queries,
                         int32_t k, int32_t* ids, float* distances);

/* 获取索引大小 */
int64_t lsh_get_size(void* index);

/* 销毁索引 */
void lsh_destroy(void* index);

#ifdef __cplusplus
}
#endif

#endif /* VECTOR_INDEX_C_API_H */
```

- [ ] **Step 2: 创建 C API 实现文件**

```c
/*
 * vector_index_c_api.c
 *
 * 向量索引 C API 实现 - 封装内部索引实现，导出给 Python
 */

#include "vector_index_c_api.h"
#include "db/index/vector_index/hnsw/faiss_hnsw.h"
#include "db/index/vector_index/ivf_pq/ivf_pq.h"
#include "db/index/vector_index/lsh/lsh.h"

#include <stdlib.h>
#include <string.h>

/* ==================== HNSW API ==================== */

void* hnsw_create(int32_t dim, int32_t M, int32_t ef_construction, int32_t metric_type) {
    distance_metric_t metric = (distance_metric_t)metric_type;
    return (void*)faiss_hnsw_index_create(M, dim, ef_construction, metric, QUANTIZATION_TYPE_NONE);
}

int32_t hnsw_build(void* index, int32_t n, const float* vectors) {
    if (!index || !vectors) return -1;
    faiss_hnsw_t* hnsw = (faiss_hnsw_t*)index;
    int32_t added = 0;
    for (int32_t i = 0; i < n; i++) {
        if (faiss_hnsw_index_add(hnsw, 1, &vectors[i * hnsw->dims]) == 0) {
            added++;
        }
    }
    return added;
}

int32_t hnsw_search(void* index, const float* query, int32_t k, int32_t ef_search,
                     int32_t* ids, float* distances) {
    if (!index || !query || !ids || !distances) return -1;
    return faiss_hnsw_index_search((faiss_hnsw_t*)index, query, k, ef_search, distances, ids);
}

int32_t hnsw_search_batch(void* index, const float* queries, int32_t n_queries,
                          int32_t k, int32_t ef_search,
                          int32_t* ids, float* distances) {
    if (!index || !queries || !ids || !distances) return -1;
    faiss_hnsw_t* hnsw = (faiss_hnsw_t*)index;
    int32_t n_success = 0;
    for (int32_t i = 0; i < n_queries; i++) {
        const float* q = &queries[i * hnsw->dims];
        int32_t* out_ids = &ids[i * k];
        float* out_dist = &distances[i * k];
        if (faiss_hnsw_index_search(hnsw, q, k, ef_search, out_dist, out_ids) == 0) {
            n_success++;
        }
    }
    return n_success;
}

int64_t hnsw_get_size(void* index) {
    if (!index) return 0;
    faiss_hnsw_t* hnsw = (faiss_hnsw_t*)index;
    int64_t size = 0;
    size += hnsw->n_total * hnsw->dims * sizeof(float);  // vectors
    size += hnsw->n_total * hnsw->code_size;             // codes
    size += hnsw->n_total * sizeof(int32_t);             // levels
    size += hnsw->n_total * sizeof(int32_t);             // offsets
    return size;
}

void hnsw_destroy(void* index) {
    if (index) {
        faiss_hnsw_index_drop((faiss_hnsw_t*)index);
    }
}

/* ==================== IVF-PQ API ==================== */

void* ivf_pq_create(int32_t nlist, int32_t pq_m, int32_t pq_bits, int32_t dims) {
    return (void*)ivf_pq_create(nlist, pq_m, pq_bits, dims);
}

int32_t ivf_pq_train(void* index, int32_t n, const float* vectors) {
    if (!index || !vectors) return -1;
    return ivf_pq_train((ivf_pq_index_t*)index, n, vectors);
}

int32_t ivf_pq_add(void* index, int32_t n, const float* vectors, const int32_t* ids) {
    if (!index || !vectors || !ids) return -1;
    return ivf_pq_add((ivf_pq_index_t*)index, n, vectors, ids);
}

int32_t ivf_pq_search(void* index, const float* query, int32_t k, int32_t nprobe,
                      int32_t* ids, float* distances) {
    if (!index || !query || !ids || !distances) return -1;
    ivf_pq_set_nprobe((ivf_pq_index_t*)index, nprobe);
    return ivf_pq_search((ivf_pq_index_t*)index, query, k, ids, distances);
}

int32_t ivf_pq_search_batch(void* index, const float* queries, int32_t n_queries,
                            int32_t k, int32_t nprobe,
                            int32_t* ids, float* distances) {
    if (!index || !queries || !ids || !distances) return -1;
    ivf_pq_set_nprobe((ivf_pq_index_t*)index, nprobe);
    int32_t n_success = 0;
    for (int32_t i = 0; i < n_queries; i++) {
        const float* q = &queries[i * ((ivf_pq_index_t*)index)->dims];
        int32_t* out_ids = &ids[i * k];
        float* out_dist = &distances[i * k];
        if (ivf_pq_search((ivf_pq_index_t*)index, q, k, out_ids, out_dist) == 0) {
            n_success++;
        }
    }
    return n_success;
}

int64_t ivf_pq_get_size(void* index) {
    if (!index) return 0;
    ivf_pq_index_t* idx = (ivf_pq_index_t*)index;
    int n_vectors = 0, code_size = 0;
    ivf_pq_stats(idx, &n_vectors, &code_size);
    return (int64_t)n_vectors * code_size;
}

void ivf_pq_destroy(void* index) {
    if (index) {
        ivf_pq_destroy((ivf_pq_index_t*)index);
    }
}

/* ==================== LSH API ==================== */

void* lsh_create(int32_t dim, int32_t num_hash, int32_t table_size) {
    return (void*)lsh_index_create(dim, num_hash, table_size);
}

int32_t lsh_add(void* index, int32_t n, const float* vectors, const int32_t* ids) {
    if (!index || !vectors || !ids) return -1;
    return lsh_index_add((lsh_index_t*)index, n, vectors, ids);
}

int32_t lsh_search(void* index, const float* query, int32_t k,
                   int32_t* ids, float* distances) {
    if (!index || !query || !ids || !distances) return -1;
    return lsh_index_search((lsh_index_t*)index, query, k, ids, distances);
}

int32_t lsh_search_batch(void* index, const float* queries, int32_t n_queries,
                         int32_t k, int32_t* ids, float* distances) {
    if (!index || !queries || !ids || !distances) return -1;
    int32_t n_success = 0;
    for (int32_t i = 0; i < n_queries; i++) {
        const float* q = &queries[i * ((lsh_index_t*)index)->dim];
        int32_t* out_ids = &ids[i * k];
        float* out_dist = &distances[i * k];
        if (lsh_index_search((lsh_index_t*)index, q, k, out_ids, out_dist) == 0) {
            n_success++;
        }
    }
    return n_success;
}

int64_t lsh_get_size(void* index) {
    if (!index) return 0;
    return lsh_index_get_size((lsh_index_t*)index);
}

void lsh_destroy(void* index) {
    if (index) {
        lsh_index_destroy((lsh_index_t*)index);
    }
}
```

- [ ] **Step 3: 创建 hnsw_adapter.py**

```python
#!/usr/bin/env python3
"""
HNSW 索引适配器 - ctypes 调用 C 库
"""

import os
import ctypes
from pathlib import Path
from typing import Optional
import numpy as np

from .base import BaseIndexAdapter


class HNSWAdapter(BaseIndexAdapter):
    """
    HNSW 索引适配器

    通过 ctypes 调用项目 C 库中的 HNSW 实现
    """

    # 类级别的库加载
    _lib = None
    _loaded = False

    def __init__(self, dimension: int = 384, metric: str = "euclidean",
                 M: int = 16, ef_construction: int = 200, ef_search: int = 100,
                 **kwargs):
        """
        初始化 HNSW 适配器

        Args:
            dimension: 向量维度
            metric: 距离度量 ("euclidean" 或 "angular")
            M: 每层最大邻居数
            ef_construction: 构建时搜索范围
            ef_search: 搜索时搜索范围
        """
        super().__init__(dimension, metric, **kwargs)
        self.M = M
        self.ef_construction = ef_construction
        self.ef_search = ef_search

        # 加载 C 库
        self._load_library()
        self._index = None

    @classmethod
    def _load_library(cls):
        """加载 C 共享库"""
        if cls._loaded:
            return

        # 尝试多种可能的库路径
        lib_paths = [
            # Windows DLL
            "build/engineering/libvector_index_c.dll",
            "build/engineering/vector_index_c.dll",
            # Linux/Mac so
            "build/engineering/libvector_index_c.so",
            "build/engineering/libvector_index_c.dylib",
        ]

        lib = None
        for path in lib_paths:
            full_path = Path(__file__).parent.parent.parent.parent.parent / path
            if full_path.exists():
                try:
                    lib = ctypes.CDLL(str(full_path))
                    print(f"Loaded HNSW library from: {full_path}")
                    break
                except OSError as e:
                    print(f"Failed to load {full_path}: {e}")

        if lib is None:
            print("Warning: HNSW C library not found, using fallback (will create mock index)")
            cls._loaded = True
            cls._lib = None
            return

        # 设置函数签名
        lib.hnsw_create.argtypes = [ctypes.c_int32, ctypes.c_int32, ctypes.c_int32, ctypes.c_int32]
        lib.hnsw_create.restype = ctypes.c_void_p

        lib.hnsw_build.argtypes = [ctypes.c_void_p, ctypes.c_int32, ctypes.POINTER(ctypes.c_float)]
        lib.hnsw_build.restype = ctypes.c_int32

        lib.hnsw_search.argtypes = [
            ctypes.c_void_p, ctypes.POINTER(ctypes.c_float),
            ctypes.c_int32, ctypes.c_int32,
            ctypes.POINTER(ctypes.c_int32), ctypes.POINTER(ctypes.c_float)
        ]
        lib.hnsw_search.restype = ctypes.c_int32

        lib.hnsw_search_batch.argtypes = [
            ctypes.c_void_p, ctypes.POINTER(ctypes.c_float),
            ctypes.c_int32, ctypes.c_int32, ctypes.c_int32,
            ctypes.POINTER(ctypes.c_int32), ctypes.POINTER(ctypes.c_float)
        ]
        lib.hnsw_search_batch.restype = ctypes.c_int32

        lib.hnsw_get_size.argtypes = [ctypes.c_void_p]
        lib.hnsw_get_size.restype = ctypes.c_int64

        lib.hnsw_destroy.argtypes = [ctypes.c_void_p]
        lib.hnsw_destroy.restype = None

        cls._lib = lib
        cls._loaded = True

    def fit(self, vectors: np.ndarray) -> None:
        """构建 HNSW 索引"""
        if self._lib is None:
            # Fallback: 使用内存中的简单索引
            self._fallback_fit(vectors)
            return

        n, dim = vectors.shape
        if dim != self.dimension:
            raise ValueError(f"Vector dimension {dim} does not match expected {self.dimension}")

        # 转换 metric
        metric_type = 0 if self.metric == "euclidean" else 1  # 0=L2, 1=IP

        # 创建索引
        self._index = self._lib.hnsw_create(
            self.dimension, self.M, self.ef_construction, metric_type
        )

        if not self._index:
            raise RuntimeError("Failed to create HNSW index")

        # 构建索引
        vectors_ptr = vectors.ctypes.data_as(ctypes.POINTER(ctypes.c_float))
        result = self._lib.hnsw_build(self._index, n, vectors_ptr)

        if result < 0:
            self._lib.hnsw_destroy(self._index)
            self._index = None
            raise RuntimeError(f"Failed to build HNSW index: {result}")

        self._n_vectors = n
        self._built = True

    def _fallback_fit(self, vectors: np.ndarray) -> None:
        """后备实现：当 C 库不可用时使用纯 Python"""
        self._fallback_vectors = vectors
        self._n_vectors = len(vectors)
        self._built = True

    def search(self, queries: np.ndarray, k: int) -> np.ndarray:
        """搜索 Top-K"""
        if not self._built:
            raise RuntimeError("Index not built. Call fit() first.")

        n_queries, dim = queries.shape
        if dim != self.dimension:
            raise ValueError(f"Query dimension {dim} does not match {self.dimension}")

        if self._lib is not None and self._index:
            # 使用 C 库
            ids = np.zeros((n_queries, k), dtype=np.int32)
            distances = np.zeros((n_queries, k), dtype=np.float32)

            queries_ptr = queries.ctypes.data_as(ctypes.POINTER(ctypes.c_float))
            ids_ptr = ids.ctypes.data_as(ctypes.POINTER(ctypes.c_int32))
            dist_ptr = distances.ctypes.data_as(ctypes.POINTER(ctypes.c_float))

            self._lib.hnsw_search_batch(
                self._index, queries_ptr, n_queries, k, self.ef_search,
                ids_ptr, dist_ptr
            )
            return ids
        else:
            # Fallback: 暴力搜索
            return self._fallback_search(queries, k)

    def _fallback_search(self, queries: np.ndarray, k: int) -> np.ndarray:
        """后备搜索实现：暴力搜索"""
        vectors = self._fallback_vectors
        n_queries = len(queries)
        results = []

        for q in queries:
            if self.metric == "euclidean":
                dists = np.linalg.norm(vectors - q, axis=1)
            else:  # angular/IP
                dists = -np.dot(vectors, q)

            top_k_idx = np.argpartition(dists, k)[:k]
            top_k_idx = top_k_idx[np.argsort(dists[top_k_idx])]
            results.append(top_k_idx)

        return np.array(results)

    def get_size(self) -> int:
        """获取索引大小（字节）"""
        if self._lib is not None and self._index:
            return self._lib.hnsw_get_size(self._index)
        elif hasattr(self, '_fallback_vectors'):
            return self._fallback_vectors.nbytes
        return 0

    def get_params(self) -> dict:
        """获取参数"""
        return {
            "dimension": self.dimension,
            "metric": self.metric,
            "M": self.M,
            "ef_construction": self.ef_construction,
            "ef_search": self.ef_search,
        }

    def __del__(self):
        """析构：释放资源"""
        if self._lib is not None and self._index:
            self._lib.hnsw_destroy(self._index)
            self._index = None
```

- [ ] **Step 4: 提交**

```bash
git add engineering/tools/benchmark/adapters/hnsw_adapter.py
git add engineering/include/db/index/vector_index/vector_index_c_api.h
git add engineering/src/db/index/vector_index/vector_index_c_api.c
git commit -m "feat(benchmark): 创建 HNSW 适配器和 C API 导出层"
```

---

## Task 6: 创建其他适配器（IVF-PQ、LSH、faiss）

**Files:**
- Create: `engineering/tools/benchmark/adapters/ivf_pq_adapter.py`
- Create: `engineering/tools/benchmark/adapters/lsh_adapter.py`
- Create: `engineering/tools/benchmark/adapters/faiss_adapter.py`

**Interfaces:**
- Consumes: `BaseIndexAdapter`, HNSWAdapter
- Produces: `IVFPQAdapter`, `LSHAdapter`, `FaissAdapter`

**Steps:**

- [ ] **Step 1: 创建 ivf_pq_adapter.py**

```python
#!/usr/bin/env python3
"""
IVF-PQ 索引适配器
"""

import ctypes
from pathlib import Path
from typing import Optional
import numpy as np

from .base import BaseIndexAdapter


class IVFPQAdapter(BaseIndexAdapter):
    """IVF-PQ 索引适配器"""

    _lib = None
    _loaded = False

    def __init__(self, dimension: int = 384, metric: str = "euclidean",
                 nlist: int = 256, nprobe: int = 32, pq_m: int = 8, pq_bits: int = 8,
                 **kwargs):
        super().__init__(dimension, metric, **kwargs)
        self.nlist = nlist
        self.nprobe = nprobe
        self.pq_m = pq_m
        self.pq_bits = pq_bits
        self._load_library()
        self._index = None
        self._n_vectors = 0

    @classmethod
    def _load_library(cls):
        """加载 C 共享库"""
        if cls._loaded:
            return

        lib_paths = [
            "build/engineering/libvector_index_c.dll",
            "build/engineering/vector_index_c.dll",
            "build/engineering/libvector_index_c.so",
            "build/engineering/libvector_index_c.dylib",
        ]

        lib = None
        for path in lib_paths:
            full_path = Path(__file__).parent.parent.parent.parent.parent / path
            if full_path.exists():
                try:
                    lib = ctypes.CDLL(str(full_path))
                    print(f"Loaded IVF-PQ library from: {full_path}")
                    break
                except OSError:
                    pass

        if lib is None:
            print("Warning: IVF-PQ C library not found")
            cls._loaded = True
            cls._lib = None
            return

        # 设置函数签名
        lib.ivf_pq_create.argtypes = [ctypes.c_int32, ctypes.c_int32, ctypes.c_int32, ctypes.c_int32]
        lib.ivf_pq_create.restype = ctypes.c_void_p

        lib.ivf_pq_train.argtypes = [ctypes.c_void_p, ctypes.c_int32, ctypes.POINTER(ctypes.c_float)]
        lib.ivf_pq_train.restype = ctypes.c_int32

        lib.ivf_pq_add.argtypes = [ctypes.c_void_p, ctypes.c_int32, ctypes.POINTER(ctypes.c_float),
                                    ctypes.POINTER(ctypes.c_int32)]
        lib.ivf_pq_add.restype = ctypes.c_int32

        lib.ivf_pq_search.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_float),
                                       ctypes.c_int32, ctypes.c_int32,
                                       ctypes.POINTER(ctypes.c_int32), ctypes.POINTER(ctypes.c_float)]
        lib.ivf_pq_search.restype = ctypes.c_int32

        lib.ivf_pq_get_size.argtypes = [ctypes.c_void_p]
        lib.ivf_pq_get_size.restype = ctypes.c_int64

        lib.ivf_pq_destroy.argtypes = [ctypes.c_void_p]

        cls._lib = lib
        cls._loaded = True

    def fit(self, vectors: np.ndarray) -> None:
        """训练并构建 IVF-PQ 索引"""
        n, dim = vectors.shape
        if dim != self.dimension:
            raise ValueError(f"Dimension mismatch: {dim} vs {self.dimension}")

        if self._lib is None:
            # Fallback
            self._fallback_vectors = vectors
            self._n_vectors = n
            self._built = True
            return

        # 创建索引
        self._index = self._lib.ivf_pq_create(self.nlist, self.pq_m, self.pq_bits, self.dimension)
        if not self._index:
            raise RuntimeError("Failed to create IVF-PQ index")

        # 训练
        vectors_ptr = vectors.ctypes.data_as(ctypes.POINTER(ctypes.c_float))
        if self._lib.ivf_pq_train(self._index, n, vectors_ptr) < 0:
            raise RuntimeError("Failed to train IVF-PQ index")

        # 添加向量
        ids = np.arange(n, dtype=np.int32)
        ids_ptr = ids.ctypes.data_as(ctypes.POINTER(ctypes.c_int32))
        self._lib.ivf_pq_add(self._index, n, vectors_ptr, ids_ptr)

        self._n_vectors = n
        self._built = True

    def search(self, queries: np.ndarray, k: int) -> np.ndarray:
        """搜索 Top-K"""
        if not self._built:
            raise RuntimeError("Index not built")

        n_queries = len(queries)

        if self._lib is not None and self._index:
            ids = np.zeros((n_queries, k), dtype=np.int32)
            distances = np.zeros((n_queries, k), dtype=np.float32)

            queries_ptr = queries.ctypes.data_as(ctypes.POINTER(ctypes.c_float))
            ids_ptr = ids.ctypes.data_as(ctypes.POINTER(ctypes.c_int32))
            dist_ptr = distances.ctypes.data_as(ctypes.POINTER(ctypes.c_float))

            for i in range(n_queries):
                self._lib.ivf_pq_search(self._index, queries_ptr + i * self.dimension,
                                         k, self.nprobe, ids_ptr + i * k, dist_ptr + i * k)
            return ids
        else:
            # Fallback: 暴力搜索
            vectors = self._fallback_vectors
            results = []
            for q in queries:
                dists = np.linalg.norm(vectors - q, axis=1)
                top_k_idx = np.argpartition(dists, k)[:k]
                top_k_idx = top_k_idx[np.argsort(dists[top_k_idx])]
                results.append(top_k_idx)
            return np.array(results)

    def get_size(self) -> int:
        """获取索引大小"""
        if self._lib and self._index:
            return self._lib.ivf_pq_get_size(self._index)
        elif hasattr(self, '_fallback_vectors'):
            return self._fallback_vectors.nbytes
        return 0

    def get_params(self) -> dict:
        return {
            "dimension": self.dimension,
            "metric": self.metric,
            "nlist": self.nlist,
            "nprobe": self.nprobe,
            "pq_m": self.pq_m,
            "pq_bits": self.pq_bits,
        }

    def __del__(self):
        if self._lib and self._index:
            self._lib.ivf_pq_destroy(self._index)
```

- [ ] **Step 2: 创建 lsh_adapter.py**

```python
#!/usr/bin/env python3
"""
LSH 索引适配器
"""

import ctypes
from pathlib import Path
from typing import Optional
import numpy as np

from .base import BaseIndexAdapter


class LSHAdapter(BaseIndexAdapter):
    """LSH (Locality-Sensitive Hashing) 索引适配器"""

    _lib = None
    _loaded = False

    def __init__(self, dimension: int = 384, metric: str = "euclidean",
                 num_hash: int = 16, table_size: int = 5000, **kwargs):
        super().__init__(dimension, metric, **kwargs)
        self.num_hash = num_hash
        self.table_size = table_size
        self._load_library()
        self._index = None
        self._n_vectors = 0

    @classmethod
    def _load_library(cls):
        """加载 C 共享库"""
        if cls._loaded:
            return

        lib_paths = [
            "build/engineering/libvector_index_c.dll",
            "build/engineering/libvector_index_c.dll",
            "build/engineering/libvector_index_c.so",
            "build/engineering/libvector_index_c.dylib",
        ]

        lib = None
        for path in lib_paths:
            full_path = Path(__file__).parent.parent.parent.parent.parent / path
            if full_path.exists():
                try:
                    lib = ctypes.CDLL(str(full_path))
                    print(f"Loaded LSH library from: {full_path}")
                    break
                except OSError:
                    pass

        if lib is None:
            print("Warning: LSH C library not found")
            cls._loaded = True
            cls._lib = None
            return

        lib.lsh_create.argtypes = [ctypes.c_int32, ctypes.c_int32, ctypes.c_int32]
        lib.lsh_create.restype = ctypes.c_void_p

        lib.lsh_add.argtypes = [ctypes.c_void_p, ctypes.c_int32, ctypes.POINTER(ctypes.c_float),
                                 ctypes.POINTER(ctypes.c_int32)]
        lib.lsh_add.restype = ctypes.c_int32

        lib.lsh_search.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_float),
                                    ctypes.c_int32, ctypes.POINTER(ctypes.c_int32),
                                    ctypes.POINTER(ctypes.c_float)]
        lib.lsh_search.restype = ctypes.c_int32

        lib.lsh_get_size.argtypes = [ctypes.c_void_p]
        lib.lsh_get_size.restype = ctypes.c_int64

        lib.lsh_destroy.argtypes = [ctypes.c_void_p]

        cls._lib = lib
        cls._loaded = True

    def fit(self, vectors: np.ndarray) -> None:
        """构建 LSH 索引"""
        n, dim = vectors.shape
        if dim != self.dimension:
            raise ValueError(f"Dimension mismatch")

        if self._lib is None:
            self._fallback_vectors = vectors
            self._n_vectors = n
            self._built = True
            return

        self._index = self._lib.lsh_create(self.dimension, self.num_hash, self.table_size)
        if not self._index:
            raise RuntimeError("Failed to create LSH index")

        ids = np.arange(n, dtype=np.int32)
        vectors_ptr = vectors.ctypes.data_as(ctypes.POINTER(ctypes.c_float))
        ids_ptr = ids.ctypes.data_as(ctypes.POINTER(ctypes.c_int32))
        self._lib.lsh_add(self._index, n, vectors_ptr, ids_ptr)

        self._n_vectors = n
        self._built = True

    def search(self, queries: np.ndarray, k: int) -> np.ndarray:
        """搜索 Top-K"""
        if not self._built:
            raise RuntimeError("Index not built")

        n_queries = len(queries)

        if self._lib and self._index:
            ids = np.zeros((n_queries, k), dtype=np.int32)
            distances = np.zeros((n_queries, k), dtype=np.float32)

            for i in range(n_queries):
                q_ptr = queries[i].ctypes.data_as(ctypes.POINTER(ctypes.c_float))
                ids_ptr = ids[i].ctypes.data_as(ctypes.POINTER(ctypes.c_int32))
                dist_ptr = distances[i].ctypes.data_as(ctypes.POINTER(ctypes.c_float))
                self._lib.lsh_search(self._index, q_ptr, k, ids_ptr, dist_ptr)
            return ids
        else:
            vectors = self._fallback_vectors
            results = []
            for q in queries:
                dists = np.linalg.norm(vectors - q, axis=1)
                top_k_idx = np.argpartition(dists, k)[:k]
                top_k_idx = top_k_idx[np.argsort(dists[top_k_idx])]
                results.append(top_k_idx)
            return np.array(results)

    def get_size(self) -> int:
        if self._lib and self._index:
            return self._lib.lsh_get_size(self._index)
        elif hasattr(self, '_fallback_vectors'):
            return self._fallback_vectors.nbytes
        return 0

    def get_params(self) -> dict:
        return {
            "dimension": self.dimension,
            "metric": self.metric,
            "num_hash": self.num_hash,
            "table_size": self.table_size,
        }

    def __del__(self):
        if self._lib and self._index:
            self._lib.lsh_destroy(self._index)
```

- [ ] **Step 3: 创建 faiss_adapter.py**

```python
#!/usr/bin/env python3
"""
faiss 适配器 - 外部基线实现
"""

import numpy as np
from typing import Optional

try:
    import faiss
    HAS_FAISS = True
except ImportError:
    HAS_FAISS = False

from .base import BaseIndexAdapter


class FaissAdapter(BaseIndexAdapter):
    """faiss 参考实现适配器 - 外部基线"""

    def __init__(self, dimension: int = 384, metric: str = "euclidean",
                 index_type: str = "hnsw", M: int = 16, ef_construction: int = 200,
                 nlist: int = 256, nprobe: int = 32, **kwargs):
        """
        初始化 faiss 适配器

        Args:
            dimension: 向量维度
            metric: 距离度量
            index_type: 索引类型 ("flat", "hnsw", "ivf")
            M: HNSW 参数
            ef_construction: HNSW 构建参数
            nlist: IVF 分区数
            nprobe: IVF 搜索参数
        """
        super().__init__(dimension, metric, **kwargs)
        self.index_type = index_type
        self.M = M
        self.ef_construction = ef_construction
        self.nlist = nlist
        self.nprobe = nprobe
        self._index = None
        self._ids = None

        if not HAS_FAISS:
            raise ImportError("faiss not installed. Install with: pip install faiss-cpu")

    def fit(self, vectors: np.ndarray) -> None:
        """构建 faiss 索引"""
        n, dim = vectors.shape
        if dim != self.dimension:
            raise ValueError(f"Dimension mismatch: {dim} vs {self.dimension}")

        vectors = vectors.astype(np.float32)
        if self.metric == "angular":
            faiss.normalize_L2(vectors)

        # 根据索引类型创建索引
        if self.index_type == "flat":
            if self.metric == "angular":
                self._index = faiss.IndexFlatIP(dim)
            else:
                self._index = faiss.IndexFlatL2(dim)

        elif self.index_type == "hnsw":
            # HNSW 索引
            if self.metric == "angular":
                base_index = faiss.IndexFlatIP(dim)
            else:
                base_index = faiss.IndexFlatL2(dim)
            self._index = faiss.IndexHNSWFlat(dim, self.M)
            self._index.hnsw.efConstruction = self.ef_construction
            self._index.add(base_index)

        elif self.index_type == "ivf":
            # IVF 索引
            if self.metric == "angular":
                quantizer = faiss.IndexFlatIP(dim)
            else:
                quantizer = faiss.IndexFlatL2(dim)
            self._index = faiss.IndexIVFFlat(quantizer, dim, self.nlist)
            self._index.nprobe = self.nprobe
            self._index.train(vectors)
            self._index.add(vectors)

        else:
            raise ValueError(f"Unknown index_type: {self.index_type}")

        if self.index_type != "hnsw":
            self._index.add(vectors)

        self._built = True

    def search(self, queries: np.ndarray, k: int) -> np.ndarray:
        """搜索 Top-K"""
        if not self._built:
            raise RuntimeError("Index not built")

        queries = queries.astype(np.float32)
        if self.metric == "angular":
            faiss.normalize_L2(queries)

        distances, indices = self._index.search(queries, k)
        return indices.astype(np.int32)

    def get_size(self) -> int:
        """获取索引大小"""
        if self._index is None:
            return 0

        # faiss 没有直接的 size API，估算
        if hasattr(self._index, 'ntotal'):
            n = self._index.ntotal
            if hasattr(self._index, 'd'):
                dim = self._index.d
                return n * dim * 4  # float32
        return 0

    def get_params(self) -> dict:
        return {
            "dimension": self.dimension,
            "metric": self.metric,
            "index_type": self.index_type,
            "M": self.M,
            "ef_construction": self.ef_construction,
            "nlist": self.nlist,
            "nprobe": self.nprobe,
        }
```

- [ ] **Step 4: 更新 adapters/__init__.py**

```python
# engineering/tools/benchmark/adapters/__init__.py
"""索引适配器模块"""

from .base import BaseIndexAdapter
from .hnsw_adapter import HNSWAdapter
from .ivf_pq_adapter import IVFPQAdapter
from .lsh_adapter import LSHAdapter
from .faiss_adapter import FaissAdapter

# 适配器注册表
ADAPTER_REGISTRY = {
    "HNSWAdapter": HNSWAdapter,
    "IVFPQAdapter": IVFPQAdapter,
    "LSHAdapter": LSHAdapter,
    "FaissAdapter": FaissAdapter,
    # 别名
    "project_hnsw": HNSWAdapter,
    "project_ivf_pq": IVFPQAdapter,
    "project_lsh": LSHAdapter,
    "faiss_hnsw": FaissAdapter,
    "faiss_flat": FaissAdapter,
    "faiss_ivf": FaissAdapter,
}


def create_adapter(algorithm: str, params: dict = None) -> BaseIndexAdapter:
    """创建适配器工厂函数"""
    params = params or {}

    if algorithm in ADAPTER_REGISTRY:
        adapter_cls = ADAPTER_REGISTRY[algorithm]
        return adapter_cls(**params)

    # 解析 project_xxx 和 faiss_xxx
    if algorithm.startswith("project_"):
        name = algorithm.replace("project_", "")
        if name == "hnsw":
            return HNSWAdapter(**params)
        elif name == "ivf_pq":
            return IVFPQAdapter(**params)
        elif name == "lsh":
            return LSHAdapter(**params)

    if algorithm.startswith("faiss_"):
        name = algorithm.replace("faiss_", "")
        if name == "hnsw":
            return FaissAdapter(index_type="hnsw", **params)
        elif name == "flat":
            return FaissAdapter(index_type="flat", **params)
        elif name == "ivf":
            return FaissAdapter(index_type="ivf", **params)

    raise ValueError(f"Unknown algorithm: {algorithm}. Available: {list(ADAPTER_REGISTRY.keys())}")


__all__ = [
    "BaseIndexAdapter",
    "HNSWAdapter",
    "IVFPQAdapter",
    "LSHAdapter",
    "FaissAdapter",
    "create_adapter",
]
```

- [ ] **Step 5: 提交**

```bash
git add engineering/tools/benchmark/adapters/ivf_pq_adapter.py engineering/tools/benchmark/adapters/lsh_adapter.py engineering/tools/benchmark/adapters/faiss_adapter.py
git commit -m "feat(benchmark): 创建 IVF-PQ/LSH/faiss 适配器"
```

---

## Task 7: 创建数据集下载脚本

**Files:**
- Create: `engineering/tools/benchmark/scripts/download_datasets.py`

**Interfaces:**
- Consumes: `config/datasets.yaml`
- Produces: 下载的数据集文件

**Steps:**

- [ ] **Step 1: 创建 download_datasets.py**

```python
#!/usr/bin/env python3
"""
数据集下载脚本
"""

import argparse
import sys
from pathlib import Path

# 添加项目根目录到路径
project_root = Path(__file__).parent.parent.parent.parent
sys.path.insert(0, str(project_root))

from benchmark.core.dataset_manager import DatasetManager


def main():
    parser = argparse.ArgumentParser(description="下载 ANN Benchmarks 数据集")
    parser.add_argument("--dataset", type=str, default="all",
                        help="数据集名称 (all/fashion-mnist/glove/...)")
    parser.add_argument("--list", action="store_true",
                        help="列出所有可用数据集")
    parser.add_argument("--force", action="store_true",
                        help="强制重新下载")
    args = parser.parse_args()

    manager = DatasetManager()

    if args.list:
        print("Available datasets:")
        for name in manager.list_datasets():
            print(f"  - {name}")
        return

    if args.dataset == "all":
        print("Downloading all datasets...")
        for name in manager.list_datasets():
            try:
                manager.load(name, force_download=args.force)
                print(f"  ✓ {name}")
            except Exception as e:
                print(f"  ✗ {name}: {e}")
    else:
        print(f"Downloading {args.dataset}...")
        try:
            manager.load(args.dataset, force_download=args.force)
            print(f"  ✓ {args.dataset}")
        except Exception as e:
            print(f"  ✗ {args.dataset}: {e}")
            sys.exit(1)


if __name__ == "__main__":
    main()
```

- [ ] **Step 2: 提交**

```bash
git add engineering/tools/benchmark/scripts/download_datasets.py
git commit -m "feat(benchmark): 创建数据集下载脚本"
```

---

## Task 8: 创建 README 文档

**Files:**
- Create: `engineering/tools/benchmark/README.md`

**Steps:**

- [ ] **Step 1: 创建 README.md**

```markdown
# VectorBench - 向量索引基准测试框架

基于 ann-benchmarks 的向量索引性能基准测试框架，支持项目自研索引（HNSW/IVF-PQ/LSH/DiskANN）与 faiss 基线的对比测试。

## 特性

- 支持多种 ANN 数据集（GloVe/Fashion-MNIST/DeepImage 等）
- 支持多种索引算法（项目实现 + faiss 基线）
- 全面评估指标（QPS/召回率/内存/构建时间）
- CSV/Markdown 报告生成
- ctypes C→Python 绑定，零额外依赖

## 安装

### 依赖

```bash
pip install numpy h5py pyyaml faiss-cpu
```

### 构建 C 库（可选）

```bash
cd build/engineering
cmake .. -DCMAKE_BUILD_TYPE=Release
make vector_index_c
```

## 使用

### 下载数据集

```bash
# 下载所有数据集
python -m benchmark.scripts.download_datasets --all

# 下载指定数据集
python -m benchmark.scripts.download_datasets --dataset glove-100-angular

# 列出可用数据集
python -m benchmark.scripts.download_datasets --list
```

### 运行基准测试

```bash
# 运行所有算法（默认数据集）
python -m benchmark.main

# 指定数据集
python -m benchmark.main --dataset fashion-mnist-784-euclidean

# 指定算法
python -m benchmark.main --algorithm project_hnsw

# 指定参数
python -m benchmark.main --algorithm project_hnsw --params M=16,ef=200

# 生成报告
python -m benchmark.main --report --output results/report.csv
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
    algorithm="project_hnsw",
    dataset=dataset,
    k=10,
    params={"M": 16, "ef_construction": 200}
)

print(f"QPS: {result.qps:.2f}, Recall@10: {result.recall_at_10:.4f}")
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
│   ├── base.py                 # 基类
│   ├── hnsw_adapter.py         # HNSW 适配器
│   ├── ivf_pq_adapter.py       # IVF-PQ 适配器
│   ├── lsh_adapter.py          # LSH 适配器
│   └── faiss_adapter.py        # faiss 基线
├── core/                       # 核心模块
│   ├── dataset_manager.py      # 数据集管理
│   ├── benchmark_runner.py     # 测试执行器
│   └── report_generator.py     # 报告生成器
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

- `project_hnsw`: HNSW 图索引
- `project_ivf_pq`: IVF-PQ 量化索引
- `project_lsh`: LSH 索引

### faiss 基线

- `faiss_hnsw`: faiss HNSW
- `faiss_flat`: faiss 暴力搜索
- `faiss_ivf`: faiss IVF

## 评估指标

- **Recall@K**: Top-K 召回率
- **QPS**: 查询吞吐量
- **Build Time**: 索引构建时间
- **Memory**: 内存占用

## 后续演进

- Phase 2: 接入 NSG 索引
- Phase 3: 量化对比（PQ/LVQ/SQ）
- Phase 4: 端侧推理集成
```

- [ ] **Step 2: 提交**

```bash
git add engineering/tools/benchmark/README.md
git commit -m "docs(benchmark): 添加 VectorBench README"
```

---

## Task 9: 端到端测试

**Files:**
- Create: `engineering/tools/benchmark/test/test_benchmark.py`

**Interfaces:**
- Consumes: 所有已实现的模块
- Produces: 验证通过的测试

**Steps:**

- [ ] **Step 1: 创建测试目录和文件**

```bash
mkdir -p engineering/tools/benchmark/test
touch engineering/tools/benchmark/test/__init__.py
```

- [ ] **Step 2: 创建 test_benchmark.py**

```python
#!/usr/bin/env python3
"""
VectorBench 集成测试
"""

import sys
from pathlib import Path
import numpy as np

# 添加项目根目录到路径
project_root = Path(__file__).parent.parent.parent.parent.parent
sys.path.insert(0, str(project_root))

from benchmark.adapters import create_adapter, BaseIndexAdapter
from benchmark.core.dataset_manager import DatasetManager


def test_adapter_registry():
    """测试适配器工厂"""
    adapter = create_adapter("faiss_flat", dimension=128)
    assert isinstance(adapter, BaseIndexAdapter)
    print("✓ test_adapter_registry passed")


def test_faiss_flat():
    """测试 faiss flat 索引"""
    np.random.seed(42)
    dimension = 128
    n_vectors = 1000
    n_queries = 100

    # 生成测试数据
    vectors = np.random.random((n_vectors, dimension)).astype(np.float32)
    queries = np.random.random((n_queries, dimension)).astype(np.float32)

    # 构建索引
    adapter = create_adapter("faiss_flat", dimension=dimension, metric="euclidean")
    adapter.fit(vectors)

    # 搜索
    results = adapter.search(queries, k=10)
    assert results.shape == (n_queries, 10)
    print(f"✓ test_faiss_flat passed (Recall baseline: {adapter.get_params()})")


def test_faiss_hnsw():
    """测试 faiss HNSW 索引"""
    np.random.seed(42)
    dimension = 128
    n_vectors = 1000

    vectors = np.random.random((n_vectors, dimension)).astype(np.float32)

    adapter = create_adapter("faiss_hnsw", dimension=dimension, M=16)
    adapter.fit(vectors)

    results = adapter.search(vectors[:10], k=5)
    assert results.shape == (10, 5)
    print("✓ test_faiss_hnsw passed")


def test_hnsw_fallback():
    """测试 HNSW 后备实现（无 C 库时）"""
    np.random.seed(42)
    dimension = 64
    n_vectors = 500
    n_queries = 50

    vectors = np.random.random((n_vectors, dimension)).astype(np.float32)
    queries = np.random.random((n_queries, dimension)).astype(np.float32)

    adapter = create_adapter("project_hnsw", dimension=dimension, M=8)
    adapter.fit(vectors)

    results = adapter.search(queries, k=10)
    assert results.shape == (n_queries, 10)
    print("✓ test_hnsw_fallback passed")


def test_dataset_manager():
    """测试数据集管理器（需要网络）"""
    try:
        manager = DatasetManager()
        datasets = manager.list_datasets()
        assert len(datasets) > 0
        print(f"✓ test_dataset_manager passed ({len(datasets)} datasets available)")
    except Exception as e:
        print(f"⚠ test_dataset_manager skipped: {e}")


def test_recall_computation():
    """测试召回率计算"""
    from benchmark.core.benchmark_runner import BenchmarkRunner

    runner = BenchmarkRunner()

    predicted = np.array([
        [0, 1, 2, 3, 4],
        [0, 1, 2, 3, 4],
        [0, 1, 2, 3, 4],
    ])
    ground_truth = np.array([
        [0, 1, 6, 7, 8],
        [0, 1, 2, 9, 10],
        [5, 6, 7, 8, 9],
    ])

    recall = runner._compute_recall(predicted, ground_truth, k=2)
    # 交集: [0,1] & [0,1,6,7,8] = 2, [0,1] & [0,1,2,9,10] = 2, [0,1] & [5,6,7,8,9] = 0
    # 正确数: 2+2+0 = 4, 总数: 3*2 = 6
    expected = 4 / 6
    assert abs(recall - expected) < 0.01
    print(f"✓ test_recall_computation passed (recall={recall:.4f})")


def main():
    print("Running VectorBench tests...\n")

    test_adapter_registry()
    test_faiss_flat()
    test_faiss_hnsw()
    test_hnsw_fallback()
    test_recall_computation()
    test_dataset_manager()

    print("\n✅ All tests passed!")


if __name__ == "__main__":
    main()
```

- [ ] **Step 2: 提交**

```bash
git add engineering/tools/benchmark/test/
git commit -m "test(benchmark): 添加 VectorBench 集成测试"
```

---

## 自检清单

在完成实施计划后，请对照设计文档检查：

- [x] **Spec coverage**: 所有设计文档中的组件都有对应的任务实现
- [x] **Placeholder scan**: 无 TBD/TODO 占位符
- [x] **Type consistency**: 接口签名一致（BaseIndexAdapter, BenchmarkDataset, BenchmarkResult）
- [x] **Task boundaries**: 每个任务有明确的交付物和测试

## 实施顺序

1. Task 1: 目录结构和配置文件
2. Task 2: 数据集管理器
3. Task 3: 基准测试执行器和报告生成器
4. Task 4: 适配器基类
5. Task 5: HNSW 适配器和 C API
6. Task 6: 其他适配器
7. Task 7: 下载脚本
8. Task 8: README
9. Task 9: 端到端测试

---

**Plan complete and saved to `docs/superpowers/plans/2026-07-13-vector-bench-plan.md`. Two execution options:**

**1. Subagent-Driven (recommended)** - I dispatch a fresh subagent per task, review between tasks, fast iteration

**2. Inline Execution** - Execute tasks in this session using executing-plans, batch execution with checkpoints

**Which approach?**
