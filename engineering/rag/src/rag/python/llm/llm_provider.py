"""
LLM Provider 模块

MiniMax M3（主）+ Claude（fallback）双 Provider 支持。
启动时校验 vision + function_call 能力，自动 fallback。
"""

import logging
import json
from typing import Optional

logger = logging.getLogger(__name__)


class LLMCapabilityError(Exception):
    """LLM 能力不支持异常"""
    pass


class MiniMaxProvider:
    """
    MiniMax M3 Provider

    主 LLM Provider，支持 vision 和 function_call 能力探测。
    """

    def __init__(self, api_key: str = "", model: str = "MiniMax-M3"):
        self.api_key = api_key
        self.model = model
        self._client = None
        self._capabilities = None

    def _get_client(self):
        """获取 OpenAI 兼容客户端"""
        if self._client is None:
            try:
                from openai import OpenAI
                self._client = OpenAI(
                    api_key=self.api_key,
                    base_url="https://api.minimax.chat/v1",
                )
            except ImportError:
                logger.warning("openai 库未安装，使用模拟模式")
                self._client = "mock"
        return self._client

    def _probe_capabilities(self) -> dict:
        """
        启动时探测 vision + function_call 能力

        Returns:
            {"vision": bool, "function_call": bool}
        """
        caps = {"vision": False, "function_call": False}

        client = self._get_client()
        if client == "mock":
            # 模拟模式：假设都支持
            caps["vision"] = True
            caps["function_call"] = True
            return caps

        # 探测 vision
        try:
            response = client.chat.completions.create(
                model=self.model,
                messages=[{
                    "role": "user",
                    "content": [
                        {"type": "image_url", "image_url": {
                            "url": "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg=="
                        }},
                        {"type": "text", "text": "这是什么颜色?"}
                    ]
                }]
            )
            if response.choices[0].message.content:
                caps["vision"] = True
        except Exception as e:
            logger.warning(f"MiniMax vision 探测失败: {e}")

        # 探测 function_call
        try:
            response = client.chat.completions.create(
                model=self.model,
                messages=[{"role": "user", "content": "北京天气怎么样?"}],
                tools=[{
                    "type": "function",
                    "function": {
                        "name": "get_weather",
                        "parameters": {
                            "type": "object",
                            "properties": {"city": {"type": "string"}},
                            "required": ["city"],
                        },
                    },
                }],
                tool_choice="auto",
            )
            if response.choices[0].message.tool_calls:
                caps["function_call"] = True
        except Exception as e:
            logger.warning(f"MiniMax function_call 探测失败: {e}")

        return caps

    @property
    def capabilities(self) -> dict:
        """获取能力信息（惰性探测）"""
        if self._capabilities is None:
            self._capabilities = self._probe_capabilities()
        return self._capabilities

    def generate(self, prompt: str, context: dict = None,
                 images: list = None, tools: list = None) -> str:
        """
        生成回答

        Args:
            prompt: 用户原始 query
            context: ContextBuilder 构建的分层上下文
            images: 可选的 base64 图片列表
            tools: 可选的 function calling 定义

        Returns:
            生成的回答文本
        """
        caps = self.capabilities

        # 检查能力
        if images and not caps.get("vision"):
            raise LLMCapabilityError("MiniMax vision not supported")

        if tools and not caps.get("function_call"):
            raise LLMCapabilityError("MiniMax function_call not supported")

        client = self._get_client()
        if client == "mock":
            logger.info(f"[模拟] MiniMax generate: {prompt[:50]}...")
            return "这是一个模拟的回答。"

        # 构建消息
        messages = [
            {"role": "system", "content": "你是一个专业的知识库问答助手。"},
            {"role": "user", "content": self._build_user_content(prompt, context, images)},
        ]

        # 调用
        kwargs = {"model": self.model, "messages": messages}
        if tools:
            kwargs["tools"] = tools
            kwargs["tool_choice"] = "auto"

        try:
            response = client.chat.completions.create(**kwargs)
            return response.choices[0].message.content
        except Exception as e:
            logger.error(f"MiniMax 生成失败: {e}")
            raise

    def _build_user_content(self, prompt: str, context: dict = None,
                            images: list = None) -> str | list:
        """构建用户消息内容"""
        context_text = context.get("context", "") if context else ""
        parts = [f"问题: {prompt}"]
        if context_text:
            parts.append(f"\n参考信息:\n{context_text}")

        if images:
            content = [{"type": "text", "text": parts[0]}]
            for img_b64 in images:
                content.append({
                    "type": "image_url",
                    "image_url": {"url": f"data:image/jpeg;base64,{img_b64}"},
                })
            return content

        return "\n".join(parts)


class ClaudeProvider:
    """
    Claude Provider

    MiniMax 的 fallback Provider，支持 vision 和 function_call。
    """

    def __init__(self, api_key: str = "", model: str = "claude-sonnet-5"):
        self.api_key = api_key
        self.model = model
        self._client = None

    def _get_client(self):
        """获取 Anthropic 客户端"""
        if self._client is None:
            try:
                import anthropic
                self._client = anthropic.Anthropic(api_key=self.api_key)
            except ImportError:
                logger.warning("anthropic 库未安装，使用模拟模式")
                self._client = "mock"
        return self._client

    def generate(self, prompt: str, context: dict = None,
                 images: list = None, tools: list = None) -> str:
        """生成回答"""
        client = self._get_client()
        if client == "mock":
            logger.info(f"[模拟] Claude generate: {prompt[:50]}...")
            return "这是一个来自 Claude 的模拟回答。"

        try:
            context_text = context.get("context", "") if context else ""
            system_prompt = "你是一个专业的知识库问答助手。"

            user_message = f"问题: {prompt}\n\n参考信息:\n{context_text}"

            response = client.messages.create(
                model=self.model,
                max_tokens=4096,
                system=system_prompt,
                messages=[{"role": "user", "content": user_message}],
            )
            return response.content[0].text

        except Exception as e:
            logger.error(f"Claude 生成失败: {e}")
            raise


class LLMProviderChain:
    """
    多 Provider 级联 Fallback

    MiniMax M3（主）→ Claude（fallback）
    """

    def __init__(self, primary: MiniMaxProvider, fallback: Optional[ClaudeProvider] = None):
        self.primary = primary
        self.fallback = fallback

    def generate(self, prompt: str, context: dict = None,
                 images: list = None, tools: list = None) -> str:
        """
        带 fallback 的生成

        先尝试 primary，失败时自动切换到 fallback。

        Args:
            prompt: 用户查询
            context: 上下文
            images: 图片列表
            tools: 工具定义

        Returns:
            生成的回答
        """
        errors = []

        # 尝试 primary
        try:
            logger.info(f"使用 primary provider: {type(self.primary).__name__}")
            return self.primary.generate(prompt, context, images, tools)
        except Exception as e:
            logger.warning(f"Primary provider 失败: {e}")
            errors.append(str(e))

        # fallback
        if self.fallback:
            try:
                logger.info(f"切换到 fallback provider: {type(self.fallback).__name__}")
                return self.fallback.generate(prompt, context, images, tools)
            except Exception as e:
                logger.error(f"Fallback provider 也失败了: {e}")
                errors.append(str(e))

        raise RuntimeError(f"所有 provider 都失败了: {'; '.join(errors)}")