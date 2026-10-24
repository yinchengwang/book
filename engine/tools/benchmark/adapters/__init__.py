# -*- coding: utf-8 -*-
"""
VectorBench 索引适配器模块
"""

from .base import BaseIndexAdapter
from .hnsw_adapter import HNSWAdapter
from .ivf_pq_adapter import IVFPQAdapter
from .lsh_adapter import LSHAdapter
from .diskann_adapter import DiskANNAdapter

# 适配器注册表
ADAPTER_REGISTRY = {
    # 直接类名
    "HNSWAdapter": HNSWAdapter,
    "IVFPQAdapter": IVFPQAdapter,
    "LSHAdapter": LSHAdapter,
    "DiskANNAdapter": DiskANNAdapter,
    # 项目实现别名
    "project_hnsw": HNSWAdapter,
    "project_ivf_pq": IVFPQAdapter,
    "project_lsh": LSHAdapter,
    "project_diskann": DiskANNAdapter,
}


def create_adapter(algorithm: str, params: dict = None) -> BaseIndexAdapter:
    """
    创建适配器工厂函数

    Args:
        algorithm: 算法名称或适配器类名
        params: 参数字典

    Returns:
        BaseIndexAdapter 实例

    Raises:
        ValueError: 未知算法
    """
    params = params or {}

    # 直接是类名
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

    raise ValueError(
        f"Unknown algorithm: {algorithm}. "
        f"Available: {list(ADAPTER_REGISTRY.keys())}"
    )


__all__ = [
    "BaseIndexAdapter",
    "HNSWAdapter",
    "IVFPQAdapter",
    "LSHAdapter",
    "FaissAdapter",
    "create_adapter",
    "ADAPTER_REGISTRY",
]
