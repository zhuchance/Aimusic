"""Google Gemini provider（Generative Language API）。"""
from __future__ import annotations

import httpx

from .base import LLMError, LLMProvider

API_BASE = "https://generativelanguage.googleapis.com/v1beta/models"


class GeminiProvider(LLMProvider):
    """调用 Google Gemini。"""

    def __init__(self, api_key: str, model: str):
        self.id = "gemini"
        self.display_name = "Google Gemini"
        self.api_key = api_key
        self.model = model

    async def complete(self, system: str, user: str) -> str:
        url = f"{API_BASE}/{self.model}:generateContent"
        payload = {
            "system_instruction": {"parts": [{"text": system}]},
            "contents": [{"role": "user", "parts": [{"text": user}]}],
            "generationConfig": {"temperature": 0.8},
        }
        headers = {"x-goog-api-key": self.api_key, "Content-Type": "application/json"}
        async with self._client() as client:
            try:
                resp = await client.post(url, json=payload, headers=headers)
            except httpx.HTTPError as exc:
                raise LLMError(f"网络请求失败：{exc}", self.id) from exc

        if resp.status_code != 200:
            raise LLMError(f"Gemini 返回 {resp.status_code}：{resp.text[:300]}", self.id)

        try:
            data = resp.json()
            parts = data["candidates"][0]["content"]["parts"]
            text = "".join(p.get("text", "") for p in parts)
            if not text:
                raise KeyError("parts text 为空")
            return text
        except (KeyError, IndexError, ValueError) as exc:
            raise LLMError(f"Gemini 响应解析失败：{resp.text[:300]}", self.id) from exc
