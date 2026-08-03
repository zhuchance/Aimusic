"""作曲 prompt 模板。

约束大模型严格输出 JSON 格式的旋律数据，便于后端解析与渲染。
"""
from __future__ import annotations

from typing import Optional

SYSTEM_PROMPT = """你是一位专业的作曲家和音乐制作人，精通乐理与旋律写作。
你的任务是根据用户的主题创作一段完整、好听、有乐理依据的主旋律。

【输出要求】只输出一个 JSON 对象，不要输出任何 JSON 之外的文字、解释或 Markdown 代码块标记。

JSON 结构：
{
  "title": "曲名（简短有诗意）",
  "key": "调性，如 C major / G major / A minor",
  "tempo": 整数 BPM,
  "time_signature": "拍号，如 4/4 或 3/4",
  "description": "一两句创作思路说明",
  "notes": [
    {"pitch": MIDI音高整数, "start": 起始拍(浮点), "duration": 时值拍数(浮点), "velocity": 力度0-127}
  ]
}

【乐理约束】
1. pitch 为 MIDI 音高整数：中央 C(C4)=60，范围建议 48~84（一个多八度的舒适音区）。
2. start 与 duration 以“拍”为单位。以 4/4 拍为例：四分音符=1拍，八分音符=0.5拍，二分音符=2拍，附点四分=1.5拍。
3. 旋律要连贯：相邻音符 start 等于上一个音符的 start+duration，可留少量休止（用 gap 表示，不必输出休止符）。
4. 音符需落在所给调性的音阶上，围绕主音与属音，结尾收在主音上。
5. 旋律要有起伏（乐句呼吸），避免长时间同音或无规律大跳。
6. notes 数量要覆盖用户要求的小节数（总拍数 ≈ 小节数 × 拍号分子）。

严格遵守：只返回 JSON，不要多余内容。"""


def build_user_prompt(
    prompt: str,
    style: Optional[str],
    mood: Optional[str],
    tempo: Optional[int],
    duration_bars: Optional[int],
) -> str:
    """拼接用户侧提示词。"""
    lines = [f"创作主题：{prompt}"]
    if style:
        lines.append(f"风格：{style}")
    if mood:
        lines.append(f"情绪：{mood}")
    if tempo:
        lines.append(f"速度：{tempo} BPM")
    bars = duration_bars or 8
    lines.append(f"长度：{bars} 小节")
    lines.append("请按系统要求只输出符合结构的 JSON。")
    return "\n".join(lines)
