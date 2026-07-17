import httpx
import json
import logging
from typing import Optional
from app.config import settings


logger = logging.getLogger("processor.summarizer")


class Summarizer:
    """LLM 摘要生成器：调用 Claude/OpenAI API 生成中文摘要"""

    def __init__(self, api_key: Optional[str] = None, model: Optional[str] = None):
        self.api_key = api_key or settings.llm_api_key
        self.model = model or settings.llm_model

    async def summarize(self, title: str, content: str) -> str:
        """为单条内容生成摘要"""
        if not self.api_key:
            logger.warning("未配置 LLM API Key，跳过摘要生成")
            return self._fallback_summary(content)

        prompt = self._build_prompt(title, content)

        try:
            # 尝试 Anthropic Claude API
            if "claude" in self.model.lower():
                return await self._call_claude(prompt)
            else:
                return await self._call_openai(prompt)
        except Exception as e:
            logger.error(f"LLM 摘要生成失败: {e}")
            return self._fallback_summary(content)

    def _build_prompt(self, title: str, content: str) -> str:
        return f"""你是一个数据库和 AI 领域的技术编辑。
请为以下技术内容生成 150-300 字的中文摘要：
1. 核心贡献/创新点是什么？
2. 为什么它重要？
3. 适合谁关注？

原文标题：{title}
原文内容：{content[:2000]}"""

    async def _call_claude(self, prompt: str) -> str:
        """调用 Anthropic Claude API"""
        async with httpx.AsyncClient(timeout=60) as client:
            resp = await client.post(
                "https://api.anthropic.com/v1/messages",
                headers={
                    "x-api-key": self.api_key,
                    "anthropic-version": "2023-06-01",
                    "content-type": "application/json",
                },
                json={
                    "model": self.model,
                    "max_tokens": 500,
                    "messages": [{"role": "user", "content": prompt}],
                },
            )
            resp.raise_for_status()
            data = resp.json()
            return data["content"][0]["text"]

    async def _call_openai(self, prompt: str) -> str:
        """调用 OpenAI API"""
        async with httpx.AsyncClient(timeout=60) as client:
            resp = await client.post(
                "https://api.openai.com/v1/chat/completions",
                headers={
                    "Authorization": f"Bearer {self.api_key}",
                    "content-type": "application/json",
                },
                json={
                    "model": self.model,
                    "max_tokens": 500,
                    "messages": [{"role": "user", "content": prompt}],
                },
            )
            resp.raise_for_status()
            data = resp.json()
            return data["choices"][0]["message"]["content"]

    @staticmethod
    def _fallback_summary(content: str) -> str:
        """无 LLM 时的降级方案：取原文前 200 字"""
        text = content[:200] if content else ""
        return text + "..." if len(content) > 200 else text
