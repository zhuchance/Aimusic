"""FastAPI 应用入口：提供 REST API 与 Web 前端。

启动：uvicorn app.main:app --reload
访问：http://127.0.0.1:8000
"""
from __future__ import annotations

from pathlib import Path

from fastapi import FastAPI, HTTPException
from fastapi.responses import FileResponse, JSONResponse
from fastapi.staticfiles import StaticFiles

from .llm import LLMError
from .schemas import CompositionOut, GenerateRequest, ModelInfo
from .services import OUTPUT_DIR, GenerationError, generate_song

BASE_DIR = Path(__file__).resolve().parent.parent
STATIC_DIR = BASE_DIR / "static"

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


# ========================= 前端静态页面 =========================
@app.get("/", include_in_schema=False)
async def index():
    return FileResponse(STATIC_DIR / "index.html")


# 挂载 /static（js / css / 资源）
app.mount("/static", StaticFiles(directory=STATIC_DIR), name="static")


@app.exception_handler(Exception)
async def unhandled_exception_handler(request, exc):  # pragma: no cover
    return JSONResponse(status_code=500, content={"detail": f"服务器内部错误：{exc}"})
