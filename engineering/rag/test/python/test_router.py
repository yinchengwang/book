"""
路由模块单元测试
"""

import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "..", "src", "rag", "python"))

from routing.router import Router, RuleEngine, LLMIntentClassifier, RoutingDecision


class TestRuleEngine:
    """RuleEngine 单元测试"""

    def setup_method(self):
        self.engine = RuleEngine()

    def test_match_direct_question(self):
        """直接回答匹配"""
        decision = self.engine.match("今天周几？")
        assert decision is not None
        assert decision.strategy == "direct"

    def test_match_greeting(self):
        """问候匹配"""
        decision = self.engine.match("你好")
        assert decision is not None
        assert decision.strategy == "direct"

    def test_match_sql_query(self):
        """SQL 查询匹配"""
        decision = self.engine.match("Q2的销售额是多少")
        assert decision is not None
        assert decision.strategy in ("sql", "table")

    def test_match_image(self):
        """图像检索匹配"""
        decision = self.engine.match("那张审批流程图在哪？")
        assert decision is not None
        assert decision.strategy == "semantic"
        assert "image" in decision.modalities

    def test_match_table(self):
        """表格检索匹配"""
        decision = self.engine.match("查找去年的销售数据表")
        assert decision is not None

    def test_match_code(self):
        """代码检索匹配"""
        decision = self.engine.match("找一下排序函数的代码")
        assert decision is not None
        assert "code" in decision.modalities

    def test_no_match(self):
        """无匹配时返回 None"""
        decision = self.engine.match("今天天气怎么样")
        assert decision is None

    def test_thank_you(self):
        """感谢语匹配"""
        decision = self.engine.match("谢谢")
        assert decision is not None
        assert decision.strategy == "direct"


class TestRouter:
    """Router 单元测试"""

    def setup_method(self):
        self.router = Router()

    def test_route_direct(self):
        """路由到直接回答"""
        decision = self.router.route("今天周几？")
        assert decision.strategy == "direct"

    def test_route_image(self):
        """路由到图像检索"""
        decision = self.router.route("那张审批流程图在哪？")
        assert decision.strategy == "semantic"

    def test_route_semantic_default(self):
        """无规则匹配时路由到语义检索"""
        decision = self.router.route("今天天气怎么样")
        # LLM 分类器不可用，默认多模态
        assert decision.strategy == "multimodal"


if __name__ == "__main__":
    import pytest
    pytest.main([__file__, "-v"])