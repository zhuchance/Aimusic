"""Pydantic 数据模型：API 请求/响应与曲谱结构定义。

曲谱采用结构化中间格式：AI 输出一段 JSON（音符列表），
后端再将其渲染为 MIDI / 供前端用 Web Audio 播放。
"""
from __future__ import annotations

from typing import List, Optional

from pydantic import BaseModel, Field


# ========================= 请求 / 响应 =========================
class GenerateRequest(BaseModel):
    """生成曲子的请求体。"""

    prompt: str = Field(..., description="创作主题 / 描述，例如：一首欢快的中国风民谣")
    provider: str = Field(..., description="模型 id，例如 openai / claude / deepseek / gemini")
    style: Optional[str] = Field(None, description="风格，例如：流行、古典、电子、民谣")
    mood: Optional[str] = Field(None, description="情绪，例如：欢快、忧伤、激昂、宁静")
    tempo: Optional[int] = Field(None, ge=40, le=220, description="速度 BPM，缺省用默认值")
    duration_bars: Optional[int] = Field(8, ge=4, le=32, description="小节数，缺省 8")


class NoteOut(BaseModel):
    """单个音符（对外输出格式）。"""

    pitch: int = Field(..., description="MIDI 音高 0-127")
    start: float = Field(..., description="起始拍（以拍为单位）")
    duration: float = Field(..., description="时值（拍）")
    velocity: int = Field(90, ge=0, le=127, description="力度 0-127")


class CompositionOut(BaseModel):
    """生成结果：一首完整的曲子。"""

    title: str
    composer_note: Optional[str] = Field(None, description="模型给出的创作说明")
    key: str = Field("C major", description="调性，例如 C major / A minor")
    tempo: int = Field(100, description="速度 BPM")
    time_signature: str = Field("4/4", description="拍号")
    notes: List[NoteOut] = Field(default_factory=list, description="主旋律音符")
    midi_url: Optional[str] = Field(None, description="MIDI 文件下载地址")


class ModelInfo(BaseModel):
    """可用模型信息。"""

    id: str
    provider: str
    display_name: str


# ========================= 内部解析结构 =========================
class RawNote(BaseModel):
    """AI 返回 JSON 中的单个音符（宽容解析）。"""

    pitch: Optional[int] = None
    note: Optional[str] = Field(None, description="音名，例如 C4 / D#5，兼容不支持数字的模型")
    start: float = 0.0
    duration: float = 1.0
    velocity: int = 90


class RawComposition(BaseModel):
    """AI 返回 JSON 的顶层结构（宽容解析）。"""

    title: Optional[str] = "Untitled"
    key: Optional[str] = "C major"
    tempo: Optional[int] = None
    time_signature: Optional[str] = "4/4"
    description: Optional[str] = None
    notes: List[RawNote] = Field(default_factory=list)
