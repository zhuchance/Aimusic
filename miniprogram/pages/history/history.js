// 历史记录页：本地列表 + 播放 / 导出 / 删除 + 微信登录云同步。
"use strict";

const {
  wxLogin,
  isLoggedIn,
  fetchHistory,
  pushHistory,
  deleteHistory,
  downloadMidi,
} = require("../../utils/api");
const historyStore = require("../../utils/history");
const createPlayer = require("../../utils/audio");

Page({
  data: {
    items: [],
    loggedIn: false,
    syncing: false,
    playingId: null,
    error: "",
  },

  onLoad() {
    this.player = createPlayer();
  },

  onShow() {
    this.refresh();
  },

  onUnload() {
    if (this.player) this.player.stop();
  },

  refresh() {
    const items = historyStore
      .load()
      .map((it) => ({ ...it, displayTime: this.formatTime(it.createdAt) }));
    this.setData({ items, loggedIn: isLoggedIn() });
  },

  formatTime(iso) {
    if (!iso) return "";
    const d = new Date(iso);
    const p = (n) => String(n).padStart(2, "0");
    return `${d.getFullYear()}-${p(d.getMonth() + 1)}-${p(d.getDate())} ${p(d.getHours())}:${p(d.getMinutes())}`;
  },

  // ---------- 登录 / 云同步 ----------
  async onSync() {
    this.setData({ syncing: true, error: "" });
    try {
      if (!isLoggedIn()) {
        await wxLogin();
      }
      // 1. 上传本地未同步的记录
      const local = historyStore.load();
      for (const it of local) {
        if (!it.synced) await pushHistory(it);
      }
      // 2. 拉取云端列表，整体替换本地
      const remote = await fetchHistory();
      historyStore.save(
        remote.map((r) => ({
          id: "srv_" + r.id,
          title: r.title,
          key: r.key,
          tempo: r.tempo,
          time_signature: r.time_signature,
          composer_note: r.composer_note,
          notes: r.notes,
          midi_url: r.midi_url,
          createdAt: r.created_at,
          synced: true,
        }))
      );
      this.setData({ loggedIn: true });
      wx.showToast({ title: "同步完成", icon: "success" });
    } catch (e) {
      this.setData({ error: "同步失败：" + (e.message || e) });
    } finally {
      this.setData({ syncing: false });
      this.refresh();
    }
  },

  // ---------- 播放 ----------
  onPlay(e) {
    const item = this.findItem(e);
    if (!item || !item.notes || !item.notes.length) return;
    this.player.stop();
    try {
      this.player.play(
        { tempo: item.tempo, notes: item.notes },
        {
          onStart: () => this.setData({ playingId: item.id }),
          onEnded: () => this.setData({ playingId: null }),
        }
      );
    } catch (err) {
      this.setData({ error: err.message || String(err) });
    }
  },

  onStop() {
    this.player.stop();
    this.setData({ playingId: null });
  },

  // ---------- 导出 / 删除 ----------
  async onExport(e) {
    const item = this.findItem(e);
    if (!item || !item.midi_url) return;
    wx.showLoading({ title: "正在下载…" });
    try {
      const filePath = await downloadMidi(item.midi_url);
      wx.hideLoading();
      wx.shareFileMessage({
        filePath,
        fileName: (item.title || "aimusic") + ".mid",
        fail: () => wx.showToast({ title: "分享已取消或失败", icon: "none" }),
      });
    } catch (err) {
      wx.hideLoading();
      this.setData({ error: "MIDI 下载失败：" + (err.message || err) });
    }
  },

  async onDelete(e) {
    const item = this.findItem(e);
    if (!item) return;
    const res = await wx.showModal({ title: "删除记录", content: "确定删除这条历史记录？" });
    if (!res.confirm) return;

    this.setData({ items: historyStore.remove(item.id) });
    if (item.synced && isLoggedIn()) {
      try {
        await deleteHistory(parseInt(String(item.id).replace("srv_", ""), 10));
      } catch (err) {
        this.setData({ error: "云端删除失败：" + (err.message || err) });
      }
    }
  },

  findItem(e) {
    const id = e.currentTarget.dataset.id;
    return this.data.items.find((it) => it.id === id);
  },
});
