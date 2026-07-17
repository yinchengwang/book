import httpx
import feedparser
from datetime import datetime, timezone
from app.services.aggregator.base import BaseAggregator, AggregatedItem


class RSSFeedsAggregator(BaseAggregator):
    """公司技术博客 RSS 聚合器"""

    FEEDS = [
        # 格式: (名称, RSS URL, 默认分类)
        ("PostgreSQL", "https://planet.postgresql.org/atom.xml", "db"),
        ("MongoDB", "https://www.mongodb.com/blog/rss.xml", "db"),
        ("Databricks", "https://www.databricks.com/blog/feed", "db"),
        ("OpenAI", "https://openai.com/blog/rss.xml", "ai"),
        ("Anthropic", "https://www.anthropic.com/blog/rss.xml", "ai"),
        ("Meta AI", "https://ai.meta.com/blog/rss.xml", "ai"),
        ("Google AI", "https://blog.google/technology/ai/rss", "ai"),
    ]

    @property
    def source_name(self) -> str:
        return "blog"

    @property
    def source_weight(self) -> int:
        return 6

    async def fetch(self) -> list[AggregatedItem]:
        items = []
        async with httpx.AsyncClient(timeout=self.timeout) as client:
            for name, url, category in self.FEEDS:
                try:
                    resp = await client.get(url)
                    resp.raise_for_status()
                    feed = feedparser.parse(resp.text)
                    for entry in feed.entries[:10]:
                        items.append(self._parse_entry(entry, name, category))
                    self.logger.info(f"RSS '{name}': 获取 {len(feed.entries[:10])} 条")
                except Exception as e:
                    self.logger.warning(f"RSS '{name}' 抓取失败: {e}")
                    continue
        return items

    def _parse_entry(self, entry, source_name: str, category: str) -> AggregatedItem:
        title = entry.get("title", "")
        link = entry.get("link", "")
        summary = entry.get("summary", "") or entry.get("description", "")

        published = None
        pub_struct = entry.get("published_parsed") or entry.get("updated_parsed")
        if pub_struct:
            try:
                published = datetime(*pub_struct[:6], tzinfo=timezone.utc)
            except (TypeError, ValueError):
                pass

        return AggregatedItem(
            source=self.source_name,
            source_id=f"{source_name}:{title[:50]}",
            title=title,
            url=link,
            summary="",
            raw_content=summary[:500],
            category=category,
            tags=[source_name.lower()],
            score=0.5,
            published=published,
            source_weight=self.source_weight,
        )