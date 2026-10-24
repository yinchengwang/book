// app.js — 全局入口
const { ensureCloud, isConfigured } = require('./utils/cloud.js')
const { ensureOpenid } = require('./utils/auth.js')

App({
  globalData: {
    openid: '',
    isLoginReady: false
  },

  async onLaunch() {
    // 仅在云服务已配置时尝试登录。占位阶段直接 pass，不让 onLaunch 报错。
    if (isConfigured()) {
      try {
        const openid = await ensureOpenid()
        this.globalData.openid = openid
        this.globalData.isLoginReady = true
      } catch (err) {
        console.error('[login] failed:', err)
      }
    }
  },

  getCloud() { return getApp().cloud }
})
