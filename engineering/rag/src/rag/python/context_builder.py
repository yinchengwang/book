"""
ContextBuilder — 分层上下文构建

将检索结果按模态分组，构建分层上下文供 LLM 生成使用。
控制 token 总数，支持表格/图像/代码的格式化。
"""

import logging
from collections import defaultdict
from typing import Optional

logger = logging.getLogger(__name__)


class ContextBuilder:
    """
    分层上下文构建器

    按模态分组检索结果，控制 token 总数不超过 LLM 限制。
    支持文本、表格、图像、代码四类模态的格式化输出。
    """

    def __init__(
        self,
        max_tokens_per_modality: dict = None,
        llm_context_limit: int = 128000,
    ):
        self.max_tokens = max_tokens_per_modality or {
            "text": 8000,
            "table": 6000,
            "image": 4000,
            "code": 4000,
        }
        self.llm_limit = llm_context_limit

    def build(
        self,
        query: str,
        results: list,
    ) -> dict:
        """
        构建分层上下文

        Args:
            query: 用户查询
            results: 检索结果列表（每个结果有 chunk_type, content, metadata, score 属性）

        Returns:
            {
                "query": 原始查询,
                "context": 拼接后的上下文字符串,
                "token_count": 总 token 数,
                "source_count": 来源数量,
                "sources": 来源列表,
            }
        """
        context_parts = []
        current_tokens = 0
        sources = []

        # 1. 按模态分组
        by_modality = defaultdict(list)
        for r in results:
            modality = getattr(r, "chunk_type", "text")
            by_modality[modality].append(r)

        # 2. 按优先级顺序处理各模态
        for modality in ["text", "table", "image", "code"]:
            chunks = by_modality.get(modality, [])
            if not chunks:
                continue

            # 按 score 排序取 top-N
            chunks.sort(key=lambda r: getattr(r, "score", 0), reverse=True)
            top_n = self._get_top_n(modality, chunks)

            for chunk in top_n:
                formatted = self._format_chunk(chunk, modality)
                tokens = self._count_tokens(formatted)

                if current_tokens + tokens > self.llm_limit:
                    break

                context_parts.append(formatted)
                current_tokens += tokens

                # 记录来源
                meta = getattr(chunk, "metadata", {}) or {}
                sources.append({
                    "chunk_id": getattr(chunk, "chunk_id", 0),
                    "modality": modality,
                    "source": meta.get("source", "未知"),
                    "score": getattr(chunk, "score", 0),
                })

        return {
            "query": query,
            "context": "\n\n---\n\n".join(context_parts),
            "token_count": current_tokens,
            "source_count": len(context_parts),
            "sources": sources,
        }

    def _get_top_n(self, modality: str, chunks: list) -> list:
        """
        根据模态获取 top-N 结果

        Args:
            modality: 模态类型
            chunks: 排序后的 chunk 列表

        Returns:
            top-N 结果
        """
        max_per_modality = {
            "text": 5,
            "table": 3,
            "image": 2,
            "code": 3,
        }
        n = max_per_modality.get(modality, 3)
        return chunks[:n]

    def _format_chunk(self, chunk, modality: str) -> str:
        """
        格式化单个 chunk

        Args:
            chunk: 检索结果对象
            modality: 模态类型

        Returns:
            格式化后的文本
        """
        content = getattr(chunk, "content", "")
        metadata = getattr(chunk, "metadata", {}) or {}

        if modality == "text":
            return self._format_text(content, metadata)
        elif modality == "table":
            return self._format_table(chunk)
        elif modality == "image":
            return self._format_image(chunk)
        elif modality == "code":
            return self._format_code(chunk)
        return str(content)

    def _format_text(self, content: str, metadata: dict) -> str:
        """格式化文本片段"""
        source = metadata.get("source", "未知")
        return f"""【文本片段】
{content}

来源: {source}"""

    def _format_table(self, chunk) -> str:
        """
        表格 Markdown 格式化

        将表格 chunk 渲染为 Markdown 表格格式，
        保持原始结构供 LLM 理解。

        Args:
            chunk: 表格检索结果

        Returns:
            Markdown 格式的表格文本
        """
        metadata = getattr(chunk, "metadata", {}) or {}
        title = metadata.get("title", "无标题")
        headers = metadata.get("headers", [])
        rows = metadata.get("rows", [])
        source = metadata.get("source", "未知")

        lines = [f"【表格】", f"标题: {title}", ""]

        # 表头
        if headers:
            header_row = "| " + " | ".join(str(h) for h in headers) + " |"
            separator = "| " + " | ".join("---" for _ in headers) + " |"
            lines.append(header_row)
            lines.append(separator)

        # 数据行
        for row in rows:
            row_str = "| " + " | ".join(str(cell) for cell in row) + " |"
            lines.append(row_str)

        # caption
        caption = metadata.get("caption")
        if caption:
            lines.append("")
            lines.append(f"*{caption}*")

        lines.append("")
        lines.append(f"来源: {source}")

        return "\n".join(lines)

    def _format_image(self, chunk) -> str:
        """
        图像格式化

        G1/G3 混合模式：引用 + Caption 传给 LLM，
        不传递 base64 原图（减少 token 消耗）。

        Args:
            chunk: 图像检索结果

        Returns:
            格式化后的图像描述文本
        """
        metadata = getattr(chunk, "metadata", {}) or {}
        caption = metadata.get("caption", "无描述")
        ocr_text = metadata.get("ocr_text", "无OCR文字")
        image_path = metadata.get("image_path", "未知")

        return f"""【图像】
描述: {caption}
OCR文字: {ocr_text}
位置: {image_path}
注释: 请根据以上描述回答用户问题"""

    def _format_code(self, chunk) -> str:
        """
        代码块格式化

        Args:
            chunk: 代码检索结果

        Returns:
            Markdown 代码块格式的文本
        """
        metadata = getattr(chunk, "metadata", {}) or {}
        content = getattr(chunk, "content", "")
        name = metadata.get("name", "未知")
        language = metadata.get("language", "")
        source = metadata.get("source", "未知")

        return f"""【代码片段】
函数: {name}
语言: {language}
```{language}
{content}
```
来源: {source}"""

    @staticmethod
    def _count_tokens(text: str) -> int:
        """
        估算 token 数（简单按字符数 / 4）

        Args:
            text: 输入文本

        Returns:
            估算的 token 数
        """
        # 简单估算：中文约 1.5 字符/token，英文约 4 字符/token
        chinese_chars = sum(1 for c in text if '一' <= c <= '鿿')
        other_chars = len(text) - chinese_chars
        return int(chinese_chars * 1.5 + other_chars / 4) + 1