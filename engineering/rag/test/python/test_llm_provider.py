"""
LLM Provider 端到端测试
"""

import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "src", "rag", "python"))

from llm.llm_provider import (
    MiniMaxProvider, ClaudeProvider, LLMProviderChain, LLMCapabilityError
)


class TestMiniMaxProvider:
    """MiniMaxProvider 单元测试"""

    def setup_method(self):
        self.provider = MiniMaxProvider(api_key="test_key")

    def test_capabilities_mock(self):
        """模拟模式时假设都支持"""
        caps = self.provider.capabilities
        assert caps["vision"] is True
        assert caps["function_call"] is True

    def test_generate_mock(self):
        """模拟模式返回模拟回答"""
        result = self.provider.generate("测试问题")
        assert result is not None
        assert isinstance(result, str)

    def test_generate_with_context(self):
        """带上下文的生成"""
        context = {"context": "这是参考信息"}
        result = self.provider.generate("测试问题", context=context)
        assert result is not None


class TestClaudeProvider:
    """ClaudeProvider 单元测试"""

    def setup_method(self):
        self.provider = ClaudeProvider(api_key="test_key")

    def test_generate_mock(self):
        """模拟模式返回模拟回答"""
        result = self.provider.generate("测试问题")
        assert result is not None
        assert "Claude" in result


class TestLLMProviderChain:
    """LLMProviderChain 单元测试"""

    def setup_method(self):
        self.primary = MiniMaxProvider(api_key="test_key")
        self.fallback = ClaudeProvider(api_key="test_key")
        self.chain = LLMProviderChain(self.primary, self.fallback)

    def test_primary_success(self):
        """主 provider 成功"""
        result = self.chain.generate("测试问题")
        assert result is not None

    def test_fallback_on_failure(self):
        """主 provider 失败时 fallback"""

        class FailingProvider(MiniMaxProvider):
            def generate(self, *args, **kwargs):
                raise RuntimeError("模拟失败")

        chain = LLMProviderChain(FailingProvider("key"), self.fallback)
        result = chain.generate("测试问题")
        assert result is not None
        assert "Claude" in result

    def test_all_fail(self):
        """所有 provider 都失败"""

        class FailingProvider:
            def generate(self, *args, **kwargs):
                raise RuntimeError("模拟失败")

        chain = LLMProviderChain(FailingProvider())
        try:
            chain.generate("测试问题")
            assert False, "应该抛出异常"
        except RuntimeError:
            pass


if __name__ == "__main__":
    import pytest
    pytest.main([__file__, "-v"])