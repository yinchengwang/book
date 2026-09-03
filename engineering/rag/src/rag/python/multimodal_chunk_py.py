"""
多模态 RAG 的 VDB C SDK ctypes 封装层

提供 TextChunk / TableChunk / ImageChunk / CodeChunk 的 Python 端数据模型
以及 MultimodalChunkAPI 客户端，封装 C 层 multimodal_insert_* / multimodal_search 等函数。

用法:
    api = MultimodalChunkAPI(lib_path="./build/libvdb.so")
    api.insert_text_chunk(TextChunk(...))
    results = api.search(tenant_id=1, query_text="...", query_embedding=emb)
"""

import ctypes
import json
from dataclasses import dataclass, field, asdict
from typing import Optional, Any
from enum import IntEnum

import numpy as np


class ChunkType(IntEnum):
    TEXT = 0
    TABLE = 1
    IMAGE = 2
    CODE = 3


# ---------- 数据模型 ----------

@dataclass
class TextChunk:
    chunk_id: int = 0
    doc_id: int = 0
    tenant_id: int = 0
    content: str = ""
    metadata: dict = field(default_factory=dict)
    embedding: Optional[np.ndarray] = None


@dataclass
class TableChunk:
    chunk_id: int = 0
    doc_id: int = 0
    tenant_id: int = 0
    title: Optional[str] = None
    headers: list = field(default_factory=list)
    rows: list = field(default_factory=list)
    caption: Optional[str] = None
    metadata: dict = field(default_factory=dict)
    embedding: Optional[np.ndarray] = None


@dataclass
class ImageChunk:
    chunk_id: int = 0
    doc_id: int = 0
    tenant_id: int = 0
    image_path: str = ""
    caption: Optional[str] = None
    ocr_text: Optional[str] = None
    metadata: dict = field(default_factory=dict)
    siglip_embedding: Optional[np.ndarray] = None
    text_embedding: Optional[np.ndarray] = None


@dataclass
class CodeChunk:
    chunk_id: int = 0
    doc_id: int = 0
    tenant_id: int = 0
    language: str = ""
    code: str = ""
    name: Optional[str] = None
    docstring: Optional[str] = None
    metadata: dict = field(default_factory=dict)
    embedding: Optional[np.ndarray] = None


@dataclass
class SearchResult:
    chunk_id: int = 0
    chunk_type: ChunkType = ChunkType.TEXT
    content: str = ""
    metadata: dict = field(default_factory=dict)
    score: float = 0.0
    highlight: Optional[str] = None


# ---------- C SDK 封装 ----------

class MultimodalChunkAPI:
    """VDB C SDK 多模态 chunk 接口的 Python 封装"""

    def __init__(self, lib_path: str):
        self.lib = ctypes.CDLL(lib_path)
        self._setup_func_signatures()

    def _setup_func_signatures(self):
        """设置 C 函数签名"""

        # int multimodal_insert_text_chunk(…)
        self.lib.multimodal_insert_text_chunk.argtypes = [ctypes.c_void_p]
        self.lib.multimodal_insert_text_chunk.restype = ctypes.c_int

        # int multimodal_insert_table_chunk(…)
        self.lib.multimodal_insert_table_chunk.argtypes = [ctypes.c_void_p]
        self.lib.multimodal_insert_table_chunk.restype = ctypes.c_int

        # int multimodal_insert_image_chunk(…)
        self.lib.multimodal_insert_image_chunk.argtypes = [ctypes.c_void_p]
        self.lib.multimodal_insert_image_chunk.restype = ctypes.c_int

        # int multimodal_insert_code_chunk(…)
        self.lib.multimodal_insert_code_chunk.argtypes = [ctypes.c_void_p]
        self.lib.multimodal_insert_code_chunk.restype = ctypes.c_int

        # int multimodal_delete_by_doc_id(uint64_t doc_id)
        self.lib.multimodal_delete_by_doc_id.argtypes = [ctypes.c_uint64]
        self.lib.multimodal_delete_by_doc_id.restype = ctypes.c_int

        # int multimodal_search(...)
        self.lib.multimodal_search.restype = ctypes.c_int

        # int multimodal_update_chunk(…)
        self.lib.multimodal_update_chunk.argtypes = [
            ctypes.c_uint64,       # chunk_id
            ctypes.c_int,          # chunk_type
            ctypes.c_void_p,       # chunk data
        ]
        self.lib.multimodal_update_chunk.restype = ctypes.c_int

    def insert_text_chunk(self, chunk: TextChunk) -> int:
        """插入文本块"""
        # TODO: 构建 C 结构体并调用 C 函数
        return self.lib.multimodal_insert_text_chunk(ctypes.c_void_p())

    def insert_table_chunk(self, chunk: TableChunk) -> int:
        """插入表格块"""
        return self.lib.multimodal_insert_table_chunk(ctypes.c_void_p())

    def insert_image_chunk(self, chunk: ImageChunk) -> int:
        """插入图像块"""
        return self.lib.multimodal_insert_image_chunk(ctypes.c_void_p())

    def insert_code_chunk(self, chunk: CodeChunk) -> int:
        """插入代码块"""
        return self.lib.multimodal_insert_code_chunk(ctypes.c_void_p())

    def delete_by_doc_id(self, doc_id: int) -> int:
        """删除文档及其所有 chunk"""
        return self.lib.multimodal_delete_by_doc_id(ctypes.c_uint64(doc_id))

    def search(
        self,
        tenant_id: int,
        query_text: str,
        query_embedding: np.ndarray,
        target_types: Optional[list[ChunkType]] = None,
        top_k: int = 10,
    ) -> list[SearchResult]:
        """多模态检索"""
        if target_types is None:
            target_types = [ChunkType.TEXT, ChunkType.TABLE, ChunkType.IMAGE, ChunkType.CODE]

        type_array = (ctypes.c_int * len(target_types))(*[int(t) for t in target_types])
        results_ptr = ctypes.POINTER(ctypes.c_void_p)()
        num_results = ctypes.c_int()

        ret = self.lib.multimodal_search(
            ctypes.c_uint64(tenant_id),
            query_text.encode("utf-8"),
            query_embedding.ctypes.data_as(ctypes.POINTER(ctypes.c_float)),
            ctypes.c_int(len(query_embedding)),
            type_array,
            ctypes.c_int(len(target_types)),
            ctypes.c_int(top_k),
            ctypes.byref(results_ptr),
            ctypes.byref(num_results),
        )

        if ret != 0:
            return []

        # TODO: 解析 C 返回的 multimodal_result_t 数组
        return []