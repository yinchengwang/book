"""
增量更新集成测试
"""

import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "src", "rag", "python"))

from incremental_updater import compute_content_hash, IncrementalUpdater, UpdateResult


class MockChunk:
    """模拟 chunk 对象"""
    def __init__(self, content="", chunk_type="text", headers=None, rows=None,
                 title=None, caption=None, image_path="", language="",
                 name=None, code=None, metadata=None):
        self.content = content
        self.chunk_type = chunk_type
        self.type = chunk_type
        self.headers = headers or []
        self.rows = rows or []
        self.title = title
        self.caption = caption
        self.image_path = image_path
        self.language = language
        self.name = name
        self.code = code
        self.metadata = metadata or {}


class TestComputeContentHash:
    """compute_content_hash 单元测试"""

    def test_hash_text(self):
        """文本 hash"""
        chunk = MockChunk(content="Hello World", chunk_type="text")
        h = compute_content_hash(chunk)
        assert len(h) == 64  # SHA256 hex
        assert isinstance(h, str)

    def test_hash_table(self):
        """表格 hash"""
        chunk = MockChunk(
            chunk_type="table",
            headers=["A", "B"],
            rows=[["1", "2"]],
            title="Test",
        )
        h = compute_content_hash(chunk)
        assert len(h) == 64

    def test_hash_code(self):
        """代码 hash"""
        chunk = MockChunk(
            chunk_type="code",
            language="python",
            name="hello",
            code="def hello(): pass",
        )
        h = compute_content_hash(chunk)
        assert len(h) == 64

    def test_hash_deterministic(self):
        """相同内容 hash 一致"""
        c1 = MockChunk(content="test", chunk_type="text")
        c2 = MockChunk(content="test", chunk_type="text")
        assert compute_content_hash(c1) == compute_content_hash(c2)

    def test_hash_different(self):
        """不同内容 hash 不同"""
        c1 = MockChunk(content="test1", chunk_type="text")
        c2 = MockChunk(content="test2", chunk_type="text")
        assert compute_content_hash(c1) != compute_content_hash(c2)


class TestUpdateResult:
    """UpdateResult 测试"""

    def test_create(self):
        """创建结果"""
        result = UpdateResult(added=5, removed=2, updated=1)
        assert result.added == 5
        assert result.removed == 2
        assert result.updated == 1

    def test_empty(self):
        """空结果"""
        result = UpdateResult()
        assert result.added == 0


class TestIncrementalUpdater:
    """IncrementalUpdater 单元测试"""

    def setup_method(self):
        self.updater = IncrementalUpdater()

    def test_fuzzy_match_high_similarity(self):
        """高相似度匹配"""
        old = [{"content": "def hello(): pass", "chunk_id": 1}]
        new = [MockChunk(content="def hello(): return 42", chunk_type="text")]
        matches = IncrementalUpdater._fuzzy_match(old, new)
        assert len(matches) > 0

    def test_fuzzy_match_low_similarity(self):
        """低相似度不匹配"""
        old = [{"content": "abc", "chunk_id": 1}]
        new = [MockChunk(content="xyz", chunk_type="text")]
        matches = IncrementalUpdater._fuzzy_match(old, new)
        assert len(matches) == 0

    def test_text_similarity_identical(self):
        """完全相同文本"""
        score = IncrementalUpdater._text_similarity("hello world", "hello world")
        assert score == 1.0

    def test_text_similarity_empty(self):
        """空文本"""
        score = IncrementalUpdater._text_similarity("", "hello")
        assert score == 0.0


if __name__ == "__main__":
    import pytest
    pytest.main([__file__, "-v"])