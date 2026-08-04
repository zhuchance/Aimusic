"""离线冒烟测试：不依赖真实 API Key，验证解析 + MIDI 渲染 + API 路由注册。

运行：.venv/bin/python smoke_test.py
"""
import json
import sys

# 1) 解析器：模拟一段模型输出（带 markdown 围栏 + 多余文字）
from app.music.parser import parse_composition, ParseError

fake_output = """好的，这是为您创作的旋律：
```json
{
  "title": "江南晨曲",
  "key": "C major",
  "tempo": 96,
  "time_signature": "4/4",
  "description": "以五声音阶描绘水乡清晨的宁静。",
  "notes": [
    {"pitch": 60, "start": 0.0, "duration": 1.0, "velocity": 90},
    {"pitch": 62, "start": 1.0, "duration": 1.0, "velocity": 90},
    {"pitch": 64, "start": 2.0, "duration": 1.0, "velocity": 90},
    {"pitch": 67, "start": 3.0, "duration": 1.0, "velocity": 90},
    {"pitch": 69, "start": 4.0, "duration": 2.0, "velocity": 95},
    {"note": "C5", "start": 6.0, "duration": 2.0, "velocity": 100}
  ]
}
```
希望你喜欢！"""

comp = parse_composition(fake_output)
assert comp.title == "江南晨曲", comp.title
assert comp.tempo == 96
assert len(comp.notes) == 6, f"期望 6 个音符，实际 {len(comp.notes)}"
# 最后一个音是 C5=72
assert comp.notes[-1].pitch == 72, comp.notes[-1].pitch
print("[OK] parser：正确提取 JSON 并把音名 C5 转成 MIDI 72")

# 2) MIDI 渲染
from app.music.midi import render_midi, midi_filename

midi_bytes = render_midi(comp)
assert midi_bytes[:4] == b"MThd", "不是合法 MIDI 文件头"
assert len(midi_bytes) > 50
print(f"[OK] midi：生成 {len(midi_bytes)} 字节的合法 MIDI，文件名 {midi_filename(comp)}")

# 3) 异常路径：非法输出应抛出 ParseError
try:
    parse_composition("这不是 JSON")
    print("[FAIL] 应当抛出 ParseError")
    sys.exit(1)
except ParseError:
    print("[OK] parser：非法输入正确抛出 ParseError")

# 4) FastAPI 路由是否注册成功
from app.main import app

routes = {getattr(r, "path", "") for r in app.routes}
for need in (
    "/api/models",
    "/api/generate",
    "/api/midi/{filename}",
    "/api/wx/login",
    "/api/history",
    "/api/history/{history_id}",
    "/",
):
    assert need in routes, f"缺少路由 {need}"
print("[OK] fastapi：所有关键路由已注册")

# 5) /api/models 在无 Key 时返回空列表（而不是报错）
from app.llm import list_models

models = list_models()
assert isinstance(models, list)
print(f"[OK] registry：当前可用模型 {len(models)} 个（未配置 Key 时应为 0）")

print("\n全部冒烟测试通过 ✅")
