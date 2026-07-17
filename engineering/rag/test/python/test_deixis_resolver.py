"""
指代消解器单元测试
"""

import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "src", "rag", "python"))

from deixis_resolver import SessionContext, DeixisResolver


class MockResult:
    """模拟检索结果"""
    def __init__(self, chunk_id=0, chunk_type="text", content="", metadata=None, score=0.0):
        self.chunk_id = chunk_id
        self.chunk_type = chunk_type
        self.content = content
        self.metadata = metadata or {}
        self.score = score


class TestSessionContext:
    """SessionContext 单元测试"""

    def setup_method(self):
        self.ctx = SessionContext(max_history=5, ttl_seconds=3600)

    def test_track_and_get(self):
        """记录和获取结果"""
        r = MockResult(chunk_id=1, chunk_type="text", content="hello")
        self.ctx.track_result(r)
        recent = self.ctx.get_recent_results()
        assert len(recent) == 1
        assert recent[0].chunk_id == 1

    def test_get_recent_images(self):
        """获取最近图像结果"""
        self.ctx.track_result(MockResult(1, "text", "文本"))
        self.ctx.track_result(MockResult(2, "image", "图片1"))
        self.ctx.track_result(MockResult(3, "image", "图片2"))

        images = self.ctx.get_recent_images(n=2)
        assert len(images) == 2
        assert all(r.chunk_type == "image" for r in images)

    def test_max_history(self):
        """超过上限时裁剪"""
        for i in range(10):
            self.ctx.track_result(MockResult(i, "text", f"item{i}"))
        assert len(self.ctx._history) <= 5

    def test_clear(self):
        """清空历史"""
        self.ctx.track_result(MockResult(1, "text", "hello"))
        self.ctx.clear()
        assert len(self.ctx.get_recent_results()) == 0


class TestDeixisResolver:
    """DeixisResolver 单元测试"""

    def setup_method(self):
        self.ctx = SessionContext()
        self.resolver = DeixisResolver(self.ctx)

        # 准备历史数据
        self.ctx.track_result(MockResult(1, "text", "文本内容", {"source": "doc1"}))
        self.ctx.track_result(MockResult(2, "image", "图片数据", {
            "caption": "审批流程图", "source": "doc1"
        }))
        self.ctx.track_result(MockResult(3, "image", "架构图", {
            "caption": "系统架构图", "source": "doc2"
        }))

    def test_resolve_demonstrative_image(self):
        """"那张" → 图像"""
        resolved, results = self.resolver._resolve_demonstrative("那张审批流程图在哪？", 5)
        assert len(results) > 0
        assert all(r.chunk_type == "image" for r in results)

    def test_resolve_ordinal_first(self):
        """"第一张" → 索引 0"""
        resolved, results = self.resolver._resolve_ordinal("第一张图的内容是什么？", 5)
        assert len(results) == 1
        assert results[0].chunk_id == 1

    def test_resolve_ordinal_second(self):
        """"第二个" → 索引 1"""
        resolved, results = self.resolver._resolve_ordinal("第二个结果是什么？", 5)
        assert len(results) == 1
        assert results[0].chunk_id == 2

    def test_resolve_attribute_caption(self):
        """"红色的架构图" → 匹配 caption"""
        resolved, results = self.resolver._resolve_attribute("系统架构图", 5)
        assert len(results) > 0

    def test_no_match(self):
        """无指代表达时返回空"""
        resolved, results = self.resolver.resolve("今天天气怎么样")
        assert results == []

    def test_chinese_to_int(self):
        """中文数字转换"""
        assert DeixisResolver._chinese_to_int('一') == 1
        assert DeixisResolver._chinese_to_int('十') == 10

    def test_full_resolve_demonstrative(self):
        """完整指代消解流程：指示词"""
        resolved, results = self.resolver.resolve("那张图怎么实现的？")
        assert len(results) > 0

    def test_full_resolve_ordinal(self):
        """完整指代消解流程：序数词"""
        resolved, results = self.resolver.resolve("第一张图的内容是什么？")
        assert len(results) > 0


if __name__ == "__main__":
    import pytest
    pytest.main([__file__, "-v"])