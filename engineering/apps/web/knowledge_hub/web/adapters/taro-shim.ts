/**
 * Taro.* API 适配桩 - H5 端实现
 *
 * 源码中可能调用以下 Taro API：
 * - Taro.navigateTo / redirectTo / switchTab / navigateBack / reLaunch
 * - Taro.showToast / showModal / showLoading / hideLoading
 * - Taro.setStorageSync / getStorageSync / removeStorageSync
 * - Taro.getSystemInfoSync
 * - Taro.getCurrentInstance / useRouter / useDidShow / useDidHide
 * - Taro.createSelectorQuery
 *
 * 在 H5 端用浏览器 API 模拟
 *
 * 同时提供 named exports，让源码可以：
 *   import { navigateTo, useDidShow, showToast } from '@tarojs/taro'
 */

let currentPath = window.location.pathname
let currentQuery: Record<string, string> = {}

// 监听 URL 变化（来自 react-router）
window.addEventListener('popstate', () => {
  currentPath = window.location.pathname
  currentQuery = Object.fromEntries(new URLSearchParams(window.location.search))
})

// ==================== 路由 ====================
export function navigateTo({ url }: { url: string }): Promise<void> {
  const [path, queryStr] = url.split('?')
  if (queryStr) {
    history.pushState(null, '', `${path}?${queryStr}`)
  } else {
    history.pushState(null, '', path)
  }
  currentPath = path
  currentQuery = queryStr ? Object.fromEntries(new URLSearchParams(queryStr)) : {}
  window.dispatchEvent(new PopStateEvent('popstate'))
  return Promise.resolve()
}

export function redirectTo({ url }: { url: string }): Promise<void> {
  const [path, queryStr] = url.split('?')
  if (queryStr) {
    history.replaceState(null, '', `${path}?${queryStr}`)
  } else {
    history.replaceState(null, '', path)
  }
  currentPath = path
  currentQuery = queryStr ? Object.fromEntries(new URLSearchParams(queryStr)) : {}
  window.dispatchEvent(new PopStateEvent('popstate'))
  return Promise.resolve()
}

export function switchTab({ url }: { url: string }): Promise<void> {
  return navigateTo({ url })
}

export function navigateBack({ delta = 1 }: { delta?: number } = {}): Promise<void> {
  history.go(-delta)
  return Promise.resolve()
}

export function reLaunch({ url }: { url: string }): Promise<void> {
  return navigateTo({ url })
}

// ==================== Toast / Modal ====================
let toastTimer: number | null = null
export function showToast({
  title,
  icon = 'none',
  duration = 1500,
  image,
  mask = false
}: {
  title: string
  icon?: 'success' | 'error' | 'loading' | 'none'
  duration?: number
  image?: string
  mask?: boolean
}): Promise<void> {
  hideToast()
  const div = document.createElement('div')
  const iconMap = { success: '✅', error: '❌', loading: '⏳', none: '' }
  const iconText = image ? '' : iconMap[icon]
  div.innerHTML = `
    <div style="
      display:flex; flex-direction:column; align-items:center; gap:12px;
      padding:20px 24px;
      background:rgba(0,0,0,0.85);
      color:#fff; border-radius:12px;
      box-shadow:0 8px 32px rgba(0,0,0,0.4);
    ">
      ${iconText ? `<div style="font-size:32px">${iconText}</div>` : ''}
      ${image ? `<img src="${image}" style="width:48px;height:48px" />` : ''}
      <div style="font-size:14px; max-width:240px; text-align:center">${title}</div>
    </div>
  `
  div.setAttribute('data-taro-toast', '1')
  div.style.cssText = `
    position:fixed; top:50%; left:50%; transform:translate(-50%,-50%);
    z-index:10000;
    ${mask ? 'background:rgba(0,0,0,0.4); inset:0; display:flex; align-items:center; justify-content:center; transform:none; top:0; left:0;' : ''}
  `
  if (mask) {
    div.style.display = 'flex'
    div.style.alignItems = 'center'
    div.style.justifyContent = 'center'
    div.style.transform = 'none'
    div.style.top = '0'
    div.style.left = '0'
  }
  document.body.appendChild(div)
  toastTimer = window.setTimeout(() => div.remove(), duration)
  return Promise.resolve()
}

export function hideToast(): Promise<void> {
  if (toastTimer) {
    clearTimeout(toastTimer)
    toastTimer = null
  }
  document.querySelectorAll('[data-taro-toast]').forEach(el => el.remove())
  return Promise.resolve()
}

export function showModal({
  title = '提示',
  content = '',
  showCancel = true,
  cancelText = '取消',
  confirmText = '确定',
  cancelColor = '#000000',
  confirmColor = '#576B95'
}: {
  title?: string
  content?: string
  showCancel?: boolean
  cancelText?: string
  confirmText?: string
  cancelColor?: string
  confirmColor?: string
} = {}): Promise<{ confirm: boolean; cancel: boolean }> {
  return new Promise((resolve) => {
    const overlay = document.createElement('div')
    overlay.style.cssText = `
      position:fixed; inset:0; background:rgba(0,0,0,0.6); z-index:10001;
      display:flex; align-items:center; justify-content:center;
    `
    overlay.innerHTML = `
      <div style="
        background:#1a1a2e; border-radius:12px; padding:24px; min-width:280px; max-width:360px;
        box-shadow:0 16px 48px rgba(0,0,0,0.5);
        border:1px solid rgba(255,255,255,0.1);
      ">
        <div style="font-size:16px; font-weight:600; color:#fff; margin-bottom:12px; text-align:center">${title}</div>
        <div style="font-size:14px; color:#cbd5e1; line-height:1.6; margin-bottom:20px; text-align:center">${content}</div>
        <div style="display:flex; gap:12px">
          ${showCancel ? `<button data-action="cancel" style="flex:1; padding:10px; background:rgba(255,255,255,0.05); border:1px solid rgba(255,255,255,0.1); color:#fff; border-radius:8px; cursor:pointer">${cancelText}</button>` : ''}
          <button data-action="confirm" style="flex:1; padding:10px; background:linear-gradient(135deg,#6366f1,#8b5cf6); border:none; color:#fff; border-radius:8px; cursor:pointer; font-weight:500">${confirmText}</button>
        </div>
      </div>
    `
    document.body.appendChild(overlay)
    overlay.addEventListener('click', (e) => {
      const target = e.target as HTMLElement
      if (target.dataset.action === 'confirm') {
        overlay.remove()
        resolve({ confirm: true, cancel: false })
      } else if (target.dataset.action === 'cancel') {
        overlay.remove()
        resolve({ confirm: false, cancel: true })
      }
    })
  })
}

export function showLoading({ title = '加载中', mask = true }: { title?: string; mask?: boolean } = {}): Promise<void> {
  return showToast({ title, icon: 'loading', mask, duration: 999999 })
}

export function hideLoading(): Promise<void> {
  return hideToast()
}

// ==================== 存储 ====================
export function setStorageSync(key: string, value: any): void {
  try {
    localStorage.setItem(key, JSON.stringify(value))
  } catch (e) {
    console.error('Taro.setStorageSync error:', e)
  }
}

export function getStorageSync(key: string): any {
  try {
    const value = localStorage.getItem(key)
    return value ? JSON.parse(value) : ''
  } catch (e) {
    console.error('Taro.getStorageSync error:', e)
    return ''
  }
}

export function removeStorageSync(key: string): void {
  localStorage.removeItem(key)
}

export function getStorageInfoSync(): { keys: string[]; currentSize: number; limitSize: number } {
  const keys: string[] = []
  let currentSize = 0
  for (let i = 0; i < localStorage.length; i++) {
    const k = localStorage.key(i)
    if (k) {
      keys.push(k)
      currentSize += (localStorage.getItem(k) || '').length
    }
  }
  return { keys, currentSize, limitSize: 10240 }
}

export function clearStorageSync(): void {
  localStorage.clear()
}

// 异步版本
export function setStorage(key: string, value: any): Promise<void> {
  setStorageSync(key, value)
  return Promise.resolve()
}

export function getStorage(key: string): Promise<{ data: any }> {
  return Promise.resolve({ data: getStorageSync(key) })
}

export function removeStorage(key: string): Promise<void> {
  removeStorageSync(key)
  return Promise.resolve()
}

// ==================== 系统信息 ====================
export function getSystemInfoSync() {
  return {
    platform: 'h5',
    system: navigator.userAgent,
    model: 'Browser',
    version: navigator.appVersion,
    screenWidth: window.innerWidth,
    screenHeight: window.innerHeight,
    windowWidth: window.innerWidth,
    windowHeight: window.innerHeight,
    pixelRatio: window.devicePixelRatio,
    statusBarHeight: 0,
    language: navigator.language,
    brand: 'web',
    fontSizeSetting: 16
  }
}

// ==================== 实例 ====================
export function getCurrentInstance() {
  return {
    router: {
      path: currentPath,
      params: currentQuery
    },
    page: {
      route: currentPath
    }
  }
}

// ==================== 生命周期 Hooks ====================
// 在 H5 端这些 hooks 简化为 noop 或简单实现
export function useRouter() {
  return {
    path: currentPath,
    params: currentQuery
  }
}

export function useDidShow(callback: () => void) {
  if (typeof callback === 'function') {
    // 组件挂载时调用
    setTimeout(callback, 0)
  }
}

export function useDidHide(callback: () => void) {
  if (typeof callback === 'function') {
    // H5 端没有 hide 概念，注册到 pagehide
    window.addEventListener('beforeunload', callback)
  }
}

export function useReady(callback: () => void) {
  if (typeof callback === 'function') {
    setTimeout(callback, 0)
  }
}

export function usePullDownRefresh(callback: () => void) {
  // H5 端不支持下拉刷新
}

export function useReachBottom(callback: () => void) {
  // H5 端用 scroll 事件模拟
  if (typeof callback === 'function') {
    window.addEventListener('scroll', () => {
      if (window.innerHeight + window.scrollY >= document.body.offsetHeight - 50) {
        callback()
      }
    })
  }
}

export function useShareAppMessage(callback: () => void) {
  // H5 端不支持
}

export function useTabItemTap(callback: () => void) {
  // H5 端不支持
}

export function useTitleClick(callback: () => void) {
  // H5 端不支持
}

export function useOptionMenuClick(callback: () => void) {
  // H5 端不支持
}

export function useResize(callback: () => void) {
  if (typeof callback === 'function') {
    window.addEventListener('resize', callback)
  }
}

export function useUnhandledRejection(callback: (error: any) => void) {
  if (typeof callback === 'function') {
    window.addEventListener('unhandledrejection', (e) => callback(e.reason))
  }
}

export function useError(callback: (error: any) => void) {
  if (typeof callback === 'function') {
    window.addEventListener('error', (e) => callback(e.error || e.message))
  }
}

export function useLaunch(callback: () => void) {
  // H5 端无 launch
}

export function usePageScroll(callback: () => void) {
  if (typeof callback === 'function') {
    window.addEventListener('scroll', callback)
  }
}

// ==================== SelectorQuery ====================
export function createSelectorQuery() {
  return {
    select(selector: string) {
      return {
        boundingClientRect(cb: (rect: any) => void) {
          const el = document.querySelector(selector)
          if (el) {
            const rect = el.getBoundingClientRect()
            cb && cb(rect)
          } else {
            cb && cb(null)
          }
          return this
        },
        fields() { return this },
        exec() { return Promise.resolve() }
      }
    },
    selectAll() { return this },
    in() { return this },
    exec() { return Promise.resolve() }
  }
}

// ==================== 事件总线 ====================
type EventCallback = (data?: any) => void
const eventCenter: Record<string, EventCallback[]> = {}

export function on(name: string, callback: EventCallback) {
  if (!eventCenter[name]) eventCenter[name] = []
  eventCenter[name].push(callback)
}

export function off(name: string, callback?: EventCallback) {
  if (!eventCenter[name]) return
  if (callback) {
    eventCenter[name] = eventCenter[name].filter(cb => cb !== callback)
  } else {
    delete eventCenter[name]
  }
}

export function trigger(name: string, data?: any) {
  if (!eventCenter[name]) return
  eventCenter[name].forEach(cb => cb(data))
}

// ==================== 暴露 ====================
declare global {
  interface Window {
    wx?: any
    __GLOBAL__TARO_SHIM__?: boolean
  }
}

if (typeof window !== 'undefined') {
  // 模拟 wx 对象（部分源码可能直接用 wx.*）
  window.wx = window.wx || {
    switchTab: ({ url }: { url: string }) => navigateTo({ url }),
    navigateTo,
    redirectTo,
    navigateBack,
    reLaunch,
    showToast,
    showModal,
    showLoading,
    hideLoading,
    setStorageSync,
    getStorageSync,
    removeStorageSync,
    clearStorageSync,
    getSystemInfoSync
  }
  window.__GLOBAL__TARO_SHIM__ = true
}

export const Taro = {
  navigateTo,
  redirectTo,
  switchTab,
  navigateBack,
  reLaunch,
  showToast,
  hideToast,
  showModal,
  showLoading,
  hideLoading,
  setStorageSync,
  getStorageSync,
  removeStorageSync,
  clearStorageSync,
  getStorageInfoSync,
  setStorage,
  getStorage,
  removeStorage,
  getSystemInfoSync,
  getCurrentInstance,
  createSelectorQuery,
  on,
  off,
  trigger,
  useRouter,
  useDidShow,
  useDidHide,
  useReady,
  usePullDownRefresh,
  useReachBottom,
  useShareAppMessage,
  useResize,
  useUnhandledRejection,
  useError,
  useLaunch,
  usePageScroll,
  getEnv() {
    return 'WEB' as 'WEB' | 'WEAPP' | 'ALIPAY' | 'TT' | 'QQ' | 'JD' | 'SWAN' | 'RN'
  }
}

// 默认导出（兼容 import Taro from '@tarojs/taro'）
export default Taro
