# Aimusic 🎵

用**主流大模型 AI** 生成音乐的 Web 应用。输入一句创作主题，选择你偏好的大模型，即可得到一段可试听、可下载 MIDI 的旋律。

> 本项目由早期的 TensorFlow 实验性仓库重构而来，旧版无实际生成逻辑的代码已全部清理。

## 特性

- **多模型可选**：OpenAI (GPT-4o)、Anthropic Claude、Google Gemini、DeepSeek、通义千问、Moonshot Kimi……配置了对应 Key 即出现在下拉框。
- **乐理约束生成**：通过精心设计的 prompt 让模型输出结构化旋律（调性 / 速度 / 拍号 / MIDI 音符），后端严格校验并规范化。
- **即时试听**：前端用 Web Audio API 直接合成播放，无需安装任何音频软件。
- **MIDI 下载**：一键导出标准 `.mid` 文件，可导入 GarageBand、MuseScore、FL Studio 等继续编辑。
- **可视化钢琴卷帘**：直观查看生成的音符分布。
- **后端 REST API**：前端可替换为微信小程序等任意客户端。

## 快速开始

```bash
# 1. 创建虚拟环境并安装依赖
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt

# 2. 配置至少一个大模型的 API Key
cp .env.example .env
#    用编辑器打开 .env，填入你想用的模型的 Key

# 3. 启动
uvicorn app.main:app --reload
```

打开浏览器访问 [http://127.0.0.1:8000](http://127.0.0.1:8000) 即可。

## 配置模型

在 `.env` 中填写对应模型的 Key，**只配置你想用的即可**，未配置的模型不会出现在下拉框：

| 模型 | 环境变量 | 申请入口 |
|------|----------|----------|
| OpenAI GPT | `OPENAI_API_KEY` | https://platform.openai.com/api-keys |
| Anthropic Claude | `ANTHROPIC_API_KEY` | https://console.anthropic.com/ |
| Google Gemini | `GEMINI_API_KEY` | https://aistudio.google.com/app/apikey |
| DeepSeek | `DEEPSEEK_API_KEY` | https://platform.deepseek.com/ |
| 通义千问 Qwen | `QWEN_API_KEY` | https://dashscope.console.aliyun.com/ |
| Moonshot Kimi | `KIMI_API_KEY` | https://platform.moonshot.cn/ |

每个模型还可单独指定具体型号（如 `OPENAI_MODEL=gpt-4o-mini`）。

## 工作原理

```
用户输入主题/风格/情绪
        │
        ▼
[构建 prompt] ──► [调用所选大模型] ──► 返回结构化 JSON 旋律
        │
        ▼
[解析 + 校验 + 规范化]（容错提取 JSON、音名转 MIDI、范围夹取）
        │
        ▼
[渲染] ──► MIDI 文件（可下载） + notes JSON（前端 Web Audio 播放）
```

## API

| 方法 | 路径 | 说明 |
|------|------|------|
| `GET` | `/api/models` | 返回已配置的可用模型列表 |
| `POST` | `/api/generate` | 生成曲子，返回曲谱 JSON |
| `GET` | `/api/midi/{filename}` | 下载生成的 MIDI 文件 |

`POST /api/generate` 请求体示例：

```json
{
  "prompt": "一首欢快的中国风民谣",
  "provider": "deepseek",
  "style": "民谣",
  "mood": "欢快",
  "tempo": 110,
  "duration_bars": 8
}
```

交互式文档见 [http://127.0.0.1:8000/docs](http://127.0.0.1:8000/docs)（FastAPI 自动生成）。

## 目录结构

```
aimusic/
├── app/
│   ├── main.py           # FastAPI 入口与路由
│   ├── config.py         # 环境变量配置
│   ├── schemas.py        # Pydantic 数据模型
│   ├── services.py       # 生成流程编排
│   ├── llm/              # 各大模型 provider
│   │   ├── base.py       # 抽象接口
│   │   ├── openai_compat.py  # OpenAI/DeepSeek/Qwen/Kimi 通用实现
│   │   ├── claude.py
│   │   ├── gemini.py
│   │   └── registry.py   # 按配置动态注册可用模型
│   └── music/
│       ├── prompt.py     # 作曲 prompt 模板
│       ├── parser.py     # 解析校验模型输出
│       └── midi.py       # 渲染 MIDI
├── static/               # Web 前端（html/js/css）
├── output/               # 生成的 MIDI（已 gitignore）
├── .env.example          # 环境变量模板
├── requirements.txt
└── smoke_test.py         # 离线冒烟测试（无需真实 Key）
```

## 测试

```bash
python smoke_test.py   # 使用 .venv 里的 python，无需配置 API Key
```

## 版权

Copyright 2018 zhuchance（couchance@gmail.com）

Apache License 2.0，详情见 [LICENSE](LICENSE)。
