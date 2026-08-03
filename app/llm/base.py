"""LLM provider 抽象基类。

所有主流大模型（OpenAI / Claude / DeepSeek / Gemini / Qwen / Kimi 等）
都实现统一接口 :meth:`LLMProvider.complete`，返回模型的文本输出。
"""
from __future__ import annotations

from abc import ABC, abstractmethod
from typing import Optional

import httpx

# 统一的请求超时（秒）
DEFAULT_TIMEOUT = httpx.Timeout(120.0, connect=15.0)


class LLMError(RuntimeError):
    """调用大模型失败时抛出。"""

    def __init__(self, message: str, provider: str = ""):
        super().__init__(message)
        self.provider = provider


class LLMProvider(ABC):
    """大模型统一调用接口。"""

    #: 模型唯一 id，例如 "openai" / "claude" / "deepseek"
    id: str = ""
    #: 面向用户展示的名称
    display_name: str = ""
    #: 具体模型名，例如 "gpt-4o"
    model: str = ""

    @abstractmethod
    async def complete(self, system: str, user: str) -> str:
        """发起一次对话补全，返回模型生成的文本。

        :param system: 系统提示词
        :param user: 用户提示词
        :return: 模型输出的纯文本
        :raises LLMError: 网络或 API 调用失败
        """
        raise NotImplementedError

    @staticmethod
    def _client() -> httpx.AsyncClient:
        return httpx.AsyncClient(timeout=DEFAULT_TIMEOUT)

    def __repr__(self) -> str:  # pragma: no cover - 仅调试
        return f"<{self.__class__.__name__} id={self.id} model={self.model}>"
