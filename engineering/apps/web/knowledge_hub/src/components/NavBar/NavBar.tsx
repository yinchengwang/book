// src/components/NavBar.tsx
import { Component } from 'react'
import { View, Text } from '@tarojs/components'
import { navigateTo } from '@tarojs/taro'
import './NavBar.scss'

// #ifdef H5
import { useNavigate } from '@/hooks/useNavigate'
// #endif

interface NavItem {
  id: string
  title: string
  icon: string
  path: string
}

const navItems: NavItem[] = [
  { id: 'dashboard', title: '首页', icon: '🏠', path: '/pages/index' },
  { id: 'review', title: '复习', icon: '📖', path: '/pages/review/Review' },
  { id: 'tracker', title: '面试', icon: '💼', path: '/pages/interview_tracker/interview_tracker' },
  { id: 'path', title: '路径', icon: '🛤️', path: '/pages/learning_path/learning_path' },
  { id: 'settings', title: '设置', icon: '⚙️', path: '/pages/settings/Settings' },
]

// H5 端导航栏
// #ifdef H5
const H5NavBar = () => {
  const { navigate } = useNavigate()

  const handleNav = (item: NavItem) => {
    navigate(item.path)
  }

  return (
    <View className="h5-nav-bar">
      {navItems.map(item => (
        <View
          key={item.id}
          className="nav-item"
          onClick={() => navigate(item.path)}
        >
          <Text className="nav-icon">{item.icon}</Text>
          <Text className="nav-title">{item.title}</Text>
        </View>
      ))}
    </View>
  )
}
// #endif

// 小程序端 TabBar
// #ifdef MP-WEIXIN
// 小程序使用 app.config.ts 的 tabBar 配置
// 这里只是一个占位组件
const MPNavBar = () => null
// #endif

// 统一导出
export const NavBar = H5NavBar
export default NavBar
