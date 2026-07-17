import httpx
from datetime import datetime, timezone
from app.services.aggregator.base import BaseAggregator, AggregatedItem


class SemanticScholarAggregator(BaseAggregator):
    """Semantic Scholar API 聚合器（热门论文）"""

    API_URL = "https://api.semanticscholar.org/graph/v1"

    @property
    def source_name(self) -> str:
        return "semantic_scholar"

    @property
    def source_weight(self) -> int:
        return 9

    async def fetch(self) -> list[AggregatedItem]:
        items = []
        # 搜索热门论文：数据库 + AI
        queries = [
            ("database", "cs.DB"),
            ("large language model", "cs.AI"),
            ("machine learning", "cs.LG"),
        ]
        async with httpx.AsyncClient(timeout=self.timeout) as client:
            for query, category in queries:
                try:
                    resp = await client.get(
                        f"{self.API_URL}/paper/search",
                        params={
                            "query": query,
                            "limit": 20,
                            "fields": "title,url,abstract,publicationDate,externalIds,authors",
                        },
                    )
                    resp.raise_for_status()
                    data = resp.json()
                    for paper in data.get("data", []):
                        items.append(self._parse_paper(paper, category))
                except Exception as e:
                    self.logger.warning(f"Semantic Scholar 查询 '{query}' 失败: {e}")
                    continue
        return items

    def _parse_paper(self, paper: dict, category: str) -> AggregatedItem:
        paper_id = paper.get("paperId", "")
        title = paper.get("title", "")
        abstract = paper.get("abstract", "")
        pub_date = paper.get("publicationDate")
        published = None
        if pub_date:
            try:
                published = datetime.strptime(pub_date, "%Y-%m-%d").replace(tzinfo=timezone.utc)
            except ValueError:
                pass

        authors = paper.get("authors", [])
        author_str = ", ".join(a.get("name", "") for a in authors[:3])

        return AggregatedItem(
            source=self.source_name,
            source_id=paper_id,
            title=title,
            url=paper.get("url", f"https://www.semanticscholar.org/paper/{paper_id}"),
            summary="",
            raw_content=f"[{author_str}] {abstract[:500]}" if author_str else abstract[:500],
            category=category,
            tags=[],
            score=0.5,
            published=published,
            source_weight=self.source_weight,
        )