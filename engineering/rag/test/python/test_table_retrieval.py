"""
检索层集成测试：测试 RRF 融合、表格结构化过滤、多模态检索器
"""

import sys
import os
import json

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "src", "rag", "python"))

from retrieval import MultimodalRetriever, TableStructuredFilter, SearchResult


class TestTableStructuredFilter:
    """TableStructuredFilter 单元测试"""

    def setup_method(self):
        self.filter = TableStructuredFilter(boost_factor=1.2)

    def test_filter_matched_headers(self):
        """匹配到 header 时提升分数"""
        results = [
            SearchResult(
                chunk_id=1, chunk_type="table",
                content="", metadata={"headers": ["季度", "销售额"]},
                score=0.7,
            ),
            SearchResult(
                chunk_id=2, chunk_type="table",
                content="", metadata={"headers": ["名称", "描述"]},
                score=0.8,
            ),
        ]
        filtered = self.filter.filter(results, "Q2销售额")
        # 第一个表匹配了"销售额"关键词
        assert len(filtered) == 1
        assert filtered[0].chunk_id == 1
        assert filtered[0].score == 0.7 * 1.2
        assert "matched_headers" in filtered[0].metadata

    def test_filter_no_keywords(self):
        """无关键词时不过滤"""
        results = [
            SearchResult(
                chunk_id=1, chunk_type="table",
                content="", metadata={"headers": ["A", "B"]},
                score=0.5,
            ),
        ]
        filtered = self.filter.filter(results, "测试")
        assert len(filtered) == 1

    def test_filter_non_table(self):
        """非表格类型不过滤"""
        results = [
            SearchResult(chunk_id=1, chunk_type="text", content="hello", score=0.9),
        ]
        filtered = self.filter.filter(results, "销售额")
        assert len(filtered) == 1

    def test_extract_query_entities(self):
        """提取查询中的关键实体"""
        keywords = self.filter._extract_query_entities("Q2的销售额是多少")
        assert "销售额" in keywords

    def test_extract_query_entities_no_match(self):
        """无匹配关键词时返回空列表"""
        keywords = self.filter._extract_query_entities("今天天气怎么样")
        assert keywords == []


class TestMultimodalRetriever:
    """MultimodalRetriever 单元测试"""

    def setup_method(self):
        self.retriever = MultimodalRetriever()

    def test_default_rrf_weights(self):
        """默认 RRF 权重"""
        assert self.retriever.rrf_weights["text"] == 0.5
        assert self.retriever.rrf_weights["table"] == 0.25
        assert self.retriever.rrf_weights["image"] == 0.15
        assert self.retriever.rrf_weights["code"] == 0.1

    def test_custom_rrf_weights(self):
        """自定义 RRF 权重"""
        weights = {"text": 0.4, "table": 0.3, "image": 0.2, "code": 0.1}
        retriever = MultimodalRetriever(rrf_weights=weights)
        assert retriever.rrf_weights["text"] == 0.4
        assert retriever.rrf_weights["table"] == 0.3

    def test_dedupe_and_sort(self):
        """去重 + 排序"""
        results = [
            SearchResult(chunk_id=2, chunk_type="text", content="b", score=0.5),
            SearchResult(chunk_id=1, chunk_type="text", content="a", score=0.9),
            SearchResult(chunk_id=2, chunk_type="text", content="b", score=0.5),  # 重复
        ]
        deduped = MultimodalRetriever._dedupe_and_sort(results)
        assert len(deduped) == 2
        assert deduped[0].chunk_id == 1  # 最高分
        assert deduped[1].chunk_id == 2

    def test_rrf_fusion_single_modality(self):
        """单模态 RRF 融合"""
        results = {
            "text": [
                (SearchResult(1, "text", "a"), 0.9),
                (SearchResult(2, "text", "b"), 0.8),
            ]
        }
        fused = self.retriever._rrf_fusion(results)
        assert len(fused) == 2
        assert fused[0].score > 0  # 应计算了 RRF 分数

    def test_rrf_fusion_multi_modality(self):
        """多模态 RRF 融合"""
        results = {
            "text": [
                (SearchResult(1, "text", "a"), 0.9),
            ],
            "table": [
                (SearchResult(1, "table", "a"), 0.7),  # 相同 chunk_id
                (SearchResult(3, "table", "c"), 0.6),
            ],
        }
        fused = self.retriever._rrf_fusion(results)
        # chunk_id=1 在两个模态中都出现，分数应更高
        r1 = [r for r in fused if r.chunk_id == 1]
        assert len(r1) == 1
        assert len(r1[0].modalities) == 2  # 同时出现在 text 和 table

    def test_retrieve_no_indices(self):
        """无索引时检索返回空列表"""
        import asyncio
        import numpy as np
        results = asyncio.run(
            self.retriever.retrieve(1, "test", np.array([0.1, 0.2]))
        )
        assert results == []


if __name__ == "__main__":
    import pytest
    pytest.main([__file__, "-v"])