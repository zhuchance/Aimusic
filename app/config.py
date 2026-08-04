"""配置加载：从环境变量 / .env 读取各大模型的 API Key 与默认参数。

只有配置了对应 API Key 的模型才会被注册进可用模型列表。
"""
from __future__ import annotations

from typing import Optional

from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    """应用全局配置。"""

    model_config = SettingsConfigDict(
        env_file=".env",
        env_file_encoding="utf-8",
        case_sensitive=True,
        extra="ignore",
    )

    # ---- OpenAI ----
    OPENAI_API_KEY: Optional[str] = None
    OPENAI_MODEL: str = "gpt-4o"
    OPENAI_BASE_URL: str = "https://api.openai.com/v1"

    # ---- Anthropic Claude ----
    ANTHROPIC_API_KEY: Optional[str] = None
    ANTHROPIC_MODEL: str = "claude-sonnet-4-20250514"

    # ---- DeepSeek ----
    DEEPSEEK_API_KEY: Optional[str] = None
    DEEPSEEK_MODEL: str = "deepseek-chat"
    DEEPSEEK_BASE_URL: str = "https://api.deepseek.com/v1"

    # ---- Google Gemini ----
    GEMINI_API_KEY: Optional[str] = None
    GEMINI_MODEL: str = "gemini-2.0-flash"

    # ---- 通义千问 Qwen ----
    QWEN_API_KEY: Optional[str] = None
    QWEN_MODEL: str = "qwen-plus"
    QWEN_BASE_URL: str = "https://dashscope.aliyuncs.com/compatible-mode/v1"

    # ---- Moonshot Kimi ----
    KIMI_API_KEY: Optional[str] = None
    KIMI_MODEL: str = "moonshot-v1-8k"
    KIMI_BASE_URL: str = "https://api.moonshot.cn/v1"

    # ---- 微信小程序登录（云同步历史用，可选）----
    WX_APPID: Optional[str] = None
    WX_SECRET: Optional[str] = None

    # ---- 生成默认参数 ----
    DEFAULT_TEMPO: int = 100

    def has_key(self, value: Optional[str]) -> bool:
        """判断一个 Key 是否真正可用（非空且不是模板占位）。"""
        return bool(value) and value.strip() not in {"", "sk-xxx", "your-key"}


settings = Settings()
