#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
索引适配器基类
"""

from abc import ABC, abstractmethod
from typing import Dict, Any
import numpy as np


class BaseIndexAdapter(ABC):
    """
    索引适配器抽象基类

    所有索引适配器必须实现以下接口:
    - fit(): 构建索引
    - add(): 增量添加向量 (可选)
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

        Raises:
            NotImplementedError: 如果索引不支持增量添加
        """
        raise NotImplementedError(
            f"{self.__class__.__name__} does not support incremental add"
        )

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
        return (
            f"{self.__class__.__name__}"
            f"(dimension={self.dimension}, metric={self.metric})"
        )
