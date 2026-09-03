"""
指代消歧模块

处理用户查询中的指代表达，如"那张图"、"这个"、"第一张"等。
"""

import logging
import re
from typing import Optional

logger = logging.getLogger(__name__)


class SessionContext:
    """
    会话级上下文管理

    记录已展示的检索结果，支持指代消解。

    用法:
        ctx = SessionContext()
        ctx.track_result(result)
        recent = ctx.get_recent_images()
    """

    def __init__(self, max_history: int = 20, ttl_seconds: int = 1800):
        self.max_history = max_history
        self.ttl_seconds = ttl_seconds
        self._history = []  # [(timestamp, SearchResult), ...]

    def track_result(self, result):
        """
        记录已展示的检索结果

        Args:
            result: 检索结果对象
        """
        import time
        now = time.time()

        self._history.append((now, result))

        # 裁剪过期记录
        self._history = [
            (ts, r) for ts, r in self._history
            if now - ts < self.ttl_seconds
        ]

        # 裁剪超出上限的记录
        if len(self._history) > self.max_history:
            self._history = self._history[-self.max_history:]

    def get_recent_results(self, modality: str = None, n: int = 5) -> list:
        """
        获取最近展示的结果

        Args:
            modality: 模态类型过滤（可选）
            n: 最近 N 条

        Returns:
            结果列表
        """
        results = [r for _, r in self._history]
        if modality:
            results = [
                r for r in results
                if getattr(r, "chunk_type", "") == modality
            ]
        return results[-n:]

    def get_recent_images(self, n: int = 3) -> list:
        """
        获取最近展示的图像结果

        Args:
            n: 最近 N 条

        Returns:
            图像结果列表
        """
        return self.get_recent_results(modality="image", n=n)

    def clear(self):
        """清空历史"""
        self._history = []


class DeixisResolver:
    """
    指代消解器

    处理指代表达：
    - 指示词："那张"/"这个" → 最近结果
    - 序数词："第一张"/"第二个" → 列表索引
    - 属性词："红色的架构图" → 元数据匹配

    用法:
        resolver = DeixisResolver(ctx)
        resolved = resolver.resolve("那张审批流程图在哪？")
    """

    # 指示词模式
    DEMONSTRATIVE_PATTERNS = [
        r'(那|这)(张|个|份|篇|段|块|幅|个)',
        r'(那|这)(些|种|类)',
    ]

    # 序数词模式
    ORDINAL_PATTERNS = [
        r'第[一二三四五六七八九十\d]+(张|个|份|篇|段|块|幅|条)',
        r'(上|下|前|后|最(后|近|新))一(张|个|份|篇|段|块|幅|条)',
    ]

    # 属性词模式
    ATTRIBUTE_PATTERNS = [
        r'(红色|蓝色|绿色|白色|黑色|黄色|架构|流程|时序|类图|部署)',
    ]

    def __init__(self, session_context: SessionContext):
        self.ctx = session_context

    def resolve(self, query: str, top_n: int = 5) -> tuple[str, list]:
        """
        解析指代表达

        Args:
            query: 用户查询
            top_n: 获取最近 N 条结果供消解

        Returns:
            (resolved_query, context_results) 元组
            - resolved_query: 替换指代后的查询
            - context_results: 指代命中的结果列表
        """
        # 1. 尝试解析指示词
        resolved, demo_results = self._resolve_demonstrative(query, top_n)
        if demo_results:
            return resolved, demo_results

        # 2. 尝试解析序数词
        resolved, ord_results = self._resolve_ordinal(query, top_n)
        if ord_results:
            return resolved, ord_results

        # 3. 尝试解析属性词
        resolved, attr_results = self._resolve_attribute(query, top_n)
        if attr_results:
            return resolved, attr_results

        # 未识别指代表达
        return query, []

    def _resolve_demonstrative(self, query: str, top_n: int) -> tuple[str, list]:
        """
        解析指示词

        "那张审批流程图" → 最近展示的图像结果

        Args:
            query: 用户查询
            top_n: 最近 N 条结果

        Returns:
            (resolved_query, results) 元组
        """
        for pattern in self.DEMONSTRATIVE_PATTERNS:
            match = re.search(pattern, query)
            if match:
                # 从 history 中取最近结果
                recent = self.ctx.get_recent_results(n=top_n)
                if recent:
                    # 尝试匹配模态
                    demo_word = match.group(0)
                    # "那张"、"这幅" → 图像
                    if '张' in demo_word or '幅' in demo_word:
                        image_results = [r for r in recent if getattr(r, "chunk_type", "") == "image"]
                        if image_results:
                            return query, image_results[:3]
                    # "那段"、"这块" → 代码
                    if '段' in demo_word or '块' in demo_word:
                        code_results = [r for r in recent if getattr(r, "chunk_type", "") == "code"]
                        if code_results:
                            return query, code_results[:3]
                    # 默认返回最近结果
                    return query, recent[:3]
        return query, []

    def _resolve_ordinal(self, query: str, top_n: int) -> tuple[str, list]:
        """
        解析序数词

        "第一张图" → 索引 0

        Args:
            query: 用户查询
            top_n: 结果数量

        Returns:
            (resolved_query, results) 元组
        """
        # 匹配 "第N张/个..."
        pattern = r'第([一二三四五六七八九十\d]+)(张|个|份|篇|段|块|幅|条)'
        match = re.search(pattern, query)
        if not match:
            return query, []

        # 中文数字转整数
        num_str = match.group(1)
        if num_str.isdigit():
            index = int(num_str) - 1
        else:
            index = self._chinese_to_int(num_str) - 1

        if index < 0:
            return query, []

        recent = self.ctx.get_recent_results(n=top_n)
        if 0 <= index < len(recent):
            return query, [recent[index]]

        return query, []

    def _resolve_attribute(self, query: str, top_n: int) -> tuple[str, list]:
        """
        解析属性词

        "红色的架构图" → 匹配 metadata 中的描述

        Args:
            query: 用户查询
            top_n: 结果数量

        Returns:
            (resolved_query, results) 元组
        """
        for pattern in self.ATTRIBUTE_PATTERNS:
            match = re.search(pattern, query)
            if match:
                attribute = match.group(1)
                recent = self.ctx.get_recent_results(n=top_n)

                # 在 metadata 中匹配
                matched = []
                for r in recent:
                    meta = getattr(r, "metadata", {}) or {}
                    caption = meta.get("caption", "")
                    content = getattr(r, "content", "")
                    if attribute in caption or attribute in content:
                        matched.append(r)

                if matched:
                    return query, matched

        return query, []

    @staticmethod
    def _chinese_to_int(chinese: str) -> int:
        """
        中文数字转整数

        Args:
            chinese: 中文数字字符串

        Returns:
            整数
        """
        num_map = {
            '一': 1, '二': 2, '三': 3, '四': 4, '五': 5,
            '六': 6, '七': 7, '八': 8, '九': 9, '十': 10,
        }
        if chinese in num_map:
            return num_map[chinese]
        return 1