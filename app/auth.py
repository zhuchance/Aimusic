"""微信小程序登录：code 换 openid，签发本地 token。

未配置 WX_APPID / WX_SECRET 时登录接口会明确报错，方便联调定位。
"""
from __future__ import annotations

import secrets
from typing import Optional

import httpx

from .config import settings
from .db import get_user_by_token, upsert_user

JSCode2Session_URL = "https://api.weixin.qq.com/sns/jscode2session"


class AuthError(RuntimeError):
    """登录认证失败。"""


async def wx_code_to_openid(code: str) -> str:
    """调用微信 jscode2session，返回 openid。"""
    if not settings.WX_APPID or not settings.WX_SECRET:
        raise AuthError("后端未配置 WX_APPID / WX_SECRET，无法登录。请在 .env 中填写小程序 AppID 与 Secret。")
    params = {
        "appid": settings.WX_APPID,
        "secret": settings.WX_SECRET,
        "js_code": code,
        "grant_type": "authorization_code",
    }
    async with httpx.AsyncClient(timeout=15.0) as client:
        try:
            resp = await client.get(JSCode2Session_URL, params=params)
        except httpx.HTTPError as exc:
            raise AuthError(f"微信登录网络请求失败：{exc}") from exc
    data = resp.json()
    if data.get("errcode"):
        raise AuthError(f"微信登录失败：{data.get('errmsg', '未知错误')}")
    openid = data.get("openid")
    if not openid:
        raise AuthError("微信未返回 openid，请确认小程序 AppID 与 Secret 匹配")
    return openid


async def login_with_code(code: str) -> tuple[str, str]:
    """根据 code 登录，返回 (openid, token)。"""
    openid = await wx_code_to_openid(code)
    token = secrets.token_hex(16)
    upsert_user(openid, token)
    return openid, token


def resolve_user(token: str) -> Optional[dict]:
    """按 token 解析用户；无效返回 None。"""
    if not token:
        return None
    return get_user_by_token(token)