/**
 * with-theme.js —— 页面主题包装器
 *
 * 原生小程序的 Behavior 只对 Component 生效，页面无法直接复用逻辑，
 * 因此用高阶函数包装 Page 配置，统一注入主题同步。
 *
 * 用法：
 *   const { withTheme } = require('../../core/with-theme.js')
 *   Page(withTheme({
 *     data: { ... },
 *     onLoad() { ... }
 *   }))
 *
 * 包装后页面自动获得：
 *   - data.themeClass / isDark / brand / brandSoft
 *   - onShow 时同步主题与导航栏颜色
 *   - auto 模式下监听系统主题变化
 *   - onUnload 时解除监听
 */

const theme = require('./theme.js')
const stores = require('./stores.js')

/** 立即把主题写入页面 data */
function applyTheme (page) {
  const settings = stores.settings.get()
  const data = theme.build(settings)
  page.setData({
    themeClass: data.themeClass,
    isDark: data.isDark,
    brand: data.brand,
    brandSoft: data.brandSoft
  })
  theme.syncNavBar(data)
  theme.syncTabBar(data)
  return data
}

/**
 * 包装页面配置
 * @param {object} options Page 配置对象
 */
function withTheme (options) {
  const config = Object.assign({}, options)

  config.data = Object.assign({
    themeClass: 'theme-light',
    isDark: false,
    brand: '#0284C7',
    brandSoft: 'rgba(2,132,199,0.12)'
  }, options.data)

  const userOnLoad = options.onLoad
  const userOnShow = options.onShow
  const userOnUnload = options.onUnload

  config.onLoad = function (query) {
    this.__themeApplied = true
    applyTheme(this)
    this.__bindSystemTheme()
    if (userOnLoad) userOnLoad.call(this, query)
  }

  config.onShow = function () {
    // 设置页可能改过主题，每次显示都重新同步
    applyTheme(this)
    if (userOnShow) userOnShow.call(this)
  }

  config.onUnload = function () {
    if (this.__themeOff) {
      this.__themeOff = null
    }
    if (userOnUnload) userOnUnload.call(this)
  }

  /** 仅在 auto 模式下响应系统主题切换 */
  config.__bindSystemTheme = function () {
    if (this.__themeBound) return
    this.__themeBound = true
    const self = this
    theme.onSystemThemeChange(function () {
      const settings = stores.settings.get()
      if (settings.theme !== 'auto') return
      applyTheme(self)
    })
  }

  /** 供页面手动刷新主题（如设置页切换后立即生效） */
  config.refreshTheme = function () {
    return applyTheme(this)
  }

  return config
}

module.exports = {
  withTheme: withTheme,
  applyTheme: applyTheme
}
