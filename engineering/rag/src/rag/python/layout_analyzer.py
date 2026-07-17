"""
PDF 布局分析模块

提取 PDF 的布局元数据：标题层级、表格/图像引用关系、页码、列布局等。
"""

import logging
from typing import Optional

logger = logging.getLogger(__name__)


class LayoutMetadata:
    """布局元数据"""
    def __init__(
        self,
        section_path: str = "",
        heading: str = "",
        level: int = 0,
        references: dict = None,
        page_num: int = 0,
        column_layout: str = "single",
    ):
        self.section_path = section_path   # "第3章 / 3.1节 / 第二段"
        self.heading = heading             # 所属标题
        self.level = level                 # 标题层级
        self.references = references or {} # {"tables": ["表3-1"], "figures": ["图3-2"]}
        self.page_num = page_num
        self.column_layout = column_layout


class LayoutAnalyzer:
    """
    PDF 布局分析器

    从 unstructured.io 解析结果中提取布局元数据。

    用法:
        analyzer = LayoutAnalyzer()
        meta = analyzer.analyze(element, current_section)
    """

    HEADING_KEYWORDS = {"Title", "Heading", "Subheading", "Section-header"}

    def __init__(self):
        self._current_section = ""
        self._heading_stack = []  # [(heading, level), ...]

    def analyze(self, element, current_section: str = "") -> LayoutMetadata:
        """
        分析单个元素的布局元数据

        Args:
            element: unstructured.io 的 Element 对象
            current_section: 当前章节路径

        Returns:
            LayoutMetadata 对象
        """
        self._current_section = current_section

        element_type = getattr(element, "category", "Unknown")

        # 提取标题层级
        heading = ""
        level = 0
        if element_type in self.HEADING_KEYWORDS:
            heading = str(element.text)[:200]  # 截断长标题
            level = self._detect_heading_level(element_type, element)
            self._update_heading_stack(heading, level)
            section_path = self._build_section_path()
        else:
            section_path = self._build_section_path()

        # 提取引用关系
        references = self._extract_references(element)

        # 提取页码
        page_num = self._extract_page_number(element)

        return LayoutMetadata(
            section_path=section_path,
            heading=heading,
            level=level,
            references=references,
            page_num=page_num,
        )

    def _detect_heading_level(self, element_type: str, element) -> int:
        """
        检测标题层级

        Args:
            element_type: 元素类型
            element: 元素对象

        Returns:
            标题层级 (1-6)
        """
        # 从元素 metadata 获取
        metadata = getattr(element, "metadata", {}) or {}
        if hasattr(metadata, "get"):
            level = metadata.get("heading_level", 0)
            if level:
                return level

        # 从元素类型推断
        level_map = {
            "Title": 1,
            "Heading": 2,
            "Subheading": 3,
            "Section-header": 4,
        }
        return level_map.get(element_type, 2)

    def _update_heading_stack(self, heading: str, level: int):
        """
        更新标题栈

        当遇到更低层级的标题时，弹出栈顶；遇到更高层级时追加。

        Args:
            heading: 标题文本
            level: 标题层级
        """
        while self._heading_stack and self._heading_stack[-1][1] >= level:
            self._heading_stack.pop()
        self._heading_stack.append((heading, level))

    def _build_section_path(self) -> str:
        """
        构建章节路径

        Returns:
            "第3章 / 3.1节 / 第二段"
        """
        if not self._heading_stack:
            return ""
        return " / ".join(h for h, _ in self._heading_stack)

    def _extract_references(self, element) -> dict:
        """
        提取表格/图像引用关系

        从元素文本中检测 "如表 X 所示"、"如图 Y 所示" 等引用模式。

        Args:
            element: 元素对象

        Returns:
            {"tables": ["表3-1"], "figures": ["图3-2"]}
        """
        text = str(getattr(element, "text", ""))

        import re
        references = {"tables": [], "figures": []}

        # 匹配 "表 X"、"表X-Y"
        table_refs = re.findall(r'[表][\s]*[\d]+[-]?[\d]*', text)
        for ref in table_refs:
            ref = ref.strip()
            if ref not in references["tables"]:
                references["tables"].append(ref)

        # 匹配 "图 X"、"图X-Y"
        figure_refs = re.findall(r'[图][\s]*[\d]+[-]?[\d]*', text)
        for ref in figure_refs:
            ref = ref.strip()
            if ref not in references["figures"]:
                references["figures"].append(ref)

        return references

    @staticmethod
    def _extract_page_number(element) -> int:
        """
        提取页码

        Args:
            element: 元素对象

        Returns:
            页码
        """
        metadata = getattr(element, "metadata", {}) or {}
        if hasattr(metadata, "get"):
            page = metadata.get("page_number", 0)
            if page:
                return int(page)

        # 尝试从 coordinates 中推断
        coordinates = getattr(element, "metadata", {}).get("coordinates", None)
        if coordinates:
            return int(getattr(coordinates, "page_number", 0))

        return 0


class MetadataEnricher:
    """
    布局元数据注入器

    将布局分析结果注入到 chunk 的 metadata 中。
    """

    @staticmethod
    def enrich(chunk: dict, layout: LayoutMetadata) -> dict:
        """
        注入布局信息到 chunk metadata

        Args:
            chunk: chunk 数据字典
            layout: 布局元数据

        Returns:
            增强后的 chunk
        """
        if "metadata" not in chunk:
            chunk["metadata"] = {}

        chunk["metadata"]["section_path"] = layout.section_path
        chunk["metadata"]["heading"] = layout.heading
        chunk["metadata"]["level"] = layout.level
        chunk["metadata"]["references"] = layout.references
        chunk["metadata"]["page_num"] = layout.page_num

        return chunk

    @staticmethod
    def filter_by_section(
        results: list,
        section_path: str,
    ) -> list:
        """
        按章节路径过滤检索结果

        Args:
            results: 检索结果列表
            section_path: 章节路径，如 "第3章"

        Returns:
            过滤后的结果列表
        """
        return [
            r for r in results
            if section_path in (getattr(r, "metadata", {}) or {}).get("section_path", "")
        ]