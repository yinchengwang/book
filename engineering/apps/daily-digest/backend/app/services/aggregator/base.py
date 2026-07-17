from abc import ABC, abstractmethod
from typing import Optional
from dataclasses import dataclass, field
from datetime import datetime


@dataclass
class AggregatedItem:
    """聚合器产出的统一数据条目"""
    source: str                              # 来源分类：arxiv / blog / github / news / conference
    source_id: str                           # 原始 ID（用于去重）
    title: str
    url: str
    summary: str = ""
    raw_content: str = ""
    category: str = "other"                  # db / ai / ml / infra / sys / other
    tags: list[str] = field(default_factory=list)
    score: float = 0.0
    published: Optional[datetime] = None
    source_weight: int = 0


class BaseAggregator(ABC):
    """聚合器抽象基类，所有数据源聚合器继承此类"""

    def __init__(self, timeout: int = 30):
        self.timeout = timeout
        self.logger = self._get_logger()

    @property
    @abstractmethod
    def source_name(self) -> str:
        """数据源名称，用于 source 字段"""
        pass

    @property
    @abstractmethod
    def source_weight(self) -> int:
        """数据源权重，影响排序"""
        pass

    @abstractmethod
    async def fetch(self) -> list[AggregatedItem]:
        """抓取并解析数据源，返回统一格式的条目列表"""
        pass

    def _get_logger(self):
        import logging
        return logging.getLogger(f"aggregator.{self.__class__.__name__}")

    async def fetch_safe(self) -> list[AggregatedItem]:
        """带异常安全包装的 fetch"""
        try:
            self.logger.info(f"开始从 {self.source_name} 抓取数据...")
            items = await self.fetch()
            self.logger.info(f"从 {self.source_name} 抓取到 {len(items)} 条")
            return items
        except Exception as e:
            self.logger.error(f"从 {self.source_name} 抓取失败: {e}")
            return []