// 本地历史记录存储（wx storage）。
// 每条记录结构：
//   {
//     id, title, key, tempo, time_signature, composer_note, notes, midi_url,
//     createdAt(ISO), synced(bool, 是否已同步到云端)
//   }
"use strict";

const KEY = "aimusic_history";

function load() {
  return wx.getStorageSync(KEY) || [];
}

function save(items) {
  wx.setStorageSync(KEY, items);
}

function add(item) {
  const items = load();
  items.unshift(item);
  save(items);
  return items;
}

function remove(id) {
  const items = load().filter((it) => it.id !== id);
  save(items);
  return items;
}

module.exports = { load, save, add, remove };
