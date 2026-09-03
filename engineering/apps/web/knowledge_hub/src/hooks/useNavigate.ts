/**
 * 路由导航 Hook
 * H5 端使用 history API + popstate 事件（轻量、零依赖）
 * 小程序端使用 wx.switchTab/navigateTo/redirectTo
 */
import { useCallback } from 'react'
import { isTabBarPage } from '@/utils/routes'
import { navigateWithGuard } from '@/utils/routerGuard'

/**
 * 路由导航 Hook
 * @returns navigate(path, options?) / goBack()
 */
export const useNavigate = () => {
  const navigate = useCallback((path: string, options?: { replace?: boolean }) => {
    if (typeof window !== 'undefined') {
      // H5 端：用 history API + 派发 popstate
      if (options?.replace) {
        window.history.replaceState(null, '', path)
      } else {
        window.history.pushState(null, '', path)
      }
      window.dispatchEvent(new PopStateEvent('popstate'))
    } else if (typeof wx !== 'undefined') {
      // 小程序端：用 wx 路由 API
      if (isTabBarPage(path)) {
        wx.switchTab({ url: path })
      } else if (options?.replace) {
        wx.redirectTo({ url: path })
      } else {
        wx.navigateTo({ url: path })
      }
    }
  }, [])

  const goBack = useCallback(() => {
    if (typeof window !== 'undefined') {
      window.history.back()
    } else if (typeof wx !== 'undefined') {
      wx.navigateBack()
    }
  }, [])

  return { navigate, goBack }
}

/**
 * 获取当前路径
 */
export const useCurrentPath = (): string => {
  if (typeof window !== 'undefined') {
    return window.location.pathname
  }
  if (typeof wx !== 'undefined') {
    try {
      const pages = wx.getCurrentPages()
      const currentPage = pages[pages.length - 1]
      return `/${currentPage.route}`
    } catch {
      return '/'
    }
  }
  return '/'
}

/**
 * 路由跳转（带守卫）
 */
export const useNavigateWithGuard = () => {
  const navigateWithGuardFn = useCallback(async (
    path: string,
    options?: { replace?: boolean }
  ) => {
    return navigateWithGuard(path, options)
  }, [])

  return navigateWithGuardFn
}

export default useNavigate
