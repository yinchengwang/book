import math
import logging
from datetime import datetime, timezone
from typing import Optional
from app.services.aggregator.base import AggregatedItem


logger = logging.getLogger("processor.scorer")


class Scorer:
    """内容重要度评分器，综合多维度评出 [0, 1] 分数"""

    # 数据源权重系数
    SOURCE_WEIGHT_MULTIPLIER = 0.1

    # 时效性衰减（半衰期天）
    FRESHNESS_HALF_LIFE_DAYS = 7

    def score(self, item: AggregatedItem) -> float:
        """综合评分"""
        score = 0.0

        # 1. 数据源权重 (0-0.2)
        score += min(item.source_weight * self.SOURCE_WEIGHT_MULTIPLIER, 0.2)

        # 2. 时效性 (0-0.3)
        score += self._freshness_score(item.published)

        # 3. 内容完整度 (0-0.2)
        score += self._completeness_score(item)

        # 4. 标题质量 (0-0.3)
        score += self._title_quality_score(item.title)

        return round(min(score, 1.0), 2)

    def _freshness_score(self, published: Optional[datetime]) -> float:
        """时效性评分：越新的文章分数越高"""
        if published is None:
            return 0.1

        now = datetime.now(timezone.utc)
        if published.tzinfo is None:
            published = published.replace(tzinfo=timezone.utc)

        delta_days = (now - published).days
        if delta_days < 0:
            return 0.3

        # 指数衰减
        freshness = math.exp(-delta_days / self.FRESHNESS_HALF_LIFE_DAYS)
        return 0.3 * freshness

    def _completeness_score(self, item: AggregatedItem) -> float:
        """内容完整度评分"""
        score = 0.0

        # 有摘要
        if item.summary and len(item.summary) > 50:
            score += 0.08

        # 有原文内容
        if item.raw_content and len(item.raw_content) > 100:
            score += 0.06

        # 有标签
        if item.tags and len(item.tags) > 0:
            score += 0.03

        # 有发布时间
        if item.published:
            score += 0.03

        return min(score, 0.2)

    def _title_quality_score(self, title: str) -> float:
        """标题质量评分"""
        if not title:
            return 0.0

        score = 0.1
        length = len(title)

        # 合理长度 (20-100 字符)
        if 20 <= length <= 100:
            score += 0.1

        # 包含技术关键词加分
        tech_keywords = {
            "new", "improved", "efficient", "scalable", "optimized",
            "novel", "state-of-the-art", "benchmark", "survey",
            "database", "learning", "neural", "distributed",
        }
        title_lower = title.lower()
        keyword_hits = sum(1 for kw in tech_keywords if kw in title_lower)
        score += min(keyword_hits * 0.05, 0.1)

        return min(score, 0.3)