"""
MiniVecDB Python 本地绑定 — 通过 ctypes 调用 db_sdk C 库
"""
from __future__ import annotations

import ctypes
import ctypes.util
import platform
import os
from pathlib import Path
from typing import Optional, List, Tuple


# ======================================================================
# ctypes 类型定义
# ======================================================================

class _DbSdkConfig(ctypes.Structure):
    _fields_ = [
        ("name", ctypes.c_char * 128),
        ("dimension", ctypes.c_int32),
        ("index_type", ctypes.c_int32),
        ("metric_type", ctypes.c_int32),
        ("index_ef_search", ctypes.c_int32),
        ("index_m", ctypes.c_int32),
        ("index_ef_construction", ctypes.c_int32),
    ]


class _DbSdkSearchParams(ctypes.Structure):
    _fields_ = [
        ("top_k", ctypes.c_int32),
        ("radius", ctypes.c_float),
        ("with_distance", ctypes.c_bool),
        ("with_metadata", ctypes.c_bool),
        ("offset", ctypes.c_int32),
        ("limit", ctypes.c_int32),
    ]


class _DbSdkResultItem(ctypes.Structure):
    _fields_ = [
        ("id", ctypes.c_int64),
        ("distance", ctypes.c_float),
        ("score", ctypes.c_float),
        ("metadata", ctypes.c_void_p),
        ("metadata_size", ctypes.c_int32),
    ]


class _DbSdkResults(ctypes.Structure):
    _fields_ = [
        ("items", ctypes.POINTER(_DbSdkResultItem)),
        ("count", ctypes.c_int32),
        ("capacity", ctypes.c_int32),
        ("total_time_us", ctypes.c_int64),
    ]


# ======================================================================
# 错误码
# ======================================================================

DB_SDK_OK = 0
DB_SDK_ERR_NOT_FOUND = -1
DB_SDK_ERR_EXISTS = -2
DB_SDK_ERR_INVALID_PARAM = -3
DB_SDK_ERR_NO_MEMORY = -4


# ======================================================================
# 异常
# ======================================================================

class NativeError(Exception):
    """本地调用错误"""
    pass


class CollectionNotFoundError(NativeError):
    """集合不存在"""
    pass


class CollectionExistsError(NativeError):
    """集合已存在"""
    pass


# ======================================================================
# 库加载
# ======================================================================

def _find_lib_path() -> Optional[str]:
    """查找 db_sdk 共享库路径"""
    search_dirs: List[Path] = []

    # 优先从环境变量读取
    env_path = os.environ.get("MINIVEDB_LIB_PATH")
    if env_path:
        search_dirs.append(Path(env_path))

    _lib_name = {
        "win32": "db_sdk.dll",
        "linux": "libdb_sdk.so",
        "darwin": "libdb_sdk.dylib",
    }.get(platform.system().lower(), "libdb_sdk.so")

    # 查找 build 目录
    cwd = Path.cwd()
    build_candidates = [
        cwd / "build/engineering",
        cwd / "build",
        cwd / "build/Release",
        cwd / "build/Debug",
        cwd.parent / "build/engineering",
        cwd.parent / "build",
    ]
    for d in build_candidates:
        if d.exists():
            search_dirs.append(d)

    # 系统库路径
    sys_path = ctypes.util.find_library("db_sdk")
    if sys_path:
        return sys_path

    for d in search_dirs:
        p = d / _lib_name
        if p.exists():
            return str(p)
    return None


def _load_lib():
    """加载 db_sdk 共享库"""
    lib_path = _find_lib_path()
    if not lib_path:
        raise RuntimeError(
            "找不到 db_sdk 共享库。请设置 MINIVEDB_LIB_PATH 环境变量指向包含 "
            "db_sdk.dll/libdb_sdk.so/libdb_sdk.dylib 的目录"
        )
    lib = ctypes.CDLL(lib_path)

    # 设置函数签名
    # db_sdk_open
    lib.db_sdk_open.argtypes = [ctypes.c_char_p]
    lib.db_sdk_open.restype = ctypes.c_void_p

    # db_sdk_close
    lib.db_sdk_close.argtypes = [ctypes.c_void_p]
    lib.db_sdk_close.restype = ctypes.c_int

    # db_sdk_error
    lib.db_sdk_error.argtypes = [ctypes.c_void_p]
    lib.db_sdk_error.restype = ctypes.c_char_p

    # db_sdk_create_collection
    lib.db_sdk_create_collection.argtypes = [
        ctypes.c_void_p, ctypes.c_char_p, ctypes.POINTER(_DbSdkConfig),
    ]
    lib.db_sdk_create_collection.restype = ctypes.c_int

    # db_sdk_drop_collection
    lib.db_sdk_drop_collection.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    lib.db_sdk_drop_collection.restype = ctypes.c_int

    # db_sdk_list_collections
    lib.db_sdk_list_collections.argtypes = [
        ctypes.c_void_p,
        ctypes.POINTER(ctypes.POINTER(ctypes.c_char_p)),
        ctypes.POINTER(ctypes.c_int32),
    ]
    lib.db_sdk_list_collections.restype = ctypes.c_int

    # db_sdk_free_names
    lib.db_sdk_free_names.argtypes = [
        ctypes.POINTER(ctypes.c_char_p), ctypes.c_int32,
    ]
    lib.db_sdk_free_names.restype = None

    # db_sdk_get_collection
    lib.db_sdk_get_collection.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    lib.db_sdk_get_collection.restype = ctypes.c_void_p

    # db_sdk_collection_info
    lib.db_sdk_collection_info.argtypes = [
        ctypes.c_void_p, ctypes.POINTER(_DbSdkConfig),
    ]
    lib.db_sdk_collection_info.restype = ctypes.c_int

    # db_sdk_size
    lib.db_sdk_size.argtypes = [ctypes.c_void_p]
    lib.db_sdk_size.restype = ctypes.c_int32

    # db_sdk_insert
    lib.db_sdk_insert.argtypes = [
        ctypes.c_void_p, ctypes.POINTER(ctypes.c_float),
        ctypes.c_int32, ctypes.c_void_p, ctypes.c_int32,
    ]
    lib.db_sdk_insert.restype = ctypes.c_int64

    # db_sdk_search
    lib.db_sdk_search.argtypes = [
        ctypes.c_void_p, ctypes.POINTER(ctypes.c_float),
        ctypes.c_int32, ctypes.POINTER(_DbSdkSearchParams),
    ]
    lib.db_sdk_search.restype = ctypes.POINTER(_DbSdkResults)

    # db_sdk_delete
    lib.db_sdk_delete.argtypes = [ctypes.c_void_p, ctypes.c_int64]
    lib.db_sdk_delete.restype = ctypes.c_int

    # db_sdk_results_free
    lib.db_sdk_results_free.argtypes = [ctypes.POINTER(_DbSdkResults)]
    lib.db_sdk_results_free.restype = None

    return lib


# 全局单例
_lib = None


def _get_lib():
    global _lib
    if _lib is None:
        _lib = _load_lib()
    return _lib


# ======================================================================
# NativeCollection
# ======================================================================

class NativeCollection:
    """本地集合封装"""

    def __init__(self, handle: int, name: str):
        self._handle = ctypes.c_void_p(handle)
        self._lib = _get_lib()
        self.name = name

    @property
    def info(self) -> dict:
        cfg = _DbSdkConfig()
        rc = self._lib.db_sdk_collection_info(self._handle, ctypes.byref(cfg))
        if rc != DB_SDK_OK:
            raise NativeError(f"获取集合信息失败: rc={rc}")
        return {
            "name": cfg.name.decode("utf-8"),
            "dimension": cfg.dimension,
            "index_type": cfg.index_type,
            "metric_type": cfg.metric_type,
        }

    def insert(
        self,
        vectors: list,
        ids: Optional[list] = None,
        metadata: Optional[list] = None,
    ) -> dict:
        count = 0
        for i, vec in enumerate(vectors):
            dim = len(vec)
            vec_arr = (ctypes.c_float * dim)(*vec)
            meta_bytes: Optional[bytes] = None
            if metadata and i < len(metadata):
                import json
                meta_str = json.dumps(metadata[i], ensure_ascii=False)
                meta_bytes = meta_str.encode("utf-8")
            rc = self._lib.db_sdk_insert(
                self._handle,
                vec_arr,
                dim,
                ctypes.c_char_p(meta_bytes) if meta_bytes else None,
                len(meta_bytes) if meta_bytes else 0,
            )
            if rc > 0:
                count += 1
        return {"inserted": count}

    def search(
        self,
        query: list,
        top_k: int = 10,
        **kwargs,
    ) -> list:
        dim = len(query)
        query_arr = (ctypes.c_float * dim)(*query)
        params = _DbSdkSearchParams(
            top_k=top_k,
            radius=0,
            with_distance=True,
            with_metadata=False,
            offset=0,
            limit=0,
        )
        res_ptr = self._lib.db_sdk_search(
            self._handle, query_arr, dim, ctypes.byref(params),
        )
        if not res_ptr:
            return []
        results = res_ptr.contents
        items = []
        for i in range(results.count):
            item = results.items[i]
            items.append({
                "id": item.id,
                "distance": item.distance,
                "score": item.score,
            })
        self._lib.db_sdk_results_free(res_ptr)
        return items

    def count(self) -> int:
        sz = self._lib.db_sdk_size(self._handle)
        return max(0, sz)

    def delete(self, vector_id: int) -> dict:
        rc = self._lib.db_sdk_delete(self._handle, vector_id)
        if rc != DB_SDK_OK:
            raise NativeError(f"删除失败: rc={rc}")
        return {"deleted": True}


# ======================================================================
# NativeClient
# ======================================================================

class NativeClient:
    """本地数据库客户端 — 直接调用 C SDK"""

    def __init__(self, data_dir: str = "./vdb_data"):
        self._lib = _get_lib()
        self._handle = self._lib.db_sdk_open(data_dir.encode("utf-8"))
        if not self._handle:
            raise RuntimeError(f"db_sdk_open 失败: {data_dir}")

    def close(self):
        if self._handle:
            self._lib.db_sdk_close(self._handle)
            self._handle = None

    def _check_handle(self):
        if not self._handle:
            raise RuntimeError("数据库已关闭")

    def create_collection(
        self,
        name: str,
        dimension: int,
        metric_type: str = "L2",
        index_type: str = "hnsw",
    ) -> dict:
        self._check_handle()
        mt = {"l2": 0, "cosine": 1, "ip": 2}.get(metric_type.lower(), 0)
        it = {"hnsw": 1, "ivf": 2, "brute_force": 4, "auto": 0}.get(
            index_type.lower(), 1
        )
        cfg = _DbSdkConfig(
            name=name.encode("utf-8"),
            dimension=dimension,
            index_type=it,
            metric_type=mt,
            index_ef_search=100,
            index_m=16,
            index_ef_construction=200,
        )
        rc = self._lib.db_sdk_create_collection(
            self._handle, name.encode("utf-8"), ctypes.byref(cfg),
        )
        if rc == DB_SDK_ERR_EXISTS:
            raise CollectionExistsError(f"集合已存在: {name}")
        if rc != DB_SDK_OK:
            err = self._lib.db_sdk_error(self._handle)
            raise NativeError(f"创建集合失败: {err.decode('utf-8')}")
        return {"name": name, "dimension": dimension, "metric_type": metric_type}

    def get_collection(self, name: str) -> NativeCollection:
        self._check_handle()
        handle = self._lib.db_sdk_get_collection(self._handle, name.encode("utf-8"))
        if not handle:
            raise CollectionNotFoundError(f"集合不存在: {name}")
        return NativeCollection(handle, name)

    def list_collections(self) -> list:
        self._check_handle()
        names_ptr = ctypes.POINTER(ctypes.c_char_p)()
        count = ctypes.c_int32(0)
        rc = self._lib.db_sdk_list_collections(
            self._handle, ctypes.byref(names_ptr), ctypes.byref(count),
        )
        if rc != DB_SDK_OK:
            raise NativeError("获取集合列表失败")
        result = []
        for i in range(count.value):
            result.append(names_ptr[i].decode("utf-8"))
        self._lib.db_sdk_free_names(names_ptr, count)
        return result

    def delete_collection(self, name: str) -> dict:
        self._check_handle()
        rc = self._lib.db_sdk_drop_collection(self._handle, name.encode("utf-8"))
        if rc == DB_SDK_ERR_NOT_FOUND:
            raise CollectionNotFoundError(f"集合不存在: {name}")
        if rc != DB_SDK_OK:
            raise NativeError(f"删除集合失败: rc={rc}")
        return {"deleted": True}

    def collection_exists(self, name: str) -> bool:
        try:
            self.get_collection(name)
            return True
        except CollectionNotFoundError:
            return False

    def health(self) -> dict:
        return {"status": "ok", "mode": "native"}
