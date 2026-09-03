"""
代码切分器单元测试

测试 CodeChunker 的 Tree-sitter 语义切分、多语言支持等功能。
"""

import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "src", "rag", "python"))

from chunker.code_chunker import CodeChunker, CodeChunk


class TestCodeChunker:
    """CodeChunker 单元测试"""

    def setup_method(self):
        self.chunker = CodeChunker()

    def test_detect_language_python(self):
        """Python 语言检测"""
        assert self.chunker._detect_language("py") == "python"

    def test_detect_language_javascript(self):
        """JavaScript 语言检测"""
        assert self.chunker._detect_language("js") == "javascript"

    def test_detect_language_c(self):
        """C 语言检测"""
        assert self.chunker._detect_language("c") == "c"

    def test_detect_language_cpp(self):
        """C++ 语言检测"""
        assert self.chunker._detect_language("cpp") == "cpp"

    def test_detect_language_go(self):
        """Go 语言检测"""
        assert self.chunker._detect_language("go") == "go"

    def test_detect_language_rust(self):
        """Rust 语言检测"""
        assert self.chunker._detect_language("rs") == "rust"

    def test_detect_language_unknown(self):
        """未知扩展名返回 plaintext"""
        assert self.chunker._detect_language("txt") == "plaintext"

    def test_chunk_plaintext_skipped(self):
        """非代码文件跳过"""
        chunks = self.chunker.chunk("some text", "readme.txt")
        assert len(chunks) == 0

    def test_chunk_small_python(self):
        """小段 Python 代码整体作为 chunk"""
        code = "x = 1\ny = 2\n"
        chunks = self.chunker.chunk(code, "test.py")
        assert len(chunks) > 0
        assert chunks[0].language == "python"

    def test_function_types_mapping(self):
        """函数类型映射完整性"""
        expected = {"python", "javascript", "c", "cpp", "go", "rust", "java"}
        actual = set(CodeChunker.FUNCTION_TYPES.keys())
        assert actual == expected

    def test_language_extensions_mapping(self):
        """语言扩展名映射完整性"""
        assert "py" in CodeChunker.LANGUAGE_EXTENSIONS
        assert "js" in CodeChunker.LANGUAGE_EXTENSIONS
        assert "c" in CodeChunker.LANGUAGE_EXTENSIONS
        assert "cpp" in CodeChunker.LANGUAGE_EXTENSIONS
        assert "go" in CodeChunker.LANGUAGE_EXTENSIONS
        assert "rs" in CodeChunker.LANGUAGE_EXTENSIONS
        assert "java" in CodeChunker.LANGUAGE_EXTENSIONS


class TestCodeChunk:
    """CodeChunk 数据模型测试"""

    def test_create_chunk(self):
        """创建 CodeChunk"""
        chunk = CodeChunk(
            content="def foo(): pass",
            language="python",
            name="foo",
            docstring='"""A test function"""',
            start_line=1,
            end_line=1,
        )
        assert chunk.content == "def foo(): pass"
        assert chunk.language == "python"
        assert chunk.name == "foo"

    def test_chunk_with_metadata(self):
        """CodeChunk 携带 metadata"""
        chunk = CodeChunk(
            content="print('hello')",
            language="python",
            metadata={"source": "test.py"},
        )
        assert chunk.metadata["source"] == "test.py"


if __name__ == "__main__":
    import pytest
    pytest.main([__file__, "-v"])