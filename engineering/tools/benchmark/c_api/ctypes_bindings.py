#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
C API ctypes 绑定层

使用 ctypes 调用项目 C 实现
"""

import os
import ctypes
from pathlib import Path
from typing import Optional


# 全局库句柄
_lib = None
_lib_path: Optional[str] = None


class CAPIError(Exception):
    """C API 调用错误"""
    pass


def _get_lib_path() -> str:
    """获取 C 库路径"""
    global _lib_path
    if _lib_path:
        return _lib_path

    # 尝试多种可能的位置
    base_dir = Path(__file__).parent.parent.parent.parent.parent
    possible_paths = [
        # 构建输出目录
        base_dir / "build" / "engineering" / "libvector_index_c_api.so",
        base_dir / "build" / "engineering" / "libvector_index_c_api.dll",
        base_dir / "build" / "engineering" / "vector_index_c_api.dll",
        # 工具目录
        base_dir / "tools" / "benchmark" / "libvector_index_c_api.so",
        base_dir / "tools" / "benchmark" / "libvector_index_c_api.dll",
        # 相对路径
        "libvector_index_c_api.so",
        "libvector_index_c_api.dll",
        "vector_index_c_api.dll",
    ]

    for path in possible_paths:
        if os.path.exists(path):
            _lib_path = str(path)
            return _lib_path

    return ""


def load_library() -> Optional[ctypes.CDLL]:
    """加载 C 库"""
    global _lib

    if _lib is not None:
        return _lib

    lib_path = _get_lib_path()
    if not lib_path or not os.path.exists(lib_path):
        return None

    try:
        _lib = ctypes.CDLL(lib_path)
        _setup_function_signatures(_lib)
        return _lib
    except Exception as e:
        print(f"Warning: Failed to load C library from {lib_path}: {e}")
        return None


def _setup_function_signatures(lib):
    """设置函数签名"""

    # HNSW API
    lib.hnsw_create.argtypes = [
        ctypes.c_int32,  # dim
        ctypes.c_int32,  # M
        ctypes.c_int32,  # ef_construction
        ctypes.c_int32,  # metric (0=L2, 1=IP)
    ]
    lib.hnsw_create.restype = ctypes.c_void_p

    lib.hnsw_build.argtypes = [
        ctypes.c_void_p,  # index
        ctypes.c_int32,   # n
        ctypes.POINTER(ctypes.c_float),  # vectors
    ]
    lib.hnsw_build.restype = ctypes.c_int32

    lib.hnsw_search.argtypes = [
        ctypes.c_void_p,  # index
        ctypes.POINTER(ctypes.c_float),  # query
        ctypes.c_int32,   # k
        ctypes.c_int32,   # ef_search
        ctypes.POINTER(ctypes.c_int32),  # ids
        ctypes.POINTER(ctypes.c_float),  # distances
    ]
    lib.hnsw_search.restype = ctypes.c_int32

    lib.hnsw_search_batch.argtypes = [
        ctypes.c_void_p,
        ctypes.POINTER(ctypes.c_float),  # queries
        ctypes.c_int32,   # n_queries
        ctypes.c_int32,   # k
        ctypes.c_int32,   # ef_search
        ctypes.POINTER(ctypes.c_int32),  # ids
        ctypes.POINTER(ctypes.c_float),  # distances
    ]
    lib.hnsw_search_batch.restype = ctypes.c_int32

    lib.hnsw_get_size.argtypes = [ctypes.c_void_p]
    lib.hnsw_get_size.restype = ctypes.c_int64

    lib.hnsw_destroy.argtypes = [ctypes.c_void_p]

    # IVF-PQ API (DiskANN 实现)
    lib.ivf_pq_create.argtypes = [
        ctypes.c_int32,  # nlist
        ctypes.c_int32,  # pq_m
        ctypes.c_int32,  # pq_bits
        ctypes.c_int32,  # dim
    ]
    lib.ivf_pq_create.restype = ctypes.c_void_p

    lib.ivf_pq_set_nprobe.argtypes = [ctypes.c_void_p, ctypes.c_int32]

    lib.ivf_pq_train.argtypes = [
        ctypes.c_void_p,
        ctypes.c_int32,
        ctypes.POINTER(ctypes.c_float),
    ]
    lib.ivf_pq_train.restype = ctypes.c_int32

    lib.ivf_pq_add.argtypes = [
        ctypes.c_void_p,
        ctypes.c_int32,
        ctypes.POINTER(ctypes.c_float),
        ctypes.POINTER(ctypes.c_int32),
    ]
    lib.ivf_pq_add.restype = ctypes.c_int32

    lib.ivf_pq_search.argtypes = [
        ctypes.c_void_p,
        ctypes.POINTER(ctypes.c_float),
        ctypes.c_int32,
        ctypes.POINTER(ctypes.c_int32),
        ctypes.POINTER(ctypes.c_float),
    ]
    lib.ivf_pq_search.restype = ctypes.c_int32

    lib.ivf_pq_search_batch.argtypes = [
        ctypes.c_void_p,
        ctypes.POINTER(ctypes.c_float),
        ctypes.c_int32,
        ctypes.c_int32,
        ctypes.POINTER(ctypes.c_int32),
        ctypes.POINTER(ctypes.c_float),
    ]
    lib.ivf_pq_search_batch.restype = ctypes.c_int32

    lib.ivf_pq_get_size.argtypes = [ctypes.c_void_p]
    lib.ivf_pq_get_size.restype = ctypes.c_int64

    lib.ivf_pq_destroy.argtypes = [ctypes.c_void_p]

    # LSH API
    lib.lsh_create.argtypes = [
        ctypes.c_int32,  # dim
        ctypes.c_int32,  # num_hash
        ctypes.c_int32,  # table_size
    ]
    lib.lsh_create.restype = ctypes.c_void_p

    lib.lsh_add.argtypes = [
        ctypes.c_void_p,
        ctypes.c_int32,
        ctypes.POINTER(ctypes.c_float),
        ctypes.POINTER(ctypes.c_int32),
    ]
    lib.lsh_add.restype = ctypes.c_int32

    lib.lsh_search.argtypes = [
        ctypes.c_void_p,
        ctypes.POINTER(ctypes.c_float),
        ctypes.c_int32,
        ctypes.POINTER(ctypes.c_int32),
        ctypes.POINTER(ctypes.c_float),
    ]
    lib.lsh_search.restype = ctypes.c_int32

    lib.lsh_search_batch.argtypes = [
        ctypes.c_void_p,
        ctypes.POINTER(ctypes.c_float),
        ctypes.c_int32,
        ctypes.c_int32,
        ctypes.POINTER(ctypes.c_int32),
        ctypes.POINTER(ctypes.c_float),
    ]
    lib.lsh_search_batch.restype = ctypes.c_int32

    lib.lsh_get_size.argtypes = [ctypes.c_void_p]
    lib.lsh_get_size.restype = ctypes.c_int64

    lib.lsh_destroy.argtypes = [ctypes.c_void_p]


def get_lib():
    """获取已加载的库"""
    global _lib
    if _lib is None:
        _lib = load_library()
    return _lib
