# -*- coding: utf-8 -*-
"""
VectorBench 端侧推理模块
"""

from .inference_latency import (
    InferenceLatencyConfig,
    InferenceImpactResult,
    MockEmbeddingModel,
    InferenceLatencyAnalyzer,
)

from .real_embedding_model import (
    RealEmbeddingModel,
    RealModelConfig,
    create_embedding_model,
    REAL_MODEL_CONFIGS,
)

from .end_to_end_benchmark import (
    EndToEndBenchmark,
    EndToEndResult,
)

__all__ = [
    # 推理延迟分析
    "InferenceLatencyConfig",
    "InferenceImpactResult",
    "MockEmbeddingModel",
    "InferenceLatencyAnalyzer",
    # 真实 Embedding 模型
    "RealEmbeddingModel",
    "RealModelConfig",
    "create_embedding_model",
    "REAL_MODEL_CONFIGS",
    # 端到端基准测试
    "EndToEndBenchmark",
    "EndToEndResult",
]
