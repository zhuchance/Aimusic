"""把 :class:`CompositionOut` 渲染为 MIDI 文件。

使用 midiutil 写入标准 MIDI，可直接下载或用任意播放器打开。
前端播放则直接用 Web Audio API 读取 notes JSON，无需依赖此文件。
"""
from __future__ import annotations

import io

from midiutil import MIDIFile

from ..schemas import CompositionOut

# 乐器音色（GM Program）：0=原声大钢琴
ACOUSTIC_GRAND_PIANO = 0


def render_midi(composition: CompositionOut) -> bytes:
    """将曲谱渲染为 MIDI 二进制内容。"""
    midi = MIDIFile(numTracks=1, adjust_origin=True)
    track = 0
    channel = 0

    midi.addTempo(track, 0, composition.tempo)
    midi.addProgramChange(track, channel, 0, ACOUSTIC_GRAND_PIANO)

    for note in composition.notes:
        midi.addNote(
            track=track,
            channel=channel,
            pitch=note.pitch,
            time=note.start,
            duration=note.duration,
            volume=note.velocity,
        )

    buffer = io.BytesIO()
    midi.writeFile(buffer)
    return buffer.getvalue()


def midi_filename(composition: CompositionOut) -> str:
    """生成一个安全的 MIDI 文件名（只保留字母数字中文与连字符）。"""
    safe = "".join(ch for ch in composition.title if ch.isalnum() or ch in {"-", "_", " "}).strip()
    if not safe:
        safe = "aimusic"
    return f"{safe}.mid"
