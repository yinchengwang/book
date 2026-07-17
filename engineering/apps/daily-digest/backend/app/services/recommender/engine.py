import json
import logging
from typing import Optional
from datetime import datetime, timezone
from app.services.aggregator.base import AggregatedItem


logger = logging.getLogger("recommender.engine")


class Recommender:
    """个性化推荐引擎"""

    # 热门层阈值
    HOT_SCORE_THRESHOLD = 0.7

    # 每日推荐上限
    MAX_DAILY_ITEMS = 30

    # 各层占比
    HOT_RATIO = 0.3       # 热门层 30%
    SUBSCRIPTION_RATIO = 0.5  # 订阅层 50%
    EXPLORATION_RATIO = 0.2   # 探索层 20%

    def recommend(
        self,
        items: list[AggregatedItem],
        user_id: Optional[int] = None,
        subscriptions: Optional[list[dict]] = None,
        user_actions: Optional[list[dict]] = None,
    ) -> list[AggregatedItem]:
        """生成个性化推荐列表

        Args:
            items: 当日所有已处理条目
            user_id: 用户 ID（可选）
            subscriptions: 用户订阅方向列表
            user_actions: 用户历史行为列表
        """
        if not items:
            return []

        max_hot = int(self.MAX_DAILY_ITEMS * self.HOT_RATIO)
        max_sub = int(self.MAX_DAILY_ITEMS * self.SUBSCRIPTION_RATIO)
        max_exp = self.MAX_DAILY_ITEMS - max_hot - max_sub

        recommended = []
        seen_ids = set()

        # 1. 热门层：高评分内容，所有人必推
        hot_items = self._get_hot_items(items, max_hot)
        for item in hot_items:
            if item.source_id not in seen_ids:
                recommended.append(item)
                seen_ids.add(item.source_id)

        # 2. 订阅层：匹配用户订阅方向
        if subscriptions:
            sub_items = self._get_subscription_items(
                items, subscriptions, max_sub, seen_ids
            )
            for item in sub_items:
                if item.source_id not in seen_ids:
                    recommended.append(item)
                    seen_ids.add(item.source_id)

        # 3. 探索层：补充未读的高质量内容
        exp_items = self._get_exploration_items(
            items, max_exp, seen_ids, user_actions
        )
        recommended.extend(exp_items)

        # 按评分排序
        recommended.sort(key=lambda x: x.score, reverse=True)

        logger.info(
            f"推荐完成: {user_id=}, "
            f"热门={len(hot_items)}, "
            f"订阅={len(sub_items) if subscriptions else 0}, "
            f"探索={len(exp_items)}, "
            f"总计={len(recommended)}"
        )

        return recommended[:self.MAX_DAILY_ITEMS]

    def _get_hot_items(
        self, items: list[AggregatedItem], max_count: int
    ) -> list[AggregatedItem]:
        """获取热门层内容：评分高于阈值"""
        hot = [i for i in items if i.score >= self.HOT_SCORE_THRESHOLD]
        hot.sort(key=lambda x: x.score, reverse=True)
        return hot[:max_count]

    def _get_subscription_items(
        self,
        items: list[AggregatedItem],
        subscriptions: list[dict],
        max_count: int,
        seen_ids: set,
    ) -> list[AggregatedItem]:
        """获取订阅层内容：匹配用户订阅的分类和关键词"""
        matched = []

        for sub in subscriptions:
            category = sub.get("category", "")
            keywords = self._parse_keywords(sub.get("keywords", "[]"))
            weight = sub.get("weight", 1)

            for item in items:
                if item.source_id in seen_ids:
                    continue

                score = 0.0

                # 分类匹配
                if item.category == category:
                    score += 0.5

                # 关键词匹配
                if keywords:
                    text = f"{item.title} {item.raw_content}".lower()
                    keyword_matches = sum(
                        1 for kw in keywords if kw.lower() in text
                    )
                    score += min(keyword_matches * 0.2, 0.4)

                if score > 0:
                    matched.append((item, score * weight))

        # 按匹配度排序
        matched.sort(key=lambda x: x[1], reverse=True)
        return [item for item, _ in matched[:max_count]]

    def _get_exploration_items(
        self,
        items: list[AggregatedItem],
        max_count: int,
        seen_ids: set,
        user_actions: Optional[list[dict]] = None,
    ) -> list[AggregatedItem]:
        """获取探索层内容：未读的高评分内容"""
        # 已读列表
        read_ids = set()
        if user_actions:
            for action in user_actions:
                if action.get("action") in ("view", "click_through"):
                    item_id = action.get("item_id")
                    if item_id:
                        read_ids.add(item_id)

        candidates = []
        for item in items:
            if item.source_id in seen_ids:
                continue
            # 如果用户看过同 source 的内容，降低权重但不排除
            if item.source_id in read_ids:
                continue

            candidates.append(item)

        # 按评分排序，取评分最高的
        candidates.sort(key=lambda x: x.score, reverse=True)
        return candidates[:max_count]

    @staticmethod
    def _parse_keywords(keywords_str: str) -> list[str]:
        """解析 JSON 关键词字符串"""
        if not keywords_str:
            return []
        try:
            return json.loads(keywords_str)
        except (json.JSONDecodeError, TypeError):
            return []