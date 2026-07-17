"""
ContextBuilder 单元测试

测试分层上下文构建的各模态格式化、token 控制等功能。
"""

import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "src", "rag", "python"))

from context_builder import ContextBuilder


class MockResult:
    """模拟检索结果"""
    def __init__(self, chunk_id=0, chunk_type="text", content="", metadata=None, score=0.0):
        self.chunk_id = chunk_id
        self.chunk_type = chunk_type
        self.content = content
        self.metadata = metadata or {}
        self.score = score


class TestContextBuilder:
    """ContextBuilder 单元测试"""

    def setup_method(self):
        self.builder = ContextBuilder()

    def test_build_empty(self):
        """空结果构建"""
        result = self.builder.build("test query", [])
        assert result["query"] == "test query"
        assert result["source_count"] == 0
        assert result["token_count"] == 0

    def test_build_text(self):
        """构建文本上下文"""
        results = [
            MockResult(1, "text", "文本内容", {"source": "doc1"}, 0.9),
        ]
        result = self.builder.build("query", results)
        assert "文本内容" in result["context"]
        assert "doc1" in result["context"]

    def test_build_table(self):
        """构建表格上下文"""
        results = [
            MockResult(1, "table", "", {
                "title": "测试表",
                "headers": ["A", "B"],
                "rows": [["1", "2"]],
            }, 0.9),
        ]
        result = self.builder.build("query", results)
        assert "测试表" in result["context"]
        assert "| A | B |" in result["context"]

    def test_build_code(self):
        """构建代码上下文"""
        results = [
            MockResult(1, "code", "def hello(): pass", {
                "name": "hello",
                "language": "python",
            }, 0.9),
        ]
        result = self.builder.build("query", results)
        assert "hello" in result["context"]
        assert "python" in result["context"]

    def test_build_image(self):
        """构建图像上下文"""
        results = [
            MockResult(1, "image", "", {
                "caption": "一张图",
                "ocr_text": "图中文本",
                "image_path": "/path/to/img.jpg",
            }, 0.9),
        ]
        result = self.builder.build("query", results)
        assert "一张图" in result["context"]
        assert "图中文本" in result["context"]

    def test_modality_priority(self):
        """模态优先级：text > table > image > code"""
        results = [
            MockResult(1, "code", "code1", {}, 0.9),
            MockResult(2, "text", "text1", {}, 0.9),
            MockResult(3, "table", "table1", {"headers": ["A"], "rows": []}, 0.9),
            MockResult(4, "image", "image1", {"caption": "c"}, 0.9),
        ]
        result = self.builder.build("query", results)
        # 文本应该在表格前面
        text_pos = result["context"].find("text1")
        table_pos = result["context"].find("table1")
        assert text_pos < table_pos

    def test_count_tokens(self):
        """token 估算"""
        text = "这是一段测试文本" * 10
        tokens = ContextBuilder._count_tokens(text)
        assert tokens > 0

    def test_top_n_per_modality(self):
        """每模态 top-N"""
        results = [
            MockResult(i, "text", f"text-{i}", {}, 0.9 - i * 0.1)
            for i in range(10)
        ]
        result = self.builder.build("query", results)
        # text 模态最多 5 个
        assert result["source_count"] <= 5


if __name__ == "__main__":
    import pytest
    pytest.main([__file__, "-v"])