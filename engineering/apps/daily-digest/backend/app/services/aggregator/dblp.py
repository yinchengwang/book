import httpx
from datetime import datetime, timezone
from app.services.aggregator.base import BaseAggregator, AggregatedItem


class DBLPAggregator(BaseAggregator):
    """DBLP API 聚合器（数据库/信息检索最新论文）"""

    API_URL = "https://dblp.org/search/publ/api"

    @property
    def source_name(self) -> str:
        return "dblp"

    @property
    def source_weight(self) -> int:
        return 8

    async def fetch(self) -> list[AggregatedItem]:
        items = []
        queries = [
            ("database", "db"),
            ("query processing", "db"),
            ("data mining", "ml"),
            ("information retrieval", "db"),
        ]
        async with httpx.AsyncClient(timeout=self.timeout) as client:
            for query, category in queries:
                try:
                    resp = await client.get(
                        self.API_URL,
                        params={
                            "q": query,
                            "format": "json",
                            "h": 20,  # 每次返回 20 条
                            "c": 0,
                        },
                    )
                    resp.raise_for_status()
                    data = resp.json()
                    hits = data.get("result", {}).get("hits", {}).get("hit", [])
                    for hit in hits[:10]:
                        info = hit.get("info", {})
                        items.append(self._parse_paper(info, category))
                except Exception as e:
                    self.logger.warning(f"DBLP 查询 '{query}' 失败: {e}")
                    continue
        return items

    def _parse_paper(self, info: dict, category: str) -> AggregatedItem:
        title = info.get("title", "")
        url = info.get("url", "")
        doi = info.get("doi", "")
        year = info.get("year", "")
        authors = info.get("authors", {}).get("author", [])
        if isinstance(authors, dict):
            authors = [authors]
        author_str = ", ".join(
            a.get("text", "") if isinstance(a, dict) else str(a)
            for a in authors[:3]
        )

        # 用 DBLP URL 作为 source_id
        source_id = url.split("/")[-1] if url else doi

        published = None
        if year:
            try:
                published = datetime(int(year), 1, 1, tzinfo=timezone.utc)
            except (ValueError, TypeError):
                pass

        return AggregatedItem(
            source=self.source_name,
            source_id=source_id,
            title=title,
            url=url or f"https://doi.org/{doi}",
            summary="",
            raw_content=f"[{author_str}]" if author_str else "",
            category=category,
            tags=[],
            score=0.4,
            published=published,
            source_weight=self.source_weight,
        )