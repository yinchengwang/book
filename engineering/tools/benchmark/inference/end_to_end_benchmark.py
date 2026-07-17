#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
端到端基准测试模块

评估 文本 → Embedding → 向量索引搜索 的完整链路性能
"""

import time
import numpy as np
from dataclasses import dataclass
from typing import List, Dict, Optional, Tuple
from pathlib import Path

from .real_embedding_model import RealEmbeddingModel, create_embedding_model
from .inference_latency import MockEmbeddingModel, InferenceLatencyConfig


@dataclass
class EndToEndResult:
    """端到端测试结果"""
    dataset_name: str
    model_name: str
    algorithm: str

    # 索引性能
    build_time_seconds: float
    index_memory_mb: float

    # 搜索性能
    total_queries: int
    inference_time_seconds: float
    search_time_seconds: float
    total_time_seconds: float

    # 吞吐量
    inference_qps: float
    search_qps: float
    effective_qps: float  # 端到端有效 QPS

    # 延迟
    avg_latency_ms: float
    inference_latency_ms: float
    search_latency_ms: float

    # 召回率
    recall_at_1: float
    recall_at_10: float
    recall_at_100: float

    # 瓶颈分析
    inference_overhead_percent: float


class EndToEndBenchmark:
    """
    端到端基准测试

    评估从文本到搜索结果的完整链路
    """

    def __init__(self, cache_dir: Optional[Path] = None):
        self.cache_dir = cache_dir

    def run(self,
            corpus_texts: List[str],
            query_texts: List[str],
            ground_truth: Optional[np.ndarray] = None,
            embedding_model: str = "auto",
            algorithm: str = "project_hnsw",
            algorithm_params: Optional[Dict] = None,
            k: int = 10) -> EndToEndResult:
        """
        运行端到端测试

        Args:
            corpus_texts: 语料库文本列表
            query_texts: 查询文本列表
            ground_truth: 真实近邻 ID (n_queries, k)
            embedding_model: Embedding 模型名称
            algorithm: 索引算法名称
            algorithm_params: 算法参数
            k: Top-K

        Returns:
            EndToEndResult
        """
        # 1. 创建 Embedding 模型
        model = create_embedding_model(embedding_model, self.cache_dir)
        model_name = getattr(model, 'model_name', 'unknown')

        print(f"\n[EndToEndBenchmark] Model: {model_name}")
        print(f"  Corpus size: {len(corpus_texts)}")
        print(f"  Query count: {len(query_texts)}")

        # 2. 语料库向量化
        print(f"\n[Step 1] Encoding corpus...")
        start_time = time.perf_counter()
        corpus_vectors = model.encode(corpus_texts, batch_size=64, show_progress=True)
        corpus_time = time.perf_counter() - start_time
        print(f"  Corpus encoding time: {corpus_time:.2f}s")

        # 3. 构建索引
        print(f"\n[Step 2] Building index...")
        from benchmark.adapters import create_adapter

        params = algorithm_params or {}
        params["dimension"] = corpus_vectors.shape[1]

        adapter = create_adapter(algorithm, params)

        build_start = time.perf_counter()
        adapter.fit(corpus_vectors)
        build_time = time.perf_counter() - build_start
        index_memory = adapter.get_size()
        print(f"  Build time: {build_time:.2f}s")
        print(f"  Index memory: {index_memory / (1024*1024):.2f} MB")

        # 4. 查询向量化 + 搜索
        print(f"\n[Step 3] Running queries...")
        inference_time = 0.0
        search_time = 0.0
        all_results = []

        batch_size = 100
        for i in range(0, len(query_texts), batch_size):
            batch = query_texts[i:i+batch_size]

            # 推理
            inf_start = time.perf_counter()
            query_vectors = model.encode(batch, batch_size=32)
            inference_time += time.perf_counter() - inf_start

            # 搜索
            search_start = time.perf_counter()
            results = adapter.search(query_vectors, k=k)
            search_time += time.perf_counter() - search_start

            all_results.append(results)

        all_results = np.vstack(all_results)
        total_time = inference_time + search_time

        # 5. 计算指标
        n_queries = len(query_texts)

        inference_qps = n_queries / inference_time if inference_time > 0 else 0
        search_qps = n_queries / search_time if search_time > 0 else 0
        effective_qps = n_queries / total_time if total_time > 0 else 0

        avg_latency_ms = (total_time / n_queries) * 1000
        inference_latency_ms = (inference_time / n_queries) * 1000
        search_latency_ms = (search_time / n_queries) * 1000

        inference_overhead = (inference_time / total_time) * 100

        # 计算召回率（如果有 ground truth）
        recall_at_1 = recall_at_10 = recall_at_100 = 0.0
        if ground_truth is not None:
            from benchmark.core.benchmark_runner import BenchmarkRunner
            runner = BenchmarkRunner()
            recall_at_1 = runner._compute_recall(all_results, ground_truth, 1)
            recall_at_10 = runner._compute_recall(all_results, ground_truth, 10)
            recall_at_100 = runner._compute_recall(all_results, ground_truth, 100)

        return EndToEndResult(
            dataset_name="custom",
            model_name=model_name,
            algorithm=algorithm,
            build_time_seconds=build_time,
            index_memory_mb=index_memory / (1024 * 1024),
            total_queries=n_queries,
            inference_time_seconds=inference_time,
            search_time_seconds=search_time,
            total_time_seconds=total_time,
            inference_qps=inference_qps,
            search_qps=search_qps,
            effective_qps=effective_qps,
            avg_latency_ms=avg_latency_ms,
            inference_latency_ms=inference_latency_ms,
            search_latency_ms=search_latency_ms,
            recall_at_1=recall_at_1,
            recall_at_10=recall_at_10,
            recall_at_100=recall_at_100,
            inference_overhead_percent=inference_overhead,
        )

    def generate_report(self, results: List[EndToEndResult],
                       output_path: Optional[Path] = None) -> str:
        """生成报告"""
        lines = [
            "# 端到端基准测试报告\n",
            "## 测试结果\n",
            "| 模型 | 算法 | 有效 QPS | 推理延迟 | 搜索延迟 | "
            "推理占比 | Recall@10 |",
            "|------|------|----------|----------|----------|"
            "----------|-----------|",
        ]

        for r in sorted(results, key=lambda x: -x.effective_qps):
            lines.append(
                f"| {r.model_name} | {r.algorithm} | "
                f"{r.effective_qps:.0f} | {r.inference_latency_ms:.1f}ms | "
                f"{r.search_latency_ms:.2f}ms | {r.inference_overhead_percent:.1f}% | "
                f"{r.recall_at_10:.4f} |"
            )

        lines.extend([
            "\n## 瓶颈分析\n",
            "```\n",
        ])

        for r in results:
            bar_len = int(r.inference_overhead_percent / 2)
            bar = "█" * bar_len + "░" * (50 - bar_len)
            lines.append(
                f"{r.model_name:20s} {bar} "
                f"Inference: {r.inference_overhead_percent:.1f}%"
            )

        lines.append("```")

        report = "\n".join(lines)

        if output_path:
            output_path.write_text(report)

        return report