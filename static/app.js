// Aimusic 前端逻辑：加载模型、生成、播放预览、可视化
"use strict";

// ---------- DOM ----------
const $ = (id) => document.getElementById(id);
const modelSelect = $("modelSelect");
const generateBtn = $("generateBtn");
const promptEl = $("prompt");
const styleEl = $("style");
const moodEl = $("mood");
const tempoEl = $("tempo");
const barsEl = $("bars");
const errorEl = $("error");
const resultEl = $("result");
const loadingEl = $("loading");
const playBtn = $("playBtn");
const stopBtn = $("stopBtn");
const downloadBtn = $("downloadBtn");
const canvas = $("pianoRoll");

// 当前生成结果
let composition = null;
// Web Audio 播放状态
let audioCtx = null;
let scheduledNodes = [];
let playTimer = null;

// ---------- 工具 ----------
function showError(msg) {
  errorEl.textContent = msg;
  errorEl.hidden = false;
}
function clearError() {
  errorEl.hidden = true;
  errorEl.textContent = "";
}
function midiToFreq(pitch) {
  return 440 * Math.pow(2, (pitch - 69) / 12);
}

// ---------- 加载模型列表 ----------
async function loadModels() {
  try {
    const res = await fetch("/api/models");
    const models = await res.json();
    modelSelect.innerHTML = "";
    if (!models.length) {
      modelSelect.innerHTML =
        '<option value="">（未配置任何模型，请先在 .env 填入 API Key）</option>';
      generateBtn.disabled = true;
      showError("尚未配置任何大模型的 API Key。请复制 .env.example 为 .env 并填入至少一个 Key 后重启。");
      return;
    }
    models.forEach((m) => {
      const opt = document.createElement("option");
      opt.value = m.id;
      opt.textContent = m.display_name;
      modelSelect.appendChild(opt);
    });
  } catch (e) {
    showError("加载模型列表失败：" + e);
  }
}

// ---------- 生成 ----------
async function generate() {
  clearError();
  const prompt = promptEl.value.trim();
  if (!prompt) {
    showError("请先填写创作主题");
    return;
  }
  if (!modelSelect.value) {
    showError("请先选择模型");
    return;
  }

  const body = {
    prompt,
    provider: modelSelect.value,
    style: styleEl.value.trim() || null,
    mood: moodEl.value.trim() || null,
    tempo: tempoEl.value ? parseInt(tempoEl.value, 10) : null,
    duration_bars: barsEl.value ? parseInt(barsEl.value, 10) : 8,
  };

  setLoading(true);
  stopPlayback();
  resultEl.hidden = true;

  try {
    const res = await fetch("/api/generate", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(body),
    });
    const data = await res.json();
    if (!res.ok) {
      throw new Error(data.detail || `请求失败 (${res.status})`);
    }
    composition = data;
    renderResult();
  } catch (e) {
    showError(e.message || String(e));
  } finally {
    setLoading(false);
  }
}

function setLoading(on) {
  loadingEl.hidden = !on;
  generateBtn.disabled = on;
  generateBtn.textContent = on ? "作曲中…" : "✨ 开始创作";
}

// ---------- 渲染结果 ----------
function renderResult() {
  $("songTitle").textContent = composition.title || "Untitled";
  $("songMeta").textContent =
    `${composition.key} · ${composition.tempo} BPM · ${composition.time_signature}`;
  $("songNote").textContent = composition.composer_note || "";
  downloadBtn.href = composition.midi_url || "#";
  $("noteCount").textContent = `共 ${composition.notes.length} 个音符`;
  drawPianoRoll();
  resultEl.hidden = false;
  playBtn.hidden = false;
}

// ---------- 播放（Web Audio） ----------
function play() {
  if (!composition || !composition.notes.length) return;
  stopPlayback();
  audioCtx = audioCtx || new (window.AudioContext || window.webkitAudioContext)();
  if (audioCtx.state === "suspended") audioCtx.resume();

  const spb = 60 / composition.tempo; // 每拍秒数
  const now = audioCtx.currentTime + 0.08;
  let maxEnd = 0;

  composition.notes.forEach((n) => {
    const start = now + n.start * spb;
    const dur = Math.max(0.06, n.duration * spb * 0.92);
    const end = start + dur;
    if (end > maxEnd) maxEnd = end;

    const osc = audioCtx.createOscillator();
    const gain = audioCtx.createGain();
    osc.type = "triangle";
    osc.frequency.value = midiToFreq(n.pitch);

    // 简易 ADSR 包络，避免爆音
    const peak = (n.velocity / 127) * 0.28;
    gain.gain.setValueAtTime(0, start);
    gain.gain.linearRampToValueAtTime(peak, start + 0.02);
    gain.gain.setTargetAtTime(peak * 0.6, start + 0.05, 0.08);
    gain.gain.setTargetAtTime(0, end - 0.03, 0.05);

    osc.connect(gain).connect(audioCtx.destination);
    osc.start(start);
    osc.stop(end + 0.1);
    scheduledNodes.push(osc);
  });

  playBtn.hidden = true;
  stopBtn.hidden = false;
  const totalMs = (maxEnd - audioCtx.currentTime + 0.2) * 1000;
  playTimer = setTimeout(() => {
    playBtn.hidden = false;
    stopBtn.hidden = true;
  }, totalMs);
}

function stopPlayback() {
  if (playTimer) {
    clearTimeout(playTimer);
    playTimer = null;
  }
  scheduledNodes.forEach((n) => {
    try { n.stop(); } catch (_) {}
  });
  scheduledNodes = [];
  playBtn.hidden = false;
  stopBtn.hidden = true;
}

// ---------- 钢琴卷帘可视化 ----------
function drawPianoRoll() {
  const ctx = canvas.getContext("2d");
  const W = canvas.width, H = canvas.height;
  ctx.clearRect(0, 0, W, H);

  const notes = composition.notes;
  if (!notes.length) return;

  const pitches = notes.map((n) => n.pitch);
  const minP = Math.min(...pitches) - 2;
  const maxP = Math.max(...pitches) + 2;
  const span = Math.max(1, maxP - minP);

  const endBeat = Math.max(...notes.map((n) => n.start + n.duration));
  const pxPerBeat = (W - 20) / Math.max(endBeat, 1);
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
    roundRect(ctx, x, y, w, h, 3);
  });
}

function roundRect(ctx, x, y, w, h, r) {
  r = Math.min(r, w / 2, h / 2);
  ctx.beginPath();
  ctx.moveTo(x + r, y);
  ctx.arcTo(x + w, y, x + w, y + h, r);
  ctx.arcTo(x + w, y + h, x, y + h, r);
  ctx.arcTo(x, y + h, x, y, r);
  ctx.arcTo(x, y, x + w, y, r);
  ctx.closePath();
  ctx.fill();
}

// ---------- 事件绑定 ----------
generateBtn.addEventListener("click", generate);
playBtn.addEventListener("click", play);
stopBtn.addEventListener("click", stopPlayback);
promptEl.addEventListener("keydown", (e) => {
  if (e.key === "Enter" && (e.metaKey || e.ctrlKey)) generate();
});

loadModels();
