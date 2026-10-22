/**
 * vibrate.js —— 带节流的振动反馈
 */

let lastAt = 0

/**
 * 触发短振动（默认 80ms 节流，避免连续触发）
 * @param {string} type light | medium | heavy
 */
function trigger (type) {
  const now = Date.now()
  if (now - lastAt < 80) return
  lastAt = now
  try {
    wx.vibrateShort({ type: type || 'light' })
  } catch (e) {
    // 真机调试器不支持时静默忽略
  }
}

module.exports = { trigger }