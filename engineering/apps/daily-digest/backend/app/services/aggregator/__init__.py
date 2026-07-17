from app.services.aggregator.base import BaseAggregator, AggregatedItem
from app.services.aggregator.arxiv import ArxivAggregator
from app.services.aggregator.semantic_scholar import SemanticScholarAggregator
from app.services.aggregator.dblp import DBLPAggregator
from app.services.aggregator.rss_feeds import RSSFeedsAggregator
from app.services.aggregator.github_releases import GitHubReleasesAggregator
from app.services.aggregator.huggingface import HuggingFaceAggregator
from app.services.aggregator.orchestrator import AggregatorOrchestrator

__all__ = [
    "BaseAggregator",
    "AggregatedItem",
    "ArxivAggregator",
    "SemanticScholarAggregator",
    "DBLPAggregator",
    "RSSFeedsAggregator",
    "GitHubReleasesAggregator",
    "HuggingFaceAggregator",
    "AggregatorOrchestrator",
]