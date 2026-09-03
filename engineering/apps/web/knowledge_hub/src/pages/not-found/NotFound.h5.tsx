/**
 * 404 Not Found 页面 —— H5 端
 */
import { View, Text } from '@tarojs/components'
import { useNavigate } from '@/hooks/useNavigate'
import './NotFound.scss'

const NotFound = () => {
  const { navigate } = useNavigate()
  const path = typeof window !== 'undefined' ? window.location.pathname : ''

  const handleGoHome = () => {
    navigate('/')
  }

  return (
    <View className="not-found-page">
      <View className="not-found-content">
        <Text className="error-code">404</Text>
        <Text className="error-title">页面不存在</Text>
        <Text className="error-desc">
          抱歉，您访问的页面 {path} 未找到
        </Text>

        <View className="action-buttons">
          <View className="btn-primary" onClick={handleGoHome}>
            <Text>返回首页</Text>
          </View>
          <View className="btn-secondary" onClick={() => window.history.back()}>
            <Text>返回上一页</Text>
          </View>
        </View>
      </View>

      <View className="illustration">
        <Text className="illustration-icon">🔍</Text>
      </View>
    </View>
  )
}

export default NotFound
