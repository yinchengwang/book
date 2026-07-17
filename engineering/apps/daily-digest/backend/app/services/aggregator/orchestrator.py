import asyncio
from typing import Optional
from datetime import datetime
from app.services.aggregator.base import AggregatedItem
from app.services.aggregator.arxiv import ArxivAggregator
from app.services.aggregator.semantic_scholar import SemanticScholarAggregator
from app.services.aggregator.dblp import DBLPAggregator
from app.services.aggregator.rss_feeds import RSSFeedsAggregator
from app.services.aggregator.github_releases import GitHubReleasesAggregator
from app.services.aggregator.huggingface import HuggingFaceAggregator
import logging


class AggregatorOrchestrator:
    """聚合编排器：并行调度所有聚合器，合并去重"""

    def __init__(self, timeout: int = 30):
        self.timeout = timeout
        self.logger = logging.getLogger("aggregator.orchestrator")
        self.aggregators = [
            ArxivAggregator(timeout),
            SemanticScholarAggregator(timeout),
            DBLPAggregator(timeout),
            RSSFeedsAggregator(timeout),
            GitHubReleasesAggregator(timeout),
            HuggingFaceAggregator(timeout),
        ]

    async def fetch_all(self) -> list[AggregatedItem]:
        """并行抓取所有数据源，去重后返回"""
        self.logger.info("开始并行抓取所有数据源...")

        tasks = [agg.fetch_safe() for agg in self.aggregators]
        results = await asyncio.gather(*tasks)

        all_items = []
        for items in results:
            all_items.extend(items)

        self.logger.info(f"聚合完成: 共 {len(all_items)} 条原始数据")

        # 去重：同一 source + source_id 只保留一条
        deduped = self._dedup(all_items)
        self.logger.info(f"去重后: {len(deduped)} 条")

        # 按发布时间排序
        deduped.sort(key=lambda x: x.published or x.created_at or datetime.min, reverse=True)

        return deduped

    def _dedup(self, items: list[AggregatedItem]) -> list[AggregatedItem]:
        """按 (source, source_id) 去重，保留第一条"""
        seen = set()
        result = []
        for item in items:
            key = (item.source, item.source_id)
            if key not in seen:
                seen.add(key)
                result.append(item)
        return result

    def get_stats(self, items: list[AggregatedItem]) -> dict:
        """获取聚合统计信息"""
        stats = {
            "total": len(items),
            "by_source": {},
            "by_category": {},
        }
        for item in items:
            stats["by_source"][item.source] = stats["by_source"].get(item.source, 0) + 1
            stats["by_category"][item.category] = stats["by_category"].get(item.category, 0) + 1
        return stats