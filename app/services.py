"""生成流程编排：串起模型调用、曲谱解析与 MIDI 渲染。"""
from __future__ import annotations

import uuid
from pathlib import Path

from .config import settings
from .llm import get_provider
from .music import build_user_prompt, midi_filename, parse_composition, render_midi
from .music.prompt import SYSTEM_PROMPT
from .schemas import CompositionOut, GenerateRequest

# 生成产物目录（MIDI 落盘，供下载）
OUTPUT_DIR = Path(__file__).resolve().parent.parent / "output"
OUTPUT_DIR.mkdir(exist_ok=True)


class GenerationError(RuntimeError):
    """生成失败的统一业务异常。"""


async def generate_song(req: GenerateRequest) -> tuple[CompositionOut, str]:
    """执行一次完整的作曲生成。

    :return: (曲谱对象, MIDI 文件名)。MIDI 已保存到 output/ 目录。
    :raises GenerationError: 模型调用失败或输出无法解析
    """
    provider = get_provider(req.provider)

    user_prompt = build_user_prompt(
        prompt=req.prompt,
        style=req.style,
        mood=req.mood,
        tempo=req.tempo,
        duration_bars=req.duration_bars,
    )

    # 1. 调用大模型生成曲谱 JSON 文本
    raw_text = await provider.complete(SYSTEM_PROMPT, user_prompt)

    # 2. 解析 + 校验 + 规范化
    default_tempo = req.tempo or settings.DEFAULT_TEMPO
    try:
        composition = parse_composition(raw_text, default_tempo=default_tempo)
    except Exception as exc:  # ParseError 及其他解析异常
        raise GenerationError(f"模型输出无法解析为有效曲谱：{exc}") from exc

    # 3. 渲染 MIDI 并落盘（文件名加随机后缀避免覆盖）
    midi_bytes = render_midi(composition)
    base_name = midi_filename(composition)
    unique_name = f"{uuid.uuid4().hex[:8]}_{base_name}"
    (OUTPUT_DIR / unique_name).write_bytes(midi_bytes)

    composition.midi_url = f"/api/midi/{unique_name}"
    return composition, unique_name
