// src/components/LazyLoad/index.tsx
// 懒加载组件封装

import { Component, Suspense } from 'react'
import { View, Text } from '@tarojs/components'

interface LazyLoadProps {
  children: React.ReactNode
  fallback?: React.ReactNode
  delay?: number
}

// 懒加载包装组件
export const LazyLoad = ({ children, fallback, delay = 0 }: LazyLoadProps) => {
  const Fallback = fallback || (
    <View className="lazy-loading">
      <Text className="loading-text">加载中...</Text>
    </View>
  )

  if (delay > 0) {
    // 带延迟的懒加载
    return <DelayedLazyLoad delay={delay}>{children}</DelayedLazyLoad>
  }

  return (
    <Suspense fallback={Fallback}>
      {children}
    </Suspense>
  )
}

// 延迟加载组件
class DelayedLazyLoad extends Component<{ children: React.ReactNode; delay: number }> {
  state = { mounted: false }

  componentDidMount() {
    setTimeout(() => {
      this.setState({ mounted: true })
    }, this.props.delay)
  }

  render() {
    if (!this.state.mounted) {
      return (
        <View className="lazy-loading">
          <Text className="loading-text">加载中...</Text>
        </View>
      )
    }
    return this.props.children
  }
}

// 预加载钩子
export const usePreload = (path: string) => {
  // #ifdef H5
  // 在 H5 端预加载路由对应的 JS chunk
  if (typeof window !== 'undefined') {
    // 预取指定的 chunk
    const link = document.createElement('link')
    link.rel = 'prefetch'
    link.href = `${path}.js`
    document.head.appendChild(link)
  }
  // #endif
}

export default LazyLoad
