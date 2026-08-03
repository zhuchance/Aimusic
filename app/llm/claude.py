"""Anthropic Claude provider（Messages API）。"""
from __future__ import annotations

import httpx

from .base import LLMError, LLMProvider

API_URL = "https://api.anthropic.com/v1/messages"
API_VERSION = "2023-06-01"


class ClaudeProvider(LLMProvider):
    """调用 Anthropic Claude。"""

    def __init__(self, api_key: str, model: str):
        self.id = "claude"
        self.display_name = "Anthropic Claude"
        self.api_key = api_key
        self.model = model

    async def complete(self, system: str, user: str) -> str:
        payload = {
            "model": self.model,
            "max_tokens": 8000,
            "temperature": 0.8,
            "system": system,
            "messages": [{"role": "user", "content": user}],
        }
        headers = {
            "x-api-key": self.api_key,
            "anthropic-version": API_VERSION,
            "Content-Type": "application/json",
        }
        async with self._client() as client:
            try:
                resp = await client.post(API_URL, json=payload, headers=headers)
            except httpx.HTTPError as exc:
                raise LLMError(f"网络请求失败：{exc}", self.id) from exc

        if resp.status_code != 200:
            raise LLMError(f"Claude 返回 {resp.status_code}：{resp.text[:300]}", self.id)

        try:
            data = resp.json()
            # Claude 返回 content 是一个 block 列表，拼接所有 text block
            blocks = data.get("content", [])
            text = "".join(b.get("text", "") for b in blocks if b.get("type") == "text")
            if not text:
                raise KeyError("content text 为空")
            return text
        except (KeyError, ValueError) as exc:
            raise LLMError(f"Claude 响应解析失败：{resp.text[:300]}", self.id) from exc
