"""
MiniVecDB Collection 封装
"""
from typing import Any, Dict, List, Optional, Union
import numpy as np


class Collection:
    """集合封装"""

    def __init__(self, client: Any, name: str):
        self._client = client
        self.name = name

    @property
    def info(self) -> Dict[str, Any]:
        """获取集合信息"""
        return self._client._get(f"/collections/{self.name}")

    def insert(
        self,
        vectors: List[List[float]],
        ids: Optional[List[str]] = None,
        metadata: Optional[List[Dict[str, Any]]] = None
    ) -> Dict[str, Any]:
        """
        批量插入向量

        Args:
            vectors: 向量列表，每个向量是浮点数列表
            ids: 可选的 ID 列表
            metadata: 可选的元数据列表

        Returns:
            插入结果
        """
        data = []
        for i, vec in enumerate(vectors):
            item = {"vector": vec}
            if ids:
                item["id"] = ids[i]
            if metadata:
                item["metadata"] = metadata[i]
            data.append(item)

        payload = {"vectors": data}
        return self._client._post(
            f"/collections/{self.name}/vectors",
            payload
        )

    def search(
        self,
        query: Union[List[float], np.ndarray],
        top_k: int = 10,
        filter: Optional[Dict[str, Any]] = None,
        include_vector: bool = False
    ) -> List[Dict[str, Any]]:
        """
        向量搜索

        Args:
            query: 查询向量
            top_k: 返回结果数量
            filter: 过滤器
            include_vector: 是否返回向量

        Returns:
            搜索结果列表
        """
        if isinstance(query, np.ndarray):
            query = query.tolist()

        payload = {
            "vector": query,
            "top_k": top_k
        }
        if filter:
            payload["filter"] = filter
        if include_vector:
            payload["include_vector"] = True

        result = self._client._post(
            f"/collections/{self.name}/search",
            payload
        )
        return result.get("results", [])

    def get(self, vector_id: str) -> Dict[str, Any]:
        """获取向量"""
        return self._client._get(
            f"/collections/{self.name}/vectors/{vector_id}"
        )

    def delete(self, vector_id: str) -> Dict[str, Any]:
        """删除向量"""
        return self._client._delete(
            f"/collections/{self.name}/vectors/{vector_id}"
        )

    def count(self) -> int:
        """获取向量数量"""
        info = self.info
        return info.get("vector_count", 0)
