"""模型注册中心：根据 .env 中配置的 API Key 动态构建可用 provider 列表。

只注册真正配置了 Key 的模型；前端下拉框仅展示这些模型。
"""
from __future__ import annotations

from typing import Dict, List, Optional

from ..config import settings
from .base import LLMError, LLMProvider
from .claude import ClaudeProvider
from .gemini import GeminiProvider
from .openai_compat import OpenAICompatProvider


def build_registry() -> Dict[str, LLMProvider]:
    """扫描配置，返回 {provider_id: provider} 的映射。"""
    registry: Dict[str, LLMProvider] = {}

    # OpenAI 兼容类：OpenAI / DeepSeek / Qwen / Kimi
    for conf in (
        {
            "id": "openai",
            "display": "OpenAI GPT",
            "key": settings.OPENAI_API_KEY,
            "base": settings.OPENAI_BASE_URL,
            "model": settings.OPENAI_MODEL,
        },
        {
            "id": "deepseek",
            "display": "DeepSeek",
            "key": settings.DEEPSEEK_API_KEY,
            "base": settings.DEEPSEEK_BASE_URL,
            "model": settings.DEEPSEEK_MODEL,
        },
        {
            "id": "qwen",
            "display": "通义千问 Qwen",
            "key": settings.QWEN_API_KEY,
            "base": settings.QWEN_BASE_URL,
            "model": settings.QWEN_MODEL,
        },
        {
            "id": "kimi",
            "display": "Moonshot Kimi",
            "key": settings.KIMI_API_KEY,
            "base": settings.KIMI_BASE_URL,
            "model": settings.KIMI_MODEL,
        },
    ):
        if settings.has_key(conf["key"]):
            registry[conf["id"]] = OpenAICompatProvider(
                id=conf["id"],
                display_name=f'{conf["display"]} ({conf["model"]})',
                api_key=conf["key"],
                base_url=conf["base"],
                model=conf["model"],
            )

    # Anthropic Claude
    if settings.has_key(settings.ANTHROPIC_API_KEY):
        registry["claude"] = ClaudeProvider(
            api_key=settings.ANTHROPIC_API_KEY,
            model=settings.ANTHROPIC_MODEL,
        )

    # Google Gemini
    if settings.has_key(settings.GEMINI_API_KEY):
        registry["gemini"] = GeminiProvider(
            api_key=settings.GEMINI_API_KEY,
            model=settings.GEMINI_MODEL,
        )

    return registry


def get_provider(provider_id: str) -> LLMProvider:
    """按 id 获取 provider；不存在时抛出 LLMError。"""
    registry = build_registry()
    provider = registry.get(provider_id)
    if provider is None:
        raise LLMError(
            f"模型 '{provider_id}' 不可用：请在 .env 中配置对应的 API Key。"
            f"当前可用：{', '.join(registry) or '（无，尚未配置任何 Key）'}",
            provider_id,
        )
    return provider


def list_models() -> List[Dict[str, str]]:
    """返回可用模型列表，供前端下拉框展示。"""
    return [
        {"id": p.id, "provider": p.__class__.__name__, "display_name": p.display_name}
        for p in build_registry().values()
    ]
