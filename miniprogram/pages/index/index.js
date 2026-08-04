// 作曲页逻辑：加载模型、生成、试听、钢琴卷帘、导出 MIDI。
"use strict";

const { fetchModels, generate, downloadMidi } = require("../../utils/api");
const historyStore = require("../../utils/history");
const createPlayer = require("../../utils/audio");

Page({
  data: {
    models: [],
    modelIndex: 0,
    prompt: "",
    style: "",
    mood: "",
    tempo: "",
    bars: "8",
    loading: false,
    error: "",
    composition: null,
    isPlaying: false,
    rollStyle: "",
  },

  onLoad() {
    this.player = createPlayer();
    this.loadModels();
  },

  onUnload() {
    if (this.player) this.player.stop();
  },

  // ---------- 模型列表 ----------
  async loadModels() {
    try {
      const models = await fetchModels();
      this.setData({ models, error: "" });
      if (!models.length) {
        this.setData({
          error: "尚未配置任何大模型的 API Key。请在后端复制 .env.example 为 .env 并填入至少一个 Key 后重启。",
        });
      }
    } catch (e) {
      this.setData({
        error: "加载模型列表失败：" + (e.message || e) + "。请确认后端已启动，且 utils/config.js 的 BASE_URL 正确。",
      });
    }
  },

  // ---------- 输入 ----------
  onModelChange(e) {
    this.setData({ modelIndex: Number(e.detail.value) });
  },
  onPromptInput(e) {
    this.setData({ prompt: e.detail.value });
  },
  onStyleInput(e) {
    this.setData({ style: e.detail.value });
  },
  onMoodInput(e) {
    this.setData({ mood: e.detail.value });
  },
  onTempoInput(e) {
    this.setData({ tempo: e.detail.value });
  },
  onBarsInput(e) {
    this.setData({ bars: e.detail.value });
  },

  // ---------- 生成 ----------
  async onGenerate() {
    const { models, modelIndex, prompt, style, mood, tempo, bars } = this.data;
    if (!prompt.trim()) {
      this.setData({ error: "请先填写创作主题" });
      return;
    }
    const model = models[modelIndex];
    if (!model) {
      this.setData({ error: "请先选择模型" });
      return;
    }

    this.setData({ loading: true, error: "" });
    this.player.stop();
    this.setData({ composition: null });

    try {
      const composition = await generate({
        prompt: prompt.trim(),
        provider: model.id,
        style: style.trim() || null,
        mood: mood.trim() || null,
        tempo: tempo ? parseInt(tempo, 10) : null,
        duration_bars: bars ? parseInt(bars, 10) : 8,
      });
      this.setData({ composition });
      this.saveToHistory(composition);
      this.drawPianoRoll(composition);
    } catch (e) {
      this.setData({ error: e.message || String(e) });
    } finally {
      this.setData({ loading: false });
    }
  },

  saveToHistory(composition) {
    historyStore.add({
      id: String(Date.now()) + Math.random().toString(36).slice(2, 6),
      title: composition.title,
      key: composition.key,
      tempo: composition.tempo,
      time_signature: composition.time_signature,
      composer_note: composition.composer_note,
      notes: composition.notes,
      midi_url: composition.midi_url,
      createdAt: new Date().toISOString(),
      synced: false,
    });
    wx.showToast({ title: "已保存到历史", icon: "success" });
  },

  // ---------- 试听 ----------
  onPlay() {
    const composition = this.data.composition;
    if (!composition || !composition.notes.length) return;
    try {
      this.player.play(composition, {
        onStart: () => this.setData({ isPlaying: true }),
        onEnded: () => this.setData({ isPlaying: false }),
      });
    } catch (e) {
      this.setData({ error: e.message || String(e) });
    }
  },

  onStop() {
    this.player.stop();
    this.setData({ isPlaying: false });
  },

  // ---------- 导出 MIDI ----------
  async onShareMidi() {
    const composition = this.data.composition;
    if (!composition || !composition.midi_url) {
      this.setData({ error: "暂无可导出的 MIDI" });
      return;
    }
    wx.showLoading({ title: "正在下载…" });
    try {
      const filePath = await downloadMidi(composition.midi_url);
      wx.hideLoading();
      wx.shareFileMessage({
        filePath,
        fileName: (composition.title || "aimusic") + ".mid",
        fail: () => wx.showToast({ title: "分享已取消或失败", icon: "none" }),
      });
    } catch (e) {
      wx.hideLoading();
      this.setData({ error: "MIDI 下载失败：" + (e.message || e) });
    }
  },

  // ---------- 钢琴卷帘（可横向滚动） ----------
  drawPianoRoll(composition) {
    const notes = composition.notes;
    if (!notes || !notes.length) return;

    const sys = wx.getSystemInfoSync();
    const dpr = sys.pixelRatio || 2;
    const rpx = sys.windowWidth / 750;
    const H = Math.round(220 * rpx);
    const viewW = sys.windowWidth - 60; // 页面左右 padding 30rpx * 2

    // 固定每拍像素宽度，长曲目自动变宽，配合 scroll-view 横向滚动
    const pxPerBeat = 90;
    const endBeat = Math.max(...notes.map((n) => n.start + n.duration));
    const W = Math.max(viewW, Math.round(20 + endBeat * pxPerBeat));

    this.setData({ rollStyle: `width:${W}px;height:${H}px;` }, () => {
      wx.createSelectorQuery()
        .select("#pianoRoll")
        .fields({ node: true })
        .exec((res) => {
          const canvas = res && res[0] && res[0].node;
          if (!canvas) return;
          canvas.width = W * dpr;
          canvas.height = H * dpr;
          const ctx = canvas.getContext("2d");
          ctx.scale(dpr, dpr);
          ctx.clearRect(0, 0, W, H);

          const pitches = notes.map((n) => n.pitch);
          const minP = Math.min(...pitches) - 2;
          const maxP = Math.max(...pitches) + 2;
          const span = Math.max(1, maxP - minP);
          const rowH = (H - 20) / span;

          // 背景网格线（每拍一条）
          ctx.strokeStyle = "rgba(255,255,255,0.05)";
          ctx.lineWidth = 1;
          for (let b = 0; b <= Math.ceil(endBeat); b++) {
            const x = 10 + b * pxPerBeat;
            ctx.beginPath();
            ctx.moveTo(x, 10);
            ctx.lineTo(x, H - 10);
            ctx.stroke();
          }

          // 音符条
          const grad = ctx.createLinearGradient(0, 0, W, 0);
          grad.addColorStop(0, "#7c6cff");
          grad.addColorStop(1, "#56c8ff");
          ctx.fillStyle = grad;

          notes.forEach((n) => {
            const x = 10 + n.start * pxPerBeat;
            const w = Math.max(2, n.duration * pxPerBeat - 1);
            const y = H - 10 - (n.pitch - minP + 1) * rowH;
            const h = Math.max(3, rowH - 2);
            this.roundRect(ctx, x, y, w, h, 3);
          });
        });
    });
  },

  roundRect(ctx, x, y, w, h, r) {
    r = Math.min(r, w / 2, h / 2);
    ctx.beginPath();
    ctx.moveTo(x + r, y);
    ctx.arcTo(x + w, y, x + w, y + h, r);
    ctx.arcTo(x + w, y + h, x, y + h, r);
    ctx.arcTo(x, y + h, x, y, r);
    ctx.arcTo(x, y, x + w, y, r);
    ctx.closePath();
    ctx.fill();
  },
});
