"""音乐生成核心：prompt、解析、渲染。"""
from .midi import midi_filename, render_midi
from .parser import ParseError, parse_composition
from .prompt import SYSTEM_PROMPT, build_user_prompt

__all__ = [
    "SYSTEM_PROMPT",
    "build_user_prompt",
    "parse_composition",
    "ParseError",
    "render_midi",
    "midi_filename",
]
