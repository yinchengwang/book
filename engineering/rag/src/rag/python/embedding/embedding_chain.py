"""
Embedding 服务链模块

支持自托管 Embedding（BGE/SigLIP/CodeBERT）优先，Jina API fallback。
"""

import logging
import os
from typing import Optional

import numpy as np

logger = logging.getLogger(__name__)


class JinaEmbeddingService:
    """
    Jina AI Embedding 服务

    使用 Jina AI API 生成 Embedding，作为自托管服务的 fallback。
    """

    def __init__(self, api_key: str = "", model: str = "jina-embeddings-v3"):
        self.api_key = api_key or os.environ.get("JINA_API_KEY", "")
        self.model = model
        self._client = None

    def _get_client(self):
        """获取 HTTP 客户端"""
        if self._client is None:
            try:
                import httpx
                self._client = httpx.Client(timeout=60.0)
            except ImportError:
                logger.warning("httpx 未安装，使用模拟模式")
                self._client = "mock"
        return self._client

    def encode(self, texts: list[str]) -> np.ndarray:
        """
        编码文本

        Args:
            texts: 文本列表

        Returns:
            Embedding 矩阵 (N x dim)
        """
        if not self.api_key:
            logger.warning("Jina API key 未设置，使用模拟")
            return np.random.randn(len(texts), 1024).astype(np.float32)

        client = self._get_client()
        if client == "mock":
            return np.random.randn(len(texts), 1024).astype(np.float32)

        try:
            import httpx
            response = client.post(
                "https://api.jina.ai/v1/embeddings",
                headers={
                    "Authorization": f"Bearer {self.api_key}",
                    "Content-Type": "application/json",
                },
                json={
                    "model": self.model,
                    "input": texts,
                },
            )
            result = response.json()
            embeddings = [item["embedding"] for item in result["data"]]
            return np.array(embeddings, dtype=np.float32)

        except Exception as e:
            logger.error(f"Jina API 调用失败: {e}")
            return np.random.randn(len(texts), 1024).astype(np.float32)


class EmbeddingServiceChain:
    """
    Embedding 服务链

    自托管优先，Jina API fallback。
    """

    def __init__(
        self,
        self_hosted=None,
        fallback: JinaEmbeddingService = None,
    ):
        self.self_hosted = self_hosted
        self.fallback = fallback

    def encode(self, texts: list[str]) -> np.ndarray:
        """
        编码文本

        先尝试自托管，失败则使用 Jina fallback。

        Args:
            texts: 文本列表

        Returns:
            Embedding 矩阵
        """
        # 尝试自托管服务
        if self.self_hosted:
            try:
                return self.self_hosted.encode(texts)
            except Exception as e:
                logger.warning(f"自托管 Embedding 失败: {e}，切换到 Jina fallback")

        # Jina fallback
        if self.fallback:
            try:
                return self.fallback.encode(texts)
            except Exception as e:
                logger.error(f"Jina fallback 也失败了: {e}")

        # 最后的兜底：随机向量
        logger.warning("所有 Embedding 服务都不可用，返回随机向量")
        return np.random.randn(len(texts), 1024).astype(np.float32)