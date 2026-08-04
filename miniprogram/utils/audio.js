// 旋律播放器：基于 wx.createWebAudioContext()（基础库 >= 2.19.0）。
// 小程序没有浏览器 Web Audio API，但 WeChat 提供了近似实现，
// 支持 OscillatorNode / GainNode / AudioParam 调度，可离线合成整段旋律。
"use strict";

function midiToFreq(pitch) {
  return 440 * Math.pow(2, (pitch - 69) / 12);
}

function createPlayer() {
  let ctx = null;
  let scheduled = [];
  let timer = null;
  let playing = false;

  function ensureCtx() {
    if (!ctx) {
      if (!wx.createWebAudioContext) {
        throw new Error("当前微信版本过低，不支持 WebAudioContext，请升级微信后重试");
      }
      ctx = wx.createWebAudioContext();
    }
    return ctx;
  }

  function play(composition, callbacks) {
    stop();
    const audioCtx = ensureCtx();
    if (audioCtx.state === "suspended" && audioCtx.resume) {
      audioCtx.resume();
    }

    const spb = 60 / composition.tempo; // 每拍秒数
    const now = audioCtx.currentTime + 0.08;
    let maxEnd = 0;

    composition.notes.forEach((n) => {
      const start = now + n.start * spb;
      const dur = Math.max(0.06, n.duration * spb * 0.92);
      const end = start + dur;
      if (end > maxEnd) maxEnd = end;

      try {
        const osc = audioCtx.createOscillator();
        const gain = audioCtx.createGain();
        osc.type = "triangle";
        osc.frequency.value = midiToFreq(n.pitch);

        // 简易 ADSR 包络，避免爆音
        const peak = (n.velocity / 127) * 0.28;
        gain.gain.setValueAtTime(0, start);
        gain.gain.linearRampToValueAtTime(peak, start + 0.02);
        if (gain.gain.setTargetAtTime) {
          gain.gain.setTargetAtTime(peak * 0.6, start + 0.05, 0.08);
          gain.gain.setTargetAtTime(0, end - 0.03, 0.05);
        } else {
          gain.gain.linearRampToValueAtTime(0, end);
        }

        osc.connect(gain);
        gain.connect(audioCtx.destination);
        osc.start(start);
        osc.stop(end + 0.1);
        scheduled.push(osc);
      } catch (e) {
        // 单个节点创建失败则跳过，不影响整体
      }
    });

    playing = true;
    if (callbacks && callbacks.onStart) callbacks.onStart();

    const totalMs = (maxEnd - audioCtx.currentTime + 0.2) * 1000;
    timer = setTimeout(() => {
      playing = false;
      if (callbacks && callbacks.onEnded) callbacks.onEnded();
    }, totalMs);
  }

  function stop() {
    if (timer) {
      clearTimeout(timer);
      timer = null;
    }
    if (ctx) {
      scheduled.forEach((n) => {
        try {
          n.stop();
        } catch (e) {
          /* ignore */
        }
      });
    }
    scheduled = [];
    playing = false;
  }

  return {
    play,
    stop,
    get isPlaying() {
      return playing;
    },
  };
}

module.exports = createPlayer;
