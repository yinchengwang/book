import type { ReactNode } from 'react'
import { View, Text } from '@tarojs/components'
import './GameHeader.scss'

/**
 * @brief 通用游戏顶部栏：左侧返回按钮 + 中间标题 + 右侧可选扩展位。
 * @note 返回行为由调用方通过 onBack 提供，保持组件可复用（不仅限于 Taro.navigateBack）。
 */
interface Props {
  title: string
  onBack: () => void
  right?: ReactNode
}

export function GameHeader ({ title, onBack, right }: Props) {
  return (
    <View className='game-header'>
      <View className='game-header-back' onClick={onBack}>←</View>
      <Text className='game-header-title'>{title}</Text>
      <View className='game-header-right'>{right}</View>
    </View>
  )
}