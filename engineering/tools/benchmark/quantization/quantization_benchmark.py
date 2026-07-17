#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
量化对比模块

评估不同量化方法（PQ/LVQ/SQ）在不同 bit 位下的精度-速度曲线
"""

import time
import numpy as np
from dataclasses import dataclass
from typing import List, Dict, Optional, Callable
from pathlib import Path

from ..core.dataset_manager import DatasetManager, BenchmarkDataset as Dataset
from ..core.benchmark_runner import BenchmarkResult


@dataclass
class QuantizationConfig:
    """量化配置"""
    method: str           # "pq", "lvq", "sq"
    bits: int            # 量化位数: 4, 6, 8, 12
    subquantizers: int   # 子空间数 (PQ/LVQ 专用)
    train_ratio: float = 0.1  # 训练数据比例


@dataclass
class QuantizationResult:
    """量化结果"""
    config: QuantizationConfig
    recall_at_1: float
    recall_at_10: float
    recall_at_100: float
    qps: float
    compression_ratio: float  # 压缩比
    memory_bytes: int
    build_time_seconds: float


class QuantizationBenchmark:
    """
    量化对比基准测试

    对比 PQ/LVQ/SQ 在不同 bit 位下的性能
    """

    def __init__(self, cache_dir: Optional[Path] = None):
        self.cache_dir = cache_dir or Path(__file__).parent.parent / "data"
        self.dataset_manager = DatasetManager(self.cache_dir)

        # 预定义的量化配置
        self.default_configs = [
            # PQ 配置
            QuantizationConfig("pq", bits=4, subquantizers=8),
            QuantizationConfig("pq", bits=6, subquantizers=8),
            QuantizationConfig("pq", bits=8, subquantizers=8),
            QuantizationConfig("pq", bits=12, subquantizers=16),
            # LVQ 配置
            QuantizationConfig("lvq", bits=4, subquantizers=1),  # LVQ 是标量量化
            QuantizationConfig("lvq", bits=6, subquantizers=1),
            QuantizationConfig("lvq", bits=8, subquantizers=1),
            # SQ 配置
            QuantizationConfig("sq", bits=4, subquantizers=1),
            QuantizationConfig("sq", bits=8, subquantizers=1),
        ]

    def _create_quantizer(self, config: QuantizationConfig, dimension: int):
        """
        创建量化器

        Args:
            config: 量化配置
            dimension: 向量维度

        Returns:
            量化器实例
        """
        if config.method == "pq":
            return PQQuantizer(dimension, config.subquantizers, config.bits)
        elif config.method == "lvq":
            return LVQQuantizer(dimension, config.bits)
        elif config.method == "sq":
            return SQQuantizer(dimension, config.bits)
        else:
            raise ValueError(f"Unknown quantization method: {config.method}")

    def evaluate_config(self, dataset: Dataset, config: QuantizationConfig,
                       k_values: List[int] = [1, 10, 100],
                       n_queries: int = 100) -> QuantizationResult:
        """
        评估单个量化配置

        Args:
            dataset: 数据集
            config: 量化配置
            k_values: 评估的 K 值列表
            n_queries: 查询数量

        Returns:
            量化结果
        """
        dimension = dataset.dimension
        n_vectors = len(dataset.train)

        # 训练量化器
        train_size = min(int(n_vectors * config.train_ratio), 100000)
        train_indices = np.random.choice(n_vectors, train_size, replace=False)
        train_data = dataset.train[train_indices].astype(np.float32)

        quantizer = self._create_quantizer(config, dimension)

        # 训练
        start_time = time.time()
        quantizer.fit(train_data)

        # 量化所有向量
        quantized_vectors = quantizer.encode(dataset.train.astype(np.float32))
        encode_time = time.time() - start_time

        # 计算压缩比
        original_size = n_vectors * dimension * 4  # float32
        compressed_size = quantized_vectors.nbytes
        compression_ratio = original_size / compressed_size

        # 搜索性能测试
        queries = dataset.test[:n_queries].astype(np.float32)

        # 计算 ground truth（使用原始向量）
        from ..core.benchmark_runner import BenchmarkRunner
        runner = BenchmarkRunner()
        ground_truth = runner._compute_knn_brute(
            dataset.train.astype(np.float32),
            queries,
            max(k_values)
        )

        # 搜索
        start_time = time.time()
        results = []
        for query in queries:
            q = quantizer.encode(query.reshape(1, -1))[0]
            distances = self._search_quantized(
                q, quantized_vectors, quantizer, k=100
            )
            results.append(distances)
        search_time = time.time() - start_time
        qps = n_queries / search_time

        # 计算召回率
        predicted = np.array([[r[0] for r in res] for res in results])

        recalls = {}
        for k in k_values:
            gt_k = ground_truth[:, :k]
            recalls[k] = runner._compute_recall(predicted[:, :k], gt_k, k)

        return QuantizationResult(
            config=config,
            recall_at_1=recalls.get(1, 0.0),
            recall_at_10=recalls.get(10, 0.0),
            recall_at_100=recalls.get(100, 0.0),
            qps=qps,
            compression_ratio=compression_ratio,
            memory_bytes=compressed_size,
            build_time_seconds=encode_time
        )

    def _search_quantized(self, query, codebook, quantized_vectors,
                         k: int = 100) -> List[tuple]:
        """在量化向量中搜索"""
        # 简化的搜索实现
        distances = np.linalg.norm(quantized_vectors - query, axis=1)
        top_k_idx = np.argpartition(distances, k)[:k]
        top_k_idx = top_k_idx[np.argsort(distances[top_k_idx])]
        return [(int(idx), float(distances[idx])) for idx in top_k_idx]

    def run_comparison(self, dataset_name: str = "fashion-mnist-784-euclidean",
                      configs: Optional[List[QuantizationConfig]] = None,
                      n_queries: int = 100) -> Dict[str, QuantizationResult]:
        """
        运行量化对比测试

        Args:
            dataset_name: 数据集名称
            configs: 量化配置列表（None 使用默认配置）
            n_queries: 查询数量

        Returns:
            方法名 -> 结果 的字典
        """
        if configs is None:
            configs = self.default_configs

        print(f"[QuantBenchmark] Loading dataset: {dataset_name}")
        dataset = self.dataset_manager.load(dataset_name)

        results = {}
        for config in configs:
            method_name = f"{config.method.upper()}-{config.bits}bit"
            if config.subquantizers > 1:
                method_name += f"-m{config.subquantizers}"

            print(f"[QuantBenchmark] Evaluating {method_name}...")
            try:
                result = self.evaluate_config(
                    dataset, config, n_queries=n_queries
                )
                results[method_name] = result
                print(f"  Recall@10: {result.recall_at_10:.4f}, "
                      f"QPS: {result.qps:.0f}, "
                      f"Compression: {result.compression_ratio:.1f}x")
            except Exception as e:
                print(f"  FAIL: {e}")

        return results

    def generate_report(self, results: Dict[str, QuantizationResult],
                       output_path: Optional[Path] = None) -> str:
        """
        生成对比报告

        Args:
            results: 量化结果字典
            output_path: 输出路径（可选）

        Returns:
            Markdown 格式报告
        """
        lines = [
            "# 量化对比报告\n",
            "## 评估结果\n",
            "| 方法 | Recall@1 | Recall@10 | QPS | 压缩比 | 内存 |",
            "|------|----------|-----------|-----|--------|------|",
        ]

        for name, result in sorted(results.items(),
                                   key=lambda x: -x[1].recall_at_10):
            lines.append(
                f"| {name} | {result.recall_at_1:.4f} | "
                f"{result.recall_at_10:.4f} | {result.qps:.0f} | "
                f"{result.compression_ratio:.1f}x | "
                f"{result.memory_bytes / 1024 / 1024:.1f} MB |"
            )

        # 添加分析
        lines.extend([
            "\n## 分析\n",
            "### 精度 vs 压缩比\n",
            "```\n",
        ])

        # 绘制 ASCII 图表
        for name, result in sorted(results.items(),
                                   key=lambda x: -x[1].compression_ratio):
            bar_len = int(result.recall_at_10 * 40)
            bar = "█" * bar_len + "░" * (40 - bar_len)
            lines.append(f"{name:20s} {bar} {result.recall_at_10:.3f}")

        lines.extend([
            "```\n",
            "### 速度 vs 精度\n",
            "```\n",
        ])

        for name, result in sorted(results.items(),
                                   key=lambda x: -x[1].qps):
            bar_len = int(result.recall_at_10 * 40)
            bar = "█" * bar_len + "░" * (40 - bar_len)
            lines.append(f"{name:20s} {bar} {result.qps:.0f} QPS")

        lines.append("```")

        report = "\n".join(lines)

        if output_path:
            output_path.write_text(report)
            print(f"[QuantBenchmark] Report saved to {output_path}")

        return report


# ============================================================================
# 量化器实现
# ============================================================================

class BaseQuantizer:
    """量化器基类"""

    def fit(self, vectors: np.ndarray) -> None:
        raise NotImplementedError

    def encode(self, vectors: np.ndarray) -> np.ndarray:
        raise NotImplementedError

    def decode(self, codes: np.ndarray) -> np.ndarray:
        raise NotImplementedError


class PQQuantizer(BaseQuantizer):
    """Product Quantization (乘积量化)"""

    def __init__(self, dimension: int, m: int, bits: int):
        self.dimension = dimension
        self.m = m  # 子空间数
        self.bits = bits  # 每子空间位数
        self.code_size = m * bits // 8  # 字节数
        self.sub_dim = dimension // m
        self.n_centroids = 2 ** bits
        self.codebook = None

    def fit(self, vectors: np.ndarray) -> None:
        """训练 PQ 码本"""
        n, dim = vectors.shape
        assert dim % self.m == 0, "Dimension must be divisible by m"

        # 分割向量
        sub_vectors = np.split(vectors, self.m, axis=1)

        # 每个子空间独立聚类
        self.codebook = []
        for i, sv in enumerate(sub_vectors):
            centroids = self._kmeans(sv, self.n_centroids)
            self.codebook.append(centroids)

    def _kmeans(self, vectors: np.ndarray, k: int,
                max_iter: int = 20) -> np.ndarray:
        """简单的 K-Means 实现"""
        n = len(vectors)

        # 初始化中心点（随机采样）
        indices = np.random.choice(n, k, replace=False)
        centroids = vectors[indices].copy()

        for _ in range(max_iter):
            # 分配到最近中心
            dists = np.linalg.norm(
                vectors[:, np.newaxis, :] - centroids[np.newaxis, :, :],
                axis=2
            )
            labels = np.argmin(dists, axis=1)

            # 更新中心点
            new_centroids = np.zeros_like(centroids)
            for j in range(k):
                mask = labels == j
                if mask.sum() > 0:
                    new_centroids[j] = vectors[mask].mean(axis=0)
                else:
                    new_centroids[j] = centroids[j]

            # 检查收敛
            if np.allclose(centroids, new_centroids):
                break
            centroids = new_centroids

        return centroids

    def encode(self, vectors: np.ndarray) -> np.ndarray:
        """编码向量"""
        n, dim = vectors.shape
        codes = np.zeros((n, self.m), dtype=np.int32)

        sub_vectors = np.split(vectors, self.m, axis=1)
        for i, sv in enumerate(sub_vectors):
            dists = np.linalg.norm(
                sv[:, np.newaxis, :] - self.codebook[i][np.newaxis, :, :],
                axis=2
            )
            codes[:, i] = np.argmin(dists, axis=1)

        return codes

    def decode(self, codes: np.ndarray) -> np.ndarray:
        """解码向量"""
        n = len(codes)
        vectors = np.zeros((n, self.dimension), dtype=np.float32)

        for i in range(self.m):
            start = i * self.sub_dim
            end = start + self.sub_dim
            vectors[:, start:end] = self.codebook[i][codes[:, i]]

        return vectors


class LVQQuantizer(BaseQuantizer):
    """Learning Vector Quantization (学习向量量化)"""

    def __init__(self, dimension: int, bits: int):
        self.dimension = dimension
        self.bits = bits
        self.codebook = None

    def fit(self, vectors: np.ndarray) -> None:
        """训练 LVQ 码本（简单实现：均匀量化）"""
        n_centroids = 2 ** self.bits

        # 简单实现：使用分位数作为码本
        self.codebook = np.percentile(
            vectors, np.linspace(0, 100, n_centroids), axis=0
        )

    def encode(self, vectors: np.ndarray) -> np.ndarray:
        """编码向量"""
        n = len(vectors)
        codes = np.zeros(n, dtype=np.int32)

        for i, v in enumerate(vectors):
            dists = np.linalg.norm(self.codebook - v, axis=1)
            codes[i] = np.argmin(dists)

        return codes

    def decode(self, codes: np.ndarray) -> np.ndarray:
        """解码向量"""
        return self.codebook[codes]


class SQQuantizer(BaseQuantizer):
    """Scalar Quantization (标量量化)"""

    def __init__(self, dimension: int, bits: int):
        self.dimension = dimension
        self.bits = bits
        self.quantiles = None
        self.n_levels = 2 ** bits

    def fit(self, vectors: np.ndarray) -> None:
        """训练 SQ（按维度均匀量化）"""
        # 使用分位数作为量化边界
        self.quantiles = np.percentile(
            vectors, np.linspace(0, 100, self.n_levels + 1), axis=0
        )

    def encode(self, vectors: np.ndarray) -> np.ndarray:
        """编码向量"""
        codes = np.zeros_like(vectors, dtype=np.int32)

        for level in range(self.n_levels):
            mask = (
                (vectors >= self.quantiles[level]) &
                (vectors < self.quantiles[level + 1])
            )
            codes[mask] = level

        # 处理边界值
        codes[vectors >= self.quantiles[-1]] = self.n_levels - 1

        return codes

    def decode(self, codes: np.ndarray) -> np.ndarray:
        """解码向量"""
        # 返回每个级别的中心值
        decoded = np.zeros_like(codes, dtype=np.float32)
        for level in range(self.n_levels):
            mask = codes == level
            center = (self.quantiles[level] + self.quantiles[level + 1]) / 2
            decoded[mask] = center[mask]
        return decoded
