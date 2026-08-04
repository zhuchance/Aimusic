// 后端 REST API 封装：wx.request / wx.downloadFile 转 Promise。
// 自动携带登录 token（Authorization: Bearer xxx）。
const { BASE_URL } = require("./config");

const TOKEN_KEY = "aimusic_token";
const OPENID_KEY = "aimusic_openid";

function getToken() {
  return wx.getStorageSync(TOKEN_KEY) || "";
}

function request(path, method = "GET", data = null, auth = false) {
  return new Promise((resolve, reject) => {
    const header = { "Content-Type": "application/json" };
    if (auth) {
      const token = getToken();
      if (!token) {
        reject(new Error("未登录，请先登录"));
        return;
      }
      header.Authorization = "Bearer " + token;
    }
    wx.request({
      url: BASE_URL + path,
      method,
      data,
      timeout: 120000,
      header,
      success(res) {
        if (res.statusCode >= 200 && res.statusCode < 300) {
          resolve(res.data);
        } else {
          reject(new Error((res.data && res.data.detail) || `请求失败 (${res.statusCode})`));
        }
      },
      fail(err) {
        reject(new Error(err.errMsg || "网络请求失败"));
      },
    });
  });
}

// 获取已配置 API Key 的可用模型列表
function fetchModels() {
  return request("/api/models");
}

// 提交生成请求
function generate(params) {
  return request("/api/generate", "POST", params);
}

// 微信登录：wx.login 拿 code -> 后端换 token
function wxLogin() {
  return new Promise((resolve, reject) => {
    wx.login({
      success: async (res) => {
        try {
          const data = await request("/api/wx/login", "POST", { code: res.code });
          wx.setStorageSync(TOKEN_KEY, data.token);
          wx.setStorageSync(OPENID_KEY, data.openid);
          resolve(data);
        } catch (e) {
          reject(e);
        }
      },
      fail: reject,
    });
  });
}

function isLoggedIn() {
  return !!getToken();
}

// 云历史
function fetchHistory() {
  return request("/api/history", "GET", null, true);
}

function pushHistory(item) {
  return request(
    "/api/history",
    "POST",
    {
      title: item.title,
      key: item.key,
      tempo: item.tempo,
      time_signature: item.time_signature,
      composer_note: item.composer_note,
      notes: item.notes,
      midi_url: item.midi_url,
    },
    true
  );
}

function deleteHistory(id) {
  return request(`/api/history/${id}`, "DELETE", null, true);
}

// 下载 MIDI 文件到本地临时路径
function downloadMidi(url) {
  return new Promise((resolve, reject) => {
    wx.downloadFile({
      url: BASE_URL + url,
      success(res) {
        if (res.statusCode === 200 && res.tempFilePath) {
          resolve(res.tempFilePath);
        } else {
          reject(new Error(`下载失败 (${res.statusCode})`));
        }
      },
      fail(err) {
        reject(new Error(err.errMsg || "下载失败"));
      },
    });
  });
}

module.exports = {
  fetchModels,
  generate,
  downloadMidi,
  wxLogin,
  isLoggedIn,
  fetchHistory,
  pushHistory,
  deleteHistory,
};
