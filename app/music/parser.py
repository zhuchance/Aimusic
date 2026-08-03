"""解析并校验大模型返回的曲谱 JSON。

模型输出可能带有 Markdown 代码块标记、前后说明文字或非法字段，
这里做宽容提取 + 严格规范化，最终产出可直接渲染的 :class:`CompositionOut`。
"""
from __future__ import annotations

import json
import re
from typing import List, Optional

from ..schemas import CompositionOut, NoteOut, RawComposition, RawNote

# 音名 -> 半音偏移（用于把 "C4" / "D#5" 这类音名转成 MIDI 音高）
_PITCH_CLASS = {
    "C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11,
}


class ParseError(ValueError):
    """模型输出无法解析为有效曲谱。"""


def extract_json(text: str) -> dict:
    """从模型文本中提取第一个 JSON 对象。

    优先去掉 ``` 代码块标记；若仍失败，则截取第一个 '{' 到最后一个 '}'。
    """
    cleaned = text.strip()
    # 去掉 markdown 代码块围栏
    cleaned = re.sub(r"^```(?:json)?\s*", "", cleaned, flags=re.IGNORECASE)
    cleaned = re.sub(r"\s*```$", "", cleaned)

    try:
        return json.loads(cleaned)
    except json.JSONDecodeError:
        pass

    # 退而求其次：截取花括号包围的部分
    start, end = cleaned.find("{"), cleaned.rfind("}")
    if start != -1 and end != -1 and end > start:
        snippet = cleaned[start : end + 1]
        try:
            return json.loads(snippet)
        except json.JSONDecodeError as exc:
            raise ParseError(f"JSON 解析失败：{exc}") from exc

    raise ParseError("模型输出中未找到 JSON 对象")


def _note_name_to_pitch(name: str) -> Optional[int]:
    """把音名（如 C4 / D#5 / Eb3）转成 MIDI 音高；失败返回 None。"""
    m = re.fullmatch(r"([A-Ga-g])([#b]?)(-?\d)", name.strip())
    if not m:
        return None
    letter, accidental, octave = m.group(1).upper(), m.group(2), int(m.group(3))
    base = _PITCH_CLASS.get(letter)
    if base is None:
        return None
    if accidental == "#":
        base += 1
    elif accidental == "b":
        base -= 1
    # MIDI：C4=60 => (octave+1)*12 + pitch_class
    return (octave + 1) * 12 + base


def _normalize_pitch(note: RawNote) -> Optional[int]:
    """优先用 pitch，其次尝试解析音名 note。"""
    if note.pitch is not None:
        return int(note.pitch)
    if note.note:
        return _note_name_to_pitch(note.note)
    return None


def parse_composition(
    text: str,
    default_tempo: int = 100,
) -> CompositionOut:
    """把模型输出文本解析为 :class:`CompositionOut`。

    :raises ParseError: 输出无法解析或没有任何有效音符
    """
    data = extract_json(text)
    try:
        raw = RawComposition(**data)
    except Exception as exc:  # pydantic 校验错误
        raise ParseError(f"曲谱结构不合法：{exc}") from exc

    notes: List[NoteOut] = []
    for raw_note in raw.notes:
        pitch = _normalize_pitch(raw_note)
        if pitch is None:
            continue  # 跳过无法识别音高的音符
        # 规范化到 MIDI 合法范围，并夹到舒适音区
        pitch = max(0, min(127, int(pitch)))
        start = max(0.0, float(raw_note.start))
        duration = max(0.05, float(raw_note.duration))
        velocity = max(1, min(127, int(raw_note.velocity)))
        notes.append(NoteOut(pitch=pitch, start=start, duration=duration, velocity=velocity))

    if not notes:
        raise ParseError("解析后没有任何有效音符")

    # 按起始拍排序，保证播放/MIDI 写入顺序正确
    notes.sort(key=lambda n: n.start)

    tempo = raw.tempo or default_tempo
    tempo = max(40, min(220, int(tempo)))

    return CompositionOut(
        title=raw.title or "Untitled",
        composer_note=raw.description,
        key=raw.key or "C major",
        tempo=tempo,
        time_signature=raw.time_signature or "4/4",
        notes=notes,
    )
