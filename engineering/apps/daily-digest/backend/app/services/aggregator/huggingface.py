import httpx
from datetime import datetime, timezone
from app.services.aggregator.base import BaseAggregator, AggregatedItem


class HuggingFaceAggregator(BaseAggregator):
    """HuggingFace Daily Papers 聚合器"""

    API_URL = "https://huggingface.co/api/daily_papers"

    @property
    def source_name(self) -> str:
        return "huggingface"

    @property
    def source_weight(self) -> int:
        return 8

    async def fetch(self) -> list[AggregatedItem]:
        items = []
        async with httpx.AsyncClient(timeout=self.timeout) as client:
            try:
                resp = await client.get(self.API_URL)
                resp.raise_for_status()
                papers = resp.json()
                for paper in papers[:30]:
                    items.append(self._parse_paper(paper))
            except Exception as e:
                self.logger.warning(f"HuggingFace 获取失败: {e}")
        return items

    def _parse_paper(self, paper: dict) -> AggregatedItem:
        paper_data = paper.get("paper", {}) or paper
        title = paper_data.get("title", "")
        paper_id = paper_data.get("id", "")
        summary = paper_data.get("summary", "")
        url = paper_data.get("url", f"https://arxiv.org/abs/{paper_id}")

        published = None
        pub_date = paper_data.get("publishedAt") or paper.get("publishedAt")
        if pub_date:
            try:
                published = datetime.fromisoformat(pub_date.replace("Z", "+00:00"))
            except (ValueError, AttributeError):
                pass

        # 分类推断
        title_lower = title.lower()
        if any(kw in title_lower for kw in ["llm", "language model", "gpt", "transformer", "attention"]):
            category = "ai"
        elif any(kw in title_lower for kw in ["learning", "reinforcement", "diffusion", "classification"]):
            category = "ml"
        elif any(kw in title_lower for kw in ["database", "storage", "index", "query"]):
            category = "db"
        else:
            category = "ai"

        return AggregatedItem(
            source=self.source_name,
            source_id=paper_id,
            title=title,
            url=url,
            summary="",
            raw_content=summary[:500],
            category=category,
            tags=[],
            score=0.6,
            published=published,
            source_weight=self.source_weight,
        )