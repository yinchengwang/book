/**
 * Responsive 响应式断点工具
 * 提供媒体查询和响应式渲染能力
 */
import { Component, ReactNode, useState, useEffect } from 'react'
import { View, Text } from '@tarojs/components'

// 断点定义
export const BREAKPOINTS = {
  xs: 320,
  sm: 640,
  md: 768,
  lg: 1024,
  xl: 1280,
  xxl: 1536,
} as const

// 断点类型
export type BreakpointKey = keyof typeof BREAKPOINTS

// 媒体查询 hook（仅 H5）
// #ifdef H5
export const useBreakpoint = (): BreakpointKey => {
  const [breakpoint, setBreakpoint] = useState<BreakpointKey>('lg')

  useEffect(() => {
    const updateBreakpoint = () => {
      const width = window.innerWidth
      if (width < BREAKPOINTS.sm) setBreakpoint('xs')
      else if (width < BREAKPOINTS.md) setBreakpoint('sm')
      else if (width < BREAKPOINTS.lg) setBreakpoint('md')
      else if (width < BREAKPOINTS.xl) setBreakpoint('lg')
      else setBreakpoint('xl')
    }

    updateBreakpoint()
    window.addEventListener('resize', updateBreakpoint)
    return () => window.removeEventListener('resize', updateBreakpoint)
  }, [])

  return breakpoint
}
// #endif

// 响应式显示组件
interface ResponsiveProps {
  children: ReactNode
  /** 显示的最小断点 */
  min?: BreakpointKey
  /** 显示的最大断点 */
  max?: BreakpointKey
  /** 仅在小程序显示 */
  onlyMP?: boolean
  /** 仅在 H5 显示 */
  onlyH5?: boolean
}

export const Responsive: React.FC<ResponsiveProps> = ({
  children,
  min,
  max,
  onlyMP = false,
  onlyH5 = false
}) => {
  // #ifdef MP-WEIXIN
  if (onlyH5) return null
  if (onlyMP) return <>{children}</>
  return <>{children}</>
  // #endif

  // #ifdef H5
  if (onlyMP) return null

  const [visible, setVisible] = useState(false)

  useEffect(() => {
    const checkMedia = () => {
      const width = window.innerWidth
      let result = true

      if (min) {
        result = result && width >= BREAKPOINTS[min]
      }
      if (max) {
        result = result && width < BREAKPOINTS[max]
      }

      setVisible(result)
    }

    checkMedia()
    window.addEventListener('resize', checkMedia)
    return () => window.removeEventListener('resize', checkMedia)
  }, [min, max])

  if (!visible) return null
  return <>{children}</>
  // #endif

  return <>{children}</>
}

// 断点匹配 hook
export const useMediaQuery = (query: string): boolean => {
  // #ifdef H5
  const [matches, setMatches] = useState(false)

  useEffect(() => {
    const mediaQuery = window.matchMedia(query)
    setMatches(mediaQuery.matches)

    const handler = (e: MediaQueryListEvent) => setMatches(e.matches)
    mediaQuery.addEventListener('change', handler)
    return () => mediaQuery.removeEventListener('change', handler)
  }, [query])

  return matches
  // #endif

  // #ifdef MP-WEIXIN
  return false
  // #endif
}

// 常见媒体查询快捷方法
export const useIsMobile = () => useMediaQuery(`(max-width: ${BREAKPOINTS.md}px)`)
export const useIsTablet = () => useMediaQuery(`(min-width: ${BREAKPOINTS.md}px) and (max-width: ${BREAKPOINTS.lg}px)`)
export const useIsDesktop = () => useMediaQuery(`(min-width: ${BREAKPOINTS.lg}px)`)

export default Responsive
