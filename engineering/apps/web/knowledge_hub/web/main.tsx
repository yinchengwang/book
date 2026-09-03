/**
 * 应用入口
 *
 * - 全局错误捕获：window error / unhandledrejection
 * - 性能监控：performance API
 * - 启动 splash 移除
 * - localStorage 损坏自动清理
 */
import React from 'react'
import ReactDOM from 'react-dom/client'
import App from './App'
import './styles/global.scss'
import './styles/light-fixes.scss'
import './styles/pc-adaptive.scss'

// === 应用元信息 ===
const APP_VERSION = '1.0.0'
const APP_NAME = 'Reading Radar'

// === 启动日志 ===
console.log(`[${APP_NAME}] v${APP_VERSION} 启动`)
console.log(`[${APP_NAME}] User Agent:`, navigator.userAgent.slice(0, 80))
console.log(`[${APP_NAME}] 视口: ${window.innerWidth}x${window.innerHeight}`)

// === 全局错误捕获 ===
window.addEventListener('error', (e) => {
  const info = {
    type: 'error',
    message: e.message,
    filename: e.filename,
    lineno: e.lineno,
    colno: e.colno,
    stack: e.error?.stack?.slice(0, 500)
  }
  console.error(`[${APP_NAME}:GlobalError]`, info)

  // 可选：上报到后端
  // fetch('/api/log/error', { method: 'POST', body: JSON.stringify(info) }).catch(() => {})

  // 暴露给测试
  ;(window as any).__lastError = info
})

window.addEventListener('unhandledrejection', (e) => {
  const reason = e.reason
  const info = {
    type: 'unhandledrejection',
    message: reason?.message || String(reason),
    stack: reason?.stack?.slice(0, 500)
  }
  console.error(`[${APP_NAME}:UnhandledRejection]`, info)
  ;(window as any).__lastRejection = info
})

// === 检测 localStorage 损坏（部分用户持久化失败导致整页空白） ===
try {
  const testKey = '__rr2_health_check__'
  localStorage.setItem(testKey, '1')
  localStorage.removeItem(testKey)
} catch (e) {
  console.warn(`[${APP_NAME}] localStorage 不可用，已禁用持久化`)
  // 提供内存 fallback
  const memoryStore = new Map<string, string>()
  Object.defineProperty(window, 'localStorage', {
    value: {
      getItem: (k: string) => memoryStore.get(k) || null,
      setItem: (k: string, v: string) => memoryStore.set(k, v),
      removeItem: (k: string) => memoryStore.delete(k),
      clear: () => memoryStore.clear()
    },
    configurable: true
  })
}

// === 路由变更日志 ===
const originalPushState = history.pushState
history.pushState = function (...args) {
  const result = originalPushState.apply(this, args as any)
  console.log(`[${APP_NAME}:Nav] → ${location.pathname}`)
  window.dispatchEvent(new CustomEvent('rr2:nav', { detail: location.pathname }))
  return result
}

// === 渲染前移除 splash ===
const splash = document.getElementById('splash')
if (splash) {
  splash.style.display = 'none'
  setTimeout(() => splash.remove(), 100)
}

ReactDOM.createRoot(document.getElementById('root')!).render(
  <React.StrictMode>
    <App />
  </React.StrictMode>
)