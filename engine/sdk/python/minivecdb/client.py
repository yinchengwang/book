"""
MiniVecDB Python 客户端
"""
from typing import Any, Dict, List, Optional
import urllib.request
import urllib.error
import json
import time
from contextlib import contextmanager

from .exceptions import (
    ConnectionError,
    TimeoutError,
    NotFoundError,
    ValidationError,
    APIError
)
from .collection import Collection
from .filter import Filter


class MiniVecDBClient:
    """
    MiniVecDB Python 客户端

    使用示例:
        client = MiniVecDBClient("http://localhost:8080")

        # 创建集合
        client.create_collection("my_vectors", dimension=128)

        # 获取集合
        collection = client.get_collection("my_vectors")

        # 插入向量
        collection.insert([[0.1, 0.2, ...] for _ in range(100)])

        # 搜索
        results = collection.search([0.1, 0.2, ...], top_k=10)

        # 删除集合
        client.delete_collection("my_vectors")
    """

    def __init__(
        self,
        url: str = "http://localhost:8080",
        timeout: float = 30.0,
        retry: int = 3,
        retry_delay: float = 0.5
    ):
        """
        初始化客户端

        Args:
            url: 服务器地址
            timeout: 请求超时时间（秒）
            retry: 重试次数
            retry_delay: 重试延迟（秒）
        """
        self.url = url.rstrip("/")
        self.timeout = timeout
        self.retry = retry
        self.retry_delay = retry_delay
        self._session: Optional[Any] = None

    def _request(
        self,
        method: str,
        path: str,
        data: Optional[Dict[str, Any]] = None
    ) -> Dict[str, Any]:
        """发送 HTTP 请求"""
        url = f"{self.url}{path}"
        headers = {
            "Content-Type": "application/json",
            "Accept": "application/json"
        }

        body = json.dumps(data).encode("utf-8") if data else None

        for attempt in range(self.retry):
            try:
                req = urllib.request.Request(
                    url,
                    data=body,
                    headers=headers,
                    method=method
                )
                with urllib.request.urlopen(req, timeout=self.timeout) as resp:
                    result = resp.read().decode("utf-8")
                    if result:
                        return json.loads(result)
                    return {}

            except urllib.error.HTTPError as e:
                if e.code == 404:
                    raise NotFoundError(f"Resource not found: {path}")
                body = e.read().decode("utf-8") if e.fp else ""
                try:
                    err_data = json.loads(body)
                    raise APIError(
                        err_data.get("message", str(e)),
                        err_data.get("code", "UNKNOWN"),
                        e.code
                    )
                except json.JSONDecodeError:
                    raise APIError(str(e), "UNKNOWN", e.code)

            except urllib.error.URLError as e:
                if attempt < self.retry - 1:
                    time.sleep(self.retry_delay)
                    continue
                raise ConnectionError(f"Failed to connect: {e.reason}")

        raise ConnectionError(f"Failed after {self.retry} retries")

    def _get(self, path: str) -> Dict[str, Any]:
        """GET 请求"""
        return self._request("GET", path)

    def _post(self, path: str, data: Dict[str, Any]) -> Dict[str, Any]:
        """POST 请求"""
        return self._request("POST", path, data)

    def _delete(self, path: str) -> Dict[str, Any]:
        """DELETE 请求"""
        return self._request("DELETE", path)

    # ===================================================================
    # 集合操作
    # ===================================================================

    def create_collection(
        self,
        name: str,
        dimension: int,
        metric_type: str = "L2",
        description: Optional[str] = None
    ) -> Dict[str, Any]:
        """
        创建集合

        Args:
            name: 集合名称
            dimension: 向量维度
            metric_type: 距离度量类型 (L2, IP, COSINE)
            description: 描述

        Returns:
            创建的集合信息
        """
        payload = {
            "name": name,
            "dimension": dimension,
            "metric_type": metric_type
        }
        if description:
            payload["description"] = description

        return self._post("/collections", payload)

    def get_collection(self, name: str) -> Collection:
        """
        获取集合对象

        Args:
            name: 集合名称

        Returns:
            Collection 对象
        """
        # 验证集合存在
        self._get(f"/collections/{name}")
        return Collection(self, name)

    def list_collections(self) -> List[Dict[str, Any]]:
        """列出所有集合"""
        return self._get("/collections")

    def delete_collection(self, name: str) -> Dict[str, Any]:
        """删除集合"""
        return self._delete(f"/collections/{name}")

    def collection_exists(self, name: str) -> bool:
        """检查集合是否存在"""
        try:
            self._get(f"/collections/{name}")
            return True
        except NotFoundError:
            return False

    # ===================================================================
    # 健康检查
    # ===================================================================

    def health(self) -> Dict[str, Any]:
        """健康检查"""
        return self._get("/health")

    def ready(self) -> Dict[str, Any]:
        """就绪检查"""
        return self._get("/ready")

    def live(self) -> Dict[str, Any]:
        """活跃检查"""
        return self._get("/live")

    def metrics(self) -> str:
        """获取 Prometheus 指标"""
        url = f"{self.url}/metrics"
        req = urllib.request.Request(url, method="GET")
        with urllib.request.urlopen(req, timeout=self.timeout) as resp:
            return resp.read().decode("utf-8")

    # ===================================================================
    # 上下文管理
    # ===================================================================

    @contextmanager
    def session(self):
        """上下文管理器"""
        yield self
        # 可以在这里添加清理逻辑

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        pass
