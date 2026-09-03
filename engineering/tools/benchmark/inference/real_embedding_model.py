#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
真实 Embedding 模型实现

支持 sentence-transformers 模型，兼容 MockEmbeddingModel 接口
"""

import time
import numpy as np
from dataclasses import dataclass
from typing import List, Optional, Dict, Any
from pathlib import Path

from .inference_latency import InferenceLatencyConfig, MockEmbeddingModel


@dataclass
class RealModelConfig:
    """真实模型配置"""
    model_name: str
    model_id: str           # HuggingFace model ID
    dimension: int
    max_seq_length: int
    normalize: bool = True


# 预定义模型配置
REAL_MODEL_CONFIGS = {
    "all-MiniLM-L6-v2": RealModelConfig(
        model_name="all-MiniLM-L6-v2",
        model_id="sentence-transformers/all-MiniLM-L6-v2",
        dimension=384,
        max_seq_length=256,
        normalize=True,
    ),
    "bge-small-en": RealModelConfig(
        model_name="bge-small-en",
        model_id="BAAI/bge-small-en",
        dimension=384,
        max_seq_length=512,
        normalize=True,
    ),
    "e5-small": RealModelConfig(
        model_name="e5-small",
        model_id="intfloat/e5-small",
        dimension=384,
        max_seq_length=512,
        normalize=True,
    ),
}


class RealEmbeddingModel:
    """
    真实 Embedding 模型

    使用 sentence-transformers 进行文本编码
    与 MockEmbeddingModel 接口兼容
    """

    def __init__(self, model_name: str = "all-MiniLM-L6-v2",
                 cache_dir: Optional[str] = None):
        self.model_name = model_name
        self.cache_dir = cache_dir

        # 获取模型配置
        config = REAL_MODEL_CONFIGS.get(model_name)
        if config is None:
            raise ValueError(f"Unknown model: {model_name}")

        self.config = config
        self.dimension = config.dimension
        self._model = None
        self._loaded = False

        # 延迟统计（首次 encode 时填充）
        self._latency_stats: Optional[Dict[str, float]] = None

    def _load_model(self):
        """延迟加载模型"""
        if self._loaded:
            return True

        try:
            from sentence_transformers import SentenceTransformer
            self._model = SentenceTransformer(
                self.config.model_id,
                cache_folder=self.cache_dir
            )
            self._loaded = True
            return True
        except ImportError:
            print("[RealEmbeddingModel] sentence-transformers not installed, "
                  "falling back to mock model")
            return False
        except Exception as e:
            print(f"[RealEmbeddingModel] Failed to load model: {e}")
            return False

    def encode(self, texts: List[str], batch_size: int = 32,
               show_progress: bool = False) -> np.ndarray:
        """
        编码文本为向量

        Args:
            texts: 文本列表
            batch_size: 批处理大小
            show_progress: 是否显示进度条

        Returns:
            向量数组 (n_texts, dimension)
        """
        if not self._load_model():
            # 回退到 Mock 模型
            mock = MockEmbeddingModel(self.model_name)
            return mock.encode(texts, batch_size)

        # 记录时间用于延迟统计
        start_time = time.perf_counter()

        embeddings = self._model.encode(
            texts,
            batch_size=batch_size,
            show_progress_bar=show_progress,
            normalize_embeddings=self.config.normalize,
            convert_to_numpy=True,
        )

        elapsed = time.perf_counter() - start_time

        # 更新延迟统计
        self._update_latency_stats(len(texts), elapsed)

        return embeddings.astype(np.float32)

    def _update_latency_stats(self, n_texts: int, total_time: float):
        """更新延迟统计"""
        avg_ms = (total_time / n_texts) * 1000

        if self._latency_stats is None:
            self._latency_stats = {
                "total_queries": n_texts,
                "total_time_ms": total_time * 1000,
                "avg_ms": avg_ms,
                "min_ms": avg_ms,
                "max_ms": avg_ms,
                "samples": [avg_ms],
            }
        else:
            stats = self._latency_stats
            stats["total_queries"] += n_texts
            stats["total_time_ms"] += total_time * 1000
            stats["samples"].append(avg_ms)
            stats["avg_ms"] = stats["total_time_ms"] / stats["total_queries"]
            stats["min_ms"] = min(stats["min_ms"], avg_ms)
            stats["max_ms"] = max(stats["max_ms"], avg_ms)

    def get_latency_config(self) -> InferenceLatencyConfig:
        """获取延迟配置"""
        if self._latency_stats is None:
            # 未运行过，返回估计值
            return InferenceLatencyConfig(
                model_name=self.model_name,
                batch_size=1,
                avg_latency_ms=8.0,  # 估计值
                p50_latency_ms=7.5,
                p95_latency_ms=12.0,
                p99_latency_ms=15.0,
                throughput_qps=125,
            )

        stats = self._latency_stats
        samples = np.array(stats["samples"])

        return InferenceLatencyConfig(
            model_name=self.model_name,
            batch_size=1,
            avg_latency_ms=stats["avg_ms"],
            p50_latency_ms=float(np.percentile(samples, 50)),
            p95_latency_ms=float(np.percentile(samples, 95)),
            p99_latency_ms=float(np.percentile(samples, 99)),
            throughput_qps=1000.0 / stats["avg_ms"],
        )

    def is_loaded(self) -> bool:
        """模型是否已加载"""
        return self._loaded

    @classmethod
    def list_available_models(cls) -> List[str]:
        """列出可用模型"""
        return list(REAL_MODEL_CONFIGS.keys())


def create_embedding_model(model_name: str = "mock:mini-lm-6",
                          cache_dir: Optional[str] = None):
    """
    创建 Embedding 模型工厂函数

    Args:
        model_name: 模型名称，格式:
            - "mock:<name>" 使用 Mock 模型
            - "<real_name>" 使用真实模型
            - "auto" 自动选择（优先真实模型）
        cache_dir: 模型缓存目录

    Returns:
        MockEmbeddingModel 或 RealEmbeddingModel 实例
    """
    if model_name.startswith("mock:"):
        name = model_name.split(":", 1)[1]
        return MockEmbeddingModel(name)

    if model_name == "auto":
        # 尝试真实模型，失败则回退
        try:
            model = RealEmbeddingModel("all-MiniLM-L6-v2", cache_dir)
            if model._load_model():
                return model
        except:
            pass
        return MockEmbeddingModel("mini-lm-6")

    # 指定真实模型
    return RealEmbeddingModel(model_name, cache_dir)