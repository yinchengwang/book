# type: ignore
"""P1 多模态嵌入式 SDK Python 类型存根"""
from typing import List, Dict, Any, Optional

class Model:
    VECTOR: int
    GRAPH: int
    TIMESERIES: int
    TEXT: int

class DB:
    def __init__(self, path: str) -> None: ...
    def close(self) -> None: ...
    def create_collection(self, name: str, model: int, vector_dim: int = 0) -> None: ...
    def drop_collection(self, name: str) -> None: ...
    def vectors_add(
        self,
        collection: str,
        ids: List[str],
        embeddings: List[List[float]],
    ) -> None: ...
    def vectors_search(
        self,
        collection: str,
        query: List[float],
        top_k: int = 10,
    ) -> List[Dict[str, Any]]: ...
    def graph_add_node(
        self,
        collection: str,
        id: str,
        label: str,
        properties: str = "",
    ) -> None: ...
    def graph_add_edge(
        self,
        collection: str,
        src_id: str,
        dst_id: str,
        label: str,
        properties: str = "",
    ) -> None: ...
    def ts_append(
        self,
        collection: str,
        timestamp: int,
        value: float,
        tags: str = "",
    ) -> None: ...
    def ts_query(
        self,
        collection: str,
        start_ts: int,
        end_ts: int,
    ) -> List[Dict[str, Any]]: ...
    def text_add(
        self,
        collection: str,
        id: str,
        text: str,
        metadata: str = "",
    ) -> None: ...
    def text_search(
        self,
        collection: str,
        query: str,
        top_k: int = 10,
    ) -> List[Dict[str, Any]]: ...
    def __enter__(self) -> "DB": ...
    def __exit__(self, exc_type: Any, exc_val: Any, exc_tb: Any) -> None: ...