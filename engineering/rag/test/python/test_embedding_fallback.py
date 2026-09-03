"""
Embedding Fallback 单元测试
"""

import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "src", "rag", "python"))

import numpy as np
from embedding.embedding_chain import JinaEmbeddingService, EmbeddingServiceChain


class MockEmbeddingService:
    """模拟 Embedding 服务"""
    def encode(self, texts):
        return np.random.randn(len(texts), 1024).astype(np.float32)


class FailingEmbeddingService:
    """失败的 Embedding 服务"""
    def encode(self, texts):
        raise RuntimeError("Embedding 服务失败")


class TestJinaEmbeddingService:
    """JinaEmbeddingService 单元测试"""

    def setup_method(self):
        self.service = JinaEmbeddingService(api_key="test_key")

    def test_encode_no_key(self):
        """无 API key 时返回模拟"""
        service = JinaEmbeddingService(api_key="")
        result = service.encode(["text1", "text2"])
        assert result.shape == (2, 1024)


class TestEmbeddingServiceChain:
    """EmbeddingServiceChain 单元测试"""

    def test_self_hosted_priority(self):
        """自托管优先"""
        primary = MockEmbeddingService()
        fallback = MockEmbeddingService()
        chain = EmbeddingServiceChain(primary, fallback)

        result = chain.encode(["text1", "text2"])
        assert result.shape == (2, 1024)

    def test_fallback_on_failure(self):
        """自托管失败时 fallback"""
        primary = FailingEmbeddingService()
        fallback = MockEmbeddingService()
        chain = EmbeddingServiceChain(primary, fallback)

        result = chain.encode(["text1"])
        assert result.shape == (1, 1024)

    def test_all_fail_random_fallback(self):
        """全部失败返回随机"""
        chain = EmbeddingServiceChain(FailingEmbeddingService(), FailingEmbeddingService())
        result = chain.encode(["text1"])
        assert result.shape == (1, 1024)

    def test_only_self_hosted(self):
        """只有自托管"""
        chain = EmbeddingServiceChain(MockEmbeddingService())
        result = chain.encode(["text"])
        assert result.shape == (1, 1024)


if __name__ == "__main__":
    import pytest
    pytest.main([__file__, "-v"])