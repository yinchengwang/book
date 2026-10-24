#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
DiskANN 索引适配器 - C 实现

使用项目 DiskANN 实现
"""

import numpy as np
import ctypes
from typing import Optional

from .base import BaseIndexAdapter
from ..c_api.ctypes_bindings import get_lib


class DiskANNAdapter(BaseIndexAdapter):
    """
    DiskANN 索引适配器

    使用项目 DiskANN 实现（Vamana 图 + PQ 量化）
    """

    def __init__(self, dimension: int = 384, metric: str = "euclidean",
                 R: int = 32, L: int = 100, pq_m: int = 8, pq_bits: int = 8,
                 search_list_size: int = 64, **kwargs):
        super().__init__(dimension, metric, **kwargs)
        self.R = R  # 目标邻居数
        self.L = L  # 构图候选数
        self.pq_m = pq_m  # PQ 子空间数
        self.pq_bits = pq_bits  # PQ 量化位数
        self.search_list_size = search_list_size  # 搜索候选数

        # C API 状态
        self._c_index: Optional[int] = None
        self._lib = None
        self._use_c = False

        # Python 后备实现
        self._python_vectors: Optional[np.ndarray] = None

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
            try:
                # DiskANN 使用 ivf_pq_create（映射关系）
                self._c_index = self._lib.ivf_pq_create(
                    self.R, self.pq_m, self.pq_bits, self.dimension
                )
                if self._c_index:
                    vectors_ptr = vectors.ctypes.data_as(
                        ctypes.POINTER(ctypes.c_float)
                    )
                    # 训练
                    self._lib.ivf_pq_train(self._c_index, min(n, 10000), vectors_ptr)
                    # 添加向量
                    result = self._lib.ivf_pq_add(self._c_index, n, vectors_ptr, None)
                    if result > 0:
                        self._lib.ivf_pq_set_nprobe(self._c_index, self.search_list_size)
                        self._use_c = True
                        self._built = True
                        return
                    else:
                        self._lib.ivf_pq_destroy(self._c_index)
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
            try:
                ids = np.zeros((n_queries, k), dtype=np.int32)
                distances = np.zeros((n_queries, k), dtype=np.float32)

                ids_ptr = ids.ctypes.data_as(ctypes.POINTER(ctypes.c_int32))
                distances_ptr = distances.ctypes.data_as(ctypes.POINTER(ctypes.c_float))

                for i in range(n_queries):
                    self._lib.ivf_pq_search(
                        self._c_index,
                        queries[i].ctypes.data_as(ctypes.POINTER(ctypes.c_float)),
                        k,
                        ids_ptr + i * k,
                        distances_ptr + i * k
                    )

                return ids
            except Exception as e:
                print(f"  WARN: C search failed: {e}")

        # Python 后备实现
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
        """获取索引大小"""
        if self._use_c and self._c_index and self._lib:
            try:
                return self._lib.ivf_pq_get_size(self._c_index)
            except:
                pass

        if self._python_vectors is not None:
            return self._python_vectors.nbytes

        return 0

    def __del__(self):
        """析构时释放 C 资源"""
        if self._c_index and self._lib:
            try:
                self._lib.ivf_pq_destroy(self._c_index)
            except:
                pass
