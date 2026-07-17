"""
布局分析器单元测试
"""

import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "src", "rag", "python"))

from layout_analyzer import LayoutAnalyzer, LayoutMetadata, MetadataEnricher


class MockElement:
    """模拟 unstructured.io 元素"""
    def __init__(self, category="NarrativeText", text="", page_number=1, heading_level=0):
        self.category = category
        self.text = text
        self.metadata = {"page_number": page_number}
        if heading_level:
            self.metadata["heading_level"] = heading_level


class TestLayoutAnalyzer:
    """LayoutAnalyzer 单元测试"""

    def setup_method(self):
        self.analyzer = LayoutAnalyzer()

    def test_analyze_narrative(self):
        """分析普通文本"""
        element = MockElement("NarrativeText", "这是一段普通文本", page_number=3)
        meta = self.analyzer.analyze(element)
        assert meta.page_num == 3
        assert meta.level == 0
        assert meta.heading == ""

    def test_analyze_heading(self):
        """分析标题"""
        element = MockElement("Heading", "第3章 架构设计", page_number=5, heading_level=2)
        meta = self.analyzer.analyze(element)
        assert meta.heading == "第3章 架构设计"
        assert meta.level == 2
        assert "第3章 架构设计" in meta.section_path

    def test_analyze_with_references(self):
        """分析含引用的文本"""
        element = MockElement("NarrativeText", "如表3-1所示，这个数据如图3-2所示。")
        meta = self.analyzer.analyze(element)
        assert "表3-1" in meta.references["tables"]
        assert "图3-2" in meta.references["figures"]

    def test_heading_stack_management(self):
        """标题栈管理"""
        analyzer = LayoutAnalyzer()

        e1 = MockElement("Heading", "第1章", heading_level=1)
        meta1 = analyzer.analyze(e1)
        assert meta1.level == 1

        e2 = MockElement("Heading", "1.1 节", heading_level=2)
        meta2 = analyzer.analyze(e2)
        assert "1.1 节" in meta2.section_path

    def test_extract_references_table_only(self):
        """仅表格引用"""
        element = MockElement("NarrativeText", "参见表5-1")
        meta = self.analyzer.analyze(element)
        assert "表5-1" in meta.references["tables"]
        assert len(meta.references["figures"]) == 0

    def test_extract_references_figure_only(self):
        """仅图像引用"""
        element = MockElement("NarrativeText", "如图2-3所示")
        meta = self.analyzer.analyze(element)
        assert "图2-3" in meta.references["figures"]
        assert len(meta.references["tables"]) == 0


class TestMetadataEnricher:
    """MetadataEnricher 单元测试"""

    def test_enrich_chunk(self):
        """注入布局信息"""
        layout = LayoutMetadata(
            section_path="第3章 / 3.1节",
            heading="架构设计",
            level=2,
            references={"tables": ["表3-1"]},
            page_num=15,
        )
        chunk = {"content": "测试", "metadata": {}}
        result = MetadataEnricher.enrich(chunk, layout)
        assert result["metadata"]["section_path"] == "第3章 / 3.1节"
        assert result["metadata"]["heading"] == "架构设计"
        assert result["metadata"]["page_num"] == 15

    def test_filter_by_section(self):
        """按章节过滤"""
        results = [
            type("R", (), {"metadata": {"section_path": "第3章 / 3.1节"}})(),
            type("R", (), {"metadata": {"section_path": "第4章 / 4.1节"}})(),
            type("R", (), {"metadata": {"section_path": "第3章 / 3.2节"}})(),
        ]
        filtered = MetadataEnricher.filter_by_section(results, "第3章")
        assert len(filtered) == 2


if __name__ == "__main__":
    import pytest
    pytest.main([__file__, "-v"])