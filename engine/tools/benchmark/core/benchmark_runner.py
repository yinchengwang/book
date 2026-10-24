#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
基准测试执行器
"""

import sys
import time
from dataclasses import dataclass, asdict, field
from pathlib import Path
from typing import Optional, Dict, Any, List
import csv

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
    recall_at_1: float = 0.0
    recall_at_10: float = 0.0
    recall_at_100: float = 0.0
    qps: float = 0.0
    build_time: float = 0.0
    memory_bytes: int = 0
    params: Dict[str, Any] = field(default_factory=dict)

    @property
    def memory_mb(self) -> float:
        """内存占用 (MB)"""
        return self.memory_bytes / (1024 * 1024)

    def to_dict(self) -> Dict[str, Any]:
        """转换为字典"""
        d = asdict(self)
        d["memory_mb"] = self.memory_mb
        return d

    def save(self, path: str) -> None:
        """保存结果到 CSV"""
        Path(path).parent.mkdir(parents=True, exist_ok=True)

        write_header = not Path(path).exists()

        with open(path, 'a', newline='', encoding='utf-8') as f:
            fieldnames = list(self.to_dict().keys())
            writer = csv.DictWriter(f, fieldnames=fieldnames)
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
            try:
                with open(config_path, 'r', encoding='utf-8') as f:
                    return yaml.safe_load(f)
            except Exception as e:
                print(f"[Warning] Failed to load config: {e}", file=sys.stderr)
        return {"algorithms": {}}

    def _compute_recall(self, predicted: np.ndarray, ground_truth: np.ndarray, k: int) -> float:
        """
        计算 Top-K 召回率

        Args:
            predicted: 预测结果 (n_queries, k)
            ground_truth: 真实近邻 (n_queries, k)
            k: Top-K

        Returns:
            召回率
        """
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
        print(f"\n[Runner] Running benchmark:")
        print(f"  Algorithm: {algorithm}")
        print(f"  Dataset: {dataset.name}")
        print(f"  Dimension: {dataset.dimension}")
        print(f"  Train size: {dataset.train_size}")
        print(f"  Test size: {dataset.test_size}")

        # 创建适配器
        adapter = self._create_adapter(algorithm, params)

        # 构建索引
        print(f"\n[Runner] Building index...")
        build_start = time.perf_counter()
        adapter.fit(dataset.train)
        build_time = time.perf_counter() - build_start
        print(f"  Build time: {build_time:.2f}s")

        # 获取内存占用
        memory_bytes = adapter.get_size()
        print(f"  Memory: {memory_bytes / (1024*1024):.2f} MB")

        # 执行查询
        print(f"\n[Runner] Running queries...")
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
