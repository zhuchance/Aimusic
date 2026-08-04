"""FastAPI 应用入口：提供 REST API 与 Web 前端。

启动：uvicorn app.main:app --reload
访问：http://127.0.0.1:8000
"""
from __future__ import annotations

from pathlib import Path

from fastapi import Depends, FastAPI, Header, HTTPException
from fastapi.responses import FileResponse, JSONResponse
from fastapi.staticfiles import StaticFiles

from . import auth, db
from .llm import LLMError
from .schemas import (
    CompositionOut,
    GenerateRequest,
    HistoryItemIn,
    HistoryItemOut,
    LoginOut,
    MessageOut,
    ModelInfo,
    WxLoginRequest,
)
from .services import OUTPUT_DIR, GenerationError, generate_song

BASE_DIR = Path(__file__).resolve().parent.parent
STATIC_DIR = BASE_DIR / "static"

db.init_db()

app = FastAPI(
    title="Aimusic",
    description="用主流大模型 AI 生成音乐",
    version="1.0.0",
)


# ========================= API 路由 =========================
@app.get("/api/models", response_model=list[ModelInfo])
async def list_models():
    """返回已配置 API Key 的可用模型列表。"""
    from .llm import list_models as _list

    return _list()


@app.post("/api/generate", response_model=CompositionOut)
async def generate(req: GenerateRequest):
    """根据主题与所选模型生成一首曲子。"""
    if not req.prompt.strip():
        raise HTTPException(status_code=400, detail="创作主题不能为空")
    try:
        composition, _ = await generate_song(req)
        return composition
    except LLMError as exc:
        raise HTTPException(status_code=502, detail=str(exc)) from exc
    except GenerationError as exc:
        raise HTTPException(status_code=422, detail=str(exc)) from exc


@app.get("/api/midi/{filename}")
async def download_midi(filename: str):
    """下载生成的 MIDI 文件（校验文件名，防止路径穿越）。"""
    if "/" in filename or "\\" in filename or ".." in filename:
        raise HTTPException(status_code=400, detail="非法文件名")
    file_path = OUTPUT_DIR / filename
    if not file_path.exists() or not file_path.is_file():
        raise HTTPException(status_code=404, detail="MIDI 文件不存在")
    return FileResponse(
        file_path,
        media_type="audio/midi",
        filename=filename,
    )


# ========================= 登录 / 云同步历史 =========================
@app.post("/api/wx/login", response_model=LoginOut)
async def wx_login(req: WxLoginRequest):
    """微信小程序登录：code 换 openid，签发 token。"""
    try:
        openid, token = await auth.login_with_code(req.code)
    except auth.AuthError as exc:
        raise HTTPException(status_code=401, detail=str(exc)) from exc
    return LoginOut(token=token, openid=openid)


def current_user(authorization: str = Header(None)) -> dict:
    """从 Authorization: Bearer <token> 解析当前用户。"""
    if not authorization or not authorization.startswith("Bearer "):
        raise HTTPException(status_code=401, detail="未登录")
    user = auth.resolve_user(authorization[7:])
    if user is None:
        raise HTTPException(status_code=401, detail="登录已失效，请重新登录")
    return user


@app.get("/api/history", response_model=list[HistoryItemOut])
async def list_history(user: dict = Depends(current_user)):
    """返回当前用户的历史记录（新到旧）。"""
    return db.list_history(user["openid"])


@app.post("/api/history", response_model=HistoryItemOut)
async def create_history(item: HistoryItemIn, user: dict = Depends(current_user)):
    """保存一条历史记录。"""
    return db.add_history(user["openid"], item)


@app.delete("/api/history/{history_id}", response_model=MessageOut)
async def delete_history(history_id: int, user: dict = Depends(current_user)):
    """删除一条历史记录。"""
    if not db.delete_history(user["openid"], history_id):
        raise HTTPException(status_code=404, detail="记录不存在")
    return MessageOut(detail="已删除")


# ========================= 前端静态页面 =========================
@app.get("/", include_in_schema=False)
async def index():
    return FileResponse(STATIC_DIR / "index.html")


# 挂载 /static（js / css / 资源）
app.mount("/static", StaticFiles(directory=STATIC_DIR), name="static")


@app.exception_handler(Exception)
async def unhandled_exception_handler(request, exc):  # pragma: no cover
    return JSONResponse(status_code=500, content={"detail": f"服务器内部错误：{exc}"})
