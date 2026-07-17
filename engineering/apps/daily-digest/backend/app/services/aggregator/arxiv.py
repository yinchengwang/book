import httpx
import xml.etree.ElementTree as ET
from datetime import datetime, timezone
from app.services.aggregator.base import BaseAggregator, AggregatedItem


class ArxivAggregator(BaseAggregator):
    """arXiv API 聚合器，抓取数据库/AI/ML 领域的最新论文"""

    API_URL = "http://export.arxiv.org/api/query"
    CATEGORIES = ["cs.DB", "cs.AI", "cs.LG", "cs.IR"]
    # arXiv API 每次最多返回 100 条
    MAX_RESULTS = 50

    @property
    def source_name(self) -> str:
        return "arxiv"

    @property
    def source_weight(self) -> int:
        return 10  # 高权重

    async def fetch(self) -> list[AggregatedItem]:
        query = "+OR+".join(f"cat:{cat}" for cat in self.CATEGORIES)
        url = f"{self.API_URL}?search_query={query}&sortBy=submittedDate&sortOrder=descending&max_results={self.MAX_RESULTS}"

        async with httpx.AsyncClient(timeout=self.timeout) as client:
            response = await client.get(url)
            response.raise_for_status()
            return self._parse_response(response.text)

    def _parse_response(self, xml_text: str) -> list[AggregatedItem]:
        """解析 arXiv API 返回的 Atom XML"""
        ns = {
            "atom": "http://www.w3.org/2005/Atom",
            "arxiv": "http://arxiv.org/schemas/atom",
        }
        root = ET.fromstring(xml_text)
        items = []

        for entry in root.findall("atom:entry", ns):
            try:
                item = self._parse_entry(entry, ns)
                if item:
                    items.append(item)
            except Exception as e:
                self.logger.warning(f"解析 arXiv 条目失败: {e}")
                continue

        return items

    def _parse_entry(self, entry: ET.Element, ns: dict) -> AggregatedItem | None:
        id_tag = entry.find("atom:id", ns)
        title_tag = entry.find("atom:title", ns)
        summary_tag = entry.find("atom:summary", ns)
        published_tag = entry.find("atom:published", ns)

        if id_tag is None or title_tag is None:
            return None

        arxiv_id = id_tag.text.strip().split("/")[-1] if id_tag.text else ""
        title = self._clean_text(title_tag.text or "")

        # 摘要取前 500 字符作为 raw_content
        summary = self._clean_text(summary_tag.text or "")
        raw_content = summary[:500] if summary else ""

        # 解析发布时间
        published = None
        if published_tag is not None and published_tag.text:
            try:
                published = datetime.fromisoformat(published_tag.text.replace("Z", "+00:00"))
            except (ValueError, AttributeError):
                pass

        # 提取分类标签
        categories = entry.findall("atom:category", ns)
        tags = [cat.get("term", "") for cat in categories if cat.get("term")]

        # 提取作者
        authors = []
        for author in entry.findall("atom:author", ns):
            name_tag = author.find("atom:name", ns)
            if name_tag is not None and name_tag.text:
                authors.append(name_tag.text.strip())
        author_str = ", ".join(authors[:3])

        return AggregatedItem(
            source=self.source_name,
            source_id=arxiv_id,
            title=title,
            url=f"https://arxiv.org/abs/{arxiv_id}",
            summary="",
            raw_content=f"[{author_str}] {raw_content}" if author_str else raw_content,
            category=self._classify_paper(title, tags),
            tags=tags[:5],
            score=0.5,  # 默认中等评分，后续由处理器调整
            published=published,
            source_weight=self.source_weight,
        )

    @staticmethod
    def _clean_text(text: str) -> str:
        """清理文本：去除多余空白和换行"""
        import re
        text = re.sub(r"\s+", " ", text or "")
        return text.strip()

    @staticmethod
    def _classify_paper(title: str, tags: list[str]) -> str:
        """根据标题和标签粗略分类"""
        title_lower = title.lower()
        tag_set = set(t.lower() for t in tags)

        # 关键词映射
        db_keywords = {"database", "storage", "query", "index", "transaction", "sql",
                       "nosql", "lsm", "btree", "raft", "paxos", "sharding"}
        ai_keywords = {"llm", "large language model", "gpt", "transformer", "attention",
                       "prompt", "chain-of-thought", "rag", "agent", "multimodal"}
        ml_keywords = {"learning", "reinforcement", "representation", "embedding",
                       "classification", "regression", "neural", "deep learning"}
        infra_keywords = {"distributed", "consensus", "replication", "load balance",
                          "container", "kubernetes", "microservice", "observability"}

        if any(kw in title_lower for kw in db_keywords):
            return "db"
        if any(kw in title_lower for kw in ai_keywords):
            return "ai"
        if any(kw in title_lower for kw in ml_keywords):
            return "ml"
        if any(kw in title_lower for kw in infra_keywords):
            return "infra"

        # 检查 tags
        if any(t in tag_set for t in {"cs.DB", "cs.DC", "cs.IR"}):
            return "db"
        if any(t in tag_set for t in {"cs.AI", "cs.CL"}):
            return "ai"
        if any(t in tag_set for t in {"cs.LG", "stat.ML"}):
            return "ml"

        return "other"

    async def fetch_safe(self) -> list[AggregatedItem]:
        items = await super().fetch_safe()
        self.logger.info(f"arXiv 聚合完成: {len(items)} 条 (分类分布: "
                         f"db={sum(1 for i in items if i.category=='db')}, "
                         f"ai={sum(1 for i in items if i.category=='ai')}, "
                         f"ml={sum(1 for i in items if i.category=='ml')})")
        return items