"""OpenAI 兼容接口 provider。

OpenAI、DeepSeek、通义千问、Moonshot Kimi 等都提供与 OpenAI 一致的
``/chat/completions`` 接口，因此用一个通用实现即可覆盖多家模型。
"""
from __future__ import annotations

import httpx

from .base import LLMError, LLMProvider


class OpenAICompatProvider(LLMProvider):
    """通用 OpenAI 兼容 provider。"""

    def __init__(self, id: str, display_name: str, api_key: str, base_url: str, model: str):
        self.id = id
        self.display_name = display_name
        self.api_key = api_key
        # base_url 可能带或不带结尾斜杠，统一处理
        self.base_url = base_url.rstrip("/")
        self.model = model

    async def complete(self, system: str, user: str) -> str:
        url = f"{self.base_url}/chat/completions"
        payload = {
            "model": self.model,
            "temperature": 0.8,
            "messages": [
                {"role": "system", "content": system},
                {"role": "user", "content": user},
            ],
        }
        headers = {
            "Authorization": f"Bearer {self.api_key}",
            "Content-Type": "application/json",
        }
        async with self._client() as client:
            try:
                resp = await client.post(url, json=payload, headers=headers)
            except httpx.HTTPError as exc:  # 网络层错误
                raise LLMError(f"网络请求失败：{exc}", self.id) from exc

        if resp.status_code != 200:
            raise LLMError(f"{self.display_name} 返回 {resp.status_code}：{resp.text[:300]}", self.id)

        try:
            data = resp.json()
            return data["choices"][0]["message"]["content"]
        except (KeyError, IndexError, ValueError) as exc:
            raise LLMError(f"{self.display_name} 响应解析失败：{resp.text[:300]}", self.id) from exc
