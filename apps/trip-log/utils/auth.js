// utils/auth.js — 静默登录：拿到 openid 即可，不弹任何授权框

const { getCloud } = require('./cloud.js')

const OPENID_CACHE_KEY = 'TRIPLOG_OPENID_CACHE'

function readOpenidCache() {
  try { return wx.getStorageSync(OPENID_CACHE_KEY) || '' } catch (e) { return '' }
}
function writeOpenidCache(openid) {
  try { wx.setStorageSync(OPENID_CACHE_KEY, openid) } catch (e) {}
}

async function codeToOpenid(code) {
  const cloud = await getCloud()
  try {
    if (cloud.auth && typeof cloud.auth.signIn === 'function') {
      const res = await cloud.auth.signIn({ provider: 'wechat_mp', code })
      if (res && res.openid) return res.openid
      if (res && res.user && res.user.openid) return res.user.openid
    }
  } catch (err) {
    console.warn('[auth] signIn fallback needed:', err && err.message)
  }
  try {
    const db = cloud.database()
    const probe = await db.collection('__login_probe').add({ data: { t: Date.now() } })
    return probe && probe._openid ? probe._openid : ''
  } catch (err) {
    console.error('[auth] fallback probe failed:', err)
    return ''
  }
}

async function ensureOpenid() {
  let cached = readOpenidCache()
  if (cached) return cached
  const code = await new Promise((resolve, reject) => {
    wx.login({
      success: (r) => r && r.code ? resolve(r.code) : reject(new Error('wx.login no code')),
      fail: reject
    })
  })
  const openid = await codeToOpenid(code)
  if (openid) writeOpenidCache(openid)
  return openid
}

module.exports = { ensureOpenid, codeToOpenid }
