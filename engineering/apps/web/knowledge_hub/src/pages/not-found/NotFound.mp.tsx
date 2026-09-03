/**
 * 404 Not Found 页面 —— 小程序端
 * 进入页面时自动跳回首页
 */
import { Component } from 'react'
import { View, Text } from '@tarojs/components'
import './NotFound.scss'

// 微信小程序全局对象
declare const wx: {
  switchTab: (options: { url: string }) => void
}

class NotFound extends Component {
  componentDidShow() {
    wx.switchTab({ url: '/pages/index' })
  }

  render() {
    return (
      <View className="not-found-page">
        <View className="not-found-content">
          <Text className="error-code">404</Text>
          <Text className="error-title">页面不存在</Text>
          <Text className="error-desc">
            抱歉，您访问的页面未找到，即将返回首页...
          </Text>
        </View>
      </View>
    )
  }
}

export default NotFound
