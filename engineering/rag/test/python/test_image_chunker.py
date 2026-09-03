"""
图像切分器单元测试

测试 ImageChunker 的 OCR、Caption、组合内容等功能。
"""

import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "src", "rag", "python"))

from chunker.image_chunker import ImageChunker, ImageChunk


class MockSigLIP:
    """模拟 SigLIP 服务"""
    def encode(self, path):
        import numpy as np
        return np.random.randn(768).astype(np.float32)


class MockOCREngine:
    """模拟 OCR 引擎"""
    def extract(self, path):
        return "这是一段 OCR 提取的文字"


class MockCaptionModel:
    """模拟 Caption 模型"""
    def generate(self, path):
        return "一张测试图片的描述"


class TestImageChunker:
    """ImageChunker 单元测试"""

    def setup_method(self):
        self.siglip = MockSigLIP()
        self.ocr = MockOCREngine()
        self.caption = MockCaptionModel()
        self.chunker = ImageChunker(self.siglip, self.ocr, self.caption)

    def test_chunk_with_all_services(self):
        """全服务可用时正常处理"""
        result = self.chunker.chunk(
            "/path/to/test.jpg",
            metadata={"page_num": 3, "source": "test.pdf"}
        )

        assert result.image_path == "/path/to/test.jpg"
        assert result.siglip_embedding is not None
        assert result.siglip_embedding.shape[0] == 768
        assert result.caption is not None
        assert result.ocr_text is not None
        assert result.metadata["has_ocr"] is True
        assert result.metadata["has_caption"] is True
        assert result.metadata["has_siglip"] is True
        assert result.metadata["page_num"] == 3

    def test_chunk_no_services(self):
        """无服务时返回空 chunk"""
        chunker = ImageChunker()
        result = chunker.chunk("/path/to/nonexistent.jpg")

        assert result.image_path == "/path/to/nonexistent.jpg"
        assert result.siglip_embedding is None
        assert result.ocr_text is None
        assert result.caption is None
        assert "error" in result.metadata

    def test_combine_content_both(self):
        """OCR + Caption 都存在时组合"""
        combined = ImageChunker._combine_content("OCR文字", "Caption内容")
        assert combined is not None
        assert "OCR文字" in combined
        assert "Caption内容" in combined

    def test_combine_content_only_ocr(self):
        """只有 OCR"""
        combined = ImageChunker._combine_content("OCR文字", None)
        assert combined is not None
        assert "OCR文字" in combined

    def test_combine_content_only_caption(self):
        """只有 Caption"""
        combined = ImageChunker._combine_content(None, "Caption内容")
        assert combined is not None
        assert "Caption内容" in combined

    def test_combine_content_none(self):
        """两者都无返回 None"""
        combined = ImageChunker._combine_content(None, None)
        assert combined is None

    def test_chunk_with_metadata(self):
        """传入 metadata"""
        result = self.chunker.chunk(
            "/path/to/test.jpg",
            metadata={"doc_id": 42, "page_num": 5}
        )
        assert result.metadata["doc_id"] == 42
        assert result.metadata["page_num"] == 5


if __name__ == "__main__":
    import pytest
    pytest.main([__file__, "-v"])