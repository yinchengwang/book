"""
MiniVecDB Python SDK
轻量级向量数据库 Python 客户端
"""
from .client import MiniVecDBClient
from .native import NativeClient, NativeCollection, NativeError
from .exceptions import (
    MiniVecDBError,
    ConnectionError,
    TimeoutError,
    NotFoundError,
    ValidationError
)
from .collection import Collection
from .filter import Filter

__version__ = "0.2.0"
__all__ = [
    "MiniVecDBClient",
    "NativeClient",
    "NativeCollection",
    "NativeError",
    "Collection",
    "Filter",
    "MiniVecDBError",
    "ConnectionError",
    "TimeoutError",
    "NotFoundError",
    "ValidationError"
]
