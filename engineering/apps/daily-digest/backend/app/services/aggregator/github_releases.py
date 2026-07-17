import httpx
from datetime import datetime, timezone
from app.services.aggregator.base import BaseAggregator, AggregatedItem


class GitHubReleasesAggregator(BaseAggregator):
    """GitHub Releases 聚合器（选定的开源项目）"""

    REPOS = [
        "postgres/postgres",
        "redis/redis",
        "mongodb/mongo",
        "facebookresearch/faiss",
        "milvus-io/milvus",
        "chroma-core/chroma",
        "lantern-ai/lantern",
    ]

    @property
    def source_name(self) -> str:
        return "github"

    @property
    def source_weight(self) -> int:
        return 5

    async def fetch(self) -> list[AggregatedItem]:
        items = []
        headers = {"Accept": "application/vnd.github.v3+json"}
        async with httpx.AsyncClient(timeout=self.timeout, headers=headers) as client:
            for repo in self.REPOS:
                try:
                    resp = await client.get(
                        f"https://api.github.com/repos/{repo}/releases",
                        params={"per_page": 5},
                    )
                    resp.raise_for_status()
                    releases = resp.json()
                    for release in releases:
                        items.append(self._parse_release(release, repo))
                except Exception as e:
                    self.logger.warning(f"GitHub '{repo}' 获取失败: {e}")
                    continue
        return items

    def _parse_release(self, release: dict, repo: str) -> AggregatedItem:
        tag = release.get("tag_name", "")
        title = release.get("name", "") or f"{repo} {tag}"
        body = release.get("body", "") or ""
        published_str = release.get("published_at")
        published = None
        if published_str:
            try:
                published = datetime.fromisoformat(published_str.replace("Z", "+00:00"))
            except (ValueError, AttributeError):
                pass

        # 从仓库名推断分类
        repo_lower = repo.lower()
        if any(kw in repo_lower for kw in ["postgres", "redis", "mongodb", "milvus", "chroma"]):
            category = "db"
        elif any(kw in repo_lower for kw in ["faiss", "lantern"]):
            category = "ai"
        else:
            category = "other"

        return AggregatedItem(
            source=self.source_name,
            source_id=f"{repo}:{tag}",
            title=title,
            url=release.get("html_url", f"https://github.com/{repo}/releases/tag/{tag}"),
            summary="",
            raw_content=body[:500],
            category=category,
            tags=[repo.split("/")[-1]],
            score=0.6,
            published=published,
            source_weight=self.source_weight,
        )