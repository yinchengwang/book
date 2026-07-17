/**
 * TabBar 组件
 *
 * 平台适配：
 *  - 小程序端：使用 class TabBar 组件（真实功能）
 *  - H5 端：通过 Vite alias 替换为 web/adapters/TabBarH5.tsx（空组件）
 *
 * 关键：通过 Vite alias 隔离，H5 端不会编译此文件
 */
import { Component } from 'react'
import { View, Text } from '@tarojs/components'
import './TabBar.scss'

interface TabItem {
  id: string
  title: string
  icon: string
  activeIcon: string
  path: string
}

const TAB_LIST: TabItem[] = [
  { id: 'dashboard', title: '首页', icon: '🏠', activeIcon: '🏠', path: '/pages/index' },
  { id: 'review', title: '复习', icon: '📖', activeIcon: '📖', path: '/pages/review/Review' },
  { id: 'tracker', title: '面试', icon: '💼', activeIcon: '💼', path: '/pages/interview_tracker/interview_tracker' },
  { id: 'path', title: '路径', icon: '🛤️', activeIcon: '🛤️', path: '/pages/learning_path/learning_path' },
  { id: 'settings', title: '设置', icon: '⚙️', activeIcon: '⚙️', path: '/pages/settings/Settings' },
]

interface TabBarProps {
  currentPath?: string
  onChange?: (path: string) => void
}

/**
 * 真实 TabBar class - 仅小程序端使用
 * H5 端通过 Vite alias 替换为 TabBarH5.tsx，不进入此文件
 */
class TabBar extends Component<TabBarProps> {
  handleTabClick = (path: string) => {
    const { onChange } = this.props
    if (onChange) {
      onChange(path)
    } else {
      // 微信小程序专用 API
      // eslint-disable-next-line @typescript-eslint/no-explicit-any
      const wxx = (typeof wx !== 'undefined' ? wx : null) as any
      wxx?.switchTab({ url: path })
    }
  }

  render() {
    const { currentPath = '' } = this.props

    return (
      <View className="mp-tab-bar">
        {TAB_LIST.map(item => {
          const isActive = currentPath === item.path
          return (
            <View
              key={item.id}
              className={`tab-item ${isActive ? 'active' : ''}`}
              onClick={() => this.handleTabClick(item.path)}
            >
              <Text className="tab-icon">{isActive ? item.activeIcon : item.icon}</Text>
              <Text className="tab-text">{item.title}</Text>
            </View>
          )
        })}
      </View>
    )
  }
}

export default TabBar
