/**
 * SafeArea 安全区域组件
 * 处理刘海屏等异形屏幕的安全区域
 */
import { Component, ReactNode } from 'react'
import { View, Text } from '@tarojs/components'
import './SafeArea.scss'

// 安全区域位置
export type SafeAreaPosition = 'top' | 'bottom' | 'left' | 'right' | 'all'

interface SafeAreaProps {
  /** 安全区域位置 */
  position?: SafeAreaPosition
  /** 子元素 */
  children?: ReactNode
  /** 自定义类名 */
  className?: string
  /** 自定义颜色（默认跟随主题） */
  color?: string
}

// 安全区域 hook
export const useSafeAreaInsets = () => {
  // #ifdef H5
  // H5 使用 CSS env()
  return {
    top: 0,
    bottom: 0,
    left: 0,
    right: 0,
    topEnv: 'env(safe-area-inset-top, 0px)',
    bottomEnv: 'env(safe-area-inset-bottom, 0px)',
  }
  // #endif

  // #ifdef MP-WEIXIN
  // 小程序使用 wx.getSystemInfoSync
  try {
    const systemInfo = wx.getSystemInfoSync()
    const safeArea = systemInfo.safeArea || { top: 0, bottom: 0, left: 0, right: 0 }
    return {
      top: safeArea.top || 0,
      bottom: (systemInfo.screenHeight || 0) - safeArea.bottom,
      left: safeArea.left || 0,
      right: (systemInfo.screenWidth || 0) - safeArea.right,
    }
  } catch {
    return { top: 0, bottom: 0, left: 0, right: 0 }
  }
  // #endif

  return { top: 0, bottom: 0, left: 0, right: 0 }
}

// 安全区域占位组件
export const SafeArea: React.FC<SafeAreaProps> = ({
  position = 'all',
  children,
  className = '',
  color
}) => {
  const insets = useSafeAreaInsets()
  const bgColor = color || 'transparent'

  const getStyle = () => {
    const styles: Record<string, string> = { backgroundColor: bgColor }

    if (position === 'all' || position === 'top') {
      styles.height = `${insets.top}px`
      styles.paddingTop = `${insets.top}px`
    }
    if (position === 'all' || position === 'bottom') {
      styles.height = `${insets.bottom}px`
      styles.paddingBottom = `${insets.bottom}px`
    }
    if (position === 'left') {
      styles.paddingLeft = `${insets.left}px`
    }
    if (position === 'right') {
      styles.paddingRight = `${insets.right}px`
    }

    return styles
  }

  return (
    <View className={`safe-area safe-area-${position} ${className}`} style={getStyle()}>
      {children}
    </View>
  )
}

// 顶部安全区域占位
export const SafeAreaTop: React.FC<{ color?: string }> = ({ color }) => (
  <SafeArea position="top" color={color} />
)

// 底部安全区域占位
export const SafeAreaBottom: React.FC<{ color?: string }> = ({ color }) => (
  <SafeArea position="bottom" color={color} />
)

export default SafeArea
