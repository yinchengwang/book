/**
 * 页面守卫
 * 处理路由跳转前的权限检查和提示
 */
import { ROUTES, ROUTE_META } from './routes'

// 页面守卫配置
interface GuardConfig {
  /** 需要登录 */
  requiresAuth?: boolean
  /** 未完成提示 */
  unfinishedHint?: string
  /** 自定义验证函数 */
  validator?: () => boolean | Promise<boolean>
  /** 验证失败跳转路径 */
  fallbackPath?: string
}

// 页面守卫映射
const GUARD_MAP: Record<string, GuardConfig> = {
  // 模拟面试（AI 功能未完成）
  [ROUTES.MOCK_INTERVIEW]: {
    unfinishedHint: '模拟面试功能正在开发中，敬请期待！',
  },
  // 项目路线（预埋功能）
  [ROUTES.PROJECT_ROADMAP]: {
    unfinishedHint: '项目路线功能正在开发中，敬请期待！',
  },
}

// 页面守卫结果
export interface GuardResult {
  allowed: boolean
  message?: string
  redirectPath?: string
}

// 全局守卫函数类型
export type NavigationGuard = (path: string) => GuardResult | Promise<GuardResult>

// 默认页面守卫
const defaultGuard: NavigationGuard = async (path: string): Promise<GuardResult> => {
  // 检查是否有自定义配置
  const config = GUARD_MAP[path]

  if (!config) {
    return { allowed: true }
  }

  // 检查未完成提示
  if (config.unfinishedHint) {
    return {
      allowed: false,
      message: config.unfinishedHint,
      redirectPath: config.fallbackPath || ROUTES.INDEX,
    }
  }

  // 自定义验证
  if (config.validator) {
    const valid = await config.validator()
    if (!valid) {
      return {
        allowed: false,
        message: '验证失败',
        redirectPath: config.fallbackPath,
      }
    }
  }

  return { allowed: true }
}

// 显示未完成提示
export const showUnfinishedHint = (message: string) => {
  // #ifdef H5
  alert(message)
  // #endif

  // #ifdef MP-WEIXIN
  wx.showToast({
    title: message,
    icon: 'none',
    duration: 2000,
  })
  // #endif
}

// 执行页面守卫
export const executeGuard = async (
  path: string,
  guard: NavigationGuard = defaultGuard
): Promise<GuardResult> => {
  const result = await guard(path)

  if (!result.allowed && result.message) {
    showUnfinishedHint(result.message)
  }

  return result
}

// 路由守卫中间件（用于 H5 SPA）
// #ifdef H5
export const createRouterGuard = (beforeGuards: NavigationGuard[] = []) => {
  return async (path: string): Promise<GuardResult> => {
    // 执行所有前置守卫
    for (const guard of beforeGuards) {
      const result = await executeGuard(path, guard)
      if (!result.allowed) {
        return result
      }
    }

    // 执行默认守卫
    return executeGuard(path)
  }
}
// #endif

// 导航函数（带守卫）
export const navigateWithGuard = async (
  path: string,
  options?: {
    guard?: NavigationGuard
    /** 是否替换当前历史记录 */
    replace?: boolean
  }
) => {
  const { guard, replace = false } = options || {}

  const result = await executeGuard(path, guard)

  if (!result.allowed) {
    return false
  }

  // #ifdef H5
  if (replace) {
    window.history.replaceState(null, '', path)
  } else {
    window.history.pushState(null, '', path)
  }
  window.dispatchEvent(new PopStateEvent('popstate'))
  // #endif

  // #ifdef MP-WEIXIN
  const isTabBar = ROUTE_META[path]?.isTabBar
  if (isTabBar) {
    wx.switchTab({ url: path })
  } else {
    if (replace) {
      wx.redirectTo({ url: path })
    } else {
      wx.navigateTo({ url: path })
    }
  }
  // #endif

  return true
}

export default {
  executeGuard,
  navigateWithGuard,
  showUnfinishedHint,
}
