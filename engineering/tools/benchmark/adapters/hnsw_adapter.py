#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
HNSW 索引适配器 - C 实现

优先使用 ctypes 调用项目 C 实现，回退到纯 Python 实现
"""

import numpy as np
from typing import Optional

from .base import BaseIndexAdapter
from ..c_api.ctypes_bindings import get_lib, CAPIError


class HNSWAdapter(BaseIndexAdapter):
    """
    HNSW 索引适配器

    优先使用项目 C 实现，ctypes 加载失败时回退到纯 Python 实现
    """

    def __init__(self, dimension: int = 384, metric: str = "euclidean",
                 M: int = 16, ef_construction: int = 200, ef_search: int = 100,
                 **kwargs):
        super().__init__(dimension, metric, **kwargs)
        self.M = M
        self.ef_construction = ef_construction
        self.ef_search = ef_search

        # C API 状态
        self._c_index: Optional[int] = None
        self._lib = None
        self._use_c = False

        # Python 后备实现
        self._python_vectors: Optional[np.ndarray] = None

        # 尝试加载 C 库
        self._try_load_c_library()

    def _try_load_c_library(self) -> bool:
        """尝试加载 C 库"""
        self._lib = get_lib()
        if self._lib is None:
            print("  INFO: C library not found, using Python fallback")
            return False
        return True

    def fit(self, vectors: np.ndarray) -> None:
        """构建索引"""
        n, dim = vectors.shape
        if dim != self.dimension:
            raise ValueError(f"Dimension mismatch: {dim} vs {self.dimension}")

        vectors = vectors.astype(np.float32)

        if self._lib is not None:
            # 使用 C 实现
            try:
                metric = 0 if self.metric == "euclidean" else 1
                self._c_index = self._lib.hnsw_create(
                    self.dimension, self.M, self.ef_construction, metric
                )
                if self._c_index:
                    # 准备向量数据
                    vectors_ptr = vectors.ctypes.data_as(
                        ctypes.POINTER(ctypes.c_float)
                    )
                    result = self._lib.hnsw_build(self._c_index, n, vectors_ptr)
                    if result > 0:
                        self._use_c = True
                        self._built = True
                        return
                    else:
                        self._lib.hnsw_destroy(self._c_index)
                        self._c_index = None
            except Exception as e:
                print(f"  WARN: C API failed: {e}")

        # 回退到 Python 实现
        self._python_vectors = vectors
        self._built = True

    def search(self, queries: np.ndarray, k: int) -> np.ndarray:
        """搜索 Top-K"""
        if not self._built:
            raise RuntimeError("Index not built")

        queries = queries.astype(np.float32)
        n_queries = queries.shape[0]

        if self._use_c and self._c_index and self._lib:
            # 使用 C 实现
            try:
                # 分配输出缓冲区
                ids = np.zeros((n_queries, k), dtype=np.int32)
                distances = np.zeros((n_queries, k), dtype=np.float32)

                ids_ptr = ids.ctypes.data_as(ctypes.POINTER(ctypes.c_int32))
                distances_ptr = distances.ctypes.data_as(ctypes.POINTER(ctypes.c_float))
                queries_ptr = queries.ctypes.data_as(ctypes.POINTER(ctypes.c_float))

                for i in range(n_queries):
                    self._lib.hnsw_search(
                        self._c_index,
                        queries_ptr + i * self.dimension,
                        k,
                        self.ef_search,
                        ids_ptr + i * k,
                        distances_ptr + i * k
                    )

                return ids
            except Exception as e:
                print(f"  WARN: C search failed: {e}")

        # Python 后备实现：暴力搜索
        if self._python_vectors is None:
            raise RuntimeError("No vectors loaded")

        results = []
        for q in queries:
            dists = np.linalg.norm(self._python_vectors - q, axis=1)
            top_k = np.argpartition(dists, k)[:k]
            top_k = top_k[np.argsort(dists[top_k])]
            results.append(top_k)

        return np.array(results)

    def get_size(self) -> int:
        """获取索引大小（字节）"""
        if self._use_c and self._c_index and self._lib:
            try:
                return self._lib.hnsw_get_size(self._c_index)
            except:
                pass

        if self._python_vectors is not None:
            return self._python_vectors.nbytes

        return 0

    def __del__(self):
        """析构时释放 C 资源"""
        if self._c_index and self._lib:
            try:
                self._lib.hnsw_destroy(self._c_index)
            except:
                pass


# ctypes 需要在模块级别导入
import ctypes
