#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
端侧推理集成模块

评估 Embedding 推理延迟对搜索 QPS 的影响
"""

import time
import numpy as np
from dataclasses import dataclass
from typing import List, Dict, Optional, Callable
from pathlib import Path
import statistics

from ..core.dataset_manager import DatasetManager, BenchmarkDataset as Dataset
from ..core.benchmark_runner import BenchmarkRunner


@dataclass
class InferenceLatencyConfig:
    """推理延迟配置"""
    model_name: str
    batch_size: int
    avg_latency_ms: float     # 平均延迟（毫秒）
    p50_latency_ms: float     # P50 延迟
    p95_latency_ms: float     # P95 延迟
    p99_latency_ms: float     # P99 延迟
    throughput_qps: float     # 吞吐量 (向量/秒)


@dataclass
class InferenceImpactResult:
    """推理影响结果"""
    config: InferenceLatencyConfig
    pure_search_qps: float     # 纯搜索 QPS（不含推理）
    effective_qps: float       # 有效 QPS（含推理）
    latency_overhead_ms: float # 每查询延迟开销
    bottleneck_ratio: float    # 推理占总时间比例


class MockEmbeddingModel:
    """
    模拟 Embedding 模型

    用于评估不同模型的推理延迟特性
    """

    # 预定义模型配置
    MODELS = {
        "mini-lm-6": {
            "dim": 384,
            "avg_ms": 8.0,
            "p50_ms": 7.5,
            "p95_ms": 12.0,
            "p99_ms": 15.0,
            "throughput": 125,
        },
        "mini-lm-12": {
            "dim": 384,
            "avg_ms": 15.0,
            "p50_ms": 14.0,
            "p95_ms": 22.0,
            "p99_ms": 28.0,
            "throughput": 67,
        },
        "bge-small": {
            "dim": 512,
            "avg_ms": 5.0,
            "p50_ms": 4.8,
            "p95_ms": 7.5,
            "p99_ms": 9.0,
            "throughput": 200,
        },
        "bge-base": {
            "dim": 768,
            "avg_ms": 18.0,
            "p50_ms": 17.0,
            "p95_ms": 26.0,
            "p99_ms": 32.0,
            "throughput": 55,
        },
        "e5-small": {
            "dim": 384,
            "avg_ms": 4.5,
            "p50_ms": 4.2,
            "p95_ms": 6.8,
            "p99_ms": 8.5,
            "throughput": 222,
        },
        "e5-base": {
            "dim": 768,
            "avg_ms": 16.0,
            "p50_ms": 15.0,
            "p95_ms": 24.0,
            "p99_ms": 30.0,
            "throughput": 62,
        },
    }

    def __init__(self, model_name: str = "mini-lm-6"):
        self.model_name = model_name
        config = self.MODELS.get(model_name, self.MODELS["mini-lm-6"])

        self.dimension = config["dim"]
        self.avg_latency_ms = config["avg_ms"]
        self.p50_latency_ms = config["p50_ms"]
        self.p95_latency_ms = config["p95_ms"]
        self.p99_latency_ms = config["p99_ms"]
        self.throughput_qps = config["throughput"]

    def get_latency_config(self) -> InferenceLatencyConfig:
        """获取延迟配置"""
        return InferenceLatencyConfig(
            model_name=self.model_name,
            batch_size=1,
            avg_latency_ms=self.avg_latency_ms,
            p50_latency_ms=self.p50_latency_ms,
            p95_latency_ms=self.p95_latency_ms,
            p99_latency_ms=self.p99_latency_ms,
            throughput_qps=self.throughput_qps,
        )

    def encode(self, texts: List[str], batch_size: int = 1,
               show_progress: bool = False) -> np.ndarray:
        """
        模拟编码文本为向量

        Args:
            texts: 文本列表
            batch_size: 批处理大小
            show_progress: 是否显示进度条（忽略）

        Returns:
            向量数组 (n_texts, dimension)
        """
        n = len(texts)
        vectors = np.random.randn(n, self.dimension).astype(np.float32)

        # 归一化
        vectors = vectors / np.linalg.norm(vectors, axis=1, keepdims=True)

        return vectors


class InferenceLatencyAnalyzer:
    """
    推理延迟影响分析器

    分析 embedding 推理延迟对端到端搜索性能的影响
    """

    def __init__(self, cache_dir: Optional[Path] = None):
        self.cache_dir = cache_dir or Path(__file__).parent.parent / "data"
        self.dataset_manager = DatasetManager(self.cache_dir)

    def _simulate_inference_latency(self, model: MockEmbeddingModel,
                                    n_queries: int) -> float:
        """
        模拟推理延迟

        使用正态分布模拟真实延迟
        """
        # 基于 P95 计算标准差
        std_ms = (model.p95_latency_ms - model.avg_latency_ms) / 1.96

        latencies = np.random.normal(model.avg_latency_ms, std_ms, n_queries)
        latencies = np.clip(latencies, 0.5, model.p99_latency_ms * 1.5)

        total_latency = latencies.sum()
        return total_latency / 1000.0  # 转换为秒

    def evaluate_inference_impact(self, dataset: Dataset,
                                 model: MockEmbeddingModel,
                                 index_search_qps: float,
                                 n_queries: int = 1000) -> InferenceImpactResult:
        """
        评估推理对整体 QPS 的影响

        Args:
            dataset: 数据集
            model: embedding 模型
            index_search_qps: 索引搜索 QPS（不含推理）
            n_queries: 查询数量

        Returns:
            推理影响结果
        """
        # 模拟查询文本
        query_texts = [f"query_{i}" for i in range(n_queries)]

        # 测量纯搜索时间
        search_time = n_queries / index_search_qps

        # 测量推理时间
        inference_time = self._simulate_inference_latency(model, n_queries)

        # 总时间
        total_time = search_time + inference_time

        # 有效 QPS
        effective_qps = n_queries / total_time

        # 延迟开销（每查询）
        latency_overhead_ms = (inference_time / n_queries) * 1000

        # 瓶颈比例
        bottleneck_ratio = inference_time / total_time

        return InferenceImpactResult(
            config=model.get_latency_config(),
            pure_search_qps=index_search_qps,
            effective_qps=effective_qps,
            latency_overhead_ms=latency_overhead_ms,
            bottleneck_ratio=bottleneck_ratio,
        )

    def run_model_comparison(self, dataset_name: str = "fashion-mnist-784-euclidean",
                            index_search_qps: float = 10000,
                            n_queries: int = 1000) -> Dict[str, InferenceImpactResult]:
        """
        运行多模型对比

        Args:
            dataset_name: 数据集名称
            index_search_qps: 索引搜索 QPS
            n_queries: 查询数量

        Returns:
            模型名 -> 结果 的字典
        """
        print(f"[InferenceAnalyzer] Comparing embedding models...")
        print(f"  Index search QPS: {index_search_qps:.0f}")
        print(f"  Query count: {n_queries}")
        print()

        results = {}
        for model_name in MockEmbeddingModel.MODELS:
            model = MockEmbeddingModel(model_name)
            result = self.evaluate_inference_impact(
                dataset=None,  # 不需要真实数据集
                model=model,
                index_search_qps=index_search_qps,
                n_queries=n_queries
            )
            results[model_name] = result

            print(f"  {model_name:12s}: "
                  f"Effective QPS={result.effective_qps:6.0f}, "
                  f"Latency={result.latency_overhead_ms:5.1f}ms, "
                  f"Overhead={result.bottleneck_ratio*100:4.1f}%")

        return results

    def generate_report(self, results: Dict[str, InferenceImpactResult],
                       output_path: Optional[Path] = None) -> str:
        """
        生成分析报告

        Args:
            results: 分析结果
            output_path: 输出路径

        Returns:
            Markdown 报告
        """
        lines = [
            "# 端侧推理影响分析报告\n",
            "## 模型对比\n",
            "| 模型 | 推理延迟 | P95 | 有效 QPS | 延迟开销 | 瓶颈比例 |",
            "|------|----------|-----|----------|----------|----------|",
        ]

        # 按有效 QPS 排序
        sorted_results = sorted(
            results.items(),
            key=lambda x: -x[1].effective_qps
        )

        for name, result in sorted_results:
            lines.append(
                f"| {name} | "
                f"{result.config.avg_latency_ms:.1f}ms | "
                f"{result.config.p95_latency_ms:.1f}ms | "
                f"{result.effective_qps:.0f} | "
                f"{result.latency_overhead_ms:.1f}ms | "
                f"{result.bottleneck_ratio*100:.1f}% |"
            )

        # 分析和建议
        lines.extend([
            "\n## 分析\n",
            "### 瓶颈分析\n",
        ])

        # 找出最大瓶颈
        max_bottleneck = max(results.items(), key=lambda x: x[1].bottleneck_ratio)
        lines.append(f"**最大瓶颈**: {max_bottleneck[0]} ({max_bottleneck[1].bottleneck_ratio*100:.1f}%)")

        lines.extend([
            "\n### 优化建议\n",
            "1. **选择轻量模型**: 在精度可接受范围内，优先选择延迟低的模型\n",
            "2. **批量推理**: 多查询合并推理，减少单查询开销\n",
            "3. **缓存向量**: 热门查询的向量可缓存，复用推理结果\n",
            "4. **异步流水线**: 推理与搜索并行执行，隐藏延迟\n",
        ])

        # QPS 曲线图
        lines.extend([
            "\n### 有效 QPS vs 索引搜索 QPS\n",
            "```\n",
            f"{'Index QPS':>12s} | {'Effective QPS by Model':50s}",
            "-" * 12 + "+" + "-" * 50,
        ])

        # 简化的 ASCII 图表
        index_qps_values = [1000, 5000, 10000, 50000, 100000]
        model_names = list(results.keys())

        for iqp in index_qps_values:
            bar_parts = []
            for name in model_names:
                # 估算有效 QPS（简化模型）
                model = results[name].config
                inference_time = model.avg_latency_ms / 1000
                search_time = 1.0 / iqp
                effective = 1.0 / (search_time + inference_time)
                bar_parts.append(f"{effective:6.0f}")

            lines.append(f"{iqp:>12,d} | " + " | ".join(bar_parts))

        lines.append("```")

        report = "\n".join(lines)

        if output_path:
            output_path.write_text(report)
            print(f"\n[InferenceAnalyzer] Report saved to {output_path}")

        return report


def run_inference_benchmark():
    """运行端侧推理基准测试"""
    analyzer = InferenceLatencyAnalyzer()

    # 对比不同模型
    results = analyzer.run_model_comparison(
        index_search_qps=10000,
        n_queries=1000
    )

    # 生成报告
    report = analyzer.generate_report(
        results,
        output_path=Path(__file__).parent.parent / "results" / "inference_impact.md"
    )

    print("\n" + report)


if __name__ == "__main__":
    run_inference_benchmark()
