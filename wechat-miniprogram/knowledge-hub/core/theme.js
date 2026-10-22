/**
 * theme.js —— 主题解析
 *
 * 原生 WXSS 不支持 `[data-theme]` 属性选择器，因此改用 class 承载变量：
 *   .theme-light { --bg-primary: #F7F8FA; ... }
 *   .theme-dark  { --bg-primary: #0B0F17; ... }
 * 每页根 view 挂 `class="page {{themeClass}}"`。
 *
 * 主色通过根 view 行内 `style="--brand: {{brand}}; --brand-soft: {{brandSoft}}"` 覆盖。
 */

const constants = require('./constants.js')

/** 读取系统主题 */
function systemTheme () {
  try {
    const info = wx.getSystemInfoSync()
    return info.theme === 'dark' ? 'dark' : 'light'
  } catch (e) {
    return 'light'
  }
}

/**
 * 把设置里的 mode 解析成实际主题
 * @param {'light'|'dark'|'auto'} mode
 */
function resolve (mode) {
  if (mode === 'light' || mode === 'dark') return mode
  return systemTheme()
}

/** HEX → rgba 字符串 */
function hexToRgba (hex, alpha) {
  const h = String(hex).replace('#', '')
  const full = h.length === 3 ? h.split('').map(function (c) { return c + c }).join('') : h
  const r = parseInt(full.slice(0, 2), 16)
  const g = parseInt(full.slice(2, 4), 16)
  const b = parseInt(full.slice(4, 6), 16)
  return 'rgba(' + r + ',' + g + ',' + b + ',' + alpha + ')'
}

/** 取主色定义 */
function primaryOf (key) {
  for (let i = 0; i < constants.PRIMARY_COLORS.length; i++) {
    if (constants.PRIMARY_COLORS[i].key === key) return constants.PRIMARY_COLORS[i]
  }
  return constants.PRIMARY_COLORS[0]
}

/**
 * 根据设置计算主题数据，供页面 setData
 * @param {object} settings stores.settings.get() 的结果
 * @returns {{themeClass:string, isDark:boolean, brand:string, brandSoft:string, navFront:string, navBg:string}}
 */
function build (settings) {
  const mode = (settings && settings.theme) || 'auto'
  const theme = resolve(mode)
  const isDark = theme === 'dark'
  const primary = primaryOf((settings && settings.primaryColor) || 'sky')
  const brand = isDark ? primary.dark : primary.light

  return {
    themeClass: isDark ? 'theme-dark' : 'theme-light',
    isDark: isDark,
    brand: brand,
    brandSoft: hexToRgba(brand, isDark ? 0.18 : 0.12),
    navFront: isDark ? '#ffffff' : '#000000',
    navBg: isDark ? '#0B0F17' : '#F7F8FA'
  }
}

/** 订阅系统主题变化（仅 auto 模式需要） */
function onSystemThemeChange (callback) {
  try {
    wx.onThemeChange(function (res) {
      callback(res.theme === 'dark' ? 'dark' : 'light')
    })
    return true
  } catch (e) {
    return false
  }
}

/** 同步导航栏颜色（让自定义色与页面底色一致） */
function syncNavBar (themeData) {
  try {
    wx.setNavigationBarColor({
      frontColor: themeData.navFront,
      backgroundColor: themeData.navBg,
      animation: { duration: 0, timingFunc: 'linear' }
    })
  } catch (e) {
    // 部分基础库版本不支持，忽略
  }
}

/**
 * 同步原生 tabBar 配色
 *
 * 仅在 tab 页生效；非 tab 页调用会失败，已被 try/catch 吞掉。
 */
function syncTabBar (themeData) {
  try {
    wx.setTabBarStyle({
      color: themeData.isDark ? '#64748B' : '#64748B',
      selectedColor: themeData.brand,
      backgroundColor: themeData.isDark ? '#151A23' : '#FFFFFF',
      borderStyle: themeData.isDark ? 'black' : 'white'
    })
  } catch (e) {
    // 非 tab 页调用时忽略
  }
}

module.exports = {
  systemTheme: systemTheme,
  resolve: resolve,
  build: build,
  hexToRgba: hexToRgba,
  primaryOf: primaryOf,
  onSystemThemeChange: onSystemThemeChange,
  syncNavBar: syncNavBar,
  syncTabBar: syncTabBar
}
