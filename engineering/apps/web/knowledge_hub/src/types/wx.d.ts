/**
 * 微信小程序全局 API 类型声明
 *
 * 实际小程序运行时由微信注入 `wx` 全局对象。
 * 这里只声明项目中用到的 API 最小集合，避免 H5 端编译报错。
 */

declare global {
  const wx: {
    /** 跳转到 tabBar 页面 */
    switchTab: (options: { url: string; success?: () => void; fail?: (err: unknown) => void }) => void
    /** 跳转到非 tabBar 页面 */
    navigateTo: (options: { url: string; success?: () => void; fail?: (err: unknown) => void }) => void
    /** 关闭当前页面跳转 */
    redirectTo: (options: { url: string; success?: () => void; fail?: (err: unknown) => void }) => void
    /** 返回上一级 */
    navigateBack: (options?: { delta?: number }) => void
    /** 获取当前页面栈 */
    getCurrentPages: () => Array<{ route: string; options?: Record<string, unknown> }>
    /** 显示 toast */
    showToast: (options: { title: string; icon?: 'success' | 'error' | 'none'; duration?: number }) => void
    /** 显示模态弹窗 */
    showModal: (options: {
      title: string
      content: string
      success?: (res: { confirm: boolean; cancel: boolean }) => void
    }) => void
    /** 获取系统信息 */
    getSystemInfoSync: () => {
      statusBarHeight: number
      windowHeight: number
      screenHeight: number
      screenWidth: number
      platform: string
      /** 微信小程序安全区域（含刘海/底部 Home Indicator） */
      safeArea?: {
        top: number
        bottom: number
        left: number
        right: number
        width: number
        height: number
      }
      /** 完整系统信息对象（其它字段透传） */
      [key: string]: unknown
    }
    /** 设置 storage */
    setStorageSync: (key: string, data: unknown) => void
    getStorageSync: <T = unknown>(key: string) => T
    removeStorageSync: (key: string) => void
    /** 震动 */
    vibrateShort: (options?: { type?: 'light' | 'medium' | 'heavy' }) => void
  }
}

export {}