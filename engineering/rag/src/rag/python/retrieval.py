"""
检索层组件：RRF 融合、表格结构化过滤、多模态检索器

提供 MultimodalRetriever 类，实现多路向量召回 + RRF 加权融合。
"""

import asyncio
import logging
from typing import Optional, Any
from collections import defaultdict

import numpy as np

logger = logging.getLogger(__name__)


class SearchResult:
    """检索结果"""
    def __init__(
        self,
        chunk_id: int,
        chunk_type: str,
        content: str,
        metadata: dict = None,
        score: float = 0.0,
    ):
        self.chunk_id = chunk_id
        self.chunk_type = chunk_type
        self.content = content
        self.metadata = metadata or {}
        self.score = score
        self.modalities = []  # 命中的模态列表


class TableStructuredFilter:
    """
    表格结构化过滤

    在召回的表格结果上做 headers 匹配过滤，
    提升表格检索的精确度。
    """

    HEADER_KEYWORDS = {
        "季度", "月份", "日期", "年份",
        "销售额", "收入", "利润", "成本", "增长率",
        "名称", "项目", "类别", "类型",
        "数量", "金额", "占比", "百分比",
    }

    def __init__(self, boost_factor: float = 1.2):
        self.boost_factor = boost_factor

    def filter(self, results: list[SearchResult], query: str) -> list[SearchResult]:
        """
        过滤并提升匹配到的表格结果

        Args:
            results: 原始检索结果列表
            query: 用户查询文本

        Returns:
            过滤后的结果列表
        """
        keywords = self._extract_query_entities(query)

        filtered = []
        for result in results:
            if result.chunk_type != "table":
                filtered.append(result)
                continue

            table_headers = result.metadata.get("headers", [])

            # 检查是否有 header 匹配
            matched_headers = []
            for kw in keywords:
                for h in table_headers:
                    if isinstance(h, str) and kw.lower() in h.lower():
                        matched_headers.append(h)

            if matched_headers:
                # 匹配到的表格提升分数
                result.score *= self.boost_factor
                result.metadata["matched_headers"] = matched_headers
                filtered.append(result)
            elif not keywords:
                # 没有关键词时不过滤
                filtered.append(result)
            # 没有匹配 header 且有关键词时，过滤掉

        return filtered

    def _extract_query_entities(self, query: str) -> list[str]:
        """
        从查询中提取关键实体

        Args:
            query: 用户查询文本

        Returns:
            关键实体列表
        """
        # 简单的关键词提取：按空格分词 + 匹配预定义关键词
        words = set()
        for char in "，。！？、；：""''（）":
            query = query.replace(char, " ")

        for word in query.split():
            word = word.strip()
            if not word:
                continue
            # 匹配预定义关键词
            if any(kw in word for kw in self.HEADER_KEYWORDS):
                words.add(word)

        return list(words)


class MultimodalRetriever:
    """
    多模态检索器

    多路召回 + RRF 加权融合，支持文本/表格/图像/代码四路召回。
    """

    # RRF 默认权重
    DEFAULT_RRF_WEIGHTS = {
        "text": 0.5,
        "table": 0.25,
        "image": 0.15,
        "code": 0.1,
    }

    def __init__(
        self,
        text_index=None,
        table_index=None,
        image_index=None,
        code_index=None,
        rrf_weights: dict = None,
    ):
        self.text_index = text_index
        self.table_index = table_index
        self.image_index = image_index
        self.code_index = code_index
        self.rrf_weights = rrf_weights or self.DEFAULT_RRF_WEIGHTS.copy()
        self.table_filter = TableStructuredFilter()

    async def retrieve(
        self,
        tenant_id: int,
        query: str,
        query_embedding: np.ndarray,
        target_modalities: list = None,
    ) -> list[SearchResult]:
        """
        多路召回 + RRF 融合

        Args:
            tenant_id: 租户 ID
            query: 查询文本
            query_embedding: 查询向量
            target_modalities: 目标模态列表，默认全部

        Returns:
            融合后的检索结果列表
        """
        if target_modalities is None:
            target_modalities = ["text", "table", "image", "code"]

        # 1. 异步并发召回
        tasks = []
        modality_map = {}

        if "text" in target_modalities:
            tasks.append(self._recall_text(query_embedding, tenant_id))
            modality_map[len(tasks) - 1] = "text"

        if "table" in target_modalities:
            tasks.append(self._recall_table(query_embedding, query, tenant_id))
            modality_map[len(tasks) - 1] = "table"

        if "image" in target_modalities:
            tasks.append(self._recall_image(query_embedding, query, tenant_id))
            modality_map[len(tasks) - 1] = "image"

        if "code" in target_modalities:
            tasks.append(self._recall_code(query_embedding, tenant_id))
            modality_map[len(tasks) - 1] = "code"

        if not tasks:
            return []

        results_by_modality = await asyncio.gather(*tasks)

        # 2. 组装按模态分组的 results
        modality_results = {}
        for idx, results in enumerate(results_by_modality):
            modality = modality_map[idx]
            modality_results[modality] = results

        # 3. RRF 融合
        fused_results = self._rrf_fusion(modality_results)

        # 4. 去重 + 排序
        return self._dedupe_and_sort(fused_results)

    async def _recall_text(
        self, query_emb: np.ndarray, tenant_id: int
    ) -> list[tuple[SearchResult, float]]:
        """文本向量召回"""
        if not self.text_index:
            logger.warning("text_index 未配置")
            return []
        # TODO: 实际调用向量索引搜索
        return []

    async def _recall_table(
        self, query_emb: np.ndarray, query: str, tenant_id: int
    ) -> list[tuple[SearchResult, float]]:
        """表格向量召回"""
        if not self.table_index:
            logger.warning("table_index 未配置")
            return []

        # 1. 向量检索
        results = []  # TODO: 实际调用 table_index.search()

        # 2. 结构化过滤
        search_results = [
            SearchResult(
                chunk_id=r[0],
                chunk_type="table",
                content=r[1],
                metadata=r[2] if len(r) > 2 else {},
                score=r[3] if len(r) > 3 else 0.0,
            )
            for r in results
        ]
        filtered = self.table_filter.filter(search_results, query)

        return [(r, r.score) for r in filtered]

    async def _recall_image(
        self, query_emb: np.ndarray, query: str, tenant_id: int
    ) -> list[tuple[SearchResult, float]]:
        """图像向量召回（SigLIP + 文本双路）"""
        if not self.image_index:
            logger.warning("image_index 未配置")
            return []
        # TODO: 实际调用 image_index.search() 双路召回
        return []

    async def _recall_code(
        self, query_emb: np.ndarray, tenant_id: int
    ) -> list[tuple[SearchResult, float]]:
        """代码向量召回"""
        if not self.code_index:
            logger.warning("code_index 未配置")
            return []
        # TODO: 实际调用 code_index.search()
        return []

    def _rrf_fusion(
        self, results_by_modality: dict[str, list[tuple[SearchResult, float]]]
    ) -> list[SearchResult]:
        """
        RRF (Reciprocal Rank Fusion) 加权融合

        RRF 公式: score(d) = Σ(weight[modality] / (k + rank[d, modality]))
        k = 60 (常量，避免除零)
        """
        k = 60
        fused: dict[int, SearchResult] = {}

        for modality, results in results_by_modality.items():
            weight = self.rrf_weights.get(modality, 0.25)

            for rank, (result, base_score) in enumerate(results, 1):
                rrf_score = weight / (k + rank)
                # 同时考虑原始相似度
                combined_score = rrf_score * 0.5 + base_score * 0.5

                if result.chunk_id in fused:
                    fused[result.chunk_id].score += combined_score
                    fused[result.chunk_id].modalities.append(modality)
                else:
                    result.score = combined_score
                    result.modalities = [modality]
                    fused[result.chunk_id] = result

        # 按 score 排序
        return sorted(fused.values(), key=lambda r: r.score, reverse=True)

    @staticmethod
    def _dedupe_and_sort(results: list[SearchResult]) -> list[SearchResult]:
        """去重 + 按 score 排序"""
        seen = set()
        deduped = []
        for r in results:
            if r.chunk_id not in seen:
                seen.add(r.chunk_id)
                deduped.append(r)
        return sorted(deduped, key=lambda r: r.score, reverse=True)