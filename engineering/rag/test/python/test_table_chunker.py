"""
表格切分器单元测试

测试 TableChunker 的 T1/T2 切分、表头识别、Markdown 序列化等功能。
"""

import json
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "src", "rag", "python"))

from chunker.table_chunker import TableChunker, TableStructure, TableChunk


class TestTableChunker:
    """TableChunker 单元测试"""

    def setup_method(self):
        self.chunker = TableChunker()

    def test_t1_small_table(self):
        """T1: 小表格 (≤10行) → Markdown 序列化"""
        table = TableStructure(
            headers=["季度", "销售额", "同比增长"],
            rows=[
                ["Q1", "1000万", "15%"],
                ["Q2", "1200万", "20%"],
                ["Q3", "1500万", "25%"],
            ],
            title="2024年销售数据",
            caption="季度销售统计表",
        )

        result = self.chunker.chunk(table)

        assert result.chunk_type == "T1"
        assert result.row_count == 3
        assert "季度" in result.content
        assert "销售额" in result.content
        assert result.title == "2024年销售数据"
        assert result.caption == "季度销售统计表"
        assert result.embedding_content == result.content  # T1 直接用内容

    def test_t2_large_table(self):
        """T2: 大表格 (>10行) → 结构化存储"""
        rows = [[f"行{i}", str(i * 100), f"{i}%"] for i in range(15)]

        table = TableStructure(
            headers=["项目", "金额", "占比"],
            rows=rows,
            title="年度预算明细表",
        )

        result = self.chunker.chunk(table)

        assert result.chunk_type == "T2"
        assert result.row_count == 15
        # T2 内容为 JSON
        parsed = json.loads(result.content)
        assert parsed["title"] == "年度预算明细表"
        assert len(parsed["rows"]) == 15

        # Embedding 内容只包含前 3 行
        emb = json.loads(result.embedding_content)
        assert len(emb["rows"]) == 3

    def test_t2_embedding_content(self):
        """T2 的 embedding_content 包含 caption + headers + 前3行"""
        rows = [[f"行{i}", str(i)] for i in range(20)]

        table = TableStructure(
            headers=["名称", "值"],
            rows=rows,
            caption="测试表",
        )

        result = self.chunker.chunk(table)

        emb = json.loads(result.embedding_content)
        assert emb["caption"] == "测试表"
        assert emb["headers"] == ["名称", "值"]
        assert len(emb["rows"]) == 3
        assert emb["rows"][0] == ["行0", "0"]

    def test_serialize_markdown_with_title(self):
        """Markdown 序列化含标题"""
        table = TableStructure(
            headers=["A", "B"],
            rows=[["1", "2"]],
            title="测试标题",
        )

        md = self.chunker._serialize_markdown(table)
        assert "**测试标题**" in md
        assert "| A | B |" in md
        assert "| 1 | 2 |" in md

    def test_serialize_markdown_with_caption(self):
        """Markdown 序列化含 caption"""
        table = TableStructure(
            headers=["X"],
            rows=[["Y"]],
            caption="说明文字",
        )

        md = self.chunker._serialize_markdown(table)
        assert "*说明文字*" in md

    def test_serialize_markdown_no_headers(self):
        """无表头时的 Markdown 序列化"""
        table = TableStructure(rows=[["a", "b"], ["c", "d"]])

        md = self.chunker._serialize_markdown(table)
        # 无表头时只输出数据行
        assert "| a | b |" in md
        assert "| c | d |" in md

    def test_detect_headers_numeric_first_row(self):
        """第一行全数字 → 默认第一行是表头"""
        rows = [
            ["100", "200", "300"],
            ["400", "500", "600"],
        ]
        headers, data = self.chunker._detect_headers(rows)
        assert headers == ["100", "200", "300"]

    def test_detect_headers_text_first_row(self):
        """第一行全文本，第二行含数字 → 第一行是表头"""
        rows = [
            ["名称", "数量", "价格"],
            ["苹果", "100", "5.5"],
            ["香蕉", "200", "3.0"],
        ]
        headers, data = self.chunker._detect_headers(rows)
        assert headers == ["名称", "数量", "价格"]
        assert len(data) == 2

    def test_detect_headers_keyword(self):
        """第一行含表头关键词 → 是表头"""
        rows = [
            ["序号", "姓名", "日期"],
            ["1", "张三", "2024-01-01"],
            ["2", "李四", "2024-01-02"],
        ]
        headers, data = self.chunker._detect_headers(rows)
        assert headers == ["序号", "姓名", "日期"]

    def test_detect_headers_single_row(self):
        """只有一行 → 整行作为表头"""
        rows = [["标题1", "标题2"]]
        headers, data = self.chunker._detect_headers(rows)
        assert headers == ["标题1", "标题2"]
        assert data == []

    def test_empty_rows(self):
        """空行 → 返回空"""
        headers, data = self.chunker._detect_headers([])
        assert headers == []
        assert data == []

    def test_embedding_content_annotation(self):
        """T2 的 _serialize_for_embedding 包含可替换模型的注释"""
        doc = self.chunker._serialize_for_embedding.__doc__
        assert doc is not None
        assert "TAPAS" in doc
        assert "TURL" in doc
        assert "TaBERT" in doc

    def test_row_threshold_constant(self):
        """ROW_THRESHOLD 常量应为 10"""
        assert TableChunker.ROW_THRESHOLD == 10

    def test_chunk_with_metadata(self):
        """chunk 时传入 metadata"""
        table = TableStructure(
            headers=["A"],
            rows=[["1"], ["2"]],
        )
        result = self.chunker.chunk(table, metadata={"doc_id": 123, "page_num": 5})

        assert result.metadata["doc_id"] == 123
        assert result.metadata["page_num"] == 5

    def test_t1_edge_10_rows(self):
        """正好 10 行 → T1"""
        rows = [[f"行{i}", str(i)] for i in range(10)]
        table = TableStructure(headers=["名称", "值"], rows=rows)
        result = self.chunker.chunk(table)
        assert result.chunk_type == "T1"

    def test_t2_edge_11_rows(self):
        """11 行 → T2"""
        rows = [[f"行{i}", str(i)] for i in range(11)]
        table = TableStructure(headers=["名称", "值"], rows=rows)
        result = self.chunker.chunk(table)
        assert result.chunk_type == "T2"


if __name__ == "__main__":
    import pytest
    pytest.main([__file__, "-v"])