/**
 * 主题副作用 Hook
 *
 * 监听 settingsStore.theme 变化，写到 document.documentElement.dataset.theme
 * 让 global.scss 的 [data-theme="light"] 变量覆盖生效
 */
import { useEffect } from 'react'
import { useSettingsStore } from '@/stores/settingsStore'

/**
 * 调用此 hook 的组件会订阅主题变化，自动应用主题到 document
 * 一般在 App 根组件或 Layout 组件挂载一次即可
 */
export function useThemeEffect() {
  const theme = useSettingsStore(s => s.theme)

  useEffect(() => {
    document.documentElement.dataset.theme = theme
    // 同时设置 meta 颜色模式（移动端浏览器）
    const meta = document.querySelector('meta[name="color-scheme"]')
    if (!meta) {
      const m = document.createElement('meta')
      m.name = 'color-scheme'
      m.content = theme
      document.head.appendChild(m)
    } else {
      meta.setAttribute('content', theme)
    }
  }, [theme])
}
