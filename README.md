# Aimusic 🎵

用**主流大模型 AI** 生成音乐的 Web 应用。输入一句创作主题，选择你偏好的大模型，即可得到一段可试听、可下载 MIDI 的旋律。

> 本项目由早期的 TensorFlow 实验性仓库重构而来，旧版无实际生成逻辑的代码已全部清理。

## 特性

- **多模型可选**：OpenAI (GPT-4o)、Anthropic Claude、Google Gemini、DeepSeek、通义千问、Moonshot Kimi……配置了对应 Key 即出现在下拉框。
- **乐理约束生成**：通过精心设计的 prompt 让模型输出结构化旋律（调性 / 速度 / 拍号 / MIDI 音符），后端严格校验并规范化。
- **即时试听**：前端用 Web Audio API 直接合成播放，无需安装任何音频软件。
- **MIDI 下载**：一键导出标准 `.mid` 文件，可导入 GarageBand、MuseScore、FL Studio 等继续编辑。
- **可视化钢琴卷帘**：直观查看生成的音符分布，长曲目可横向滚动。
- **历史记录**：生成结果自动保存为本地历史，随时回看 / 重播 / 再导出。
- **微信小程序端**：`miniprogram/` 提供完整的小程序工程，支持登录后云同步历史（跨设备找回）。
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
| `POST` | `/api/wx/login` | 微信登录（code 换 token），云同步历史用 |
| `GET` | `/api/history` | 当前用户的历史记录（需 `Authorization: Bearer <token>`） |
| `POST` | `/api/history` | 保存一条历史记录（需登录） |
| `DELETE` | `/api/history/{id}` | 删除一条历史记录（需登录） |

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

## 微信小程序端

架构不受影响：后端是纯 REST API，小程序用 `wx.request` 直接调用，Web 端逻辑改动为零，只需新增一个小程序工程。

启动后端后，用微信开发者工具导入 `miniprogram/` 目录即可：

1. **开发者工具**下载：<https://developers.weixin.qq.com/miniprogram/dev/devtools/download.html>，扫码后用**游客模式**即可起步预览。
2. **启动后端**：`uvicorn app.main:app --reload`。
3. **导入项目**：工具 → 导入 → 选择 `miniprogram/` 目录。

### BASE_URL 配置

未登录跳过域名校验（`project.config.json` 已设 `urlCheck: false`），只需在 `miniprogram/utils/config.js` 调整地址：

| 场景 | BASE_URL |
|------|----------|
| 开发者工具模拟器 | `http://127.0.0.1:8000` |
| 真机预览（同一局域网） | `http://<电脑局域网IP>:8000` |
| 正式上线 | 已备案 HTTPS 域名，并在公众平台配置 request / downloadFile 合法域名 |

### 登录与云同步

- **本地历史**开箱即用，无需任何配置。
- **云同步**需在 `.env` 填写真实的小程序 `WX_APPID` / `WX_SECRET`（小程序后台「开发管理 → 开发设置」获取），且开发者工具不能再选游客模式，需填入你的 AppID。登录成功后，历史记录页「☁ 同步」会先上传本地未同步记录，再拉取云端列表整体合并（云端条目带「云」角标）。

### 小程序目录

```
miniprogram/
├── project.config.json       # 项目配置（游客 appid / 跳过域名校验）
├── app.json                  # 全局配置 + app.json tabBar（作曲 / 历史）
├── utils/
│   ├── config.js             # BASE_URL（改这里切换地址）
│   ├── api.js                # wx.request 封装 + token 注入 + 登录/历史接口
│   ├── history.js            # 本地历史记录存储（wx storage）
│   └── audio.js              # wx.createWebAudioContext 播放器（试听）
└── pages/
    ├── index/                # 作曲页：生成 + 试听 + 钢琴卷帘 + 导出 MIDI
    └── history/              # 历史页：列表 / 播放 / 删除 / 登录云同步
```

## 目录结构

```
aimusic/
├── app/
│   ├── main.py           # FastAPI 入口与路由
│   ├── config.py         # 环境变量配置
│   ├── schemas.py        # Pydantic 数据模型
│   ├── services.py       # 生成流程编排
│   ├── db.py             # SQLite 存储（用户 / 云端历史）
│   ├── auth.py           # 微信登录与 token 认证
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
├── miniprogram/          # 微信小程序端
├── static/               # Web 前端（html/js/css）
├── output/               # 生成的 MIDI（已 gitignore）
├── aimusic.db            # SQLite（已 gitignore，应用启动自动创建）
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
