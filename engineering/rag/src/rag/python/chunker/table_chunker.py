"""
表格切分模块 — T1/T2 混合切分策略

T1（小表格 ≤10 行）: Markdown 序列化，当作文本块处理
T2（大表格 >10 行）: 结构化元数据存储

支持 pdfplumber 增强提取（bbox 定位、大表格结构保留）。
"""

import json
import logging
from typing import Optional, Any
from dataclasses import dataclass, field

logger = logging.getLogger(__name__)


@dataclass
class TableStructure:
    """表格结构数据"""
    headers: list = field(default_factory=list)
    rows: list = field(default_factory=list)
    title: Optional[str] = None
    caption: Optional[str] = None
    bbox: Optional[tuple] = None  # (x0, y0, x1, y1)
    merges: list = field(default_factory=list)  # 合并单元格


@dataclass
class TableChunk:
    """表格块输出"""
    content: str = ""
    headers: list = field(default_factory=list)
    rows: list = field(default_factory=list)
    title: Optional[str] = None
    caption: Optional[str] = None
    row_count: int = 0
    chunk_type: str = "table"  # T1 或 T2
    embedding_content: str = ""
    metadata: dict = field(default_factory=dict)


class TableChunker:
    """
    T1/T2 混合表格切分器

    用法:
        chunker = TableChunker()
        result = chunker.chunk(table_structure, metadata)
    """

    ROW_THRESHOLD = 10  # ≤10 行用 T1，>10 行用 T2

    def chunk(self, table: TableStructure, metadata: dict = None) -> TableChunk:
        """
        执行表格切分

        Args:
            table: 表格结构数据
            metadata: 附加元数据（doc_id, page_num, source 等）

        Returns:
            TableChunk 对象
        """
        if metadata is None:
            metadata = {}

        row_count = len(table.rows)

        if row_count <= self.ROW_THRESHOLD:
            # T1: 小表格直接序列化为 Markdown
            content = self._serialize_markdown(table)
            embedding_content = content
            chunk_type = "T1"
        else:
            # T2: 大表格结构化存储
            content = json.dumps({
                "title": table.title or "",
                "headers": table.headers,
                "rows": table.rows,
                "caption": table.caption or "",
            }, ensure_ascii=False)

            # Embedding 用 caption + headers + 前 3 行
            embedding_content = self._serialize_for_embedding(table)
            chunk_type = "T2"

        return TableChunk(
            content=content,
            headers=table.headers,
            rows=table.rows,
            title=table.title,
            caption=table.caption,
            row_count=row_count,
            chunk_type=chunk_type,
            embedding_content=embedding_content,
            metadata=metadata,
        )

    def _serialize_markdown(self, table: TableStructure) -> str:
        """
        T1 小表格序列化为 Markdown 格式

        Args:
            table: 表格结构

        Returns:
            Markdown 格式的表格字符串
        """
        lines = []

        # 可选标题
        if table.title:
            lines.append(f"**{table.title}**")
            lines.append("")

        # 表头
        if table.headers:
            header_row = "| " + " | ".join(str(h) for h in table.headers) + " |"
            separator = "| " + " | ".join("---" for _ in table.headers) + " |"
            lines.append(header_row)
            lines.append(separator)

        # 数据行
        for row in table.rows:
            row_str = "| " + " | ".join(str(cell) for cell in row) + " |"
            lines.append(row_str)

        # 可选 caption
        if table.caption:
            lines.append("")
            lines.append(f"*{table.caption}*")

        return "\n".join(lines)

    def _serialize_for_embedding(self, table: TableStructure) -> str:
        """
        T2 大表格 Embedding 序列化

        使用 caption + headers + 前 3 行作为语义摘要。

        TODO(model-replacement):
        替换为以下模型的调用:
        - TAPAS (IBM): table_model = TAPASModel.from_pretrained('ibm/tapas-large')
        - TURL (CAMBRIDGE): table_model = TURLModel.from_pretrained('camembert/turl-large')
        - TaBERT (Salesforce): table_model = TaBERTModel.from_pretrained('tables TABERT')

        Args:
            table: 表格结构

        Returns:
            用于生成 Embedding 的文本
        """
        return json.dumps({
            "caption": table.caption or "",
            "headers": table.headers,
            "rows": table.rows[:3],  # 前 3 行作为语义摘要
        }, ensure_ascii=False, sort_keys=True)

    def _detect_headers(self, rows: list[list[str]]) -> tuple[list[str], list[list[str]]]:
        """
        自动识别表头行

        启发式规则:
        1. 第一行如果全是非数字 → 表头
        2. 第一行如果比第二行短很多 → 表头
        3. 第一行如果包含"序号"/"名称"/"日期"等关键词 → 表头

        Args:
            rows: 原始表格行数据

        Returns:
            (headers, data_rows) 元组
        """
        if not rows:
            return [], []

        # 如果只有一行，整行作为表头，无数据行
        if len(rows) == 1:
            return rows[0], []

        # 启发式 1: 第一行如果全是非数字 → 表头
        first_row = rows[0]
        second_row = rows[1]

        def is_numeric(val: str) -> bool:
            val = val.strip().replace(",", "").replace("%", "").replace("$", "")
            try:
                float(val)
                return True
            except ValueError:
                return False

        # 检查第一行是否全是非数字
        first_is_all_text = all(not is_numeric(cell) for cell in first_row if cell.strip())

        # 检查第二行是否包含数字
        second_has_numbers = any(is_numeric(cell) for cell in second_row if cell.strip())

        # 表头关键词
        header_keywords = {"序号", "名称", "日期", "时间", "编号", "项目", "类别",
                          "类型", "姓名", "地址", "说明", "备注", "描述",
                          "ID", "Name", "Date", "Type", "Category", "Description"}

        first_has_keyword = any(
            any(kw in cell for kw in header_keywords)
            for cell in first_row
        )

        if (first_is_all_text and second_has_numbers) or first_has_keyword:
            return first_row, rows[1:]

        # 默认：第一行是表头
        return first_row, rows[1:]

    @staticmethod
    def enhance_with_pdfplumber(page, bbox: tuple) -> Optional[TableStructure]:
        """
        使用 pdfplumber 增强表格提取

        从 PDF 页面中精确提取表格结构，包括 bbox 定位、合并单元格识别。

        Args:
            page: pdfplumber 页面对象
            bbox: 边界框 (x0, y0, x1, y1)

        Returns:
            TableStructure 对象，失败返回 None
        """
        try:
            tables = page.extract_tables()
            if not tables:
                return None

            table_data = tables[0]
            if not table_data or len(table_data) < 1:
                return None

            headers = table_data[0]
            rows = table_data[1:] if len(table_data) > 1 else []

            # 识别合并单元格
            merges = []
            try:
                # pdfplumber 的 find_tables 方法提供更详细的表格信息
                found_tables = page.find_tables()
                if found_tables:
                    raw_table = found_tables[0]
                    merges = [list(m) for m in getattr(raw_table, "merges", [])]
            except Exception as e:
                logger.debug(f"合并单元格识别失败: {e}")

            return TableStructure(
                headers=headers,
                rows=rows,
                bbox=bbox,
                merges=merges,
            )

        except Exception as e:
            logger.warning(f"pdfplumber 表格提取失败: {e}")
            return None

    @staticmethod
    def detect_merged_cells(page, bbox: tuple) -> list:
        """
        检测表格中的合并单元格

        Args:
            page: pdfplumber 页面对象
            bbox: 边界框

        Returns:
            合并单元格列表
        """
        try:
            tables = page.find_tables()
            if tables:
                return [list(m) for m in getattr(tables[0], "merges", [])]
        except Exception as e:
            logger.debug(f"合并单元格检测失败: {e}")
        return []